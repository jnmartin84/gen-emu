#include <kos.h>

#include "gen-emu.h"
#include "vdp.h"

extern struct vdp_s vdp;

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
void do_frame()
{
//	if(1)
//return;
	pvr_vertex_t vert;
	int x, y, w, h;

	vert.argb = 0xffffffff;
	vert.oargb = 0x00000000;
	vert.z = 1;

	display_txr = disp_txr[display_ptr];
	display_ptr = (display_ptr ? 0 : 1);

//	pvr_wait_ready();
	//while(1) {printf("wait ready worked\n");}
	pvr_scene_begin(); 
//if(	
pvr_list_begin(PVR_LIST_TR_POLY);
//)  
//{
//	while(1) { printf("pvr_list_begin failed\n"); }
//}

	/* Main display */
#if 0
	x = 25; y = 25;
	w = 320; h = 240;
#else
	x = 0; y = 0;
	w = 640; h = 480;
#endif
#if 0
	pvr_prim(&disp_hdr[display_ptr], sizeof(pvr_poly_hdr_t));
	vert.flags = PVR_CMD_VERTEX;
	vert.x = x;
	vert.y = y + h;
	vert.u = 0.0f;
	vert.v = (240.0f/1.0f)/256.0f;
	pvr_prim(&vert, sizeof(vert));

	vert.y = y;
	vert.v = 0.0f;
	pvr_prim(&vert, sizeof(vert));

	vert.x = x + w;
	vert.y = y + h;
	vert.u = ((vdp.regs[12] & 0x01) ? 320.0f : 256.0f)/512.0f;
	vert.v = (240.0f/1.0f)/256.0f;
	pvr_prim(&vert, sizeof(vert));

	vert.flags = PVR_CMD_VERTEX_EOL;
	vert.y = y;
	vert.v = 0.0f;
	pvr_prim(&vert, sizeof(vert));
#endif


#if 1
for(int tpr=0;tpr<2;tpr++) {
for(int tp=1;tp>-1;tp--) {
//int tpr=1;int tp=1;
for(int ty=0;ty<224/8;ty++) {
	for(int tx=0;tx<40;tx++) {
		if(!skip[tpr][tp][(ty*40)+tx]) {
		pvr_prim(&tile_hdr[tpr][tp][(ty*40)+tx], sizeof(pvr_poly_hdr_t));

		vert.flags = PVR_CMD_VERTEX;
		vert.x = (tx*8);
		vert.y = (ty*8)+8;
		vert.u = 0.0f;
		vert.v = 1.0f;
//		vert.z = (0.0 - (float)tpr) + (tp == 1 ? 0.0 : -1.0);
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
		vert.z = 2.0f;
	}
	if(tp == 0) {
		vert.z = 2.0f;
	}
}

//		vert.z = tpr + (2-tp);
	
	pvr_prim(&vert, sizeof(vert));
		vert.y = (ty*8);
		vert.v = 0.0f;
		pvr_prim(&vert, sizeof(vert));

		vert.x = (tx*8) + 8;
		vert.y = (ty*8) + 8;
		vert.u = 1.0f;//((vdp.regs[12] & 0x01) ? 320.0f : 256.0f)/512.0f;
		vert.v = 1.0f;//240.0f/256.0f;
		pvr_prim(&vert, sizeof(vert));

		vert.flags = PVR_CMD_VERTEX_EOL;
		vert.y = (ty*8);
		vert.v = 0.0f;
		pvr_prim(&vert, sizeof(vert));
		}
	}
}
}
}
#endif

#if 0
	/* CRAM display */
	x = 550; y = 25;
	w = 64; h = 64;

	pvr_prim(&cram_hdr, sizeof(cram_hdr));
	vert.flags = PVR_CMD_VERTEX;
	vert.x = x;
	vert.y = y + h;
	vert.u = 0.0f;
	vert.v = 1.0f;
	pvr_prim(&vert, sizeof(vert));

	vert.y = y;
	vert.v = 0.0f;
	pvr_prim(&vert, sizeof(vert));

	vert.x = x + w;
	vert.y = y + h;
	vert.u = 1.0f;
	vert.v = 1.0f;
	pvr_prim(&vert, sizeof(vert));

	vert.flags = PVR_CMD_VERTEX_EOL;
	vert.y = y;
	vert.v = 0.0f;
	pvr_prim(&vert, sizeof(vert));
#endif

//if(
pvr_list_finish();
//) {
//	while(1) { printf("pvr_list_finish failed\n"); }
//}
//	if(
	pvr_scene_finish();
//	) {
//	while(1) { printf("pvr_scene_finish failed\n"); }
//}
}
