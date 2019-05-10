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
int32_t VIC_RegisterHandler( VICInt_t num, void* ptr );
int32_t VIC_EnableHandler( VICInt_t num );
int32_t VIC_DisableHandler( VICInt_t num );

/*
 * some static inlines to allow inlining :-)
 *   link time optimization might have resolved this anyway?!
 */

static inline uint32_t VIC_IsIRQDisabled( void ) {
	uint32_t state;
	asm("MRS %0,cpsr" : "=r" (state));
	return !!(state&0x80);
}

static inline uint32_t VIC_DisableIRQ( void ) {
	uint32_t retval;
	asm("MRS %0,cpsr" : "=r" (retval));
	asm("MSR cpsr_c,#(0x1F | 0x80 | 0x40)");
	//FIO0CLR = (1<<11); // Visualize interrupts being disabled by turning backlight off
	return retval;
}

static inline void VIC_RestoreIRQ( uint32_t mask ) {
	//if(!(mask & 0x80)) FIO0SET = (1<<11); // Visualize when interrupts are enabled again by turning backlight on
	asm("MSR cpsr_c,%0" : : "r" (mask));
}


#endif /* VIC_H_ */
