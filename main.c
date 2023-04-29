#include <kos.h>

#include "gen-emu.h"

#include "m68k.h"
#include "z80.h"
#include "vdp.h"
//#include "SN76489.h"

KOS_INIT_FLAGS(INIT_DEFAULT | INIT_MALLOCSTATS);// | INIT_OCRAM);
#define FIELD_SKIP 2


char *romname = "/cd/vector2.bin";

char *scrcapname = "/pc/home/jkf/src/dc/gen-emu/screen.ppm";

uint8_t debug = 0;
uint8_t quit = 0;
uint8_t dump = 0;
//uint8_t pause = 0;
uint64_t field_count;
uint32_t rom_load(char *name);
void rom_free(void);
void run_one_field(void);
void gen_init(void);
void gen_reset(void);

//extern SN76489 PSG; 
extern struct vdp_s vdp;
uint64_t start_time;
uint64_t end_time;
uint64_t total_cycles;
extern struct plane_pvr_tile *planes_head;
extern uint8_t __attribute__ ((aligned(32))) tn_mod[2048];

int main(int argc, char *argv[])
{
	int ch, fd;

//    dbgio_dev_select("fb");
	gen_init();
	
	rom_load(romname);

	gen_reset();
//memset(tn_mod,1,2048);

	start_time = rtc_unix_secs();
	do {
		run_one_field();
#if 0
paused:
		ch = kbd_get_key();
		switch(ch) {
		case 0x000d:	/* Enter */
			pause = !pause;
			break;
		case 0x0020:	/* Space */
			quit = 1;
			break;
		case 0x0039:	/* 9 */
			dump = 1;
			break;
		case 0x4600:	/* Print Screen */
			vid_screen_shot(scrcapname);
			fd = fs_open("/pc/home/jkf/src/dc/gen-emu/vdp.bin", O_WRONLY | O_TRUNC);
			fs_write(fd, &vdp, sizeof(vdp));
			fs_close(fd);
			break;
		}
		if (pause)
			goto paused;
#endif
	} while (1); 

#if 0
	(127856 * (field_count/**FIELD_SKIP*/)) < (1048576*20*20));
	end_time = rtc_unix_secs();
	total_cycles = (127856 * field_count/**FIELD_SKIP*/) ;
	double emulated_MHz = (total_cycles / 1048576.0) / (end_time - start_time);
	
	printf("Elapsed seconds: %lld\n", (end_time - start_time));
	printf("Total cycles: %lld\n", total_cycles);
	printf("Emulated MHz: %f\n", emulated_MHz);
#endif

	rom_free();

	return 0;
}

char str[256];
//extern uint8_t tn_used[4096];
extern int planes_size;
void run_one_field(void)
{
	const uint32_t field_skip = (field_count & 7);//% FIELD_SKIP);
	int line;

	for(line = 0; line < 262 /*&& !quit*/; line++) {
		vdp_interrupt(line);
		m68k_execute(488);
		//if (z80_enabled())
		//	z80_execute(228);
	}

//	if (field_skip) 
	{
		vdp_setup();
		pvr_wait_ready();
		pvr_scene_begin(); 
		pvr_list_begin(PVR_LIST_PT_POLY);

		vdp_render_pvr_planes();
		vdp_render_pvr_sprites();

		pvr_list_finish();
		pvr_scene_finish();
	}

	/* Send sound to ASIC, call once per frame */
	//Sync76489(&PSG,SN76489_FLUSH);
#if 1
	/* input processing */
	field_count++;
	end_time = rtc_unix_secs();


	total_cycles = (127856 * field_count/**FIELD_SKIP*/) ;
	double emulated_MHz = (total_cycles / 1048576.0) / (end_time - start_time);
	sprintf(str, "emulated mhz: %f", emulated_MHz);
	minifont_draw_str(vram_s + 640*20 + 20, 640, str);
	
	//sprintf(str, "plane tiles %d\n", planes_size);
	//minifont_draw_str(vram_s + 640*40 + 20, 640, str);
#endif
}
