#ifndef SETUP_H_
#define SETUP_H_

typedef struct {
	const char* formatstr;
	const NVItem_t nvval;
	const uint8_t minval;
	const uint8_t maxval;
	const int8_t offset;
	const float multiplier;
} setupMenuStruct;

int Setup_getNumItems(void);
float Setup_getValue(int item);
void Setup_setRealValue(int item, float value);
void Setup_setValue(int item, int value);
void Setup_increaseValue(int item, int amount);
void Setup_decreaseValue(int item, int amount);
void Setup_printFormattedValue(int item);
int Setup_snprintFormattedValue(char* buf, int n, int item);

#endif /* SETUP_H_ */
