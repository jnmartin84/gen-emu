
/* $Id$ */

#include <kos.h>
#include <malloc.h>
#include "gen-emu.h"
#include "cart.h"

#if 0
#include "md5.h"
#endif

/* Super Street Fighter 2 Game IDs. */
#define	SSF2_JP		"GM T-12043 -00"
#define SSF2_US		"GM T-12056 -00"


cart_t cart;

#define USE_SONIC2_H 1

#if USE_SONIC2_H
//#include "tforce4.h"
//#include "vector2.h"
#include "sonic2.h"
#endif

uint32_t rom_load(char *name)
{
 file_t fd; uint32_t len, i;
	uint8_t *rom = NULL;
#if 0	
	uint8 md5sum[16];
	MD5_CTX ctx;
#endif
	printf("Loading rom %s ... ", name);
//	memset((void*)&cart, 0, sizeof(cart_t));
#if !USE_SONIC2_H
	fd = fs_open(name, O_RDONLY);

	if(fd < 1) {
		printf("fd was %d\n", fd);
	}

	if (fd == 0) {
		printf("rom_load(): fs_open() failed.\n");
		goto error;
	}
	/* Get file length. */
	len = fs_seek(fd, 0, SEEK_END);
	printf("%d\n", len);

	fs_seek(fd, 0, SEEK_SET);
#else
	len = 1048576;
	rom = sonic2_rom;
#endif

#if !USE_SONIC2_H
	if (strstr(name, ".bin") != NULL) {
		/* Allocate ROM memory. */
		rom = malloc(len);
		if (rom == NULL) {
			while(1) { printf("rom_load(): malloc() failed.\n"); }
			goto error;
		}

		fs_read(fd, rom, len);
	}
#endif
#if 0
	else
	if (strstr(name, ".smd") != NULL) {
		uint8_t buf[512];

		/* Read file header and check for SMD signature. */
		fs_read(fd, buf, 512);
		if ((buf[8] == 0xaa) && (buf[9] == 0xbb)) {
			uint32_t blocks;
			uint8_t *blk, *tmp;

			len -= 512;
			if (len % 16384) {
				printf(".smd rom corrupt. Not a multiple of 16KB.\n");
				goto error;
			}
			blocks = len / 16384;

			/* Allocate ROM memory. */
			rom = malloc(len);
			if (rom == NULL) {
				printf("rom_load(): malloc() failed.\n");
				goto error;
			}

			/* Allocate temp block. */
			blk = malloc(16384);
			if (blk == NULL) {
				printf("rom_load(): malloc() failed.\n");
				goto error;
			}

			/* Load/decode encoded blocks. */
			tmp = rom;
			for(i = 0; i < blocks; i++) {
				uint32_t j;

				fs_read(fd, blk, 16384);
				for(j = 0; j < 8192; j++) {
					*tmp++ = blk[j+8192];
					*tmp++ = blk[j];
				}
			}

			/* Free temp block. */
			free(blk);
		} else {
			printf("rom_load(): Invalid .smd file loaded.\n");
			goto error;
		}
	} else {
		printf("ROM is not a recognized format.\n");
		goto error;
	}
	fs_close(fd);
#endif

	printf("Done.\n");

	/* Add ROM to cart. */
	cart.rom = rom;
	cart.rom_len = len;
#if 1
	/* Check if this cart has save ram. */
	if (rom[0x1b0] == 'R' && rom[0x1b1] == 'A') {
		uint32_t sr_start, sr_end, sr_len;
		uint8_t *sram = NULL;

		/* Extract saveram details from ROM header. */
		sr_start = ((uint32_t*)rom)[0x1b4/4];
		sr_end = ((uint32_t*)rom)[0x1b8/4];
		SWAPBYTES32(sr_start);
		SWAPBYTES32(sr_end);
		if (sr_start % 1)
			sr_start -= 1;
		if (!(sr_end % 1))
			sr_end += 1;
		sr_len = sr_end - sr_start + 1;
		if (sr_len) {
			sram = malloc(sr_len);
			if (sram == NULL) {
				printf("rom_load(): malloc() failed.\n");
				goto error;
			}
			bzero(sram, sr_len);
		}

		/* Save details in cart struct. */
		cart.sram = sram;
		cart.sram_len = sr_len;
		cart.sram_start = sr_start;
		cart.sram_end = sr_end;
		if (cart.sram_start < cart.rom_len)
			cart.sram_banked = 1;
	}

	/* Check the cart to see if it is Super Street Fighter 2. */
	if ((memcmp(&rom[0x180], SSF2_US, (sizeof(SSF2_US) - 1)) == 0) ||
		(memcmp(&rom[0x180], SSF2_JP, (sizeof(SSF2_JP) - 1)) == 0)) {
		/* Enable banking and init default bank values. */
		cart.banked = 1;
		cart.banks[0] = (uint32_t)(rom + 0x000000);
		cart.banks[1] = (uint32_t)(rom + 0x080000);
		cart.banks[2] = (uint32_t)(rom + 0x100000);
		cart.banks[3] = (uint32_t)(rom + 0x180000);
		cart.banks[4] = (uint32_t)(rom + 0x200000);
		cart.banks[5] = (uint32_t)(rom + 0x280000);
		cart.banks[6] = (uint32_t)(rom + 0x300000);
		cart.banks[7] = (uint32_t)(rom + 0x380000);
	} else {
		cart.banked = 0;
	}
#endif	
#if 0
	/* Calculate MD5 checksum of ROM. */
	MD5Init(&ctx);
	MD5Update(&ctx, rom, len);
	MD5Final(md5sum, &ctx);

	/* Display the checksum. */
	printf("ROM MD5 = ");
	for(i = 0; i < 16; i++)
		printf("%02x", md5sum[i]);
	printf("\n");
#endif

	printf("Cart details...\n");
	printf("ROM Length: 0x%x (%d) bytes\n", cart.rom_len, cart.rom_len);
#if 1
	if (cart.sram_len)
		printf("RAM Length: 0x%x (%d) bytes%s\n", cart.sram_len, cart.sram_len,
			   (cart.sram_banked ? ", banked" : ""));
	if (cart.banked)
		printf("Detected banked cartridge. Enabling banking.\n");
#endif
	return 1;

error:
#if !USE_SONIC2_H
	if (rom)
		free(rom);
	if (fd)
		fs_close(fd);
#endif
	return 0;
}

void rom_free(void)
{
	if (cart.rom) {
		free(cart.rom);
		cart.rom = NULL;
		cart.rom_len = 0;
	}
	if (cart.sram) {
		free(cart.sram);
		cart.sram = NULL;
		cart.sram_len = 0;
		cart.sram_start = 0;
		cart.sram_end = 0;
		cart.sram_banked = 0;
	}
}
