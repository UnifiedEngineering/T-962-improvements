#ifndef SC18IS602B_H_
#define SC18IS602B_H_

typedef enum eSPIclk {
	SPICLK_1843KHZ=0,
	SPICLK_461KHZ=1,
	SPICLK_115KHZ=2,
	SPICLK_58KHZ=3,
} SPIclk_t;

typedef enum eSPImode {
	SPIMODE_0=(0<<2),
	SPIMODE_1=(1<<2),
	SPIMODE_2=(2<<2),
	SPIMODE_3=(3<<2),
} SPImode_t;

typedef enum eSPIorder {
	SPIORDER_MSBFIRST=(0<<5),
	SPIORDER_LSBFIRST=(1<<5),
} SPIorder_t;

typedef struct __attribute__ ((__packed__)) {
	uint8_t ssmask;
	uint8_t data[16]; // Max 16 bytes at the moment
	uint8_t len;
} SPIxfer_t;

int32_t SC18IS602B_Init( SPIclk_t clk, SPImode_t mode, SPIorder_t order );
int32_t SC18IS602B_SPI_Xfer( SPIxfer_t* item );

#endif /* SC18IS602B_H_ */
