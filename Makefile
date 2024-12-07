all:
	make cleaneobjs
	make elks
	make cleanhobjs
	make host

elks:
	make -C compiler -f Makefile.elks
	make -C assembler -f Makefile.elks
	make -C cpp -f Makefile.elks
	make -C ld -f Makefile.elks
	make -C ar -f Makefile.elks
	make -C make -f Makefile.elks
	make -C disasm -f Makefile.elks
	make -C nasm32 -f Makefile.elks

host:
	make -C compiler
	make -C assembler
	make -C cpp
	make -C ld
	make -C ar
	make -C make
	make -C disasm
	make -C nasm32
	make -C emulator
	make -C libc

copy:
	cp libc/*.a elks-bin/* $(TOPDIR)/elkscmd/rootfs_template/root

.PHONY: clean cleanobjs

clean: cleaneobjs cleanhobjs

cleaneobjs:
	make -C compiler -f Makefile.elks clean
	make -C assembler -f Makefile.elks clean
	make -C cpp -f Makefile.elks clean
	make -C ld -f Makefile.elks clean
	make -C ar -f Makefile.elks clean
	make -C make -f Makefile.elks clean
	make -C disasm -f Makefile.elks clean
	make -C nasm32 -f Makefile.elks clean

cleanhobjs:
	make -C compiler clean
	make -C assembler clean
	make -C cpp clean
	make -C ld clean
	make -C ar clean
	make -C make clean
	make -C disasm clean
	make -C nasm32 clean
	make -C emulator clean
	make -C libc clean
