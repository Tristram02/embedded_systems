/**************************************************************************//**
 * @file     lpc17xx_spi.c
 * @brief    Drivers for SSP peripheral in lpc17xx.
 * @version  1.0
 * @date     18. Nov. 2010
 *
 * @note
 * Copyright (C) 2010 NXP Semiconductors(NXP). All rights reserved.
 *
 * @par
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * products. This software is supplied "AS IS" without any warranties.
 * NXP Semiconductors assumes no responsibility or liability for the
 * use of the software, conveys no license or title under any patent,
 * copyright, or mask work right to the product. NXP Semiconductors
 * reserves the right to make changes in the software without
 * notification. NXP Semiconductors also make no representation or
 * warranty that such application will be suitable for the specified
 * use without further testing or modification.
 ******************************************************************************/

#include "../inc/lpc17xx_spi.h"

/* Macro defines for SSP SR register */
#define SSP_SR_TFE      ((uint32_t)(1<<0)) /** SSP status TX FIFO Empty bit */
#define SSP_SR_TNF      ((uint32_t)(1<<1)) /** SSP status TX FIFO not full bit */
#define SSP_SR_RNE      ((uint32_t)(1<<2)) /** SSP status RX FIFO not empty bit */
#define SSP_SR_RFF      ((uint32_t)(1<<3)) /** SSP status RX FIFO full bit */
#define SSP_SR_BSY      ((uint32_t)(1<<4)) /** SSP status SSP Busy bit */
#define SSP_SR_BITMASK	((uint32_t)(0x1F)) /** SSP SR bit mask */


/**
  * @brief  Initializes the SSP0.
  *
  * @param  None
  * @retval None 
  */
void SPI_Init (void) 
{
    /* Enable SSPI0 block */
    LPC_SC->PCONP |= (1 << 21);
    
    /* Set SSEL0 as GPIO, output high */
    LPC_PINCON->PINSEL1 &= ~(3 << 0);          /* Configure P0.16(SSEL) as GPIO */
    LPC_GPIO0->FIODIR |= (1 << 16);            /* set P0.16 as output */
    
    /* Configure other SSP pins: SCK, MISO, MOSI */
    LPC_PINCON->PINSEL0 &= ~(3UL << 30);
    LPC_PINCON->PINSEL0 |=  (2UL << 30);          /* P0.15: SCK0 */
    LPC_PINCON->PINSEL1 &= ~((3<<2) | (3<<4));
    LPC_PINCON->PINSEL1 |=  ((2<<2) | (2<<4));  /* P0.17: MISO0, P0.18: MOSI0 */
    
    /* Configure SSP0_PCLK to CCLK(100MHz), default value is CCLK/4 */
    LPC_SC->PCLKSEL1 &= ~(3 << 10);
    LPC_SC->PCLKSEL1 |=  (1 << 10);  /* SSP0_PCLK=CCLK */
    
    /* 8bit, SPI frame format, CPOL=0, CPHA=0, SCR=0 */  
    LPC_SSP0->CR0 = (0x07 << 0) |     /* data width: 8bit*/
                    (0x00 << 4) |     /* frame format: SPI */
                    (0x00 << 6) |     /* CPOL: low level */
                    (0x00 << 7) |     /* CPHA: first edge */
                    (0x00 << 8);      /* SCR = 0 */

    /* Enable SSP0 as a master */
    LPC_SSP0->CR1 = (0x00 << 0) |   /* Normal mode */
                    (0x01 << 1) |   /* Enable SSP0 */
                    (0x00 << 2) |   /* Master */
                    (0x00 << 3);    /* slave output disabled */

    /* Configure SSP0 clock rate to 400kHz (100MHz/250) */
    SPI_ConfigClockRate (SPI_CLOCKRATE_LOW);

    /* Set SSEL to high */
    SPI_CS_High ();
}

/**
  * @brief  Configure SSP0 clock rate.
  *
  * @param  SPI_CLOCKRATE: Specifies the SPI clock rate.
  *         The value should be SPI_CLOCKRATE_LOW or SPI_CLOCKRATE_HIGH.
  * @retval None 
  *
  * SSP0_CLK = CCLK / SPI_CLOCKRATE
  */
void SPI_ConfigClockRate (uint32_t SPI_CLOCKRATE)
{
    /* CPSR must be an even value between 2 and 254 */
    LPC_SSP0->CPSR = (SPI_CLOCKRATE & 0xFE);    
}

/**
  * @brief  Set SSEL to low: select spi slave.
  *
  * @param  None.
  * @retval None 
  */
void SPI_CS_Low (void)
{
    /* SSEL is GPIO, set to high.  */
    LPC_GPIO0->FIOPIN &= ~(1 << 16);            
}

/**
  * @brief  Set SSEL to high: de-select spi slave.
  *
  * @param  None.
  * @retval None 
  */
void SPI_CS_High (void)
{
    /* SSEL is GPIO, set to high.  */
    LPC_GPIO0->FIOPIN |= (1 << 16);             
}

/**
  * @brief  Send one byte via MOSI and simutaniously receive one byte via MISO.
  *
  * @param  data: Specifies the byte to be sent out.
  * @retval Returned byte.
  *
  * Note: Each time send out one byte at MOSI, Rx FIFO will receive one byte. 
  */
uint8_t SPI_SendByte (uint8_t data)
{
    /* Put the data on the FIFO */
    LPC_SSP0->DR = data;
    /* Wait for sending to complete */
    while (LPC_SSP0->SR & SSP_SR_BSY); 
    /* Return the received value */              
    return (LPC_SSP0->DR);                        
}

/**
  * @brief  Receive one byte via MISO.
  *
  * @param  None.
  * @retval Returned received byte.
  */
uint8_t SPI_RecvByte (void)
{
    /* Send 0xFF to provide clock for MISO to receive one byte */
    return SPI_SendByte (0xFF);
}

/* --------------------------------- End Of File ------------------------------ */
