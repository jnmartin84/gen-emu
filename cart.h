/* $Id$ */

#ifndef _CART_H_
#define _CART_H_

typedef struct __attribute__ ((packed, aligned(8))) cart_s {
	uint32_t	rom_len;
	uint32_t	sram_len;
	uint32_t	sram_start;
	uint32_t	sram_end;
	uint32_t	banks[8];
	uint8_t		banked;
	uint8_t		sram_banked;
	uint8_t		unused8_0;
	uint8_t		unused8_1;
	uint8		*rom;
	uint8		*sram;
} cart_t;

extern cart_t cart;

#endif /* _CART_H_ */
