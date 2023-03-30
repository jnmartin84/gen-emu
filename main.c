#include <kos.h>

#include "gen-emu.h"

#include "m68k.h"
#include "z80.h"
#include "vdp.h"
//#include "SN76489.h"

//KOS_INIT_FLAGS(INIT_DEFAULT | INIT_MALLOCSTATS | INIT_GDB);
KOS_INIT_FLAGS(INIT_DEFAULT | INIT_MALLOCSTATS);// | INIT_OCRAM);
#define FIELD_SKIP 2


//char *romname = "/cd/sonic_1.bin";
//char *romname = "/cd/pstar_2.bin";
char *romname = "/cd/sonic2.bin";

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

int main(int argc, char *argv[])
{
	int ch, fd;

//    dbgio_dev_select("fb");
	gen_init();
	
	void *test = malloc(64);
	printf("%08x\n", test);
//	while(1) {}

	rom_load(romname);

	gen_reset();
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
while(1) {
	printf("Emulated MHz: %f\n", emulated_MHz);
}	//(1); //!quit);
printf("\n");
printf("\n");
printf("\n");printf("\n");	
while(1) {
	//printf("it died\n");
}
#endif
	rom_free();

	return 0;
}
//uint32_t field_count = 0;
char str[256];
extern uint32_t alltime_maxtn;
extern uint8_t skip[2][2][40*28];
void run_one_field(void)
{
//	static int cnt;
	int line;
	const uint32_t field_skip = //!((field_count %1) ==0 || (field_count %2) == 0);//
	(field_count % FIELD_SKIP);// == 0;

	for(line = 0; line < 262 /*&& !quit*/; line++) {
		vdp_interrupt(line);

		m68k_execute(488);
//		if (z80_enabled())
	//		z80_execute(228);

	}
	#if 1
if (field_skip == 0)
	{
#if 0

for(line=0;line<224;line+=4) {
//			if (line < 224) {
			/* render scanline to vram*/
			vdp_render_scanline(line);
//			vdp_render_scanline(line+2);
//			vdp_render_scanline(line+4);
//			vdp_render_scanline(line+6);
//		}

}
#endif
	/* Render debug cram display. */
#if 0
	vdp_render_cram();
#endif
memset(skip,1,4*40*28);
		vdp_render_pvr_plane(0);
		vdp_render_pvr_sprites(0);
		vdp_render_pvr_plane(1);
		vdp_render_pvr_sprites(1);

	/* Submit whole screen to pvr. */
	do_frame();
}
	#endif
	/* Send sound to ASIC, call once per frame */
	//Sync76489(&PSG,SN76489_FLUSH);

	/* input processing */
	field_count++;
//	if ((cnt % 60) == 0)
//		printf("%d\n",cnt);
end_time = rtc_unix_secs();
#if 1
	total_cycles = (127856 * field_count/**FIELD_SKIP*/) ;
	double emulated_MHz = (total_cycles / 1048576.0) / (end_time - start_time);
sprintf(str, "emulated mhz: %f", emulated_MHz);
minifont_draw_str(vram_s + 640*20 + 20, 640, str);
		sprintf(str, "new all-time max tilenum %08x (%d)\n", alltime_maxtn, alltime_maxtn);
		minifont_draw_str(vram_s + 640*40 + 20, 640, str);
#endif
}
