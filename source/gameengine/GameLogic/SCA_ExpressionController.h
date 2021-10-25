/*
 * KX_EXPRESSIONController.h
 *
 *
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

/** \file SCA_ExpressionController.h
 *  \ingroup gamelogic
 */

#ifndef __SCA_EXPRESSIONCONTROLLER_H__
#define __SCA_EXPRESSIONCONTROLLER_H__

#include "SCA_IController.h"

class SCA_ExpressionController : public SCA_IController
{
//	Py_Header
	STR_String			m_exprText;
	CExpression*		m_exprCache;

public:
	SCA_ExpressionController(SCA_IObject* gameobj,
							 const STR_String& exprtext);

	virtual ~SCA_ExpressionController();
	virtual CValue* GetReplica();
	virtual void Trigger(SCA_LogicManager* logicmgr);
	virtual CValue*		FindIdentifier(const STR_String& identifiername);
	/** 
	 *  used to release the expression cache
	 *  so that self references are removed before the controller itself is released
	 */
	virtual void Delete();


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:SCA_ExpressionController")
#endif
};

#endif  /* __SCA_EXPRESSIONCONTROLLER_H__ */
