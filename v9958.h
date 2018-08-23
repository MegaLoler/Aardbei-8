#include <stdint.h>
#include <allegro5/allegro.h>

#define VRAM_SIZE (1024*128)

struct VDC {
	uint8_t regs[47];
	uint8_t vram[VRAM_SIZE];
	ALLEGRO_DISPLAY *display;
};

void initVDC(struct VDC *);
void destroyVDC(struct VDC *);
void draw(struct VDC *);

void vdcWrite(struct VDC *, uint8_t, uint8_t);
uint8_t vdcRead(struct VDC *, int);
