/*
 * enet.h
 *
 *  Created on: 12 lip 2023
 *      Author: student
 */

#ifndef INC_ENET_H_
#define INC_ENET_H_

#include <stdio.h>
#include "lwip/netif.h"


#define USE_RMII
#define NO_SYS                          1

#define true	1
#define false	0

typedef void (*p_msDelay_func_t)(uint32_t);

#define ENET_MCFG_RES_MII       0x00008000		/*!< Reset MII Management Hardware */
#define ENET_MADR_PHYADDR(n)    (((n) & 0x1F) << 8)	/*!< PHY Address Field */


//typedef enum {ERROR = 0, SUCCESS = !ERROR} Status;
#define ERROR 0
#define SUCCESS 1



#define LAN8_BCR_REG        0x0	/*!< Basic Control Register */
#define LAN8_BSR_REG        0x1	/*!< Basic Status Reg */
#define LAN8_PHYID1_REG     0x2	/*!< PHY ID 1 Reg  */
#define LAN8_PHYID2_REG     0x3	/*!< PHY ID 2 Reg */
#define LAN8_PHYSPLCTL_REG  0x1F/*!< PHY special control/status Reg */

/* LAN8720 BCR register definitions */
#define LAN8_RESET          (1 << 15)	/*!< 1= S/W Reset */
#define LAN8_LOOPBACK       (1 << 14)	/*!< 1=loopback Enabled */
#define LAN8_SPEED_SELECT   (1 << 13)	/*!< 1=Select 100MBps */
#define LAN8_AUTONEG        (1 << 12)	/*!< 1=Enable auto-negotiation */
#define LAN8_POWER_DOWN     (1 << 11)	/*!< 1=Power down PHY */
#define LAN8_ISOLATE        (1 << 10)	/*!< 1=Isolate PHY */
#define LAN8_RESTART_AUTONEG (1 << 9)	/*!< 1=Restart auto-negoatiation */
#define LAN8_DUPLEX_MODE    (1 << 8)	/*!< 1=Full duplex mode */

/* LAN8720 BSR register definitions */
#define LAN8_100BASE_T4     (1 << 15)	/*!< T4 mode */
#define LAN8_100BASE_TX_FD  (1 << 14)	/*!< 100MBps full duplex */
#define LAN8_100BASE_TX_HD  (1 << 13)	/*!< 100MBps half duplex */
#define LAN8_10BASE_T_FD    (1 << 12)	/*!< 100Bps full duplex */
#define LAN8_10BASE_T_HD    (1 << 11)	/*!< 10MBps half duplex */
#define LAN8_AUTONEG_COMP   (1 << 5)	/*!< Auto-negotation complete */
#define LAN8_RMT_FAULT      (1 << 4)	/*!< Fault */
#define LAN8_AUTONEG_ABILITY (1 << 3)	/*!< Auto-negotation supported */
#define LAN8_LINK_STATUS    (1 << 2)	/*!< 1=Link active */
#define LAN8_JABBER_DETECT  (1 << 1)	/*!< Jabber detect */
#define LAN8_EXTEND_CAPAB   (1 << 0)	/*!< Supports extended capabilities */

/* LAN8720 PHYSPLCTL status definitions */
#define LAN8_SPEEDMASK      (7 << 2)	/*!< Speed and duplex mask */
#define LAN8_SPEED100F      (6 << 2)	/*!< 100BT full duplex */
#define LAN8_SPEED10F       (5 << 2)	/*!< 10BT full duplex */
#define LAN8_SPEED100H      (2 << 2)	/*!< 100BT half duplex */
#define LAN8_SPEED10H       (1 << 2)	/*!< 10BT half duplex */

#ifndef ETHARP_HWADDR_LEN
#define ETHARP_HWADDR_LEN     6
#endif



#define SIZEOF_ETH_HDR (14 + ETH_PAD_SIZE)


#define ENET_MAC1_MASK          0xcf1f		/*!< MAC1 register mask */
#define ENET_MAC1_RXENABLE      0x00000001	/*!< Receive Enable */
#define ENET_MAC1_PARF          0x00000002	/*!< Pass All Receive Frames */
#define ENET_MAC1_RXFLOWCTRL    0x00000004	/*!< RX Flow Control */
#define ENET_MAC1_TXFLOWCTRL    0x00000008	/*!< TX Flow Control */
#define ENET_MAC1_LOOPBACK      0x00000010	/*!< Loop Back Mode */
#define ENET_MAC1_RESETTX       0x00000100	/*!< Reset TX Logic */
#define ENET_MAC1_RESETMCSTX    0x00000200	/*!< Reset MAC TX Control Sublayer */
#define ENET_MAC1_RESETRX       0x00000400	/*!< Reset RX Logic */
#define ENET_MAC1_RESETMCSRX    0x00000800	/*!< Reset MAC RX Control Sublayer */
#define ENET_MAC1_SIMRESET      0x00004000	/*!< Simulation Reset */
#define ENET_MAC1_SOFTRESET     0x00008000	/*!< Soft Reset MAC */

/*
 * @brief MAC Configuration Register 2 bit definitions
 */
#define ENET_MAC2_MASK          0x73ff		/*!< MAC2 register mask */
#define ENET_MAC2_FULLDUPLEX    0x00000001	/*!< Full-Duplex Mode */
#define ENET_MAC2_FLC           0x00000002	/*!< Frame Length Checking */
#define ENET_MAC2_HFEN          0x00000004	/*!< Huge Frame Enable */
#define ENET_MAC2_DELAYEDCRC    0x00000008	/*!< Delayed CRC Mode */
#define ENET_MAC2_CRCEN         0x00000010	/*!< Append CRC to every Frame */
#define ENET_MAC2_PADCRCEN      0x00000020	/*!< Pad all Short Frames */
#define ENET_MAC2_VLANPADEN     0x00000040	/*!< VLAN Pad Enable */
#define ENET_MAC2_AUTODETPADEN  0x00000080	/*!< Auto Detect Pad Enable */
#define ENET_MAC2_PPENF         0x00000100	/*!< Pure Preamble Enforcement */
#define ENET_MAC2_LPENF         0x00000200	/*!< Long Preamble Enforcement */
#define ENET_MAC2_NOBACKOFF     0x00001000	/*!< No Backoff Algorithm */
#define ENET_MAC2_BP_NOBACKOFF  0x00002000	/*!< Backoff Presurre / No Backoff */
#define ENET_MAC2_EXCESSDEFER   0x00004000	/*!< Excess Defer */

#define ENET_COMMAND_RXENABLE           0x00000001		/*!< Enable Receive */
#define ENET_COMMAND_TXENABLE           0x00000002		/*!< Enable Transmit */
#define ENET_COMMAND_REGRESET           0x00000008		/*!< Reset Host Registers */
#define ENET_COMMAND_TXRESET            0x00000010		/*!< Reset Transmit Datapath */
#define ENET_COMMAND_RXRESET            0x00000020		/*!< Reset Receive Datapath */
#define ENET_COMMAND_PASSRUNTFRAME      0x00000040		/*!< Pass Runt Frames */
#define ENET_COMMAND_PASSRXFILTER       0x00000080		/*!< Pass RX Filter */
#define ENET_COMMAND_TXFLOWCONTROL      0x00000100		/*!< TX Flow Control */
#define ENET_COMMAND_RMII               0x00000200		/*!< Reduced MII Interface */
#define ENET_COMMAND_FULLDUPLEX         0x00000400		/*!< Full Duplex */

#define ENET_MCFG_SCANINC       0x00000001		/*!< Scan Increment PHY Address */
#define ENET_MCFG_SUPPPREAMBLE  0x00000002		/*!< Suppress Preamble */
#define ENET_MCFG_CLOCKSEL(n)   (((n) & 0x0F) << 2)	/*!< Clock Select Field */
#define ENET_MCFG_RES_MII       0x00008000		/*!< Reset MII Management Hardware */
#define ENET_MCFG_RESETMIIMGMT  2500000UL		/*!< MII Clock max */
#define ENET_MCMD_READ          0x00000001		/*!< MII Read */

#define ENET_IPGR_NBTOBINTEGAP2(n) ((n) & 0x7F)
#define ENET_IPGT_BTOBINTEGAP(n) ((n) & 0x7F)
#define ENET_IPGT_FULLDUPLEX (ENET_IPGT_BTOBINTEGAP(0x15))
#define ENET_IPGR_P2_DEF (ENET_IPGR_NBTOBINTEGAP2(0x12))
#define ENET_SUPP_100Mbps_SPEED 0x00000100		/*!< Reduced MII Logic Current Speed */
#define ENET_ETH_MAX_FLEN (1536)
#define ENET_CLRT_RETRANSMAX(n) ((n) & 0x0F)
#define ENET_CLRT_COLLWIN(n) (((n) & 0x3F) << 8)
#define ENET_CLRT_DEF ((ENET_CLRT_RETRANSMAX(0x0F)) | (ENET_CLRT_COLLWIN(0x37)))

#define ENET_MADR_REGADDR(n)    ((n) & 0x1F)		/*!< MII Register Address field */
#define ENET_MADR_PHYADDR(n)    (((n) & 0x1F) << 8)	/*!< PHY Address Field */

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifdef __cplusplus
  #define     __I     volatile                /*!< defines 'read only' permissions      */
#else
  #define     __I     volatile const          /*!< defines 'read only' permissions      */
#endif
#define     __O     volatile                  /*!< defines 'write only' permissions     */
#define     __IO    volatile                  /*!< defines 'read / write' permissions   */

extern void msDelay(uint32_t ms);
#define LPC_PHYDEF_PHYADDR 1

/* Selectable CPU clock sources
*/
typedef enum CHIP_SYSCTL_CCLKSRC {
	SYSCTL_CCLKSRC_SYSCLK,		/*!< Select Sysclk as the input to the CPU clock divider. */
	SYSCTL_CCLKSRC_MAINPLL,		/*!< Select the output of the Main PLL as the input to the CPU clock divider. */
} CHIP_SYSCTL_CCLKSRC_T;

#define SYSCTL_PLLSTS_ENABLED   (1 << 8)	/*!< PLL enable flag */

/* Build for 175x/6x chip family */
#define CHIP_LPC175X_6X

/**
 * PLL source clocks
 */
typedef enum CHIP_SYSCTL_PLLCLKSRC {
	SYSCTL_PLLCLKSRC_IRC,			/*!< PLL is sourced from the internal oscillator (IRC) */
	SYSCTL_PLLCLKSRC_MAINOSC,		/*!< PLL is sourced from the main oscillator */
#if defined(CHIP_LPC175X_6X)
	SYSCTL_PLLCLKSRC_RTC,			/*!< PLL is sourced from the RTC oscillator */
#else
	SYSCTL_PLLCLKSRC_RESERVED1,
#endif
	SYSCTL_PLLCLKSRC_RESERVED2
} CHIP_SYSCTL_PLLCLKSRC_T;




#if defined(CHIP_LPC175X_6X)
#define SYSCTL_PLL0STS_ENABLED   (1 << 24)	/*!< PLL0 enable flag */
#define SYSCTL_PLL0STS_CONNECTED (1 << 25)	/*!< PLL0 connect flag */
#define SYSCTL_PLL0STS_LOCKED    (1 << 26)	/*!< PLL0 connect flag */
#define SYSCTL_PLL1STS_ENABLED   (1 << 8)	/*!< PLL1 enable flag */
#define SYSCTL_PLL1STS_CONNECTED (1 << 9)	/*!< PLL1 connect flag */
#define SYSCTL_PLL1STS_LOCKED    (1 << 10)	/*!< PLL1 connect flag */
#else
#define SYSCTL_PLLSTS_ENABLED   (1 << 8)	/*!< PLL enable flag */
#define SYSCTL_PLLSTS_LOCKED    (1 << 10)	/*!< PLL connect flag */
#endif

/*!< Internal oscillator frequency */
#if defined(CHIP_LPC175X_6X)
#define SYSCTL_IRC_FREQ (4000000)
#else
#define SYSCTL_IRC_FREQ (12000000)
#endif


/* System oscillator rate and RTC oscillator rate */
//const uint32_t OscRateIn = 12000000;
//const uint32_t RTCOscRateIn = 32768;

#define ETHTYPE_ARP       0x0806U
#define ETHTYPE_IP        0x0800U

PACK_STRUCT_BEGIN
struct eth_addr {
  PACK_STRUCT_FIELD(u8_t addr[ETHARP_HWADDR_LEN]);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END





PACK_STRUCT_BEGIN
/** Ethernet header */
struct eth_hdr {
#if ETH_PAD_SIZE
  PACK_STRUCT_FIELD(u8_t padding[ETH_PAD_SIZE]);
#endif
  PACK_STRUCT_FIELD(struct eth_addr dest);
  PACK_STRUCT_FIELD(struct eth_addr src);
  PACK_STRUCT_FIELD(u16_t type);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END

#define ENET_INT_RXOVERRUN      0x00000001	/*!< Overrun Error in RX Queue */
#define ENET_INT_RXERROR        0x00000002	/*!< Receive Error */
#define ENET_INT_RXFINISHED     0x00000004	/*!< RX Finished Process Descriptors */
#define ENET_INT_RXDONE         0x00000008	/*!< Receive Done */
#define ENET_INT_TXUNDERRUN     0x00000010	/*!< Transmit Underrun */
#define ENET_INT_TXERROR        0x00000020	/*!< Transmit Error */
#define ENET_INT_TXFINISHED     0x00000040	/*!< TX Finished Process Descriptors */
#define ENET_INT_TXDONE         0x00000080	/*!< Transmit Done */
#define ENET_INT_SOFT           0x00001000	/*!< Software Triggered Interrupt */
#define ENET_INT_WAKEUP         0x00002000	/*!< Wakeup Event Interrupt */

#define ENET_RXFILTERCTRL_ABE       0x00000002	/*!< Accept Broadcast Frames Enable */
#define TXINTGROUP (ENET_INT_TXUNDERRUN | ENET_INT_TXERROR | ENET_INT_TXDONE)
#define RXINTGROUP (ENET_INT_RXOVERRUN | ENET_INT_RXERROR | ENET_INT_RXDONE)
#define ENET_RXFILTERCTRL_APE       0x00000020	/*!< Accept Perfect Match Enable */


typedef struct {
	uint32_t Packet;		/*!< Base address of the data buffer for storing receive data */
	uint32_t Control;		/*!< Control information */
} ENET_RXDESC_T;

typedef struct {
	uint32_t StatusInfo;		/*!< Receive status return flags.*/
	uint32_t StatusHashCRC;		/*!< The concatenation of the destination address hash CRC and the source
								   address hash CRC */
} ENET_RXSTAT_T;

typedef struct {
	uint32_t Packet;	/*!< Base address of the data buffer containing transmit data */
	uint32_t Control;	/*!< Control information */
} ENET_TXDESC_T;

typedef struct {
	uint32_t StatusInfo;	/*!< Receive status return flags.*/
} ENET_TXSTAT_T;

#define ALIGNED(n)  __attribute__((aligned (n)))

#define LPC_NUM_BUFF_RXDESCS 4

/* Defines the number of descriptors used for TX */
#define LPC_NUM_BUFF_TXDESCS 4



typedef struct {
	__IO uint32_t PLLCON;					/*!< (R/W)  PLL Control Register */
	__IO uint32_t PLLCFG;					/*!< (R/W)  PLL Configuration Register */
	__I  uint32_t PLLSTAT;					/*!< (R/ )  PLL Status Register */
	__O  uint32_t PLLFEED;					/*!< ( /W)  PLL Feed Register */
	uint32_t RESERVED1[4];
} SYSCTL_PLL_REGS_T;


typedef enum {
	SYSCTL_MAIN_PLL,			/*!< Main PLL (PLL0) */
	SYSCTL_USB_PLL,				/*!< USB PLL (PLL1) */
} CHIP_SYSCTL_PLL_T;


typedef struct {
	__IO uint32_t FLASHCFG;					/*!< Offset: 0x000 (R/W)  Flash Accelerator Configuration Register */
	uint32_t RESERVED0[15];
	__IO uint32_t MEMMAP;					/*!< Offset: 0x000 (R/W)  Flash Accelerator Configuration Register */
	uint32_t RESERVED1[15];
	SYSCTL_PLL_REGS_T PLL[SYSCTL_USB_PLL + 1];		/*!< Offset: 0x080: PLL0 and PLL1 */
	__IO uint32_t PCON;						/*!< Offset: 0x0C0 (R/W)  Power Control Register */
	__IO uint32_t PCONP;					/*!< Offset: 0x0C4 (R/W)  Power Control for Peripherals Register */
#if defined(CHIP_LPC175X_6X)
	uint32_t RESERVED2[15];
#elif defined(CHIP_LPC177X_8X)
	uint32_t RESERVED2[14];
	__IO uint32_t EMCCLKSEL;				/*!< Offset: 0x100 (R/W)  External Memory Controller Clock Selection Register */
#else
	__IO uint32_t PCONP1;					/*!< Offset: 0x0C8 (R/W)  Power Control 1 for Peripherals Register */
	uint32_t RESERVED2[13];
	__IO uint32_t EMCCLKSEL;				/*!< Offset: 0x100 (R/W)  External Memory Controller Clock Selection Register */
#endif
	__IO uint32_t CCLKSEL;					/*!< Offset: 0x104 (R/W)  CPU Clock Selection Register */
	__IO uint32_t USBCLKSEL;				/*!< Offset: 0x108 (R/W)  USB Clock Selection Register */
	__IO uint32_t CLKSRCSEL;				/*!< Offset: 0x10C (R/W)  Clock Source Select Register */
	__IO uint32_t CANSLEEPCLR;				/*!< Offset: 0x110 (R/W)  CAN Sleep Clear Register */
	__IO uint32_t CANWAKEFLAGS;				/*!< Offset: 0x114 (R/W)  CAN Wake-up Flags Register */
	uint32_t RESERVED3[10];
	__IO uint32_t EXTINT;					/*!< Offset: 0x140 (R/W)  External Interrupt Flag Register */
	uint32_t RESERVED4;
	__IO uint32_t EXTMODE;					/*!< Offset: 0x148 (R/W)  External Interrupt Mode Register */
	__IO uint32_t EXTPOLAR;					/*!< Offset: 0x14C (R/W)  External Interrupt Polarity Register */
	uint32_t RESERVED5[12];
	__IO uint32_t RSID;						/*!< Offset: 0x180 (R/W)  Reset Source Identification Register */
#if defined(CHIP_LPC175X_6X) || defined(CHIP_LPC40XX)
	uint32_t RESERVED6[7];
#elif defined(CHIP_LPC177X_8X)
	uint32_t RESERVED6;
	uint32_t MATRIXARB;
	uint32_t RESERVED6A[5];
#endif
	__IO uint32_t SCS;						/*!< Offset: 0x1A0 (R/W)  System Controls and Status Register */
	__IO uint32_t RESERVED7;
#if defined(CHIP_LPC175X_6X)
	__IO uint32_t PCLKSEL[2];				/*!< Offset: 0x1A8 (R/W)  Peripheral Clock Selection Register */
	uint32_t RESERVED8[4];
#else
	__IO uint32_t PCLKSEL;				/*!< Offset: 0x1A8 (R/W)  Peripheral Clock Selection Register */
	uint32_t RESERVED9;
	__IO uint32_t PBOOST;					/*!< Offset: 0x1B0 (R/W)  Power Boost control register */
	__IO uint32_t SPIFICLKSEL;
	__IO uint32_t LCD_CFG;					/*!< Offset: 0x1B8 (R/W)  LCD Configuration and clocking control Register */
	uint32_t RESERVED10;
#endif
	__IO uint32_t USBIntSt;					/*!< Offset: 0x1C0 (R/W)  USB Interrupt Status Register */
	__IO uint32_t DMAREQSEL;				/*!< Offset: 0x1C4 (R/W)  DMA Request Select Register */
	__IO uint32_t CLKOUTCFG;				/*!< Offset: 0x1C8 (R/W)  Clock Output Configuration Register */
#if defined(CHIP_LPC175X_6X)
	uint32_t RESERVED11[6];
#else
	__IO uint32_t RSTCON[2];				/*!< Offset: 0x1CC (R/W)  RESET Control0/1 Registers */
	uint32_t RESERVED11[2];
	__IO uint32_t EMCDLYCTL;				/*!< Offset: 0x1DC (R/W) SDRAM programmable delays          */
	__IO uint32_t EMCCAL;					/*!< Offset: 0x1E0 (R/W) Calibration of programmable delays */
#endif
} LPC_SYSCTL_T;

typedef enum CHIP_SYSCTL_CLOCK {
#if defined(CHIP_LPC175X_6X)
	SYSCTL_CLOCK_RSVD0,
#else
	SYSCTL_CLOCK_LCD,					/*!< LCD clock */
#endif
	SYSCTL_CLOCK_TIMER0,			/*!< Timer 0 clock */
	SYSCTL_CLOCK_TIMER1,			/*!< Timer 1 clock */
	SYSCTL_CLOCK_UART0,				/*!< UART 0 clock */
	SYSCTL_CLOCK_UART1,				/*!< UART 1 clock */
#if defined(CHIP_LPC175X_6X)
	SYSCTL_CLOCK_RSVD5,
#else
	SYSCTL_CLOCK_PWM0,				/*!< PWM0 clock */
#endif
	SYSCTL_CLOCK_PWM1,				/*!< PWM1 clock */
	SYSCTL_CLOCK_I2C0,				/*!< I2C0 clock */
#if defined(CHIP_LPC175X_6X)
	SYSCTL_CLOCK_SPI,					/*!< SPI clock */
#else
	SYSCTL_CLOCK_UART4,				/*!< UART 4 clock */
#endif
	SYSCTL_CLOCK_RTC,					/*!< RTC clock */
	SYSCTL_CLOCK_SSP1,				/*!< SSP1 clock */
#if defined(CHIP_LPC175X_6X)
	SYSCTL_CLOCK_RSVD11,
#else
	SYSCTL_CLOCK_EMC,					/*!< EMC clock */
#endif
	SYSCTL_CLOCK_ADC,					/*!< ADC clock */
	SYSCTL_CLOCK_CAN1,				/*!< CAN1 clock */
	SYSCTL_CLOCK_CAN2,				/*!< CAN2 clock */
	SYSCTL_CLOCK_GPIO,				/*!< GPIO clock */
#if defined(CHIP_LPC175X_6X)
	SYSCTL_CLOCK_RIT,				/*!< RIT clock */
#else
	SYSCTL_CLOCK_SPIFI,				/*!< SPIFI clock */
#endif
	SYSCTL_CLOCK_MCPWM,				/*!< MCPWM clock */
	SYSCTL_CLOCK_QEI,					/*!< QEI clock */
	SYSCTL_CLOCK_I2C1,				/*!< I2C1 clock */
#if defined(CHIP_LPC175X_6X)
	SYSCTL_CLOCK_RSVD20,
#else
	SYSCTL_CLOCK_SSP2,				/*!< SSP2 clock */
#endif
	SYSCTL_CLOCK_SSP0,				/*!< SSP0 clock */
	SYSCTL_CLOCK_TIMER2,			/*!< Timer 2 clock */
	SYSCTL_CLOCK_TIMER3,			/*!< Timer 3 clock */
	SYSCTL_CLOCK_UART2,				/*!< UART 2 clock */
	SYSCTL_CLOCK_UART3,				/*!< UART 3 clock */
	SYSCTL_CLOCK_I2C2,				/*!< I2C2 clock */
	SYSCTL_CLOCK_I2S,					/*!< I2S clock */
#if defined(CHIP_LPC175X_6X)
	SYSCTL_CLOCK_RSVD28,
#else
	SYSCTL_CLOCK_SDC,				/*!< SD Card interface clock */
#endif
	SYSCTL_CLOCK_GPDMA,				/*!< GP DMA clock */
	SYSCTL_CLOCK_ENET,				/*!< EMAC/Ethernet clock */
	SYSCTL_CLOCK_USB,					/*!< USB clock */
	SYSCTL_CLOCK_RSVD32,
	SYSCTL_CLOCK_RSVD33,
	SYSCTL_CLOCK_RSVD34,
#if defined(CHIP_LPC40XX)
	SYSCTL_CLOCK_CMP,				/*!< Comparator clock (PCONP1) */
#else
	SYSCTL_CLOCK_RSVD35,
#endif
} CHIP_SYSCTL_CLOCK_T;


#define LPC_SYSCTL_BASE           0x400FC000
#define LPC_SYSCTL                ((LPC_SYSCTL_T           *) LPC_SYSCTL_BASE)
#define ENET_MIND_BUSY          0x00000001		/*!< MII is Busy */

#define LPC_ENET_BASE             0x50000000
#define LPC_ETHERNET              ((LPC_ENET_T             *) LPC_ENET_BASE)
#define ENET_TCTRL_SIZE(n)       (((n) - 1) & 0x7FF)	/*!< Size of data buffer in bytes */
#define ENET_TCTRL_OVERRIDE      0x04000000				/*!< Override Default MAC Registers */
#define ENET_TCTRL_HUGE          0x08000000				/*!< Enable Huge Frame */
#define ENET_TCTRL_PAD           0x10000000				/*!< Pad short Frames to 64 bytes */
#define ENET_TCTRL_CRC           0x20000000				/*!< Append a hardware CRC to Frame */
#define ENET_TCTRL_LAST          0x40000000				/*!< Last Descriptor for TX Frame */
#define ENET_TCTRL_INT           0x80000000				/*!< Generate TxDone Interrupt */

#define LINK_STATS_INC(x)

#define ARP_MAXPENDING 2

typedef struct {
	/* prxs must be 8 byte aligned! */
	ENET_RXSTAT_T prxs[LPC_NUM_BUFF_RXDESCS];	/**< Pointer to RX statuses */
	ENET_RXDESC_T prxd[LPC_NUM_BUFF_RXDESCS];	/**< Pointer to RX descriptor list */
	ENET_TXSTAT_T ptxs[LPC_NUM_BUFF_TXDESCS];	/**< Pointer to TX statuses */
	ENET_TXDESC_T ptxd[LPC_NUM_BUFF_TXDESCS];	/**< Pointer to TX descriptor list */
	struct netif *pnetif;						/**< Reference back to LWIP parent netif */

	struct pbuf *rxb[LPC_NUM_BUFF_RXDESCS];		/**< RX pbuf pointer list, zero-copy mode */

	uint32_t rx_fill_desc_index;					/**< RX descriptor next available index */
	volatile uint32_t rx_free_descs;				/**< Count of free RX descriptors */
	struct pbuf *txb[LPC_NUM_BUFF_TXDESCS];		/**< TX pbuf pointer list, zero-copy mode */

	uint32_t lpc_last_tx_idx;						/**< TX last descriptor index, zero-copy mode */
#if NO_SYS == 0
	sys_sem_t rx_sem;							/**< RX receive thread wakeup semaphore */
	sys_sem_t tx_clean_sem;						/**< TX cleanup thread wakeup semaphore */
	sys_mutex_t tx_lock_mutex;					/**< TX critical section mutex */
	sys_mutex_t rx_lock_mutex;					/**< RX critical section mutex */
	xSemaphoreHandle xtx_count_sem;				/**< TX free buffer counting semaphore */
#endif
} lpc_enetdata_t;

/** \brief  LPC EMAC driver work data
 */
ALIGNED(8) lpc_enetdata_t lpc_enetdata;

typedef int8_t err_t;

typedef struct {
	__IO uint32_t MAC1;			/*!< MAC Configuration register 1 */
	__IO uint32_t MAC2;			/*!< MAC Configuration register 2 */
	__IO uint32_t IPGT;			/*!< Back-to-Back Inter-Packet-Gap register */
	__IO uint32_t IPGR;			/*!< Non Back-to-Back Inter-Packet-Gap register */
	__IO uint32_t CLRT;			/*!< Collision window / Retry register */
	__IO uint32_t MAXF;			/*!< Maximum Frame register */
	__IO uint32_t SUPP;			/*!< PHY Support register */
	__IO uint32_t TEST;			/*!< Test register */
	__IO uint32_t MCFG;			/*!< MII Mgmt Configuration register */
	__IO uint32_t MCMD;			/*!< MII Mgmt Command register */
	__IO uint32_t MADR;			/*!< MII Mgmt Address register */
	__O  uint32_t MWTD;			/*!< MII Mgmt Write Data register */
	__I  uint32_t MRDD;			/*!< MII Mgmt Read Data register */
	__I  uint32_t MIND;			/*!< MII Mgmt Indicators register */
	uint32_t RESERVED0[2];
	__IO uint32_t SA[3];		/*!< Station Address registers */
} ENET_MAC_T;

/**
 * @brief Ethernet Transfer register Block Structure
 */
typedef struct {
	__IO uint32_t DESCRIPTOR;		/*!< Descriptor base address register */
	__IO uint32_t STATUS;			/*!< Status base address register */
	__IO uint32_t DESCRIPTORNUMBER;	/*!< Number of descriptors register */
	__IO uint32_t PRODUCEINDEX;		/*!< Produce index register */
	__IO uint32_t CONSUMEINDEX;		/*!< Consume index register */
} ENET_TRANSFER_INFO_T;

/**
 * @brief Ethernet Control register block structure
 */
typedef struct {
	__IO uint32_t COMMAND;				/*!< Command register */
	__I  uint32_t STATUS;				/*!< Status register */
	ENET_TRANSFER_INFO_T RX;	/*!< Receive block registers */
	ENET_TRANSFER_INFO_T TX;	/*!< Transmit block registers */
	uint32_t RESERVED0[10];
	__I  uint32_t TSV0;					/*!< Transmit status vector 0 register */
	__I  uint32_t TSV1;					/*!< Transmit status vector 1 register */
	__I  uint32_t RSV;					/*!< Receive status vector register */
	uint32_t RESERVED1[3];
	__IO uint32_t FLOWCONTROLCOUNTER;	/*!< Flow control counter register */
	__I  uint32_t FLOWCONTROLSTATUS;	/*!< Flow control status register */
} ENET_CONTROL_T;

/**
 * @brief Ethernet Receive Filter register block structure
 */
typedef struct {
	__IO uint32_t CONTROL;			/*!< Receive filter control register */
	__I  uint32_t WOLSTATUS;		/*!< Receive filter WoL status register */
	__O  uint32_t WOLCLEAR;			/*!< Receive filter WoL clear register */
	uint32_t RESERVED;
	__IO uint32_t HashFilterL;		/*!< Hash filter table LSBs register */
	__IO uint32_t HashFilterH;		/*!< Hash filter table MSBs register */
} ENET_RXFILTER_T;

/**
 * @brief Ethernet Module Control register block structure
 */
typedef struct {
	__I  uint32_t INTSTATUS;		/*!< Interrupt status register */
	__IO uint32_t INTENABLE;		/*!< Interrupt enable register */
	__O  uint32_t INTCLEAR;			/*!< Interrupt clear register */
	__O  uint32_t INTSET;			/*!< Interrupt set register */
	uint32_t RESERVED;
	__IO uint32_t POWERDOWN;		/*!< Power-down register */
} ENET_MODULE_CTRL_T;

/**
 * @brief Ethernet register block structure
 */
typedef struct {
	ENET_MAC_T  MAC;				/*!< MAC registers */
	uint32_t RESERVED1[45];
	ENET_CONTROL_T CONTROL;		/*!< Control registers */
	uint32_t RESERVED4[34];
	ENET_RXFILTER_T RXFILTER;		/*!< RxFilter registers */
	uint32_t RESERVED6[882];
	ENET_MODULE_CTRL_T MODULE_CONTROL;	/*!< Module Control registers */
} LPC_ENET_T;


err_t lpc_enetif_init(struct netif *netif);

/**
 * @brief RX Descriptor Control structure type definition
 */
#define ENET_RCTRL_SIZE(n)       (((n) - 1) & 0x7FF)	/*!< Buffer size field */
#define ENET_RCTRL_INT           0x80000000				/*!< Generate RxDone Interrupt */

#define eth_addr_cmp(addr1, addr2) (memcmp((addr1)->addr, (addr2)->addr, ETHARP_HWADDR_LEN) == 0)

#define ENET_RINFO_CRC_ERR 0x00800000
#define ENET_RINFO_SYM_ERR 0x01000000
#define ENET_RINFO_ALIGN_ERR 0x08000000
#define ENET_RINFO_LEN_ERR 0x02000000

#define SIZEOF_ETHARP_HDR 28
#define SIZEOF_ETHARP_PACKET (SIZEOF_ETH_HDR + SIZEOF_ETHARP_HDR)
#define HWTYPE_ETHERNET 1
#define ETHARP_FLAG_TRY_HARD     1
#define ETHARP_FLAG_FIND_ONLY    2
#define ARP_REQUEST 1
#define ARP_REPLY   2

PACK_STRUCT_BEGIN
/** the ARP message, see RFC 826 ("Packet format") */
struct etharp_hdr {
  PACK_STRUCT_FIELD(u16_t hwtype);
  PACK_STRUCT_FIELD(u16_t proto);
  PACK_STRUCT_FIELD(u8_t  hwlen);
  PACK_STRUCT_FIELD(u8_t  protolen);
  PACK_STRUCT_FIELD(u16_t opcode);
  PACK_STRUCT_FIELD(struct eth_addr shwaddr);
  PACK_STRUCT_FIELD(struct ip_addr2 sipaddr);
  PACK_STRUCT_FIELD(struct eth_addr dhwaddr);
  PACK_STRUCT_FIELD(struct ip_addr2 dipaddr);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END

enum etharp_state {
  ETHARP_STATE_EMPTY = 0,
  ETHARP_STATE_PENDING,
  ETHARP_STATE_STABLE,
  ETHARP_STATE_STABLE_REREQUESTING
#if ETHARP_SUPPORT_STATIC_ENTRIES
  ,ETHARP_STATE_STATIC
#endif /* ETHARP_SUPPORT_STATIC_ENTRIES */
};

#ifndef ARP_TABLE_SIZE
#define ARP_TABLE_SIZE                  10
#endif

struct etharp_entry {
#if ARP_QUEUEING
  /** Pointer to queue of pending outgoing packets on this ARP entry. */
  struct etharp_q_entry *q;
#else /* ARP_QUEUEING */
  /** Pointer to a single pending outgoing packet on this ARP entry. */
  struct pbuf *q;
#endif /* ARP_QUEUEING */
  ip_addr_t ipaddr;
  struct netif *netif;
  struct eth_addr ethaddr;
  u8_t state;
  u8_t ctime;
};

static struct etharp_entry arp_table[ARP_TABLE_SIZE];

err_t ethernet_input(struct pbuf *p, struct netif *netif);

#ifndef ETHADDR16_COPY
#define ETHADDR16_COPY(src, dst)  SMEMCPY(src, dst, ETHARP_HWADDR_LEN)
#endif

#define snmp_insert_arpidx_tree(ni,ip)
#define snmp_delete_arpidx_tree(ni,ip)

#define free_etharp_q(q) pbuf_free(q)
#ifndef ETHADDR32_COPY
#define ETHADDR32_COPY(src, dst)  SMEMCPY(src, dst, ETHARP_HWADDR_LEN)
#endif

#define ENET_RINFO_SIZE(n)       (((n) & 0x7FF) + 1)	/*!< Data size in bytes */

#define PHY_LINK_ERROR     (1 << 0)
#define PHY_LINK_BUSY      (1 << 1)
#define PHY_LINK_CHANGED   (1 << 2)
#define PHY_LINK_CONNECTED (1 << 3)
#define PHY_LINK_SPEED100  (1 << 4)
#define PHY_LINK_FULLDUPLX (1 << 5)

#define ENET_IPGT_HALFDUPLEX (ENET_IPGT_BTOBINTEGAP(0x12))

void Chip_ENET_Set100Mbps(LPC_ENET_T *pENET);
void Chip_ENET_Set10Mbps(LPC_ENET_T *pENET);

#define ARP_MAXAGE              240
#define ARP_AGE_REREQUEST_USED  (ARP_MAXAGE - 12)

#if !LWIP_NETIF_HWADDRHINT
static u8_t etharp_cached_entry;
#endif /* !LWIP_NETIF_HWADDRHINT */

#if LWIP_NETIF_HWADDRHINT
#define ETHARP_SET_HINT(netif, hint)  if (((netif) != NULL) && ((netif)->addr_hint != NULL))  \
                                      *((netif)->addr_hint) = (hint);
#else /* LWIP_NETIF_HWADDRHINT */
#define ETHARP_SET_HINT(netif, hint)  (etharp_cached_entry = (hint))
#endif /* LWIP_NETIF_HWADDRHINT */

#endif /* INC_ENET_H_ */
