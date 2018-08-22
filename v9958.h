#include <stdint.h>
#include <SDL2/SDL.h>

#define VRAM_SIZE (1024*128)

struct VDC {
	uint8_t regs[47];
	uint8_t vram[VRAM_SIZE];
	SDL_Window *window;
	SDL_Surface *surface;
};

void initVDC(struct VDC *);
void freeVDC(struct VDC *);
void draw(struct VDC *);

void vdcWrite(struct VDC *, uint8_t, uint8_t);
uint8_t vdcRead(struct VDC *, int);
