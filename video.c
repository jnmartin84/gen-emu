#include <kos.h>

#include "gen-emu.h"
#include "vdp.h"

/* PVR data from vdp.c */

pvr_vertex_t vert;

extern struct plane_pvr_tile *planes_head;
extern int planes_size;
extern struct plane_pvr_tile planes_pool[2240];
extern int sprites_size;
extern struct vdp_pvr_sprite sprites_pool[80];

void do_frame()
{
	pvr_vertex_t vert;

	vert.argb = 0xffffffff;
	vert.oargb = 0x00000000;
	vert.z = 1;

//	pvr_wait_ready();
	pvr_scene_begin(); 
	pvr_list_begin(PVR_LIST_TR_POLY);

	// iterate over used planes
	struct plane_pvr_tile *p;
	for (int pi=0;pi<planes_size;pi++) {
		p = &planes_pool[pi];
		int tx = p->x;
		int ty = p->y;
		
		pvr_prim(p->hdr, sizeof(pvr_poly_hdr_t));
		vert.flags = PVR_CMD_VERTEX;

		if(p->priority == 0) {
			if(p->plane == 1) {
				vert.z = 1.0f;
			}
			else {
				vert.z = 1.5f;
			}
		}
		else {
			if(p->plane == 1) {
				vert.z = 3.0f;
			}
			else {
				vert.z = 3.5f;
			}
		}
						
		vert.x = (tx*16);
		vert.y = (ty*16)+16;

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
		vert.y = (ty*16);
		if(!p->vf) {
			vert.v = 0.0f;
		}
		else {
			vert.v = 1.0;
		}
		pvr_prim(&vert, sizeof(vert));

		vert.x = (tx*16) + 16;
		vert.y = (ty*16) + 16;
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
		vert.y = (ty*16);
		if(!p->vf) {
			vert.v = 0.0f;
		}
		else {
			vert.v = 1.0;
		}
		pvr_prim(&vert, sizeof(vert));
	}

	// iterate over used sprites
	for(int si=0;si<sprites_size;si++) {
		struct vdp_pvr_sprite *s = &sprites_pool[si];
		int sx = s->x;
		int sy = s->y;
		int sh = s->h;
		int sv = s->v;
		int shf = s->hf;
		int svf = s->vf;
		int tpr = s->priority;

		for(int v=0;v<sv;v++) {
			for(int h=0;h<sh;h++) {
				if(svf) {
					if(shf) {
						pvr_prim(s->hdr[((sv-v-1)*sh)+(sh-h-1)], sizeof(pvr_poly_hdr_t));
					}
					else {
						pvr_prim(s->hdr[((sv-v-1)*sh)+h], sizeof(pvr_poly_hdr_t));
					}
				}
				else {
					if(shf) {
						pvr_prim(s->hdr[(v*sh)+(sh-h-1)], sizeof(pvr_poly_hdr_t));
					}
					else {
						pvr_prim(s->hdr[(v*sh)+h], sizeof(pvr_poly_hdr_t));
					}
				}
				vert.flags = PVR_CMD_VERTEX;
				if(tpr == 0) {
					vert.z = 2.0f + (0.01*si);
				}
				if(tpr == 1) {
					vert.z = 4.0f + (0.01*si);
				}

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

	pvr_list_finish();
	pvr_scene_finish();
}
