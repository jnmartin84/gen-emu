#include <kos.h>

#include "gen-emu.h"
#include "vdp.h"

/* PVR data from vdp.c */

extern struct vdp_s vdp;
pvr_vertex_t vert;

extern pvr_poly_hdr_t disp_hdr[2];//[2];
extern pvr_ptr_t disp_txr[2];//[2];
extern struct plane_pvr_tile *planes_head;
extern int planes_size;
extern struct plane_pvr_tile planes_pool[2240];
extern int sprites_size;
extern struct vdp_pvr_sprite sprites_pool[80];



void do_frame()
{
	pvr_vertex_t vert;
	int disp_x, disp_y, disp_w, disp_h;
	disp_x = 0; disp_y = 0;
	disp_w = 640; disp_h = 480;
	
	vert.argb = 0xffffffff;
	vert.oargb = 0x00000000;
	vert.z = 1;

	pvr_wait_ready();
	pvr_scene_begin();

	pvr_list_begin(PVR_LIST_TR_POLY);
#if 1
//	for(int tpr=0;tpr<2;tpr++) 
//	{
		pvr_prim(&disp_hdr[0], sizeof(pvr_poly_hdr_t));
		vert.flags = PVR_CMD_VERTEX;
	//	if(tpr == 0) {
			vert.z = 1.0f;
	//	}
	//	if(tpr == 1) {
	//		vert.z = 3.0f;
	//	}

		vert.x = disp_x;
		vert.y = disp_y + disp_h;
		vert.u = 0.0f;
		vert.v = 240.0f/256.0f;
		pvr_prim(&vert, sizeof(vert));

		vert.y = disp_y;
		vert.v = 0.0f;
		pvr_prim(&vert, sizeof(vert));

		vert.x = disp_x + disp_w;
		vert.y = disp_y + disp_h;
		vert.u = ((vdp.regs[12] & 0x01) ? 320.0f : 256.0f)/512.0f;
		vert.v = 240.0f/256.0f;
		pvr_prim(&vert, sizeof(vert));

		vert.flags = PVR_CMD_VERTEX_EOL;
		vert.y = disp_y;
		vert.v = 0.0f;
		pvr_prim(&vert, sizeof(vert));
//	}

		pvr_prim(&disp_hdr[1], sizeof(pvr_poly_hdr_t));
		vert.flags = PVR_CMD_VERTEX;
	//	if(tpr == 0) {
	//		vert.z = 1.0f;
	//	}
	//	if(tpr == 1) {
			vert.z = 3.0f;
	//	}

		vert.x = disp_x;
		vert.y = disp_y + disp_h;
		vert.u = 0.0f;
		vert.v = 240.0f/256.0f;
		pvr_prim(&vert, sizeof(vert));

		vert.y = disp_y;
		vert.v = 0.0f;
		pvr_prim(&vert, sizeof(vert));

		vert.x = disp_x + disp_w;
		vert.y = disp_y + disp_h;
		vert.u = ((vdp.regs[12] & 0x01) ? 320.0f : 256.0f)/512.0f;
		vert.v = 240.0f/256.0f;
		pvr_prim(&vert, sizeof(vert));

		vert.flags = PVR_CMD_VERTEX_EOL;
		vert.y = disp_y;
		vert.v = 0.0f;
		pvr_prim(&vert, sizeof(vert));

#endif

#if 0
	// iterate over used planes
	struct plane_pvr_tile *p;
	for (int pi=0;pi<planes_size;pi++) {
		p = &planes_pool[pi];
		int tx = p->x << 4;
		int ty = p->y << 4;
		
		pvr_prim(p->hdr, sizeof(pvr_poly_hdr_t));
		vert.flags = PVR_CMD_VERTEX;

		if(p->priority == 0) {
			if(p->plane == 1) {
				vert.z = 1.0f;
			}
			else {
				vert.z = 1.1f;
			}
		}
		else {
			if(p->plane == 1) {
				vert.z = 3.0f;
			}
			else {
				vert.z = 3.1f;
			}
		}
						
		vert.x = (tx);
		vert.y = (ty)+16;

		if(!p->hf) {
			vert.u = 1.0f;
		}
		else {
			vert.u = 0.0f;
		}
						
		if(!p->vf) {
			vert.v = 1.0f;
		}
		else {
			vert.v = 0.0;
		}

		pvr_prim(&vert, sizeof(vert));
		vert.y = (ty);
		if(!p->vf) {
			vert.v = 0.0f;
		}
		else {
			vert.v = 1.0;
		}
		pvr_prim(&vert, sizeof(vert));

		vert.x = (tx) + 16;
		vert.y = (ty) + 16;
		if(!p->hf) {
			vert.u = 0.0f;
		}
		else {
			vert.u = 1.0f;
		}
		if(!p->vf) {
			vert.v = 1.0f;
		}
		else {
			vert.v = 0.0;
		}
		pvr_prim(&vert, sizeof(vert));

		vert.flags = PVR_CMD_VERTEX_EOL;
		vert.y = (ty);
		if(!p->vf) {
			vert.v = 0.0f;
		}
		else {
			vert.v = 1.0;
		}
		pvr_prim(&vert, sizeof(vert));
	}
#endif

	// iterate over used sprites
	for(int si=0;si<sprites_size;si++) {
		struct vdp_pvr_sprite *s = &sprites_pool[si];
		int sx = s->x << 1;
		int sy = s->y << 1;
		int sh = s->h;
		int sv = s->v;
		int shf = s->hf;
		int svf = s->vf;
		int tpr = s->priority;

		for(int v=0;v<sv;v++) {
			for(int h=0;h<sh;h++) {
				pvr_poly_hdr_t *ph;
				if(svf) {
					if(shf) {
						ph = s->hdr[((sv-v-1)*sh)+(sh-h-1)];
					}
					else {
						//pvr_prim(
						ph = s->hdr[((sv-v-1)*sh)+h];
						//, sizeof(pvr_poly_hdr_t));
					}
				}
				else {
					if(shf) {
						//pvr_prim(
						ph = s->hdr[(v*sh)+(sh-h-1)];
						//, sizeof(pvr_poly_hdr_t));
					}
					else {
						//pvr_prim(
						ph = s->hdr[(v*sh)+h];
						//, sizeof(pvr_poly_hdr_t));
					}
				}
				pvr_prim(ph, sizeof(pvr_poly_hdr_t));

				vert.flags = PVR_CMD_VERTEX;
				if(tpr == 0) {
					vert.z = 2.0f + (0.01*(sprites_size-si));
				}
				if(tpr == 1) {
					vert.z = 4.0f + (0.01*(sprites_size-si));
				}

				vert.x = (sx) + (h<<4);
				vert.y = (sy) + (v<<4) + (16);
				vert.u = 1.0f;
				if(shf) {
					vert.u = 0.0f;
				}
				vert.v = 1.0f;
				if(svf) {
					vert.v = 0.0f;
				}
						
				pvr_prim(&vert, sizeof(vert));
				vert.y = (sy) + (v<<4);
				vert.v = 0.0f;
				if(svf) {
					vert.v = 1.0f;
				}
				pvr_prim(&vert, sizeof(vert));

				vert.x = (sx) + (h<<4) + (16);
				vert.y = (sy) + (v<<4) + (16);
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
				vert.y = (sy) + (v<<4);
				vert.v = 0.0f;
				if(svf) {
					vert.v = 1.0f;
				}
				pvr_prim(&vert, sizeof(vert));
			}
		}		
	}

	pvr_list_finish();
	pvr_scene_finish();
}
