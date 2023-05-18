/*****************************************************************************
 *   Peripherals such as temp sensor, light sensor, accelerometer,
 *   and trim potentiometer are monitored and values are written to
 *   the OLED display.
 *
 *   Copyright(C) 2010, Embedded Artists AB
 *   All rights reserved.
 *
 ******************************************************************************/

//############################//
//                            //
//          INCLUDY           //
//                            //
//############################//

//	PLYTKA
#include "lpc17xx_pinsel.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_ssp.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_timer.h"

//	SPEAKER
#include "lpc17xx_dac.h"

// UART
#include "uart2.h"

//	REAL TIME CLOCK
#include "lpc17xx_rtc.h"

#include "pca9532.h"
#include "lpc17xx_uart.h"

//	KOLORY DIÓD
#include "rgb.h"

//	MMC
#include "../inc/diskio.h"
#include "../inc/ff.h"


//	OLED
#include "oled.h"

//############################//
//                            //
//           DEFINE           //
//                            //
//############################//


#define NOTE_PIN_HIGH() GPIO_SetValue(0, 1<<26);
#define NOTE_PIN_LOW()  GPIO_ClearValue(0, 1<<26);
#define UART_DEV LPC_UART3



//############################//
//                            //
//      ZMIENNE GLOBALNE      //
//                            //
//############################//


static uint32_t notes[] = {
        2272, // A - 440 Hz
        2024, // B - 494 Hz
        3816, // C - 262 Hz
        3401, // D - 294 Hz
        3030, // E - 330 Hz
        2865, // F - 349 Hz
        2551, // G - 392 Hz
        1136, // a - 880 Hz
        1012, // b - 988 Hz
        1912, // c - 523 Hz
        1703, // d - 587 Hz
        1517, // e - 659 Hz
        1432, // f - 698 Hz
        1275, // g - 784 Hz
};

static uint32_t msTicks = 0;
static uint8_t buf[100];

LPC_RTC_TypeDef *RTCx = (LPC_RTC_TypeDef *) LPC_RTC_BASE;

static FATFS Fatfs[1];
static uint8_t buf_mmc[22]; //21 znaki alarmu + 1 znak LF
FIL *fp;
UINT bw = 0;
DIR dir;
FRESULT res;

//############################//
//                            //
//          FUNKCJE           //
//                            //
//############################//
//############################//
//                            //
//           INITY            //
//                            //
//############################//

//############################//
//        INIT SPEAKER        //
//############################//


//############################//
//         INIT UART          //
//############################//

static void init_uart(void)
{
	PINSEL_CFG_Type PinCfg;
	UART_CFG_Type uartCfg;

	/* Initialize UART3 pin connect */
	PinCfg.Funcnum = 2;
	PinCfg.Pinnum = 0;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 1;
	PINSEL_ConfigPin(&PinCfg);

	uartCfg.Baud_rate = 115200;
	uartCfg.Databits = UART_DATABIT_8;
	uartCfg.Parity = UART_PARITY_NONE;
	uartCfg.Stopbits = UART_STOPBIT_1;

	UART_Init(UART_DEV, &uartCfg);

	UART_TxCmd(UART_DEV, ENABLE);

}

//############################//
//          INIT SSP          //
//############################//


static void init_ssp(void)
{
	SSP_CFG_Type SSP_ConfigStruct;
	PINSEL_CFG_Type PinCfg;

	/*
	 * Initialize SPI pin connect
	 * P0.7 - SCK;
	 * P0.8 - MISO
	 * P0.9 - MOSI
	 * P2.2 - SSEL - used as GPIO
	 */
	PinCfg.Funcnum = 2;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 0;
	PinCfg.Pinnum = 7;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 8;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 9;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Funcnum = 0;
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 2;
	PINSEL_ConfigPin(&PinCfg);

	SSP_ConfigStructInit(&SSP_ConfigStruct);

	// Initialize SSP peripheral with parameter given in structure above
	SSP_Init(LPC_SSP1, &SSP_ConfigStruct);

	// Enable SSP peripheral
	SSP_Cmd(LPC_SSP1, ENABLE);

}

//############################//
//          INIT I2C          //
//############################//


static void init_i2c(void)
{
	PINSEL_CFG_Type PinCfg;

	/* Initialize I2C2 pin connect */
	PinCfg.Funcnum = 2;
	PinCfg.Pinnum = 10;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 11;
	PINSEL_ConfigPin(&PinCfg);

	// Initialize I2C peripheral
	I2C_Init(LPC_I2C2, 100000);

	/* Enable I2C1 operation */
	I2C_Cmd(LPC_I2C2, ENABLE);
}

//############################//
//          INIT ADC          //
//############################//


static void init_adc(void)
{
	PINSEL_CFG_Type PinCfg;

	/*
	 * Init ADC pin connect
	 * AD0.5 on P1.31
	 */
	PinCfg.Funcnum = 3;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Pinnum = 31;
	PinCfg.Portnum = 1;
	PINSEL_ConfigPin(&PinCfg);

	/* Configuration for ADC :
	 * 	Frequency at 0,2Mhz
	 *  ADC channel 5, no Interrupt
	 */
	ADC_Init(LPC_ADC, 200000);
	ADC_IntConfig(LPC_ADC,ADC_CHANNEL_5,DISABLE);
	ADC_ChannelCmd(LPC_ADC,ADC_CHANNEL_5,ENABLE);

}

//############################//
//          INIT MMC          //
//############################//

static int init_mmc(void)
{

	res = f_mount(0, &Fatfs[0]);
	if (res != FR_OK) {
		int i;
		i = sprintf((char*)buf_mmc, "Failed to mount 0: %d \r\n", res);
		oled_putString(1,40, buf_mmc, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
		return 1;
	}

	res = f_opendir(&dir, "/");
	if (res != FR_OK) {
		sprintf((char*)buf_mmc, "Failed to open /: %d \r\n", res);
		oled_putString(1,40, buf_mmc, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
		return 1;
	}
}

//############################//
//                            //
//          MELODIA           //
//                            //
//############################//

//############################//
//        PLAY MELODY         //
//############################//

//############################//
//         PLAY NOTE          //
//############################//

static void playNote(uint32_t note, uint32_t durationMs) {

    uint32_t t = 0;

    if (note > 0) {

        while (t < (durationMs*1000)) {
            NOTE_PIN_HIGH();
            Timer0_us_Wait(note / 2);
            //delay32Us(0, note / 2);

            NOTE_PIN_LOW();
            Timer0_us_Wait(note / 2);
            //delay32Us(0, note / 2);

            t += note;
        }

    }
    else {
        Timer0_Wait(durationMs);
        //delay32Ms(0, durationMs);
    }
}

static uint32_t getNote(uint8_t ch)
{
    if (ch >= 'A' && ch <= 'G')
        return notes[ch - 'A'];

    if (ch >= 'a' && ch <= 'g')
        return notes[ch - 'a' + 7];

    return 0;
}

static uint32_t getDuration(uint8_t ch)
{
    if (ch < '0' || ch > '9')
        return 400;

    /* number of ms */

    return (ch - '0') * 200;
}

static uint32_t getPause(uint8_t ch)
{
    switch (ch) {
        case '+':
            return 0;
        case ',':
            return 5;
        case '.':
            return 20;
        case '_':
            return 30;
        default:
            return 5;
    }
}

static void playSong(uint8_t *song) {
    uint32_t note = 0;
    uint32_t dur  = 0;
    uint32_t pause = 0;

    /*
     * A song is a collection of tones where each tone is
     * a note, duration and pause, e.g.
     *
     * "E2,F4,"
     */

    while(*song != '\0') {
        note = getNote(*song++);
        if (*song == '\0')
            break;
        dur  = getDuration(*song++);
        if (*song == '\0')
            break;
        pause = getPause(*song++);

        playNote(note, dur);
        //delay32Ms(0, pause);
        Timer0_Wait(pause);
    }
}

static uint8_t * song = (uint8_t*)"A1_";



//############################//
//                            //
//           DIODY            //
//                            //
//############################//


//############################//
//         LEDS COLOR         //
//############################//

void makeLEDsColor(uint32_t time) {
	uint16_t ledOn = 0;
	    uint32_t count = 0;
	    uint32_t delay = 40;

		PINSEL_CFG_Type PinCfg;

		/* Initialize I2C2 pin connect */
		PinCfg.Funcnum = 2;
		PinCfg.Pinnum = 10;
		PinCfg.Portnum = 0;
		PINSEL_ConfigPin(&PinCfg);
		PinCfg.Pinnum = 11;
		PINSEL_ConfigPin(&PinCfg);

		// Initialize I2C peripheral
		I2C_Init(LPC_I2C2, 100000);

		/* Enable I2C1 operation */
		I2C_Cmd(LPC_I2C2, ENABLE);

		pca9532_init();

	    if(time > 3){
	         for(int i=0; i<50; i++) {
	            if (count < 8)
	                ledOn |= (1 << count);

	            pca9532_setLeds(ledOn, 0);


	            if (count >= 7){
	                count = 0;

	                pca9532_setLeds(0, 0xffff);
	                break;
	            }
	            count++;


	            Timer0_Wait(delay);
	    }
	    }
	    else{
	        count = 8;
	         for(int i=0; i<50; i++) {
	            if (count < 16 )
	                ledOn |= (1 << count);


	            pca9532_setLeds(ledOn, 0);


	            if (count >= 16 ){
	                count = 7;

	                pca9532_setLeds(0, 0xffff);
	                break;
	            }
	            count++;


	            Timer0_Wait(delay);
	    }
	    }

}

//############################//
//           DIODA            //
//############################//

static void colorRgbDiode(uint32_t time){

    rgb_init();

    if (time > 3)
    {
        rgb_setLeds(RGB_RED|RGB_GREEN|0);

    }
    else
    {
        rgb_setLeds(0|RGB_GREEN|0);
    }

}

//############################//
//                            //
//      REAL TIME CLOCK       //
//                            //
//############################//

void display_time() {
    uint32_t hour, minute, second;
    hour = RTC_GetTime(LPC_RTC, RTC_TIMETYPE_HOUR);
    minute = RTC_GetTime(LPC_RTC, RTC_TIMETYPE_MINUTE);
    second = RTC_GetTime(LPC_RTC, RTC_TIMETYPE_SECOND);
    snprintf(buf, 9, "%02d:%02d:%02d", hour, minute, second);
}

//############################//
//                            //
//            OLED            //
//                            //
//############################//

static void intToString(int value, uint8_t* pBuf, uint32_t len, uint32_t base)
{
    static const char* pAscii = "0123456789abcdefghijklmnopqrstuvwxyz";
    int pos = 0;
    int tmpValue = value;

    // the buffer must not be null and at least have a length of 2 to handle one
    // digit and null-terminator
    if (pBuf == NULL || len < 2)
    {
        return;
    }

    // a valid base cannot be less than 2 or larger than 36
    // a base value of 2 means binary representation. A value of 1 would mean only zeros
    // a base larger than 36 can only be used if a larger alphabet were used.
    if (base < 2 || base > 36)
    {
        return;
    }

    // negative value
    if (value < 0)
    {
        tmpValue = -tmpValue;
        value    = -value;
        pBuf[pos++] = '-';
    }

    // calculate the required length of the buffer
    do {
        pos++;
        tmpValue /= base;
    } while(tmpValue > 0);


    if (pos > len)
    {
        // the len parameter is invalid.
        return;
    }

    pBuf[pos] = '\0';

    do {
        pBuf[--pos] = pAscii[value % base];
        value /= base;
    } while(value > 0);

    return;

}

//############################//
//                            //
//           TIMER            //
//                            //
//############################//

void SysTick_Handler(void) {
    msTicks++;
}

static uint32_t getTicks(void)
{
    return msTicks;
}

//############################//
//                            //
//                            //
//            MAIN            //
//                            //
//                            //
//############################//


int main (void)
{
//############################//
//         VARIABLES          //
//############################//

	uint8_t sw3 = 0;
	uint8_t sw4 = 0;
	uint8_t sw3_pressed=0;
	uint8_t stop_timer=0;
	uint8_t hour = 15;
	uint8_t minute = 3;
	uint8_t second = 47;
	uint8_t day = 21;
	uint8_t month = 5;
	uint16_t year = 2023;

	uint32_t s;
	uint32_t ms;

	uint32_t cnt = 0;
	uint32_t sampleRate = 0;
	uint32_t delay = 0;
	uint32_t turnOff = 0;

	uint32_t* ptrCnt = &cnt;
	uint32_t* ptrSampleRate = &sampleRate;
	uint32_t* ptrDelay = &delay;
	uint32_t* ptrTurnOff = &turnOff;


//############################//
//           INITS            //
//############################//

	init_i2c();
	init_ssp();
	init_adc();
	if(init_mmc() == 1) return 1;
	



    oled_init();
    init_uart();



//############################//
//      REAL TIME CLOCK       //
//############################//


//    RTC_Init(LPC_RTC);

//    RTC_SetTime(LPC_RTC, RTC_TIMETYPE_HOUR, 0);
//    RTC_SetTime(LPC_RTC, RTC_TIMETYPE_MINUTE, 0);
//    RTC_SetTime(LPC_RTC, RTC_TIMETYPE_SECOND, 0);


//    RTC_Cmd(LPC_RTC, 1);


//############################//
//       ERROR CAPTURE        //
//############################//

	if (SysTick_Config(SystemCoreClock / 1000)) {
		    while (1);  // Capture error
	}

//############################//
//            OLED            //
//############################//


    oled_clearScreen(OLED_COLOR_WHITE);

    oled_putString(1,9,  (uint8_t*)"Timer:", OLED_COLOR_BLACK, OLED_COLOR_WHITE);

//############################//
//            MMC             //
//############################//
   FRESULT a =f_open(&fp, "log.txt", FA_OPEN_ALWAYS|FA_WRITE);
	if(a == FR_OK) {
		snprintf(buf_mmc, sizeof(buf_mmc), "%02d:%02d:%02d %02d.%02d.%04dr.", hour, minute, second, day, month, year);
		if(f_write(&fp, buf_mmc, sizeof(buf_mmc), &bw) == FR_OK) {
			oled_putString(27,41, "Zapisano.", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
		} else {
			oled_putString(1,41, "Zapis nieudany.", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
		}
	}
	else {
		oled_putString(1,41, "Nic nie udane.", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	}
	f_close(&fp);


//############################//
//         MAIN LOOP          //
//############################//
    while(1) {

		//############################//
		//        BUTTON VALUE        //
		//############################//

		sw3 = ((GPIO_ReadValue(0) >> 4 ) & 0x01);
		sw4 = ((GPIO_ReadValue(1) >> 31) & 0x01);

		if (sw3 != 0) {
			sw3_pressed = 0;
		}

		if (sw3 == 0 && sw3_pressed == 0) {
			sw3_pressed = 1;//	BUTTON PRESSED

			//############################//
			//        START TIMER         //
			//############################//
			if(stop_timer == 0) {

				msTicks = 0;
				stop_timer = 1;

			}
			else{
				//############################//
				//         STOP TIMER         //
				//############################//
				stop_timer = 0;
				s = getTicks();
				ms = s%1000;
				s/=1000;

				colorRgbDiode(s);//	DIODA
				makeLEDsColor(s);//	LEDY

				snprintf(buf, 9, "%2d.%3d", s, ms);//	CONVERT MEASURED TIME
				oled_putString(40,9, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);//	PRINT TIME ON OLED

				//############################//
				//        PLAY MELODY         //
				//############################//
				playSong(song);
			}
		}

    }

}

void check_failed(uint8_t *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while(1);

}
