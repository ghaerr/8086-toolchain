# Examples Makefile for host building of ELKS example programs

ifndef TOPDIR
$(error ELKS TOPDIR is not defined)
endif

BIN=../host-bin/
CPP=$(BIN)cpp86
CC=$(BIN)c86
AS=$(BIN)as86
LD=$(BIN)ld86
NASM=$(BIN)nasm86

C86LIB=$(TOPDIR)/libc
INCLUDES=-I$(TOPDIR)/libc/include -I$(TOPDIR)/elks/include -I$(C86LIB)/include/c86
DEFINES=-DDRIVER=1

CPPFLAGS=-0 $(INCLUDES) $(DEFINES)
CFLAGS=-g -O -bas86 -warn=4 -lang=c99
CFLAGS+=-align=yes -separate=yes -stackopt=minimum -peep=all -stackcheck=no
#ASFLAGS=-0 -j -O -w-
ASFLAGS=-0 -j
LDFLAGS=-0 -i -L$(C86LIB)
LDLIBS=-lc86
NASMFLAGS=-f as86

# Automated rules for C86 toolchain
%.o: %.c

%.i: %.c
	$(CPP) $(CPPFLAGS) -o $*.i $<

%.as: %.i
	$(CC) $(CFLAGS) $< $*.as

%.o: %.as
	$(AS) $(ASFLAGS) -o $*.o -l $*.lst $*.as

%.o: %.s
	$(AS) $(ASFLAGS) -o $*.o -l $*.lst $*.s

##### End of standardized section #####

#DEFINES+=

PROGS=chess test_cc test showfont vgatest paint evtest

all: $(PROGS)
#ifeq "$(TOPDIR)" "/Users/greg/net/elks-gh"
#	cp -p $(PROGS) $(TOPDIR)/elkscmd/rootfs_template/root
#endif

showfont: showfont.o
	$(LD) $(LDFLAGS) -o $@ $^

#.PRECIOUS: test.i test.as cprintf.i cprintf.as
test: test.o cprintf.o
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

test_cc: test_cc.o
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

#.PRECIOUS: chess.i chess.as
chess: chess.o
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)

#.PRECIOUS: vgatest.i vgatest.as graphics.i graphics.as
vgatest: vgatest.o graphics.o vga-4bp.o strtoi.o
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	#cp vgatest $(TOPDIR)/elkscmd/rootfs_template/root

paint: paint.o graphics.o vga-4bp.o event.o mouse.o strtoi.o
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	#cp paint $(TOPDIR)/elkscmd/rootfs_template/root

#.PRECIOUS: evtest.i evtest.as event.i event.as mouse.i mouse.as
evtest: evtest.o event.o mouse.o
	$(LD) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	#cp evtest $(TOPDIR)/elkscmd/rootfs_template/root

clean:
	rm -f *.o *.as *.i *.lst $(PROGS)
