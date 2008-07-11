/**
 * Actuator sensor
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

#ifndef __KX_ACTUATORSENSOR
#define __KX_ACTUATORSENSOR

#include "SCA_ISensor.h"
#include "SCA_IActuator.h"

class SCA_ActuatorSensor : public SCA_ISensor
{
	Py_Header;
	STR_String		m_checkactname;
	bool			m_lastresult;
	bool			m_midresult;
 protected:
	SCA_IActuator*	m_actuator;
public:
	SCA_ActuatorSensor(class SCA_EventManager* eventmgr,
					  SCA_IObject* gameobj,
					  const STR_String& actname,
					  PyTypeObject* T=&Type );
	
	virtual ~SCA_ActuatorSensor();
	virtual CValue* GetReplica();
	virtual void Init();
	virtual bool Evaluate(CValue* event);
	virtual bool	IsPositiveTrigger();
	virtual void	ReParent(SCA_IObject* parent);
	void Update();

	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */

	virtual PyObject* _getattr(const STR_String& attr);

	/* 3. setProperty */
	KX_PYMETHOD_DOC(SCA_ActuatorSensor,SetActuator);
	/* 4. getProperty */
	KX_PYMETHOD_DOC(SCA_ActuatorSensor,GetActuator);
	
};

#endif

