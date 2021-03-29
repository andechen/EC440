GCC = gcc -g -Werror -Wall

all: tls

tls: tls.o tls.h
	$(GCC) -c tls.c -o tls.o

clean:
	rm tls.o