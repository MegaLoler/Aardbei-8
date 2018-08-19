#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define DEBUG

/* CPU STATE AND REGISTERS */

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

/* MEMORY */

#define EEPROM_SIZE (1024*8)
#define RAM_SIZE    (1024*24)
#define BANK_SIZE   (1024*16)
#define FLASH_SIZE  (1024*512)

#define EEPROM_BASE (1024*56)
#define RAM_BASE    (1024*32)
#define BANK_BASE   (1024*16)

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

/* IO */

struct Peripherals {

};

/* CPU CONTROL */

int CYCLES;

// wait n T cycles
void sync(int cycles) {
	CYCLES += cycles;
}

void out(struct Peripherals *peripherals, uint16_t port, uint8_t data) {
	// TODO
	sync(4);
}

uint8_t in(struct Peripherals *peripherals, uint16_t port) {
	sync(4);
	// TODO
	return 0;
}

void writeByte(struct Memory *memory, uint16_t addr, uint8_t data) {
	if(addr < RAM_BASE) // flash bank latch
		memory->flashBank = data;
	else *addressDecode(memory, addr) = data;
	sync(3);
}

uint8_t readByte(struct Memory *memory, uint16_t addr) {
	sync(3);
	return *addressDecode(memory, addr);
}

uint16_t readWord(struct Memory *memory, uint16_t addr) {
	sync(6);
	return *addressDecode(memory, addr);
}

uint8_t fetchByte(struct CPUState *cpu, struct Memory *memory) {
	return readByte(memory, cpu->regs.pc++);
}

uint16_t fetchWord(struct CPUState *cpu, struct Memory *memory) {
	uint8_t low = fetchByte(cpu, memory);
	uint8_t high = fetchByte(cpu, memory);
	return low + (high << 8);
}

uint8_t fetchOpcode(struct CPUState *cpu, struct Memory *memory) {
	sync(1);
	return fetchByte(cpu, memory);
}

void swapByte(uint8_t *a, uint8_t *b) {
	uint8_t tmp = *a;
	*a = *b;
	*b = tmp;
}

void swapWord(uint16_t *a, uint16_t *b) {
	uint16_t tmp = *a;
	*a = *b;
	*b = tmp;
}

void swapRegs(struct CPUState *cpu) {
	struct RegisterSet tmp = cpu->regs.main;
	cpu->regs.main = cpu->regs.alt;
	cpu->regs.alt = tmp;
}

#define C_FLAG  (1)
#define N_FLAG  (1 << 1)
#define PV_FLAG (1 << 2)
#define H_FLAG  (1 << 4)
#define Z_FLAG  (1 << 6)
#define S_FLAG  (1 << 7)

#define SET_FLAG   (F) (cpu->regs.main.f |= F)
#define RESET_FLAG (F) (cpu->regs.main.f &= ~F)

#define SET_C  (N) (N ? SET_FLAG(C_FLAG)  : RESET_FLAG(C_FLAG))
#define SET_N  (N) (N ? SET_FLAG(N_FLAG)  : RESET_FLAG(N_FLAG))
#define SET_PV (N) (N ? SET_FLAG(PV_FLAG) : RESET_FLAG(PV_FLAG))
#define SET_H  (N) (N ? SET_FLAG(H_FLAG)  : RESET_FLAG(H_FLAG))
#define SET_Z  (N) (N ? SET_FLAG(Z_FLAG)  : RESET_FLAG(Z_FLAG))
#define SET_S  (N) (N ? SET_FLAG(S_FLAG)  : RESET_FLAG(S_FLAG))

// perform one instruction cycle
void step(struct CPUState *cpu, struct Memory *memory) {
	uint8_t opcode = fetchOpcode(cpu, memory);
#ifdef DEBUG
	printf("\t@addr 0x%x: got opcode 0x%x\n\n", cpu->regs.pc, opcode);
#endif

	// switch all the opcodes lol
	switch(opcode) {
		case 0x00: // nop
			break;
		case 0x01: // ld bc,**
			cpu->regs.main.bc = fetchWord(cpu, memory);
			break;
		case 0x02: // ld (bc),a
			writeByte(memory, cpu->regs.main.bc, cpu->regs.main.a);
			break;
		case 0x03: // inc bc
			sync(2);
			cpu->regs.main.bc++;
			break;
		case 0x04: // inc b
			// TODO: set flags
			cpu->regs.main.b++;
			break;
		case 0x05: // dec b
			// TODO: set flags
			cpu->regs.main.b--;
			break;
		case 0x06: // ld b,*
			cpu->regs.main.b = fetchByte(cpu, memory);
			break;
		case 0x07: // rlca
			// TODO: set flags
			cpu->regs.main.a
				= cpu->regs.main.a << 1
				| (cpu->regs.main.a & 1 << 7) >> 7;
			break;
		case 0x08: // ex af,af'
			swapWord(&cpu->regs.main.af, &cpu->regs.alt.af);
			break;
		case 0x09: // add hl,bc
			// TODO: set flags
			sync(7);
			cpu->regs.main.hl += cpu->regs.main.bc;
			break;
		case 0x0a: // ld a,(bc)
			cpu->regs.main.a = readByte(memory, cpu->regs.main.bc);
			break;
		case 0x0b: // dec bc
			sync(2);
			cpu->regs.main.bc--;
			break;
		case 0x0c: // inc c
			// TODO: set flags
			cpu->regs.main.c++;
			break;
		case 0x0d: // dec c
			// TODO: set flags
			cpu->regs.main.c--;
			break;
		case 0x0e: // ld c,*
			cpu->regs.main.c = fetchByte(cpu, memory);
			break;
		case 0x0f: // rrca
			// TODO: set flags
			cpu->regs.main.a
				= cpu->regs.main.a >> 1
				| (cpu->regs.main.a & 1) << 7;
			break;
	}
}

/* ENTRY POINT */

int main(int argc, char *argv[]) {
	// TODO: mmap flash and eeprom
	CYCLES = 0;
	struct CPUState *cpu = malloc(sizeof(struct CPUState));
	struct Memory *memory = malloc(sizeof(struct Memory));

	while(1) {
#ifdef DEBUG
		printf("cycle %i:\n", CYCLES);
#endif
		step(cpu, memory);
	}
	
	return 0;
}
