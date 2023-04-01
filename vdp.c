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

uint8_t __attribute__ ((aligned(8))) last_tn[2][2][40*28];


// priority plane tile
pvr_poly_hdr_t __attribute__ ((aligned(8))) tile_hdr[2][2][40*28];
struct plane_pvr_tile planes_pool[2240];
int planes_size;
// set when tile is loaded to pvr mem
uint8_t __attribute__ ((aligned(8))) tn_used[4096] = {0};
pvr_ptr_t __attribute__ ((aligned(8))) tn_ptr[4096];

// priority sprite tile 
pvr_poly_hdr_t __attribute__ ((aligned(8))) sprite_hdr[2][80][4*4];
struct vdp_pvr_sprite sprites_pool[80];
uint8_t list_ordered[80];
int sprites_size;


uint8_t nt_cells[4] = { 32, 64, 0, 128 };
uint8_t mode_cells[4] = { 32, 40, 0, 40 };
#define SWAP_WORDS(x) __asm__ volatile ("swap.w %0, %0" : "+r" (x))

#define get_color_argb1555(r,g,b,a) ((uint16_t)(((a&1)<<15) | ((r>>3)<<10) | ((g>>3)<<5) | (b>>3)))

//static uint16_t  ocr_vram[8192];//*ocr_vram = (uint16_t *)0x7c002000;
//uint8_t *tn_used = (uint8_t *)0x7c002000;

void vdp_init(void)
{
	//debug = 0;
#if 0
	int filters;
#if 0
	filters = vid_check_cable() ? PVR_FILTER_BILINEAR : PVR_FILTER_BILINEAR;
#else
	filters = PVR_FILTER_BILINEAR;
#endif
#endif

	pvr_set_pal_format(PVR_PAL_ARGB1555);
	memset(last_tn,-1,4*40*28);

	// 8*8@4bpp texture for each pattern in VDP VRAM
	for(int i=0;i<4096;i++) {
		tn_ptr[i] = pvr_mem_malloc(32);
	}

	// 4*4*(8*8@4bpp) texture for each sprite, over-allocates
#if 0
	for(int i=0;i<80;i++) {
		for(int j=0;j<16;j++) {
			sprite_ptr[0][i][j] = pvr_mem_malloc(32);
			sprite_ptr[1][i][j] = pvr_mem_malloc(32);
		}
	}
#endif
    vdp.sat_dirty = 1;
}

uint32_t data_copy[8];
uint16_t tmptex[32];


#if 1
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

				uint16_t col = vdp.dc_cram[val & 0x3f]; //vdp.dc_cram[vdp.regs[7] & 0x3f];

				r = ((col >> 10) & 0x1f);//<<3;
				g = ((col >> 5) & 0x1f);//<<3;
				b = ((col /*>> 0*/) & 0x1f);//<<3;

				pvr_set_bg_color(r / 32.0f/*255.0f*/, g / 32.0f/*255.0f*/, b / 32.0f/*255.0f*/);
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
				vdp.dis_cells = mode_cells[((vdp.regs[12] & 0x01) << 1) |
											(vdp.regs[12] >> 7)];
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
#if 0
						uint16_t start_vaddr_copy = vdp.addr;
						uint16_t len_copy_copy = len;
#endif
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
#if 0
						if(start_vaddr_copy % 32 == 0) {
							if(start_vaddr_copy >= 0 && start_vaddr_copy < 0x20000) {
								if(len_copy_copy % 32 == 0) {
									for(int ti=0;ti<len_copy_copy/32;ti++) {
									uint16_t name_ent = (start_vaddr_copy>>5) + ti;
									uint8_t *pixels = (vdp.vram + ((name_ent & 0x7ff) << 5));
									tn_used[name_ent & 0x7ff] = 1;
									uint32_t *data = (uint32_t *)pixels;
									for(int di = 0; di < 8; di++) {
										uint32_t d = data[di];
										SWAP_WORDS(d);
										data_copy[di] = d;
									}
									pixels = (uint8_t *)data_copy;
#if !TWIDDLEIT
									for(int i = 0; i < 8; i += 2) {

										int yout = i;

										for(int j = 0; j < 8; j += 2) {
											tmptex[TWIDOUT((j & 7) / 2, (yout & 7) / 2) + (j / 8 + yout / 8)*8 * 8 / 4] =
											(pixels[(j + i * 8) >> 1] & 15) | ((pixels[(j + (i + 1) * 8) >> 1] & 15) << 4) |
											((pixels[(j + i * 8) >> 1] >> 4) << 8) | ((pixels[(j + (i + 1) * 8) >> 1] >> 4) << 12);
										}
									}
									pvr_txr_load(tmptex, tn_ptr[(name_ent & 0x7ff)], 32);
#else
									pvr_txr_load(pixels, tn_ptr[(name_ent & 0x7ff)], 32);
#endif								
									}
								}
							}
						}
#endif						
					}
						break;
					case 0x03: {
						uint16_t start_vaddr = vdp.addr;
						uint16_t len_copy = len;
						/* cram */
						do {
							uint16_t laddr = vdp.addr >> 1;
							vdp.cram[laddr] = src_mem[src_off & src_mask];
							vdp.addr += vdp.regs[15];
							src_off += 1;

							//if(vdp.addr > 0x7f)
							if(laddr > 0x3f)
								break;
						} while(--len);

						/* cram */
						do {
							uint16_t laddr = start_vaddr >> 1;
							uint8_t r,g,b,a;
							val = vdp.cram[laddr];
							SWAPBYTES16(val);
							a = (laddr%16) != 0;
							r = (val & 0x000e) << 4;
							g = ((val>>4) & 0x000e) << 4;
							b = ((val>>8) & 0x000e) << 4;
							vdp.dc_cram[laddr] = get_color_argb1555(r,g,b,a);
								//(((len%16 == 0) ? 0 : 1) << 15)
								//|
////								(((val & 0x000e) << 12) |
	//							 ((val & 0x00e0) << 3) |
		//						 ((val & 0x0e00) >> 7));
		                    pvr_set_pal_entry(laddr, get_color_argb1555(r,g,b,a));
							start_vaddr += vdp.regs[15];
							src_off += 1;
#if 1
							if(laddr == vdp.regs[7]) {
	pvr_set_bg_color(r / 255.0f, g / 255.0f, b / 255.0f);

							}
#endif
							//if(vdp.addr > 0x7f)
							if(laddr > 0x3f)
								break;
						} while(--len_copy);
					} break;
					case 0x05: {
						uint16_t start_vaddr = vdp.addr;
						uint16_t len_copy = len;
						/* vsram */
						do {
							vdp.vsram[vdp.addr >> 1] = src_mem[src_off & src_mask];//val;
							vdp.addr += vdp.regs[15];
							src_off += 1;

							if(vdp.addr > 0x7f)
								break;
						} while(--len);

						do {
							SWAPBYTES16(vdp.vsram[start_vaddr >> 1]);
							start_vaddr += vdp.regs[15];
						} while(--len_copy);
					}	break;
						
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
#if 0
		if(!start_tile_write) {
			if(vdp.addr >= 0 && vdp.addr < 0x20000) {
				if (vdp.addr % 32 == 0) {
					start_tile_write = 1;
					start_tile_addr = vdp.addr;
				}
			}
		}
#endif
		((uint16_t *)vdp.vram)[vdp.addr >> 1] = val;
		vdp.addr += 2;

#if 0
		if(start_tile_write) {
			if(vdp.addr == (start_tile_addr + 32)) {
				start_tile_write = 0;
				uint16_t name_ent = (start_tile_addr>>5);
				uint8_t *pixels = (vdp.vram + ((name_ent & 0x7ff) << 5));
				uint32_t *data = (uint32_t *)pixels;
				for(int di = 0; di < 8; di++) {
					uint32_t d = data[di];
					SWAP_WORDS(d);
					data_copy[di] = d;
				}
				pixels = (uint8_t *)data_copy;
#if TWIDDLEIT
				for(int i = 0; i < 8; i += 2) {
					int yout = i;

					for(int j = 0; j < 8; j += 2) {
						tmptex[TWIDOUT((j & 7) / 2, (yout & 7) / 2) + (j / 8 + yout / 8)*8 * 8 / 4] =
						(pixels[(j + i * 8) >> 1] & 15) | ((pixels[(j + (i + 1) * 8) >> 1] & 15) << 4) |
						((pixels[(j + i * 8) >> 1] >> 4) << 8) | ((pixels[(j + (i + 1) * 8) >> 1] >> 4) << 12);
					}
				}
				pvr_txr_load(tmptex, tn_ptr[(name_ent & 0x7ff)], 32);
#else
				pvr_txr_load(pixels, tn_ptr[(name_ent & 0x7ff)], 32);
#endif				
			}
		}
#endif		
		break;
	case 0x03:
		uint32_t cram_addr = (vdp.addr >> 1) & 0x3f;
		uint8_t r,g,b,a;
//		SWAPBYTES16(val);
		vdp.cram[cram_addr] = val;
            //((vdp.addr >> 1)%16 == 0 ? 0 : 1) << 15 | 
			//(((val & 0x000e) << 12) |
			 //((val & 0x00e0) << 3) |
			 //((val & 0x0e00) >> 7));
//		    pvr_set_pal_entry((vdp.addr >> 1) & 0x3f, vdp.dc_cram[(vdp.addr >> 1) & 0x3f]);
		a = (cram_addr%16) != 0;
		r = (val & 0x000e)<<4;
		g = ((val & 0x00e0)>>4)<<4;
		b = ((val & 0x0e00)>>8)<<4;
		vdp.dc_cram[cram_addr] = get_color_argb1555(r,g,b,a);
		pvr_set_pal_entry(cram_addr, get_color_argb1555(r,g,b,a));		
		vdp.addr += 2;
#if 1
							if(cram_addr == vdp.regs[7]) {
	pvr_set_bg_color(r / 255.0f, g / 255.0f, b / 255.0f);

							}
#endif

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
			} else
			if ((vdp.code == 0x30) && ((vdp.regs[23] & 0xc0) == 0xc0)) {
				/* vdp copy */
				uint16_t len = (vdp.regs[20] << 8) | vdp.regs[19];
				uint16_t addr = (vdp.regs[22] << 8) | vdp.regs[21];

#if 0
				if (vdp.regs[15] == 2) {
					printf("got there\n");
					printf("got there\n");
					printf("got there\n");
					printf("got there\n");
					printf("got there\n");
					memcpy(vdp.vram + vdp.addr, vdp.vram + addr, len*2);
					vdp.addr += (len*2);
					
				}
				else 
#endif
				{
				do {
					vdp.vram[vdp.addr] = vdp.vram[addr++];
					vdp.addr += vdp.regs[15];
				} while(--len);
				}
			} else {
				//printf("VDP DMA Error, code %02x, r23 %02x\n", vdp.code, vdp.regs[23]);
			}
		}
	}
}

uint16_t vdp_hv_read(void)
{
	uint16_t h, v;

	v = vdp.scanline;
	if ( v > 0xea)
		v -= 6;

	h = (uint16_t)(m68k_cycles_run() * 0.70082f);
	if (h > 0xe9)
		h -= 86;

	return(((v & 0xff) << 8) | (h & 0xff));
}

void vdp_interrupt(int line)
{
	uint8_t h_int_pending = 0;

	vdp.scanline = line;

	if (vdp.h_int_counter == 0) {
		vdp.h_int_counter = vdp.regs[10];
		if (vdp.regs[0] & 0x10)
			h_int_pending = 1;
	}

	if (line < 224) {
		if (line == 0) {
			vdp.h_int_counter = vdp.regs[10];
			vdp.status &= 0x0001;
		}

		if (h_int_pending) {
			m68k_set_irq(4);
		}
	} else
	if (line == 224) {
		z80_set_irq_line(0, PULSE_LINE);
		if (h_int_pending) {
			m68k_set_irq(4);
		} else {
			if (vdp.regs[1] & 0x20) {
				vdp.status |= 0x0080;
				m68k_set_irq(6);
			}
		}
	} else {
		vdp.h_int_counter = vdp.regs[10];
		vdp.status |= 0x08;
	}

	vdp.h_int_counter--;
}
#endif 

int last_larget_tn_used = 0;
void vdp_setup_pvr_planes(void) {
	memset(tn_used,0,32*last_larget_tn_used);
#if 0
	for(int tn=0;tn<last_larget_tn_used;tn++) {
		uint8_t *pixels = (vdp.vram + (tn << 5));
		//tn_used[tn] = 1;
		uint32_t *data = (uint32_t *)pixels;
		for(int di = 0; di < 8; di++) {
			uint32_t d = data[di];
			SWAP_WORDS(d);
			data_copy[di] = d;
		}
		pixels = (uint8_t *)data_copy;
#if TWIDDLEIT
		for(int i = 0; i < 8; i += 2) {
			int yout = i;
			for(int j = 0; j < 8; j += 2) {
				tmptex[TWIDOUT((j & 7) / 2, (yout & 7) / 2) + (j / 8 + yout / 8)*8 * 8 / 4] =
					(pixels[(j + i * 8) >> 1] & 15) | ((pixels[(j + (i + 1) * 8) >> 1] & 15) << 4) |
					((pixels[(j + i * 8) >> 1] >> 4) << 8) | ((pixels[(j + (i + 1) * 8) >> 1] >> 4) << 12);
			}
		}
		pvr_txr_load(tmptex, tn_ptr[tn], 32);
#else
		pvr_txr_load(pixels, tn_ptr[tn], 32);	
#endif	
	}
#endif
	last_larget_tn_used = 0;
	planes_size = 0;
	
}

// builds all plane/priority combos in one pass
void vdp_render_pvr_planes(void) {
	for(int plane=1;plane>-1;plane--) {
		uint16_t *p;

		p = plane ? vdp.bgb : vdp.bga;

		for(int y=0;y<28;y+=2) {
			int line = y*8;
			int row;//, pixrow;
			int16_t hscroll = 0;
			int8_t  col_off, pix_off;

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

			if ((vdp.regs[11] & 0x04) == 0)
				line = (line + (vdp.vsram[(plane ? 1 : 0)] & 0x3ff)) % (vdp.sc_height << 3);

			row = (line / 8) * vdp.sc_width;
			//pixrow = line % 8;

			for(int x=0;x<(vdp.dis_cells);x++) {
				uint16_t name_ent = p[row + ((col_off + ((pix_off + (x*8)) >> 3)) % vdp.sc_width)];
				int tn = (name_ent & 0x7ff);
				if (tn > last_larget_tn_used) {
					last_larget_tn_used = tn;
				}

				int priority = (name_ent >> 15);
				//if ((name_ent >> 15) == priority) {
					// we haven't already loaded this tile if necessary
#if 1
					if(!tn_used[tn]) {
						uint8_t *pixels = (vdp.vram + (tn << 5));
						tn_used[tn] = 1;
						uint32_t *data = (uint32_t *)pixels;
						for(int di = 0; di < 8; di++) {
							uint32_t d = data[di];
							SWAP_WORDS(d);
							data_copy[di] = d;
						}
						pixels = (uint8_t *)data_copy;
#if TWIDDLEIT
						for(int i = 0; i < 8; i += 2) {

							int yout = i;

							for(int j = 0; j < 8; j += 2) {
								tmptex[TWIDOUT((j & 7) / 2, (yout & 7) / 2) + (j / 8 + yout / 8)*8 * 8 / 4] =
								(pixels[(j + i * 8) >> 1] & 15) | ((pixels[(j + (i + 1) * 8) >> 1] & 15) << 4) |
								((pixels[(j + i * 8) >> 1] >> 4) << 8) | ((pixels[(j + (i + 1) * 8) >> 1] >> 4) << 12);
							}
						}
						pvr_txr_load(tmptex, tn_ptr[tn], 32);
#else
						pvr_txr_load(pixels, tn_ptr[tn], 32);
#endif
					}
#endif
					// finished loading tile into texture here
					//if((name_ent & 0x7ff) != last_tn[priority][plane][(y*40)+x]) 
					{
					pvr_poly_cxt_t cxt;
					//last_tn[priority][plane][(y*40)+x] = (name_ent & 0x7ff);
					int pal = (name_ent >> 13) & 0x3;
					int vft = ((name_ent >> 12) & 0x1);
					int hft = ((name_ent >> 11) & 0x1);

					struct plane_pvr_tile *tmp = &planes_pool[planes_size];//(struct plane_pvr_tile *)malloc(sizeof(struct plane_pvr_tile));

					pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_PAL4BPP | PVR_TXRFMT_4BPP_PAL(pal), 8, 8, tn_ptr[tn], PVR_FILTER_BILINEAR);
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
				//}
			}
		}
	}
}

#define spr_start (vdp.vram + sn)

void vdp_render_pvr_sprites(void) {
    uint32_t spr_ent_bot,spr_ent_top;
    uint32_t c=0, cells=64, i=0, sp, sl, sh, sv, sn, sc, shf, svf;
    int32_t sx, sy;
    uint64_t spr_ent;
	memset(list_ordered,-1,80);
	sprites_size = 0;
    if (!(vdp.dis_cells == 32))
        cells = 80;

    vdp.status &= 0x0040; // not too sure about this... 

    for(i=0;i<cells;++i)
    {
		spr_ent = vdp.sat[c];

        spr_ent_bot = (spr_ent >> 32);
        SWAP_WORDS(spr_ent_bot);
		spr_ent_top = (spr_ent & 0x00000000ffffffff);
		SWAP_WORDS(spr_ent_top);
        sy = ((spr_ent_top & 0x03FF0000) >> 16)-128;
        sv = ((spr_ent_top & 0x00000300) >> 8)+1;

        if (0 <= sy && (sy+(sv<<3)) <= 223)
		{
//            sp = (spr_ent_bot & 0x80000000) >> 31;
        	list_ordered[i] = c;
        }

        sl = (spr_ent_top & 0x0000007F);	
        if(sl)
            c = sl;
        else
            break;			
    }
	
    for(i=0;i<cells;i++)
    {
		int next_index = list_ordered[( 79 - ( (cells==64)?16:0 ) - i )];
		if( next_index > -1 )
		{
			spr_ent = vdp.sat[next_index];

            spr_ent_bot = (spr_ent >> 32);
            SWAP_WORDS(spr_ent_bot);
			spr_ent_top = (spr_ent & 0x00000000ffffffff);
			SWAP_WORDS(spr_ent_top);
            sy = ((spr_ent_top & 0x03FF0000) >> 16)-128;
            sh = ((spr_ent_top & 0x00000C00) >> 10)+1;
            sv = ((spr_ent_top & 0x00000300) >> 8)+1;
            sp = (spr_ent_bot & 0x80000000) >> 31;
            svf = (spr_ent_bot & 0x10000000) >> 28;
            shf = (spr_ent_bot & 0x08000000) >> 27;
            sn = (spr_ent_bot & 0x07FF0000) >> 11;
            sx = (spr_ent_bot & 0x000003FF)-128;		
            sc = (spr_ent_bot & 0x60000000) >> 29;

            if (sx < -31 || sy < -31)
                continue;

			struct vdp_pvr_sprite *tmp = &sprites_pool[sprites_size];
			sprites_size++;
			tmp->x = sx;
			tmp->y = sy;
			tmp->v = sv;
			tmp->h = sh;
			tmp->vf = svf;
			tmp->hf = shf;
			tmp->priority = sp;
			
            for(int v = 0; v < sv; ++v) 
            {
                for(int h = 0; h < sh; ++h) 
                {
					int sprite_tn = (sn>>5) + ((sv*h)+v);
#if 1
					if(!tn_used[sprite_tn]) {
						uint8_t *pixels;
						tn_used[sprite_tn] = 1;
						pixels = (uint8_t *)(spr_start + (((sv*h)+v)<<5));
						uint32_t *data = (uint32_t *)pixels;
						for(int di = 0; di < 8; di++) {
							uint32_t d = data[di];
							SWAP_WORDS(d);
							data_copy[di] = d;
						}
						pixels = (uint8_t*)data_copy;
#if TWIDDLEIT
						for(int ti = 0; ti < 8; ti += 2) {
							int yout = ti;
							for(int tj = 0; tj < 8; tj += 2) {
								tmptex[TWIDOUT((tj & 7) / 2, (yout & 7) / 2) + (tj / 8 + yout / 8)*8 * 8 / 4] =
								(pixels[(tj + ti * 8) >> 1] & 15) | ((pixels[(tj + (ti + 1) * 8) >> 1] & 15) << 4) |
								((pixels[(tj + ti * 8) >> 1] >> 4) << 8) | ((pixels[(tj + (ti + 1) * 8) >> 1] >> 4) << 12);
							}
						}		
						pvr_txr_load(tmptex, tn_ptr[sprite_tn], 32);
#else
						pvr_txr_load(pixels, tn_ptr[sprite_tn], 32);
#endif
					}
#endif					
					tmp->hdr[(v*sh)+h] = &sprite_hdr[sp][next_index][(v*sh)+h];

					pvr_poly_cxt_t cxt;
					pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, PVR_TXRFMT_PAL4BPP | PVR_TXRFMT_4BPP_PAL(sc), 8, 8, tn_ptr[sprite_tn], PVR_FILTER_BILINEAR);
					pvr_poly_compile(&sprite_hdr[sp][next_index][(v*sh)+h], &cxt);
					cxt.blend.src = PVR_BLEND_DESTCOLOR;
					cxt.blend.dst = PVR_BLEND_ZERO;
               } 
            } 
        }       
    }	
}

#if 0
void vdp_render_plane(int line, int plane, int priority)
{
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
	pix_tmp = pix_off;

	if ((vdp.regs[11] & 0x04) == 0)
		line = (line + (vdp.vsram[(plane ? 1 : 0)] & 0x3ff)) % (vdp.sc_height << 3);

	row = (line / 8) * vdp.sc_width;
	pixrow = line % 8;

	i = 0;
	while (i < (vdp.dis_cells * 8)) {
		uint16_t name_ent = p[row + ((col_off + ((pix_off + i) >> 3)) % vdp.sc_width)];
		if ((name_ent >> 15) == priority) {
			uint32_t data;
			int32_t pal, pixel;

			pal = (name_ent >> 9) & 0x30;

			if ((name_ent >> 12) & 0x1)
				data = *(uint32_t *)(vdp.vram + ((name_ent & 0x7ff) << 5) + (28 - (pixrow * 4)));
			else
				data = *(uint32_t *)(vdp.vram + ((name_ent & 0x7ff) << 5) + (pixrow * 4));
			SWAP_WORDS(data);

			if ((name_ent >> 11) & 0x1) {
				for (j = 0; j < 8; j++) {
					pixel = data & 0x0f;
					data >>= 4;
					if (pix_tmp > 0) {
						pix_tmp--;
						continue;
					}
					if (pixel)
						ocr_vram[i] = vdp.dc_cram[pal | pixel];
					i++;
				}
			} else {
				for (j = 0; j < 8; j++) {
					pixel = data >> 28;
					data <<= 4;
					if (pix_tmp > 0) {
						pix_tmp--;
						continue;
					}
					if (pixel)
						ocr_vram[i] = vdp.dc_cram[pal | pixel];
					i++;
				}
			}
		} else {
			if (pix_tmp > 0) {
				i += (8 - pix_tmp);
				pix_tmp = 0;
			} else {
				i += 8;
			}
		}
	}
}

struct spr_ent_s {
    int16_t y;
    uint8_t v;
    uint8_t h;
    uint8_t l;
    uint16_t n;
    uint8_t hf;
    uint8_t vf;
    uint8_t pal;
    uint8_t prio;
    int16_t x;
};

static uint16_t pix_byte_map[4][32] = {
    {   1,   1,   0,   0,   3,   3,   2,   2,
       33,  33,  32,  32,  35,  35,  34,  34,
       65,  65,  64,  64,  67,  67,  66,  66,
       97,  97,  96,  96,  99,  99,  98,  98
    },
    {   1,   1,   0,   0,   3,   3,   2,   2,
       65,  65,  64,  64,  67,  67,  66,  66,
      129, 129, 128, 128, 131, 131, 130, 130,
      193, 193, 192, 192, 195, 195, 194, 194
    },
    {   1,   1,   0,   0,   3,   3,   2,   2,
       97,  97,  96,  96,  99,  99,  98,  98,
      193, 193, 192, 192, 195, 195, 194, 194,
      289, 289, 288, 288, 291, 291, 290, 290
    },
    {   1,   1,   0,   0,   3,   3,   2,   2,
      129, 129, 128, 128, 131, 131, 130, 130,
      257, 257, 256, 256, 259, 259, 258, 258,
      385, 385, 384, 384, 387, 387, 386, 386
    }
};

void vdp_render_sprites2(int line, int priority)
{
    static struct spr_ent_s spr_list[80] = { {0} };
    static uint8_t spr_list_len = 0;
    static uint8_t spr_this_field = 0;

    uint8_t max_sprites = vdp.dis_cells == 40 ? 80 : 64;

    if (line == 0 && priority == 0) {
        spr_this_field = 0;
        int i = 0, j = 0;
        while (j < max_sprites && i < max_sprites) {
            uint8_t *spr_ent_raw = (uint8_t *)&(vdp.sat[i]);
            struct spr_ent_s *spr_ent = &(spr_list[j++]);
            spr_ent->y = (((spr_ent_raw[1] & 0x03) << 8) | spr_ent_raw[0]) - 128;
            spr_ent->v = (spr_ent_raw[3] & 0x03) + 1;
            spr_ent->h = ((spr_ent_raw[3] & 0x0c) >> 2) + 1;
            spr_ent->l = spr_ent_raw[2] & 0x7f;

            spr_ent->n = (((spr_ent_raw[5] & 0x07) << 8) | spr_ent_raw[4]) << 5;
            spr_ent->hf = (spr_ent_raw[5] & 0x08) >> 3;
            spr_ent->vf = (spr_ent_raw[5] & 0x10) >> 4;
            spr_ent->pal = (spr_ent_raw[5] & 0x60) >> 1;
            spr_ent->prio = (spr_ent_raw[5] & 0x80) >> 7;
            spr_ent->x = (((spr_ent_raw[7] & 0x03) << 8) | spr_ent_raw[6]) - 128;

            i = spr_ent->l;
            if (i == 0)
                break;
        }
        spr_list_len = j;
    }

    uint8_t max_spr_per_line = vdp.dis_cells / 2;
    uint16_t max_spr_pix_per_line = vdp.dis_cells * 8;
    struct spr_ent_s *spr_to_render[20] = { 0 };
    uint8_t spr_to_render_len = 0;

    uint8_t pix_overflow = 0;
    /* Walk sprite list to find what spites we need to render */
    for (int i = 0, j = 0, k = 0; j < max_spr_per_line && k < max_spr_pix_per_line && spr_this_field < max_sprites && i < spr_list_len;) {
        struct spr_ent_s *spr_ent = &(spr_list[i++]);

        /* Contains our scanline? */
        if ((line < spr_ent->y) || (line >= (spr_ent->y + (spr_ent->v * 8)))) {
            continue;
        }

        /* Masking? */
        if (spr_ent->x == -128) {
            break;
        }

        /* Priority match? */
        if (spr_ent->prio != priority) {
            continue;
        }

        spr_to_render[j++] = spr_ent;
        spr_to_render_len++;
        k += spr_ent->h * 8;
        if (k > max_spr_pix_per_line) {
            pix_overflow = k - max_spr_pix_per_line;
        }
    }

    /* Render sprites back to front */
    for (int i = spr_to_render_len - 1; i >= 0; i--) {
        struct spr_ent_s *spr_ent = spr_to_render[i];
        uint8_t spr_pat_width = spr_ent->h * 8;

        /* Offscreen? */
        if (spr_ent->x < -(spr_pat_width - 1) ||
            spr_ent->x > max_spr_pix_per_line) {
            if (pix_overflow > 0) {
                pix_overflow = 0;
            }
            continue;
        }

        uint8_t spr_pat_height = spr_ent->v * 8;
        uint8_t spr_pat_line, spr_pat_vskip;
        if (!spr_ent->vf) {
            spr_pat_line = (line - spr_ent->y) % 8;
            spr_pat_vskip = (line - spr_ent->y) / 8;
        } else {
            spr_pat_line = (spr_pat_height - 1 - (line - spr_ent->y)) % 8;
            spr_pat_vskip = (spr_pat_height - 1 - (line - spr_ent->y)) / 8;
        }
        uint16_t spr_pat_voff = spr_pat_vskip * 32 + spr_pat_line * 4;

        uint8_t *pixels = vdp.vram + spr_ent->n + spr_pat_voff;
//		if (alltime_maxtn < ((spr_ent->n + spr_pat_voff)>>5)) {
//				alltime_maxtn = ((spr_ent->n + spr_pat_voff)>>5);
//		}
        uint8_t pal = spr_ent->pal;
        uint8_t pat_stride_idx = spr_ent->v - 1;

        if (pix_overflow > 0) {
            spr_pat_width -= pix_overflow;
            pix_overflow = 0;
        }
        for (int x = spr_ent->x, i = 0; i < spr_pat_width && x < max_spr_pix_per_line; i++, x++) {
            if (x < 0) {
                continue;
            }

            uint8_t pixel = 0;
            if (!spr_ent->hf) {
                pixel = pixels[pix_byte_map[pat_stride_idx][i]];
                pixel = i % 2 == 0 ? (pixel & 0xf0) >> 4 : pixel & 0x0f;
            } else {
                pixel = pixels[pix_byte_map[pat_stride_idx][spr_pat_width-1-i]];
                pixel = i % 2 == 1 ? (pixel & 0xf0) >> 4 : pixel & 0x0f;
            }

            if (pixel)
                ocr_vram[x] = vdp.dc_cram[pal | pixel];
        }
    }
}


void vdp_render_sprites1(int line, int priority)
{
    uint8_t pixel;
    uint16_t *pal;
    uint32_t data;
    uint32_t spr_ent_bot,spr_ent_top;
    uint32_t c=0, cells=64, i=0, j, k, h,v, sp, sl, sh, sv, sn, sc, shf, svf;
    uint32_t dis_line=16;
    uint32_t ppl=0;
    uint32_t dis_ppl=256;
    uint32_t sol=0;  
    int32_t sx, sy;
    int32_t list_ordered[80];
    uint64_t spr_ent;

    for(j=0;j<80;++j)
        list_ordered[j]=-1;

    if (!(vdp.dis_cells == 32))
        cells = 80;

    if(cells == 80)
    {
        dis_line=20;	
        dis_ppl=320;
    }

    vdp.status &= 0x0040; // not too sure about this... 

    for(i=0;i<cells;++i)
    {
		spr_ent = vdp.sat[c];

        spr_ent_bot = (spr_ent >> 32);
        SWAP_WORDS(spr_ent_bot);
		spr_ent_top = (spr_ent & 0x00000000ffffffff);
		SWAP_WORDS(spr_ent_top);
        sy = ((spr_ent_top & 0x03FF0000) >> 16)-128;
        sh = ((spr_ent_top & 0x00000C00) >> 10)+1;
        sv = ((spr_ent_top & 0x00000300) >> 8)+1;

        if((line >= sy) && (line < (sy+(sv<<3)))) 
        {
            sp = (spr_ent_bot & 0x80000000) >> 31;

            if(sp == priority) 
            {
	        	list_ordered[i]=c;
	    	}

            sol++;
            ppl+=(sh<<3);

            if(!(sol < dis_line))
            {
     	        vdp.status |= 0x0040;
				goto end;
            }			

	    	if(!(ppl < dis_ppl))
	    	{
	        	goto end;
            } 		
        }

        sl = (spr_ent_top & 0x0000007F);	
        if(sl)
            c = sl;
        else
end:
            break;			
    }

    for(i=0;i<cells;i++)
    {
        if( list_ordered[ ( 79 - ( (cells==64)?16:0 ) - i ) ] > -1 )
        {
	    	spr_ent = vdp.sat[list_ordered[(79-((cells==64)?16:0)-i)]];
            spr_ent_bot = (spr_ent >> 32);
            SWAP_WORDS(spr_ent_bot);
			spr_ent_top = (spr_ent & 0x00000000ffffffff);
			SWAP_WORDS(spr_ent_top);
            sy = ((spr_ent_top & 0x03FF0000) >> 16)-128;
            sh = ((spr_ent_top & 0x00000C00) >> 10)+1;
            sv = ((spr_ent_top & 0x00000300) >> 8)+1;
            svf = (spr_ent_bot & 0x10000000) >> 28;
            shf = (spr_ent_bot & 0x08000000) >> 27;
            sn = (spr_ent_bot & 0x07FF0000) >> 11;
            sx = (spr_ent_bot & 0x000003FF)-128;		
            sc = (spr_ent_bot & 0x60000000) >> 29;
            pal = vdp.dc_cram + (sc << 4);	 

            if (sx < -31 || sy < -31)
                continue;

            for(v = 0; v < sv; ++v) 
            {
                for(k=0;k<8;k++)
        		{
                    if((sy+(v<<3)+k) == line) 
                    {
                        for(h = 0; h < sh; ++h) 
                        {
                            if (!svf) 
    						{
                                if(shf)
                                    data = *(uint32_t *)(spr_start + (((sv*(sh-h-1))+v)<<5) + (k << 2));
                                else
                                    data = *(uint32_t *)(spr_start + (((sv*h)+v)<<5) + (k << 2));
                            }
                            else 
    						{
                                if(shf)
                                    data = *(uint32_t *)(spr_start + (((sv*(sh-h-1))+(sv-v-1))<<5) + (28 - (k << 2)));
                                else
                                    data = *(uint32_t *)(spr_start + (((sv*h)+(sv-v-1))<<5) + (28 - (k << 2)));
                            }
                            SWAP_WORDS(data);

                            if (shf) 
                            {
								for(j=0;j<8;j++)
                                {
                                    pixel = data & 0x0f;
                                    data >>= 4;
                                    if (pixel)	
                                        ocr_vram[sx + j + (h<<3)] = pal[pixel];
                                }
                            }
                            else 
                            {
								for(j=0;j<8;j++)
                                {
                                    pixel = data >> 28;
                                    data <<= 4;
                                    if (pixel)
                                        ocr_vram[sx + j + (h<<3)] = pal[pixel];
                                }
                            }
                        } 
                    } 
                } 
            } 
        }       
    }
}
#endif