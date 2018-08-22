aardbei: *.c
	gcc -Wall -layemu -lSDL2 -o aardbei *.c

run: aardbei
	./aardbei
