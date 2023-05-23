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

//EEPROM
#include "../inc/eeprom.h"

//	OLED
#include "oled.h"

//############################//
//                            //
//           DEFINE           //
//                            //
//############################//


#define NOTE_PIN_HIGH() GPIO_SetValue(0, (unsigned int)1<<26);
#define NOTE_PIN_LOW()  GPIO_ClearValue(0, (unsigned int)1<<26);
#define UART_DEV LPC_UART3
#define EEPROMLen 20


//############################//
//            UART            //
//############################//

#define RDR			((unsigned int)1<<0)
#define THRE		((unsigned int)1<<5)
#define	MULVAL		15
#define DIVADDVAL	2
#define Ux_FIFO_EN	((unsigned int)1<<0)
#define Rx_FIFO_RST	((unsigned int)1<<1)
#define Tx_FIFO_RST ((unsigned int)1<<2)
#define DLAB_BIT	((unsigned int)1<<7)
#define LINE_FEED	0x0A
#define ENTER	0x0D


//############################//
//                            //
//      ZMIENNE GLOBALNE      //
//                            //
//############################//




static uint32_t msTicks = 0;
static uint8_t buf[100]; //	Bufor do przechowania czasu wejscia
static uint8_t pBuf[EEPROMLen]; //	Bufor do przechowania liczby ludzi

LPC_RTC_TypeDef *RTCx = (LPC_RTC_TypeDef *) LPC_RTC_BASE;


static uint8_t buf_mmc[22]; //21 znaki alarmu + 1 znak LF
FIL *fp;
UINT bw = 0;
UINT br = 0;
DIR dir;
FRESULT res;



//############################//
//                            //
//     DEKLARACJA FUNKCJI	  //
//                            //
//############################//
static void init_uart(void);
void initUART0(void);
void U0Write(char txData);
char U0Read(void);
uint16_t GetTimeFromUART();
void writeUARTMsg(char msg[]);
static void init_ssp(void);
static void init_i2c(void);
static void init_adc(void);
static int init_mmc(void);
void save_log(const uint8_t log[], const uint8_t filename[]);
static void playNote(uint32_t note, uint32_t durationMs);
static uint32_t getNote(uint8_t ch);
static uint32_t getDuration(uint8_t ch);
static uint32_t getPause(uint8_t ch);
static void playSong(uint8_t *song);
void makeLEDsColor(uint32_t time);
static void colorRgbDiode(uint32_t time);
void display_time();
static void intToString(int value, uint8_t* pBuf, uint32_t len, uint32_t base);
int arrayToInt(uint8_t arr[]);
void SysTick_Handler(void);
static uint32_t getTicks(void);
int main(void);

/*!
 *  @brief    inicjalizacja UART
 *
 */
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

/*!
 *  @brief    inicjazacja bloku UART0
 */

void initUART0(void)
{
	LPC_PINCON->PINSEL0 |= ((unsigned int)1<<4) | ((unsigned int)1<<6);

	(unsigned int)(LPC_UART0->LCR = 3) | (unsigned int)DLAB_BIT ; /* 8 bits, no Parity, 1 Stop bit & DLAB set to 1  */
	LPC_UART0->DLL = 12;
	LPC_UART0->DLM = 0;

	//LPC_UART0->IER |= ..; //Edit this if want you to use UART interrupts
	LPC_UART0->FCR |= Ux_FIFO_EN | Rx_FIFO_RST | Tx_FIFO_RST;
	LPC_UART0->FDR = ((unsigned int)MULVAL<<4) | (unsigned int)DIVADDVAL; /* MULVAL=15(bits - 7:4) , DIVADDVAL=2(bits - 3:0)  */
	LPC_UART0->LCR &= ~(DLAB_BIT);
}

/*!
 *  @brief    Przekazuje znak do THR co zapisuje dane w bloku UART0
 *
 *  @param txData
 *             zmienna typu char ktora zostanie
 *             zapisana do rejsetru THR
 */

void U0Write(char txData)
{
	while(!(LPC_UART0->LSR & THRE)){}; //wait until THR is empty
	//now we can write to Tx FIFO
	LPC_UART0->THR = txData;
}

/*!
 *  @brief    Odczytuje dane z bloku UART0
 *
 *  @returns  wartosc rejestru RBR bloku UART0
 */

char U0Read(void)
{
	while(!(LPC_UART0->LSR & RDR)){}; //wait until data arrives in Rx FIFO
	return LPC_UART0->RBR;
}


/*!
 *  @brief    Odczytuje wartosc podana poprzez UART i jest dostosowana
 *  		  do podawania daty oraz godziny
 *
 *  @returns  Zwraca wartosc liczbowa 16-bitowa
 *  @side effects:
 *            wartosc zwracana moze przekraczacz maksymalne wartosci RTC
 */

uint16_t GetTimeFromUART()
{
	uint16_t data = 0;
	while (1)
	{
		char input = U0Read(); //Read Data from Rx
		if((int)input == (int)ENTER) //Check if user pressed Enter key
		{
			//Send NEW Line Character(s) i.e. "\n"
			U0Write(ENTER); //Comment this for Linux or MacOS
			U0Write(LINE_FEED); //Windows uses CR+LF for newline.
		}
		else if((int)input == (int)'n')
		{
			return 0;
		}
		else if(input == ' ')
		{
			U0Write(input); //Tx Read Data back
			if ((int)data < 0)
			{
				const char msg[] = "Prawdopodobnie wybrales zle dane! Ustawiam je jako 1\n\r";
				writeUARTMsg(msg);
				return 1;
			}
			return data;
		}
		else
		{
			U0Write(input); //Tx Read Data back
			data = (uint16_t)data * (uint16_t)10 + ((uint16_t)input - (uint16_t)'0');
		}
	}
}

/*!
 *  @brief    Wypisuje wiadomosc zawarta w tablicy znakow do bloku UART0
 *
 *  @param msg
 *			  Tablica znakow ktora ma zostac przeslana przez funkcje
 *			  U0Write do bloku UART0 
 * 
 */

void writeUARTMsg(char msg[])
{
	int count = 0;
	while( msg[count] != '\0' )
	{
		U0Write(msg[count]);
		count++;
	}
	//Send NEW Line Character(s) i.e. "\n"
	U0Write(LINE_FEED);
	U0Write(ENTER);
	count = 0; // reset counter
}

//############################//
//          INIT SSP          //
//############################//

/*!
 *  @brief    Inicjalizacja SSP
 *
 */

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

/*!
 *  @brief    Inicjalizacja I2C
 */

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

/*!
 *  @brief    Inicjalizacja ADC
 */

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

/*!
 *  @brief    Inicjalizacja MMC
 *			  Montowanie systemu plikow oraz otwieranie katalogu
 */

static int init_mmc(void)
{
	static FATFS Fatfs[1];
	res = f_mount(&Fatfs[0],"", 0);
	if (res != FR_OK) {
		int i;
		i = sprintf(buf_mmc, "Failed to mount 0: %d \r\n", res);
		oled_putString(1,40, buf_mmc, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
		return 1;
	}

	res = f_opendir(&dir, "/");
	if (res != FR_OK) {
		(void)sprintf(buf_mmc, "Failed to open /: %d \r\n", res);
		oled_putString(1,40, buf_mmc, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
		return 1;
	}
	return 0;
}

/*!
 *  @brief    Procedura ma za zadanie zapisać komunikat do pliku
 *			  ktory zostanie umieszczony na karcie SD
 *  @param log
 *             Komunikat ktory ma zostac zapisany w pliku
 *  @param filename
 *             Nazwa pliku do ktorego ma zostac zapisany komunikat
 */

void save_log(const uint8_t log[], const uint8_t filename[])
{
	FRESULT a = f_open(&fp, filename, FA_OPEN_APPEND | FA_WRITE);
		if(a == FR_OK) {
			if(f_write(&fp, log, strlen(log), &bw) == FR_OK) {

			} else {
				oled_putString(1,41, "Zapis nieudany.", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
			}
		}
		else {
			oled_putString(1,41, "Blad SD", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
		}
		f_close(&fp);
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

/*!
 *  @brief    Zagranie nuty przez okreslony czas
 *  @param note
 *             nuta (czestotliwosc) ktora ma zostac zagrana
 *  @param durationMs
 *             czas przez jaki nuta ma byc odgrywana w mikrosekundach
 */

static void playNote(uint32_t note, uint32_t durationMs) 
{
    uint32_t t = 0;

    if ((int)note > 0) {

        while ((int)t < ((int)durationMs*1000)) {
            NOTE_PIN_HIGH();
            Timer0_us_Wait((int)note / 2);
            //delay32Us(0, note / 2);

            NOTE_PIN_LOW();
            Timer0_us_Wait((int)note / 2);
            //delay32Us(0, note / 2);

            t += note;
        }

    }
    else {
        Timer0_Wait(durationMs);
    }
}


/*!
 *  @brief    Zwraca czestotliwosc na podstawie przeslanej nuty
 *  @param ch
 *             Literal opisujacy nute
 * 
 *  @returns  Zwraca wartosc z tablicy notes[] lub 0 jesli argument
 *			  nie jest A-G lub a-g
 */

static uint32_t getNote(uint8_t ch)
{
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

    if ((int)ch >= (int)'A' && (int)ch <= (int)'G')
        return notes[(int)ch - (int)'A'];

    if ((int)ch >= (int)'a' && (int)ch <= (int)'g')
        return notes[(int)ch - (int)'a' + 7];

    return 0;
}

/*!
 *  @brief    Zwraca dlugosc trwania dzwieku na podstawie przeslanej liczby
 * 
 *  @param ch
 *             Liczba 8-bitowa na podstawie ktorej zwracana jest wartosc
 * 
 *  @returns  Jesli argument jest mniejszy od 0 lub wiekszy od 9 to zwracamy 400
 *			  W przeciwnym razie mnozymy argument razy 200
 * 
 */

static uint32_t getDuration(uint8_t ch)
{
    if ((int)ch < (int)'0' || (int)ch > (int)'9')
        return 400;

    /* number of ms */

    return ((int)ch - (int)'0') * 200;
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

static void playSong(uint8_t *song)
{
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
        if (*song == '\0'){
            break;
		}
        dur  = getDuration(*song++);
        if (*song == '\0'){
            break;
		}
        pause = getPause(*song++);

        playNote(note, dur);
        //delay32Ms(0, pause);
        Timer0_Wait(pause);
    }
}

uint8_t * buzzer_sound = (uint8_t*)"A1_";



//############################//
//                            //
//           DIODY            //
//                            //
//############################//


//############################//
//         LEDS COLOR         //
//############################//

void makeLEDsColor(uint32_t time) 
{
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

	    if((int)time > 3){
	         for(int i=0; i<50; i++) {
	            if ((int)count < 8){
	                (unsigned int)ledOn |= ((unsigned int)1 << count);
				}

	            pca9532_setLeds(ledOn, 0);


	            if ((int)count >= 7){
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
	            if ((int)count < 16 ){
	                (unsigned int)ledOn |= ((unsigned int)1 << count);
				}

	            pca9532_setLeds(ledOn, 0);


	            if ((int)count >= 16 ){
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

static void colorRgbDiode(uint32_t time)
{
    rgb_init();

    if ((int)time > 3)
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

void display_time() 
{
    uint32_t hour;
	uint32_t minute;
	uint32_t second;
    hour = RTC_GetTime(LPC_RTC, RTC_TIMETYPE_HOUR);
    minute = RTC_GetTime(LPC_RTC, RTC_TIMETYPE_MINUTE);
    second = RTC_GetTime(LPC_RTC, RTC_TIMETYPE_SECOND);
    (void)snprintf(buf, 9, "%02d:%02d:%02d", hour, minute, second);
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
    if (pBuf == NULL || (int)len < 2)
    {
        return;
    }

    // a valid base cannot be less than 2 or larger than 36
    // a base value of 2 means binary representation. A value of 1 would mean only zeros
    // a base larger than 36 can only be used if a larger alphabet were used.
    if ((int)base < 2 || (int)base > 36)
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


    if (pos > (int)len)
    {
        // the len parameter is invalid.
        return;
    }

    pBuf[pos] = '\0';

    do {
        (int)pBuf[--pos] = (int)pAscii[value % (int)base];
        value /= base;
    } while(value > 0);

    return;

}

int arrayToInt(uint8_t arr[])
{
	uint8_t number = 0;
	for (int i = 0; i < EEPROMLen; i++)
	{
		if ((int)arr[i] == (int)'\0')
			break;
		if ((int)arr[i] >= (int)'0' && (int)arr[i] <= (int)'9')
			number = (uint8_t)number * (uint8_t)10 + ((uint8_t)arr[i] - (uint8_t)'0');
	}
	return number;
}

//############################//
//                            //
//           TIMER            //
//                            //
//############################//

void SysTick_Handler(void) 
{
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


int main(void)
{
	//############################//
	//         VARIABLES          //
	//############################//

	uint8_t sw3 = 0;
	uint8_t sw3_pressed = 0;

	uint8_t signal1 = 0;
	uint8_t signal2 = 0;

	uint8_t entry = 0;
	uint8_t leave = 0;

	uint8_t hour = 0;
	uint8_t minute = 0;
	uint8_t second = 0;
	uint8_t day = 1;
	uint8_t month = 1;
	uint16_t year = 1900;

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

	uint8_t liczbaOsob = 0;
	uint16_t offset = 240;
	uint8_t len = 0;

	const char uartEnter[] = "Ktos wszedl!";
	const char uartLeave[] = "Ktos wyszedl!";
	char data = 0;




	//############################//
	//           INITS            //
	//############################//

	init_i2c();
	init_ssp();
	init_adc();
	if (init_mmc() == 1) return 1;
	eeprom_init();



	oled_init();
	initUART0();



	//############################//
	//      REAL TIME CLOCK       //
	//############################//

	const char msg[] = "Wprowadz date oraz godzine. Jesli chcesz pominac wpisz \'n\'\n\rPodaj dane w kolejnosc:\n\rDzien Miesiac Rok Godzina Minuta Sekunda\n\r";
	writeUARTMsg(msg);

	day = GetTimeFromUART();
	if ((int)day != 0)
	{
		month = GetTimeFromUART();
		year = GetTimeFromUART();
		hour = GetTimeFromUART();
		minute = GetTimeFromUART();
		second = GetTimeFromUART();
	}
	else{
		day = 1;
	}

	RTC_Init(LPC_RTC);

	RTC_SetTime(LPC_RTC, RTC_TIMETYPE_DAYOFMONTH, day);
	RTC_SetTime(LPC_RTC, RTC_TIMETYPE_MONTH, month);
	RTC_SetTime(LPC_RTC, RTC_TIMETYPE_YEAR, year);
	RTC_SetTime(LPC_RTC, RTC_TIMETYPE_HOUR, hour);
	RTC_SetTime(LPC_RTC, RTC_TIMETYPE_MINUTE, minute);
	RTC_SetTime(LPC_RTC, RTC_TIMETYPE_SECOND, second);


	RTC_Cmd(LPC_RTC, 1);


	//############################//
	//       ERROR CAPTURE        //
	//############################//

	if (SysTick_Config(SystemCoreClock / 1000) == 1) {
		while(1){};  // Capture error
	}

	//############################//
	//            OLED            //
	//############################//


	oled_clearScreen(OLED_COLOR_WHITE);

	oled_putString(1, 9, "Timer:", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	oled_putString(1, 20, "IleOsob:", OLED_COLOR_BLACK, OLED_COLOR_WHITE);

	//############################//
	//            MMC             //
	//############################//

	(void)snprintf(buf_mmc, sizeof(buf_mmc), "%02d:%02d:%02d %02d.%02d.%04dr.\n", hour, minute, second, day, month, year);
	save_log(buf_mmc, "log.txt");

	//############################//
	//           EEPROM           //
	//############################//

	len = eeprom_read(pBuf, offset, EEPROMLen);

	if ((int)len != (int)EEPROMLen)
	{
		(void)snprintf(buf_mmc, sizeof(buf_mmc), "%02d:%02d:%02d %02d.%02d.%04dr.", hour, minute, second, day, month, year);
		save_log(buf_mmc, "log.txt");
		save_log("\nBlad EEPROM\n", "log.txt");

		FRESULT a = f_open(&fp, "ludzie.txt", FA_READ);
		if (a == FR_OK) {
			if (f_read(&fp, pBuf, EEPROMLen, &br) == FR_OK) {
				(void)snprintf(buf_mmc, sizeof(buf_mmc), "%02d:%02d:%02d %02d.%02d.%04dr.", hour, minute, second, day, month, year);
				save_log(buf_mmc, "log.txt");
				save_log("\nOdczyt z SD liczby ludzi\n", "log.txt");
				liczbaOsob = arrayToInt(pBuf) - 1;
			}
			else {
				(void)snprintf(buf_mmc, sizeof(buf_mmc), "%02d:%02d:%02d %02d.%02d.%04dr.", hour, minute, second, day, month, year);
				save_log(buf_mmc, "log.txt");
				save_log("\nBlad odczytu z SD liczby ludzi\n", "log.txt");
			}
		}
		else {
			(void)snprintf(buf_mmc, sizeof(buf_mmc), "%02d:%02d:%02d %02d.%02d.%04dr.", hour, minute, second, day, month, year);
			save_log(buf_mmc, "log.txt");
			oled_putString(1, 41, "\nBlad SD\n", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
		}
		f_close(&fp);

	}
	else
	{
		liczbaOsob = arrayToInt(pBuf) - 1;
		(void)snprintf(buf_mmc, sizeof(buf_mmc), "%02d:%02d:%02d %02d.%02d.%04dr.", hour, minute, second, day, month, year);
		save_log(buf_mmc, "log.txt");
		save_log("\nOdczyt z EEPROM liczby ludzi\n", "log.txt");
	}


	//############################//
	//         MAIN LOOP          //
	//############################//
	while (1) {

		//############################//
		//            DATA            //
		//############################//

		display_time();
		oled_putString(1, 40, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);//	PRINT DATA ON OLED

		//############################//
		//        BUTTON VALUE        //
		//############################//

		sw3 = ((GPIO_ReadValue(0) >> 4) & 0x01);

		signal1 = ((GPIO_ReadValue(0) >> 5) & 0x01);
		signal2 = ((GPIO_ReadValue(0) >> 8) & 0x01);

		if ((int)sw3 != 0) {
			sw3_pressed = 0;
		}

		if ((int)sw3 == 0 && (int)sw3_pressed == 0) {
			sw3_pressed = 1;//	BUTTON PRESSED


			writeUARTMsg(uartEnter);

			//############################//
			//        PLAY MELODY         //
			//############################//
			playSong(song);

		}

		if ((int)signal1 == 0 && (int)signal2 == 1 && !leave)
		{
			msTicks = 0;
			entry = 1;
		}
		else if ((int)signal1 == 0 && (int)signal2 == 1 && leave){
			leave = 0;
			(int)liczbaOsob -= 1;
			(void)snprintf(pBuf, 9, "%2d", liczbaOsob);
			oled_putString(70, 20, pBuf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
			makeLEDsColor(5);//	LEDY
		}
		else{}


		if ((int)signal2 == 0 && (int)signal1 == 1 && entry)
		{
			entry = 0;
			s = getTicks();
			ms = (int)s % 1000;
			s /= 1000;

			colorRgbDiode(s);//	DIODA
			makeLEDsColor(1);//	LEDY

			(void)snprintf(buf, 9, "%2d.%3d", s, ms);//	CONVERT MEASURED TIME
			oled_putString(40, 9, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);//	PRINT TIME ON OLED


			(int)liczbaOsob += 1;
			(void)snprintf(pBuf, 9, "%2d", liczbaOsob);
			oled_putString(70, 20, pBuf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
		}
		else if ((int)signal2 == 0 && (int)signal1 == 1){
			leave = 1;
		}
		else{}


		len = eeprom_write(pBuf, offset, EEPROMLen);
		if ((int)len != (int)EEPROMLen){
			(void)snprintf(buf_mmc, sizeof(buf_mmc), "%02d:%02d:%02d %02d.%02d.%04dr.", hour, minute, second, day, month, year);
			save_log(buf_mmc, "log.txt");
			save_log("\nBlad zapisu do EEPROM\n", "log.txt");
		}

	}

}

void check_failed(uint8_t *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while(1){};

}
check_failed() ma parametry, które nie są używane, misra nie dopuszcza,
sorry za kod, a nie komentarz, ale zróbcie coś z tym, bo nie wiem czy ta funkcja jest obligatoryjna 
