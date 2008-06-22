/**
 * Property sensor
 *
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

#ifndef __KX_PROPERTYSENSOR
#define __KX_PROPERTYSENSOR

#include "SCA_ISensor.h"

class SCA_PropertySensor : public SCA_ISensor
{
	Py_Header;
	//class CExpression*	m_rightexpr;
	int				m_checktype;
	STR_String		m_checkpropval;
	STR_String		m_checkpropmaxval;
	STR_String		m_checkpropname;
	STR_String		m_previoustext;
	bool			m_lastresult;
	bool			m_recentresult;
	CExpression*	m_range_expr;

	/**
	 * Test whether this is a sensible value (type check)
	 */
	bool validValueForProperty(char *val, STR_String &prop);
 protected:

public:
	enum KX_PROPSENSOR_TYPE {
		KX_PROPSENSOR_NODEF = 0,
		KX_PROPSENSOR_EQUAL,
		KX_PROPSENSOR_NOTEQUAL,
		KX_PROPSENSOR_INTERVAL,
		KX_PROPSENSOR_CHANGED,
		KX_PROPSENSOR_EXPRESSION,
		KX_PROPSENSOR_MAX
	};

	const STR_String S_KX_PROPSENSOR_EQ_STRING;
	
	SCA_PropertySensor(class SCA_EventManager* eventmgr,
					  SCA_IObject* gameobj,
					  const STR_String& propname,
					  const STR_String& propval,
					  const STR_String& propmaxval,
					  KX_PROPSENSOR_TYPE checktype,
					  PyTypeObject* T=&Type );
	
	virtual void Delete();
	virtual ~SCA_PropertySensor();
	virtual CValue* GetReplica();
	virtual void Init();
	void	PrecalculateRangeExpression();
	bool	CheckPropertyCondition();

	virtual bool Evaluate(CValue* event);
	virtual bool	IsPositiveTrigger();
	virtual CValue*		FindIdentifier(const STR_String& identifiername);

	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */

	virtual PyObject* _getattr(const STR_String& attr);

	/* 1. getType */
	KX_PYMETHOD_DOC(SCA_PropertySensor,GetType);
	/* 2. setType */
	KX_PYMETHOD_DOC(SCA_PropertySensor,SetType);
	/* 3. setProperty */
	KX_PYMETHOD_DOC(SCA_PropertySensor,SetProperty);
	/* 4. getProperty */
	KX_PYMETHOD_DOC(SCA_PropertySensor,GetProperty);
	/* 5. getValue */
	KX_PYMETHOD_DOC(SCA_PropertySensor,GetValue);
	/* 6. setValue */
	KX_PYMETHOD_DOC(SCA_PropertySensor,SetValue);
	
};

#endif

