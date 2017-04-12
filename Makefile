CC ?= gcc
PROG = capturebate

.PHONY: all clean install
all: capturebate

capturebate: main.c utarray.h uthash.h utstring.h
	$(CC) -O3 -o $(PROG) main.c -pthread -lcurl -lpcre2-8

clean:
	rm -rf $(PROG)

install:
	install -c $(PROG) /usr/local/bin/$(PROG)
