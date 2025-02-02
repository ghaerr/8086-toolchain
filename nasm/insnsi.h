/* This file is auto-generated from insns.dat by insns.pl - don't edit it */

/* This file in included by nasm.h */

/* Instruction names */
enum {
    I_AAA,
    I_AAD,
    I_AAM,
    I_AAS,
    I_ADC,
    I_ADD,
    I_AND,
    I_CALL,
    I_CBW,
    I_CLC,
    I_CLD,
    I_CLI,
    I_CMC,
    I_CMP,
    I_CMPSB,
    I_CMPSW,
    I_CWD,
    I_DAA,
    I_DAS,
	I_DB,
	I_DD,
    I_DEC,
    I_DIV,
	I_DQ,
	I_DT,
	I_DW,
    I_EQU,
    I_F2XM1,
    I_FABS,
    I_FADD,
    I_FADDP,
    I_FBLD,
    I_FBSTP,
    I_FCHS,
    I_FCLEX,
    I_FCOM,
    I_FCOMP,
    I_FCOMPP,
    I_FDECSTP,
    I_FDISI,
    I_FDIV,
    I_FDIVP,
    I_FDIVR,
    I_FDIVRP,
    I_FENI,
    I_FFREE,
    I_FIADD,
    I_FICOM,
    I_FICOMP,
    I_FIDIV,
    I_FIDIVR,
    I_FILD,
    I_FIMUL,
    I_FINCSTP,
    I_FINIT,
    I_FIST,
    I_FISTP,
    I_FISUB,
    I_FISUBR,
    I_FLD,
    I_FLD1,
    I_FLDCW,
    I_FLDENV,
    I_FLDL2E,
    I_FLDL2T,
    I_FLDLG2,
    I_FLDLN2,
    I_FLDPI,
    I_FLDZ,
    I_FMUL,
    I_FMULP,
    I_FNCLEX,
    I_FNDISI,
    I_FNENI,
    I_FNINIT,
    I_FNOP,
    I_FNSAVE,
    I_FNSTCW,
    I_FNSTENV,
    I_FNSTSW,
    I_FPATAN,
    I_FPREM,
    I_FPTAN,
    I_FRNDINT,
    I_FRSTOR,
    I_FSAVE,
    I_FSCALE,
    I_FSQRT,
    I_FST,
    I_FSTCW,
    I_FSTENV,
    I_FSTP,
    I_FSTSW,
    I_FSUB,
    I_FSUBP,
    I_FSUBR,
    I_FSUBRP,
    I_FTST,
    I_FWAIT,
    I_FXAM,
    I_FXCH,
    I_FXTRACT,
    I_FYL2X,
    I_FYL2XP1,
    I_HLT,
    I_IDIV,
    I_IMUL,
    I_IN,
    I_INC,
	I_INCBIN,
    I_INT,
    I_INT03,
    I_INT3,
    I_INTO,
    I_IRET,
    I_IRETW,
    I_JCXZ,
    I_JMP,
    I_LAHF,
    I_LDS,
    I_LEA,
    I_LES,
    I_LODSB,
    I_LODSW,
    I_LOOP,
    I_LOOPE,
    I_LOOPNE,
    I_LOOPNZ,
    I_LOOPZ,
    I_MOV,
    I_MOVSB,
    I_MOVSW,
    I_MUL,
    I_NEG,
    I_NOP,
    I_NOT,
    I_OR,
    I_OUT,
    I_POP,
    I_POPF,
    I_POPFW,
    I_PUSH,
    I_PUSHF,
    I_PUSHFW,
    I_RCL,
    I_RCR,
    I_RESB,
    I_RESD,
    I_RESQ,
    I_REST,
    I_RESW,
    I_RET,
    I_RETF,
    I_RETN,
    I_ROL,
    I_ROR,
    I_SAHF,
    I_SAL,
    I_SALC,
    I_SAR,
    I_SBB,
    I_SCASB,
    I_SCASW,
    I_SHL,
    I_SHR,
    I_STC,
    I_STD,
    I_STI,
    I_STOSB,
    I_STOSW,
    I_SUB,
    I_TEST,
    I_WAIT,
    I_XCHG,
    I_XLAT,
    I_XLATB,
    I_XOR,
    I_Jcc,
};

#define MAX_INSLEN 11
