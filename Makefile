# ind/Makefile

ECHO=echo
SED=sed
GZIP=gzip
GIT=git
CFLAGS=-Wall -W -g -pedantic -pipe
LDFLAGS=-Wall -W -g -pedantic
LIBS=-lutil

all: ind
doc: ind.1

PREFIX=/usr/local

install: all
	mkdir -p $(PREFIX)/bin
	mkdir -p $(PREFIX)/man/man1
	cp ind $(PREFIX)/bin/
	cp ind.1 $(PREFIX)/man/man1/

uninstall:
	rm -f $(PREFIX)/bin/ind $(PREFIX)/man/man1/
#
ind: ind.o
	$(CC) $(LDFLAGS) -o $@ $< $(LIBS)

ind.1: ind.yodl
	yodl2man -o $@ $<
#
ind-%.tar.gz:
	$(GIT) archive --format=tar \
            --prefix=$(shell $(ECHO) $@ | $(SED) 's/\.tar\.gz//')/ \
            ind-$(shell $(ECHO) $@ | $(SED) 's/.*-//' \
		| $(SED) 's/\.tar\.gz//') \
            | $(GZIP) -9 > $@

#
check: ind
	mkdir -p testsuite/logs
	runtest -a
#
clean: 
	rm -f ind *.o
