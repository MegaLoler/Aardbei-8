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
	// TODO: actually wait
}

void out(struct Peripherals *peripherals, uint16_t port, uint8_t data) {
	sync(4);
	// TODO
}

uint8_t in(struct Peripherals *peripherals, uint16_t port) {
	sync(4);
	// TODO
	return 0;
}

void writeByte(struct Memory *memory, uint16_t addr, uint8_t data) {
	sync(3);
	if(addr < RAM_BASE) // flash bank latch
		memory->flashBank = data;
	else *addressDecode(memory, addr) = data;
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

int parity(int n) {
	int parity = 0;
	while(n) {
		parity = !parity;
		n &= n - 1;
	}
	return parity;
}

// test to see if a carry occurred on some bit
int carry(int pre, int post, int bit) {
	int mask = 1 << bit;
	return (pre & mask) & ~(post & mask);
}

// test to see if a borrow occurred on some bit
int borrow(int pre, int post, int bit) {
	int mask = 1 << bit;
	return ~(pre & mask) & (post & mask);
}

#define C_FLAG  (1)
#define N_FLAG  (1 << 1)
#define PV_FLAG (1 << 2)
#define H_FLAG  (1 << 4)
#define Z_FLAG  (1 << 6)
#define S_FLAG  (1 << 7)

#define GET_FLAG(F)   ((cpu->regs.main.f & F) != 0)
#define SET_FLAG(F)   (cpu->regs.main.f |= F)
#define RESET_FLAG(F) (cpu->regs.main.f &= ~F)

#define GET_C  GET_FLAG(C_FLAG)
#define GET_N  GET_FLAG(N_FLAG)
#define GET_PV GET_FLAG(PV_FLAG)
#define GET_H  GET_FLAG(H_FLAG)
#define GET_Z  GET_FLAG(Z_FLAG)
#define GET_S  GET_FLAG(S_FLAG)

#define SET_C(N)  ((N) ? SET_FLAG(C_FLAG)  : RESET_FLAG(C_FLAG))
#define SET_N(N)  ((N) ? SET_FLAG(N_FLAG)  : RESET_FLAG(N_FLAG))
#define SET_PV(N) ((N) ? SET_FLAG(PV_FLAG) : RESET_FLAG(PV_FLAG))
#define SET_H(N)  ((N) ? SET_FLAG(H_FLAG)  : RESET_FLAG(H_FLAG))
#define SET_Z(N)  ((N) ? SET_FLAG(Z_FLAG)  : RESET_FLAG(Z_FLAG))
#define SET_S(N)  ((N) ? SET_FLAG(S_FLAG)  : RESET_FLAG(S_FLAG))

#define SET_CARRY(PRE, POST)       SET_C(carry(PRE, POST, 7))
#define SET_BORROW(PRE, POST)      SET_C(borrow(PRE, POST, 7))
#define SET_ADD                    SET_N(0)
#define SET_SUBTRACT               SET_N(1)
#define SET_PARITY(VALUE)          SET_PV(parity(VALUE))
#define SET_OVERFLOW(PRE, POST)    SET_PV(carry(PRE, POST, 7))
#define SET_UNDERFLOW(PRE, POST)   SET_PV(borrow(PRE, POST, 7))
#define SET_HALF_CARRY(PRE, POST)  SET_H(carry(PRE, POST, 3))
#define SET_HALF_BORROW(PRE, POST) SET_H(borrow(PRE, POST, 3))
#define SET_ZERO(VALUE)            SET_Z(!VALUE)
#define SET_SIGNED(VALUE)          SET_S(VALUE & (1 << 7))

// perform one instruction cycle
void step(struct CPUState *cpu, struct Memory *memory) {
	uint8_t opcode = fetchOpcode(cpu, memory);
#ifdef DEBUG
	printf("\t@addr 0x%x: got opcode 0x%x\n", cpu->regs.pc, opcode);
#endif

	int pre, post, c;
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
			pre = cpu->regs.main.b;
			post = ++cpu->regs.main.b;
			SET_ADD;
			SET_OVERFLOW(pre, post);
			SET_HALF_CARRY(pre, post);
			SET_ZERO(post);
			SET_SIGNED(post);
			break;
		case 0x05: // dec b
			pre = cpu->regs.main.b;
			post = --cpu->regs.main.b;
			SET_SUBTRACT;
			SET_UNDERFLOW(pre, post);
			SET_HALF_BORROW(pre, post);
			SET_ZERO(post);
			SET_SIGNED(post);
			break;
		case 0x06: // ld b,*
			cpu->regs.main.b = fetchByte(cpu, memory);
			break;
		case 0x07: // rlca
			c = (cpu->regs.main.a & (1 << 7)) >> 7;
			cpu->regs.main.a <<= 1;
			cpu->regs.main.a |= c;
			SET_C(c);
			SET_H(0);
			SET_N(0);
			break;
		case 0x08: // ex af,af'
			swapWord(&cpu->regs.main.af, &cpu->regs.alt.af);
			break;
		case 0x09: // add hl,bc
			sync(7);
			pre = cpu->regs.main.h | cpu->regs.main.b;
			cpu->regs.main.hl += cpu->regs.main.bc;
			post = cpu->regs.main.h | cpu->regs.main.b;
			SET_CARRY(pre, post);
			SET_HALF_CARRY(pre, post);
			SET_ADD;
			break;
		case 0x0a: // ld a,(bc)
			cpu->regs.main.a = readByte(memory, cpu->regs.main.bc);
			break;
		case 0x0b: // dec bc
			sync(2);
			cpu->regs.main.bc--;
			break;
		case 0x0c: // inc c
			pre = cpu->regs.main.c;
			post = ++cpu->regs.main.c;
			SET_ADD;
			SET_OVERFLOW(pre, post);
			SET_HALF_CARRY(pre, post);
			SET_ZERO(post);
			SET_SIGNED(post);
			break;
		case 0x0d: // dec c
			pre = cpu->regs.main.c;
			post = --cpu->regs.main.c;
			SET_SUBTRACT;
			SET_UNDERFLOW(pre, post);
			SET_HALF_BORROW(pre, post);
			SET_ZERO(post);
			SET_SIGNED(post);
			break;
		case 0x0e: // ld c,*
			cpu->regs.main.c = fetchByte(cpu, memory);
			break;
		case 0x0f: // rrca
			c = cpu->regs.main.a & 1;
			cpu->regs.main.a >>= 1;
			cpu->regs.main.a |= c << 7;
			SET_C(c);
			SET_H(0);
			SET_N(0);
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
		printf("\ncycle %i:\n", CYCLES);
#endif
		step(cpu, memory);
#ifdef DEBUG
		printf("\tFLAGS:\n\t\t%i%i %i %i%i%i\n\t\tSZ-H-PNC\n\t\t     V  \n",
				GET_S,
				GET_Z,
				GET_H,
				GET_PV,
				GET_N,
				GET_C);
#endif
	}
	
	return 0;
}
