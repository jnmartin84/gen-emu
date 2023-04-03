
#include <kos.h>

#include "gen-emu.h"
#include "m68k.h"
#include "z80.h"
#include "vdp.h"
#include "input.h"
//#include "SN76489.h"
#include "cart.h"

pvr_init_params_t pvr_params = {
	{ PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_16,
	  PVR_BINSIZE_0, PVR_BINSIZE_0 }, 512 * 1024
};

//extern SN76489 PSG; 


void gen_init(void)
{
	pvr_init(&pvr_params);
//	pvr_mem_reset();
	vid_border_color(0, 0, 255);
//	pvr_set_bg_color(0, 0, 1.0f);

	vdp_init();
	ctlr_init();
//printf("bwah\n");
}

extern uint8_t m68k_ram[65536];
extern cart_t cart;
// we want cart.rom

void gen_reset(void)
{
	mmucontext_t * cxt;
	
	/* Setup a context */
//    cxt = mmu_context_create(0);
//    mmu_use_table(cxt);
//    mmu_switch_context(cxt);

#if 0
    /* Map the ROM to 0 */
    mmu_page_map(cxt, 0, ((uintptr_t)cart.rom) >> PAGESIZE_BITS, (1 * 1024 * 1024) >> PAGESIZE_BITS,
                 MMU_ALL_RDONLY, MMU_NO_CACHE, 0, 1);	

    /* Map the 68k RAM to 0 */
    mmu_page_map(cxt, 0xff0000, ((uintptr_t)m68k_ram) >> PAGESIZE_BITS, 0xffff >> PAGESIZE_BITS, //(1 * 1024 * 1024) >> PAGESIZE_BITS,
                 MMU_ALL_RDWR, MMU_NO_CACHE, 0, 1);
#endif
	
	m68k_pulse_reset();
//	z80_init();

//	Reset76489(&PSG, 0);
//	Sync76489(&PSG, SN76489_SYNC);

	ctlr_reset();
}
