/*!
 *  Projekt: Licznik osob wchodzacych lub wychodzacych z pokoju
 *
 *  Autorzy:
 *  		Kamil Wlodarczyk
 *  		Kacper Pietrzak
 *  		Kamil Ruszkiewicz
 *  		Robert Laski
 *
 *  Przedmiot: Systemy wbudowane
 *  Semestr: IV
 *  Rok: 2022/23
 *
 */



//	PLYTKA
#include "lpc17xx_pinsel.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_ssp.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_uart.h"

//	REAL TIME CLOCK
#include "lpc17xx_rtc.h"

#include "pca9532.h"

//	KOLORY DIÓD
#include "rgb.h"

//	MMC
#include "../inc/diskio.h"
#include "../inc/ff.h"

//EEPROM
#include "../inc/eeprom.h"

//	OLED
#include "oled.h"

//	TEMPERATURE | LIGHT | ACCELEROMETER
#include "temp.h"
#include "light.h"
#include "acc.h"

#include "easyweb.h"
#include "tcpip.h"


#define NOTE_PIN_HIGH() GPIO_SetValue(0, (unsigned int)1<<26);
#define NOTE_PIN_LOW()  GPIO_ClearValue(0, (unsigned int)1<<26);
#define EEPROMLen 20


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



static uint32_t msTicks = 0;
static uint8_t tBuf[20]; //	Bufor do przechowywania temperatury
static uint8_t buf[100]; //	Bufor do przechowania czasu wejscia
static uint8_t pBuf[EEPROMLen]; //	Bufor do przechowania liczby ludzi

volatile uint32_t TimeTick = 0;
volatile uint32_t TimeTick2 = 0;

LPC_RTC_TypeDef *RTCx = (LPC_RTC_TypeDef *) LPC_RTC_BASE;


static uint8_t buf_mmc[22]; //21 znaki alarmu + 1 znak LF
FIL *fp;
UINT bw = 0;
UINT br = 0;
DIR dir;
FRESULT res;

uint8_t * buzzer_sound = (uint8_t*)"A1_";
uint8_t * erase_sound = (uint8_t*)"B3_";

void initUART0(void);
void U0Write(char txData);
char U0Read(void);
uint16_t GetTimeFromUART();
void writeUARTMsg(char msg[]);
static void init_ssp(void);
static void init_adc(void);
static void init_i2c(void);
static int init_mmc(void);
void save_log(const uint8_t log[], const uint8_t filename[], uint8_t SDFlag);
void init_speaker(void);
static void playNote(uint32_t note, uint32_t durationMs);
static uint32_t getNote(uint8_t ch);
static uint32_t getDuration(uint8_t ch);
static uint32_t getPause(uint8_t ch);
static void playSong(uint8_t *song);
void makeLEDsColor(uint32_t status);
static void colorRgbDiode(uint8_t signal1, uint8_t signal2);
void display_time();
static void intToString(int value, uint8_t* pBuf, uint32_t len, uint32_t base);
int arrayToInt(uint8_t arr[]);
void SysTick_Handler_Timer(void);
static uint32_t getTicks(void);
int main(void);


/*!
 *  @brief    inicjazacja bloku UART0
 */

void initUART0(void)
{

	LPC_PINCON->PINSEL0 |= (1<<4) | (1<<6); //Select TXD0 and RXD0 function for P0.2 & P0.3!

//	LPC_SC->PCONP |= 1<<3;

	LPC_UART0->LCR = 3 | DLAB_BIT ; /* 8 bits, no Parity, 1 Stop bit & DLAB set to 1  */
	LPC_UART0->DLL = 12;
	LPC_UART0->DLM = 0;

	LPC_UART0->FCR |= Ux_FIFO_EN | Rx_FIFO_RST | Tx_FIFO_RST;
	LPC_UART0->FDR = (MULVAL<<4) | DIVADDVAL; /* MULVAL=15(bits - 7:4) , DIVADDVAL=2(bits - 3:0)  */
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
	while(!(LPC_UART0->LSR & RDR)){
		if (((GPIO_ReadValue(0) >> 4) & 0x01) == 0)
		{
			return 'n';
		}
	}; //wait until data arrives in Rx FIFO
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
			U0Write(ENTER);
			U0Write(LINE_FEED);
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
}


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

/*!
 *
 *  @brief    Inicjalizacja ADC
 *
 */


static void init_adc(void)
{
	PINSEL_CFG_Type PinCfg;

	/*
	 * Init ADC pin connect
	 * AD0.0 on P0.23
	 */
	PinCfg.Funcnum = 1;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Pinnum = 23;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);

	/* Configuration for ADC :
	 * 	Frequency at 0.2Mhz
	 *  ADC channel 0, no Interrupt
	 */
	ADC_Init(LPC_ADC, 200000);
	ADC_IntConfig(LPC_ADC,ADC_CHANNEL_0,DISABLE);
	ADC_ChannelCmd(LPC_ADC,ADC_CHANNEL_0,ENABLE);

}


/*!
 *
 *  @brief    Inicjalizacja I2C
 *
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


/*!
 *  @brief    Inicjalizacja MMC
 *			  Montowanie systemu plikow oraz otwieranie katalogu
 */

static int init_mmc(void)
{
	static FATFS Fatfs[1];
	res = f_mount(&Fatfs[0],"", 0);
	int result = 0;
	if (res != FR_OK) {
		int i;
		i = sprintf(buf_mmc, "Failed to mount 0: %d \r\n", res);
		oled_putString(1,40, buf_mmc, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
		result = 1;
	}

	res = f_opendir(&dir, "/");
	if (res != FR_OK) {
		(void)sprintf(buf_mmc, "Failed to open /: %d \r\n", res);
		oled_putString(1,40, buf_mmc, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
		result = 1;
	}
	if (result == 1)
	{
		const char msg[] = "Blad inicjalizacji SD!\n\r";
		writeUARTMsg(msg);
	}
	return result;
}

/*!
 *  @brief    Procedura ma za zadanie zapisać komunikat do pliku
 *			  ktory zostanie umieszczony na karcie SD
 *  @param log
 *             Komunikat ktory ma zostac zapisany w pliku
 *  @param filename
 *             Nazwa pliku do ktorego ma zostac zapisany komunikat
 */

void save_log(const uint8_t log[], const uint8_t filename[], uint8_t SDFlag)
{
	if (SDFlag == 0)
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
}


/*!
 *
 *  @brief    Inicjalizacja pinow odpowiedzialnych za dzialanie speakera
 *
 */


void init_speaker(void)
{
	GPIO_SetDir(2, 1<<0, 1);
	GPIO_SetDir(2, 1<<1, 1);

	GPIO_SetDir(0, 1<<27, 1);
	GPIO_SetDir(0, 1<<28, 1);
	GPIO_SetDir(2, 1<<13, 1);
	GPIO_SetDir(0, 1<<26, 1);

	GPIO_ClearValue(0, 1<<27); //LM4811-clk
	GPIO_ClearValue(0, 1<<28); //LM4811-up/dn
	GPIO_ClearValue(2, 1<<13); //LM4811-shutdn
}


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

/*!
 *  @brief    Sprawdza jak dluga powinna byc pauza po nucie
 *  @param ch
 *            znak w zaleznosci od ktorego zwracamy wartosc
 *
 *  @returns  Domyslnie zwraca 5 jednakze w przypadku rozpoznanego znaku
 *  		  zawraca inne wartosci pauzy
 *
 */

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

/*!
 *  @brief    Gra podany utwor
 *  @param song
 *             wskaznik na uint8_t ktory przechowuje tony utworu
 *
 */

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

/*!
 *  @brief    Tworzy sekwencje zapalania sie ledow w zaleznosci od parametru
 *
 *  @param status
 *             zawiera informacje o tym czy uzytkownik wszedl czy wyszedl z pomieszczenia
 *
 */

void makeLEDsColor(uint32_t status)
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

	if((int)status == 1){
		 for(int i=0; i<50; i++) {
			if ((int)count < 8){
				ledOn |= ((unsigned int)1 << count);
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
				ledOn |= ((unsigned int)1 << count);
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


/*!
 *  @brief    Modyfikuje kolor diody w zaleznosci od stanu czujnikow
 *
 *  @param signal1
 *             sygnal pochodzacy z pierwszego czujnika
 *
 *	@param signal2
 *             sygnal pochdzacy z drugiego czujnika
 */

static void colorRgbDiode(uint8_t signal1, uint8_t signal2)
{
    rgb_init();

    if (signal1 == 1 || signal2 == 1)
    {
        rgb_setLeds(RGB_RED|RGB_GREEN|0);
    }
    else
    {
        rgb_setLeds(0|RGB_GREEN|0);
    }

}

/*!
 *  @brief    Pobiera dane z RTC i modyfikuje globalny bufor
 *  		  ktory przechowuje informacje o aktualnej godzinie
 *
 */

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


static void intToString(int value, uint8_t* pBuf, uint32_t len, uint32_t base)
{
    static const char* pAscii = "0123456789abcdefghijklmnopqrstuvwxyz";
    int pos = 0;
    int tmpValue = value;

    // the buffer must not be null and at least have a length of 2 to handle one
    // digit and null-terminator
    if ((pBuf == NULL) || ((int)len < 2))
    {
        return;
    }

    // a valid base cannot be less than 2 or larger than 36
    // a base value of 2 means binary representation. A value of 1 would mean only zeros
    // a base larger than 36 can only be used if a larger alphabet were used.
    if (((int)base < 2) || ((int)base > 36))
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
//        (int)pBuf[--pos] = (int)pAscii[value % (int)base];
        pBuf[--pos] = (int)pAscii[value % (int)base];
        value /= base;
    } while(value > 0);

    return;

}

/*!
 *  @brief    Zamienia tablice znakow w ktorej znajduja sie pojedyncze cyfry
 *  		  na liczba calkowita
 *  @param arr
 *            Tablica znakow ktora przechowuje znaki cyfr
 *
 *  @returns  Liczba calkowita uzyskana z tablicy
 *
 */

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

/*!
 *  @brief    Inkrementuje timer
 */

void SysTick_Handler(void)
{
    msTicks++;

	TimeTick++;		// Increment first SysTick counter
	TimeTick2++;	// Increment second SysTick counter

	// After 100 ticks (100 x 10ms = 1sec)
	if (TimeTick >= 100) {
	  TimeTick = 0;	// Reset counter
	  LPC_GPIO1->FIOPIN ^= 1 << 25;	// Toggle user LED
	}
	// After 20 ticks (20 x 10ms = 1/5sec)
	if (TimeTick2 >= 20) {
	  TimeTick2 = 0; // Reset counter
	  TCPClockHandler();  // Call TCP handler
	}
}

/*!
 *  @brief    Pobiera wartosc timera
 *
 *  @returns  Liczba calkowita, wartosc timera (w milisekundach)
 *
 */

static uint32_t getTicks(void)
{
    return msTicks;
}


/*!
 *  @brief	  Wyswietla na ekranie informacje o przejsciu osoby
 *
 *  @param walk_time
 *            Czas przejscia osoby
 *
 *  @param liczbaOsob
 *  		  Ile osob aktualnie znajduje sie w pokoju
 *
 */

void oledInfo(uint16_t walk_time, uint8_t liczbaOsob)
{
	uint16_t s = walk_time;
	uint16_t ms = (int)s % 1000;
	s /= 1000;

	(void)snprintf(buf, 9, "%2d.%3d", s, ms);//	CONVERT MEASURED TIME
	oled_putString(40, 9, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);//	PRINT TIME ON OLED

	(void)snprintf(pBuf, 9, "%2d", liczbaOsob);
	oled_putString(70, 20, pBuf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);

}


/*!
 *  @brief    Zeruje tablice
 *
 *  @param arr
 *            Tablica do wyzerowania
 *
 *
 */

void clearArray(uint8_t arr[])
{
	for (int i = 0; i < 4; i++)
	{
		arr[i] = 0;
	}
}

/*!
 *  @brief    Glowna funkcja programu
 */

int main(void)
{
	//############################//
	//         VARIABLES          //
	//############################//


	uint8_t sw3 = 0;
	uint8_t sw3_pressed = 0;

	uint8_t signal1 = 0;
	uint8_t signal2 = 0;

	uint8_t hour = 0;
	uint8_t minute = 0;
	uint8_t second = 0;
	uint8_t day = 1;
	uint8_t month = 1;
	uint16_t year = 1900;

	uint8_t liczbaOsob = 0;
	uint16_t offset = 240;
	uint8_t len = 0;

	uint16_t walk_time;
	uint8_t signal_num[10] = {0};
	uint16_t signal_time[10] = {0};
	uint8_t index = 0;
	uint8_t signal2_saved;
	uint8_t signal1_saved;
	uint16_t timestamp;
	uint16_t MIN_ENTRY_TIME = 300;

	uint8_t SDFlag = 0;

	uint32_t temperature = 0;
	uint32_t light = 0;

	int32_t xoff = 0;
	int32_t yoff = 0;
	int32_t zoff = 0;

	int8_t x = 0;
	int8_t y = 0;
	int8_t z = 0;


	//############################//
	//           INITS            //
	//############################//

	init_i2c();
	init_ssp();
	init_adc();
	oled_init();
	eeprom_init();
	initUART0();
	init_speaker();
	temp_init(&getTicks);
	light_init();
	acc_init();
	TCPLowLevelInit();

	light_enable();
	light_setRange(LIGHT_RANGE_4000);

	 /*
	 * Assume base board in zero-g position when reading first value.
	 */
	acc_read(&x, &y, &z);
	xoff = 0-x;
	yoff = 0-y;
	zoff = 64-z;

	//############################//
	//            OLED            //
	//############################//


	oled_clearScreen(OLED_COLOR_WHITE);

	oled_putString(1, 10, "Podaj date", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	oled_putString(1, 20, "i godzine", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	oled_putString(1, 30, "Szczegoly w", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	oled_putString(1, 40, "terminalu", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	oled_putString(1, 50, "SW3 by pominac", OLED_COLOR_BLACK, OLED_COLOR_WHITE);

//	SDFlag = init_mmc();


	//############################//
	//      REAL TIME CLOCK       //
	//############################//

	writeUARTMsg("\n\rWprowadz date oraz godzine. Jesli chcesz pominac wpisz \'n\'\n\r"
			"Spacja zatwierdza wybor a wprowadzone dane podaj wedlug ponizszego schematu\n\r"
			"Dzien Miesiac Rok Godzina Minuta Sekunda\n\r");


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
	//            MMC             //
	//############################//
	oled_clearScreen(OLED_COLOR_WHITE);
	oled_putString(1, 10, "Timer:", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	oled_putString(1, 20, "IleOsob:", OLED_COLOR_BLACK, OLED_COLOR_WHITE);

	(void)snprintf(buf_mmc, sizeof(buf_mmc), "%02d:%02d:%02d %02d.%02d.%04dr.\n", hour, minute, second, day, month, year);
	save_log(buf_mmc, "log.txt", SDFlag);

	//############################//
	//           EEPROM           //
	//############################//

	len = eeprom_read(pBuf, offset, EEPROMLen);

	if ((int)len != (int)EEPROMLen)
	{
		(void)snprintf(buf_mmc, sizeof(buf_mmc), "%02d:%02d:%02d %02d.%02d.%04dr.", hour, minute, second, day, month, year);
		save_log(buf_mmc, "log.txt", SDFlag);
		save_log("\nBlad EEPROM\n", "log.txt", SDFlag);
		writeUARTMsg("\nBlad EEPROM\n");

		FRESULT a = f_open(&fp, "ludzie.txt", FA_READ);
		if (a == FR_OK) {
			if (f_read(&fp, pBuf, EEPROMLen, &br) == FR_OK) {
				(void)snprintf(buf_mmc, sizeof(buf_mmc), "%02d:%02d:%02d %02d.%02d.%04dr.", hour, minute, second, day, month, year);
				save_log(buf_mmc, "log.txt", SDFlag);
				save_log("\nOdczyt z SD liczby ludzi\n", "log.txt", SDFlag);
				writeUARTMsg("\nOdczyt z SD liczby ludzi\n");
				liczbaOsob = arrayToInt(pBuf);
			}
			else {
				(void)snprintf(buf_mmc, sizeof(buf_mmc), "%02d:%02d:%02d %02d.%02d.%04dr.", hour, minute, second, day, month, year);
				save_log(buf_mmc, "log.txt", SDFlag);
				save_log("\nBlad odczytu z SD liczby ludzi\n", "log.txt", SDFlag);
				writeUARTMsg("\nBlad odczytu z SD liczby ludzi\n");
			}
		}
		else {
			(void)snprintf(buf_mmc, sizeof(buf_mmc), "%02d:%02d:%02d %02d.%02d.%04dr.", hour, minute, second, day, month, year);
			save_log(buf_mmc, "log.txt", SDFlag);
			oled_putString(1, 41, "\nBlad SD\n", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
			writeUARTMsg("\nBlad SD\n");
		}
		f_close(&fp);

	}
	else
	{
		liczbaOsob = arrayToInt(pBuf);
		(void)snprintf(buf_mmc, sizeof(buf_mmc), "%02d:%02d:%02d %02d.%02d.%04dr.", hour, minute, second, day, month, year);
		save_log(buf_mmc, "log.txt", SDFlag);
		save_log("\nOdczyt z EEPROM liczby ludzi\n", "log.txt", SDFlag);
	}

	(void)snprintf(pBuf, 9, "%2d", liczbaOsob);
	oled_putString(70, 20, pBuf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);


	//############################//
	//         MAIN LOOP          //
	//############################//
	TCPLowLevelInit();
	HTTPStatus = 0;                                // clear HTTP-server's flag register
	TCPLocalPort = TCP_PORT_HTTP;                  // set port we want to listen to

	while (1) {
		easyweb();

		//############################//
		//            DATA            //
		//############################//

		display_time();
		oled_putString(1, 40, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);//	PRINT DATA ON OLED


		//############################//
		//     TEMPERATURE | LIGHT    //
		//############################//


		if (abs(RTC_GetTime(LPC_RTC, RTC_TIMETYPE_SECOND) - second) >= 5)
		{

			acc_read(&x, &y, &z);
			x = x+xoff;
			y = y+yoff;
			z = z+zoff;

			if (abs(x) > 10)
			{
				writeUARTMsg("Czujniki poruszaja sie szybko w osi OX\n\r");
			}
			if (abs(y) > 10)
			{
				writeUARTMsg("Czujniki poruszaja sie szybko w osi OY\n\r");
			}
			if (z < 0)
			{
				writeUARTMsg("Czujniki pokonaly sile grawitacji\n\r");
			}

			LPC_PINCON->PINSEL0 &= ~(1<<4) & ~(1<<6);

			temperature = temp_read();

			(void)snprintf(tBuf, 19, "Temperatura: %2d\n\r", temperature);

			LPC_PINCON->PINSEL0 |= (1<<4) | (1<<6);

			writeUARTMsg("\0");

			light = light_read();

			if (light < 100)
			{
				writeUARTMsg("Pokoj slabo oswietlony\n\r");
			}
			else if(light < 300)
			{
				writeUARTMsg("Pokoj srednio oswietlony\n\r");
			}
			else if (light < 600)
			{
				writeUARTMsg("Pokoj dobrze oswietlony\n\r");
			}
			else if (light < 1000)
			{
				writeUARTMsg("Pokoj bardzo dobrze oswietlony\n\r");
			}
			else
			{
				writeUARTMsg("Jest az za jasno...\n\rChyba sie cos pali\n\r");
			}

			writeUARTMsg(tBuf);

			second = RTC_GetTime(LPC_RTC, RTC_TIMETYPE_SECOND);
		}

		//############################//
		//        BUTTON VALUE        //
		//############################//

		sw3 = ((GPIO_ReadValue(0) >> 4) & 0x01);

		signal1 = ((GPIO_ReadValue(0) >> 5) & 0x01);
		signal2 = ((GPIO_ReadValue(0) >> 8) & 0x01);	//Ten blizej wlacznika

		colorRgbDiode(signal1, signal2);

		if ((int)sw3 != 0) {
			sw3_pressed = 0;
		}

		if (((int)sw3 == 0) && ((int)sw3_pressed == 0)) {
			sw3_pressed = 1;//	BUTTON PRESSED
			oled_clearScreen(OLED_COLOR_WHITE);

			oled_putString(1, 9, "Timer:", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
			oled_putString(1, 20, "IleOsob:", OLED_COLOR_BLACK, OLED_COLOR_WHITE);

			liczbaOsob = 0;
			(void)snprintf(pBuf, 9, "%2d", liczbaOsob);
			oled_putString(70, 20, pBuf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
			(void)playSong(erase_sound);
			clearArray(signal_num);

		}

		// 0 Kiedy czujniki sa dobrze podlaczone i nic nie zakloca swiatla
		// 1 Kiedy jest przerwanie


		timestamp = getTicks();


		if((int)signal2 == 1 && ((int)signal2_saved != 1))
		{
			signal_num[index] = 2;
			signal_time[index] = timestamp;
			index += 1;
			signal2_saved = 1;
		}

		if((int)signal1 == 1 && ((int)signal1_saved != 1))
		{
			signal_num[index] = 1;
			signal_time[index] = timestamp;
			index += 1;
			signal1_saved = 1;
		}

		if(index > 4){
			index = 0;
		}

		if((int)signal2 == 0)
		{
			signal2_saved = 0;
		}

		if((int)signal1 == 0)
		{
			signal1_saved = 0;
		}


		if((int)index == 2) // Kiedy mamy zapisane 2 sygnaly
		{
			//	Sprawdzamy czy sa z roznych stron oraz czy czas pomiedzy
			//	przejsciem przez nie jest wiekszy od MIN_ENTRY_TIME
			//	Oznacza to ze ktos po prostu wszedl lub wyszedl
			if(signal_num[0] != signal_num[1])
			{
				if(signal_time[1] - signal_time[0] > MIN_ENTRY_TIME) // NORMALNE PRZEJSCIE
				{
					walk_time = signal_time[1] - signal_time[0];
					if(signal_num[0] == 2)
					{
						if (liczbaOsob != 255)
						{
							liczbaOsob += 1;
						}
						(void)playSong(buzzer_sound);
						(void)oledInfo(walk_time, liczbaOsob);
						(void)makeLEDsColor(0);
						(void)snprintf(buf_mmc, sizeof(buf_mmc), "%02d:%02d:%02d %02d.%02d.%04dr.", hour, minute, second, day, month, year);
						save_log(buf_mmc, "log.txt", SDFlag);
						save_log("\n\rKtos wszedl do pomieszczenia\n\r", "log.txt", SDFlag);
						writeUARTMsg("Ktos wszedl");
					}
					else
					{
						if (liczbaOsob != 0)
						{
							liczbaOsob -= 1;
						}
						(void)playSong(buzzer_sound);
						(void)oledInfo(walk_time, liczbaOsob);
						(void)makeLEDsColor(1);
						(void)snprintf(buf_mmc, sizeof(buf_mmc), "%02d:%02d:%02d %02d.%02d.%04dr.", hour, minute, second, day, month, year);
						save_log(buf_mmc, "log.txt", SDFlag);
						save_log("\n\rKtos wyszedl z pomieszczenia\n\r", "log.txt", SDFlag);
						writeUARTMsg("Ktos wyszedl");
					}
					index = 0;
//					msTicks = 0;
					(void)clearArray(signal_num);
				}
				else // ENTRY TIME ZA MALY, PRAWDOPODOBNIE DWIE OSOBY WESZLY NA RAZ Z ROZNYCH STRON
				{
					// CZEKAMY NA TO CO ZROBIA
				}
			}
			else // DWA SYGNALY Z TEGO SAMEGO LASERA
			{
				index = 0; // PO PROSTU JE IGNORUJEMY
				(void)clearArray(signal_num);
			}
		}
		if((int)index == 4) // Kiedy mamy zapisane 4 sygnaly
							// oznacza to ze weszly 2 osoby na raz z roznych kierunkow w roznym czasie
		{
			if(signal_num[2] == signal_num[3]) // jesli dwa nastepne sygnaly sa te same
			{
				walk_time = signal_time[3] - signal_time[0];
				//	Przepuszczenie kogos w drzwiach
				if((int)signal_num[3] == 2) // jesli ostatnim sygnalem jest 2 to wychodzimy
				{
					if ((int)liczbaOsob != 0)
					{
						liczbaOsob -= 1;
					}
					(void)playSong(buzzer_sound);
					(void)oledInfo(walk_time, liczbaOsob);
					(void)makeLEDsColor(1);
					(void)snprintf(buf_mmc, sizeof(buf_mmc), "%02d:%02d:%02d %02d.%02d.%04dr.", hour, minute, second, day, month, year);
					save_log(buf_mmc, "log.txt", SDFlag);
					save_log("\n\rKtos wyszedl z pomieszczenia\n\r", "log.txt", SDFlag);
					writeUARTMsg("Ktos wyszedl");
				}
				else
				{
					if ((int)liczbaOsob != 255)
					{
						liczbaOsob += 1;
					}
					(void)playSong(buzzer_sound);
					(void)oledInfo(walk_time, liczbaOsob);
					(void)makeLEDsColor(0);
					(void)snprintf(buf_mmc, sizeof(buf_mmc), "%02d:%02d:%02d %02d.%02d.%04dr.", hour, minute, second, day, month, year);
					save_log(buf_mmc, "log.txt", SDFlag);
					save_log("\n\rKtos wszedl do pomieszczenia\n\r", "log.txt", SDFlag);
					writeUARTMsg("Ktos wszedl");
				}
				index = 0;
				(void)clearArray(signal_num);
			}
			else // jesli sygnaly sa rozne, oznacza to ze osoby weszly i wyszly
			{
				index = 0;
				(void)clearArray(signal_num);
			}
		}


		(void)snprintf(pBuf, 9, "%d.%d.%d.%d", signal_num[0], signal_num[1], signal_num[2], signal_num[3]);
		oled_putString(10, 50, pBuf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);

		(void)snprintf(pBuf, 9, "%2d", liczbaOsob);
		len = eeprom_write(pBuf, offset, EEPROMLen);
		if ((int)len != (int)EEPROMLen){
			(void)snprintf(buf_mmc, sizeof(buf_mmc), "%02d:%02d:%02d %02d.%02d.%04dr.", hour, minute, second, day, month, year);
			save_log(buf_mmc, "log.txt", SDFlag);
			save_log("\nBlad zapisu do EEPROM\n", "log.txt", SDFlag);
		}

	}

}
