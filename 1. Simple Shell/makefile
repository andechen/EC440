myshell: myshell.o myshell_parser.o
	gcc -Wall -Werror -g -o myshell myshell.o myshell_parser.o
myshell_parser.o: myshell_parser.c
	gcc -c myshell_parser.c myshell_parser.h
myshell.o: myshell.c
	gcc -c myshell.c

clean:
	rm myshell myshell.o myshell_parser.o