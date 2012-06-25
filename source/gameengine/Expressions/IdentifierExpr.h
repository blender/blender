/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file IdentifierExpr.h
 *  \ingroup expressions
 */

#ifndef __IDENTIFIEREXPR_H__
#define __IDENTIFIEREXPR_H__

#include "Expression.h"

class CIdentifierExpr : public CExpression
{
	CValue*		m_idContext;
	STR_String	m_identifier;
public:
	CIdentifierExpr(const STR_String& identifier,CValue* id_context);
	virtual ~CIdentifierExpr();

	virtual CValue*			Calculate();
	virtual bool			MergeExpression(CExpression* otherexpr);
	virtual unsigned char	GetExpressionID();
	virtual bool			NeedsRecalculated();
	virtual CExpression*	CheckLink(std::vector<CBrokenLinkInfo*>& brokenlinks);
	virtual void			ClearModified();
	virtual void			BroadcastOperators(VALUE_OPERATOR op);


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:CIdentifierExpr")
#endif
};

#endif //__IDENTIFIEREXPR_H__

