CFLAGS = -g -Wall -Werror -std=c99

cfilepirate_test: cfilepirate_test.o cfilepirate.o
	$(CC) -o $@ $+


