/**************************************************************************
 * burn-firmware.h
 *
 *  Create on: 10/01/2018
 *     Author: Israel de Simas
 *
 *  Header File to burner-firmware
 *
 * Copyrights Intelbras, 2020
 *
 **************************************************************************/

#ifndef BURN_FIRMWARE_H_
#define BURN_FIRMWARE_H_

/**************************************************************************
 * INCLUDES
 **************************************************************************/
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <utils.h>

/**************************************************************************
 * DEFINITIONS
 **************************************************************************/

#define MAX_PARTITIONS	          5
#define NUM_PARTITIONS	          2											// Number of partitions supported by the system
#define MTD_FILE				          "/proc/mtd"
#define LOG_FILE    		          ""

#define PARTITION_NAME	          "rootfs-%d"

#define PARTITION_NAME_1          "rootfs"
#define PARTITION_NAME_2          "rootfs-2"
#define DEVICE_MTD2               "/dev/mtd2"
#define DEVICE_MTD3               "/dev/mtd3"
#define DEVICE_MTDBLOCK_2         "/dev/mtdblock2"
#define DEVICE_MTDBLOCK_3         "/dev/mtdblock3"
#define DEV_LEDS                  "/dev/leds"

#define CMD_CHANGE_PARTITION      "fw_setenv rootfspart %s && fw_setenv linux_bootargs root=mtd:%s rootfstype=jffs2"
#define CMD_STATUS_BURN           "echo %d > /tmp/burningPercent"

#define FIRMWARE_NAME_FILE_LENGHT	100

#define BUFFER_MTD_LENGHT					2048

#define PARTITION_NAME_LENGHT			256

#define DEVIVE_MTD_LENGHT   			8
#define SIZE_MTD_LENGTH						12
#define ERASESIZE_MTD_LENGHT			12

#define CMD_ERASE_FLASH						"flash_eraseall --jffs2 %s"
#define CMD_SYNC									"sync"

#define INVALID_PRODUCT		        -1
#define INVALID_FILE_DESCRIPTOR   -1

#define FIRM_HEADER 		          0x55
#define SIZEOF_HEADER		          15

#define BUFFER_READ_FIRMWARE 	    10000

#define CFG_DEV_ID					      "general:dev_id"
#define CFG_LED_BURNER_SUPPORT    "led:ledBurnerSupport"
#define CFG_LED_BURNER            "led:numLedsBurner"
#define CFG_LED_1                 "led:Init1"
#define CFG_LED_2              	  "led:Init2"
#define CFG_LED_3                 "led:Init3"

#define NUM_LEDS                  3

#define SLEEP_BLINK_LED				    500	  // Blink time in ms

#define MAJOR_RECOVERY            4
#define MINOR_RECOVERY            2

#define MAJOR_INDEX               9
#define MINOR_INDEX               11
#define VERSION_SIZE              5
#define SIZE_READ_ENV             50

#define MAX_LENGHT_COMMAND				1024
#define DELAY_PERCENT             100

/**************************************************************************
 * TYPEDEFS
 **************************************************************************/

/**
 * 	@struct MTD_PARTS
 *  Structure with information from Flash MTD parts.
 */
typedef struct MTD_PARTS {
	char pchDev[DEVIVE_MTD_LENGHT];
	char pchSize[SIZE_MTD_LENGTH];
	char pchEraseSize[ERASESIZE_MTD_LENGHT];
	char pchName[PARTITION_NAME_LENGHT];
} MTD_PARTS;

/*
 * @enum E_ERROR_FIRMWARE
 *
 * Types of errors when performing Firmware recording
 */
typedef enum {
	ERROR_OPEN_FIRMWARE				= 10,
	ERROR_READ_FIRMWARE_HDR		= 11,
	ERROR_READ_FIRMWARE_INFO	= 12,
	ERROR_PARTITION_PATH			= 13,
	ERROR_READ_CRYPTO_KEY			= 14,
	ERROR_INCOMPATIBLE				= 15,
	ERROR_DEFAULT							= 16,
	ERROR_OPEN_PARTITION			= 17,
	ERROR_WRITE_PARTITION			= 18,
  ERROR_ERASE_PARTITION     = 19,
} E_ERROR_FIRMWARE;

/* Package Header
 *
 * +--------+
 * |  55    |	-> 1 Byte 	- Unsigned char
 * +--------+
 * |        |
 * | vendor |	-> 4 bytes	- unsigned int
 * |   id   |
 * |        |
 * +--------+
 * |        |
 * | device |	-> 4 bytes	- unsigned int
 * |   id   |
 * |        |
 * +--------+
 * |  major |	-> 2 bytes	- unsigned short
 * +--------+
 * |  minor |	-> 2 bytes	- unsigned short
 * +--------+
 * |  patch |	-> 2 bytes	- unsigned short
 * +--------+
 * |        |
 */

/**
 * 	@struct FIRMWARE_HEADER
 *  Structure with Firmware information.
 */
typedef struct FIRMWARE_HEADER {
	unsigned int    vendor;
	unsigned int    dev_id;
	unsigned short  major;
	unsigned short  minor;
	unsigned short  patch;
} FIRMWARE_HEADER;

/*
 * @enum E_LED_NUM
 *
 * Product LED identifier to change state
 */
typedef enum {
  LED_1        = 0,
  LED_2,
  LED_3
} E_LED_NUM;

/**************************************************************************
 * INTERNAL FUNCTIONS
 **************************************************************************/

/*
 * Validate Firmware file
 */
int isValidFirmware(char *pchFirmwarePath);

/*
 * Updates the firmware version on the platform.
 *
 * @param pchFirmwarePath Firmware path to be updated.
 * @param pchPartition Name of the partition to be updated.
 *
 * @return ERROR or SUCCESS depending on the update actions.
 */
int updateFirmware(char *pchFirmwarePath, char *pchPartition);

/*
 * Retrieves firmware size.
 * 
 * @param pchFirmwarePath Firmware path to be updated.
 */
int getFirmwareSize(char *pchFirmwarePath);

/*
 * Turns a specific LED on or off.
 *
 * @param bOn Indicates whether or not to turn on the Led.
 * @param off Led indexer.
 *
 * @return LED operation status.
 */
int ledOnOff(bool bOn, unsigned char off);

/*
 * Opens the LED driver.
 *
 * @return driver open status.
 */
int openDriverLeds();

/*
 * Blinks leds task.
 */
void *blinkLeds();

/*
 * Opens the firmware file
 *
 * @param pchPath Firmware path
 *
 * Return code:
 *  on error: < 0
 *  on success: The file descriptor
 */
int openFirmware(char *pchPath);

/*
 * Closes the file
 */
int closeFirmware(int file);

/*
 * Read the firmware header
 *
 * @param firm_fd: Descriptor do firmware.
 * @param pFirmHeader pointer to Firmware header.
 *
 * @return < 0 if the file has not been read.
 * @return > 0 and the firm_header structure with the new data
 */
int readFirmwareHeader(int firm_fd, FIRMWARE_HEADER *pFirmHeader);

/*
 * Deletes the indicated partition.
 * 
 * @param pchPartitionErase pointer to partition name.
 */
int eraseFlash(char *pchPartitionErase);

#endif
