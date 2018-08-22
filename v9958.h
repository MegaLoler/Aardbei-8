#include <stdint.h>

#define VRAM_SIZE (1024*128)

struct VDC {
	uint8_t regs[32];
	uint8_t vram[VRAM_SIZE];
};

void initVideo();
void initVDC(struct VDC *);

void vdcWrite(struct VDC *, uint8_t, uint8_t);
uint8_t vdcRead(struct VDC *, int);
