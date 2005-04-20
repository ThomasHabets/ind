# $Id$

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
ind: ind.c
	$(CC) -Wall -w -g -o $@ $<

ind.1: ind.yodl
	yodl2man -o $@ $<
#
clean: 
	rm -f ind
