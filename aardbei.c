#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// cpu state

struct RegisterSet {
	union {
		uint16_t af;
		struct {
			uint8_t a;
			uint8_t f;
		};
	};
	union {
		uint16_t bc;
		struct {
			uint8_t b;
			uint8_t c;
		};
	};
	union {
		uint16_t de;
		struct {
			uint8_t d;
			uint8_t e;
		};
	};
	union {
		uint16_t hl;
		struct {
			uint8_t h;
			uint8_t l;
		};
	};
};

struct Registers {
	struct RegisterSet main;
	struct RegisterSet alt;
	uint8_t i;
	uint8_t r;
	uint16_t ix;
	uint16_t iy;
	uint16_t sp;
	uint16_t pc;
};

struct CPUState {
	struct Registers regs;
};

// memory

#define EEPROM_SIZE (1024*8)
#define RAM_SIZE (1024*24)
#define BANK_SIZE (1024*16)
#define FLASH_SIZE (1024*512)

#define EEPROM_BASE (1024*56)
#define RAM_BASE (1024*32)
#define BANK_BASE (1024*16)

struct Memory {
	uint8_t eeprom[EEPROM_SIZE]; // mmap this
	uint8_t ram[RAM_SIZE];
	uint8_t flash[FLASH_SIZE]; // mmap this
	uint8_t flashBank;
};

uint8_t *addressDecode(struct Memory *memory, uint16_t addr) {
	if(addr < BANK_BASE) // zero-page
		return &memory->flash[addr];
	else if(addr < RAM_BASE) // selected bank of flash
		return &memory->flash[addr - BANK_SIZE*memory->flashBank];
	else if(addr < EEPROM_BASE) // ram
		return &memory->ram[addr - RAM_BASE];
	else // eeprom
		return &memory->eeprom[addr - EEPROM_BASE];
}

void writeByte(struct Memory *memory, uint16_t addr, uint8_t data) {
	if(addr < RAM_BASE) // flash bank latch
		memory->flashBank = data;
	else *addressDecode(memory, addr) = data;
}

uint8_t readByte(struct Memory *memory, uint16_t addr) {
	return *addressDecode(memory, addr);
}

uint16_t readWord(struct Memory *memory, uint16_t addr) {
	return *addressDecode(memory, addr);
}

// cpu control

void tick(struct CPUState *state, struct Memory *memory) {

}

// entry point

int main(int argc, char *argv[]) {
	// TODO: mmap flash and eeprom
	struct CPUState *cpu = malloc(sizeof(struct CPUState));
	struct Memory *memory = malloc(sizeof(struct Memory));

	// TODO: replace this with alarm interrupt
	while(1) {
		tick(cpu, memory);
	}
	
	return 0;
}
