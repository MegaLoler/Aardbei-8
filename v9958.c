#include <stdio.h>
#include <stdint.h>
#include <allegro5/allegro.h>
#include "v9958.h"

struct Dimensions {
	int x;
	int y;
};

void undefinedMode(int mode) {
	fprintf(stderr, "Undefined v9958 mode 0x%x\n", mode);
}

int getScreenEnable(struct VDC *vdc) {
	return vdc->regs[1] & 0b01000000;
}

int getModeFlags(struct VDC *vdc) {
	return
		((vdc->regs[1] & 0b00010000) && 0b00000001) |
		((vdc->regs[1] & 0b00001000) && 0b00000010) |
		((vdc->regs[0] & 0b00000010) && 0b00000100) |
		((vdc->regs[0] & 0b00000100) && 0b00001000) |
		((vdc->regs[0] & 0b00001000) && 0b00010000);
}

struct Dimensions getScreenDimensions(struct VDC *vdc) {
	switch(getModeFlags(vdc)) {
		case 0b00001: return (struct Dimensions){ 40*6, 24*8 };
		default: return (struct Dimensions){ 1, 1 };
	}	
}

void setScreenDimensions(struct VDC *vdc, struct Dimensions dimensions) {
	al_resize_display(vdc->display, dimensions.x, dimensions.y);
}

void updateScreenDimensions(struct VDC *vdc) {
	setScreenDimensions(vdc, getScreenDimensions(vdc));
}

void initVDC(struct VDC *vdc) {
	vdc->port1Sequence = 0;
	vdc->display = al_create_display(1, 1);
	if(!vdc->display) {
		fprintf(stderr, "Allegro display could not be created\n");
		exit(1);
	}
}

void destroyVDC(struct VDC *vdc) {
	al_destroy_display(vdc->display);
}

void drawTEXT1(struct VDC *vdc) {

}

void draw(struct VDC *vdc) {
	al_clear_to_color(al_map_rgb(0, 0, 0));
	if(getScreenEnable(vdc)) {
		int mode = getModeFlags(vdc);
		switch(mode) {
			case 0b00001:
				drawTEXT1(vdc);
				break;
			default:
				undefinedMode(mode);
				break;
		}	
	}
	al_flip_display();
}

void vdcWrite(struct VDC *vdc, uint8_t port, uint8_t data) {
	if(port == 0)
		0;
	else if(port == 1)
		if((vdc->port1Sequence = !vdc->port1Sequence)) vdc->dataLatch = data;
		else vdc->regs[data & 0x3f] = vdc->dataLatch;
	else if(port == 2)
		0;
	else if(port == 3)
		0;
	else fprintf(stderr, "Writing to undefined VDC port 0x02%x\n", port);
	// this is jsut... a semi convenient place for this.. prob should do better tho
	updateScreenDimensions(vdc);
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
