// Parser.cpp: implementation of the CParser class.
/*
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

#include <stdlib.h>

#include "MT_assert.h"

#include "Value.h"
#include "InputParser.h"
#include "ErrorValue.h"
#include "IntValue.h"
#include "StringValue.h"
#include "FloatValue.h"
#include "BoolValue.h"
#include "EmptyValue.h"
#include "ConstExpr.h"
#include "Operator2Expr.h"
#include "Operator1Expr.h"
#include "IdentifierExpr.h"

// this is disable at the moment, I expected a memleak from it, but the error-cleanup was the reason
// well, looks we don't need it anyway, until maybe the Curved Surfaces are integrated into CSG 
// cool things like (IF(LOD==1,CCurvedValue,IF(LOD==2,CCurvedValue2)) etc...
#include "IfExpr.h" 

#if (defined(WIN32) || defined(WIN64)) && !defined(FREE_WINDOWS)
#define strcasecmp	_stricmp

#ifndef strtoll
#define strtoll		_strtoi64
#endif

#endif /* Def WIN32 or Def WIN64 */

#define NUM_PRIORITY 6
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CParser::CParser() : m_identifierContext(NULL)
{
}



CParser::~CParser()
{
	if (m_identifierContext)
		m_identifierContext->Release();
}



void CParser::ScanError(const char *str)
{
	// sets the global variable errmsg to an errormessage with
	// contents str, appending if it already exists
	//	AfxMessageBox("Parse Error:"+str,MB_ICONERROR);
	if (errmsg)
		errmsg = new COperator2Expr(VALUE_ADD_OPERATOR, errmsg,	Error(str));
	else
		errmsg = Error(str);

	sym = errorsym;
}



CExpression* CParser::Error(const char *str)
{
	// makes and returns a new CConstExpr filled with an CErrorValue
	// with string str
	//	AfxMessageBox("Error:"+str,MB_ICONERROR);
	return new CConstExpr(new CErrorValue(str));
}



void CParser::NextCh()
{
	// sets the global variable ch to the next character, if it exists
	// and increases the global variable chcount
	chcount++;

	if (chcount < text.Length())
		ch = text[chcount];
	else
		ch = 0x00;
}



void CParser::TermChar(char c)
{
	// generates an error if the next char isn't the specified char c,
	// otherwise, skip the char
	if(ch == c)
	{
		NextCh();
	}
	else
	{
		STR_String str;
		str.Format("Warning: %c expected\ncontinuing without it", c);
		trace(str);
	}
}



void CParser::DigRep()
{
	// changes the current character to the first character that
	// isn't a decimal
	while ((ch >= '0') && (ch <= '9'))
		NextCh();
}



void CParser::CharRep()
{
	// changes the current character to the first character that
	// isn't an alphanumeric character
	while (((ch >= '0') && (ch <= '9'))
		|| ((ch >= 'a') && (ch <= 'z'))
		|| ((ch >= 'A') && (ch <= 'Z'))
		|| (ch == '.') || (ch == '_'))
		NextCh();
}



void CParser::GrabString(int start)
{
	// puts part of the input string into the global variable
	// const_as_string, from position start, to position chchount
	const_as_string = text.Mid(start, chcount-start);
}



void CParser::NextSym()
{
	// sets the global variable sym to the next symbol, and
	// if it is an operator
	//   sets the global variable opkind to the kind of operator
	// if it is a constant
	//   sets the global variable constkind to the kind of operator
	// if it is a reference to a cell
	//   sets the global variable cellcoord to the kind of operator
	
	errmsg = NULL;
	while(ch == ' ' || ch == 0x9)
		NextCh();

	switch(ch)
	{
    case '(':
		sym = lbracksym; NextCh();
		break;
    case ')':
		sym = rbracksym; NextCh();
		break;
    case ',':
		sym = commasym; NextCh();
		break;
	case '%' :
		sym = opsym; opkind = OPmodulus; NextCh();
		break;
    case '+' :
		sym = opsym; opkind = OPplus; NextCh();
		break;
    case '-' :
		sym = opsym; opkind = OPminus; NextCh();
		break;
    case '*' :
		sym = opsym; opkind = OPtimes; NextCh();
		break;
    case '/' :
		sym = opsym; opkind = OPdivide; NextCh();
		break;
	case '&' :
		sym = opsym; opkind = OPand; NextCh(); TermChar('&');
		break;
	case '|' :
		sym = opsym; opkind = OPor; NextCh(); TermChar('|');
		break;
	case '=' :
		sym = opsym; opkind = OPequal; NextCh(); TermChar('=');
		break;
	case '!' :
		sym = opsym;
		NextCh();
		if (ch == '=')
		{
			opkind = OPunequal;
			NextCh();
		}
		else
		{
			opkind = OPnot;
		}
		break;
	case '>':
		sym = opsym;
		NextCh();
		if (ch == '=')
		{
			opkind = OPgreaterequal;
			NextCh();
		}
		else
		{
			opkind = OPgreater;
		}
		break;
	case '<':
		sym = opsym;
		NextCh();
		if (ch == '=') {
			opkind = OPlessequal;
			NextCh();
		} else {
			opkind = OPless;
		}
		break;
    case '\"' : {
		int start;
		sym = constsym;
		constkind = stringtype;
		NextCh();
		start = chcount;
		while ((ch != '\"') && (ch != 0x0))
			NextCh();
		GrabString(start);
		TermChar('\"');	// check for eol before '\"'
		break;
				}
    case 0x0: sym = eolsym; break;
    default: 
		{
			int start;
			start = chcount;
			DigRep();
			if ((start != chcount) || (ch == '.')) { // number
				sym = constsym;
				if (ch == '.') {
					constkind = floattype;
					NextCh();
					DigRep();
				}
				else constkind = inttype;
				if ((ch == 'e') || (ch == 'E')) {
					int mark;
					constkind = floattype;
					NextCh();
					if ((ch == '+') || (ch == '-')) NextCh();
					mark = chcount;
					DigRep();
					if (mark == chcount) {
						ScanError("Number expected after 'E'");
						return;
					}
				}
				GrabString(start);
			} else if (((ch >= 'a') && (ch <= 'z'))
				|| ((ch >= 'A') && (ch <= 'Z')))
			{ // reserved word?
				
				start = chcount;
				CharRep();
				GrabString(start);
				if (!strcasecmp(const_as_string, "SUM")) {
					sym = sumsym;
				}
				else if (!strcasecmp(const_as_string, "NOT")) {
					sym = opsym;
					opkind = OPnot;
				}
				else if (!strcasecmp(const_as_string, "AND")) {
					sym = opsym; opkind = OPand;
				}
				else if (!strcasecmp(const_as_string, "OR")) {
					sym = opsym; opkind = OPor;
				}
				else if (!strcasecmp(const_as_string, "IF"))
					sym = ifsym;
				else if (!strcasecmp(const_as_string, "WHOMADE"))
					sym = whocodedsym;
				else if (!strcasecmp(const_as_string, "FALSE")) {
					sym = constsym; constkind = booltype; boolvalue = false;
				} else if (!strcasecmp(const_as_string, "TRUE")) {
					sym = constsym; constkind = booltype; boolvalue = true;
				} else {
					sym = idsym;
					//STR_String str;
					//str.Format("'%s' makes no sense here", (const char*)funstr);
					//ScanError(str);
				}
			} else { // unknown symbol
				STR_String str;
				str.Format("Unexpected character '%c'", ch);
				NextCh();
				ScanError(str);
				return;
			}
		}
	}
}

#if 0
int CParser::MakeInt() {
	// returns the integer representation of the value in the global
	// variable const_as_string
	// pre: const_as_string contains only numercal chars
	return atoi(const_as_string);
}
#endif

STR_String CParser::Symbol2Str(int s) {
	// returns a string representation of of symbol s,
	// for use in Term when generating an error
	switch(s) {
    case errorsym: return "error";
    case lbracksym: return "(";
    case rbracksym: return ")";
    case commasym: return ",";
    case opsym: return "operator";
    case constsym: return "constant";
	case sumsym: return "SUM";
	case ifsym: return "IF";
	case whocodedsym: return "WHOMADE";
    case eolsym: return "end of line";
	case idsym: return "identifier";
    default: return "unknown";  // should not happen
	}
}

void CParser::Term(int s) {
	// generates an error if the next symbol isn't the specified symbol s
	// otherwise, skip the symbol
	if(s == sym) NextSym();
	else {
		STR_String msg;
		msg.Format("Warning: " + Symbol2Str(s) + " expected\ncontinuing without it");

//		AfxMessageBox(msg,MB_ICONERROR);

		trace(msg);
	}
}

int CParser::Priority(int optorkind) {
	// returns the priority of an operator
	// higher number means higher priority
	switch(optorkind) {
	case OPor: return 1;
	case OPand: return 2;
	case OPgreater:
	case OPless:
	case OPgreaterequal:
	case OPlessequal:
	case OPequal:
	case OPunequal: return 3;
    case OPplus:
    case OPminus: return 4;
	case OPmodulus:
    case OPtimes:
    case OPdivide: return 5;
	}
	MT_assert(false);
	return 0;      // should not happen
}

CExpression *CParser::Ex(int i) {
	// parses an expression in the imput, starting at priority i, and
	// returns an CExpression, containing the parsed input
	CExpression *e1 = NULL, *e2 = NULL;
	int opkind2;
	
	if (i < NUM_PRIORITY) {
		e1 = Ex(i + 1);
		while ((sym == opsym) && (Priority(opkind) == i)) {
			opkind2 = opkind;
			NextSym();
			e2 = Ex(i + 1);
			switch(opkind2) {
			case OPmodulus: e1 = new COperator2Expr(VALUE_MOD_OPERATOR,e1, e2); break;
			case OPplus: e1 = new COperator2Expr(VALUE_ADD_OPERATOR,e1, e2); break;
			case OPminus: e1 = new COperator2Expr(VALUE_SUB_OPERATOR,e1, e2); break;
			case OPtimes:	e1 = new COperator2Expr(VALUE_MUL_OPERATOR,e1, e2); break;
			case OPdivide: e1 = new COperator2Expr(VALUE_DIV_OPERATOR,e1, e2); break;
			case OPand: e1 = new COperator2Expr(VALUE_AND_OPERATOR,e1, e2); break;
			case OPor: e1 = new COperator2Expr(VALUE_OR_OPERATOR,e1, e2); break;
			case OPequal: e1 = new COperator2Expr(VALUE_EQL_OPERATOR,e1, e2); break;
			case OPunequal: e1 = new COperator2Expr(VALUE_NEQ_OPERATOR,e1, e2); break;
			case OPgreater: e1 = new COperator2Expr(VALUE_GRE_OPERATOR,e1, e2); break;
			case OPless: e1 = new COperator2Expr(VALUE_LES_OPERATOR,e1, e2); break;
			case OPgreaterequal: e1 = new COperator2Expr(VALUE_GEQ_OPERATOR,e1, e2); break;
			case OPlessequal: e1 = new COperator2Expr(VALUE_LEQ_OPERATOR,e1, e2); break;
			default: MT_assert(false);	break; // should not happen
			}
		}
	} else if (i == NUM_PRIORITY) {
		if ((sym == opsym) 
			&& ( (opkind == OPminus) || (opkind == OPnot) || (opkind == OPplus) ) 
			)
		{
			NextSym();
			switch(opkind) {
			/* +1 is also a valid number! */
			case OPplus: e1 = new COperator1Expr(VALUE_POS_OPERATOR, Ex(NUM_PRIORITY)); break;
			case OPminus: e1 = new COperator1Expr(VALUE_NEG_OPERATOR, Ex(NUM_PRIORITY)); break;
			case OPnot: e1 = new COperator1Expr(VALUE_NOT_OPERATOR, Ex(NUM_PRIORITY)); break;
			default: {
						// should not happen
						e1 = Error("operator +, - or ! expected");
					 }
			}
		}
		else {
			switch(sym) {
			case constsym: {
				switch(constkind) {
				case booltype:
					e1 = new CConstExpr(new CBoolValue(boolvalue));
					break;
				case inttype:
					{
						cInt temp;
						temp = strtoll(const_as_string, NULL, 10); /* atoi is for int only */
						e1 = new CConstExpr(new CIntValue(temp));
						break;
					}
				case floattype:
					{
						double temp;
						temp = atof(const_as_string);
						e1 = new CConstExpr(new CFloatValue(temp));
						break;
					}
				case stringtype:
					e1 = new CConstExpr(new CStringValue(const_as_string,""));
					break;
				default :
					MT_assert(false);
					break;
				}
				NextSym();
				break;
						   }
			case lbracksym:
				NextSym();
				e1 = Ex(1);
				Term(rbracksym);
				break;
			case ifsym:
			{
				CExpression *e3;
				NextSym();
				Term(lbracksym);
				e1 = Ex(1);
				Term(commasym);
				e2 = Ex(1);
				if (sym == commasym) {
					NextSym();
					e3 = Ex(1);
				} else {
					e3 = new CConstExpr(new CEmptyValue());
				}
				Term(rbracksym);
				e1 = new CIfExpr(e1, e2, e3);
				break;
			}
			case idsym:
				{
					e1 = new CIdentifierExpr(const_as_string,m_identifierContext);
					NextSym();
					
					break;
				}
			case errorsym:
				{
					MT_assert(!e1);
					STR_String errtext="[no info]";
					if (errmsg)
					{
						CValue* errmsgval = errmsg->Calculate();
						errtext=errmsgval->GetText();
						errmsgval->Release();
					
						//e1 = Error(errmsg->Calculate()->GetText());//new CConstExpr(errmsg->Calculate());
						
						if ( !(errmsg->Release()) )
						{
							errmsg=NULL;
						} else {
							// does this happen ?
							MT_assert ("does this happen");
						}
					}
					e1 = Error(errtext);

					break;				
				}
			default:
				NextSym();
				//return Error("Expression expected");
				MT_assert(!e1);
				e1 = Error("Expression expected");
			}
		}
	}
	return e1;
}

CExpression *CParser::Expr() {
	// parses an expression in the imput, and
	// returns an CExpression, containing the parsed input
	return Ex(1);
}

CExpression* CParser::ProcessText
(const char *intext) {
	
	// and parses the string in intext and returns it.
	
	
	CExpression* expr;
	text = intext;
	
	
	chcount = 0;	
	if (text.Length() == 0) {
		return NULL;
	}
	
	ch = text[0];
	/*if (ch != '=') {
	expr = new CConstExpr(new CStringValue(text));
	*dependant = deplist;
	return expr;
	} else 
	*/
	//	NextCh();
	NextSym();
	expr = Expr();
	if (sym != eolsym) {
		CExpression* oldexpr = expr;
		expr = new COperator2Expr(VALUE_ADD_OPERATOR,
			oldexpr, Error(STR_String("Extra characters after expression")));//new CConstExpr(new CErrorValue("Extra characters after expression")));
	}
	if (errmsg)
		errmsg->Release();
	
	return expr;
}



float CParser::GetFloat(STR_String& txt)
{
	// returns parsed text into a float
	// empty string returns -1
	
//	AfxMessageBox("parsed string="+txt);
	CValue* val=NULL;
	float result=-1;
//	String tmpstr;

	CExpression* expr = ProcessText(txt);
	if (expr) {
		val = expr->Calculate();
		result=(float)val->GetNumber();
		
		
	
		val->Release();
		expr->Release();
	}
//	tmpstr.Format("parseresult=%g",result);
//		AfxMessageBox(tmpstr);
	return result;
}

CValue* CParser::GetValue(STR_String& txt, bool bFallbackToText)
{
	// returns parsed text into a value, 
	// empty string returns NULL value !
	// if bFallbackToText then unparsed stuff is put into text
	
	CValue* result=NULL;
	CExpression* expr = ProcessText(txt);
	if (expr) {
		result = expr->Calculate();
		expr->Release();
	}
	if (result)
	{
		// if the parsed stuff lead to an errorvalue, don't return errors, just NULL
		if (result->IsError()) {
			result->Release();
			result=NULL;
			if (bFallbackToText) {
				if (txt.Length()>0)
				{
					result = new CStringValue(txt,"");
				}
			}
		}
	}
	return result;
}

void CParser::SetContext(CValue* context)
{
	if (m_identifierContext)
	{
		m_identifierContext->Release();
	}
	m_identifierContext = context;
}
