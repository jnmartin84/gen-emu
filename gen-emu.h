#ifndef _GEN_EMU_H_
#define _GEN_EMU_H_
#include <stdint.h>
#if 0
typedef signed char					sint8_t;
typedef signed short				sint16_t;
typedef signed int					sint32_t;
typedef signed long long			sint64_t;

typedef unsigned char				uint8_t;
typedef unsigned short				uint16_t;
typedef unsigned int				uint32_t;
typedef unsigned long long			uint64_t;

typedef volatile signed char		vsint8_t;
typedef volatile signed short		vsint16_t;
typedef volatile signed int			vsint32_t;
typedef volatile signed long long	vsint64_t;

typedef volatile unsigned char		vuint8_t;
typedef volatile unsigned short		vuint16_t;
typedef volatile unsigned int		vuint32_t;
typedef volatile unsigned long long	vuint64_t;
#endif
extern uint8_t debug;
extern uint8_t quit;
extern uint8_t dump;

/* From misc.s */
extern uint16_t endswaps(uint16_t);
extern uint32_t endswapl(uint32_t);

#define SWAPBYTES16(x)	\
	__asm__ volatile ("swap.b %0, %0" : "+r" (x))

#define SWAPBYTES32(x) \
	__asm__ volatile ("swap.b %0,%0" : "+r" (x)); \
	__asm__ volatile ("swap.w %0,%0" : "+r" (x)); \
	__asm__ volatile ("swap.b %0,%0" : "+r" (x))

#include <dc/minifont.h>

#undef printf
#define printf(...) {\
char str[256];\
 for(int py = 20; py < 40; py++) {\
        for(int px = 0; px < 640; px++) {\
            int ppc = 0;\
            vram_s[py * 640 + px] = ((ppc >> 3) << 12)\
                                  | ((ppc >> 2) << 5)\
                                  | ((ppc >> 3) << 0);\
        }\
    }\
sprintf(str,__VA_ARGS__);\
minifont_draw_str(vram_s + 640*20 + 20, 640, str);\
}


#endif /* _GEN_EMU_H_ */
