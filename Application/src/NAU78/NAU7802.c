/*
 * NAU7802.c
 */ 
#include "NAU7802.h"
#include "I2cDriver/I2cDriver.h"
#include "SerialConsole.h"
uint8_t msgOut[64];
I2C_Data SEN_Data; 

//write the data to the reg
static int32_t reg_write(uint8_t reg, uint8_t *bufp,uint16_t len)
{
	int32_t error = ERROR_NONE;
	msgOut[0] = reg;
	
	for(int i = 0; i < len; i++){
		msgOut[i+1] = bufp[i];
	}
	
	SEN_Data.address = ADC_SLAVE_ADDR;
	SEN_Data.msgOut = &msgOut;
	SEN_Data.lenOut = len + 1;
	
	error = I2cWriteDataWait(&SEN_Data,100);
	return error;
}

//read the data inside regs
static  int32_t reg_read(uint8_t reg, uint8_t *bufp, uint16_t len)
{
	int32_t error = ERROR_NONE;
	
	SEN_Data.address = ADC_SLAVE_ADDR;
	SEN_Data.msgIn = bufp;
	SEN_Data.lenIn = len;

	msgOut[0] = reg;
	SEN_Data.lenOut = 1;
	SEN_Data.msgOut = &msgOut;

	error = I2cReadDataWait(&SEN_Data,5 ,100);
	return error;
}

//read data from a single reg
uint8_t read_a_reg(uint8_t u8RegAddr)
{
	static uint8_t read_bytes;
	int32_t err= reg_read(u8RegAddr, &read_bytes,1);
	return read_bytes;
}

//write data to a specific reg
uint8_t write_a_reg(uint8_t u8RegAddr, uint8_t data)
{
	int32_t error=reg_write(u8RegAddr,&data,1);
	return error;
}

//calibration adc
static void NAU78_calibration(void)
{
	uint8_t reg = 0;
	while (1)
	{
		reg = read_a_reg(CTRL2_ADDR);
		reg &= ~(CALMOD_Msk | CALS_Msk);

		/* Set Calibration mode */
		reg |= CALMOD_OFFSET_INTERNAL;   /* Calibration mode = Internal Offset Calibration */
		write_a_reg(CTRL2_ADDR, reg);
		/* Start calibration */
		reg |= CALS_ACTION;              /* Start calibration */
		write_a_reg(CTRL2_ADDR, reg);

		while (1)
		{
			/* Wait for calibration finish */
			delay_ms(50); /* Wait 50ms */
			/* Read calibration result */
			reg = read_a_reg(CTRL2_ADDR);

			if ((reg & CALS_Msk) == CALS_FINISHED)
			break;
		}
		reg &= CAL_ERR_Msk;
		if ((reg & CAL_ERR_Msk) == 0) /* There is no error */
		break;
	}
	delay_ms(1);    /* Wait 1 ms */

}

//initialize ADC
void NAU78_init(void)
{
	uint8_t reg = 0;

	/* Reset */
	reg =  0x01;                   /* Enter reset mode */
	write_a_reg(PU_CTRL_ADDR, reg);
	delay_ms(1);         /* Wait 1 ms */

	reg =  0x02 ;                  /* Enter Noraml mode */
	write_a_reg(PU_CTRL_ADDR, reg);
	delay_ms(50);         /* Wait 50 ms */

	reg=0x27;
	write_a_reg(CTRL1_ADDR, reg);
	delay_ms(1);
	
	reg=0x86;
	write_a_reg(PU_CTRL_ADDR, reg);
	delay_ms(1);
	
	reg=0x30;
	write_a_reg(OTP_B1_ADDR , reg);
	delay_ms(1);
   

	/* Calibration */
	NAU78_calibration();
}

//set cycle start = 1 to enable the connection
static void cycle_ready(void)
{
	uint8_t reg = 0;
	/* Start conversion */
	reg = read_a_reg(PU_CTRL_ADDR);
	reg |= CS_START_CONVERSION; /* CS=1 */
	write_a_reg(PU_CTRL_ADDR, reg);
}

//read the raw data of weight
int32_t get_raw_data(void)
{
	int32_t temp1, temp2, temp3, raw_weight;
	temp1 = read_a_reg(CTRL2_ADDR);
	temp1 |= 0 << 7;
	write_a_reg(CTRL2_ADDR, temp1);
	vTaskDelay(10);

	temp1 = read_a_reg(ADCO_B2_ADDR );
	temp2 = read_a_reg(ADCO_B1_ADDR );
	temp3 = read_a_reg(ADCO_B0_ADDR );

	raw_weight = temp1 << 16 | temp2 << 8 | temp3 << 0;
	return  raw_weight;
}

//calculate the raw data to real weight
float raw_data_to_weight(int raw_data){
	float adc_out = 00.00;
	float gain = 00.00;
	float offset = 00.00;
	uint8_t gain_reg[4];
	uint8_t offset_reg[3];

	gain_reg[0]=read_a_reg(GCAL1_B3_ADDR);
	gain_reg[1]=read_a_reg(GCAL1_B2_ADDR);
	gain_reg[2]=read_a_reg(GCAL1_B1_ADDR);
	gain_reg[3]=read_a_reg(GCAL1_B0_ADDR);
	offset_reg[0]=read_a_reg(OCAL1_B2_ADDR);
	offset_reg[1]=read_a_reg(OCAL1_B1_ADDR);
	offset_reg[2]=read_a_reg(OCAL1_B0_ADDR);
	
	for(int i=31;i>=0;i--){
		gain+=(float)(((gain_reg[3-i/8]>>(i%8))&0x01)*(2<<(i-23)*10000));
	}
	for(int i=22;i>=0;i--){
		offset+=(float)(((offset_reg[2-i/8]>>(i%8))&0x01)*(2<<(i-23)*10000));
	}
	offset*=(float)(1-(offset_reg[0]>>7)&0x01);

	adc_out = (float)gain/10000*((float)raw_data-(float)offset/10000);
	return adc_out;
}
float get_weight(void)
{	
	I2cInitializeDriver();
	NAU78_init();
	cycle_ready();
	
	//read CR->until data ready
	while ((read_a_reg(PU_CTRL_ADDR)&CR_Msk) != CR_DATA_RDY);
	uint32_t raw_data = get_raw_data();
	float w = raw_data_to_weight(raw_data);
	return w;
}

int32_t get_adc_id(void){
	uint8_t ver;
	NAU78_init();
	ver = read_a_reg(0x1F);
	return ver;
}