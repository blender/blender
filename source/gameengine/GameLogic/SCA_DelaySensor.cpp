/**
 * Delay trigger
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
// This warning tells us about truncation of __long__ stl-generated names.
// It can occasionally cause DevStudio to have internal compiler warnings.
#pragma warning( disable : 4786 )     
#endif

#include "SCA_DelaySensor.h"
#include "SCA_LogicManager.h"
#include "SCA_EventManager.h"

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

SCA_DelaySensor::SCA_DelaySensor(class SCA_EventManager* eventmgr,
								 SCA_IObject* gameobj,
								 int delay,
								 int duration,
								 bool repeat)
	: SCA_ISensor(gameobj,eventmgr),
	m_repeat(repeat),
	m_delay(delay),
	m_duration(duration)
{
	Init();
}

void SCA_DelaySensor::Init()
{
	m_lastResult = false;
	m_frameCount = -1;
	m_reset = true;
}

SCA_DelaySensor::~SCA_DelaySensor()
{
	/* intentionally empty */
}

CValue* SCA_DelaySensor::GetReplica()
{
	CValue* replica = new SCA_DelaySensor(*this);
	// this will copy properties and so on...
	replica->ProcessReplica();

	return replica;
}



bool SCA_DelaySensor::IsPositiveTrigger()
{ 
	return (m_invert ? !m_lastResult : m_lastResult);
}

bool SCA_DelaySensor::Evaluate()
{
	bool trigger = false;
	bool result;

	if (m_frameCount==-1) {
		// this is needed to ensure ON trigger in case delay==0
		// and avoid spurious OFF trigger when duration==0
		m_lastResult = false;
		m_frameCount = 0;
	}

	if (m_frameCount<m_delay) {
		m_frameCount++;
		result = false;
	} else if (m_duration > 0) {
		if (m_frameCount < m_delay+m_duration) {
			m_frameCount++;
			result = true;
		} else {
			result = false;
			if (m_repeat)
				m_frameCount = -1;
		}
	} else {
		result = true;
		if (m_repeat)
			m_frameCount = -1;
	}
	if ((m_reset && m_level) || result != m_lastResult)
		trigger = true;
	m_reset = false;
	m_lastResult = result;
	return trigger;
}

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject SCA_DelaySensor::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SCA_DelaySensor",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&SCA_ISensor::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef SCA_DelaySensor::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef SCA_DelaySensor::Attributes[] = {
	KX_PYATTRIBUTE_INT_RW("delay",0,100000,true,SCA_DelaySensor,m_delay),
	KX_PYATTRIBUTE_INT_RW("duration",0,100000,true,SCA_DelaySensor,m_duration),
	KX_PYATTRIBUTE_BOOL_RW("repeat",SCA_DelaySensor,m_repeat),
	{ NULL }	//Sentinel
};

/* eof */
