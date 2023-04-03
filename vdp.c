/* $Id$ */

#include <kos.h>

#include "gen-emu.h"
#include "vdp.h"
#include "m68k.h"
#include "z80.h"
#include "cart.h"

#define TWIDDLEIT 1

#if TWIDDLEIT
#define TWIDTAB(xx) ( (xx&1)|((xx&2)<<1)|((xx&4)<<2)|((xx&8)<<3)|((xx&16)<<4)| \
                     ((xx&32)<<5)|((xx&64)<<6)|((xx&128)<<7)|((xx&256)<<8)|((xx&512)<<9) )
#define TWIDOUT(xx, yy) ( TWIDTAB((yy)) | (TWIDTAB((xx)) << 1) )
#endif

#define SWAP_WORDS(x) __asm__ volatile ("swap.w %0, %0" : "+r" (x))


extern uint16_t *m68k_ram16;

struct vdp_s vdp;


pvr_poly_hdr_t disp_hdr[2];
pvr_ptr_t disp_txr[2];
//pvr_ptr_t display_txr;


// priority plane tile
pvr_poly_hdr_t __attribute__ ((aligned(8))) tile_hdr[2][2][40*28];
struct plane_pvr_tile __attribute__ ((aligned(8))) planes_pool[2240];
int planes_size;
// set when tile is loaded to pvr mem
uint8_t __attribute__ ((aligned(8))) tn_used[2048] = {0};
pvr_ptr_t __attribute__ ((aligned(8))) tn_ptr[2048];

uint8_t __attribute__ ((aligned(8))) tn_16used[2048] = {0};
uint16_t __attribute__ ((aligned(8))) *tn_16bit[2048];


// priority sprite tile 
pvr_poly_hdr_t __attribute__ ((aligned(8))) sprite_hdr[2][80][4*4];
struct vdp_pvr_sprite __attribute__ ((aligned(8))) sprites_pool[80];
int sprites_size;


uint8_t __attribute__ ((aligned(8))) nt_cells[4] = { 32, 64, 0, 128 };
uint8_t __attribute__ ((aligned(8))) mode_cells[4] = { 32, 40, 0, 40 };
#define SWAP_WORDS(x) __asm__ volatile ("swap.w %0, %0" : "+r" (x))

#define get_color_argb1555(r,g,b,a) ((uint16_t)(((a&1)<<15) | ((r>>3)<<10) | ((g>>3)<<5) | (b>>3)))


void vdp_init(void)
{
	pvr_set_pal_format(PVR_PAL_ARGB1555);


	/* Allocate and build display texture data */
	disp_txr[0]/*[0]*/ = pvr_mem_malloc(512 * 256 * 2);
//	disp_txr[0][1] = pvr_mem_malloc(512 * 256 * 2);
	disp_txr[1]/*[0]*/ = pvr_mem_malloc(512 * 256 * 2);
//	disp_txr[1][1] = pvr_mem_malloc(512 * 256 * 2);

	// 8*8@4bpp texture for each pattern in VDP VRAM
	for(int i=0;i<2048;i++) {
		tn_ptr[i] = pvr_mem_malloc(32);
		tn_16bit[i] = malloc(64);
	}
	
	//for(int i=0;i<80;i++

	pvr_poly_cxt_t cxt;
	
	pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY,
		PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED,
		512, 256, disp_txr[0]/*[0]*/, PVR_FILTER_NONE);
	pvr_poly_compile(&disp_hdr[0]/*[0]*/, &cxt);
					cxt.blend.src = PVR_BLEND_DESTCOLOR;
					cxt.blend.dst = PVR_BLEND_ZERO;
/*
	pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY,
		PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED,
		512, 256, disp_txr[0][1], PVR_FILTER_NONE);
	pvr_poly_compile(&disp_hdr[0][1], &cxt);
					cxt.blend.src = PVR_BLEND_DESTCOLOR;
					cxt.blend.dst = PVR_BLEND_ZERO;
*/
	pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY,
		PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED,
		512, 256, disp_txr[1]/*[0]*/, PVR_FILTER_NONE);
	pvr_poly_compile(&disp_hdr[1]/*[0]*/, &cxt);
					cxt.blend.src = PVR_BLEND_DESTCOLOR;
					cxt.blend.dst = PVR_BLEND_ZERO;
/*
	pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY,
		PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED,
		512, 256, disp_txr[1][1], PVR_FILTER_NONE);
	pvr_poly_compile(&disp_hdr[1][1], &cxt);
					cxt.blend.src = PVR_BLEND_DESTCOLOR;
					cxt.blend.dst = PVR_BLEND_ZERO;
*/

    vdp.sat_dirty = 1;
}

uint32_t data_copy[8];
uint16_t tmptex[32];

uint16_t vdp_control_read(void)
{
	uint16_t ret = 0x3500;

	vdp.write_pending = 0;
	m68k_set_irq(0);

	ret |= (vdp.status & 0x00ff);

	//if (debug)
		//printf("VDP C -> %04x\n", ret);

	return ret;
}

/* val is little endian */
void vdp_control_write(uint16_t val)
{
//	if (debug)
	//	printf("VDP C <- %04x\n", val);

	if ((val & 0xc000) == 0x8000) {
		if (!vdp.write_pending) {
			uint8_t vdp_regno = ((val >> 8) & 0x1f);
			vdp.regs[vdp_regno] = (val & 0xff);

			// update pvr background color if register 7 value updated
			if (7 == vdp_regno) {
				int r,g,b;

				uint16_t col = vdp.dc_cram[val & 0x3f];

				r = ((col >> 10) & 0x1f)<<3;
				g = ((col >>  5) & 0x1f)<<3;
				b = ((col      ) & 0x1f)<<3;

				pvr_set_bg_color(r / 256.0f, g / 256.0f, b / 256.0f);
				vid_border_color(r,g,b);

			}
			
			
			switch((val >> 8) & 0x1f) {
			case 0x02:	/* BGA */
				vdp.bga = (uint16_t *)(vdp.vram + ((val & 0x38) << 10));
				break;
			case 0x03:	/* WND */
				vdp.wnd = (uint16_t *)(vdp.vram + ((val & 0x3e) << 10));
				break;
			case 0x04:	/* BGB */
				vdp.bgb = (uint16_t *)(vdp.vram + ((val & 0x07) << 13));
				break;
			case 0x05:	/* SAT */
				vdp.sat = (uint64_t *)(vdp.vram + ((val & 0x7f) << 9));
				break;
			case 0x0c:	/* Mode Set #4 */
				vdp.dis_cells = mode_cells[((vdp.regs[12] & 0x01) << 1) | (vdp.regs[12] >> 7)];
				break;
			case 0x0d:
				vdp.hs_off = vdp.regs[13] << 10;
				break;
			case 0x10:	/* Scroll size */
				vdp.sc_width = nt_cells[vdp.regs[16] & 0x03];
				vdp.sc_height = nt_cells[(vdp.regs[16] & 0x30) >> 4];
				break;
			}
			vdp.code = 0x0000;
		}
	} else {
		if (!vdp.write_pending) {
			vdp.write_pending = 1;
			vdp.addr = ((vdp.addr & 0xc000) | (val & 0x3fff));
			vdp.code = ((vdp.code & 0x3c) | ((val & 0xc000) >> 14));
		} else {
			vdp.write_pending = 0;
			vdp.addr = ((vdp.addr & 0x3fff) | ((val & 0x0003) << 14));
			vdp.code = ((vdp.code & 0x03) | ((val & 0x00f0) >> 2));

			if ((vdp.code & 0x20) && (vdp.regs[1] & 0x10)) {
				/* dma operation */
				if (((vdp.code & 0x30) == 0x20) && !(vdp.regs[23] & 0x80)) {
					uint16_t len = (vdp.regs[20] << 8) |  vdp.regs[19];
					uint16_t src_off = (vdp.regs[22] << 8) | vdp.regs[21];
					uint16_t *src_mem, src_mask;

					if ((vdp.regs[23] & 0x70) == 0x70) {
						src_mem = m68k_ram16;
						src_mask = 0x7fff;
					} else
					if ((vdp.regs[23] & 0x70) < 0x20) {
						src_mem = (uint16_t *)(cart.rom + (vdp.regs[23] << 17));
						src_mask = 0xffff;
					} else {
						while(1) {printf("DMA from an unknown block... 0x%02x\n", (vdp.regs[23] << 1));}
						//arch_abort();
					}

					/* 68k -> vdp */
					switch(vdp.code & 0x07) {
					case 0x01: {
						uint16_t start_vaddr = vdp.addr;
						uint16_t len_copy = len;
						uint16_t r15 = vdp.regs[15];
						
						/* vram */
						do {
							val = src_mem[src_off & src_mask];
							((uint16_t *)vdp.vram)[vdp.addr >> 1] = val;
							vdp.addr += r15;
							src_off += 1;
						} while(--len);
						
						do {
							if(!(start_vaddr & 0x01)) {
								SWAPBYTES16(((uint16_t *)vdp.vram)[start_vaddr >> 1]);
							}
							start_vaddr += r15;
						} while (--len_copy);
					}
						break;
					case 0x03: {
						/* cram */
						do {
							uint16_t val = src_mem[src_off & src_mask];
							uint16_t laddr = vdp.addr >> 1;
							uint8_t r,g,b,a;
							vdp.cram[laddr] = val;
							vdp.addr += vdp.regs[15];
							SWAPBYTES16(val);
							a = (laddr%16) != 0;
							r = (val & 0x000e) << 4;
							g = ((val>>4) & 0x000e) << 4;
							b = ((val>>8) & 0x000e) << 4;
							vdp.dc_cram[laddr] = get_color_argb1555(r,g,b,a);
		                    pvr_set_pal_entry(laddr, get_color_argb1555(r,g,b,a));
							src_off += 1;

							if(laddr == vdp.regs[7]) {
								pvr_set_bg_color(r / 255.0f, g / 255.0f, b / 255.0f);
											vid_border_color(r,g,b);

							}

							//if(vdp.addr > 0x7f)
							if(laddr > 0x3f)
								break;
						} while(--len);
					} break;
					case 0x05: {
						/* vsram */
						do {
							uint16_t laddr = vdp.addr >> 1;
							vdp.vsram[laddr] = src_mem[src_off & src_mask];//val;
							SWAPBYTES16(vdp.vsram[laddr]);
							vdp.addr += vdp.regs[15];
							src_off += 1;

							if(vdp.addr > 0x7f)
								break;
						} while(--len);
					} break;
						
					default:
						printf("68K->VDP DMA Error, code %d.", vdp.code);
					}

					vdp.regs[19] = vdp.regs[20] = 0;
					vdp.regs[21] = src_off & 0xff;
					vdp.regs[22] = src_off >> 8;
				}
			}
		}
	}
}

uint16_t vdp_data_read(void)
{
	uint16_t ret = 0x0000;

	vdp.write_pending = 0;

	switch(vdp.code) {
	case 0x00:
		ret = ((uint16_t *)vdp.vram)[vdp.addr >> 1];
		break;
	case 0x04:
		ret = vdp.vsram[vdp.addr >> 1];
		break;
	case 0x08:
		ret = vdp.cram[vdp.addr >> 1];
		break;
	}

	//if (debug)
		//printf("VDP D -> %04x\n", ret);

	vdp.addr += 2;

	return ret;
}


uint8_t start_tile_write = 0;
uint16_t start_tile_addr = 0;
void vdp_data_write(uint16_t val)
{
	vdp.write_pending = 0;

//	if (debug)
	//	printf("VDP D <- %04x\n", val);

	switch(vdp.code) {
	case 0x01:
		((uint16_t *)vdp.vram)[vdp.addr >> 1] = val;
		vdp.addr += 2;
		
		break;
	case 0x03:
		uint32_t cram_addr = (vdp.addr >> 1) & 0x3f;
		uint8_t r,g,b,a;
		vdp.cram[cram_addr] = val;
		a = (cram_addr%16) != 0;
		r = (val & 0x000e)<<4;
		g = ((val & 0x00e0)>>4)<<4;
		b = ((val & 0x0e00)>>8)<<4;
		vdp.dc_cram[cram_addr] = get_color_argb1555(r,g,b,a);
		pvr_set_pal_entry(cram_addr, get_color_argb1555(r,g,b,a));		
		vdp.addr += 2;

		if( cram_addr == vdp.regs[7]) {
			pvr_set_bg_color(r / 255.0f, g / 255.0f, b / 255.0f);
						vid_border_color(r,g,b);

		}

		break;
	case 0x05:
		vdp.vsram[(vdp.addr >> 1) & 0x3f] = val;
		vdp.addr += 2;
		break;

	default:
		if ((vdp.code & 0x20) && (vdp.regs[1] & 0x10)) {
			if (((vdp.code & 0x30) == 0x20) && ((vdp.regs[23] & 0xc0) == 0x80)) {
				/* vdp fill */
				uint16_t len = ((vdp.regs[20] << 8) | vdp.regs[19]);

				vdp.vram[vdp.addr] = val & 0xff;
				val = (val >> 8) & 0xff;

				do {
					vdp.vram[vdp.addr ^ 1] = val;
					vdp.addr += vdp.regs[15];
				} while(--len);
			} 
			else if ((vdp.code == 0x30) && ((vdp.regs[23] & 0xc0) == 0xc0)) {
				/* vdp copy */
				uint16_t len = (vdp.regs[20] << 8) | vdp.regs[19];
				uint16_t addr = (vdp.regs[22] << 8) | vdp.regs[21];

				do {
					vdp.vram[vdp.addr] = vdp.vram[addr++];
					vdp.addr += vdp.regs[15];
				}
				while (--len);
			}
			else {
				//printf("VDP DMA Error, code %02x, r23 %02x\n", vdp.code, vdp.regs[23]);
			}
		}
	}
}

uint16_t vdp_hv_read(void)
{
	uint16_t h, v;

	v = vdp.scanline;
	if ( v > 0xea) {
		v -= 6;
	}

	h = (uint16_t)(m68k_cycles_run() * 0.70082f);
	if (h > 0xe9) {
		h -= 86;
	}

	return(((v & 0xff) << 8) | (h & 0xff));
}

void vdp_interrupt(int line)
{
	uint8_t h_int_pending = 0;

	vdp.scanline = line;

	if (vdp.h_int_counter == 0) {
		vdp.h_int_counter = vdp.regs[10];
		if (vdp.regs[0] & 0x10) {
			h_int_pending = 1;
		}
	}

	if (line < 224) {
		if (line == 0) {
			vdp.h_int_counter = vdp.regs[10];
			vdp.status &= 0x0001;
		}

		if (h_int_pending) {
			m68k_set_irq(4);
		}
	}
	else if (line == 224) {
		z80_set_irq_line(0, PULSE_LINE);
		if (h_int_pending) {
			m68k_set_irq(4);
		}
		else {
			if (vdp.regs[1] & 0x20) {
				vdp.status |= 0x0080;
				m68k_set_irq(6);
			}
		}
	}
	else {
		vdp.h_int_counter = vdp.regs[10];
		vdp.status |= 0x08;
	}

	vdp.h_int_counter--;
}

int last_larget_tn_used = 0;

// builds all plane/priority combos in one pass
void vdp_render_pvr_planes(void) {
	memset(tn_used,0,last_larget_tn_used);
	last_larget_tn_used = 0;
	planes_size = 0;

	for (int plane=0;plane<2;plane++) {
		uint16_t *p;

		p = plane ? vdp.bgb : vdp.bga;

		for (int y=1;y<28-1;y+=2) {
			int line = y*8;
			int row;//, pixrow;
			int16_t hscroll = 0;
			int8_t  col_off, pix_off;

			switch (vdp.regs[11] & 0x03) {
			case 0x0:
				hscroll = ((uint16_t *)vdp.vram)[(vdp.hs_off + (plane ? 2 : 0)) >> 1];
				break;
			case 0x1:
				hscroll = ((uint16_t *)vdp.vram)[(vdp.hs_off + ((line & 0x7) << 1) + (plane ? 2 : 0)) >> 1];
				break;
			case 0x2:
				hscroll = ((uint16_t *)vdp.vram)[(vdp.hs_off + ((line & ~0x7) << 1) + (plane ? 2 : 0)) >> 1];
				break;
			case 0x3:
				hscroll = ((uint16_t *)vdp.vram)[(vdp.hs_off + (line << 2) + (plane ? 2 : 0)) >> 1];
				break;
			}

			hscroll = (0x400 - hscroll) & 0x3ff;
			col_off = hscroll >> 3;
			pix_off = hscroll & 0x7;

			if ((vdp.regs[11] & 0x04) == 0)
				line = (line + (vdp.vsram[(plane ? 1 : 0)] & 0x3ff)) % (vdp.sc_height << 3);

			row = (line / 8) * vdp.sc_width;
			//pixrow = line % 8;

			for (int x=1;x<(vdp.dis_cells)-1;x++) {
				uint16_t name_ent = p[row + ((col_off + ((pix_off + (x*8)) >> 3)) % vdp.sc_width)];
				uint16 tn = (name_ent & 0x7ff);
				if (tn > last_larget_tn_used) {
					last_larget_tn_used = tn;
				}

				int priority = (name_ent >> 15);
#if 1

				// we haven't already loaded this tile if necessary
				if (!tn_used[tn]) {
//				if (!is_tile_loaded(tn)) {
					uint8_t *source = (vdp.vram + (tn << 5));
					uint32_t *data = (uint32_t *)source;
					uint8_t *pixels = (uint8_t* )data_copy;

					tn_used[tn] = 1;
//					loaded_tile(tn);

					memcpy(data_copy, data, 32);
					for(int di = 0; di < 8; di++) {
						SWAP_WORDS(data_copy[di]);
					}

#if TWIDDLEIT
/*						for (int ti = 0; ti < 8; ti += 2) {
							int yout = ti;
							for (int tj = 0; tj < 8; tj += 2) {
								tmptex[TWIDOUT((tj & 7) / 2, (yout & 7) / 2) + (tj / 8 + yout / 8)*8 * 8 / 4] =
									(pixels[(tj + ti * 8) >> 1] & 15) | ((pixels[(tj + (ti + 1) * 8) >> 1] & 15) << 4) |
									((pixels[(tj + ti * 8) >> 1] >> 4) << 8) | ((pixels[(tj + (ti + 1) * 8) >> 1] >> 4) << 12);
							}
						}*/

					for(uint32_t tj = 0; tj < 8; tj += 2) {
					for(uint32_t ti = 0; ti < 8; ti += 2) {
						uint32_t ti8 = ti << 3;
						uint32_t yout = ti;
						tmptex[TWIDOUT((tj /*& 7*/) >> 1, (yout /*& 7*/) >> 1) + (((tj >> 3) + (yout >> 3))<<4)] =
							(pixels[(tj + ti8) >> 1] & 15) | ((pixels[(tj + ti8 + 8) >> 1] & 15) << 4) |
							((pixels[(tj + ti8) >> 1] >> 4) << 8) | ((pixels[(tj + ti8 + 8) >> 1] >> 4) << 12);
					}
					}
					pvr_txr_load(tmptex, tn_ptr[tn], 32);
#else
					pvr_txr_load(pixels, tn_ptr[tn], 32);
#endif
				}
#endif
				// finished loading tile into texture here
				pvr_poly_cxt_t cxt;
				int pal = (name_ent >> 13) & 0x3;
				int vft = ((name_ent >> 12) & 0x1);
				int hft = ((name_ent >> 11) & 0x1);

				struct plane_pvr_tile *tmp = &planes_pool[planes_size];

				pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_PAL4BPP | PVR_TXRFMT_4BPP_PAL(pal), 8, 8, tn_ptr[tn], PVR_FILTER_NONE);
				pvr_poly_compile(&tile_hdr[priority][plane][(y*40)+x], &cxt);
				cxt.blend.src = PVR_BLEND_DESTCOLOR;
				cxt.blend.dst = PVR_BLEND_ZERO;					

				tmp->hdr = &tile_hdr[priority][plane][(y*40)+x];
				tmp->x = x;
				tmp->y = y;
				tmp->hf = hft;
				tmp->vf = vft;
				tmp->priority = priority;
				tmp->plane = plane;

				planes_size++;
			}
		}
	}
}

#define spr_start (vdp.vram + sn)

void vdp_render_pvr_sprites(void) {
	uint64_t spr_ent;
	uint32_t spr_ent_bot,spr_ent_top;
	int32_t sy;
	uint32_t sl, sv;
	memset(tn_used,0,2048);
	sprites_size = 0;

	uint32_t c=0,cells=64;

    if (32 != vdp.dis_cells) {
        cells = 80;
	}

    for (int i=0;i<cells;++i) {
		spr_ent = vdp.sat[c];
        spr_ent_bot = (spr_ent >> 32);
		spr_ent_top = (spr_ent & 0x00000000ffffffff);

        SWAP_WORDS(spr_ent_bot);
		SWAP_WORDS(spr_ent_top);

        sy = ((spr_ent_top & 0x03FF0000) >> 16)-128;
        sv = ((spr_ent_top & 0x00000300) >> 8)+1;

        if (0 <= sy && (sy+(sv<<3)) <= 223) {
			uint32_t sp, sh, sn, sc, shf, svf;
			int32_t sx;

			sx = (spr_ent_bot & 0x000003FF)-128;		

            if (sx < -31 || sy < -31) {
                goto next_sprite;
			}

            svf = (spr_ent_bot & 0x10000000) >> 28;
            shf = (spr_ent_bot & 0x08000000) >> 27;
            sp = (spr_ent_bot & 0x80000000) >> 31;
            sn = (spr_ent_bot & 0x07FF0000) >> 11;
            sc = (spr_ent_bot & 0x60000000) >> 29;
			
            sh = ((spr_ent_top & 0x00000C00) >> 10)+1;

			struct vdp_pvr_sprite *tmp = &sprites_pool[sprites_size];
			sprites_size++;
			tmp->x = sx;
			tmp->y = sy;
			tmp->v = sv;
			tmp->h = sh;
			tmp->vf = svf;
			tmp->hf = shf;
			tmp->priority = sp;
			
            for (int v = 0; v < sv; ++v) {
                for (int h = 0; h < sh; ++h) {
					uint16_t sprite_tn = (sn>>5) + ((sv*h)+v);
					if (sprite_tn > last_larget_tn_used) {
						last_larget_tn_used = sprite_tn;
					}
#if 1
					if (!tn_used[sprite_tn]) {
//					if (!is_tile_loaded(sprite_tn)) {
						uint8_t *source = (uint8_t *)(spr_start + (((sv*h)+v)<<5));
						uint32_t *data = (uint32_t *)source;
						uint8_t *pixels = (uint8_t *)data_copy;

						tn_used[sprite_tn] = 1;
//						loaded_tile(sprite_tn);

						memcpy(data_copy, data, 32);
						for(int di = 0; di < 8; di++) {
							SWAP_WORDS(data_copy[di]);
						}
		
#if TWIDDLEIT
/*						for (int ti = 0; ti < 8; ti += 2) {
							int yout = ti;
							for (int tj = 0; tj < 8; tj += 2) {
								tmptex[TWIDOUT((tj & 7) / 2, (yout & 7) / 2) + (tj / 8 + yout / 8)*8 * 8 / 4] =
									(pixels[(tj + ti * 8) >> 1] & 15) | ((pixels[(tj + (ti + 1) * 8) >> 1] & 15) << 4) |
									((pixels[(tj + ti * 8) >> 1] >> 4) << 8) | ((pixels[(tj + (ti + 1) * 8) >> 1] >> 4) << 12);
							}
						}*/
						for(uint32_t tj = 0; tj < 8; tj += 2) {
						for(uint32_t ti = 0; ti < 8; ti += 2) {
							uint32_t ti8 = ti << 3;
							uint32_t yout = ti;
							tmptex[TWIDOUT((tj /*& 7*/) >> 1, (yout /*& 7*/) >> 1) + (((tj >> 3) + (yout >> 3))<<4)] =
								(pixels[(tj + ti8) >> 1] & 15) | ((pixels[(tj + ti8 + 8) >> 1] & 15) << 4) |
								((pixels[(tj + ti8) >> 1] >> 4) << 8) | ((pixels[(tj + ti8 + 8) >> 1] >> 4) << 12);
							}
						}
						pvr_txr_load(tmptex, tn_ptr[sprite_tn], 32);
#else
						pvr_txr_load(pixels, tn_ptr[sprite_tn], 32);
#endif
					}
#endif					
					tmp->hdr[(v*sh)+h] = &sprite_hdr[sp][c][(v*sh)+h];

					pvr_poly_cxt_t cxt;
					pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_PAL4BPP | PVR_TXRFMT_4BPP_PAL(sc), 8, 8, tn_ptr[sprite_tn], PVR_FILTER_NONE);
					pvr_poly_compile(&sprite_hdr[sp][c][(v*sh)+h], &cxt);
					cxt.blend.src = PVR_BLEND_DESTCOLOR;
					cxt.blend.dst = PVR_BLEND_ZERO;
               } 
            }
        }
next_sprite:
        sl = (spr_ent_top & 0x0000007F);	
        if(sl) {
            c = sl;
		}
        else {
            break;
		}
    }
}

#if 1

uint16_t *ocr_vram_pp[2] = {(uint16_t *)0x7c002000, (uint16_t *)(0x7c002000+1024) }; // /*[2]*/[512];
uint16_t *ocr_vram;

void vdp_render_plane(int line) 
//, int plane, int priority)
{
//	int updated[2][2] = {{0}};
//	if(1) return;
	int i;
	if(line == 0) {
		memset(tn_16used,0,2048);
	}
#if 1	
	//uint32_t fill = 0x00000000;//vdp.dc_cram[vdp.regs[7] & 0x3f] << 16 | vdp.dc_cram[vdp.regs[7] & 0x3f];
	/* Prefill the scanline with the backdrop color. */
	memset(ocr_vram_pp[0]/*[0]*/,0, 2*8*vdp.dis_cells);
//	memset(ocr_vram_pp[0][1],0, 2*8*vdp.dis_cells);
	memset(ocr_vram_pp[1]/*[0]*/,0, 2*8*vdp.dis_cells);
//	memset(ocr_vram_pp[1][1],0, 2*8*vdp.dis_cells);
#endif		

	if (vdp.regs[1] & 0x40) {	
	
	for(int plane=1;plane>-1;plane--) {
	//plane<2;plane++) {
	int y = line;
	int row, pixrow, i, j;
	int16_t hscroll = 0;
	int8_t  col_off, pix_off, pix_tmp;
	uint16_t *p;

	p = plane ? vdp.bgb : vdp.bga;

	switch(vdp.regs[11] & 0x03) {
	case 0x0:
		hscroll = ((uint16_t *)vdp.vram)[(vdp.hs_off + (plane ? 2 : 0)) >> 1];
		break;
	case 0x1:
		hscroll = ((uint16_t *)vdp.vram)[(vdp.hs_off + ((line & 0x7) << 1) + (plane ? 2 : 0)) >> 1];
		break;
	case 0x2:
		hscroll = ((uint16_t *)vdp.vram)[(vdp.hs_off + ((line & ~0x7) << 1) + (plane ? 2 : 0)) >> 1];
		break;
	case 0x3:
		hscroll = ((uint16_t *)vdp.vram)[(vdp.hs_off + (line << 2) + (plane ? 2 : 0)) >> 1];
		break;
	}

	hscroll = (0x400 - hscroll) & 0x3ff;
	col_off = hscroll >> 3;
	pix_off = hscroll & 0x7;
//	pix_tmp = pix_off;

	if ((vdp.regs[11] & 0x04) == 0)
		y = (line + (vdp.vsram[(plane ? 1 : 0)] & 0x3ff)) % (vdp.sc_height << 3);

	row = (y / 8) * vdp.sc_width;
	pixrow = y & 7; //% 8;

	i = 0;
	while (i < (vdp.dis_cells * 8)) {
		uint16_t name_ent = p[row + ((col_off + ((pix_off + i) >> 3)) % vdp.sc_width)];
		uint16_t tn = (name_ent & 0x7ff);
		int32_t pal, pixel;
		pal = (name_ent >> 9) & 0x30;

		// we haven't already loaded this tile if necessary
		if (!tn_16used[tn]) {
			uint8_t *source = (vdp.vram + (tn << 5));

			memcpy(data_copy, source, 32);
			for(int di = 0; di < 8; di++) {
				SWAP_WORDS(data_copy[di]);
			}			
			
			for(int k=0;k<8;k++) {
				uint32_t pixs = data_copy[k];
				for (j = 0; j < 8; j++) {
					pixel = pixs >> 28;
					pixs <<= 4;
					//if (pix_tmp > 0) {
					//	pix_tmp--;
					//	continue;
					//}
					if (pixel)
					tn_16bit[tn][(k*8)+j] = vdp.dc_cram[pal | pixel];
					else
					tn_16bit[tn][(k*8)+j] = 0;
				}
			}
			
			tn_16used[tn] = 1;
		}

		int priority = (name_ent >> 15);
		ocr_vram = ocr_vram_pp[priority];//[0];
//		updated[priority][plane] = 1;
		
		//if ((name_ent >> 15) == priority) {
//			uint32_t data;
//			int32_t pal, pixel;
			uint16_t *tn_pix;
			//pal = (name_ent >> 9) & 0x30;

			if ((name_ent >> 12) & 0x1) {
				//data = *(uint32_t *)(vdp.vram + ((name_ent & 0x7ff) << 5) + (28 - (pixrow * 4)));
				tn_pix = (uint16_t *)&tn_16bit[tn][(56 - (pixrow*8))];
			}
		else {
				//data = *(uint32_t *)(vdp.vram + ((name_ent & 0x7ff) << 5) + (pixrow * 4));
				tn_pix = (uint16_t *)&tn_16bit[tn][(pixrow<<3)];
		}
			//SWAP_WORDS(data);
#if 1
			if ((name_ent >> 11) & 0x1) {
				for (j = 0; j < 8; j++) {
					pixel = tn_pix[(8-j)];
					/*pixel = data & 0x0f;
					data >>= 4;
					*/
					if (pixel)
					ocr_vram[i] = pixel;//vdp.dc_cram[pal | pixel];
		//updated[priority][plane] = i;
					i++;
				}
			} else {
#endif
				for (j = 0; j < 8; j++) {
					pixel = tn_pix[j];
					/*pixel = data & 0x0f;
					data >>= 4;
					*/
					if (pixel)
					ocr_vram[i] = pixel;//vdp.dc_cram[pal | pixel];
		//updated[priority][plane] = i;
					i++;
				}
#if 1
			}
#endif
	}
	}

	}
		sq_cpy((((uint16_t *)disp_txr[0]/*[0]*/) + (line * 512)), ocr_vram_pp[0]/*[0]*/, (vdp.dis_cells * 8  * 2 ));
		//sq_cpy((((uint16_t *)disp_txr[0][1]) + (line * 512)), ocr_vram_pp[0][1], (vdp.dis_cells * 8  * 2 ));
		sq_cpy((((uint16_t *)disp_txr[1]/*[0]*/) + (line * 512)), ocr_vram_pp[1]/*[0]*/, (vdp.dis_cells * 8  * 2 ));
		//..sq_cpy((((uint16_t *)disp_txr[1][1]) + (line * 512)), ocr_vram_pp[1][1], (vdp.dis_cells * 8  * 2 ));


}
#if 0
void vdp_render_scanline(int line)
{
	int i;
	uint32_t fill = vdp.dc_cram[vdp.regs[7] & 0x3f] << 16 | vdp.dc_cram[vdp.regs[7] & 0x3f];
	/* Prefill the scanline with the backdrop color. */
	for (i = 0; i < (vdp.sc_width * 8); i++)
		((uint32_t*)ocr_vram)[i] = fill;

	if (vdp.regs[1] & 0x40) {
		vdp_render_plane(line, 1, 0);
		vdp_render_plane(line, 0, 0);
		vdp_render_sprites2(line, 0);
		vdp_render_plane(line, 1, 1);
		vdp_render_plane(line, 0, 1);
		vdp_render_sprites2(line, 1);
	}

	sq_cpy((((uint16_t *)display_txr) + (line * 512)), ocr_vram, (vdp.dis_cells * 8  * 2 ));
}
#endif

#endif