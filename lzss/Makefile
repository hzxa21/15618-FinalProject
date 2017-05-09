############################################################################
# Makefile for lzss encode/decode library and sample program
############################################################################
CC = g++
LD = g++
DFLAGS = -g -O0 -ggdb
PFLAGS = -O3
CFLAGS = -I. $(PFLAGS)  -std=c++11 -Wall -Wextra 
LDFLAGS = -O3

# libraries
LIBS = -L. -llzss -loptlist

# Treat NT and non-NT windows the same
ifeq ($(OS),Windows_NT)
	OS = Windows
endif

ifeq ($(OS),Windows)
	ifeq ($(OSTYPE), cygwin)
		EXE = .exe
		DEL = rm
	else
		EXE = .exe
		DEL = del
	endif
else	#assume Linux/Unix
	EXE =
	DEL = rm -f
endif

# define the method to be used for searching for matches (choose one)
# brute force
# FMOBJ = brute.o

# linked list
# FMOBJ = list.o

# hash table
# FMOBJ = hash.o

# Knuth–Morris–Pratt search
# FMOBJ = kmp.o

# binary tree
FMOBJ = tree.o

LZOBJS = $(FMOBJ) lzss.o

all:		lzss$(EXE) liblzss.a liboptlist.a

lzss$(EXE):   main.o liblzss.a liboptlist.a
		$(LD) $< $(LIBS) $(LDFLAGS) -o $@

main.o:	main.cpp lzss.h optlist.h
	        $(CC) $(CFLAGS) $< -c -o $@

liblzss.a:	$(LZOBJS) bitfile.o
		ar crv liblzss.a $(LZOBJS) bitfile.o
		ranlib liblzss.a

lzss.o:	lzss.cpp lzlocal.h bitfile.h
		$(CC) $(CFLAGS) $< -c -o $@

brute.o:	brute.cpp lzlocal.h
		$(CC) $(CFLAGS) $< -c -o $@

list.o:		list.cpp lzlocal.h
		$(CC) $(CFLAGS) $< -c -o $@

hash.o:		hash.cpp lzlocal.h
		$(CC) $(CFLAGS) $< -c -o $@

kmp.o:		kmp.cpp lzlocal.h
		$(CC) $(CFLAGS) $< -c -o $@

tree.o:		tree.cpp lzlocal.h
		$(CC) $(CFLAGS) $< -c -o $@

bitfile.o:	bitfile.cpp bitfile.h
		$(CC) $(CFLAGS) $< -c -o $@

liboptlist.a:	optlist.o
		ar crv liboptlist.a optlist.o
		ranlib liboptlist.a

optlist.o:	optlist.cpp optlist.h
		$(CC) $(CFLAGS) $< -c -o $@

clean:
		$(DEL) *.o
		$(DEL) *.a
		$(DEL) lzss$(EXE)
