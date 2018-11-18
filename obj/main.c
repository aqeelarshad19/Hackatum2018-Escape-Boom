#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif // HAVE_CONFIG_H

#include <err.h>
#include <stddef.h>
#include <string.h>

#include <nfc/nfc.h>

#include "nfc-utils.h"

#define MAX_DEVICE_COUNT 16
#define MAX_TARGET_COUNT 16

static nfc_device *pnd;
static nfc_target nt;


#include <stdio.h>		//printf()
#include <stdlib.h>		//exit()

#include "OLED_Driver.h"
#include "OLED_GUI.h"
#include "DEV_Config.h"
#include <time.h>
#include "bcm2835.h"

// PWM output on RPi Plug P1 pin 12 (which is GPIO pin 18)
// in alt fun 5.
// Note that this is the _only_ PWM pin available on the RPi IO headers
#define PIN RPI_GPIO_P1_12
// and it is controlled by PWM channel 0
#define PWM_CHANNEL 0
// This controls the max range of the PWM signal
#define RANGE 1024
//
char value[10] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
const unsigned char tagUidTable[9][4] = {{ 0x06,0xE0,0x7C,0x12}, {0x7D,0xDA,0x19,0x2E},
		{0x75, 0xa4, 0x4d, 0x3a }, {0xD6, 0x11, 0xA8, 0xAC}, {0xE4, 0x11, 0xCB, 0xFC},
		{0x46, 0xF8, 0xC8, 0x49}, {0xB0, 0x97, 0x84, 0x63}, {0x96, 0x9B, 0xB4, 0xAC},
		{0x7A, 0xA2, 0xA3, 0xB9}};  //1 - 9

#define BUZZER_PIN 23

int get_uid_number(unsigned char * uid) {
	int i;
	int res;
	for (int i = 0 ; i < 9 ; i++) {
		res = memcmp(&tagUidTable[i], uid, 4);
		if (res == 0x00) {
//			printf("Uid Number: %d \n", i +1);
			return i +1;
		}
	}
	return 0x00;
}
void set_buzzer_pwm(void) {
//	bcm2835_gpio_fsel(BUZZER_PIN, BCM2835_GPIO_FSEL_OUTP);
//	bcm2835_gpio_write(BUZZER_PIN,LOW);
	  // Set the output pin to Alt Fun 5, to allow PWM channel 0 to be output there
	bcm2835_gpio_fsel(PIN, BCM2835_GPIO_FSEL_ALT5);
	  // Clock divider is set to 16.
	  // With a divider of 16 and a RANGE of 1024, in MARKSPACE mode,
	  // the pulse repetition frequency will be
	  // 1.2MHz/1024 = 1171.875Hz, suitable for driving a DC motor with PWM
	bcm2835_pwm_set_clock(BCM2835_PWM_CLOCK_DIVIDER_16);
	bcm2835_pwm_set_mode(PWM_CHANNEL, 1, 1);
	bcm2835_pwm_set_range(PWM_CHANNEL, RANGE);
	int data = 0;
	bcm2835_pwm_set_data(PWM_CHANNEL, data);
	bcm2835_delay(1);
}

int read_nfc_tag(int rfidKeyNumber) {
	  const char *acLibnfcVersion;
	  nfc_context *context;
	  nfc_init(&context);
	  size_t  i;
	  bool verbose = 0x00;
	  int res = 0;
	  int mask = 0xff;
	  int arg;
	  int error;
	  int rfidTagNumberRead;
	  int tag_read = 0x00;

	  if (context == NULL) {
	    ERR("Unable to init libnfc (malloc)");
	    exit(EXIT_FAILURE);
	  }
	  // Display libnfc version
	  acLibnfcVersion = nfc_version();
//	  printf("uses libnfc %s\n", acLibnfcVersion);

	  /* Lazy way to open an NFC device */
#if 0
	  pnd = nfc_open(context, NULL);
#endif

	  nfc_connstring connstrings[MAX_DEVICE_COUNT];
	  size_t szDeviceFound = nfc_list_devices(context, connstrings, MAX_DEVICE_COUNT);

	  if (szDeviceFound == 0) {
	    printf("No NFC device found.\n");
	  }

//	  for (i = 0; i < szDeviceFound; i++) {
	  while (tag_read == 0x00) {

	    nfc_target ant[MAX_TARGET_COUNT];
//	    printf("Open the device");
	    pnd = nfc_open(context, connstrings[0]);

	    if (pnd == NULL) {
	      ERR("Unable to open NFC device: %s", connstrings[0]);
	      continue;
	    }
	    if (nfc_initiator_init(pnd) < 0) {
	      nfc_perror(pnd, "nfc_initiator_init");
	      nfc_exit(context);
	      exit(EXIT_FAILURE);
	    }

//	    printf("NFC device: %s opened\n", nfc_device_get_name(pnd));

	    nfc_modulation nm;
//	    printf("Mask : %d ",mask);
	    if (mask & 0x1) {
	      nm.nmt = NMT_ISO14443A;
	      nm.nbr = NBR_106;
	      // List ISO14443A targets
	      if (nfc_initiator_select_passive_target(pnd, nm, NULL, 0, &nt) > 0) {
//	    	  printf("       UID (NFCID%c): ", (nt.nti.nai.abtUid[0] == 0x08 ? '3' : '1'));
//	    	  print_hex(nt.nti.nai.abtUid, nt.nti.nai.szUidLen);
	    	  rfidTagNumberRead = get_uid_number(nt.nti.nai.abtUid);
	    	  if (rfidTagNumberRead == rfidKeyNumber) {
//	    		  printf("Correct Key \n");
	    		  return 0x01;
	    	  }else {
//	    		  printf("Err Incorrect Key \n");
	    	  }
		      tag_read = 0x01;
	      }
	    }
	    nfc_close(pnd);
//	    printf("Sleeping some time");
	    usleep(1000* 100);
	  }
//	  printf("Tag was read : %d",tag_read);
	  nfc_exit(context);
	  return 0x00;
}

int display_Oled(int pos, char * data1, char * data2, char * level , char * levelMsg){

	//1.System Initialization
	if(System_Init())
		exit(0);
	//2.show
	OLED_SCAN_DIR OLED_ScanDir = SCAN_DIR_DFT;//SCAN_DIR_DFT = D2U_L2R
	OLED_Init(OLED_ScanDir );
	OLED_Clear(0x00);
//	GUI_Show();

	GUI_Disbitmap(0, 2, Signal816, 16, 8);
	GUI_Disbitmap(24, 2, Bluetooth88, 8, 8);
	GUI_Disbitmap(40, 2, Msg816, 16, 8);
	GUI_Disbitmap(64, 2, GPRS88, 8, 8);
	GUI_Disbitmap(90, 2, Alarm88, 8, 8);
	GUI_Disbitmap(112, 2, Bat816, 16, 8);

	GUI_DisString_EN(0, 16, data1, &Font24, FONT_BACKGROUND, WHITE);
	GUI_DisString_EN(0, 32, data2, &Font24, FONT_BACKGROUND, WHITE);

	GUI_DisString_EN(0, 52, "LEVEL", &Font12, FONT_BACKGROUND, WHITE);
	GUI_DisString_EN(52, 52, level, &Font12, FONT_BACKGROUND, WHITE);
	GUI_DisString_EN(90, 52, levelMsg, &Font12, FONT_BACKGROUND, WHITE);

	OLED_Display();
	OLED_RST_1;
	System_Exit();
	return 0x01;
}
enum {ODD_DIGIT, EVEN_DIGIT, LETTER_AM, LETTER_NZ};
const keyL1Table[4][4] = {{1,7,3,6},
						  {4,2,5,7},
						  {2,4,3,1},
						  {8,6,9,5}};
const keyL2Table[4][4] = {{6,9,6,2}, {1,5,1,3}, {3,2,8,5}, {7,4,7,4}};
const keyL3Table[4][4] = {{9,7,1,3}, {4,8,6,2}, {8,6,5,9}, {2,1,3,4}};

int get_key_number(int rfidLevel, char * key) {
	int rfidTagNumber ;
	int column;
	int row;
	char key1;
	char key2;
	int key1Type;
	int key2Type;
	key1 = key[1];
	key2 = key[2];
//	printf("Key1 %c Key2 %c ",key1 , key2);
	if ((key1 == '1') || (key1 == '3') || (key1 == '5') || (key1 == '7') || (key1 == '9')) {
		key1Type = ODD_DIGIT;
	}else if ((key1 == '0') || (key1 == '2') || (key1 == '4') || (key1 == '6') || (key1 == '8')) {
		key1Type = EVEN_DIGIT;
	}else if ((key1 >= 'A') && (key1 <= 'M')) {
		key1Type = LETTER_AM;
	}else if ((key1 >= 'N') && (key1 <= 'Z')) {
		key1Type = LETTER_NZ;
	}else {
		printf("ERROR Parsing the serial, Key1");
	}

	if ((key2 == '1') || (key2 == '3') || (key2 == '5')|| (key2 == '7') || (key2 == '9')) {
		key2Type = ODD_DIGIT;
	}else if ((key2 == '0') || (key2 == '2') || (key2 == '4') || (key2 == '6') || (key2 == '8')) {
		key2Type = EVEN_DIGIT;
	}else if ((key2 >= 'A') && (key2 <= 'M')) {
		key2Type = LETTER_AM;
	}else if ((key2 >= 'N') && (key2 <= 'Z')) {
		key2Type = LETTER_NZ;
	}else {
		printf("ERROR Parsing the serial, Key2");
	}
	switch( rfidLevel)
	{
	case 0x01:
		switch(key1Type) {
		case ODD_DIGIT:
			column = 0x00;
			break;
		case EVEN_DIGIT:
			column = 0x01;
			break;
		case LETTER_AM:
			column = 0x02;
			break;
		case LETTER_NZ:
			column = 0x03;
			break;
		}
		switch(key2Type) {
		case ODD_DIGIT:
			row = 0x01;
			break;
		case EVEN_DIGIT:
			row = 0x03;
			break;
		case LETTER_AM:
			row = 0x00;
			break;
		case LETTER_NZ:
			row = 0x02;
			break;
		}
		rfidTagNumber = keyL1Table[row][column];
//		printf("Calcualted : %d , Key Level: %d \n", rfidTagNumber, rfidLevel);
		break;

	case 0x02:
		switch(key1Type) {
		case ODD_DIGIT:
			column = 0x00;
			break;
		case EVEN_DIGIT:
			column = 0x01;
			break;
		case LETTER_AM:
			column = 0x02;
			break;
		case LETTER_NZ:
			column = 0x03;
			break;
		}
		switch(key2Type) {
		case ODD_DIGIT:
			row = 0x00;
			break;
		case EVEN_DIGIT:
			row = 0x03;
			break;
		case LETTER_AM:
			row = 0x02;
			break;
		case LETTER_NZ:
			row = 0x01;
			break;
		}
		rfidTagNumber = keyL2Table[row][column];
//		printf("Calcualted : %d , Key Level: %d \n", rfidTagNumber, rfidLevel);
		break;

	case 0x03:
		switch(key1Type) {
		case ODD_DIGIT:
			column = 0x00;
			break;
		case EVEN_DIGIT:
			column = 0x02;
			break;
		case LETTER_AM:
			column = 0x01;
			break;
		case LETTER_NZ:
			column = 0x03;
			break;
		}
		switch(key2Type) {
		case ODD_DIGIT:
			row = 0x00;
			break;
		case EVEN_DIGIT:
			row = 0x02;
			break;
		case LETTER_AM:
			row = 0x03;
			break;
		case LETTER_NZ:
			row = 0x01;
			break;
		}
		rfidTagNumber = keyL3Table[row][column];
//		printf("Calcualted : %d , Key Level: %d \n", rfidTagNumber, rfidLevel);
		break;

	default:
		break;
	}
	return rfidTagNumber;

}
int main(int argc, const char *argv[])
{
	(void) argc;
	int error;
	int i;
	char serialNumber[16];
	int rfidKeyLevel1;
	int rfidKeyLevel2;
	int rfidKeyLevel3;
	int keyType;
	int result;
//	for(i = 1; i < argc; i++)
//	{
//		serialNumber[i] = argv[i];
//	}
	if (argc >= 2) {
		memcpy(&serialNumber[0], argv[1], 8);
//		printf("Serial Number: %s argc: %d \n", serialNumber, argc);
		rfidKeyLevel1 = get_key_number(0x01, &serialNumber[0]);
//		printf("Rfid Number : %d \n", rfidKeyLevel1);
		rfidKeyLevel2 = get_key_number(0x02, &serialNumber[0]);
//		printf("Rfid Number : %d \n", rfidKeyLevel2);
		rfidKeyLevel3 = get_key_number(0x03, &serialNumber[0]);
//		printf("Rfid Number : %d \n", rfidKeyLevel3);
	}
	else {
		printf("ERROR: Serial Number not provided \n");
	}

	display_Oled(1,"-ESCAPE", "  BOOM ", "ONE", "PRESS");
	bcm2835_delay(10);
	display_Oled(1,"- FIRST", "  KEY ", "ONE", "DECODE");
	result = read_nfc_tag(rfidKeyLevel1);
	if (result == 0x00) {
		display_Oled(1,"WRONG", "  KEY ", "ONE", "DECODE");
		printf("FALSE");
		exit(0x03);
	}

	display_Oled(1,"SECOND", "  KEY ", "ONE", "DECODE");
	result = read_nfc_tag(rfidKeyLevel2);
	if (result == 0x00) {
		display_Oled(1,"WRONG", "  KEY ", "ONE", "DECODE");
		printf("FALSE");
		exit(0x03);
	}

	display_Oled(1,"-THIRD", "  KEY ", "ONE", "DECODE");
	result = read_nfc_tag(rfidKeyLevel3);
	if (result == 0x00) {
		display_Oled(1,"WRONG", "  KEY ", "ONE", "DECODE");
		printf("FALSE");
		exit(0x03);
	}
	display_Oled(1,"*PASSED*", " ##### ", "ONE", "DECODE");
	printf("TRUE");
	exit(0x00);

//	exit(EXIT_FAILURE);

//	time_t now;
//    struct tm *timenow;
//    time(&now);
//	timenow = localtime(&now);
//	GUI_DisChar(0, 16, value[timenow->tm_hour / 10], &Font24, FONT_BACKGROUND, WHITE);
//	GUI_DisChar(16, 16, value[timenow->tm_hour % 10], &Font24, FONT_BACKGROUND, WHITE);
//	GUI_DisChar(32, 16, ':', &Font24, FONT_BACKGROUND, WHITE);
//	GUI_DisChar(48, 16, value[timenow->tm_min / 10],  &Font24, FONT_BACKGROUND, WHITE);
//	GUI_DisChar(64, 16, value[timenow->tm_min % 10],  &Font24, FONT_BACKGROUND, WHITE);
//	GUI_DisChar(80, 16, ':',  &Font24, FONT_BACKGROUND, WHITE);
//	GUI_DisChar(96, 16, value[timenow->tm_sec / 10],  &Font24, FONT_BACKGROUND, WHITE);
//	GUI_DisChar(112, 16, value[timenow->tm_sec % 10],  &Font24, FONT_BACKGROUND, WHITE);
//
//	OLED_Display();
//	OLED_Clear(0x00);
	
	//3.System Exit
	System_Exit();
	return 0;
	
}

