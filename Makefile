aardbei: aardbei.c
	gcc -Wall -layemu -o aardbei aardbei.c

run: aardbei
	aoss ./aardbei
