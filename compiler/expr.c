/*
 * C compiler
 * ==========
 *
 * Copyright 1989, 1990, 1991 Christoph van Wuellen.
 * Credits to Matthew Brandt.
 * All commercial rights reserved.
 *
 * This compiler may be redistributed as long there is no
 * commercial interest. The compiler must not be redistributed
 * without its full sources. This notice must stay intact.
 *
 * History:
 *
 * 1989   starting an 68000 C compiler, starting with material
 *		  originally by M. Brandt
 * 1990   68000 C compiler further bug fixes
 *		  started i386 port (December)
 * 1991   i386 port finished (January)
 *		  further corrections in the front end and in the 68000
 *		  code generator.
 *		  The next port will be a SPARC port
 */

/******************************************************************************
 *
 * expression evaluation
 *
 * this set of routines builds a parse tree for an expression. no code is
 * generated for the expressions during the build, this is the job of the
 * codegen module. for most purposes expression() is the routine to call. it
 * will allow all of the C operators. for the case where the comma operator
 * is not valid (function parameters for instance) call exprnc().
 *
 * each of the routines returns a pointer to a describing type structure. each
 * routine also takes one parameter which is a pointer to an expression node
 * by reference (address of pointer). the completed expression is returned in
 * this pointer. all routines return either a pointer to a valid type or NULL
 * if the hierarchy of the next operator is too low or the next symbol is not
 * part of an expression.
 *
 *****************************************************************************/

#include "chdr.h"
#include "expr.h"
#include "cglbdec.h"
#include "proto.h"

/********************************************************* Macro Definitions */

#define NIL_STREE	((STREE *)0)

#ifdef SYNTAX_CORRECT
#define check_scalar(x)
#define check_integral(x)
#define check_arithmetic(x)
#define check_zero(x)
#define check_shift(x1,x2)
#define check_discard(x)
#define check_unsigned(x1,x2,x3)
#define check_relational(x1,x2)
#define check_modifiable_lvalue(x)
#define check_sizeof(x1,x2)
#define set_used(x)
#define check_set(x)
#define check_equality(x1,x2)
#endif /* SYNTAX_CORRECT */

/********************************************************** Type Definitions */

typedef struct _stree STREE;
struct _stree
{
    LABEL   label;
    const CHAR *sptr;
    size_t  len;
    STREE  *less;
    STREE  *more;
};

enum fpos
{
    format_start, format_precision, format_optional
};

/********************************************************** Static Variables */

static STREE *strtree = NIL_STREE;	/* Tree for string constants */
static BOOL sizeof_flag = FALSE;
static BOOL address_flag = FALSE;

#ifdef SEQUENCE
static SEQNUM sequence_number = (SEQNUM) 1;

#else /* SEQUENCE */
#define check_sequence(ep,mnum)
#define check_sequence_modified(ep)
#define check_sequence_accessed(ep)
#endif /* SEQUENCE */

/*********************************************** Static Function Definitions */

#ifndef SYNTAX_CORRECT
static void check_object P_ ((const EXPR *));
static void check_scalar P_ ((const EXPR *));
static void check_integral P_ ((const EXPR *));
static void check_arithmetic P_ ((const EXPR *));
static void check_modifiable_lvalue P_ ((const EXPR *));

#ifdef FORMAT_CHECK
static void check_parameter

P_ ((const CHAR *, int, const TYP *, const TYP *));
static void check_pointer_parameter
P_ ((const CHAR *, int, const TYP *, const TYP *));
static void check_array_parameter P_ ((const CHAR *, int, const TYP *));
static const CHAR *check_printf
P_ ((const CHAR *, int, const CHAR *, enum fpos *, const TYP *));
static const CHAR *check_scanf
P_ ((const CHAR *, int, const CHAR *, enum fpos *, const TYP *));
static const CHAR *findstr P_ ((const STREE *, LABEL));

#endif /* FORMAT_CHECK */
#endif /* SYNTAX_CORRECT */

static BOOL is_null_pointer P_ ((const EXPR *));
static EXPR *condition P_ ((EXPR *));
static EXPR *integral_promotion P_ ((EXPR *));
static EXPR *mk_enode P_ ((EXPRTYPE, TYP *));
static EXPR *mk_size P_ ((SIZE, TYP *));

static EXPR *deref P_ ((EXPR *, TYP *));
static EXPR *cond_deref P_ ((EXPR *, TYP *));
static EXPR *nameref P_ ((void));
static EXPR *addops P_ ((void));
static EXPR *andop P_ ((void));
static EXPR *asnop P_ ((void));
static EXPR *binlog P_ ((EXPR *(*)(void), EXPRTYPE, TOKEN));
static EXPR *binop P_ ((EXPR *(*)(void), EXPRTYPE, TOKEN, const char *));
static EXPR *bitandop P_ ((void));
static EXPR *bitorop P_ ((void));
static EXPR *bitxor P_ ((void));
static EXPR *commaop P_ ((void));
static EXPR *conditional P_ ((void));
static EXPR *equalops P_ ((void));
static EXPR *multops P_ ((void));
static EXPR *orop P_ ((void));
static EXPR *parmlist P_ ((const EXPR *, const BLOCK *));
static EXPR *primary P_ ((void));
static EXPR *relation P_ ((void));
static EXPR *shiftop P_ ((void));
static EXPR *unary P_ ((void));
static EXPR *explicit_castop P_ ((EXPR *, TYP *));
static TYP *arithmetic_conversion P_ ((EXPR **, EXPR **));
static TYP *arithmetic_conversion2 P_ ((EXPR **, EXPR **));


#ifdef FLOAT_SUPPORT
#ifndef TMS320C30
static EXPR *mk_fcon P_ ((const RVAL *, TYP *));

#endif /* TMS320C30 */
#endif /* FLOAT_SUPPORT */

/*****************************************************************************/

/*
 * returns true if the expression is a constant integral expression node,
 * otherwise returns false.
 */
static BOOL is_constexpr P1 (const EXPR *, ep)
{
    return (is_icon (ep));
}

/*
 * returns true if the expression is a constant which can be represented
 * within the range of the type, otherwise it returns false.
 */
BOOL is_constant_in_range P2 (const EXPR *, ep, const TYP *, tp)
{
    return (is_constexpr (ep)
	    && is_constant_in_type_range (ep->v.i, ep->etp, tp));
}

/*
 * returns true if the expression is a null pointer constant, otherwise
 * returns false.
 */
static BOOL is_null_pointer P1 (const EXPR *, ep)
{
    return (is_constexpr (ep) && ep->v.i == 0L);
}

#ifndef SYNTAX_CORRECT
static BOOL is_negative P1 (const EXPR *, ep)
{
    return (is_constexpr (ep) && ep->v.i < 0L);
}

static BOOL is_negative_or_zero P1 (const EXPR *, ep)
{
    return (is_constexpr (ep) && ep->v.i <= 0L);
}
#endif /* SYNTAX_CORRECT */

/*
 * returns true if ep is an expression suitable as an lvalue
 */
BOOL is_lvalue P1 (const EXPR *, ep)
{
    switch (ep->nodetype) {
    case en_ref:
	/*
	 * a function returning a structure (which cannot be used as an
	 * lvalue) will create an en_deref node
	 */
	return (ep->v.p[0]->nodetype != en_add ||
		ep->v.p[0]->v.p[0]->nodetype != en_deref);
    case en_fieldref:
	return (ep->v.bit.p->nodetype != en_add ||
		ep->v.bit.p->v.p[0]->nodetype != en_deref);
    default:
	break;
    }
    return FALSE;
}

#ifdef SEQUENCE
static void check_sequence P2 (const EXPR *, ep, MSGNUM, mnum)
{
    SYM    *sp;

    switch (ep->nodetype) {
    case en_ref:
	check_sequence (ep->v.p[0], mnum);
	break;
    case en_sym:
	sp = ep->v.sp;
	if (sp->sequence == sequence_number) {
	    /* already modified at this sequence point */
	    message (mnum, nameof (sp));
	}
#ifndef SYNTAX_CORRECT
	if (mnum == WARN_MODIFIED) {
	    sp->sequence = sequence_number;
	}
#endif /* SYNTAX_CORRECT */
	break;
    default:
	break;
    }
}

static void check_sequence_modified P1 (const EXPR *, ep)
{
#ifndef SYNTAX_CORRECT
    check_sequence (ep, WARN_MODIFIED);
#endif /* SYNTAX_CORRECT */
}

static void check_sequence_accessed P1 (const EXPR *, ep)
{
#ifndef SYNTAX_CORRECT
    check_sequence (ep, WARN_ACCESS);
#endif /* SYNTAX_CORRECT */
}

/*
 * a sequence-point has been encountered
 */
void sequence_point P0 (void)
{
    sequence_number++;
}

#endif

static void check_object P1 (const EXPR *, ep)
{
#ifndef SYNTAX_CORRECT
    if (!is_object_type (referenced_type (ep->etp))) {
	message (ERR_OBJECT);
    }
#endif /* SYNTAX_CORRECT */
}

#ifndef SYNTAX_CORRECT
static void check_scalar P1 (const EXPR *, ep)
{
    if (!is_scalar_type (ep->etp)) {
	message (ERR_SCALAR);
    }
}

static void check_integral P1 (const EXPR *, ep)
{
    if (!is_integral_type (ep->etp)) {
	message (ERR_INTEGER);
    }
}

static void check_arithmetic P1 (const EXPR *, ep)
{
    if (!is_arithmetic_type (ep->etp)) {
	message (ERR_ARITHMETIC);
    }
}

static void check_zero P1 (const EXPR *, ep)
{
    if (!is_constexpr (ep)) {
	return;
    }
    if (is_integral_type (ep->etp) && (ep->v.i == 0L)) {
	message (WARN_ZERO);
    }
#ifdef FLOAT_SUPPORT
#ifndef FLOAT_BOOTSTRAP
    else if (is_real_floating_type (ep->etp)
	     && FEQ (((const EXPR *) ep)->v.f, F_zero)) {
	message (WARN_ZERO);
    }
#endif /* FLOAT_BOOTSTRAP */
#endif /* FLOAT_SUPPORT */
}

static void check_shift P2 (const EXPR *, ep, const TYP *, tp)
{
    if (!is_constexpr (ep) || !is_integral_type (ep->etp)) {
	return;
    }
    if ((ep->v.i < (IVAL) 0L) ||
	(ep->v.i >= (IVAL) (tp->size * bits_in_sizeunit))) {
	message (WARN_SHIFT, ep->v.i, nameoftype (tp));
    }
}

void check_discard P1 (const EXPR *, ep)
{
    if (ep == NIL_EXPR) {
	return;
    }
    switch (ep->nodetype) {
    case en_fcall:
    case en_call:
    case en_usercall:
	if (!is_void (ep->etp)) {
	    message (WARN_IGNORE,
		     ep->v.p[0]->nodetype == en_nacon ?
		     (const char *) ep->v.p[0]->v.str : "");
	}
	break;
    case en_assign:
    case en_asadd:
    case en_assub:
    case en_asmul:
    case en_asmul2:
    case en_asdiv:
    case en_asdiv2:
    case en_asmod:
    case en_asrsh:
    case en_asxor:
    case en_aslsh:
    case en_asand:
    case en_asor:
    case en_ainc:
    case en_adec:
	break;
    case en_comma:
	check_discard (ep->v.p[1]);
	break;
    default:
	if (!is_void (ep->etp)) {
	    message (WARN_DISCARD);
	}
	break;
    }
}

static void check_unsigned P3 (const EXPR *, ep1, const EXPR *, ep2, BOOL,
			       check_for_zero)
{
    if (is_signed_type (ep2->etp)) {
	if (is_unsigned_type (ep1->etp)) {
	    if (check_for_zero ? is_negative_or_zero (ep2) :
		is_negative (ep2)) {
		message (WARN_UNSIGNED, nameoftype (ep1->etp));
	    }
	} else if (is_char (ep1->etp)) {
	    if (check_for_zero ? is_negative_or_zero (ep2) :
		is_negative (ep2)) {
		message (WARN_CHAR);
	    }
	}
    }
}

static void check_relational P2 (const EXPR *, ep1, const EXPR *, ep2)
{
    check_unsigned (ep1, ep2, TRUE);
    check_unsigned (ep2, ep1, TRUE);
}

static void check_equality P2 (const EXPR *, ep1, const EXPR *, ep2)
{
    check_unsigned (ep1, ep2, FALSE);
    check_unsigned (ep2, ep1, FALSE);
}

/*
 *	 A modifiable lvalue is an lvalue that does not have array
 *	 type, does not have an incomplete type, does not have a
 *	 const-qualified type, and if it is a structure or union,
 *	 does not have any member (including, recursively, any member
 *	 of all contained structures or unions) with const-qualified
 *	 type.
 */

static void check_modifiable_lvalue P1 (const EXPR *, ep)
{
    if (!is_lvalue (ep)) {
	message (ERR_LVALUE);
    }
    if (is_const_qualified (ep->etp)) {
	message (ERR_CONST);
    }
}

/*
 *	 Check that the sizeof hasn't been applied to a bitfield, void
 *	 or a function.
 */

static void check_sizeof P2 (const EXPR *, ep, SIZE, size)
{
    if (size == UNKNOWN_SIZE ||
	(ep != NIL_EXPR &&
	 ((ep->nodetype == en_fieldref) ||
	  ((lang_option >= LANG_C90)
	   && (is_sym (ep) && is_function_type (ep->etp)))))) {
	message (ERR_ILLSIZEOF);
    }
    if (size == 0L) {
	message (WARN_SIZEOF0);
    }
    /* a ">>" is undefined if the RHS equals number of bits of LHS! */
    if (((USIZE) size >> ((USIZE) tp_size->size * 4L)) >>
	((USIZE) tp_size->size * 4L) != Ox0UL) {
	message (WARN_SIZEOFBIG, size);
    }
}


/*
 *	 If the expression is a symbol then mark the symbol as set
 */

static void set_used P1 (const EXPR *, ep)
{
    switch (ep->nodetype) {
    case en_ref:
	set_used (ep->v.p[0]);
	break;
    case en_sym:
	symbol_set (ep->v.sp);
	break;
    default:
	break;
    }
}


/*
 *	 Check to see if the symbol has been set before it is about
 *	 to be used.
 */

static void check_set P1 (const EXPR *, ep)
{
    SYM    *sp;

    switch (ep->nodetype) {
    case en_ref:
	check_set (ep->v.p[0]);
	break;
    case en_sym:
	sp = ep->v.sp;
	switch (storageof (sp)) {
	case sc_auto:
	case sc_register:
	    if (!is_symbol_set (sp)) {
		message (WARN_NOTSET, nameof (sp));
		symbol_set (sp);
	    }
	    break;
	default:
	    break;
	}
	break;
    default:
	break;
    }
}
#endif /* SYNTAX_CORRECT */


#ifdef FORMAT_CHECK
/*
 * Search the tree looking for the string associated with the supplied label
 */
static const CHAR *findstr P2 (const STREE *, tree, LABEL, lab)
{
    const CHAR *str;

    if (tree == (STREE *) 0) {
	return (CHAR *) 0;
    }
    if (tree->label == lab) {
	return tree->sptr;
    }
    str = findstr (tree->less, lab);
    return str ? str : findstr (tree->more, lab);
}

static const CHAR *get_stringlit P1 (LABEL, lab)
{
    return findstr (strtree, lab);
}
#endif /* FORMAT_CHECK */

/*
 * mk_ s a string literal and return it's label number.
 *
 *		Strict K&R requires that all strings even when
 *		written identically are distinct.	This
 *		behaviour is enforced if the 'lang_option' flag
 *		is set to LANG_KANDR.
 */
static EXPR *mk_stringlit P2 (const CHAR *, s, size_t, len)
{
    STREE  *p, *q = NIL_STREE;
    STRING *lp;
    int     local_global = global_flag;
    int     result = 0;

    if (lang_option >= LANG_C90) {
	/*
	 * Not in traditional mode, so shared strings allowed.
	 * Search our tree of existing strings to see if
	 * an identical one has already been generated.
	 * (this is allowed by ANSI).
	 * If so we can merely return its label.
	 */
	for (q = p = strtree; p; p = (result < 0) ? p->less : p->more) {
	    ptrdiff_t diff = (ptrdiff_t) (p->len - len);

	    if (diff >= 0) {
		result =
		    memcmp ((const void *) s, (const void *) (p->sptr + diff),
			    (size_t) len);
		if (result == 0) {
		    EXPR   *ep = mk_lcon (p->label);

		    if (diff) {
			ep =
			    mk_node (en_add, ep,
				     mk_icon ((IVAL) diff, tp_pointer),
				     tp_pointer);
		    }
		    return ep;
		}
		result = memcmp(s, p->sptr, len);     /* ghaerr: fix tree search */
	    }
	    q = p;
	}
    }
    /*
     *      If we reach this point, either the string does not exist
     *      or we are operating in 'traditional' mode.       We must therefore
     *      create a new string.   The actual string text is allocated
     *      in local space if in traditional mode, and global otherwise
     *      (so we can check for strings already generated).
     */
    global_flag = 0;		/* always allocate from local space. */
    lp = (STRING *) xalloc (sizeof (STRING));

    lp->label = nextlabel++;
    global_flag = lang_option >= LANG_C90;
    lp->str = s;
    lp->len = len;
    lp->next = strtab;
    strtab = lp;
    if (lang_option >= LANG_C90) {
	/*
	 *      traditional mode not set, so create a new entry for
	 *      our tree of already generated strings.
	 */
	p = (STREE *) xalloc (sizeof (STREE));
	p->label = lp->label;
	p->sptr = lp->str;
	p->len = lp->len;
	p->less = p->more = NIL_STREE;
	if (q == NIL_STREE) {
	    strtree = p;
	} else if (result < 0) {
	    q->less = p;
	} else {
	    q->more = p;
	}
    }
    global_flag = local_global;
    return mk_lcon (lp->label);
}


/*
 * generates the cast node if required for an integral promotion
 */
static EXPR *integral_promotion P1 (EXPR *, ep)
{
    TYP    *tp = unary_conversion (ep->etp);

    if (tp != ep->etp) {
	return implicit_castop (ep, tp);
    }
    return ep;
}

static EXPR *mk_enode P2 (EXPRTYPE, nt, TYP *, tp)
{
    EXPR   *ep = (EXPR *) xalloc (sizeof (EXPR));

    ep->nodetype = nt;
    ep->etp = tp;
    return ep;
}

EXPR   *copynode P1 (const EXPR *, ep)
{
    EXPR   *temp;

    if (ep == NIL_EXPR) {
	return NIL_EXPR;
    }
    temp = (EXPR *) xalloc (sizeof (EXPR));

    *temp = *ep;
    return temp;
}

/*
 * build an expression node with a node type of nt and values v1 and v2.
 */
EXPR   *mk_node P4 (EXPRTYPE, nt, EXPR *, v1, EXPR *, v2, TYP *, tp)
{
    EXPR   *ep = mk_enode (nt, tp);

#ifndef SYNTAX_CORRECT
#ifndef FLOAT_SUPPORT
    switch (tp->type) {
    case bt_float:
    case bt_double:
    case bt_longdouble:
    case bt_floatcomplex:
    case bt_doublecomplex:
    case bt_longdoublecomplex:
	message (ERR_NOFLOATSUPPORT);
	break;
    default:
	break;
    }
    
#endif	/* FLOAT_SUPPORT */
#ifndef LONGLONG_SUPPORT
    switch (tp->type) {
    case bt_longlong:
    case bt_ulonglong:
	message (ERR_NOLONGLONGSUPPORT);
	break;
    default:
	break;
    }
    
#endif	/* LONGLONG_SUPPORT */
#endif	/* SYNTAX_CORRECT */
    ep->v.p[0] = v1;
    ep->v.p[1] = v2;
    return ep;
}

/*
 * build an expression node forming an integer constant
 */
EXPR   *mk_icon P2 (IVAL, i, TYP *, tp)
{
    EXPR   *ep = mk_enode (en_icon, tp);

    ep->v.i = i;
    return ep;
}

/*
 * build an expression node forming an size constant
 */ 
static EXPR *mk_size P2 (SIZE, size, TYP *, tp) 
{
    EXPR  *ep = mk_enode (en_icon, tp);

    ep->v.i = (IVAL) size;
    
    return ep;
}



/*
 * build an expression node forming a string constant
 */
EXPR   *mk_scon P2 (const CHAR *, str, TYP *, tp)
{
    EXPR   *ep = mk_enode (en_str, tp);

    ep->v.str = str;
    return ep;
}

#ifdef FLOAT_SUPPORT
/*
 * build an expression node forming a floating point constant
 */
#ifndef TMS320C30
static
#endif				/* TMS320C30 */
EXPR   *mk_fcon P2 (const RVAL *, fp, TYP *, tp)
{
    EXPR   *ep = mk_enode (en_fcon, tp);

    FASSIGN (ep->v.f, *fp);
    return ep;
}
#endif

/*
 * build an expression node forming a label
 */
EXPR   *mk_lcon P1 (LABEL, lab)
{
    EXPR   *ep = mk_enode (en_labcon, tp_pointer);

    ep->v.l = lab;
    return ep;
}

EXPR   *mk_ref P2 (EXPR *, ep, TYP *, tp)
{
    return mk_node (en_ref, ep, NIL_EXPR, tp);
}

EXPR   *mk_symnode P1 (SYM *, sp)
{
    EXPR   *ep = mk_enode (en_sym, typeof (sp));

    ep->v.sp = sp;
    return ep;
}

EXPR   *mk_autocon P1 (SIZE, offset)
{
    EXPR   *ep = mk_node (en_autocon, NIL_EXPR, NIL_EXPR, tp_pointer);

    ep->v.i = (IVAL) offset;
    return ep;
}

EXPR   *mk_litval P2 (EXPR *, ep1, SIZE, offset)
{
    EXPR   *ep =

	mk_node (en_litval, ep1, mk_size (offset, tp_size), ep1->etp);

    return ep;
}

/*
 * build the proper dereference operation for a node using the type pointer
 * tp.
 */
static EXPR *deref P2 (EXPR *, ep, TYP *, tp)
{
    switch (tp->type) {
    case bt_func:		/* ANSI - functions automatically dereferenced */
#ifndef SYNTAX_CORRECT
	if (lang_option == LANG_KANDR) {
	    message (ERR_DEREF);
	}
	/*lint -fallthrough */
#endif /* SYNTAX_CORRECT */
    case bt_bool:
    case bt_char:
    case bt_charu:
    case bt_uchar:
    case bt_schar:
    case bt_short:
    case bt_ushort:
    case bt_int16:
    case bt_uint16:
    case bt_pointer16:
    case bt_int32:
    case bt_uint32:
    case bt_long:
    case bt_pointer32:
    case bt_ulong:
    case bt_longlong:
    case bt_ulonglong:
    case bt_struct:
    case bt_union:
    case bt_float:
    case bt_double:
    case bt_longdouble:
    case bt_floatcomplex:
    case bt_doublecomplex:
    case bt_longdoublecomplex:
    case bt_floatimaginary:
    case bt_doubleimaginary:
    case bt_longdoubleimaginary:
	ep = mk_ref (ep, tp);
	break;
    case bt_ubitfield:
	ep = mk_node (en_fieldref, ep, NIL_EXPR, tp_uint);
	ep->v.bit.width = bitfield_width (tp);
	ep->v.bit.offset = bitfield_offset (tp);
	break;
    case bt_bitfield:
	ep = mk_node (en_fieldref, ep, NIL_EXPR, tp_int);
	ep->v.bit.width = bitfield_width (tp);
	ep->v.bit.offset = bitfield_offset (tp);
	break;
    case bt_bbitfield:
	ep = mk_node (en_fieldref, ep, NIL_EXPR, tp_bool);
	ep->v.bit.width = bitfield_width (tp);
	ep->v.bit.offset = bitfield_offset (tp);
	break;
    case bt_void:
	if (is_qualified_type (tp)) {
	    ep = mk_ref (ep, tp);
	    break;
	}
	/*lint -fallthrough */
    default:
#ifndef SYNTAX_CORRECT
	message (ERR_DEREF);
#endif /* SYNTAX_CORRECT */
	break;
    }
    return ep;
}

/*
 * dereference the node if state is not STATE_DERIVED
 * If state is STATE_DERIVED and tp is a bt_pointer (array reference)
 *	   set the size field to the pointer size
 */
static EXPR *cond_deref P2 (EXPR *, ep, TYP *, tp)
{
    if (is_derived_type (tp)) {
	/*
	 * Here, the information on array sizes is lost.
	 */
	if (is_pointer_type (tp)) {
	    if (sizeof_flag) {
		ep->nodetype = en_size;
		ep->v.i = (IVAL) tp->size;
	    }
	    tp = mk_type (tp_array, referenced_type (tp));
	    tp->size = tp_pointer->size;
	    set_derived (tp);
	} else if (is_function_type (tp)) {
	    tp = mk_type (tp_pointer, tp);
	    set_derived (tp);
	}
	ep->etp = tp;
    } else {
	ep = deref (ep, tp);
    }
    return ep;
}


/*
 *	 nameref() will build an expression tree that references an identifier.
 *
 *	 Non-value references generate an additional level of indirection.
 */
static EXPR *nameref P0 (void)
{
    EXPR   *ep;
    SYM    *sp;
    TYP    *tp;

    sp = identifier ();
    tp = typeof (sp);
    symbol_used (sp);
    switch (storageof (sp)) {
    case sc_register:
#ifndef SYNTAX_CORRECT
	if (address_flag) {
	    if (is_array_type (tp) || is_structure_type (tp)) {
		message (ERR_ADDREGVAR, nameof (sp));
	    }
	} else if (!sizeof_flag && is_array_type (tp)) {
	    message (ERR_IMPLICITADDR, nameof (sp));
	}
	/*lint -fallthrough */
#endif /* SYNTAX_CORRECT */
    case sc_static:
    case sc_global:
    case sc_external:
    case sc_auto:
    case sc_parms:
	if (address_flag || is_array_type (tp) || is_structure_type (tp)) {
	    symbol_set (sp);
	}
	ep = mk_symnode (sp);
	break;
    case sc_const:
	ep = mk_icon (sp->value.i, tp);
	break;
    default:
#ifndef SYNTAX_CORRECT
	message (ERR_ILLCLASS);
	ep = mk_autocon ((SIZE) sp->value.i);
#endif /* SYNTAX_CORRECT */
	break;
    }
    address_flag = FALSE;
    return cond_deref (ep, tp);
}


#ifdef FORMAT_CHECK
#ifndef SYNTAX_CORRECT
static void check_parameter P4 (const CHAR *, fname, int, num, const TYP *,
				tp1, const TYP *, tp2)
{
    if (tp1 == NIL_TYP) {
	message (WARN_COUNTPARAM, fname);
    } else if (!is_same_type (tp1, tp2)) {
	message (WARN_FORMAT, num, fname);
    }
}

static void check_pointer_parameter P4 (const CHAR *, fname, int, num,
					const TYP *, tp1, const TYP *, tp2)
{
    if (tp1 == NIL_TYP) {
	message (WARN_COUNTPARAM, fname);
    } else if (!is_pointer_type (tp1)
	       || !is_same_type (referenced_type (tp1), tp2)) {
	message (WARN_FORMAT, num, fname);
    }
}

static void check_array_parameter P3 (const CHAR *, fname, int, num,
				      const TYP *, tp1)
{
    if (tp1 == NIL_TYP) {
	message (WARN_COUNTPARAM, fname);
    } else if (!is_pointer_type (tp1)) {
	message (WARN_FORMAT, num, fname);
    } else {
	switch (referenced_type (tp1)->type) {
	case bt_char:
	case bt_uchar:
	case bt_schar:
	case bt_charu:
	    break;
	default:
	    message (WARN_FORMAT, num, fname);
	    break;
	}
    }
}

static const CHAR *check_printf P5 (const CHAR *, fname, int, num,
				    const CHAR *, fstr, enum fpos *, pos,
				    const TYP *, ptp)
{
    CHAR    optional;
    TYP    *tp;

    if (fstr == NIL_CHAR || *fstr == (CHAR) 0) {
	if (ptp != NIL_TYP) {
	    message (WARN_COUNTPARAM, fname);
	}
	return NIL_CHAR;
    }
    switch (*pos) {
    case format_start:
	for (; *fstr; fstr++) {
	    if (*fstr == (CHAR) '%') {
		fstr++;
		switch (*fstr) {
		case '%':
		    continue;
		case '\0':
		    message (WARN_FORMATEND, fname);
		    return NIL_CHAR;
		default:
		    break;
		}
		break;
	    }
	}

	/* flags */
	for (optional = (CHAR) 1; optional && *fstr;) {
	    switch (*fstr) {
	    case '-':
	    case '+':
	    case ' ':
	    case '#':
	    case '0':
		fstr++;
		break;
	    default:
		optional = (CHAR) 0;
		break;
	    }
	}

	/* field width */
	for (; *fstr; fstr++) {
	    if (*fstr == (CHAR) '*') {
		*pos = format_precision;
		check_parameter (fname, num, ptp, tp_int);
		return fstr + 1;
	    }
	    if ((*fstr < '0') || (*fstr > '9')) {
		break;
	    }
	}
	/*lint -fallthrough */
    case format_precision:
	/* precision */
	if (*fstr == (CHAR) '.') {
	    for (fstr++; *fstr; fstr++) {
		if (*fstr == (CHAR) '*') {
		    *pos = format_optional;
		    check_parameter (fname, num, ptp, tp_int);
		    return fstr + 1;
		}
		if ((*fstr < '0') || (*fstr > '9')) {
		    break;
		}
	    }
	}
	/*lint -fallthrough */
    case format_optional:
	/* optional */
	*pos = format_start;
	switch (*fstr) {
	case 'h':
	case 'l':
	case 'L':
	    optional = *fstr++;
	    break;
	case 'j':
	case 'z':
	case 't':
	    if (lang_option >= LANG_C99) {
		optional = *fstr++;
		break;
	    }
	    /*lint -fallthrough*/
	default:
	    optional = (CHAR) 0;
	    break;
	}

	/* type */
	switch (*fstr) {
	case 'c':
	case 'd':
	case 'i':
	    switch (optional) {
	    case 'l':
		tp = tp_long;
		break;
	    case 'h':
		tp = tp_short;
		break;
	    case 'j':
		tp = tp_int;
		break;
	    case 't':
		tp = tp_ptrdiff;
		break;
	    case 'z':
		tp = tp_size;
		break;
	    default:
		tp = tp_int;
		break;
	    }
	    check_parameter (fname, num, ptp, promote_type (tp));
	    return fstr + 1;
	case 'o':
	case 'u':
	case 'x':
	case 'X':
	    switch (optional) {
	    case 'l':
		tp = tp_ulong;
		break;
	    case 'h':
		tp = tp_ushort;
		break;
	    case 'j':
		tp = tp_int;
		break;
	    case 't':
		tp = tp_ptrdiff;
		break;
	    case 'z':
		tp = tp_size;
		break;
	    default:
		tp = tp_uint;
		break;
	    }
	    check_parameter (fname, num, ptp, promote_type (tp));
	    return fstr + 1;
	case 'a':
	case 'e':
	case 'E':
	case 'f':
	case 'g':
	case 'G':
	    switch (optional) {
	    case 'L':
		tp = tp_longdouble;
		break;
	    default:
		tp = tp_double;
		break;
	    }
	    check_parameter (fname, num, ptp, tp);
	    return fstr + 1;
	case 's':
	    tp = tp_string;
	    check_parameter (fname, num, ptp, tp);
	    return fstr + 1;
	case 'p':
	    tp = tp_pointer;
	    check_parameter (fname, num, ptp, tp);
	    return fstr + 1;
	case 'n':
	    switch (optional) {
	    case 'l':
		tp = tp_long;
		break;
	    case 'h':
		tp = tp_short;
		break;
	    default:
		tp = tp_int;
		break;
	    }
	    check_pointer_parameter (fname, num, ptp, tp);
	    return fstr + 1;
	default:
	    break;
	}
	break;
    default:
	CANNOT_REACH_HERE ();
	break;
    }
    return fstr;
}

static const CHAR *check_scanf P5 (const CHAR *, fname, int, num,
				   const CHAR *, fstr, enum fpos *, pos,
				   const TYP *, ptp)
{
    int     suppress;
    CHAR    optional;
    TYP    *tp;

    if (fstr == NIL_CHAR || *fstr == (CHAR) 0) {
	if (ptp != NIL_TYP) {
	    message (WARN_COUNTPARAM, fname);
	}
	return NIL_CHAR;
    }

    for (; *fstr; fstr++) {
	suppress = 0;
	switch (*fstr) {
	case '%':
	    fstr++;
	    /* optional assignment-suppressing character */
	    if (*fstr == (CHAR) '*') {
		suppress++;
		fstr++;
	    }
	    /* optional decimal integer that specifies maximum field width */
	    while (('0' <= *fstr) && (*fstr <= '9')) {
		fstr++;
	    }

	    /* optional size of object */
	    switch (*fstr) {
	    case 'h':
	    case 'l':
	    case 'L':
		optional = *fstr++;
		break;
	    default:
		optional = (CHAR) 0;
		break;
	    }

	    /* type of conversion */
	    switch (*fstr) {
	    case 'd':
	    case 'i':
	    case 'n':
		switch (optional) {
		case 'l':
		    tp = tp_long;
		    break;
		case 'h':
		    tp = tp_short;
		    break;
		default:
		    tp = tp_int;
		    break;
		}
		if (suppress) {
		    continue;
		}
		check_pointer_parameter (fname, num, ptp, tp);
		return fstr + 1;
	    case 'o':
	    case 'u':
	    case 'x':
		switch (optional) {
		case 'l':
		    tp = tp_ulong;
		    break;
		case 'h':
		    tp = tp_ushort;
		    break;
		default:
		    tp = tp_uint;
		    break;
		}
		if (suppress) {
		    continue;
		}
		check_pointer_parameter (fname, num, ptp, tp);
		return fstr + 1;
	    case 'e':
	    case 'f':
	    case 'g':
		if (suppress) {
		    continue;
		}
		switch (optional) {
		case 'L':
		    tp = tp_longdouble;
		    break;
		case 'l':
		    tp = tp_double;
		    break;
		default:
		    tp = tp_float;
		    break;
		}
		check_pointer_parameter (fname, num, ptp, tp);
		return fstr + 1;
	    case '[':
		fstr++;
		if (*fstr == (CHAR) '^') {
		    fstr++;
		}
		if (*fstr == (CHAR) ']') {
		    fstr++;
		}
		while (*fstr && *fstr != (CHAR) ']') {
		    fstr++;
		}
		/*lint -fallthrough */
	    case 'c':
	    case 's':
		if (suppress) {
		    continue;
		}
		check_array_parameter (fname, num, ptp);
		return fstr + 1;
	    case 'P':
		if (suppress) {
		    continue;
		}
		tp = tp_pointer;
		check_pointer_parameter (fname, num, ptp, tp);
		return fstr + 1;
	    default:
		break;
	    }
	    break;
	default:
	    break;
	}
    }
    return NIL_CHAR;
}
#endif /* SYNTAX_CORRECT */
#endif /* FORMAT_CHECK */

/*
 * parmlist will build a list of parameter expressions in a function call and
 * return a pointer to the last expression parsed. since parameters are
 * generally pushed from right to left we get just what we asked for...
 */
static EXPR *parmlist P2 (const EXPR *, ep, const BLOCK *, block)
{
    SYM    *sp = block ? symbolsof (block) : NIL_SYM;
    SYM    *sp1 = sp;
    EXPR   *ep1, *ep2;
    TYP    *tp, *tp2;
    int     pnum = 0;
    const CHAR *fname =
	ep->nodetype == en_sym ? nameof (ep->v.sp) : (const CHAR *) "";

#ifndef SYNTAX_CORRECT
#ifdef FORMAT_CHECK
    enum fpos format_position;
    const CHAR *fstr = NIL_CHAR;
    struct pflike
    {
	const CHAR **name;
	int     num;
	const CHAR *(*func)
	    P_ ((const CHAR *, int, const CHAR *, enum fpos *, const TYP *));
    };
    struct pflike *pf = NULL;
    static struct pflike printflike[] = {
	{&fprintf_name, 2, check_printf},
	{&printf_name, 1, check_printf},
	{&sprintf_name, 2, check_printf},
	{&fscanf_name, 2, check_scanf},
	{&scanf_name, 1, check_scanf},
	{&sscanf_name, 2, check_scanf},
	{(const CHAR **) NULL, 0, NULL}
    };


    if (format_option) {
	for (pf = &printflike[0]; pf->name; pf++) {
	    if (fname == *(pf->name)) {
		break;
	    }
	}
    }
#endif /*FORMAT_CHECK */
    if ((lang_option >= LANG_C90) && sp == NIL_SYM) {
	message (WARN_NOPROTO, fname);
    }
#endif /* SYNTAX_CORRECT */
    ep1 = NIL_EXPR;
    while (lastst != tk_closepa) {
	pnum++;
	ep2 = exprnc ();	/* evaluate a parameter */
	if (ep2 == NIL_EXPR) {
	    return NIL_EXPR;
	}
	check_set (ep2);
	check_sequence_accessed (ep2);
	tp2 = tp = ep2->etp;
#ifndef SYNTAX_CORRECT
	if (is_void (tp)) {
	    message (ERR_VOIDPARM, fname);
	    break;
	}
#endif /* SYNTAX_CORRECT */
	if (sp != NIL_SYM) {	/* do prototype checking */
	    /* skip enumeration constants etc. */
	    while ((sp1 != NIL_SYM) && (is_const (sp1) || is_member (sp1)))
		sp1 = nextsym (sp1);

	    if (sp1 == NIL_SYM || is_void (typeof (sp1))) {
#ifndef SYNTAX_CORRECT
		message (ERR_COUNTPARAM, fname);
#endif /* SYNTAX_CORRECT */
	    } else if (!promoteparam_option && !is_ellipsis (typeof (sp1))) {
		if (is_array_type (typeof (sp1))) {
		    ep2 = implicit_castop (ep2, tp_pointer);
		} else {
		    ep2 = implicit_castop (ep2, typeof (sp1));
		}
	    }
	}
	/*
	 * do the default promotions
	 */
	switch (tp->type) {
	case bt_char:
	case bt_charu:
	case bt_uchar:
	case bt_schar:
	case bt_short:
	case bt_ushort:
	case bt_float:
	    if (sp1 == NIL_SYM || promoteparam_option
		|| is_ellipsis (typeof (sp1))) {
		TYP    *tp1 = promote_type (tp);

#ifndef SYNTAX_CORRECT
		if ((lang_option >= LANG_C90) && (sp1 == NIL_SYM)
		    && (tp->size < tp1->size)) {
		    message (WARN_PROMOTE, pnum, fname, nameoftype (tp1));
		}
#endif /* SYNTAX_CORRECT */
		ep2 = implicit_castop (ep2, tp1);
	    }
	    break;
	case bt_double:
	case bt_longdouble:
	case bt_struct:
	case bt_union:
	    /* trap struct assigns */
	    uses_structassign = TRUE;
	    break;
	default:
	    break;
	}

#ifndef SYNTAX_CORRECT
#ifdef FORMAT_CHECK
	if (format_option && (pf != NULL)) {
	    if ((pf->num == pnum) && (ep2->nodetype == en_labcon)) {
		fstr = get_stringlit (ep2->v.l);
		format_position = format_start;
	    } else if (fstr != NULL) {
		fstr =
		    (pf->func) (fname, pnum, fstr, &format_position,
				ep2->etp);
	    }
	}
#endif /*FORMAT_CHECK */
#endif /* SYNTAX_CORRECT */

	if (sp1 != NIL_SYM && !is_ellipsis (typeof (sp1))) {
	    /*
	     * Check to see if this would cause a problem if compiled by a
	     * K&R compiler.
	     */
	    tp = promote_type (typeof (sp1));
	    tp2 = promote_type (tp2);
	    if (is_array_type (tp)) {
		tp = tp_pointer;
	    }
	    if (is_array_type (tp2)) {
		tp2 = tp_pointer;
	    }
#ifndef SYNTAX_CORRECT
	    if (lang_option == LANG_KANDR && !is_same_size (tp, tp2)) {
		message (WARN_PARAMSIZE, pnum, fname);
	    }
#endif /* SYNTAX_CORRECT */
	    sp1 = nextsym (sp1);
	}
	ep1 = mk_node (en_list, ep2, ep1, tp_void);
	if (lastst != tk_comma) {
	    break;
	}
	getsym ();
    }
#ifndef SYNTAX_CORRECT
    /* skip enumeration constants etc. */
    while ((sp1 != NIL_SYM) && (is_const (sp1) || is_member (sp1)))
	sp1 = nextsym (sp1);
    if (sp1 != NIL_SYM && !is_ellipsis (typeof (sp1))
	&& !is_void (typeof (sp1))) {
	message (ERR_COUNTPARAM, fname);
    }
#ifdef FORMAT_CHECK
    else if (format_option && (fstr != NIL_CHAR) && (pf != NULL)) {
	VOIDCAST (pf->func) (fname, pnum, fstr, &format_position, NIL_TYP);
    }
#endif /* FORMAT_CHECK */
#endif /* SYNTAX_CORRECT */
    return ep1;
}

/*
 * primary will parse a primary expression and set the node pointer returning
 * the type of the expression parsed. primary expressions are any of:
 * id
 * constant
 * string
 * ( expression )
 * primary[ expression ]
 * primary.id
 * primary->id
 * primary( parameter list )
 * primary++
 * primary--
 * ( type-name ) { initializer-list }
 * ( type-name ) { initializer-list , }
 * -- or just a semicolon, yields empty expression --
 *
 */
static EXPR *primary P0 (void)
{
    EXPR   *ep, *ep1, *ep2;
    SYM    *sp;
    TYP    *tp, *tp1;

    switch (lastst) {
    case tk_id:
	ep = nameref ();
	if (ep == NIL_EXPR) {
	    return NIL_EXPR;
	}
	/*
	 * function names alone are pointers to functions.
	 * If followed by '(', the reference is stripped off
	 * later.
	 */
	if (is_func (ep->etp)) {
	    ep->etp = mk_type (tp_pointer, ep->etp);
	}
	break;
    case tk_iconst:
	tp = tp_int;
	goto const1;	    /*lint !e801*/  /* use of goto is deprecated */
    case tk_uconst:
	tp = tp_uint;
	goto const1;	    /*lint !e801*/  /* use of goto is deprecated */
    case tk_lconst:
	tp = tp_long;
	goto const1;	    /*lint !e801*/  /* use of goto is deprecated */
    case tk_ulconst:
	tp = tp_ulong;
	goto const1;	    /*lint !e801*/  /* use of goto is deprecated */
    case tk_llconst:
	tp = tp_longlong;
	goto const1;	    /*lint !e801*/  /* use of goto is deprecated */
    case tk_ullconst:
	tp = tp_ulonglong;
	goto const1;	    /*lint !e801*/  /* use of goto is deprecated */
    case tk_wconst:
	tp = tp_wchar;
      const1:
	ep = mk_icon ((IVAL) ival, tp);
	getsym ();
	break;
#ifdef FLOAT_SUPPORT
    case tk_fconst:
	tp = tp_float;
	goto const2;	    /*lint !e801*/  /* use of goto is deprecated */
    case tk_rconst:
	tp = tp_double;
	goto const2;	    /*lint !e801*/  /* use of goto is deprecated */
    case tk_lrconst:
	tp = tp_longdouble;
      const2:
	ep = mk_fcon (&rval, tp);
	getsym ();
	break;
#endif /* FLOAT_SUPPORT */
    case tk_wsconst:
	tp = tp_wstring;
	goto string;	    /*lint !e801*/  /* use of goto is deprecated */
    case tk_sconst:
	tp = tp_string;
      string:
	if (sizeof_flag) {
	    ep = mk_node (en_size, NIL_EXPR, NIL_EXPR, tp);
	    ep->v.i = (IVAL) lastsymlen + 1L;
	} else {
	    ep = mk_stringlit (lastsym, (size_t) lastsymlen);
	    ep->etp = tp;
	}
	getsym ();
	break;
    case tk_openpa:
	getsym ();
	if (is_type_name (lastst)) {
	    tp = type_name ();
	    needpunc (tk_closepa);
	    switch (lastst) {
	    case tk_begin:
		if (lang_option >= LANG_C99) {
		    /*
		     *      A postfix expression that consists of a
		     *      parenthesized type name followed by a
		     *      brace-enclosed list of initializers is a
		     *      compound literal.  It provides an unnamed
		     *      object whose value is given by the initializer
		     *      list.
		     *
		     *      The type name shall specify an object type or
		     *      an array of unknown size, but not a variable
		     *      length array type.
		     */
#ifndef SYNTAX_CORRECT
		    if (is_object_type (tp)) {
			if (is_variable_array_type (tp)) {
			    message (ERR_VARRAY);
			}
		    } else {
			message (ERR_OBJECT);
		    }
#endif	/* SYNTAX_CORRECT */

		    getsym ();
		    /*
		     *      If the compound literal occurs outside the body
		     *      of a function, the object has static storage
		     *      duration; otherwise, it has automatic storage
		     *      duration associated with the enclosing block.
		     */
		    ep = initializer_list (tp, 0L,
					  (global_flag ? sc_static : sc_auto));
		    ep = mk_node (en_literal, ep, mk_size (tp->size, tp_size),
				 tp);
		    if (lastst == tk_comma)
			getsym ();
		    needpunc (tk_end);
		    return ep;
		}
		/*lint -fallthrough */
	    default:
		ep = unary ();
#ifndef SYNTAX_CORRECT
		if (ep == NIL_EXPR) {
		    message (ERR_IDEXPECT);
		    return NIL_EXPR;
		}
#endif /* SYNTAX_CORRECT */
		ep = explicit_castop (ep, tp);
		break;
	    }
	} else {
	    ep = expression ();
	    needpunc (tk_closepa);
	    if (ep == NIL_EXPR) {
		return NIL_EXPR;
	    }
	}
	break;
    default:
	return NIL_EXPR;
    }
    for (;;) {
	switch (lastst) {
	case tk_openbr:	/* build a subscript reference */
	    getsym ();
	    /*
	     * a[b] is defined as *(a+b), such exactly one of (a,b) must be a
	     * pointer and one of (a,b) must be an integer expression
	     */
	    if (is_pointer_type (ep->etp)) {
		ep2 = expression ();
		ep1 = ep;
	    } else {
		ep2 = ep;
		ep1 = expression ();
		ep = ep1;
#ifndef SYNTAX_CORRECT
		if (is_char (ep->etp)) {
		    message (WARN_CHARINDEX);
		}
#endif /* SYNTAX_CORRECT */
	    }

	    /*
	     * now, ep1 describes the pointer,
	     *              ep2 describes the integral value
	     */
	    check_integral (ep2);

	    tp = ep1->etp;
	    if (is_pointer_type (tp)) {
		tp = referenced_type (tp);
#ifndef SYNTAX_CORRECT
		check_complete (tp);
	    } else {
		message (ERR_NOPOINTER);
#endif /* SYNTAX_CORRECT */
	    }

        // Treat immediate array index as signed, not unsigned like code below. - WSF
        tp1 = is_same_size (tp_pointer, tp_ulong) ? tp_long : tp_short;
	    //tp1 = is_same_size (tp_pointer, tp_ulong) ? tp_ulong : tp_ushort;

	    ep = mk_icon ((IVAL) tp->size, tp1);
	    /*
	     * ep is the size of the referenced object
	     */
	    ep2 = explicit_castop (ep2, tp1);
	    ep = mk_node (en_mul, ep, ep2, tp1);
	    ep = explicit_castop (ep, tp1);
	    ep = mk_node (en_add, ep, ep1, ep1->etp);
	    ep = cond_deref (ep, tp);
	    needpunc (tk_closebr);
	    break;
	case tk_pointsto:
	    check_set (ep);
	    check_sequence_accessed (ep);
	    tp = ep->etp;
	    if (is_pointer_type (tp)) {
		tp = referenced_type (tp);
#ifndef SYNTAX_CORRECT
	    } else {
		message (ERR_NOPOINTER);
#endif /* SYNTAX_CORRECT */
	    }
	    /*
	     * tp->type should be bt_struct or bt_union
	     * the ref node will be stripped off in a minute
	     */
	    ep = cond_deref (ep, tp);
	    /*lint -fallthrough */
	case tk_dot:
	    getsym ();		/* past -> or . */
	    tp = ep->etp;
	    if (lastst == tk_id) {
		if (is_structure_type (tp) &&
		    (members (tp) != NIL_BLOCK) &&
		    ((sp = search (lastsym, &(members (tp)->symbols))) !=
		     NIL_SYM)) {
		    /* strip off the en_ref node on top */
		    if (ep->nodetype == en_ref) {
			ep = ep->v.p[0];
			ep->etp = tp_pointer;
		    } else {
			ep = mk_node (en_deref, ep, NIL_EXPR, tp_pointer);
		    }
		    tp = qualify_type (typeof (sp),
				       (QUALIFIER) (tp->qual |
						    typeof (sp)->qual));
		    ep1 = mk_icon (sp->value.i, tp_long);
		    ep = mk_node (en_add, ep, ep1, mk_type (tp_pointer, tp));
		    ep = cond_deref (ep, tp);
#ifndef SYNTAX_CORRECT
		} else {
		    message (ERR_NOMEMBER, lastsym);
#endif /* SYNTAX_CORRECT */
		}
		getsym ();	/* past id */
#ifndef SYNTAX_CORRECT
	    } else {
		message (ERR_IDEXPECT);
#endif /* SYNTAX_CORRECT */
	    }
	    break;
	case tk_openpa:	/* function reference */
	    getsym ();
	    tp = ep->etp;
	    if (is_function_type (tp)) {
		/*
		 * the '*' may be omitted with pointers to functions
		 * we have included another indirection (see above,
		 * case id:)
		 */
		if (is_pointer_type (tp)) {
		    tp = referenced_type (tp);
		}
		/*
		 * This hack lets us remember that this function itself
		 * calls other functions.
		 * The code generator might use this information to
		 * generate safer register-pop-off code.
		 */
		is_leaf_function = FALSE;

		ep2 = parmlist (ep, parameters (tp));
		sequence_point ();
		tp1 = tp;
		tp = returned_type (tp);
#ifndef SYNTAX_CORRECT
		if (tp == NIL_TYP) {
		    tp = tp_void;	/* this happens on an error path! */
		}
		if (is_structure_type (tp)) {
		    check_complete (tp);
		}
#endif /* SYNTAX_CORRECT */
		switch (funckind (tp1))
		{
		case fc_normal:
		    ep = mk_node (en_fcall, ep, ep2, tp);
		    break;
		case fc_user:
		    ep = mk_enode (en_str, tp_pointer);
		    ep->v.str = funccall (tp1);
		    ep = mk_node (en_usercall, ep, ep2, tp);
		    break;
		default:
		    CANNOT_REACH_HERE ();
		}
		needpunc (tk_closepa);
#ifndef SYNTAX_CORRECT
	    } else {
		message (ERR_NOFUNC);
#endif /* SYNTAX_CORRECT */
	    }
	    break;
	case tk_autodec:
	case tk_autoinc:
	    check_set (ep);
	    check_sequence_modified (ep);
	    check_modifiable_lvalue (ep);
	    check_scalar (ep);
	    tp = ep->etp;
	    check_complete (tp);
#ifdef FLOAT_SUPPORT
	    if (is_real_floating_type (tp)) {
		ep1 = mk_fcon (&F_one, tp);
	    } else
#endif
	    {
		SIZE    size;

		if (is_pointer_type (tp)) {
		    check_object (ep);
		    check_complete (referenced_type (tp));
		    size = referenced_type (tp)->size;
		} else {
		    size = 1L;
		}
		ep1 = mk_icon ((IVAL) size, tp_longlong);
	    }
	    if (lastst == tk_autodec) {
		ep = mk_node (en_adec, ep, ep1, tp);
	    } else {
		ep = mk_node (en_ainc, ep, ep1, tp);
	    }
	    getsym ();
	    if ((lang_option == LANG_KANDR) && lastst == tk_pointsto) {
		needpunc ((TOKEN) (lastst + 1));	/* force an error as K&R doesn't allow this */
	    }
	    break;
	default:
	    return ep;
	}
    }
}

/*
 * unary evaluates unary expressions and returns the type of the expression
 * evaluated. unary expressions are any of:
 *
 * primary
 * !unary
 * ~unary
 * ++unary
 * --unary
 * +unary
 * -unary
 * *unary
 * &unary
 * (typecast)unary
 * sizeof(typecast)
 * sizeof unary
 *
 */
static EXPR *unary P0 (void)
{
    TYP    *tp;
    EXPR   *ep, *ep1;
    BOOL    flag = FALSE;
    SIZE    size;
#ifdef TYPEOF
    TYP    *typeof_tp;
#endif /* TYPEOF */

    switch (lastst) {
    case tk_autodec:
	flag = TRUE;
	/*lint -fallthrough */
    case tk_autoinc:
	getsym ();
	ep = unary ();
#ifndef SYNTAX_CORRECT
	if (ep == NIL_EXPR) {
	    message (ERR_IDEXPECT);
	    return NIL_EXPR;
	}
#endif /* SYNTAX_CORRECT */
	check_modifiable_lvalue (ep);
	check_scalar (ep);
	check_sequence_modified (ep);
	tp = ep->etp;
	check_complete (tp);
#ifdef FLOAT_SUPPORT
	if (is_real_floating_type (tp)) {
	    ep1 = mk_fcon (&F_one, tp);
	} else
#endif /* FLOAT_SUPPORT */
	{
	    if (is_pointer_type (tp)) {
		check_object (ep);
		check_complete (referenced_type (tp));
		size = referenced_type (tp)->size;
	    } else {
		size = 1L;
	    }
	    ep1 = mk_size (size, tp_long);
	}
	ep = mk_node ((EXPRTYPE) (flag ? en_assub : en_asadd), ep, ep1, tp);
	break;
    case tk_plus:
	if (lang_option == LANG_KANDR) {
	    /* only ANSI C allows the unary + operator */
	    ep = primary ();
	    break;
	}
	flag = TRUE;
	/*lint -fallthrough */
    case tk_minus:
	getsym ();
	ep = unary ();
#ifndef SYNTAX_CORRECT
	if (ep == NIL_EXPR) {
	    message (ERR_IDEXPECT);
	    return NIL_EXPR;
	}
	check_arithmetic (ep);
	check_set (ep);
	check_sequence_accessed (ep);
	if (!flag && is_unsigned_type (ep->etp)) {
	    message (WARN_MINUS);
	}
#endif /* SYNTAX_CORRECT */
	ep = integral_promotion (ep);

	if (!flag) {
	    /* don't bother about unary plus */
	    ep = mk_node (en_uminus, ep, NIL_EXPR, ep->etp);
	}
	break;
    case tk_not:
	getsym ();
	ep = unary ();
#ifndef SYNTAX_CORRECT
	if (ep == NIL_EXPR) {
	    message (ERR_IDEXPECT);
	    return NIL_EXPR;
	}
	check_scalar (ep);
	check_set (ep);
	check_sequence_accessed (ep);
	if (is_constexpr (ep)) {
	    message (WARN_NOT);
	}
#endif /* SYNTAX_CORRECT */
	ep = mk_node (en_not, ep, NIL_EXPR, tp_int);
	break;
    case tk_compl:
	getsym ();
	ep = unary ();
#ifndef SYNTAX_CORRECT
	if (ep == NIL_EXPR) {
	    message (ERR_IDEXPECT);
	    return NIL_EXPR;
	}
	check_integral (ep);
	check_set (ep);
	check_sequence_accessed (ep);
#endif /* SYNTAX_CORRECT */
	ep = integral_promotion (ep);
#ifndef SYNTAX_CORRECT
	if (is_signed_type (ep->etp)) {
	    message (WARN_UNDEFINED, "~");
	}
#endif /* SYNTAX_CORRECT */
	ep = mk_node (en_compl, ep, NIL_EXPR, ep->etp);
	break;
    case tk_star:
	getsym ();
	ep = unary ();
#ifndef SYNTAX_CORRECT
	if (ep == NIL_EXPR) {
	    message (ERR_IDEXPECT);
	    return NIL_EXPR;
	}
	check_set (ep);
	check_sequence_accessed (ep);
#endif /* SYNTAX_CORRECT */
	tp = ep->etp;
	if (referenced_type (tp)) {
	    /*
	     * ANSI C specifies that the function identifier will be converted
	     * to a pointer to function if necessary.
	     */
	    if (!is_func (tp)) {
		tp = referenced_type (tp);
	    }
#ifndef SYNTAX_CORRECT
	} else {
	    message (ERR_DEREF);
#endif /* SYNTAX_CORRECT */
	}
	ep = cond_deref (ep, tp);
	break;
    case tk_and:
	getsym ();
	address_flag = TRUE;
	ep = unary ();
	address_flag = FALSE;
#ifndef SYNTAX_CORRECT
	if (ep == NIL_EXPR) {
	    message (ERR_IDEXPECT);
	    return NIL_EXPR;
	}
#endif /* SYNTAX_CORRECT */
	if (is_function_type (ep->etp) && is_derived_type (ep->etp)) {
	    if (is_func (ep->etp)) {
		/* functions are implicitly converted to pointers to functions */
		ep->etp = mk_type (tp_pointer, ep->etp);
	    }
	} else if (is_lvalue (ep)) {
#ifndef SYNTAX_CORRECT
	    if (ep->nodetype == en_fieldref) {
		message (ERR_BITFIELD);
		break;
	    }
#endif /* SYNTAX_CORRECT */
	    tp = ep->etp;
	    ep = ep->v.p[0];
	    ep->etp = mk_type (tp_pointer, tp);
	    if (ep->nodetype == en_size) {
		ep->v.i = (IVAL) ep->etp->size;
	    }
#ifdef CPU_DEFINED
	    /*
	     * possibly remove a variable which is a candidate for a
	     * register.  This is done in the analyze module normally,
	     * but the use of the address operator may be hidden due
	     * to optimisations.
	     * imagine '(char *)(&len) + 2'
	     */
	    if (is_sym (ep) &&
		(is_auto (ep->v.sp) ||
		 is_parms (ep->v.sp) || is_register (ep->v.sp))) {
		deloptinfo (ep);
	    }
#endif /* CPU_DEFINED */
	} else if (is_pointer_type (ep->etp)) {
	    if (is_func (referenced_type (ep->etp))) {
#ifndef SYNTAX_CORRECT
		message (WARN_ADDFUNC);
#endif /* SYNTAX_CORRECT */
	    } else if (is_array_type (ep->etp)) {
		ep->etp = mk_type (tp_pointer, ep->etp);
#ifndef SYNTAX_CORRECT
	    } else {
		message (ERR_LVALUE);
#endif /* SYNTAX_CORRECT */
	    }
#ifndef SYNTAX_CORRECT
	} else {
	    message (ERR_LVALUE);
#endif /* SYNTAX_CORRECT */
	}
	break;
    case kw_sizeof:
	getsym ();
	/*
	 * This is a mess.
	 * Normally, we treat array names just as pointers, but with sizeof,
	 * we want to get the real array size. So, the 'real' size is stored
	 * in an en_size node in v.i
	 */
	if (lastst == tk_openpa) {
	    getsym ();
	    if (is_type_name (lastst)) {
		tp = type_name ();
#ifndef SYNTAX_CORRECT
		if (is_func (tp)) {
		    message (ERR_ILLSIZEOF);
		}
#endif /* SYNTAX_CORRECT */
		size = tp->size;
		ep = NIL_EXPR;
	    } else {
		/* '(' met -- so any expression allowed */
		sizeof_flag++;
		ep = expression ();
		sizeof_flag--;
		if (ep == NIL_EXPR) {
#ifndef SYNTAX_CORRECT
		    message (ERR_ILLSIZEOF);
#endif /* SYNTAX_CORRECT */
		    size = (SIZE) 1;
		} else if (ep->nodetype == en_size) {
		    size = (SIZE) ep->v.i;
		} else {
		    size = ep->etp->size;
		}
	    }
	    needpunc (tk_closepa);
	} else {
	    sizeof_flag++;
	    ep = unary ();
	    sizeof_flag--;
#ifndef SYNTAX_CORRECT
	    if (ep == NIL_EXPR) {
		message (ERR_ILLSIZEOF);
		size = (SIZE) 1;
	    } else
#endif /* SYNTAX_CORRECT */
	    if (ep->nodetype == en_size) {
		size = (SIZE) ep->v.i;
	    } else {
		size = ep->etp->size;
	    }
	}
	check_sizeof (ep, size);
	ep = mk_size (size, tp_size);
	break;
#ifdef TYPEOF
    case kw_typeof:
	getsym ();
	/*
	 * This is a mess.
	 * Normally, we treat array names just as pointers, but with sizeof,
	 * we want to get the real array size. So, the 'real' size is stored
	 * in an en_size node in v.i
	 */
	if (lastst == tk_openpa) {
	    getsym ();
	    if (is_type_name (lastst)) {
		tp = type_name ();
#ifndef SYNTAX_CORRECT
		if (is_func (tp)) {
		    message (ERR_ILLTYPEOF);
		}
#endif /* SYNTAX_CORRECT */
		typeof_tp = tp;
		ep = NIL_EXPR;
	    } else {
		/* '(' met -- so any expression allowed */
		ep = expression ();
		if (ep == NIL_EXPR) {
#ifndef SYNTAX_CORRECT
		    message (ERR_ILLTYPEOF);
#endif /* SYNTAX_CORRECT */
		    typeof_tp = tp_void;
		} else {
		    typeof_tp = ep->etp;
		}
	    }
	    needpunc (tk_closepa);
	} else {
	    ep = unary ();
#ifndef SYNTAX_CORRECT
	    if (ep == NIL_EXPR) {
		message (ERR_ILLTYPEOF);
		typeof_tp = tp_void;
	    } else
#endif /* SYNTAX_CORRECT */
	    {
		typeof_tp = ep->etp;
	    }
	}
	if (is_function_type (typeof_tp))
	    typeof_tp = tp_func;
	ep = mk_icon ((IVAL)typeof_tp->type, tp_int);
	break;
#endif /* TYPEOF */
    default:
	ep = primary ();
	break;
    }
    ep = opt0 (ep);
    return ep;
}

/*
 * arithmetic_conversion will coerce the nodes passed into compatible
 * types and return the type of the resulting expression.
 */
static TYP *arithmetic_conversion P2 (EXPR **, node1, EXPR **, node2)
{
    EXPR   *ep1 = *node1;
    EXPR   *ep2 = *node2;
    TYP    *tp1 = ep1->etp;
    TYP    *tp2 = ep2->etp;
    TYP    *tp;

    /* pointers may be combined with integer constant 0 */
    ep1 = opt0 (ep1);
    ep2 = opt0 (ep2);
    if (is_pointer_type (tp1) && is_null_pointer (ep2)) {
	return tp1;
    }
    if (is_pointer_type (tp2) && is_null_pointer (ep1)) {
	return tp2;
    }
    if (is_pointer_type (tp1) && is_pointer_type (tp2)) {
	return tp1;
    }
    /*
     * if either operand has type long double, the other operand is
     * converted to long double.
     */
    tp = tp_longdouble;
    if (is_same_type (tp1, tp) || is_same_type (tp2, tp)) {
	*node2 = implicit_castop (ep2, tp);
	*node1 = implicit_castop (ep1, tp);
	return tp;
    }
    /*
     * if either operand has type double the other operand is converted
     * to double.
     */
    tp = tp_double;
    if (is_same_type (tp1, tp) || is_same_type (tp2, tp)) {
	*node2 = implicit_castop (ep2, tp);
	*node1 = implicit_castop (ep1, tp);
	return tp;
    }
    /*
     * if either operand has type float the other operand is converted
     * to float.
     *
     * However K&R mandates that all floating point arithmetic be done as
     * doubles.
     */
    tp = tp_float;
    if (is_same_type (tp1, tp) || is_same_type (tp2, tp)) {
	if (lang_option == LANG_KANDR) {
	    tp = tp_double;
	}
	*node2 = implicit_castop (ep2, tp);
	*node1 = implicit_castop (ep1, tp);
	return tp;
    }
    /*
     * The integral promotions are performed on both operands.
     * If an int can represent all values of the original type, the value is
     * converted to an int; otherwise it is converted to an unsigned int.
     */
    ep1 = integral_promotion (ep1);
    tp1 = ep1->etp;
    ep2 = integral_promotion (ep2);
    tp2 = ep2->etp;

    /* 
     * is both operands have the same type, then no further
     * conversion is needed.
     */
    if (is_same_type (tp1, tp2)) {
	*node2 = ep2;
	*node1 = ep1;
	return tp1;
    }
    /*
     *if either operand has type unsigned long long int, the other operand
     * is converted to unsigned long long int
     */
    tp = tp_ulonglong;
    if (is_same_type (tp1, tp) || is_same_type (tp2, tp)) {
	*node2 = implicit_castop (ep2, tp);
	*node1 = implicit_castop (ep1, tp);
	return tp;
    }
    /*
     *if either operand has type long long int, the other operand
     * is converted to long long int
     */
    tp = tp_longlong;
    if (is_same_type (tp1, tp) || is_same_type (tp2, tp)) {
	*node2 = implicit_castop (ep2, tp);
	*node1 = implicit_castop (ep1, tp);
	return tp;
    }
    /*
     * if either operand has type unsigned long int, the other operand is
     * converted to unsigned long int.
     */
    tp = tp_ulong;
    if (is_same_type (tp1, tp) || is_same_type (tp2, tp)) {
	*node2 = implicit_castop (ep2, tp);
	*node1 = implicit_castop (ep1, tp);
	return tp;
    }
    /*
     * if one operand has type long int and the other has type unsigned int,
     * if a long int can represent all values of an unsigned int, the operand
     * of type unsigned int is converted to long int;  if a long int cannot
     * represent all the values of an unsigned int, both operands are converted
     * to unsigned long int.
     */
    if ((tp1->type == bt_long && is_same_type (tp2, tp_uint)) ||
	(tp2->type == bt_long && is_same_type (tp1, tp_uint))) {
	tp = (tp_long->size > tp_uint->size) ? tp_long : tp_ulong;
	*node2 = implicit_castop (ep2, tp);
	*node1 = implicit_castop (ep1, tp);
	return tp;
    }
    /*
     * if either operand has type long int, the other operand is
     * converted to long int.
     */
    tp = tp_long;
    if (is_same_type (tp1, tp) || is_same_type (tp2, tp)) {
	*node2 = implicit_castop (ep2, tp);
	*node1 = implicit_castop (ep1, tp);
	return tp;
    }
    /*
     * if either operand has type unsigned int, the other operand is
     * converted to unsigned int.
     */
    tp = tp_uint;
    if (is_same_type (tp1, tp) || is_same_type (tp2, tp)) {
	*node2 = implicit_castop (ep2, tp);
	*node1 = implicit_castop (ep1, tp);
	return tp;
    }
    /*
     * both operands have type int
     */
    tp = tp_int;
    *node2 = implicit_castop (ep2, tp);
    *node1 = implicit_castop (ep1, tp);
    return tp;
}

/*
 * ,,arithmetic_conversion'' for comparisons:
 * When comparing two char's, it is not necessary to cast
 * both of them to long in advance
 *
 * Perhaps not strictly K&R, but more efficient.
 * If you don't like it, use arithmetic_conversion in ALL cases
 */
static TYP *arithmetic_conversion2 P2 (EXPR **, node1, EXPR **, node2)
{
    TYP    *tp1 = (*node1)->etp;
    TYP    *tp2 = (*node2)->etp;
    TYP    *tp;

    /* short cut: */
    if (is_same_type (tp1, tp2)) {
	return tp1;
    }
    /* comparison with integer constant */
    *node1 = opt0 (*node1);
    *node2 = opt0 (*node2);
    if (is_constexpr (*node1)) {
	EXPR  **node = node1;

	tp = tp1;
	node1 = node2;
	tp1 = tp2;
	node2 = node;
	tp2 = tp;
    }
    if (is_constexpr (*node2)) {
#ifndef SYNTAX_CORRECT
	if (is_pointer_type (tp2) && !is_pointer_type (tp1)
	    && !is_null_pointer (*node2)) {
	    message (ERR_MISMATCH);
	}
#endif /* SYNTAX_CORRECT */
	if (is_constant_in_range (*node2, tp1)) {
	    tp = tp1;
	    *node2 = implicit_castop (*node2, tp);
	    return tp;
	}
    }
    switch (tp1->type) {
	/* Type of first operand */
    case bt_char:
    case bt_schar:
	switch (tp2->type) {
	case bt_char:
	case bt_schar:
	    return tp2;
	case bt_charu:
	case bt_uchar:
	    tp = tp_short;
	    *node1 = implicit_castop (*node1, tp);
	    *node2 = implicit_castop (*node2, tp);
	    return tp;
	case bt_short:
	case bt_ushort:
	case bt_int16:
	case bt_uint16:
	case bt_int32:
	case bt_uint32:
	case bt_long:
	case bt_ulong:
	case bt_longlong:
	case bt_ulonglong:
	case bt_float:
	case bt_double:
	case bt_longdouble:
	    tp = tp2;
	    *node1 = implicit_castop (*node1, tp);
	    return tp;
	default:
	    break;
	}
	break;
    case bt_charu:
    case bt_uchar:
	switch (tp2->type) {
	case bt_charu:
	case bt_uchar:
	    return tp2;
	case bt_char:
	case bt_schar:
	    tp = tp_short;
	    *node1 = implicit_castop (*node1, tp);
	    *node2 = implicit_castop (*node2, tp);
	    return tp;
	case bt_short:
	case bt_ushort:
	case bt_int16:
	case bt_uint16:
	case bt_int32:
	case bt_uint32:
	case bt_long:
	case bt_ulong:
	case bt_longlong:
	case bt_ulonglong:
	case bt_float:
	case bt_double:
	case bt_longdouble:
	    tp = tp2;
	    *node1 = implicit_castop (*node1, tp);
	    return tp;
	default:
	    break;
	}
	break;
    case bt_short:
    case bt_ushort:
    case bt_int16:
    case bt_uint16:
	switch (tp2->type) {
	case bt_char:
	case bt_charu:
	case bt_uchar:
	case bt_schar:
	    tp = tp1;
	    *node2 = implicit_castop (*node2, tp);
	    return tp;
	case bt_ushort:
	case bt_uint16:
	    if (tp_int->size == tp2->size) {
		tp = tp2;
		*node1 = implicit_castop (*node1, tp);
	    } else {
		tp = tp_long;
		*node1 = implicit_castop (*node1, tp);
		*node2 = implicit_castop (*node2, tp);
	    }
	    return tp;
	case bt_short:
	case bt_int16:
	    if (tp_int->size == tp2->size) {
		tp = tp_ushort;
		*node2 = implicit_castop (*node2, tp);
	    } else {
		tp = tp_long;
		*node1 = implicit_castop (*node1, tp);
		*node2 = implicit_castop (*node2, tp);
	    }
	    return tp;
	case bt_int32:
	case bt_uint32:
	case bt_long:
	case bt_ulong:
	case bt_longlong:
	case bt_ulonglong:
	case bt_float:
	case bt_double:
	case bt_longdouble:
	    tp = tp2;
	    *node1 = implicit_castop (*node1, tp);
	    return tp;
	default:
	    break;
	}
	break;
	/*
	 * pointers are equivalent to function names
	 */
    case bt_pointer16:
    case bt_pointer32:
	if (is_func (tp2)) {
	    tp = tp1;
	    *node2 = implicit_castop (*node2, tp);
	    return tp;
	}
	/*lint -fallthrough */
    case bt_func:
	if (is_pointer_type (tp2)) {
	    *node1 = implicit_castop (*node1, tp2);
	}
	/*lint -fallthrough */
    case bt_int32:
    case bt_uint32:
    case bt_long:
    case bt_ulong:
	switch (tp2->type) {
	case bt_char:
	case bt_charu:
	case bt_uchar:
	case bt_schar:
	case bt_short:
	case bt_ushort:
	case bt_int16:
	case bt_uint16:
	case bt_pointer16:
	case bt_int32:
	case bt_long:
	    tp = tp1;
	    *node2 = implicit_castop (*node2, tp);
	    return tp;
	case bt_uint32:
	case bt_ulong:
	case bt_pointer32:
	case bt_longlong:
	case bt_ulonglong:
	case bt_float:
	case bt_double:
	case bt_longdouble:
	    tp = tp2;
	    *node1 = implicit_castop (*node1, tp);
	    return tp;
	default:
	    break;
	}
	break;
    case bt_longlong:
    case bt_ulonglong:
	switch (tp2->type) {
	case bt_char:
	case bt_charu:
	case bt_uchar:
	case bt_schar:
	case bt_short:
	case bt_ushort:
	case bt_int16:
	case bt_uint16:
	case bt_pointer16:
	case bt_int32:
	case bt_long:
	    tp = tp1;
	    *node2 = implicit_castop (*node2, tp);
	    return tp;
	case bt_uint32:
	case bt_ulong:
	case bt_pointer32:
	case bt_longlong:
	case bt_ulonglong:
	case bt_float:
	case bt_double:
	case bt_longdouble:
	    tp = tp2;
	    *node1 = implicit_castop (*node1, tp);
	    return tp;
	default:
	    break;
	}
	break;
    case bt_float:
	switch (tp2->type) {
	case bt_char:
	case bt_charu:
	case bt_uchar:
	case bt_schar:
	case bt_short:
	case bt_ushort:
	case bt_int16:
	case bt_uint16:
	case bt_int32:
	case bt_uint32:
	case bt_long:
	case bt_ulong:
	case bt_longlong:
	case bt_ulonglong:
	    tp = tp1;
	    *node2 = implicit_castop (*node2, tp);
	    return tp;
	case bt_float:
	case bt_double:
	case bt_longdouble:
	    tp = tp2;
	    *node1 = implicit_castop (*node1, tp);
	    return tp;
	default:
	    break;
	}
	break;
    case bt_double:
	switch (tp2->type) {
	case bt_char:
	case bt_charu:
	case bt_uchar:
	case bt_schar:
	case bt_short:
	case bt_ushort:
	case bt_int16:
	case bt_uint16:
	case bt_int32:
	case bt_uint32:
	case bt_long:
	case bt_ulong:
	case bt_longlong:
	case bt_ulonglong:
	case bt_float:
	    tp = tp1;
	    *node2 = implicit_castop (*node2, tp);
	    return tp;
	case bt_double:
	case bt_longdouble:
	    tp = tp2;
	    *node1 = implicit_castop (*node1, tp);
	    return tp;
	default:
	    break;
	}
	break;
    case bt_longdouble:
	switch (tp2->type) {
	case bt_char:
	case bt_charu:
	case bt_uchar:
	case bt_schar:
	case bt_short:
	case bt_ushort:
	case bt_int16:
	case bt_uint16:
	case bt_int32:
	case bt_uint32:
	case bt_long:
	case bt_ulong:
	case bt_longlong:
	case bt_ulonglong:
	case bt_float:
	case bt_double:
	case bt_longdouble:
	    tp = tp2;
	    *node1 = implicit_castop (*node1, tp);
	    return tp;
	default:
	    break;
	}
	break;
    default:
	break;
    }
#ifndef SYNTAX_CORRECT
    message (ERR_MISMATCH);
#endif /* SYNTAX_CORRECT */
    return tp1;
}


/*
 * multops parses the multiply priority operators. the syntax of this group
 * is:
 *
 * unary multop * unary multop / unary multop % unary
 */
static EXPR *multops P0 (void)
{
    EXPR   *ep1, *ep2;
    EXPRTYPE nt;
    TYP    *tp;

    ep1 = unary ();
    if (ep1 == NIL_EXPR) {
	return NIL_EXPR;
    }
    for (;;) {
	switch (lastst) {
	case tk_star:
	    nt = en_mul;
	    break;
	case tk_divide:
	    nt = en_div;
	    break;
	case tk_mod:
	    nt = en_mod;
	    break;
	default:
	    ep1 = opt0 (ep1);
	    return ep1;
	}
	getsym ();		/* move on to next unary op */
	ep2 = unary ();
	if (ep2) {
	    switch (nt) {
	    case en_div:
		check_zero (ep2);
		/*lint -fallthrough */
	    case en_mul:
		check_arithmetic (ep1);
		check_arithmetic (ep2);
		break;
	    case en_mod:
		check_zero (ep2);
		check_integral (ep1);
		check_integral (ep2);
		break;
	    default:
		CANNOT_REACH_HERE ();
		break;
	    }
	    check_set (ep1);
	    check_set (ep2);
	    check_sequence_accessed (ep1);
	    check_sequence_accessed (ep2);
	    tp = arithmetic_conversion (&ep1, &ep2);
	    ep1 = mk_node (nt, ep1, ep2, tp);
#ifndef SYNTAX_CORRECT
	} else {
	    message (ERR_IDEXPECT);
#endif /* SYNTAX_CORRECT */
	}
    }
}

/*
 * addops handles the addition and subtraction operators.
 */
static EXPR *addops P0 (void)
{
    EXPR   *ep1, *ep2, *ep3;
    TYP    *tp;
    TYP    *tp1 = is_same_size (tp_pointer, tp_ulong) ? tp_long : tp_short;
    EXPRTYPE nt;

    ep1 = multops ();
#ifndef SYNTAX_CORRECT
    if (ep1 == NIL_EXPR) {
	return NIL_EXPR;
    }
#endif /* SYNTAX_CORRECT */
    for (;;) {
	switch (lastst) {
	case tk_plus:
	    nt = en_add;
	    break;
	case tk_minus:
	    nt = en_sub;
	    break;
	default:
	    ep1 = opt0 (ep1);
	    return ep1;
	}
	getsym ();
	ep2 = multops ();
#ifndef SYNTAX_CORRECT
	if (ep2 == NIL_EXPR) {
	    message (ERR_IDEXPECT);
	    return ep2;
	}
	check_set (ep1);
	check_set (ep2);
	check_sequence_accessed (ep1);
	check_sequence_accessed (ep2);
#endif /* SYNTAX_CORRECT */
	if (is_pointer_type (ep1->etp)) {
	    if ((nt == en_sub) && is_pointer_type (ep2->etp)) {
		/* pointer subtraction */
#ifndef SYNTAX_CORRECT
		if (!is_compatible_type (ep1->etp, ep2->etp)) {
		    message (WARN_POINTER);
		}
#endif /* SYNTAX_CORRECT */
		check_object (ep1);
		check_complete (ep1->etp);
		check_object (ep2);
		check_complete (ep2->etp);
		ep1 = mk_node (nt, ep1, ep2, tp1);
		/* divide the result by the size */
		ep2 = mk_icon ((IVAL) referenced_type (ep2->etp)->size, tp1);
		ep1 = mk_node (en_div, ep1, ep2, tp_ptrdiff);
		ep1 = explicit_castop (ep1, tp_ptrdiff);
	    } else {
		/* pointer +/- integer */
		check_integral (ep2);
		check_object (ep1);
		check_complete (referenced_type (ep1->etp));
		ep2 = explicit_castop (ep2, tp1);
		ep3 = mk_icon ((IVAL) referenced_type (ep1->etp)->size, tp1);
		ep2 = mk_node (en_mul, ep3, ep2,
			       (is_same_size (tp_pointer, tp_ulong) ? tp_long
				: tp_short));
		ep1 = mk_node (nt, ep1, ep2, ep1->etp);
	    }
	} else {
	    if (is_pointer_type (ep2->etp)) {
		/* integer + pointer */
#ifndef SYNTAX_CORRECT
		if (nt == en_add) {
		    check_integral (ep1);
		} else {
		    message (ERR_MINUS);
		}
		check_object (ep2);
		check_complete (ep2->etp);
#endif /* SYNTAX_CORRECT */
		ep1 = explicit_castop (ep1, tp1);
		ep3 = mk_icon ((IVAL) referenced_type (ep2->etp)->size, tp1);
		ep1 = mk_node (en_mul, ep3, ep1, tp1);
		ep1 = mk_node (nt, ep1, ep2, ep2->etp);
	    } else {
		tp = arithmetic_conversion (&ep1, &ep2);
		ep1 = mk_node (nt, ep1, ep2, tp);
	    }
	}
    }
}

/*
 * shiftop handles the shift operators << and >>.
 */
static EXPR *shiftop P0 (void)
{
    EXPR   *ep1, *ep2;
    EXPRTYPE nt;

    ep1 = addops ();
#ifndef SYNTAX_CORRECT
    if (ep1 == NIL_EXPR) {
	return NIL_EXPR;
    }
#endif /* SYNTAX_CORRECT */
    for (;;) {
	switch (lastst) {
	case tk_lshift:
	    nt = en_lsh;
#ifndef SYNTAX_CORRECT
	    if (is_signed_type (ep1->etp)) {
		message (WARN_UNDEFINED, "<<");
	    }
#endif /* SYNTAX_CORRECT */
	    break;
	case tk_rshift:
	    nt = en_rsh;
#ifndef SYNTAX_CORRECT
	    if (is_signed_type (ep1->etp)) {
		message (WARN_UNDEFINED, ">>");
	    }
#endif /* SYNTAX_CORRECT */
	    break;
	default:
	    ep1 = opt0 (ep1);
	    return ep1;
	}
	getsym ();
	ep2 = addops ();
	if (ep2) {
	    check_integral (ep1);
	    check_integral (ep2);
	    check_set (ep1);
	    check_set (ep2);
	    check_sequence_accessed (ep1);
	    check_sequence_accessed (ep2);
	    ep1 = integral_promotion (ep1);
	    check_shift (ep2, ep1->etp);
	    ep2 = integral_promotion (ep2);
	    ep1 = mk_node (nt, ep1, ep2, ep1->etp);
#ifndef SYNTAX_CORRECT
	} else {
	    message (ERR_IDEXPECT);
#endif /* SYNTAX_CORRECT */
	}
    }
}

/*
 * relation handles the relational operators < <= > and >=.
 */
static EXPR *relation P0 (void)
{
    EXPR   *ep1, *ep2;
    EXPRTYPE nt;

    ep1 = shiftop ();
#ifndef SYNTAX_CORRECT
    if (ep1 == NIL_EXPR) {
	return NIL_EXPR;
    }
#endif /* SYNTAX_CORRECT */
    for (;;) {
	switch (lastst) {

	case tk_lt:
	    nt = en_lt;
	    break;
	case tk_gt:
	    nt = en_gt;
	    break;
	case tk_leq:
	    nt = en_le;
	    break;
	case tk_geq:
	    nt = en_ge;
	    break;
	default:
	    ep1 = opt0 (ep1);
	    return ep1;
	}
	getsym ();
	ep2 = shiftop ();
	if (ep2) {
#ifndef SYNTAX_CORRECT
	    check_set (ep1);
	    check_set (ep2);
	    check_sequence_accessed (ep1);
	    check_sequence_accessed (ep2);
	    if (nt == en_lt) {
		check_relational (ep1, ep2);
	    }
#endif /* SYNTAX_CORRECT */
	    VOIDCAST arithmetic_conversion2 (&ep1, &ep2);

#ifndef SYNTAX_CORRECT
	    if (is_pointer_type (ep1->etp) && is_pointer_type (ep2->etp) &&
		!is_compatible_type (ep1->etp, ep2->etp))
		message (WARN_PTRCAST);
#endif /* SYNTAX_CORRECT */
	    ep1 = mk_node (nt, ep1, ep2, tp_int);
#ifndef SYNTAX_CORRECT
	} else {
	    message (ERR_IDEXPECT);
#endif /* SYNTAX_CORRECT */
	}
    }
}

/*
 * equalops handles the equality and inequality operators.
 */
static EXPR *equalops P0 (void)
{
    EXPR   *ep1, *ep2;
    EXPRTYPE nt;

    ep1 = relation ();
#ifndef SYNTAX_CORRECT
    if (ep1 == NIL_EXPR) {
	return NIL_EXPR;
    }
#endif /* SYNTAX_CORRECT */
    for (;;) {
	switch (lastst) {
	case tk_eq:
	    nt = en_eq;
	    break;
	case tk_neq:
	    nt = en_ne;
	    break;
	default:
	    ep1 = opt0 (ep1);
	    return ep1;
	}
	getsym ();
	ep2 = relation ();
	if (ep2) {
	    check_set (ep1);
	    check_set (ep2);
	    check_sequence_accessed (ep1);
	    check_sequence_accessed (ep2);
	    check_equality (ep1, ep2);
	    VOIDCAST arithmetic_conversion2 (&ep1, &ep2);

#ifndef SYNTAX_CORRECT
	    if (is_pointer_type (ep1->etp) && is_pointer_type (ep2->etp) &&
		!is_compatible_type (ep1->etp, ep2->etp))
		message (WARN_PTRCAST);
#endif /* SYNTAX_CORRECT */
	    ep1 = mk_node (nt, ep1, ep2, tp_int);
#ifndef SYNTAX_CORRECT
	} else {
	    message (ERR_IDEXPECT);
#endif /* SYNTAX_CORRECT */
	}
    }
}

/*
 * binop is a common routine to handle all of the leg work and error checking
 * for bitandop, bitorop, bitxor
 */
#ifdef __STDC__
static EXPR *binop (EXPR *(*xfunc) (void), EXPRTYPE nt, TOKEN sy,
		    const char *op)
#else
static EXPR *binop (xfunc, nt, sy, op)
     EXPR   *(*xfunc) P_ ((void));
     EXPRTYPE nt;
     TOKEN   sy;
     const char *op;
#endif
{
    EXPR   *ep1, *ep2;
    TYP    *tp;

    ep1 = (*xfunc) ();
#ifndef SYNTAX_CORRECT
    if (ep1 == NIL_EXPR) {
	return NIL_EXPR;
    }
#endif /* SYNTAX_CORRECT */
    while (lastst == sy) {
	getsym ();
	ep2 = (*xfunc) ();
	if (ep2) {
	    check_integral (ep1);
	    check_integral (ep2);
	    check_set (ep1);
	    check_set (ep2);
	    check_sequence_accessed (ep1);
	    check_sequence_accessed (ep2);
	    tp = arithmetic_conversion2 (&ep1, &ep2);
#ifndef SYNTAX_CORRECT
	    if (is_signed_type (ep1->etp)) {
		message (WARN_UNDEFINED, op);
	    }
#endif /* SYNTAX_CORRECT */
	    ep1 = mk_node (nt, ep1, ep2, tp);
#ifndef SYNTAX_CORRECT
	} else {
	    message (ERR_IDEXPECT);
#endif /* SYNTAX_CORRECT */
	}
    }
    ep1 = opt0 (ep1);
    return ep1;
}

/*
 * binlog is a common routine to handle all of the legwork and error checking
 * for logical and, or
 */
#ifdef __STDC__
static EXPR *binlog (EXPR *(*xfunc) (void), EXPRTYPE nt, TOKEN sy)
#else
static EXPR *binlog (xfunc, nt, sy)
     EXPR   *(*xfunc) P_ ((void));
     EXPRTYPE nt;
     TOKEN   sy;

#endif
{
    EXPR   *ep1, *ep2;

    ep1 = (*xfunc) ();
#ifndef SYNTAX_CORRECT
    if (ep1 == NIL_EXPR) {
	return NIL_EXPR;
    }
#endif /* SYNTAX_CORRECT */
    while (lastst == sy) {
	sequence_point ();
	getsym ();
	ep2 = (*xfunc) ();
	if (ep2) {
	    check_scalar (ep1);
	    check_scalar (ep2);
	    check_set (ep1);
	    check_set (ep2);
	    ep1 = condition (ep1);
	    ep2 = condition (ep2);
	    ep1 = mk_node (nt, ep1, ep2, tp_int);
#ifndef SYNTAX_CORRECT
	} else {
	    message (ERR_IDEXPECT);
#endif /* SYNTAX_CORRECT */
	}
    }
    ep1 = opt0 (ep1);
    return ep1;
}

/*
 * the bitwise and operator...
 */
static EXPR *bitandop P0 (void)
{
    return binop (equalops, en_and, tk_and, "&");
}

static EXPR *bitxor P0 (void)
{
    return binop (bitandop, en_xor, tk_uparrow, "^");
}

static EXPR *bitorop P0 (void)
{
    return binop (bitxor, en_or, tk_or, "|");
}

static EXPR *andop P0 (void)
{
    return binlog (bitorop, en_land, tk_land);
}

static EXPR *orop P0 (void)
{
    return binlog (andop, en_lor, tk_lor);
}

/*
 * this routine processes the hook operator.
 */
static EXPR *conditional P0 (void)
{
    TYP    *tp1, *tp2, *tp3;
    EXPR   *ep1, *ep2, *ep3;

    ep1 = orop ();		/* get condition */
#ifndef SYNTAX_CORRECT
    if (ep1 == NIL_EXPR) {
	return NIL_EXPR;
    }
#endif /* SYNTAX_CORRECT */
    if (lastst == tk_hook) {
	check_scalar (ep1);
	check_set (ep1);
	check_sequence_accessed (ep1);
	ep1 = condition (ep1);
	sequence_point ();
	getsym ();
	ep2 = expression ();
#ifndef SYNTAX_CORRECT
	if (ep2 == NIL_EXPR) {
	    message (ERR_IDEXPECT);
	    return NIL_EXPR;
	}
#endif /* SYNTAX_CORRECT */
	check_set (ep2);
	needpunc (tk_colon);
	ep3 = exprnc ();
#ifndef SYNTAX_CORRECT
	if (ep3 == NIL_EXPR) {
	    message (ERR_IDEXPECT);
	    return NIL_EXPR;
	}
#endif /* SYNTAX_CORRECT */
	check_set (ep3);

	ep2 = opt0 (ep2);
	ep3 = opt0 (ep3);
	tp2 = ep2->etp;
	tp3 = ep3->etp;
	if (is_arithmetic_type (tp2) && is_arithmetic_type (tp3)) {
	    /*
	     * if both the second and third operands have arithmetic type,
	     * the usual arithmetic conversions are performed to bring them
	     * to a common type and the result has that type
	     */
	    tp1 = arithmetic_conversion2 (&ep2, &ep3);
	} else if (is_structure_type (tp2) && is_structure_type (tp3) &&
		   is_compatible_type (tp2, tp3)) {
	    /*
	     * If both the operands have structure or union type, the
	     * result has that type
	     */
	    tp1 = tp2;
	} else if (is_void (tp2) && is_void (tp3)) {
	    /*
	     * If both operands have void type, the result has void type
	     */
	    tp1 = tp2;
	} else if (is_pointer_type (tp2) && is_pointer_type (tp3)) {
	    /*
	     * If both the second and third operands are pointers or one is a
	     * null pointer constant and the other is a pointer, the result
	     * type is a pointer to a type qualified with all the type
	     * qualifiers of the types pointed-to by both operands.
	     *
	     * Furthermore, if both operands are pointers to compatible
	     * types or differently qualified versions of a compatible type,
	     * the result has the composite type;  if one operand is a null
	     * pointer constant, the result has the type of the other operand;
	     * otherwise, one operand is a pointer to void or a qualified
	     * version of void, in which case the other operand is converted
	     * to type pointer to void, and the result has that type.
	     */
	    tp1 = copy_type (arithmetic_conversion2 (&ep2, &ep3));
	    if (is_compatible_type (tp2, tp3)) {
		set_referenced_type (tp1,
				     qualify_type (referenced_type (tp1),
						   (QUALIFIER)
						   (referenced_type
						    (tp2)->qual |
						    referenced_type
						    (tp3)->qual)));
#ifndef SYNTAX_CORRECT
	    } else {
		message (WARN_PTRCAST);
#endif /* SYNTAX_CORRECT */
	    }
	} else if ((is_pointer_type (tp2) && is_null_pointer (ep3)) ||
		   (is_pointer_type (tp3) && is_null_pointer (ep2))) {
	    tp1 = arithmetic_conversion2 (&ep2, &ep3);
	} else {
#ifndef SYNTAX_CORRECT
	    /*
	     * If either type is void and the other is not, cast the other
	     * one to void.  I dare not do this in arithmetic_conversion2.
	     * Strict ANSI does not allow that only one part of the sentence
	     * is void, that is what gcc -pedantic tells me.
	     * But since such conditionals occur even in GNU Software (look at
	     * obstack.h), I allow such constructs here.
	     */
	    if (is_void (tp2) && !is_void (tp3)) {
		ep3 = implicit_castop (ep3, tp2);
		message (WARN_CONDVOID, 3);
	    } else if (is_void (tp3) && !is_void (tp2)) {
		ep2 = implicit_castop (ep2, tp3);
		message (WARN_CONDVOID, 2);
	    } else {
		message (ERR_ILLTYPE);
	    }
#endif /* SYNTAX_CORRECT */
	    tp1 = arithmetic_conversion2 (&ep2, &ep3);
	}

#ifndef SYNTAX_CORRECT
	if (tp1 == NIL_TYP) {
	    return NIL_EXPR;
	}
#endif /* SYNTAX_CORRECT */
	ep2 = mk_node (en_list, ep2, ep3, tp_void);
	ep1 = mk_node (en_cond, ep1, ep2, tp1);
    }
    return ep1;
}

/*
 * asnop handles the assignment operators.
 */
static EXPR *asnop P0 (void)
{
    EXPR   *ep1, *ep2, *ep3;
    TYP    *tp;
    EXPRTYPE op;
    BOOL    implicit_cast;

    ep1 = conditional ();
#ifndef SYNTAX_CORRECT
    if (ep1 == NIL_EXPR) {
	return NIL_EXPR;
    }
#endif /* SYNTAX_CORRECT */
    for (;;) {
	switch (lastst) {
	case tk_asrshift:
	    op = en_asrsh;
	    break;
	case tk_asand:
	    op = en_asand;
	    break;
	case tk_asor:
	    op = en_asor;
	    break;
	case tk_asuparrow:
	    op = en_asxor;
	    break;
	case tk_aslshift:
	    op = en_aslsh;
	    break;
	case tk_astimes:
	    op = en_asmul;
	    break;
	case tk_asdivide:
	    op = en_asdiv;
	    break;
	case tk_asmod:
	    op = en_asmod;
	    break;
	case tk_assign:
	    op = en_assign;
	    break;
	case tk_asminus:
	    op = en_assub;
	    break;
	case tk_asplus:
	    op = en_asadd;
	    break;
	default:
	    return ep1;
	}
	getsym ();
	ep2 = asnop ();
	if (ep2 == NIL_EXPR) {
	    continue;
	}
	implicit_cast = TRUE;
	switch (op) {
	case en_aslsh:
	case en_asrsh:
	    implicit_cast = FALSE;
	    check_shift (ep2, ep1->etp);	/* should really be promoted type */
	    /*lint -fallthrough */
	case en_asand:
	case en_asor:
	case en_asxor:
	case en_asmod:
	    /* For operators which must have integral operands */
	    check_integral (ep1);
	    check_integral (ep2);
	    check_set (ep1);
	    check_sequence_modified (ep1);
	    break;

	case en_asmul:
	case en_asdiv:
	    check_arithmetic (ep1);
	    check_arithmetic (ep2);
	    check_set (ep1);
	    check_sequence_modified (ep1);
	    break;

	case en_assign:
	    check_qualifiers (ep1->etp, ep2->etp);
	    set_used (ep1);
	    break;

	case en_assub:
	case en_asadd:
	    check_set (ep1);
	    check_sequence_modified (ep1);
	    tp = ep1->etp;
	    if (is_pointer_type (tp) && is_integral_type (ep2->etp)) {
		check_object (ep1);
		check_complete (ep1->etp);
#if 1   /* ghaerr: must cast to int not long when target sizeof(void *) == 2 (8086) */
		ep2 = explicit_castop (ep2, tp_int);
		ep3 = mk_icon ((IVAL) referenced_type (tp)->size, tp_int);
                ep2 = mk_node (en_mul, ep2, ep3, tp_int);
		ep2 = explicit_castop (ep2, tp);
#else
		ep2 = explicit_castop (ep2, tp_long);
		ep3 = mk_icon ((IVAL) referenced_type (tp)->size, tp_long);
		ep2 = mk_node (en_mul, ep2, ep3, tp_long);
		ep2 = explicit_castop (ep2, tp);
#endif
	    }
	    break;

	default:
	    CANNOT_REACH_HERE ();
	    break;
	}
	check_set (ep2);
	check_sequence_accessed (ep2);

	if (ep2->etp == NIL_TYP) {
	    continue;
	}
	check_modifiable_lvalue (ep1);
	if (implicit_cast) {
	    switch (op) {
#ifdef FLOAT_SUPPORT
	    case en_asmul:
	    case en_asdiv:
		if (is_integral_type (ep1->etp)
		    && is_real_floating_type (ep2->etp)) {
		    op = (EXPRTYPE) (op + 1);
		    break;
		}
		/*lint -fallthrough */
#endif /* FLOAT_SUPPORT */
	    default:
		ep2 = implicit_castop (ep2, ep1->etp);
		break;
	    }
	} else {
	    ep2 = explicit_castop (ep2, ep1->etp);
	}
	ep1->etp = qualify_type (ep1->etp, QUAL_NONE);
	ep1 = mk_node (op, ep1, ep2, ep1->etp);
	/* trap struct assigns */
	switch (ep1->etp->type) {
	case bt_struct:
	case bt_union:
#ifdef FLOAT_SUPPORT
	    check_complete (ep1->etp);
#endif /* FLOAT_SUPPORT */
#ifdef INTEL_386
	    /*lint -fallthrough */
	case bt_float:
	case bt_double:
	case bt_longdouble:
	    uses_structassign = TRUE;
#endif
	    break;
	default:
	    break;
	}
    }
}

/*
 * evaluate an expression where the comma operator is not legal.
 */
EXPR   *exprnc P0 (void)
{
    return asnop ();
}

/*
 * evaluate the comma operator.
 */
static EXPR *commaop P0 (void)
{
    EXPR   *ep1, *ep2;

    ep1 = asnop ();
    if (ep1 == NIL_EXPR) {
	return NIL_EXPR;
    }
    if (lastst == tk_comma) {
	sequence_point ();
	getsym ();
	ep2 = commaop ();
	if (ep2) {
	    check_discard (ep1);
	    ep1 = mk_node (en_comma, ep1, ep2, ep2->etp);
#ifndef SYNTAX_CORRECT
	} else {
	    message (ERR_IDEXPECT);
#endif /* SYNTAX_CORRECT */
	}
    }
#ifdef FLOAT_CHECK
#ifdef FLOAT_SUPPORT
#ifndef SYNTAX_CORRECT
    if (fcheck_option && is_real_floating_type (ep1->etp)) {
	message (WARN_FLOAT);
    }
#endif /* SYNTAX_CORRECT */
#endif /* FLOAT_SUPPORT */
#endif /* FLOAT_CHECK */
    return ep1;
}

/*
 * evaluate an expression where all operators are legal.
 */
EXPR   *expression P0 (void)
{
    return commaop ();
}

/*
 *	 Checks to see whether there is an assignment when a condition
 *	 was expected.	 Although legal it is sometimes an error
 *	 when "=" was used instead of "==".
 */
static EXPR *condition P1 (EXPR *, ep)
{
    switch (ep->nodetype) {
    case en_land:
    case en_lor:
	ep->v.p[0] = condition (ep->v.p[0]);
	ep->v.p[1] = condition (ep->v.p[1]);
	/*lint -fallthrough */
    case en_eq:
    case en_ne:
    case en_lt:
    case en_le:
    case en_gt:
    case en_ge:
    case en_not:
    case en_test:
	return ep;
    case en_assign:
#ifndef SYNTAX_CORRECT
	message (WARN_ASSIGN);
	/*lint -fallthrough */
#endif /* SYNTAX_CORRECT */
    default:
	ep = mk_node (en_test, ep, NIL_EXPR, tp_int);
	return ep;
    }
}

/*
 *	 Evaluates a conditional expression.   All conditional expression
 *	 must be scalars.
 */
EXPR   *condition_expression P0 (void)
{
    EXPR   *ep;

    ep = commaop ();
#ifndef SYNTAX_CORRECT
    if (ep == NIL_EXPR) {
	message (ERR_EXPREXPECT);
	return ep;
    }
#endif /* SYNTAX_CORRECT */
    check_scalar (ep);
    sequence_point ();
    return condition (ep);
}

/*
 * evaluate an expression where all operators are legal but result must be
 * an integral value.
 */
EXPR   *integral_expression P0 (void)
{
    EXPR   *ep;

    ep = commaop ();
#ifndef SYNTAX_CORRECT
    if (ep == NIL_EXPR) {
	return ep;
    }
#endif /* SYNTAX_CORRECT */
    check_integral (ep);
    return ep;
}

#ifdef ASM
/*
 * evaluate an ASM expression
 */
EXPR   *asm_expression P0 (void)
{
    EXPR   *ep = NIL_EXPR;

    if (lastst == tk_sconst) {
	ep = mk_enode (en_str, tp_pointer);
	ep->v.str = lastsym;
	getsym ();
    }
    return ep;
}
#endif /* ASM */

/*
 *	 Cast the expression 'ep' to type 'tp2'.
 */
static EXPR *castop P2 (EXPR *, ep, TYP *, tp2)
{
    TYP    *tp1 = ep->etp;

    if (is_same_type (tp1, tp2)) {
#ifndef SYNTAX_CORRECT
	if (is_structure_type (tp1)) {
	    if (!is_same_size (tp1, tp2)) {
		message (ERR_CAST);
	    }
	}
#endif /* SYNTAX_CORRECT */
	/*
	 * A cast may be syntactically relevant:
	 * sizeof("long string") is something else than
	 * sizeof((char *) "long string")!
	 * So no shortcut in this case.
	 */
	if (ep->nodetype == en_size) {
	    ep->v.i = (IVAL) tp2->size;
	    return ep;
	}
#ifndef SYNTAX_CORRECT
    } else {
	switch (tp1->type) {
	case bt_void:
	    /* A cast from void to anything other than void must be an error */
	    message (ERR_ILLCAST, nameoftype (tp1), nameoftype (tp2));
	    break;
	case bt_pointer16:
	case bt_pointer32:
	    if (!is_integral_type (tp2) &&
		!is_pointer_type (tp2) && !is_void (tp2)) {
		message (ERR_ILLCAST, nameoftype (tp1), nameoftype (tp2));
	    }
	    break;
	default:
	    break;
	}
	if (!is_void (tp2)
	    && (is_structure_type (tp1) || is_structure_type (tp2))) {
	    message (ERR_ILLCAST, nameoftype (tp1), nameoftype (tp2));
	    return ep;
	}
#endif /* SYNTAX_CORRECT */
    }
    ep = mk_node (en_cast, ep, NIL_EXPR, tp2);
    return ep;
}

/*
 *	 An explicit cast is a cast requested by the programmer.
 */
static EXPR   *explicit_castop P2 (EXPR *, ep, TYP *, tp2)
{
#ifndef SYNTAX_CORRECT
    if (ep == NIL_EXPR || tp2 == NIL_TYP) {
	message (ERR_CAST);
	return NIL_EXPR;
    }
    if (is_void (tp2) && is_void (ep->etp)) {
	message (WARN_VOID);
    }
#endif /* SYNTAX_CORRECT */
    return castop (ep, tp2);
}

/*
 *	 An implicit cast is a cast generated by the compiler.
 *
 *	 Casts are a frequent source of errors within programs so the
 *	 implicit cast routine has many checks to warn the programmer
 *	 when something potentially dangerous is being done.
 */
EXPR   *implicit_castop P2 (EXPR *, ep, TYP *, tp2)
{
    TYP    *tp1;

#ifndef SYNTAX_CORRECT
    if (ep == NIL_EXPR || tp2 == NIL_TYP) {
	message (ERR_CAST);
	return NIL_EXPR;
    }
#endif /* SYNTAX_CORRECT */
    tp1 = ep->etp;

    if (is_same_type (tp1, tp2)) {
#ifndef SYNTAX_CORRECT
	if (is_pointer_type (tp1) &&
	    (referenced_type (tp1)->qual > referenced_type (tp2)->qual)) {
	    message (WARN_CASTCONST);
	}
	if (!is_compatible_type (tp1, tp2)) {
	    if (is_pointer_type (tp1)) {
		if (!is_null_pointer (ep)) {
		    message (WARN_PTRCAST);
		}
	    } else {
		message (WARN_TYPECAST, nameoftype (tp1), nameoftype (tp2));
	    }
	}
	if (is_enum (tp2)
	    && (!is_enum (tp1) || (enumtype (tp1) != enumtype (tp2)))) {
	    message (WARN_ENUM, nameoftype (tp1));
	}
#endif /* SYNTAX_CORRECT */
	ep->etp = tp2;
    } else {
#ifndef SYNTAX_CORRECT
	if (is_pointer_type (tp1)) {
	    message (WARN_TYPECAST, nameoftype (tp1), nameoftype (tp2));
	    if (tp2->size < tp1->size) {
		message (WARN_SHORTPTR, nameoftype (tp2));
	    }
	} else {
	    if (is_pointer_type (tp2)) {
		if (is_null_pointer (ep)) {
		    message (WARN_NULLCAST);
		} else {
		    message (WARN_TYPECAST, nameoftype (tp1),
			     nameoftype (tp2));
		}
#ifdef FACIST
	    } else {
		if (!is_constexpr (ep)) {
		    message (WARN_NOIMPLICIT, nameoftype (tp1),
			     nameoftype (tp2));
		}
#endif /* FACIST */
	    }
	}
	if (!is_void (tp2) && !is_float (tp2)) {
	    if (is_constexpr (ep)) {
		if (!is_constant_in_range (ep, tp2)) {
		    if (is_unsigned_type (ep->etp)) {
			message (WARN_RANGEU, (unsigned long) ep->v.u,
				  nameoftype (tp2));
		    } else {
			message (WARN_RANGEI, (long) ep->v.i,
				  nameoftype (tp2)); 
		    }
		}
	    } else if (tp1->size > tp2->size) {
		message (WARN_NARROWER, nameoftype (tp1), nameoftype (tp2));
	    }
	} else if ((ep->nodetype == en_cast) && (tp1->size < tp2->size)) {
	    message (WARN_WIDER, nameoftype (tp2), nameoftype (tp1));
	}
#endif /* SYNTAX_CORRECT */
	ep = castop (ep, tp2);
    }
    return ep;
}

#ifdef TRACE
EXPR   *traceexpr P0 (void)
{
    EXPR   *ep1, *ep2;
    SYM    *sp;

    /* create line number parameter */
    ep2 = mk_icon ((IVAL) act_line, tp_int);
    ep1 = mk_node (en_list, ep2, NIL_EXPR, tp_void);

    /* create file name parameter */
    ep2 = mk_stringlit (act_file, strlen ((const char *) act_file));
    ep1 = mk_node (en_list, ep2, ep1, tp_void);

    /* function call name */
    sp = internal_symbol (SUP_TRACE, tp_func);
    ep2 = mk_symnode (sp);

    /* do function call */
    ep1 = mk_node (en_fcall, ep2, ep1, tp_void);
    return ep1;
}
#endif /* TRACE */

/*
 *	 walkexpr() descends the expression tree recursively.
 */
#if defined(__STDC__) || defined(__cplusplus)
EXPR   *walkexpr (EXPR *ep, EXPR *(*exprfunc) (EXPR *))
#else
EXPR   *walkexpr (ep, exprfunc)
     EXPR   *ep;
     EXPR   *(*exprfunc) ();

#endif
{
#ifndef SYNTAX_CORRECT
    if (ep == NIL_EXPR) {
	return NIL_EXPR;
    }
#endif /* SYNTAX_CORRECT */
    switch (ep->nodetype) {
    case en_add:
    case en_sub:
    case en_mul:
    case en_div:
    case en_mod:
    case en_lsh:
    case en_rsh:
    case en_and:
    case en_or:
    case en_xor:
    case en_land:
    case en_lor:
    case en_eq:
    case en_ne:
    case en_lt:
    case en_le:
    case en_gt:
    case en_ge:
    case en_cond:
    case en_comma:
    case en_list:
    case en_asadd:
    case en_assub:
    case en_asmul:
    case en_asmul2:
    case en_asdiv:
    case en_asdiv2:
    case en_asor:
    case en_asxor:
    case en_asand:
    case en_asmod:
    case en_aslsh:
    case en_asrsh:
    case en_fcall:
    case en_call:
    case en_usercall:
    case en_assign:
    case en_litval:
	ep->v.p[1] = walkexpr (ep->v.p[1], exprfunc);
	/*lint -fallthrough */
    case en_uminus:
    case en_not:
    case en_test:
    case en_compl:
    case en_ainc:
    case en_adec:
    case en_cast:
    case en_deref:
    case en_ref:
    case en_literal:
	ep->v.p[0] = walkexpr (ep->v.p[0], exprfunc);
	/*lint -fallthrough */
    case en_size:
    case en_fcon:
    case en_icon:
    case en_nacon:
    case en_labcon:
    case en_autocon:
    case en_global:
    case en_sym:
    case en_register:
    case en_str:
	ep = (*exprfunc) (ep);
	break;
    case en_fieldref:
	ep->v.bit.p = walkexpr (ep->v.bit.p, exprfunc);
	ep = (*exprfunc) (ep);
	break;
    default:
	CANNOT_REACH_HERE ();
	break;
    }
    return ep;
}

/******************************************************************************
**
**	The following routines are used by the code generators to change
**	operator nodes into run-time support function calls.   Previously
**	these calls were performed directly within the code generation
**	routines themselves.   By changing the expression trees themselves
**	it simplifies the code generation routines and also allows the
**	global optimiser to also work on the parameters passed to the
**	support routines.
*/

#ifdef FLOAT_SUPPORT
/*
 *	 This routine takes a unary operator 'ep' and changes it
 *	 to call the function 'name' with the operand as the parameter
 *	 to the function
 */
EXPR   *transform_unary P2 (EXPR *, ep, const CHAR *, name)
{
    ep->v.p[1] = mk_node (en_list, ep->v.p[0], NIL_EXPR, tp_void);
    ep->v.p[0] = mk_symnode (internal_symbol (name, ep->etp));
    ep->nodetype = en_call;
    return ep;
}

/*
 *	 This routine takes a unary operator 'ep' and changes it
 *	 to call the function 'name' with the reference to the operand
 *	 as the parameter to the function
 */
EXPR   *transform_unary_ref P2 (EXPR *, ep, const CHAR *, name)
{
    ep->v.p[1] = mk_node (en_list, ep->v.p[0]->v.p[0], NIL_EXPR, tp_void);
    ep->v.p[0] = mk_symnode (internal_symbol (name, ep->etp));
    ep->nodetype = en_call;
    return ep;
}
#endif /* FLOAT_SUPPORT */

/*
 *	 This routine takes a binary operator 'ep' and changes it
 *	 to call the function 'name' with the operands as the parameters
 *	 to the function
 */
EXPR   *transform_binary P2 (EXPR *, ep, const CHAR *, name)
{
    EXPR   *ep1;

    ep1 = mk_node (en_list, ep->v.p[0], NIL_EXPR, tp_void);
    ep1 = mk_node (en_list, ep->v.p[1], ep1, tp_void);
    ep->v.p[1] = ep1;
    ep->v.p[0] = mk_symnode (internal_symbol (name, ep->etp));
    ep->nodetype = en_call;
    return ep;
}

#ifdef FLOAT_SUPPORT
/*
 *	 This routine takes a binary operator 'ep' and changes it
 *	 to call the function 'name' with the operands as the parameters
 *	 to the function - the 1st parameter being a reference to the
 *	 operand.
 */
EXPR   *transform_binary_ref P2 (EXPR *, ep, const CHAR *, name)
{
    EXPR   *ep1;

    ep1 = mk_node (en_list, ep->v.p[0]->v.p[0], NIL_EXPR, tp_void);
    ep1 = mk_node (en_list, ep->v.p[1], ep1, tp_void);
    ep->v.p[1] = ep1;
    ep->v.p[0] = mk_symnode (internal_symbol (name, ep->etp));
    ep->nodetype = en_call;
    return ep;
}
#endif /* FLOAT_SUPPORT */

/*
 *	 This routine takes a binary assign operator 'ep' and changes it
 *	 to call the function 'name1' if it is an ordinary assignment
 *	 and 'name2' if it is to a bitfield, with the operands as the
 *	 parameters to the function - the 1st parameter being a reference
 *	 to the operand.
 */
EXPR   *transform_assign P4 (EXPR *, ep, const CHAR *, name1, const CHAR *,
			     name2, const CHAR *, name3)
{
    EXPR   *ep1;
    EXPR   *ep0 = ep->v.p[0];
    TYP    *tp = ep->etp;
    SIZE    size;

    switch (ep0->nodetype) {
    case en_fieldref:
	/*
	 *       The encoding information passed to the support routine
	 *       as follows:
	 *
	 *                       F E D C B A 9 8 | 7 6 5 4 3 2 1 0
	 *                       ----------------+----------------
	 *                       S 0 0 W W W W W | 0 0 0 F F F F F
	 *
	 *       S - sign
	 *       W - width of the LHS
	 *       F - offset of the LHS
	 */
	size = (SIZE) ((((USIZE) ep0->v.bit.width) << 8) |
		       ((((USIZE) tp_long->size * 8L) -
			 (USIZE) (ep0->v.bit.offset)) -
			 (USIZE) (ep0->v.
				 bit.width)) | ((USIZE) is_unsigned_type (tp)
						<< 15));
	ep1 = mk_node (en_list, ep0->v.bit.p, NIL_EXPR, tp_void);
	ep1 = mk_node (en_list, mk_size (size, tp_short), ep1, tp_void);
	ep1 =
	    mk_node (en_list, mk_symnode (internal_symbol (name2, tp)), ep1,
		     tp_void);
	ep1 = mk_node (en_list, ep->v.p[1], ep1, tp_void);
	ep->v.p[1] = ep1;
	ep->v.p[0] = mk_symnode (internal_symbol (name3, tp));
	ep->nodetype = en_call;
	break;
    case en_ref:
	ep1 = mk_node (en_list, ep0->v.p[0], NIL_EXPR, tp_void);
	ep1 = mk_node (en_list, ep->v.p[1], ep1, tp_void);
	ep->v.p[1] = ep1;
	ep->v.p[0] = mk_symnode (internal_symbol (name1, tp));
	ep->nodetype = en_call;
	break;
    default:
	CANNOT_REACH_HERE ();
    }
    return ep;
}

#ifdef FLOAT_SUPPORT
/*
 *	 This routine takes a binary assign operator 'ep' and changes it
 *	 to call the function 'name'.
 */
EXPR   *transform_assign2 P2 (EXPR *, ep, const CHAR *, name)
{
    EXPR   *ep1;
    EXPR   *ep0 = ep->v.p[0];
    TYP    *tp = ep->etp;
    SIZE    size = (SIZE) ((USIZE) (tp->size * 8L) << 8);

    switch (ep0->nodetype) {
    case en_fieldref:
	size = (SIZE) ((((USIZE) ep0->v.bit.width) << 8) |
		       ((((USIZE) tp_long->size * 8L) -
			 (USIZE) (ep0->v.bit.offset)) -
			(USIZE) (ep0->v.bit.width)));
	size |= (SIZE) (is_unsigned_type (tp) << 15);
	/*
	 *       The encoding information passed to the support routine
	 *       as follows:
	 *
	 *                       F E D C B A 9 8 | 7 6 5 4 3 2 1 0
	 *                       ----------------+----------------
	 *                       S 0 0 W W W W W | 0 0 0 F F F F F
	 *
	 *       S - sign
	 *       W - width in bits of the LHS
	 *       F - offset in bits of the LHS
	 */
	ep1 = mk_node (en_list, ep0->v.bit.p, NIL_EXPR, tp_void);
	ep1 = mk_node (en_list, mk_size (size, tp_short), ep1, tp_void);
	ep1 = mk_node (en_list, ep->v.p[1], ep1, tp_void);
	ep->v.p[1] = ep1;
	ep->v.p[0] = mk_symnode (internal_symbol (name, tp));
	ep->nodetype = en_call;
	break;
    case en_ref:
	size |= (SIZE) (is_unsigned_type (tp) << 15);
	/*
	 *       The encoding information passed to the support routine
	 *       as follows:
	 *
	 *                       F E D C B A 9 8 | 7 6 5 4 3 2 1 0
	 *                       ----------------+----------------
	 *                       S 0 0 W W W W W | 0 0 0 F F F F F
	 *
	 *       S - sign
	 *       W - width in bits of the LHS
	 *       F - offset in bits of the LHS
	 */
	ep1 = mk_node (en_list, ep0->v.p[0], NIL_EXPR, tp_void);
	ep1 = mk_node (en_list, mk_size (size, tp_short), ep1, tp_void);
	ep1 = mk_node (en_list, ep->v.p[1], ep1, tp_void);
	ep->v.p[1] = ep1;
	ep->v.p[0] = mk_symnode (internal_symbol (name, tp));
	ep->nodetype = en_call;
	break;
    default:
	CANNOT_REACH_HERE ();
    }
    return ep;
}
#endif /* FLOAT_SUPPORT */
