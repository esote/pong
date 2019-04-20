pong: pong.c
	gcc -O3 -std=c99 -o pong.out pong.c -lncurses

debug: pong.c
	gcc -Wall -Wextra -O3 -g -pedantic -std=c99 -o pong.out pong.c -lncurses

clean:
	rm -f *.out
