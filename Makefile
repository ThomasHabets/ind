# $Id$

all: ind
doc: ind.1

ind: ind.c
	$(CC) -Wall -w -g -o $@ $<

ind.1: ind.yodl
	yodl2man -o $@ $<

clean: 
	rm -f ind
