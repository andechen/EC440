FLAGS = -Werror -Wall -g

all: main

main: fs.o disk.o
	gcc -o main $(FLAGS) $^ main.c

fs: fs.o disk.o
	gcc -o $@ $(FLAG)S $^

%.o: %.c
	gcc -o $@ $(FLAGS) -c $<

clean:
	rm -f fs *.o *~ main