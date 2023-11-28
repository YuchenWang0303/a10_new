/**************************************************************************//**
* @file      BootMain.c
* @brief     Main file for the ESE516 bootloader. Handles updating the main application
* @details   Main file for the ESE516 bootloader. Handles updating the main application
* @author    Eduardo Garcia
* @date      2020-02-15
* @version   2.0
* @copyright Copyright University of Pennsylvania
******************************************************************************/


/******************************************************************************
* Includes
******************************************************************************/
#include <asf.h>
#include "conf_example.h"
#include <string.h>
#include "sd_mmc_spi.h"

#include "SD Card/SdCard.h"
#include "Systick/Systick.h"
#include "SerialConsole/SerialConsole.h"
#include "ASF/sam0/drivers/dsu/crc32/crc32.h"





/******************************************************************************
* Defines
******************************************************************************/
#define APP_START_ADDRESS  ((uint32_t)0x12000) ///<Start of main application. Must be address of start of main application
#define APP_START_RESET_VEC_ADDRESS (APP_START_ADDRESS+(uint32_t)0x04) ///< Main application reset vector address
//#define MEM_EXAMPLE 1 //COMMENT ME TO REMOVE THE MEMORY WRITE EXAMPLE BELOW
#define BUFFER_SIZE 64
#define ROW_SIZE 256 //bytes



/******************************************************************************
* Structures and Enumerations
******************************************************************************/

struct usart_module cdc_uart_module; ///< Structure for UART module connected to EDBG (used for unit test output)

/******************************************************************************
* Local Function Declaration
******************************************************************************/
static void jumpToApplication(void);
static bool StartFilesystemAndTest(void);
static void configure_nvm(void);
static bool erase_all(void);
static bool write_all(char* file_name);

/******************************************************************************
* Global Variables
******************************************************************************/
//INITIALIZE VARIABLES
char test_file_name[] = "0:sd_mmc_test.txt";	///<Test TEXT File name
char test_bin_file[] = "0:sd_binary.bin";	///<Test BINARY File name

char testA_file_name[] = "0:FlagA.txt"; //test A file name txt
char testA_bin_file[] = "0:TestA.bin"; //test A file name bin

char testB_file_name[] = "0:FlagB.txt"; //test B file name txt
char testB_bin_file[] = "0:TestB.bin"; //test A file name bin
char helpStr[64];

bool isTaskA = true;
bool FlagA = false;
Ctrl_status status; ///<Holds the status of a system initialization
FRESULT res; //Holds the result of the FATFS functions done on the SD CARD TEST
FATFS fs; //Holds the File System of the SD CARD
FIL file_object; //FILE OBJECT used on main for the SD Card Test



/******************************************************************************
* Global Functions
******************************************************************************/

/**************************************************************************//**
* @fn		int main(void)
* @brief	Main function for ESE516 Bootloader Application

* @return	Unused (ANSI-C compatibility).
* @note		Bootloader code initiates here.
*****************************************************************************/

int main(void)
{

	/*1.) INIT SYSTEM PERIPHERALS INITIALIZATION*/
	system_init();
	delay_init();
	InitializeSerialConsole();
	system_interrupt_enable_global();
	/* Initialize SD MMC stack */
	sd_mmc_init();

	//Initialize the NVM driver
	configure_nvm();

	irq_initialize_vectors();
	cpu_irq_enable();

	//Configure CRC32
	dsu_crc32_init();

	SerialConsoleWriteString("ESE516 - ENTER BOOTLOADER");	//Order to add string to TX Buffer

	/*END SYSTEM PERIPHERALS INITIALIZATION*/


	/*2.) STARTS SIMPLE SD CARD MOUNTING AND TEST!*/

	//EXAMPLE CODE ON MOUNTING THE SD CARD AND WRITING TO A FILE
	//See function inside to see how to open a file
	SerialConsoleWriteString("\x0C\n\r-- SD/MMC Card Example on FatFs --\n\r");

	if(StartFilesystemAndTest() == false)
	{
		SerialConsoleWriteString("SD CARD failed! Check your connections. System will restart in 5 seconds...\r\n");
		delay_cycles_ms(5000);
		system_reset();
	}
	else
	{
		SerialConsoleWriteString("SD CARD mount success! Filesystem also mounted. \r\n");
	}

	/*END SIMPLE SD CARD MOUNTING AND TEST!*/


	/*3.) STARTS BOOTLOADER HERE!*/

	//PERFORM BOOTLOADER HERE!

	#define MEM_EXAMPLE

	#ifdef MEM_EXAMPLE
	testA_file_name[0] = LUN_ID_SD_MMC_0_MEM + '0';
	testA_bin_file[0] = LUN_ID_SD_MMC_0_MEM + '0';
	testB_file_name[0] = LUN_ID_SD_MMC_0_MEM + '0';
	testB_bin_file[0] = LUN_ID_SD_MMC_0_MEM + '0';

	res = f_open(&file_object, (char const *)testA_file_name, FA_READ);
	f_close(&file_object);
	if (res == FR_OK)
	{
		FlagA = true;
	}
	else
	{
		res = f_open(&file_object, (char const *)testB_file_name, FA_READ);
		f_close(&file_object);
		if (res != FR_OK)
		{
			goto end_test;
		}
		else{
			FlagA = false;
		}
	}


	if(FlagA == true){
		if(erase_all()){
			SerialConsoleWriteString("TestA \r\n");
		if(write_all(testA_bin_file)){
			f_unlink(testA_file_name);
		}else{
			goto end_test;
			}
		}
	}
	else if(FlagA == false)
	{
		if(erase_all()){
			SerialConsoleWriteString("TestB \r\n");
		if(write_all(testB_bin_file)){
			f_unlink(testB_file_name);
		}else{
			goto end_test;
		}
		}
	}

	#endif

	/*END BOOTLOADER HERE!*/
	end_test:
	//4.) DEINITIALIZE HW AND JUMP TO MAIN APPLICATION!
	SerialConsoleWriteString("ESE516 - EXIT BOOTLOADER \r\n");	//Order to add string to TX Buffer
	delay_cycles_ms(100); //Delay to allow print

	//Deinitialize HW - deinitialize started HW here!
	DeinitializeSerialConsole(); //Deinitializes UART
	sd_mmc_deinit(); //Deinitialize SD CARD


	//Jump to application
	jumpToApplication();

	//Should not reach here! The device should have jumped to the main FW.
	}



/******************************************************************************
* Static Functions
******************************************************************************/
static bool erase_all(void)
{
	uint32_t current_addr = APP_START_ADDRESS;
	int  row = (0x40000 - APP_START_ADDRESS) / 256;//erase all application space
	for (int i = 0; i < row; i++){
		enum status_code nvmError = nvm_erase_row(current_addr);
		if(nvmError != STATUS_OK)
	{
		SerialConsoleWriteString("Erase error \r\n");
		return false;
	}
//Make sure it got erased - we read the page. Erasure in NVM is an 0xFF
	for(int iter = 0; iter < 256; iter++)
	{
		char *a = (char *)(APP_START_ADDRESS + iter); //Pointer pointing to address APP_START_ADDRESS
		if(*a != 0xFF)
		{
			SerialConsoleWriteString("Error - test page is not erased! \r\n");
			break;
		}
	}
	current_addr += 256;
	}
	SerialConsoleWriteString(" Pages are erased! \r\n");
	return true;
}

static bool write_all(char* file_name)
{
	uint32_t resultCrcSd = 0;
	uint32_t resultCrcNVM = 0;
	uint8_t readBuffer[256];
	FIL file_object_bin;
	struct nvm_parameters parameters;
	nvm_get_parameters (&parameters); //Get NVM parameters

	int i =0;
	int addr_offset = 256;
	int numBytesRead = 0;
	int numberBytesTotal = 0;
	res = f_open(&file_object_bin, file_name, FA_READ);
	int numBytesLeft = f_size(&file_object_bin);
	if(res != FR_OK){
		SerialConsoleWriteString("Can NOT find corresponding binary file \r\n");
		return false;
	}

	//read all file
	while(numBytesLeft > 0){
		res = f_read(&file_object_bin, &readBuffer[0],  ROW_SIZE, &numBytesRead);//read a row = 256 Bytes
		numBytesLeft -= numBytesRead;
		numberBytesTotal += numBytesRead;
		if(res != FR_OK){
			SerialConsoleWriteString("Failed Error\r\n");
			f_close(&file_object_bin);
			return false;
		}

	int32_t cur_address = APP_START_ADDRESS + (addr_offset * i);

	//write 256 Bytes = 4 times 64 Bytes
	res = nvm_write_buffer(cur_address, &readBuffer[0], BUFFER_SIZE);
	res = nvm_write_buffer(cur_address + 64, &readBuffer[64], BUFFER_SIZE);
	res = nvm_write_buffer(cur_address + 128, &readBuffer[128], BUFFER_SIZE);
	res = nvm_write_buffer(cur_address + 192, &readBuffer[192], BUFFER_SIZE);

	//ERRATA Part 1 - To be done before RAM CRC
	//CRC of SD Card. Errata 1.8.3 determines we should run this code every time we do CRC32 from a RAM source. See http://ww1.microchip.com/downloads/en/DeviceDoc/SAM-D21-Family-Silicon-Errata-and-DataSheet-Clarification-DS80000760D.pdf Section 1.8.3
	uint32_t resultCrcSd = 0;
	*((volatile unsigned int*) 0x41007058) &= ~0x30000UL;

	//CRC of SD Card
	enum status_code crcres = dsu_crc32_cal	(readBuffer	,256, &resultCrcSd); //Instructor note: Was it the third parameter used for? Please check how you can use the third parameter to do the CRC of a long data stream in chunks - you will need it!

	//Errata Part 2 - To be done after RAM CRC
	*((volatile unsigned int*) 0x41007058) |= 0x20000UL;

	//CRC of memory (NVM)
	uint32_t resultCrcNvm = 0;
	crcres |= dsu_crc32_cal	(APP_START_ADDRESS	,256, &resultCrcNvm);

	if (crcres != STATUS_OK)
	{
		SerialConsoleWriteString("Could not calculate CRC!!\r\n");
	}
	else
	{
		snprintf(helpStr, 63,"CRC SD CARD: %d  CRC NVM: %d \r\n", resultCrcSd, resultCrcNvm);
		SerialConsoleWriteString(helpStr);
	}
	i++;
}

	if(resultCrcSd != resultCrcNVM){
		SerialConsoleWriteString("CRC ERROR \r\n");
		snprintf(helpStr, 63,"CRC SD CARD: %d  CRC NVM: %d \r\n", resultCrcSd, resultCrcNVM);
		SerialConsoleWriteString(helpStr);
		f_close(&file_object_bin);
		return false;
	}

	f_close(&file_object_bin);
	return true;

}


/**************************************************************************//**
* function      static void StartFilesystemAndTest()
* @brief        Starts the filesystem and tests it. Sets the filesystem to the global variable fs
* @details      Jumps to the main application. Please turn off ALL PERIPHERALS that were turned on by the bootloader
*				before performing the jump!
* @return       Returns true is SD card and file system test passed. False otherwise.
******************************************************************************/
static bool StartFilesystemAndTest(void)
{
	bool sdCardPass = true;
	uint8_t binbuff[256];

	//Before we begin - fill buffer for binary write test
	//Fill binbuff with values 0x00 - 0xFF
	for(int i = 0; i < 256; i++)
	{
		binbuff[i] = i;
	}

	//MOUNT SD CARD
	Ctrl_status sdStatus= SdCard_Initiate();
	if(sdStatus == CTRL_GOOD) //If the SD card is good we continue mounting the system!
	{
		SerialConsoleWriteString("SD Card initiated correctly!\n\r");

		//Attempt to mount a FAT file system on the SD Card using FATFS
		SerialConsoleWriteString("Mount disk (f_mount)...\r\n");
		memset(&fs, 0, sizeof(FATFS));
		res = f_mount(LUN_ID_SD_MMC_0_MEM, &fs); //Order FATFS Mount
	if (FR_INVALID_DRIVE == res)
	{
		LogMessage(LOG_INFO_LVL ,"[FAIL] res %d\r\n", res);
		sdCardPass = false;
		goto main_end_of_test;
	}
	SerialConsoleWriteString("[OK]\r\n");

	//Create and open a file
	SerialConsoleWriteString("Create a file (f_open)...\r\n");

	test_file_name[0] = LUN_ID_SD_MMC_0_MEM + '0';
	res = f_open(&file_object,
	(char const *)test_file_name,
	FA_CREATE_ALWAYS | FA_WRITE);

	if (res != FR_OK)
	{
		LogMessage(LOG_INFO_LVL ,"[FAIL] res %d\r\n", res);
		sdCardPass = false;
		goto main_end_of_test;
	}

	SerialConsoleWriteString("[OK]\r\n");

	//Write to a file
	SerialConsoleWriteString("Write to test file (f_puts)...\r\n");

	if (0 == f_puts("Test SD/MMC stack\n", &file_object))
	{
		f_close(&file_object);
		LogMessage(LOG_INFO_LVL ,"[FAIL]\r\n");
		sdCardPass = false;
		goto main_end_of_test;
	}

	SerialConsoleWriteString("[OK]\r\n");
	f_close(&file_object); //Close file
	SerialConsoleWriteString("Test is successful.\n\r");


	//Write binary file
	//Read SD Card File
	test_bin_file[0] = LUN_ID_SD_MMC_0_MEM + '0';
	res = f_open(&file_object, (char const *)test_bin_file, FA_WRITE | FA_CREATE_ALWAYS);

	if (res != FR_OK)
	{
		SerialConsoleWriteString("Could not open binary file!\r\n");
		LogMessage(LOG_INFO_LVL ,"[FAIL] res %d\r\n", res);
		sdCardPass = false;
		goto main_end_of_test;
	}

	//Write to a binaryfile
	SerialConsoleWriteString("Write to test file (f_write)...\r\n");
	uint32_t varWrite = 0;
	if (0 != f_write(&file_object, binbuff,256, (UINT*) &varWrite))
	{
		f_close(&file_object);
		LogMessage(LOG_INFO_LVL ,"[FAIL]\r\n");
		sdCardPass = false;
		goto main_end_of_test;
	}

	SerialConsoleWriteString("[OK]\r\n");
	f_close(&file_object); //Close file
	SerialConsoleWriteString("Test is successful.\n\r");

	main_end_of_test:
	SerialConsoleWriteString("End of Test.\n\r");

	}
	else
	{
		SerialConsoleWriteString("SD Card failed initiation! Check connections!\n\r");
		sdCardPass = false;
	}

	return sdCardPass;
	}



/**************************************************************************//**
* function      static void jumpToApplication(void)
* @brief        Jumps to main application
* @details      Jumps to the main application. Please turn off ALL PERIPHERALS that were turned on by the bootloader
*				before performing the jump!
* @return
******************************************************************************/
static void jumpToApplication(void)
{
	// Function pointer to application section
	void (*applicationCodeEntry)(void);

	// Rebase stack pointer
	__set_MSP(*(uint32_t *) APP_START_ADDRESS);

	// Rebase vector table
	SCB->VTOR = ((uint32_t) APP_START_ADDRESS & SCB_VTOR_TBLOFF_Msk);

	// Set pointer to application section
	applicationCodeEntry =
	(void (*)(void))(unsigned *)(*(unsigned *)(APP_START_RESET_VEC_ADDRESS));

	// Jump to application. By calling applicationCodeEntry() as a function we move the PC to the point in memory pointed by applicationCodeEntry,
	//which should be the start of the main FW.
	applicationCodeEntry();
}



/**************************************************************************//**
* function      static void configure_nvm(void)
* @brief        Configures the NVM driver
* @details
* @return
******************************************************************************/
static void configure_nvm(void)
{
	struct nvm_config config_nvm;
	nvm_get_config_defaults(&config_nvm);
	config_nvm.manual_page_write = false;
	nvm_set_config(&config_nvm);
}

