
#include <kos.h>

#include "gen-emu.h"
#include "m68k.h"
#include "z80.h"
#include "vdp.h"
#include "input.h"
//#include "SN76489.h"


pvr_init_params_t pvr_params = {
	{ PVR_BINSIZE_0, PVR_BINSIZE_0, PVR_BINSIZE_0,
	  PVR_BINSIZE_0, PVR_BINSIZE_16 }, 512 * 1024, 0, 0, 0
};

//extern SN76489 PSG; 


void gen_init(void)
{
	pvr_init(&pvr_params);
//	pvr_mem_reset();
//	pvr_set_bg_color(0, 0, 1.0f);

	vdp_init();
	ctlr_init();
//printf("bwah\n");
}

void gen_reset(void)
{
	m68k_pulse_reset();
//	z80_init();

//	Reset76489(&PSG, 0);
//	Sync76489(&PSG, SN76489_SYNC);

	ctlr_reset();
}
