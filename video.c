#include <kos.h>

#include "gen-emu.h"
#include "vdp.h"

extern struct vdp_s vdp;
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
/* PVR data from vdp.c */
extern pvr_poly_hdr_t disp_hdr[2];
extern pvr_ptr_t disp_txr[2];
extern pvr_ptr_t display_txr;
extern pvr_poly_hdr_t cram_hdr;
extern pvr_ptr_t cram_txr;
extern uint8_t display_ptr;
pvr_vertex_t vert;//[2][2][40*28][4];

extern pvr_poly_hdr_t tile_hdr[2][2][40*28];
extern pvr_ptr_t tile_txr[2][2][40*28];
extern uint8_t skip[2][2][40*28];
extern uint8_t hf[2][2][40*28];
extern uint8_t sprite_used[2][80];
extern pvr_poly_hdr_t sprite_hdr[2][80][16];
extern uint8_t sprnum[2][80];

void do_frame()
{
	pvr_vertex_t vert;
//	int x, y, w, h;

	vert.argb = 0xffffffff;
	vert.oargb = 0x00000000;
	vert.z = 1;

//	pvr_wait_ready();
	pvr_scene_begin(); 
	pvr_list_begin(PVR_LIST_TR_POLY);

	for(int tpr=0;tpr<2;tpr++) {
		//if (0)
		for(int tp=1;tp>-1;tp--) {
			for(int ty=0;ty<224/8;ty++) {
				for(int tx=0;tx<40;tx++) {
					if(!skip[tpr][tp][(ty*40)+tx]) {
						pvr_prim(&tile_hdr[tpr][tp][(ty*40)+tx], sizeof(pvr_poly_hdr_t));

						vert.flags = PVR_CMD_VERTEX;
						vert.x = (tx*16);
						vert.y = (ty*16)+16;
						vert.u = 1.0f;
						if(hf[tpr][tp][(ty*40)+tx]) {
							vert.u = 0.0f;
						}
						vert.v = 1.0f;
						if(tpr == 0) {
							if(tp == 1) {
								vert.z = 1.0f;
							}
							if(tp == 0) {
								vert.z = 1.0f;
							}
						}
						if(tpr == 1) {
							if(tp == 1) {
								vert.z = 3.0f;
							}
							if(tp == 0) {
								vert.z = 3.0f;
							}
						}

						pvr_prim(&vert, sizeof(vert));
						vert.y = (ty*16);
						vert.v = 0.0f;
						pvr_prim(&vert, sizeof(vert));

						vert.x = (tx*16) + 16;
						vert.y = (ty*16) + 16;
						vert.u = 0.0f;
						if(hf[tpr][tp][(ty*40)+tx]) {
							vert.u = 1.0f;
						}
						vert.v = 1.0f;
						pvr_prim(&vert, sizeof(vert));

						vert.flags = PVR_CMD_VERTEX_EOL;
						vert.y = (ty*16);
						vert.v = 0.0f;
						pvr_prim(&vert, sizeof(vert));
					}
				}
			}
		}
#if 1
		// now do sprites
		for (int si = 0; si < 80; si++) {
			if (sprite_used[tpr][si]) {
				uint32_t spr_ent_bot,spr_ent_top;
				uint32_t sh, sv, shf, svf;
				int32_t sx, sy;
				uint64_t spr_ent;
	#define SWAP_WORDS(x) __asm__ volatile ("swap.w %0, %0" : "+r" (x))
				spr_ent = vdp.sat[si];
				spr_ent_bot = (spr_ent >> 32);
				SWAP_WORDS(spr_ent_bot);
				spr_ent_top = (spr_ent & 0x00000000ffffffff);
				SWAP_WORDS(spr_ent_top);
				sy = ((spr_ent_top & 0x03FF0000) >> 16)-128;
				sh = ((spr_ent_top & 0x00000C00) >> 10)+1;
				sv = ((spr_ent_top & 0x00000300) >> 8)+1;
				svf = (spr_ent_bot & 0x10000000) >> 28;
				shf = (spr_ent_bot & 0x08000000) >> 27;
				sx = (spr_ent_bot & 0x000003FF)-128;		
				for(int h=0;h<sh;h++) {
				for(int v=0;v<sv;v++) {
					if(svf) {
						if(shf) {
							pvr_prim(&sprite_hdr[tpr][si][((sv-v)*sh)+(sh-h)], sizeof(pvr_poly_hdr_t));
						}
						else {
							pvr_prim(&sprite_hdr[tpr][si][((sv-v)*sh)+h], sizeof(pvr_poly_hdr_t));
						}
					}
					else {
						if(shf) {
							pvr_prim(&sprite_hdr[tpr][si][(v*sh)+(sh-h)], sizeof(pvr_poly_hdr_t));
						}
						else {
							pvr_prim(&sprite_hdr[tpr][si][(v*sh)+h], sizeof(pvr_poly_hdr_t));
						}
					}
					vert.flags = PVR_CMD_VERTEX;
					vert.x = (sx*2) + (h*16);
					vert.y = (sy*2) + (v*16) + (16);
					vert.u = 1.0f;
					if(shf) {
						vert.u = 0.0f;
					}
					vert.v = 1.0f;
					if(svf) {
						vert.v = 0.0f;
					}
					if(tpr == 0) {
						vert.z = 2.0f;
					}
					if(tpr == 1) {
						vert.z = 4.0f;
					}
					
					pvr_prim(&vert, sizeof(vert));
					vert.y = (sy*2) + (v*16);
					vert.v = 0.0f;
					if(svf) {
						vert.v = 1.0f;
					}
					pvr_prim(&vert, sizeof(vert));

					vert.x = (sx*2) + (h*16) + (16);
					vert.y = (sy*2) + (v*16) + (16);
					vert.u = 0.0f;
					if(shf) {
						vert.u = 1.0f;
					}
					vert.v = 1.0f;
					if(svf) {
						vert.v = 0.0f;
					}
					pvr_prim(&vert, sizeof(vert));

					vert.flags = PVR_CMD_VERTEX_EOL;
					vert.y = (sy*2) + (v*16);
					vert.v = 0.0f;
					if(svf) {
						vert.v = 1.0f;
					}
					pvr_prim(&vert, sizeof(vert));
					}
				}
			}
		}
#endif
	}

	pvr_list_finish();
	pvr_scene_finish();
}
