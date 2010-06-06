/*
 * Parser.h: interface for the CParser class.
 * Eindhoven University of Technology 1997
 * OOPS team (Serge vd Boom, Erwin Coumans, Tom Geelen, Wynke Stuylemeier)
 * $Id$
 * Copyright (c) 1996-2000 Erwin Coumans <coockie@acm.org>
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Erwin Coumans makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 */
#ifndef __INPUTPARSER_H__
#define __INPUTPARSER_H__

class CParser;
#include "Expression.h"


class CParser
{
public:
	CParser();
	virtual				~CParser();

	float				GetFloat(STR_String& txt);
	CValue*				GetValue(STR_String& txt, bool bFallbackToText=false);
	CExpression*		ProcessText(const char *intext);
	void				SetContext(CValue* context);

private:
	enum symbols {
		errorsym,
		lbracksym,
		rbracksym,
		cellsym,
		commasym,
		opsym,
		constsym,
		sumsym,
		ifsym,
		whocodedsym,
		eolsym,
		idsym
	};			// all kinds of symbols

	enum optype {
		OPmodulus,
		OPplus,
		OPminus,
		OPtimes,
		OPdivide,
		OPand,
		OPor,
		OPequal,
		OPunequal,
		OPgreater,
		OPless,
		OPgreaterequal,
		OPlessequal,
		OPnot
	};		// all kinds of operators

	enum consttype {
		booltype,
		inttype,
		floattype,
		stringtype
	};		// all kinds of constants
	
	int sym,					// current symbol
		opkind,					// kind of operator, if symbol is an operator
		constkind;				// kind of operator, if symbol is a constant
	
	char ch;					// current character
	int chcount;				// index to character in input string
	CExpression *errmsg;		// contains a errormessage, if scanner error
	
	STR_String text,				// contains a copy of the original text
		const_as_string;		// string representation of the symbol, if symbol is a constant
	bool boolvalue;				// value of the boolean, if symbol is a constant of type boolean
	CValue*	m_identifierContext;// context in which identifiers are looked up
	
	
	void ScanError(const char *str);
	CExpression* Error(const char *str);
	void NextCh();
	void TermChar(char c);
	void DigRep();
	void CharRep();
	void GrabString(int start);
	void NextSym();
#if 0	/* not used yet */
	int MakeInt();
#endif
	STR_String Symbol2Str(int s);
	void Term(int s);
	int Priority(int optor);
	CExpression *Ex(int i);
	CExpression *Expr();
	
	
#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:CParser"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif

