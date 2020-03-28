/**************************************************************************
 *
 * burn-firmware.c
 *
 *    Application responsible for updating the firmware on linux.
 *
 * Copyright 2020 Intelbras
 *
 **************************************************************************/

#include <burn-firmware.h>
#include <utils.h>
#include <orcania.h>
#include <iniparser.h>
#include <pthread.h>
#include <getopt.h>

#define THIS_FILE "burn-firmware.c"

char pchFirmwareName[FIRMWARE_NAME_FILE_LENGHT], pchPartition[PARTITION_NAME_LENGHT];
int numLedsUpdate, led_fd, g_led1, g_led2, g_led3, product_id;
bool bLedUpdate, bValidateFirmware, bErasePartition, bBurnFirmware, bHasPartition, bSuport_fw_ver;
MTD_PARTS mtd[MAX_PARTITIONS];

/* Just parse options */
static void usage (char **argv) {

  printf("Usage: %s \n"
      "-v validation\n"
      "-e erase\n"
      "-p partition\n"
      "-f firmware\n", argv[0]);

  exit (0);
}

void parseOptions (int argc, char **argv) {

  while (true) {

    int option_index = 0;
    int c;

    /** Options for getopt */
    static struct option long_options[] = {
      {"validation",  optional_argument,  NULL, 'v'},
      {"erase",       optional_argument,  NULL, 'e'},
      {"partition",   optional_argument,  NULL, 'p'},
      {"firmware",    optional_argument,  NULL, 'f'},
      {"help",        no_argument,        NULL, '?'},
      {NULL,          0,                  NULL, 0}
    };

    c = getopt_long (argc, argv, "v:e:p:f:", long_options, &option_index);
    if (c == ERROR) {
      break;
    }

    switch (c) {

      case 'v':
        bValidateFirmware = true;
        strcpy(pchFirmwareName, optarg);
        break;
      
      case 'e':
        bErasePartition = true;
        strcpy(pchPartition, optarg);
        break;

      case 'p':
        bHasPartition = true;
        strcpy(pchPartition, optarg);
        break; 

      case 'f':
        bBurnFirmware = true;
        strcpy(pchFirmwareName, optarg);
        break;            

      case '?':
      case 'h':
        usage(argv);
        break;
    }
  }

  return;
}

int openFirmware(char *pchPath) {
	return open(pchPath, O_RDONLY);
}

int closeFirmware(int file) {
	return close(file);
}

int led_write_data(unsigned char *data, unsigned char size) {

  int retval;

  if (size % 2) {
    return EIO;
  }

  retval = write(led_fd, data, size);
  if (retval == 2) {
    return SUCCESS;
  } else {
    return errno;
  }
}

int ctrlLeds() {

  pthread_t procThrId;
  int error;

  log("start ctrl leds");

  error = (int) pthread_create(&procThrId, NULL, blinkLeds, NULL);
  if (error) {
    log_error("Fail create Thread to leds. Error: %d", error);
    return ERROR;
  } else {
    pthread_detach(procThrId);
  }

  return SUCCESS;
}

int openDriverLeds() {

  led_fd = open(DEV_LEDS, O_RDWR);

  return led_fd;
}

int ledOnOff(bool bOn, unsigned char ledType) {

  unsigned char data[2] = { ledType, 1 };

  data[1] = bOn;

  return led_write_data(data, 2);
}

static void turnAllLeds(bool bValue){
  ledOnOff(bValue, g_led1);
  ledOnOff(bValue, g_led2);
  ledOnOff(bValue, g_led3);  
}

void *blinkLeds() {

  E_LED_NUM numLed = LED_1;
  int ledID = g_led1;

  log("Running blink leds");

  turnAllLeds(false);  

  while (true) {

    ledOnOff(false, ledID);

    switch (numLed) {

       case LED_1:
          ledID = g_led1;
          break;

       case LED_2:
          ledID = g_led2;
          break;

       case LED_3:
          ledID = g_led3;
          break;

       default:
          ledID = g_led1;
          break;
    }

    if (++numLed > numLedsUpdate) {
       numLed = 0;
    }

    ledOnOff(true, ledID);
    u_msleep(SLEEP_BLINK_LED);
  }

  return NULL;
}

int readFirmwareHeader(int firm_fd, FIRMWARE_HEADER *pFirmFeader) {

	int retval = 0;
	char firm_buff[SIZEOF_HEADER];

	lseek(firm_fd, 0, SEEK_SET);

	retval = read(firm_fd, firm_buff, SIZEOF_HEADER);
	if (retval < 0) {
		return ERROR;
	}

	memcpy(pFirmFeader, &firm_buff[1], sizeof(struct FIRMWARE_HEADER));

	return SUCCESS;
}

int getFirmwareSize(char *pchFirmwarePath) {

	struct stat lenghFile;

	if (stat(pchFirmwarePath, &lenghFile) != SUCCESS) {
		log_error("Error to open firmware");
		return ERROR;
  }

	return lenghFile.st_size - SIZEOF_HEADER;
}

void getConfig() {

	if (iniparser_open()) {
		log_error("Cannot parse file: %s", CONFIG_FILE_PATH);
		return;
	}

  iniparser_get_config(CFG_LED_BURNER_SUPPORT,  &bLedUpdate,    TYPE_BOOL);
  iniparser_get_config(CFG_LED_BURNER,          &numLedsUpdate, TYPE_INT);
  iniparser_get_config(CFG_LED_1,               &g_led1,        TYPE_INT);
  iniparser_get_config(CFG_LED_2,               &g_led2,        TYPE_INT);
  iniparser_get_config(CFG_LED_3,               &g_led3,        TYPE_INT);
  iniparser_get_config(CFG_DEV_ID,              &product_id,    TYPE_INT);

  iniparser_close();
}

static bool isCompatibleProduct(unsigned int product_id_burn) {

	if (product_id == product_id_burn) {
		return true;
	} else {
		return false;
	}
}

int isValidFirmware(char *pchFirmwarePath) {

	int firm_fd, status;
	FIRMWARE_HEADER firmHeader;

	firm_fd = openFirmware(pchFirmwarePath);
	if (firm_fd < 0) {
		log_error("Error to open firmware");
		return ERROR_OPEN_FIRMWARE;
	}

	status = readFirmwareHeader(firm_fd, &firmHeader);
	if (status < 0) {
		log_error("Error to read Firmware header");
		closeFirmware(firm_fd);
		return ERROR_READ_FIRMWARE_HDR;
	}

	closeFirmware(firm_fd);

	if (isCompatibleProduct(firmHeader.dev_id)) {
		return SUCCESS;
	} else {
		return ERROR_INCOMPATIBLE;
	}

	return SUCCESS;
}

static char *getDeviceMtd(char *pchPartitionErase) {

  char *pchDevice = NULL;

  if (!pchPartitionErase) {
    return NULL;
  }

  if (!o_strcmp(pchPartitionErase, PARTITION_NAME_1)) {
    pchDevice = DEVICE_MTD2;
  } else if (!o_strcmp(pchPartitionErase, PARTITION_NAME_2)) {
    pchDevice = DEVICE_MTD3;
  }

  return pchDevice;
}

int eraseFlash(char *pchPartitionErase) {

  char *pchDevice, pchCommand[MAX_LENGHT_COMMAND];
  int status;

  pchDevice = getDeviceMtd(pchPartitionErase);
  if (!pchDevice) {
    return ERROR_ERASE_PARTITION;
  }

  sprintf(pchCommand, CMD_ERASE_FLASH, pchDevice);
  status = system(pchCommand);
  if (WEXITSTATUS(status) == SUCCESS) {
    return SUCCESS;
  } else {
    return ERROR_ERASE_PARTITION;
  }
}

static bool getSupportFwVer() {

	FILE *pf = popen("fw_printenv support_fw_ver | cut -d\"=\" -f2", "r");

	if (pf) {
		char pchSupportFw[SIZE_READ_ENV];

		memset(pchSupportFw, 0, SIZE_READ_ENV);
		fgets(pchSupportFw, SIZE_READ_ENV, pf);

		pclose(pf);

    if (pchSupportFw && (strlen(pchSupportFw) > 1)) {

      if (o_strstr(pchSupportFw, "yes")) {
        return true;    
      } else {
        return false;  
      }

    } else {
      return false;  
    }

	} else {
		return false;
	}
}

static void setStartBurnFirmware(char *pchPartition) {

  if (bSuport_fw_ver) {    
    system("fw_setenv check_fw none");
    if (!o_strcmp(pchPartition, PARTITION_NAME_1)) {
      system("fw_setenv partition1_valid no");
    } else {
      system("fw_setenv partition2_valid no");
    }
  }

  system(CMD_SYNC);
}

static void setFinishBurnFirmware(char *pchPartition) {

  if (bSuport_fw_ver) {
    system("fw_setenv check_fw verify");
    if (!o_strcmp(pchPartition, PARTITION_NAME_1)) {
      system("fw_setenv partition1_valid none");
    } else {
      system("fw_setenv partition2_valid none");
    }
  }

  system(CMD_SYNC);
}

static void getDevicePartitionBurn(char *pchPartition, char **ppchDeviceBlk, char **ppchNewPartition) {

  if (!pchPartition) {
    *ppchDeviceBlk    = NULL;
    *ppchNewPartition = NULL;
    return;
  }

  if (!o_strcmp(pchPartition, PARTITION_NAME_1)) {
    *ppchDeviceBlk    = DEVICE_MTDBLOCK_2;
    *ppchNewPartition = PARTITION_NAME_1;
  } else if (!o_strcmp(pchPartition, PARTITION_NAME_2)) {
    *ppchDeviceBlk    = DEVICE_MTDBLOCK_3;
    *ppchNewPartition = PARTITION_NAME_2;
  }
}

int updateFirmware(char *pchFirmwarePath, char *pchPartition) {

	int firm_fd, status, partition_fd, len, writen, burningPercent, OldBurningPercent;
	char pchCommand[MAX_LENGHT_COMMAND], buffer[BUFFER_READ_FIRMWARE], *pchDeviceBlk, *pchNewPartition;
  float firmSize, writeenTotal;
  pthread_t procThrId;

	log("Partition choice: %s", pchPartition);

  OldBurningPercent = 0;
  burningPercent    = 0;

  getSupportFwVer();
  setStartBurnFirmware(pchPartition);

	status = eraseFlash(pchPartition);
	if (status != SUCCESS) {
		log_error("Fail to erase flash. Status: %d", status);
    return status;
	}

  getDevicePartitionBurn(pchPartition, &pchDeviceBlk, &pchNewPartition);
  if (!pchDeviceBlk || !pchNewPartition) {
    return ERROR_OPEN_PARTITION;
  }  

  partition_fd = open(pchDeviceBlk, O_WRONLY);
	if (partition_fd < 0) {
		log_error("Error to open mtd. partition_fd: %d", partition_fd);
		return ERROR_OPEN_PARTITION;
	}

	firm_fd = openFirmware(pchFirmwarePath);
	if (firm_fd < 0) {
		log_error("Error to open firmware. firm_fd: %d", firm_fd);
		return ERROR_OPEN_FIRMWARE;
	} else {
		lseek(firm_fd, SIZEOF_HEADER, SEEK_SET);  // jump Header
	}

	firmSize 		 = getFirmwareSize(pchFirmwarePath);
	writeenTotal = 0;
	status 			 = SUCCESS;

  log("Firmware size: %d", firmSize);

	while ((len = read(firm_fd, buffer, BUFFER_READ_FIRMWARE)) > 0) {

		if (write(partition_fd, buffer, len) < len) {
			status = ERROR_WRITE_PARTITION;
			log_error("Error writing to partition");
			break;
		}

		writeenTotal 	+= len;
    burningPercent = (int) writeenTotal/firmSize * 100;

    if ((OldBurningPercent != burningPercent) && ((burningPercent % 4) == 0)) {
      sprintf(pchCommand, CMD_STATUS_BURN, burningPercent);
      system(pchCommand);
      OldBurningPercent = burningPercent;
      u_msleep(DELAY_PERCENT);
      log("Burning: %d", burningPercent);
    }
	}

	if (status == SUCCESS) {
    log("Firmware burned Success");
    system(CMD_SYNC); 
    sprintf(pchCommand, CMD_CHANGE_PARTITION, pchNewPartition, pchNewPartition);
    system(pchCommand);
    burningPercent = 100;
    sprintf(pchCommand, CMD_STATUS_BURN, burningPercent);
    system(pchCommand);       
	}

	closeFirmware(firm_fd);
  setFinishBurnFirmware(pchPartition);

	return status;
}

/*
 * Handles leds to signal the burn status.
 */
static void handleLeds() {

  if (!bLedUpdate) {
    return;
  }

  if (openDriverLeds() < 0) {
    log("BurnFirmware error: failed to open led driver");
  }

  if( ctrlLeds() < 0){
    log("BurnFirmware error: failed to create ctrl led thread");
  }
}

static void initBurner() {

  bValidateFirmware = false;
  bErasePartition   = false;
  bBurnFirmware     = false;
  bHasPartition     = false;
  led_fd            = INVALID_FILE_DESCRIPTOR;
  product_id        = INVALID_PRODUCT;
  bSuport_fw_ver    = false;
  bLedUpdate        = false;
  numLedsUpdate     = 0;

  getConfig();
}

/*
 * Start of firmware update.
 *
 *	@param argv Argument pointer indicating the name of the firmware file to be updated.
 */
int main(int argc, char **argv) {

	int status, numImage, numImageUpdate, error;	
	pthread_t procStatus;

  initBurner();
  parseOptions(argc, argv);

  if (bValidateFirmware) {

    log("Validate Firmware");
    return isValidFirmware(pchFirmwareName);

  } else if (bErasePartition) {

    log("Erase Partition");
    return eraseFlash(pchPartition);

  } else if (bBurnFirmware && bHasPartition) {

    log("Update Firmware");

    status = isValidFirmware(pchFirmwareName);
    if (status != SUCCESS) {
      log_error("Invalid Firmware");
      return status;
    }   

    handleLeds();
    
    status = updateFirmware(pchFirmwareName, pchPartition);
    if (status == SUCCESS) {
      log("Firmware burned in partition: %s", pchPartition);
    } else {
      log_error("Error to burn on parition %s", pchPartition);
    }
  }

 	return status;
}
