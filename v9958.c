#include <stdio.h>
#include <stdint.h>
#include <allegro5/allegro.h>
#include "v9958.h"

void initVDC(struct VDC *vdc) {
	vdc->display = al_create_display(100, 100);
	if(!vdc->display) {
		fprintf(stderr, "Allegro display could not be created\n");
		exit(1);
	}
}

void destroyVDC(struct VDC *vdc) {
	al_destroy_display(vdc->display);
}

void draw(struct VDC *vdc) {
	al_clear_to_color(al_map_rgb(255,0,0));
	al_flip_display();
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
