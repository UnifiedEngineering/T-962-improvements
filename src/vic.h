#ifndef VIC_H_
#define VIC_H_

typedef enum eVICInt {
	VIC_WDT=0,
	VIC_SWI,
	VIC_ICERX,
	VIC_ICETX,
	VIC_TIMER0,
	VIC_TIMER1,
	VIC_UART0,
	VIC_UART1,
	VIC_PWM0,
	VIC_I2C0,
	VIC_SPI0,
	VIC_SPI1,
	VIC_PLL,
	VIC_RTC,
	VIC_EINT0,
	VIC_EINT1,
	VIC_EINT2,
	VIC_EINT3,
	VIC_ADC0,
	VIC_I2C1,
	VIC_BOD,
	VIC_ADC1,
	VICINT_NUM_ITEMS // Last value
} VICInt_t;

void VIC_Init( void );
uint32_t VIC_IsIRQDisabled( void );
uint32_t VIC_DisableIRQ( void );
void VIC_RestoreIRQ( uint32_t mask );
int32_t VIC_RegisterHandler( VICInt_t num, void* ptr );
int32_t VIC_EnableHandler( VICInt_t num );
int32_t VIC_DisableHandler( VICInt_t num );

#endif /* VIC_H_ */
