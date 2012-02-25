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

/** \file SCA_RandomSensor.h
 *  \ingroup gamelogic
 *  \brief Generate random pulses
 */

#ifndef __SCA_RANDOMSENSOR_H__
#define __SCA_RANDOMSENSOR_H__

#include "SCA_ISensor.h"
#include "BoolValue.h"
#include "SCA_RandomNumberGenerator.h"

class SCA_RandomSensor : public SCA_ISensor
{
	Py_Header

	unsigned int m_currentDraw;
	int m_iteration;
	int m_interval;
	SCA_RandomNumberGenerator *m_basegenerator;
	bool m_lastdraw;
public:
	SCA_RandomSensor(class SCA_EventManager* rndmgr,
					SCA_IObject* gameobj,
					int startseed);
	virtual ~SCA_RandomSensor();
	virtual CValue* GetReplica();
	virtual void ProcessReplica();
	virtual bool Evaluate();
	virtual bool IsPositiveTrigger();
	virtual void Init();

#ifdef WITH_PYTHON

	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */
	
	static PyObject*	pyattr_get_seed(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_seed(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
#endif
};

#endif //__SCA_RANDOMSENSOR_H__

