/* $Id$ */

#ifndef __gen_vdp_h
#define __gen_vdp_h

#include "gen-emu.h"

struct vdp_mem {
	uint8_t  __attribute__ ((aligned(32))) regs[32];		/* Only first 24 used, rest are address padding. */
	uint16_t  __attribute__ ((aligned(32))) vsram[64];		/* Only first 40 used. rest are address padding. */

	uint16_t  __attribute__ ((aligned(32))) cram[64];
	uint16_t  __attribute__ ((aligned(32))) dc_cram[64];	/* cram in dc format */

	uint8_t  __attribute__ ((aligned(32))) vram[65536];

};

struct __attribute__ ((packed, aligned(4))) vdp_s
{
	uint32_t control;
	uint32_t unused32_0;

	uint16_t *bga;
	uint16_t *bgb;
	uint16_t *wnd;
	uint64_t *sat;

	uint16_t status;
	uint16_t scanline;
	uint16_t hv;
	uint16_t hs_off;
	uint16_t addr;
	uint16_t unused16_0;
	uint16_t unused16_1;
	uint16_t unused16_2;

	uint8_t code;
	uint8_t h_int_counter;
	uint8_t write_pending;
	uint8_t sc_width;
	uint8_t sc_height;
	uint8_t dis_cells;
	uint8_t sat_dirty;
	uint8_t unused8_0;
};

uint16_t vdp_control_read(void);
uint16_t vdp_data_read(void);
void vdp_control_write(uint16_t);
void vdp_data_write(uint16_t);
void vdp_interrupt(int line);
uint16_t vdp_hv_read(void);
void vdp_init(void);
void vdp_render_cram(void);
void vdp_render_scanline(int);

struct __attribute__ ((packed,aligned(4))) plane_pvr_tile {
	int x;
	int y;
	int hf;
	int vf;
	int plane;
	int priority;
	int unused;
	pvr_poly_hdr_t *hdr;
};

struct __attribute__ ((packed,aligned(4))) vdp_pvr_sprite {
	// 64
	int x;
	// 68
	int y;
	// 72
	int h;
	// 76
	int v;
	// 80
	int hf;
	// 84
	int vf;
	// 88
	int priority;
	// 92 - 128
	// 9 doesn't work but 17 does 
		int unused[17];
	// 0 - 64
	pvr_poly_hdr_t *hdr[16];
};

#endif // __gen_vdp_h
