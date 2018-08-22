aardbei: *.c
	gcc -Wall -layemu -lallegro -o aardbei *.c

run: aardbei
	./aardbei
