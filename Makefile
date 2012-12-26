CFLAGS = -g -Wall -Werror -std=c99

.PHONY: ALL
ALL: cfilepirate.so cfilepirate_test

cfilepirate_test: cfilepirate_test.o cfilepirate.o
	$(CC) -g -o $@ $+

cfilepirate.so: cfilepirate.o
	$(CC) -shared -o $@ $+
