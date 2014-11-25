/******************************************************************************
 *   LPC214X.h:  Header file for Philips LPC214x Family Microprocessors
 *   The header file is the super set of all hardware definition of the 
 *   peripherals for the LPC214x family microprocessor.
 *
 *   Copyright(C) 2006, Philips Semiconductor
 *   All rights reserved.

 *   History
 *   2005.10.01  ver 1.00    Prelimnary version, first Release
 *   2005.10.13  ver 1.01    Removed CSPR and DC_REVISION register.
 *                           CSPR can not be accessed at the user level,
 *                           DC_REVISION is no long available.
 *                           All registers use "volatile unsigned long". 
******************************************************************************/

#ifndef __LPC214x_H
#define __LPC214x_H

/* Vectored Interrupt Controller (VIC) */
#define VIC_BASE_ADDR	0xFFFFF000

#define VICIRQStatus   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x000))
#define VICFIQStatus   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x004))
#define VICRawIntr     (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x008))
#define VICIntSelect   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x00C))
#define VICIntEnable   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x010))
#define VICIntEnClr    (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x014))
#define VICSoftInt     (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x018))
#define VICSoftIntClr  (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x01C))
#define VICProtection  (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x020))
#define VICVectAddr    (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x030))
#define VICDefVectAddr (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x034))
#define VICVectAddr0   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x100))
#define VICVectAddr1   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x104))
#define VICVectAddr2   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x108))
#define VICVectAddr3   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x10C))
#define VICVectAddr4   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x110))
#define VICVectAddr5   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x114))
#define VICVectAddr6   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x118))
#define VICVectAddr7   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x11C))
#define VICVectAddr8   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x120))
#define VICVectAddr9   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x124))
#define VICVectAddr10  (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x128))
#define VICVectAddr11  (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x12C))
#define VICVectAddr12  (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x130))
#define VICVectAddr13  (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x134))
#define VICVectAddr14  (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x138))
#define VICVectAddr15  (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x13C))
#define VICVectCntl0   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x200))
#define VICVectCntl1   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x204))
#define VICVectCntl2   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x208))
#define VICVectCntl3   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x20C))
#define VICVectCntl4   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x210))
#define VICVectCntl5   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x214))
#define VICVectCntl6   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x218))
#define VICVectCntl7   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x21C))
#define VICVectCntl8   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x220))
#define VICVectCntl9   (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x224))
#define VICVectCntl10  (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x228))
#define VICVectCntl11  (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x22C))
#define VICVectCntl12  (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x230))
#define VICVectCntl13  (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x234))
#define VICVectCntl14  (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x238))
#define VICVectCntl15  (*(volatile unsigned long *)(VIC_BASE_ADDR + 0x23C))

/* Pin Connect Block */
#define PINSEL_BASE_ADDR	0xE002C000
#define PINSEL0        (*(volatile unsigned long *)(PINSEL_BASE_ADDR + 0x00))
#define PINSEL1        (*(volatile unsigned long *)(PINSEL_BASE_ADDR + 0x04))
#define PINSEL2        (*(volatile unsigned long *)(PINSEL_BASE_ADDR + 0x14))

/* General Purpose Input/Output (GPIO) */
#define GPIO_BASE_ADDR		0xE0028000
#define IOPIN0         (*(volatile unsigned long *)(GPIO_BASE_ADDR + 0x00))
#define IOSET0         (*(volatile unsigned long *)(GPIO_BASE_ADDR + 0x04))
#define IODIR0         (*(volatile unsigned long *)(GPIO_BASE_ADDR + 0x08))
#define IOCLR0         (*(volatile unsigned long *)(GPIO_BASE_ADDR + 0x0C))
#define IOPIN1         (*(volatile unsigned long *)(GPIO_BASE_ADDR + 0x10))
#define IOSET1         (*(volatile unsigned long *)(GPIO_BASE_ADDR + 0x14))
#define IODIR1         (*(volatile unsigned long *)(GPIO_BASE_ADDR + 0x18))
#define IOCLR1         (*(volatile unsigned long *)(GPIO_BASE_ADDR + 0x1C))

/* Fast I/O setup */
#define FIO_BASE_ADDR		0x3FFFC000
#define FIO0DIR        (*(volatile unsigned long *)(FIO_BASE_ADDR + 0x00)) 
#define FIO0MASK       (*(volatile unsigned long *)(FIO_BASE_ADDR + 0x10))
#define FIO0PIN        (*(volatile unsigned long *)(FIO_BASE_ADDR + 0x14))
#define FIO0SET        (*(volatile unsigned long *)(FIO_BASE_ADDR + 0x18))
#define FIO0CLR        (*(volatile unsigned long *)(FIO_BASE_ADDR + 0x1C))
#define FIO1DIR        (*(volatile unsigned long *)(FIO_BASE_ADDR + 0x20)) 
#define FIO1MASK       (*(volatile unsigned long *)(FIO_BASE_ADDR + 0x30))
#define FIO1PIN        (*(volatile unsigned long *)(FIO_BASE_ADDR + 0x34))
#define FIO1SET        (*(volatile unsigned long *)(FIO_BASE_ADDR + 0x38))
#define FIO1CLR        (*(volatile unsigned long *)(FIO_BASE_ADDR + 0x3C))

/* System Control Block(SCB) modules include Memory Accelerator Module,
Phase Locked Loop, VPB divider, Power Control, External Interrupt, 
Reset, and Code Security/Debugging */

#define SCB_BASE_ADDR	0xE01FC000

/* Memory Accelerator Module (MAM) */
#define MAMCR          (*(volatile unsigned long *)(SCB_BASE_ADDR + 0x000))
#define MAMTIM         (*(volatile unsigned long *)(SCB_BASE_ADDR + 0x004))
#define MEMMAP         (*(volatile unsigned long *)(SCB_BASE_ADDR + 0x040))

/* Phase Locked Loop (PLL) */
#define PLLCON         (*(volatile unsigned long *)(SCB_BASE_ADDR + 0x080))
#define PLLCFG         (*(volatile unsigned long *)(SCB_BASE_ADDR + 0x084))
#define PLLSTAT        (*(volatile unsigned long *)(SCB_BASE_ADDR + 0x088))
#define PLLFEED        (*(volatile unsigned long *)(SCB_BASE_ADDR + 0x08C))

/* PLL48 Registers */
#define PLL48CON       (*(volatile unsigned long *)(SCB_BASE_ADDR + 0x0A0))
#define PLL48CFG       (*(volatile unsigned long *)(SCB_BASE_ADDR + 0x0A4))
#define PLL48STAT      (*(volatile unsigned long *)(SCB_BASE_ADDR + 0x0A8))
#define PLL48FEED      (*(volatile unsigned long *)(SCB_BASE_ADDR + 0x0AC))

/* Power Control */
#define PCON           (*(volatile unsigned long *)(SCB_BASE_ADDR + 0x0C0))
#define PCONP          (*(volatile unsigned long *)(SCB_BASE_ADDR + 0x0C4))

/* VPB Divider */
#define VPBDIV         (*(volatile unsigned long *)(SCB_BASE_ADDR + 0x100))

/* External Interrupts */
#define EXTINT         (*(volatile unsigned long *)(SCB_BASE_ADDR + 0x140))
#define INTWAKE        (*(volatile unsigned long *)(SCB_BASE_ADDR + 0x144))
#define EXTMODE        (*(volatile unsigned long *)(SCB_BASE_ADDR + 0x148))
#define EXTPOLAR       (*(volatile unsigned long *)(SCB_BASE_ADDR + 0x14C))

/* Reset */
#define RSIR           (*(volatile unsigned long *)(SCB_BASE_ADDR + 0x180))

/* System Controls and Status */
#define SCS            (*(volatile unsigned long *)(SCB_BASE_ADDR + 0x1A0))	

/* Timer 0 */
#define TMR0_BASE_ADDR		0xE0004000
#define T0IR           (*(volatile unsigned long *)(TMR0_BASE_ADDR + 0x00))
#define T0TCR          (*(volatile unsigned long *)(TMR0_BASE_ADDR + 0x04))
#define T0TC           (*(volatile unsigned long *)(TMR0_BASE_ADDR + 0x08))
#define T0PR           (*(volatile unsigned long *)(TMR0_BASE_ADDR + 0x0C))
#define T0PC           (*(volatile unsigned long *)(TMR0_BASE_ADDR + 0x10))
#define T0MCR          (*(volatile unsigned long *)(TMR0_BASE_ADDR + 0x14))
#define T0MR0          (*(volatile unsigned long *)(TMR0_BASE_ADDR + 0x18))
#define T0MR1          (*(volatile unsigned long *)(TMR0_BASE_ADDR + 0x1C))
#define T0MR2          (*(volatile unsigned long *)(TMR0_BASE_ADDR + 0x20))
#define T0MR3          (*(volatile unsigned long *)(TMR0_BASE_ADDR + 0x24))
#define T0CCR          (*(volatile unsigned long *)(TMR0_BASE_ADDR + 0x28))
#define T0CR0          (*(volatile unsigned long *)(TMR0_BASE_ADDR + 0x2C))
#define T0CR1          (*(volatile unsigned long *)(TMR0_BASE_ADDR + 0x30))
#define T0CR2          (*(volatile unsigned long *)(TMR0_BASE_ADDR + 0x34))
#define T0CR3          (*(volatile unsigned long *)(TMR0_BASE_ADDR + 0x38))
#define T0EMR          (*(volatile unsigned long *)(TMR0_BASE_ADDR + 0x3C))
#define T0CTCR         (*(volatile unsigned long *)(TMR0_BASE_ADDR + 0x70))

/* Timer 1 */
#define TMR1_BASE_ADDR		0xE0008000
#define T1IR           (*(volatile unsigned long *)(TMR1_BASE_ADDR + 0x00))
#define T1TCR          (*(volatile unsigned long *)(TMR1_BASE_ADDR + 0x04))
#define T1TC           (*(volatile unsigned long *)(TMR1_BASE_ADDR + 0x08))
#define T1PR           (*(volatile unsigned long *)(TMR1_BASE_ADDR + 0x0C))
#define T1PC           (*(volatile unsigned long *)(TMR1_BASE_ADDR + 0x10))
#define T1MCR          (*(volatile unsigned long *)(TMR1_BASE_ADDR + 0x14))
#define T1MR0          (*(volatile unsigned long *)(TMR1_BASE_ADDR + 0x18))
#define T1MR1          (*(volatile unsigned long *)(TMR1_BASE_ADDR + 0x1C))
#define T1MR2          (*(volatile unsigned long *)(TMR1_BASE_ADDR + 0x20))
#define T1MR3          (*(volatile unsigned long *)(TMR1_BASE_ADDR + 0x24))
#define T1CCR          (*(volatile unsigned long *)(TMR1_BASE_ADDR + 0x28))
#define T1CR0          (*(volatile unsigned long *)(TMR1_BASE_ADDR + 0x2C))
#define T1CR1          (*(volatile unsigned long *)(TMR1_BASE_ADDR + 0x30))
#define T1CR2          (*(volatile unsigned long *)(TMR1_BASE_ADDR + 0x34))
#define T1CR3          (*(volatile unsigned long *)(TMR1_BASE_ADDR + 0x38))
#define T1EMR          (*(volatile unsigned long *)(TMR1_BASE_ADDR + 0x3C))
#define T1CTCR         (*(volatile unsigned long *)(TMR1_BASE_ADDR + 0x70))

/* Pulse Width Modulator (PWM) */
#define PWM_BASE_ADDR		0xE0014000
#define PWMIR          (*(volatile unsigned long *)(PWM_BASE_ADDR + 0x00))
#define PWMTCR         (*(volatile unsigned long *)(PWM_BASE_ADDR + 0x04))
#define PWMTC          (*(volatile unsigned long *)(PWM_BASE_ADDR + 0x08))
#define PWMPR          (*(volatile unsigned long *)(PWM_BASE_ADDR + 0x0C))
#define PWMPC          (*(volatile unsigned long *)(PWM_BASE_ADDR + 0x10))
#define PWMMCR         (*(volatile unsigned long *)(PWM_BASE_ADDR + 0x14))
#define PWMMR0         (*(volatile unsigned long *)(PWM_BASE_ADDR + 0x18))
#define PWMMR1         (*(volatile unsigned long *)(PWM_BASE_ADDR + 0x1C))
#define PWMMR2         (*(volatile unsigned long *)(PWM_BASE_ADDR + 0x20))
#define PWMMR3         (*(volatile unsigned long *)(PWM_BASE_ADDR + 0x24))
#define PWMMR4         (*(volatile unsigned long *)(PWM_BASE_ADDR + 0x40))
#define PWMMR5         (*(volatile unsigned long *)(PWM_BASE_ADDR + 0x44))
#define PWMMR6         (*(volatile unsigned long *)(PWM_BASE_ADDR + 0x48))
#define PWMEMR         (*(volatile unsigned long *)(PWM_BASE_ADDR + 0x3C))
#define PWMPCR         (*(volatile unsigned long *)(PWM_BASE_ADDR + 0x4C))
#define PWMLER         (*(volatile unsigned long *)(PWM_BASE_ADDR + 0x50))

/* Universal Asynchronous Receiver Transmitter 0 (UART0) */
#define UART0_BASE_ADDR		0xE000C000
#define U0RBR          (*(volatile unsigned long *)(UART0_BASE_ADDR + 0x00))
#define U0THR          (*(volatile unsigned long *)(UART0_BASE_ADDR + 0x00))
#define U0DLL          (*(volatile unsigned long *)(UART0_BASE_ADDR + 0x00))
#define U0DLM          (*(volatile unsigned long *)(UART0_BASE_ADDR + 0x04))
#define U0IER          (*(volatile unsigned long *)(UART0_BASE_ADDR + 0x04))
#define U0IIR          (*(volatile unsigned long *)(UART0_BASE_ADDR + 0x08))
#define U0FCR          (*(volatile unsigned long *)(UART0_BASE_ADDR + 0x08))
#define U0LCR          (*(volatile unsigned long *)(UART0_BASE_ADDR + 0x0C))
#define U0MCR          (*(volatile unsigned long *)(UART0_BASE_ADDR + 0x10))
#define U0LSR          (*(volatile unsigned long *)(UART0_BASE_ADDR + 0x14))
#define U0MSR          (*(volatile unsigned long *)(UART0_BASE_ADDR + 0x18))
#define U0SCR          (*(volatile unsigned long *)(UART0_BASE_ADDR + 0x1C))
#define U0ACR          (*(volatile unsigned long *)(UART0_BASE_ADDR + 0x20))
#define U0FDR          (*(volatile unsigned long *)(UART0_BASE_ADDR + 0x28))
#define U0TER          (*(volatile unsigned long *)(UART0_BASE_ADDR + 0x30))

/* Universal Asynchronous Receiver Transmitter 1 (UART1) */
#define UART1_BASE_ADDR		0xE0010000
#define U1RBR          (*(volatile unsigned long *)(UART1_BASE_ADDR + 0x00))
#define U1THR          (*(volatile unsigned long *)(UART1_BASE_ADDR + 0x00))
#define U1DLL          (*(volatile unsigned long *)(UART1_BASE_ADDR + 0x00))
#define U1DLM          (*(volatile unsigned long *)(UART1_BASE_ADDR + 0x04))
#define U1IER          (*(volatile unsigned long *)(UART1_BASE_ADDR + 0x04))
#define U1IIR          (*(volatile unsigned long *)(UART1_BASE_ADDR + 0x08))
#define U1FCR          (*(volatile unsigned long *)(UART1_BASE_ADDR + 0x08))
#define U1LCR          (*(volatile unsigned long *)(UART1_BASE_ADDR + 0x0C))
#define U1MCR          (*(volatile unsigned long *)(UART1_BASE_ADDR + 0x10))
#define U1LSR          (*(volatile unsigned long *)(UART1_BASE_ADDR + 0x14))
#define U1MSR          (*(volatile unsigned long *)(UART1_BASE_ADDR + 0x18))
#define U1SCR          (*(volatile unsigned long *)(UART1_BASE_ADDR + 0x1C))
#define U1ACR          (*(volatile unsigned long *)(UART1_BASE_ADDR + 0x20))
#define U1FDR          (*(volatile unsigned long *)(UART1_BASE_ADDR + 0x28))
#define U1TER          (*(volatile unsigned long *)(UART1_BASE_ADDR + 0x30))

/* I2C Interface 0 */
#define I2C0_BASE_ADDR		0xE001C000
#define I20CONSET      (*(volatile unsigned long *)(I2C0_BASE_ADDR + 0x00))
#define I20STAT        (*(volatile unsigned long *)(I2C0_BASE_ADDR + 0x04))
#define I20DAT         (*(volatile unsigned long *)(I2C0_BASE_ADDR + 0x08))
#define I20ADR         (*(volatile unsigned long *)(I2C0_BASE_ADDR + 0x0C))
#define I20SCLH        (*(volatile unsigned long *)(I2C0_BASE_ADDR + 0x10))
#define I20SCLL        (*(volatile unsigned long *)(I2C0_BASE_ADDR + 0x14))
#define I20CONCLR      (*(volatile unsigned long *)(I2C0_BASE_ADDR + 0x18))

/* I2C Interface 1 */
#define I2C1_BASE_ADDR		0xE005C000
#define I21CONSET      (*(volatile unsigned long *)(I2C1_BASE_ADDR + 0x00))
#define I21STAT        (*(volatile unsigned long *)(I2C1_BASE_ADDR + 0x04))
#define I21DAT         (*(volatile unsigned long *)(I2C1_BASE_ADDR + 0x08))
#define I21ADR         (*(volatile unsigned long *)(I2C1_BASE_ADDR + 0x0C))
#define I21SCLH        (*(volatile unsigned long *)(I2C1_BASE_ADDR + 0x10))
#define I21SCLL        (*(volatile unsigned long *)(I2C1_BASE_ADDR + 0x14))
#define I21CONCLR      (*(volatile unsigned long *)(I2C1_BASE_ADDR + 0x18))

/* SPI0 (Serial Peripheral Interface 0) */
#define SPI0_BASE_ADDR		0xE0020000
#define S0SPCR         (*(volatile unsigned long *)(SPI0_BASE_ADDR + 0x00))
#define S0SPSR         (*(volatile unsigned long *)(SPI0_BASE_ADDR + 0x04))
#define S0SPDR         (*(volatile unsigned long *)(SPI0_BASE_ADDR + 0x08))
#define S0SPCCR        (*(volatile unsigned long *)(SPI0_BASE_ADDR + 0x0C))
#define S0SPINT        (*(volatile unsigned long *)(SPI0_BASE_ADDR + 0x1C))

/* SSP Controller */
#define SSP_BASE_ADDR		0xE0068000
#define SSPCR0         (*(volatile unsigned long * )(SSP_BASE_ADDR + 0x00))
#define SSPCR1         (*(volatile unsigned long * )(SSP_BASE_ADDR + 0x04))
#define SSPDR          (*(volatile unsigned long * )(SSP_BASE_ADDR + 0x08))
#define SSPSR          (*(volatile unsigned long * )(SSP_BASE_ADDR + 0x0C))
#define SSPCPSR        (*(volatile unsigned long * )(SSP_BASE_ADDR + 0x10))
#define SSPIMSC        (*(volatile unsigned long * )(SSP_BASE_ADDR + 0x14))
#define SSPRIS         (*(volatile unsigned long * )(SSP_BASE_ADDR + 0x18))
#define SSPMIS         (*(volatile unsigned long * )(SSP_BASE_ADDR + 0x1C))
#define SSPICR         (*(volatile unsigned long * )(SSP_BASE_ADDR + 0x20))

/* Real Time Clock */
#define RTC_BASE_ADDR		0xE0024000
#define ILR            (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x00))
#define CTC            (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x04))
#define CCR            (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x08))
#define CIIR           (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x0C))
#define AMR            (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x10))
#define CTIME0         (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x14))
#define CTIME1         (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x18))
#define CTIME2         (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x1C))
#define SEC            (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x20))
#define MIN            (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x24))
#define HOUR           (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x28))
#define DOM            (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x2C))
#define DOW            (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x30))
#define DOY            (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x34))
#define MONTH          (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x38))
#define YEAR           (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x3C))
#define ALSEC          (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x60))
#define ALMIN          (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x64))
#define ALHOUR         (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x68))
#define ALDOM          (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x6C))
#define ALDOW          (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x70))
#define ALDOY          (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x74))
#define ALMON          (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x78))
#define ALYEAR         (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x7C))
#define PREINT         (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x80))
#define PREFRAC        (*(volatile unsigned long *)(RTC_BASE_ADDR + 0x84))

/* A/D Converter 0 (AD0) */
#define AD0_BASE_ADDR		0xE0034000
#define AD0CR          (*(volatile unsigned long *)(AD0_BASE_ADDR + 0x00))
#define AD0GDR         (*(volatile unsigned long *)(AD0_BASE_ADDR + 0x04))
#define AD0STAT        (*(volatile unsigned long *)(AD0_BASE_ADDR + 0x30))
#define AD0INTEN       (*(volatile unsigned long *)(AD0_BASE_ADDR + 0x0C))
#define AD0DR0         (*(volatile unsigned long *)(AD0_BASE_ADDR + 0x10))
#define AD0DR1         (*(volatile unsigned long *)(AD0_BASE_ADDR + 0x14))
#define AD0DR2         (*(volatile unsigned long *)(AD0_BASE_ADDR + 0x18))
#define AD0DR3         (*(volatile unsigned long *)(AD0_BASE_ADDR + 0x1C))
#define AD0DR4         (*(volatile unsigned long *)(AD0_BASE_ADDR + 0x20))
#define AD0DR5         (*(volatile unsigned long *)(AD0_BASE_ADDR + 0x24))
#define AD0DR6         (*(volatile unsigned long *)(AD0_BASE_ADDR + 0x28))
#define AD0DR7         (*(volatile unsigned long *)(AD0_BASE_ADDR + 0x2C))

#define ADGSR          (*(volatile unsigned long *)(AD0_BASE_ADDR + 0x08))

/* A/D Converter 1 (AD1) */
#define AD1_BASE_ADDR		0xE0060000
#define AD1CR          (*(volatile unsigned long *)(AD1_BASE_ADDR + 0x00))
#define AD1GDR         (*(volatile unsigned long *)(AD1_BASE_ADDR + 0x04))
#define AD1STAT        (*(volatile unsigned long *)(AD1_BASE_ADDR + 0x30))
#define AD1INTEN       (*(volatile unsigned long *)(AD1_BASE_ADDR + 0x0C))
#define AD1DR0         (*(volatile unsigned long *)(AD1_BASE_ADDR + 0x10))
#define AD1DR1         (*(volatile unsigned long *)(AD1_BASE_ADDR + 0x14))
#define AD1DR2         (*(volatile unsigned long *)(AD1_BASE_ADDR + 0x18))
#define AD1DR3         (*(volatile unsigned long *)(AD1_BASE_ADDR + 0x1C))
#define AD1DR4         (*(volatile unsigned long *)(AD1_BASE_ADDR + 0x20))
#define AD1DR5         (*(volatile unsigned long *)(AD1_BASE_ADDR + 0x24))
#define AD1DR6         (*(volatile unsigned long *)(AD1_BASE_ADDR + 0x28))
#define AD1DR7         (*(volatile unsigned long *)(AD1_BASE_ADDR + 0x2C))

/* D/A Converter */
#define DAC_BASE_ADDR		0xE006C000
#define DACR           (*(volatile unsigned long *)(DAC_BASE_ADDR + 0x00))

/* Watchdog */
#define WDG_BASE_ADDR		0xE0000000
#define WDMOD          (*(volatile unsigned long *)(WDG_BASE_ADDR + 0x00))
#define WDTC           (*(volatile unsigned long *)(WDG_BASE_ADDR + 0x04))
#define WDFEED         (*(volatile unsigned long *)(WDG_BASE_ADDR + 0x08))
#define WDTV           (*(volatile unsigned long *)(WDG_BASE_ADDR + 0x0C))

/* USB Controller */
#define USB_BASE_ADDR		0xE0090000			/* USB Base Address */
/* Device Interrupt Registers */
#define DEV_INT_STAT    (*(volatile unsigned long *)(USB_BASE_ADDR + 0x00))
#define DEV_INT_EN      (*(volatile unsigned long *)(USB_BASE_ADDR + 0x04))
#define DEV_INT_CLR     (*(volatile unsigned long *)(USB_BASE_ADDR + 0x08))
#define DEV_INT_SET     (*(volatile unsigned long *)(USB_BASE_ADDR + 0x0C))
#define DEV_INT_PRIO    (*(volatile unsigned long *)(USB_BASE_ADDR + 0x2C))

/* Endpoint Interrupt Registers */
#define EP_INT_STAT     (*(volatile unsigned long *)(USB_BASE_ADDR + 0x30))
#define EP_INT_EN       (*(volatile unsigned long *)(USB_BASE_ADDR + 0x34))
#define EP_INT_CLR      (*(volatile unsigned long *)(USB_BASE_ADDR + 0x38))
#define EP_INT_SET      (*(volatile unsigned long *)(USB_BASE_ADDR + 0x3C))
#define EP_INT_PRIO     (*(volatile unsigned long *)(USB_BASE_ADDR + 0x40))

/* Endpoint Realization Registers */
#define REALIZE_EP      (*(volatile unsigned long *)(USB_BASE_ADDR + 0x44))
#define EP_INDEX        (*(volatile unsigned long *)(USB_BASE_ADDR + 0x48))
#define MAXPACKET_SIZE  (*(volatile unsigned long *)(USB_BASE_ADDR + 0x4C))

/* Command Reagisters */
#define CMD_CODE        (*(volatile unsigned long *)(USB_BASE_ADDR + 0x10))
#define CMD_DATA        (*(volatile unsigned long *)(USB_BASE_ADDR + 0x14))

/* Data Transfer Registers */
#define RX_DATA         (*(volatile unsigned long *)(USB_BASE_ADDR + 0x18))
#define TX_DATA         (*(volatile unsigned long *)(USB_BASE_ADDR + 0x1C))
#define RX_PLENGTH      (*(volatile unsigned long *)(USB_BASE_ADDR + 0x20))
#define TX_PLENGTH      (*(volatile unsigned long *)(USB_BASE_ADDR + 0x24))
#define USB_CTRL        (*(volatile unsigned long *)(USB_BASE_ADDR + 0x28))

/* DMA Registers */
#define DMA_REQ_STAT        (*((volatile unsigned long *)USB_BASE_ADDR + 0x50))
#define DMA_REQ_CLR         (*((volatile unsigned long *)USB_BASE_ADDR + 0x54))
#define DMA_REQ_SET         (*((volatile unsigned long *)USB_BASE_ADDR + 0x58))
#define UDCA_HEAD           (*((volatile unsigned long *)USB_BASE_ADDR + 0x80))
#define EP_DMA_STAT         (*((volatile unsigned long *)USB_BASE_ADDR + 0x84))
#define EP_DMA_EN           (*((volatile unsigned long *)USB_BASE_ADDR + 0x88))
#define EP_DMA_DIS          (*((volatile unsigned long *)USB_BASE_ADDR + 0x8C))
#define DMA_INT_STAT        (*((volatile unsigned long *)USB_BASE_ADDR + 0x90))
#define DMA_INT_EN          (*((volatile unsigned long *)USB_BASE_ADDR + 0x94))
#define EOT_INT_STAT        (*((volatile unsigned long *)USB_BASE_ADDR + 0xA0))
#define EOT_INT_CLR         (*((volatile unsigned long *)USB_BASE_ADDR + 0xA4))
#define EOT_INT_SET         (*((volatile unsigned long *)USB_BASE_ADDR + 0xA8))
#define NDD_REQ_INT_STAT    (*((volatile unsigned long *)USB_BASE_ADDR + 0xAC))
#define NDD_REQ_INT_CLR     (*((volatile unsigned long *)USB_BASE_ADDR + 0xB0))
#define NDD_REQ_INT_SET     (*((volatile unsigned long *)USB_BASE_ADDR + 0xB4))
#define SYS_ERR_INT_STAT    (*((volatile unsigned long *)USB_BASE_ADDR + 0xB8))
#define SYS_ERR_INT_CLR     (*((volatile unsigned long *)USB_BASE_ADDR + 0xBC))
#define SYS_ERR_INT_SET     (*((volatile unsigned long *)USB_BASE_ADDR + 0xC0))    
#define MODULE_ID           (*((volatile unsigned long *)USB_BASE_ADDR + 0xFC))

#endif  // __LPC214x_H

