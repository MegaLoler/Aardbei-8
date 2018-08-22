#include <stdio.h>
#include <stdint.h>
#include "v9958.h"

void initVideo() {

}

void initVDC(struct VDC *vdc) {

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
