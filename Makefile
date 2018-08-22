aardbei: *.c
	gcc -Wall -layemu -o aardbei *.c

run: aardbei
	aoss ./aardbei
