CFLAGS = -fPIC -g -Wall -Werror -std=c99

.PHONY: clean

cfilepirate.so: cfilepirate.o
	$(CC) -shared -o $@ $+

cfilepirate_test: cfilepirate_test.o cfilepirate.o
	$(CC) -g -o $@ $+

clean:
	rm -f *.o *.so cfilepirate_test

