# $Id$

all: ind ind.1

ind: ind.c
	$(CC) -g -o $@ $<

ind.1: ind.yodl
	yodl2man -o $@ $<
