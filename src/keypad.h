#ifndef KEYPAD_H_
#define KEYPAD_H_

#define KEY_F1 (1<<0)
#define KEY_F2 (1<<1)
#define KEY_F3 (1<<2)
#define KEY_F4 (1<<3)
#define KEY_S (1<<4)

uint32_t Keypad_Get(void);
void Keypad_Init( void );

/* The following polls the keypad WITHOUT debouncing, used only for startup checks.
 * This function returns a type defined below under 'raw' */
uint32_t Keypad_GetRaw(void);
#define RAWONLY_F1KEY_PORTBIT (1<<23)
#define RAWONLY_F2KEY_PORTBIT (1<<15)
#define RAWONLY_F3KEY_PORTBIT (1<<16)
#define RAWONLY_F4KEY_PORTBIT (1<<4)
#define RAWONLY_S_KEY_PORTBIT (1<<20)

#endif /* KEYPAD_H_ */
