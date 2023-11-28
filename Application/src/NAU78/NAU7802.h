/**************************************************************************//**
* @file      NAU7802.h
* @brief     Template for ESE516 with Doxygen-style comments
* @author    Yuchen Wang
* @date      2023-11-05

******************************************************************************/
#ifndef NAU7802_H
#define NAU7802_H

/******************************************************************************
* Includes
******************************************************************************/
#include "I2cDriver/I2cDriver.h"

/******************************************************************************
* Defines
******************************************************************************/

#define ADC_SLAVE_ADDR        0x2A
#define PU_CTRL_ADDR          0x00
#define CTRL1_ADDR            0x01
#define CTRL2_ADDR            0x02
#define OCAL1_B2_ADDR         0x03
#define OCAL1_B1_ADDR         0x04
#define OCAL1_B0_ADDR         0x05
#define GCAL1_B3_ADDR         0x06
#define GCAL1_B2_ADDR         0x07
#define GCAL1_B1_ADDR         0x08
#define GCAL1_B0_ADDR         0x09
#define OCAL2_B2_ADDR         0x0A
#define OCAL2_B1_ADDR         0x0B
#define OCAL2_B0_ADDR         0x0C
#define GCAL2_B3_ADDR         0x0D
#define GCAL2_B2_ADDR         0x0E
#define GCAL2_B1_ADDR         0x0F
#define GCAL2_B0_ADDR         0x10
#define I2C_CONTROL_ADDR      0x11
#define ADCO_B2_ADDR          0x12
#define ADCO_B1_ADDR          0x13
#define ADCO_B0_ADDR          0x14
#define OTP_B1_ADDR           0x15
#define OTP_B0_ADDR           0x16
#define PGA_PWR_ADDR          0x1B
#define DEVICE_REVISION_ADDR  0x1F

/* Cycle ready (Read only Status) */

#define CR_Pos       (5)
#define CR_Msk       (1<<CR_Pos)
#define CR_DATA_RDY  (1<<CR_Pos)  /* ADC DATA is ready */

/* Cycle start */

#define CS_Pos               (4)
#define CS_Msk               (1<<CS_Pos)
#define CS_START_CONVERSION  (1<<CS_Pos)  /* Synchronize conversion to the rising edge of this register */

/* Read Only calibration result */
#define CAL_ERR_Pos      (3)
#define CAL_ERR_Msk      (1<<CAL_ERR_Pos)
#define CAL_ERR_ERROR    (1<<CAL_ERR_Pos)  /* 1: there is error in this calibration */
#define CAL_ERR_NO_ERROR (0<<CAL_ERR_Pos)  /* 0: there is no error */


/* Write 1 to this bit will trigger calibration based on the selection in CALMOD[1:0] */

/* This is an "Action" register bit. When calibration is finished, it will reset to 0 */

#define CALS_Pos      (2)
#define CALS_Msk      (1<<CALS_Pos)
#define CALS_ACTION   (1<<CALS_Pos)
#define CALS_FINISHED (0<<CALS_Pos)


/* Calibration mode */

#define CALMOD_Pos              (0)
#define CALMOD_Msk              (3<<CALMOD_Pos)
#define CALMOD_GAIN             (3<<CALMOD_Pos)  /* 11 = Gain Calibration System */
#define CALMOD_OFFSET           (2<<CALMOD_Pos)  /* 10 = Offset Calibration System */
#define CALMOD_ RESERVED        (1<<CALMOD_Pos)  /* 01 = Reserved */
#define CALMOD_OFFSET_INTERNAL  (0<<CALMOD_Pos)  /* 00 = Offset Calibration Internal (default) */


static int32_t reg_write(uint8_t reg, uint8_t *bufp,uint16_t len);
static int32_t reg_read(uint8_t reg, uint8_t *bufp, uint16_t len);
uint8_t read_a_reg(uint8_t u8RegAddr);
uint8_t write_a_reg(uint8_t u8RegAddr, uint8_t data);
static void NAU78_calibration(void);
void  NAU78_init(void);
static void cycle_ready(void);
int32_t get_raw_data(void);
float raw_data_to_weight(int raw_data);
float get_weight(void);
int32_t get_adc_id(void);

#endif