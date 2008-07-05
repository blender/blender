/**
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
#ifndef __KX_ILOGICBRICK
#define __KX_ILOGICBRICK

#include "Value.h"
#include "SCA_IObject.h"
#include "BoolValue.h"

class SCA_ILogicBrick : public CValue
{
	Py_Header;
protected:
	SCA_IObject*		m_gameobj;
	int					m_Execute_Priority;
	int					m_Execute_Ueber_Priority;

	bool				m_bActive;
	CValue*				m_eventval;
	STR_String			m_text;
	STR_String			m_name;
	//unsigned long		m_drawcolor;
	void RegisterEvent(CValue* eventval);
	void RemoveEvent();
	CValue* GetEvent();

public:
	SCA_ILogicBrick(SCA_IObject* gameobj,PyTypeObject* T );
	virtual ~SCA_ILogicBrick();

	void SetExecutePriority(int execute_Priority);
	void SetUeberExecutePriority(int execute_Priority);

	SCA_IObject*	GetParent();
	virtual void	ReParent(SCA_IObject* parent);

	// act as a BoolValue (with value IsPositiveTrigger)
	virtual CValue*	Calc(VALUE_OPERATOR op, CValue *val);
	virtual CValue*	CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val);

	virtual const STR_String &	GetText();
	virtual float		GetNumber();
	virtual STR_String	GetName();
	virtual void		SetName(STR_String name);
	virtual void		ReplicaSetName(STR_String name);
		
	bool				IsActive();
	void				SetActive(bool active) ;

	virtual	bool		LessComparedTo(SCA_ILogicBrick* other);
	
	virtual PyObject* _getattr(const STR_String& attr);

	static class SCA_LogicManager*	m_sCurrentLogicManager;


	// python methods

	KX_PYMETHOD_NOARGS(SCA_ILogicBrick,GetOwner);
	KX_PYMETHOD(SCA_ILogicBrick,SetExecutePriority);
	KX_PYMETHOD_NOARGS(SCA_ILogicBrick,GetExecutePriority);

	enum KX_BOOL_TYPE {
		KX_BOOL_NODEF = 0,
		KX_TRUE,
		KX_FALSE,
		KX_BOOL_MAX
	};


protected: 
	/* Some conversions to go with the bool type. */
	/** Convert a KX_TRUE, KX_FALSE in Python to a c++ value. */
	bool PyArgToBool(int boolArg);

	/** Convert a a c++ value to KX_TRUE, KX_FALSE in Python. */
	PyObject* BoolToPyArg(bool);

	
};

#endif

