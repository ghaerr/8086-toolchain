/* insns.h   header file for insns.c
 * $Id: insns.h,v 1.29 2002/09/12 16:34:06 hpa Exp $
 *
 * The Netwide Assembler is copyright (C) 1996 Simon Tatham and
 * Julian Hall. All rights reserved. The software is
 * redistributable under the licence given in the file "Licence"
 * distributed in the NASM archive.
 */

#ifndef NASM_INSNS_H
#define NASM_INSNS_H

#include "insnsi.h"		       /* instruction opcode enum */

/* max length of any instruction, register name etc. */
#if MAX_INSLEN > 9		/* MAX_INSLEN defined in insnsi.h */
#define MAX_KEYWORD MAX_INSLEN
#else
#define MAX_KEYWORD 9
#endif

struct itemplate {
    int opcode;			       /* the token, passed from "parser.c" */
    int operands;		       /* number of operands */
    long opd[3];		       /* bit flags for operand types */
    const char *code;		   /* the code it assembles to */
    unsigned long flags;	       /* some flags */
};

/* 
 * this define is used to signify the end of an itemplate 
 */
#define ITEMPLATE_END {-1,-1,{-1,-1,-1},NULL,0}

/*
 * Instruction template flags. These specify which processor
 * targets the instruction is eligible for, whether it is
 * privileged or undocumented, and also specify extra error
 * checking on the matching of the instruction.
 *
 * IF_SM stands for Size Match: any operand whose size is not
 * explicitly specified by the template is `really' intended to be
 * the same size as the first size-specified operand.
 * Non-specification is tolerated in the input instruction, but
 * _wrong_ specification is not.
 *
 * IF_SM2 invokes Size Match on only the first _two_ operands, for
 * three-operand instructions such as SHLD: it implies that the
 * first two operands must match in size, but that the third is
 * required to be _unspecified_.
 *
 * IF_SB invokes Size Byte: operands with unspecified size in the
 * template are really bytes, and so no non-byte specification in
 * the input instruction will be tolerated. IF_SW similarly invokes
 * Size Word, and IF_SD invokes Size Doubleword.
 *
 * (The default state if neither IF_SM nor IF_SM2 is specified is
 * that any operand with unspecified size in the template is
 * required to have unspecified size in the instruction too...)
 */

#define IF_SM     0x00000001UL	       /* size match */
#define IF_SM2    0x00000002UL	       /* size match first two operands */
#define IF_SB     0x00000004UL	       /* unsized operands can't be non-byte */
#define IF_SW     0x00000008UL	       /* unsized operands can't be non-word */
#define IF_SD     0x00000010UL	       /* unsized operands can't be nondword */
#define IF_AR0	  0x00000020UL	       /* SB, SW, SD applies to argument 0 */
#define IF_AR1	  0x00000040UL	       /* SB, SW, SD applies to argument 1 */
#define IF_AR2	  0x00000060UL	       /* SB, SW, SD applies to argument 2 */
#define IF_ARMASK 0x00000060UL         /* mask for unsized argument spec */
#define IF_PRIV   0x00000100UL	       /* it's a privileged instruction */
#define IF_SMM    0x00000200UL	       /* it's only valid in SMM */
#define IF_PROT   0x00000400UL         /* it's protected mode only */
#define IF_UNDOC  0x00001000UL	       /* it's an undocumented instruction */
#define IF_FPU    0x00002000UL	       /* it's an FPU instruction */
#define IF_PMASK  0xFF000000UL	       /* the mask for processor types */
#define IF_PLEVEL 0x0F000000UL         /* the mask for processor instr. level */
					/* also the highest possible processor */
#define IF_PFMASK 0xF001FF00UL	       /* the mask for disassembly "prefer" */
#define IF_8086   0x00000000UL	       /* 8086 instruction */

#endif
