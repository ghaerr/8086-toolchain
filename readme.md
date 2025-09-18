# Native ELKS Toolchain

ELKS 8086 Toolchain for ELKS. This project contains a C and assembly languages toolchain for 8086
CPU targetting ELKS, which can run nativelly on ELKS or on other platforms (eg. Linux and MacOS X).

ELKS toolchain C compiler is C86, by Matthew Brandt, Christoph van Wuellen, Keith and Dave Walker.
Also part of the toolchain are an assembler, linker, C pre-preprocessor, archiver and disassembler. 
Bruce Evans, Jody Bruchon et al. are the authors for dev86's cpp, ld, ar and as. 
Make is courtesy of David Given, Carl Eric Codere and Ceriel Jacobs, while disasm (dis88) was originally authored by G. M. Harding.
Many improvements and changes by Rafael Diniz and Gregory Haerr for making all the software run on ELKS.

The most important components of the toolchain are:

- cpp, ld, as and ar from dev86 project: https://codeberg.org/jbruchon/dev86
- make from: https://github.com/davidgiven/ack/tree/default/util/make
- C compiler (C86): Also called C68. [Manual](https://github.com/ghaerr/8086-toolchain/blob/dev86/man/c86man.txt),
[Historic](http://ftp.funet.fi/pub/minix/lang/c68/) [versions](https://ladsoft.tripod.com/cc386_compiler.html) are
available. Thanks to Prof. Dr. James Archibald for preserving and using this compiler for [real-time operating system classes](https://hintron.github.io/8086-toolchain/)
- disasm:  Check [README](https://github.com/ghaerr/8086-toolchain/blob/dev86/disasm/README)

## Build and Install

In order to build the ELKS 8086 Toolchain, gcc-ia16 and also OpenWatcom v2 is required. Please refer to instructions: [Wiki: Setting up the 8086 toolchain (C86 compiler and tools)](https://github.com/ghaerr/elks/wiki/Setting-up-the-8086-toolchain-(C86-compiler-and-tools)).

Basically, after all environment variables set, type:
```
make
```

ELKS binaries will be in "elks-bin" and host binaries in "host-bin". Build should
work on any recent Linux or MacOS with OpenWatcom v2 installed (source the OW owsetenv.sh script for setting
OpenWatcom environment variables and env.sh from ELKS, or just env.sh from ELKS and wcenv.sh in libc directory of 
ELKS top-dir, after adjusting the path to OpenWatcom directory).

The ELKS source code top directory contains a script 'buildc86.sh' which builds the
toolchain binaries, and 'copyc86.sh' which copies the toolchain binaries, headers and
library to ELKS /usr, as well as an archive 'devc86.tar'.


## Softwares using 8086-toolchain

- ELKS PAINT: https://github.com/ghaerr/elks/tree/master/elkscmd/gui
- ELKS Image Viewer: https://github.com/rafael2k/elks-viewer

## Issues

Please report issues at [Issues](https://github.com/ghaerr/8086-toolchain/issues).
