#ifndef KEYPAD_H_
#define KEYPAD_H_

/* bit masks */
typedef enum {
	KEY_F1 = 1,
	KEY_F2 = 2,
	KEY_F3 = 4,
	KEY_F4 = 8,
	KEY_S  = 16
} key_mask_t;

typedef struct {
	key_mask_t priorized_key;
	uint8_t keymask;
	uint16_t acceleration;
} fkey_t;

fkey_t Keypad_Get(uint16_t, uint16_t);
void Keypad_Init(void);

#endif /* KEYPAD_H_ */
