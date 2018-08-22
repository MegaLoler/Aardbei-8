#include <stdio.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include "v9958.h"

void initVideo() {
	if(SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}
}

void freeVideo() {
	SDL_Quit();
}

void initVDC(struct VDC *vdc) {
	vdc->window = SDL_CreateWindow("v9958", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0, SDL_WINDOW_SHOWN);
	if(vdc->window == NULL) {
		fprintf(stderr, "SDL window could not be created: %s\n", SDL_GetError());
		exit(1);
	}
	vdc->surface = SDL_GetWindowSurface(vdc->window);
}

void freeVDC(struct VDC *vdc) {
	SDL_DestroyWindow(vdc->window);
}

void draw(struct VDC *vdc) {
	SDL_UpdateWindowSurface(vdc->window);
}

void vdcWrite(struct VDC *vdc, uint8_t port, uint8_t data) {
	if(port == 0)
		0;
	else if(port == 1)
		0;
	else if(port == 2)
		0;
	else if(port == 3)
		0;
	else fprintf(stderr, "Writing to undefined VDC port 0x02%x\n", port);
}

uint8_t vdcRead(struct VDC *vdc, int port) {
	if(port == 0)
		0;
	else if(port == 1)
		0;
	else if(port == 2)
		0;
	else if(port == 3)
		0;
	else fprintf(stderr, "Reading from undefined VDC port 0x02%x\n", port);
	return 0;
}
