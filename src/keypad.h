#ifndef KEYPAD_H_
#define KEYPAD_H_

#define KEY_F1 (1<<0)
#define KEY_F2 (1<<1)
#define KEY_F3 (1<<2)
#define KEY_F4 (1<<3)
#define KEY_S (1<<4)
#define KEY_ANY (KEY_F1 | KEY_F2 | KEY_F3 | KEY_F4 | KEY_S)

uint32_t Keypad_Get(void);
void Keypad_Init(void);

#endif /* KEYPAD_H_ */
