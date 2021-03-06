// TODO:
// 	complete opcodes
//	v9958 emulation
//	full uart emulation
//	make the audio timing fixed.........
// 	mmap flash and eeprom
// 	cli args parsing
// 	cleaner debug output
// 	refresh register
// 	gdb integration
// 	factor out the code into multiple files
// 	make documentation n stuff

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <allegro5/allegro.h>
#include "allegro5/allegro_audio.h"
#include <ayemu.h>
#include "v9958.h"

//#define DEBUG
//#define DEBUG_IO
//#define DEBUG_AY
#define STRICT

#define CPU_RATE 3579545
#define AUDIO_RATE 44100
#define AUDIO_DEPTH ALLEGRO_AUDIO_DEPTH_INT16
#define AUDIO_CHANNELS ALLEGRO_CHANNEL_CONF_2
#define AUDIO_BUFFER_FRAGS 2
#define SAMPLES_PER_BUFFER 1024
#define BUFFER_LENGTH (SAMPLES_PER_BUFFER * 2 * 2)



/* UTIL */

long int nanos() {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
	return ts.tv_nsec + ts.tv_sec * 1000000000;
}




/* CPU STATE AND REGISTERS */

struct RegisterSet {
	union {
		uint16_t af;
		struct {
			uint8_t f;
			uint8_t a;
		};
	};
	union {
		uint16_t bc;
		struct {
			uint8_t c;
			uint8_t b;
		};
	};
	union {
		uint16_t de;
		struct {
			uint8_t e;
			uint8_t d;
		};
	};
	union {
		uint16_t hl;
		struct {
			uint8_t l;
			uint8_t h;
		};
	};
};

struct Registers {
	struct RegisterSet main;
	struct RegisterSet alt;
	uint8_t i;
	uint8_t r;
	union {
		uint16_t ix;
		struct {
			uint8_t ixl;
			uint8_t ixh;
		};
	};
	union {
		uint16_t iy;
		struct {
			uint8_t iyl;
			uint8_t iyh;
		};
	};
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
	uint8_t eeprom[EEPROM_SIZE]; // mmap this?
	uint8_t ram[RAM_SIZE];
	uint8_t flash[FLASH_SIZE]; // mmap this?
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

struct AY {
	ayemu_ay_t ay;
	uint8_t regs[14];
	uint8_t latch;
	ALLEGRO_AUDIO_STREAM *stream;
	ALLEGRO_EVENT_QUEUE *queue;
};

struct Peripherals {
	struct AY ay1;
	struct AY ay2;
	struct VDC vdc;
};

void printAYRegisters(ayemu_ay_t *a) {
	printf("\nAY REGS: A=%04d B=%04d C=%04d N=%02d R7=[%d%d%d%d%d%d] "
			"\n   VOLS: A=%04d B=%04d C=%04d ENVFREQ=%d STYLE %d",
			a->regs.tone_a, a->regs.tone_b, a->regs.tone_c, a->regs.noise,
			a->regs.R7_tone_a, a->regs.R7_tone_b, a->regs.R7_tone_c,
			a->regs.R7_noise_a, a->regs.R7_noise_b, a->regs.R7_noise_c,
			a->regs.vol_a, a->regs.vol_b, a->regs.vol_c,
			a->regs.env_freq, a->regs.env_style);
}

void play(struct AY *ay) {
	ALLEGRO_EVENT event;
	while(al_get_next_event(ay->queue, &event)) {
		if(event.type == ALLEGRO_EVENT_AUDIO_STREAM_FRAGMENT) {
			uint8_t *buffer = al_get_audio_stream_fragment(ay->stream);
			if(!buffer) continue;
			ayemu_ay_t *a = &ay->ay;
			ayemu_set_regs(a, ay->regs);
			ayemu_gen_sound(a, buffer, BUFFER_LENGTH);
			if(!al_set_audio_stream_fragment(ay->stream, buffer)) {
				fprintf(stderr, "Error setting stream fragment buffer\n");
				exit(1);
			}
#ifdef DEBUG_AY
			printAYRegisters(a);
#endif
		}
	}
}

void initAY(struct AY* ay) {
	ayemu_init(&ay->ay);
	ay->stream = al_create_audio_stream(
			AUDIO_BUFFER_FRAGS,
			SAMPLES_PER_BUFFER,
			AUDIO_RATE,
			AUDIO_DEPTH,
			AUDIO_CHANNELS);
	if(!ay->stream)
		fprintf(stderr, "Could not create audio stream\n");
	else if (!al_attach_audio_stream_to_mixer(ay->stream, al_get_default_mixer()))
		fprintf(stderr, "Could not attach audio stream to mixer\n");
	else {
		ay->queue = al_create_event_queue();
		al_register_event_source(ay->queue, al_get_audio_stream_event_source(ay->stream));
		return;
	}
	exit(1);
}

void destroyAY(struct AY* ay) {
	al_drain_audio_stream(ay->stream);
	al_destroy_event_queue(ay->queue);
	al_destroy_audio_stream(ay->stream);
}



/* SYSTEM */

struct System {
	struct CPUState cpu;
	struct Memory memory;
	struct Peripherals peripherals;
	int cycles;
};

// return the amount of emulated nanoseconds passed since the system has started
long int systemNanos(struct System *system) {
	return (long)1000000000 * system->cycles / CPU_RATE;
}

struct System *newSystem() {
	struct System *system = malloc(sizeof(struct System));
	system->cycles = 0;
	initAY(&system->peripherals.ay1);
	initAY(&system->peripherals.ay2);
	initVDC(&system->peripherals.vdc);
	return system;
}

void destroySystem(struct System *system) {
	destroyAY(&system->peripherals.ay1);
	destroyAY(&system->peripherals.ay2);
	destroyVDC(&system->peripherals.vdc);
	free(system);
}



/* CPU CONTROL */

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

// print the state of the cpu for debug or whatever
void printState(struct CPUState *cpu) {
	printf("FLAGS: %i%i %i %i%i%i\n       SZ-H-PNC\n",
			GET_S,
			GET_Z,
			GET_H,
			GET_PV,
			GET_N,
			GET_C);
	printf("REGS: AF(0x%04x) BC(0x%04x) DE(0x%04x) HL(0x%04x)\n",
			cpu->regs.main.af,
			cpu->regs.main.bc,
			cpu->regs.main.de,
			cpu->regs.main.hl);
	printf("      IX(0x%04x) IY(0x%04x) SP(0x%04x) PC(0x%04x)\n",
			cpu->regs.ix,
			cpu->regs.iy,
			cpu->regs.sp,
			cpu->regs.pc);
	printf("       I(0x%02x)    R(0x%02x)\n",
			cpu->regs.i,
			cpu->regs.r);
}

// log n T cycles
void cycles(int cycles, struct System *system) {
	system->cycles += cycles;
}

void out(struct System *system, uint16_t port, uint8_t data) {
	struct Peripherals *peripherals = &system->peripherals;
	cycles(4, system);
#ifdef DEBUG_IO
	printf("\n[OUT] @0x%04x = 0x%02x", port, data);
#endif
	// ay 1
	if(port == 0)
		peripherals->ay1.latch = data;
	else if(port == 1)
		peripherals->ay1.regs[peripherals->ay1.latch] = data;
	// ay 2
	else if(port == 2)
		peripherals->ay2.latch = data;
	else if(port == 3)
		peripherals->ay2.regs[peripherals->ay2.latch] = data;
	// vdc
	else if(port >= 4 && port < 8)
		vdcWrite(&peripherals->vdc, port-4, data);
	// uart
	else if(port == 8)
		putchar(data);
	else fprintf(stderr, "Writing to undefined I/O port 0x%04x\n", port);
}

uint8_t in(struct System *system, uint16_t port) {
	struct Peripherals *peripherals = &system->peripherals;
	cycles(4, system);
#ifdef DEBUG_IO
	printf("\n[IN] @0x%04x", port);
#endif
	// ay 1
	if(port == 0)
		fprintf(stderr, "Reading from read-only I/O port 0x%04x\n", port);
	else if(port == 1)
		return peripherals->ay1.regs[peripherals->ay1.latch];
	// ay 2
	else if(port == 2)
		fprintf(stderr, "Reading from read-only I/O port 0x%04x\n", port);
	else if(port == 3)
		return peripherals->ay2.regs[peripherals->ay2.latch];
	// vdc
	else if(port >= 4 && port < 8)
		return vdcRead(&peripherals->vdc, port-4);
	// uart
	else if(port == 8)
		fprintf(stderr, "Reading from read-only I/O port 0x%04x\n", port);
	else fprintf(stderr, "Reading from undefined I/O port 0x%04x\n", port);
	return 0;
}

void writeByte(struct System *system, uint16_t addr, uint8_t data) {
	cycles(3, system);
	if(addr < RAM_BASE) // flash bank latch
		system->memory.flashBank = data;
	else *addressDecode(&system->memory, addr) = data;
}

uint8_t readByte(struct System *system, uint16_t addr) {
	cycles(3, system);
	return *addressDecode(&system->memory, addr);
}

uint16_t readWord(struct System *system, uint16_t addr) {
	cycles(6, system);
	return *addressDecode(&system->memory, addr);
}

uint8_t fetchByte(struct System *system) {
	return readByte(system, system->cpu.regs.pc++);
}

uint16_t fetchWord(struct System *system) {
	uint8_t low = fetchByte(system);
	uint8_t high = fetchByte(system);
	return low + (high << 8);
}

uint8_t fetchOpcode(struct System *system) {
	cycles(1, system);
	return fetchByte(system);
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

void unknownOpcode(int opcode) {
	fprintf(stderr, "[WARNING] Unknown opcode: 0x%x\n", opcode);
#ifdef STRICT
	exit(1);
#endif
}

// perform one instruction cycle
void step(struct System *system) {
	struct CPUState *cpu = &system->cpu;
	uint8_t opcode = fetchOpcode(system);
#ifdef DEBUG
	printf("\nT cycle %i:\n", system->cycles);
	printf("@addr 0x%04x: got opcode 0x%02x", cpu->regs.pc-1, opcode);
#endif

	int pre, post, c, extendedByte, arg;
	// switch all the opcodes lol
	switch(opcode) {
		case 0x00: // nop
			break;
		case 0x01: // ld bc,**
			cpu->regs.main.bc = fetchWord(system);
			break;
		case 0x02: // ld (bc),a
			writeByte(system, cpu->regs.main.bc, cpu->regs.main.a);
			break;
		case 0x03: // inc bc
			cycles(2, system);
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
			cpu->regs.main.b = fetchByte(system);
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
			cycles(7, system);
			pre = cpu->regs.main.h | cpu->regs.main.b;
			cpu->regs.main.hl += cpu->regs.main.bc;
			post = cpu->regs.main.h;
			SET_CARRY(pre, post);
			SET_HALF_CARRY(pre, post);
			SET_ADD;
			break;
		case 0x0a: // ld a,(bc)
			cpu->regs.main.a = readByte(system, cpu->regs.main.bc);
			break;
		case 0x0b: // dec bc
			cycles(2, system);
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
			cpu->regs.main.c = fetchByte(system);
			break;
		case 0x0f: // rrca
			c = cpu->regs.main.a & 1;
			cpu->regs.main.a >>= 1;
			cpu->regs.main.a |= c << 7;
			SET_C(c);
			SET_H(0);
			SET_N(0);
			break;
		case 0x11: // ld de,**
			cpu->regs.main.de = fetchWord(system);
			break;
		case 0x17: // rla
			c = (cpu->regs.main.a & (1 << 7)) >> 7;
			cpu->regs.main.a <<= 1;
			cpu->regs.main.a |= GET_C;
			SET_C(c);
			SET_H(0);
			SET_N(0);
			break;
		case 0x1f: // rra
			c = cpu->regs.main.a & 1;
			cpu->regs.main.a >>= 1;
			cpu->regs.main.a |= GET_C << 7;
			SET_C(c);
			SET_H(0);
			SET_N(0);
			break;
		case 0x3c: // inc a
			pre = cpu->regs.main.a;
			post = ++cpu->regs.main.a;
			SET_ADD;
			SET_OVERFLOW(pre, post);
			SET_HALF_CARRY(pre, post);
			SET_ZERO(post);
			SET_SIGNED(post);
			break;
		case 0x3d: // dec a
			pre = cpu->regs.main.a;
			post = --cpu->regs.main.a;
			SET_SUBTRACT;
			SET_UNDERFLOW(pre, post);
			SET_HALF_BORROW(pre, post);
			SET_ZERO(post);
			SET_SIGNED(post);
			break;
		case 0x3e: // ld a,*
			cpu->regs.main.a = fetchByte(system);
			break;
		case 0x47: // ld b,a
			cpu->regs.main.b = cpu->regs.main.a;
			break;
		case 0x4f: // ld c,a
			cpu->regs.main.c = cpu->regs.main.a;
			break;
		case 0x60: // ld h,b
			cpu->regs.main.h = cpu->regs.main.b;
			break;
		case 0x67: // ld h,a
			cpu->regs.main.h = cpu->regs.main.a;
			break;
		case 0x69: // ld l,c
			cpu->regs.main.l = cpu->regs.main.c;
			break;
		case 0x6f: // ld l,a
			cpu->regs.main.l = cpu->regs.main.a;
			break;
		case 0x78: // ld a,b
			cpu->regs.main.a = cpu->regs.main.b;
			break;
		case 0x79: // ld a,c
			cpu->regs.main.a = cpu->regs.main.c;
			break;
		case 0x7a: // ld a,d
			cpu->regs.main.a = cpu->regs.main.d;
			break;
		case 0x7b: // ld a,e
			cpu->regs.main.a = cpu->regs.main.e;
			break;
		case 0xb7: // or a
			pre = cpu->regs.main.a;
			post = pre | pre;
			cpu->regs.main.a = post;
			SET_C(0);
			SET_H(0);
			SET_N(0);
			SET_PARITY(post);
			SET_ZERO(post);
			SET_SIGNED(post);
			break;
		case 0xc2: // jp nz,**
			arg = fetchWord(system);
			if(!GET_Z) cpu->regs.pc = arg;
			break;
		case 0xc3: // jp **
			cpu->regs.pc = fetchWord(system);
			break;
		case 0xc6: // add a,*
			pre = cpu->regs.main.a;
			cpu->regs.main.a += fetchByte(system);
			post = cpu->regs.main.a;
			SET_CARRY(pre, post);
			SET_HALF_CARRY(pre, post);
			SET_ADD;
			SET_OVERFLOW(pre, post);
			SET_ZERO(post);
			SET_SIGNED(post);
			break;
		case 0xca: // jp z,**
			arg = fetchWord(system);
			if(GET_Z) cpu->regs.pc = arg;
			break;
		case 0xcb: // bits
			extendedByte = fetchOpcode(system);
#ifdef DEBUG
			printf("%02x", extendedByte);
#endif
			// switch the extended byte lol
			switch(extendedByte) {
				case 0x1a: // rr d
					c = cpu->regs.main.d & 1;
					cpu->regs.main.d >>= 1;
					cpu->regs.main.d |= GET_C << 7;
					post = cpu->regs.main.d;
					SET_C(c);
					SET_H(0);
					SET_N(0);
					SET_PARITY(post);
					SET_ZERO(post);
					SET_SIGNED(post);
					break;
				case 0x1b: // rr e
					c = cpu->regs.main.e & 1;
					cpu->regs.main.e >>= 1;
					cpu->regs.main.e |= GET_C << 7;
					post = cpu->regs.main.e;
					SET_C(c);
					SET_H(0);
					SET_N(0);
					SET_PARITY(post);
					SET_ZERO(post);
					SET_SIGNED(post);
					break;
				default: unknownOpcode((opcode << 8) | extendedByte);
			}
			break;
		case 0xd3: // out (*),a
			out(system, fetchByte(system), cpu->regs.main.a);	
			break;
		case 0xdd: // ix
			extendedByte = fetchOpcode(system);
#ifdef DEBUG
			printf("%02x", extendedByte);
#endif
			// switch the extended byte lol
			switch(extendedByte) {
				case 0x21: // ld ix,**
					cpu->regs.ix = fetchWord(system);
					break;
				case 0x23: // inc ix
					cycles(2, system);
					cpu->regs.ix++;
					break;
				case 0x7c: // ld a,ixh
					cpu->regs.main.a = cpu->regs.ixh;
					break;
				case 0x7d: // ld a,ixl
					cpu->regs.main.a = cpu->regs.ixl;
					break;
				case 0x7e: // ld a,(ix+*)
					cycles(5, system);
					cpu->regs.main.a = readByte(system,
							fetchByte(system)
							+ cpu->regs.ix);
					break;
				default: unknownOpcode((opcode << 8) | extendedByte);
			}
			break;
		case 0xe6: // and *
			pre = cpu->regs.main.a;
			post = pre & fetchByte(system);
			cpu->regs.main.a = post;
			SET_C(0);
			SET_H(1);
			SET_N(0);
			SET_PARITY(post);
			SET_ZERO(post);
			SET_SIGNED(post);
			break;
		case 0xed: // extd
			extendedByte = fetchOpcode(system);
#ifdef DEBUG
			printf("%02x", extendedByte);
#endif
			// switch the extended byte lol
			switch(extendedByte) {
				case 0x52: // sbc hl,de
					cycles(7, system);
					pre = cpu->regs.main.h;
					cpu->regs.main.hl -= cpu->regs.main.de
						+ GET_C;
					post = cpu->regs.main.h;
					SET_BORROW(pre, post);
					SET_HALF_BORROW(pre, post);
					SET_SUBTRACT;
					SET_UNDERFLOW(pre, post);
					SET_ZERO(post);
					SET_SIGNED(post);
					break;
				default: unknownOpcode((opcode << 8) | extendedByte);
			}
			break;
		case 0xf3: // di
			// TODO: actually di
			// right now this is just my debug opcode lol
			printf("-----------\n\n");
			break;
		case 0xfb: // ei
			// TODO: actually ei
			// right now this is just my debug opcode lol
			printState(cpu);
			break;
		case 0xfe: // cp *
			pre = cpu->regs.main.a;
			post = pre - fetchByte(system);
			SET_BORROW(pre, post);
			SET_HALF_BORROW(pre, post);
			SET_SUBTRACT;
			SET_UNDERFLOW(pre, post);
			SET_ZERO(post);
			SET_SIGNED(post);
			break;
		default: unknownOpcode(opcode);
	}
#ifdef DEBUG
	printf("\n");
	printState(cpu);
#endif
}



/* ENTRY POINT */

struct System *mainSystem;

void initAllegro() {
	if(!al_init())
		fprintf(stderr, "Could not initialize Allegro\n");
	else if(!al_install_audio())
		fprintf(stderr, "Could not initialize Allegro audio\n");
	else {
		al_reserve_samples(0);
		return;
	}
	exit(1);
}

// load a file into memory
void load(const char filename[], int size, uint8_t *destination) {
	FILE *fp = fopen(filename, "r");
	fread(destination, sizeof(uint8_t), size, fp);
	fclose(fp);
}

void init() {
	initAllegro();
	mainSystem = newSystem();

	// load the program and save data
	// TODO: parse args to load different files than defaults
	// TODO: mmap instead? ? ? 
	load("test/music.rom", FLASH_SIZE, mainSystem->memory.flash);
}

void quit() {
	destroySystem(mainSystem);
	al_uninstall_audio();
}

void systemLoop() {
	long int startNanos = nanos();
	while(1) {
		// cpu
		// gotta catch it up to realtime
		while(systemNanos(mainSystem) < nanos()-startNanos)
			step(mainSystem);

		// ays
		play(&mainSystem->peripherals.ay1);
		play(&mainSystem->peripherals.ay2);

		// uart
		fflush(stdout);
		
		// vdc
		draw(&mainSystem->peripherals.vdc);
	}
}

int main(int argc, char *argv[]) {
	init();
	systemLoop();
	quit();
	return 0;
}
