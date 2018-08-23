aardbei: *.c
	gcc -Wall -layemu -lallegro -lallegro_audio -o aardbei *.c

run: aardbei
	./aardbei
