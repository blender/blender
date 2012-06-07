/*
 * Do Ipo stuff
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

/** \file gameengine/Ketsji/KX_IpoActuator.cpp
 *  \ingroup ketsji
 */

#include <cmath>
 
#include "KX_IpoActuator.h"
#include "KX_GameObject.h"
#include "FloatValue.h"

#include "KX_KetsjiEngine.h"

/* ------------------------------------------------------------------------- */
/* Type strings                                                              */
/* ------------------------------------------------------------------------- */

const char *KX_IpoActuator::S_KX_ACT_IPO_PLAY_STRING      = "Play";
const char *KX_IpoActuator::S_KX_ACT_IPO_PINGPONG_STRING  = "PingPong";
const char *KX_IpoActuator::S_KX_ACT_IPO_FLIPPER_STRING   = "Flipper";
const char *KX_IpoActuator::S_KX_ACT_IPO_LOOPSTOP_STRING  = "LoopStop";
const char *KX_IpoActuator::S_KX_ACT_IPO_LOOPEND_STRING   = "LoopEnd";
const char *KX_IpoActuator::S_KX_ACT_IPO_KEY2KEY_STRING   = "Key2key";
const char *KX_IpoActuator::S_KX_ACT_IPO_FROM_PROP_STRING = "FromProp";

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

KX_IpoActuator::KX_IpoActuator(SCA_IObject* gameobj,
							   const STR_String& propname,
							   const STR_String& framePropname,
							   float starttime,
							   float endtime,
							   bool recurse,
							   int acttype,
							   bool ipo_as_force,
							   bool ipo_add,
							   bool ipo_local)
	: SCA_IActuator(gameobj, KX_ACT_IPO),
	m_bNegativeEvent(false),
	m_startframe (starttime),
	m_endframe(endtime),
	m_recurse(recurse),
	m_localtime(starttime),
	m_direction(1),
	m_propname(propname),
	m_framepropname(framePropname),
	m_ipo_as_force(ipo_as_force),
	m_ipo_add(ipo_add),
	m_ipo_local(ipo_local),
	m_type(acttype)
{
	this->ResetStartTime();
	m_bIpoPlaying = false;
}

void KX_IpoActuator::SetStart(float starttime) 
{ 
	m_startframe=starttime;
}

void KX_IpoActuator::SetEnd(float endtime) 
{ 
	m_endframe=endtime;
}

bool KX_IpoActuator::ClampLocalTime()
{
	if (m_startframe < m_endframe)
	{
		if (m_localtime < m_startframe)
		{
			m_localtime = m_startframe;
			return true;
		} 
		else if (m_localtime > m_endframe)
		{
			m_localtime = m_endframe;
			return true;
		}
	} else {
		if (m_localtime > m_startframe)
		{
			m_localtime = m_startframe;
			return true;
		}
		else if (m_localtime < m_endframe)
		{
			m_localtime = m_endframe;
			return true;
		}
	}
	return false;
}

void KX_IpoActuator::SetStartTime(float curtime)
{
	float direction = m_startframe < m_endframe ? 1.0f : -1.0f;

	if (m_direction > 0)
		m_starttime = curtime - direction*(m_localtime - m_startframe)/KX_KetsjiEngine::GetAnimFrameRate();
	else
		m_starttime = curtime - direction*(m_endframe - m_localtime)/KX_KetsjiEngine::GetAnimFrameRate();
}

void KX_IpoActuator::SetLocalTime(float curtime)
{
	float delta_time = (curtime - m_starttime)*KX_KetsjiEngine::GetAnimFrameRate();
	
	// negative delta_time is caused by floating point inaccuracy
	// perhaps the inaccuracy could be reduced a bit
	if ((m_localtime==m_startframe || m_localtime==m_endframe) && delta_time<0.0)
	{
		delta_time = 0.0;
	}
	
	if (m_endframe < m_startframe)
		delta_time = -delta_time;

	if (m_direction > 0)
		m_localtime = m_startframe + delta_time;
	else
		m_localtime = m_endframe - delta_time;
}

bool KX_IpoActuator::Update(double curtime, bool frame)
{
	// result = true if animation has to be continued, false if animation stops
	// maybe there are events for us in the queue !
	bool bNegativeEvent = false;
	bool numevents = false;
	bool bIpoStart = false;

	curtime -= KX_KetsjiEngine::GetSuspendedDelta();

	if (frame)
	{
		numevents = m_posevent || m_negevent;
		bNegativeEvent = IsNegativeEvent();
		RemoveAllEvents();
	}
	
	float  start_smaller_then_end = ( m_startframe < m_endframe ? 1.0f : -1.0f);

	bool result=true;
	if (!bNegativeEvent)
	{
		if (m_starttime < -2.0f*fabs(m_endframe - m_startframe))
		{
			// start for all Ipo, initial start for LOOP_STOP
			m_starttime = curtime;
			m_bIpoPlaying = true;
			bIpoStart = true;
		}
	}	

	switch ((IpoActType)m_type)
	{
		
	case KX_ACT_IPO_PLAY:
	{
		// Check if playing forwards.  result = ! finished
		
		if (start_smaller_then_end > 0.f)
			result = (m_localtime < m_endframe && m_bIpoPlaying);
		else
			result = (m_localtime > m_endframe && m_bIpoPlaying);
		
		if (result)
		{
			SetLocalTime(curtime);
		
			/* Perform clamping */
			ClampLocalTime();
	
			if (bIpoStart)
				((KX_GameObject*)GetParent())->InitIPO(m_ipo_as_force, m_ipo_add, m_ipo_local);
			((KX_GameObject*)GetParent())->UpdateIPO(m_localtime,m_recurse);
		} else
		{
			m_localtime=m_startframe;
			m_direction=1;
		}
		break;
	}
	case KX_ACT_IPO_PINGPONG:
	{
		result = true;
		if (bNegativeEvent && !m_bIpoPlaying)
			result = false;
		else
			SetLocalTime(curtime);
			
		if (ClampLocalTime())
		{
			result = false;
			m_direction = -m_direction;
		}
		
		if (bIpoStart && m_direction > 0)
			((KX_GameObject*)GetParent())->InitIPO(m_ipo_as_force, m_ipo_add, m_ipo_local);
		((KX_GameObject*)GetParent())->UpdateIPO(m_localtime,m_recurse);
		break;
	}
	case KX_ACT_IPO_FLIPPER:
	{
		if (bNegativeEvent && !m_bIpoPlaying)
			result = false;
		if (numevents)
		{
			float oldDirection = m_direction;
			if (bNegativeEvent)
				m_direction = -1;
			else
				m_direction = 1;
			if (m_direction != oldDirection)
				// changing direction, reset start time
				SetStartTime(curtime);
		}
		
		SetLocalTime(curtime);
		
		if (ClampLocalTime() && m_localtime == m_startframe)
			result = false;

		if (bIpoStart)
			((KX_GameObject*)GetParent())->InitIPO(m_ipo_as_force, m_ipo_add, m_ipo_local);			
		((KX_GameObject*)GetParent())->UpdateIPO(m_localtime,m_recurse);
		break;
	}

	case KX_ACT_IPO_LOOPSTOP:
	{
		if (numevents)
		{
			if (bNegativeEvent)
			{
				result = false;
				m_bNegativeEvent = false;
				numevents = false;
			}
			if (!m_bIpoPlaying)
			{
				// Ipo was stopped, make sure we will restart from where it stopped
				SetStartTime(curtime);
				if (!bNegativeEvent)
					// positive signal will restart the Ipo
					m_bIpoPlaying = true;
			}

		} // fall through to loopend, and quit the ipo animation immediatly 
	}
	case KX_ACT_IPO_LOOPEND:
	{
		if (numevents) {
			if (bNegativeEvent && m_bIpoPlaying) {
				m_bNegativeEvent = true;
			}
		}
		
		if (bNegativeEvent && !m_bIpoPlaying) {
			result = false;
		} 
		else
		{
			if (m_localtime*start_smaller_then_end < m_endframe*start_smaller_then_end)
			{
				SetLocalTime(curtime);
			}
			else {
				if (!m_bNegativeEvent) {
					/* Perform wraparound */
					SetLocalTime(curtime);
					if (start_smaller_then_end > 0.f)
						m_localtime = m_startframe + fmod(m_localtime - m_startframe, m_endframe - m_startframe);
					else
						m_localtime = m_startframe - fmod(m_startframe - m_localtime, m_startframe - m_endframe);
					SetStartTime(curtime);
					bIpoStart = true;
				}
				else
				{	
					/* Perform clamping */
					m_localtime=m_endframe;
					result = false;
					m_bNegativeEvent = false;
				}
			}
		}
		
		if (m_bIpoPlaying && bIpoStart)
			((KX_GameObject*)GetParent())->InitIPO(m_ipo_as_force, m_ipo_add, m_ipo_local);
		((KX_GameObject*)GetParent())->UpdateIPO(m_localtime,m_recurse);
		break;
	}
	
	case KX_ACT_IPO_KEY2KEY:
	{
		// not implemented yet
		result = false;
		break;
	}
	
	case KX_ACT_IPO_FROM_PROP:
	{
		result = !bNegativeEvent;

		CValue* propval = GetParent()->GetProperty(m_propname);
		if (propval)
		{
			m_localtime = propval->GetNumber(); 
	
			if (bIpoStart)
				((KX_GameObject*)GetParent())->InitIPO(m_ipo_as_force, m_ipo_add, m_ipo_local);
			((KX_GameObject*)GetParent())->UpdateIPO(m_localtime,m_recurse);
		} else
		{
			result = false;
		}
		break;
	}
		
	default:
		result = false;
	}

	/* Set the property if its defined */
	if (m_framepropname[0] != '\0') {
		CValue* propowner = GetParent();
		CValue* oldprop = propowner->GetProperty(m_framepropname);
		CValue* newval = new CFloatValue(m_localtime);
		if (oldprop) {
			oldprop->SetValue(newval);
		} else {
			propowner->SetProperty(m_framepropname, newval);
		}
		newval->Release();
	}

	if (!result)
	{
		if (m_type != KX_ACT_IPO_LOOPSTOP)
			this->ResetStartTime();
		m_bIpoPlaying = false;
	}

	return result;
}

void KX_IpoActuator::ResetStartTime()
{
	this->m_starttime = -2.0*fabs(this->m_endframe - this->m_startframe) - 1.0;
}

int KX_IpoActuator::string2mode(const char *modename)
{
	IpoActType res = KX_ACT_IPO_NODEF;

	if (strcmp(modename, S_KX_ACT_IPO_PLAY_STRING)==0) { 
		res = KX_ACT_IPO_PLAY;
	} else if (strcmp(modename, S_KX_ACT_IPO_PINGPONG_STRING)==0) {
		res = KX_ACT_IPO_PINGPONG;
	} else if (strcmp(modename, S_KX_ACT_IPO_FLIPPER_STRING)==0) {
		res = KX_ACT_IPO_FLIPPER;
	} else if (strcmp(modename, S_KX_ACT_IPO_LOOPSTOP_STRING)==0) {
		res = KX_ACT_IPO_LOOPSTOP;
	} else if (strcmp(modename, S_KX_ACT_IPO_LOOPEND_STRING)==0) {
		res = KX_ACT_IPO_LOOPEND;
	} else if (strcmp(modename, S_KX_ACT_IPO_KEY2KEY_STRING)==0) {
		res = KX_ACT_IPO_KEY2KEY;
	} else if (strcmp(modename, S_KX_ACT_IPO_FROM_PROP_STRING)==0) {
		res = KX_ACT_IPO_FROM_PROP;
	}

	return res;
}

#ifdef WITH_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */


/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_IpoActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_IpoActuator",
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
	&SCA_IActuator::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_IpoActuator::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_IpoActuator::Attributes[] = {
	KX_PYATTRIBUTE_RW_FUNCTION("frameStart", KX_IpoActuator, pyattr_get_frame_start, pyattr_set_frame_start),
	KX_PYATTRIBUTE_RW_FUNCTION("frameEnd", KX_IpoActuator, pyattr_get_frame_end, pyattr_set_frame_end),
	KX_PYATTRIBUTE_STRING_RW("propName", 0, MAX_PROP_NAME, false, KX_IpoActuator, m_propname),
	KX_PYATTRIBUTE_STRING_RW("framePropName", 0, MAX_PROP_NAME, false, KX_IpoActuator, m_framepropname),
	KX_PYATTRIBUTE_INT_RW("mode", KX_ACT_IPO_NODEF+1, KX_ACT_IPO_MAX-1, true, KX_IpoActuator, m_type),
	KX_PYATTRIBUTE_BOOL_RW("useIpoAsForce", KX_IpoActuator, m_ipo_as_force),
	KX_PYATTRIBUTE_BOOL_RW("useIpoAdd", KX_IpoActuator, m_ipo_add),
	KX_PYATTRIBUTE_BOOL_RW("useIpoLocal", KX_IpoActuator, m_ipo_local),
	KX_PYATTRIBUTE_BOOL_RW("useChildren", KX_IpoActuator, m_recurse),
	
	{ NULL }	//Sentinel
};

PyObject* KX_IpoActuator::pyattr_get_frame_start(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_IpoActuator* self= static_cast<KX_IpoActuator*>(self_v);
	return PyFloat_FromDouble(self->m_startframe);
}

int KX_IpoActuator::pyattr_set_frame_start(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_IpoActuator* self= static_cast<KX_IpoActuator*>(self_v);
	float param = PyFloat_AsDouble(value);

	if (PyErr_Occurred()) {
		PyErr_SetString(PyExc_AttributeError, "frameStart = float: KX_IpoActuator, expected a float value");
		return PY_SET_ATTR_FAIL;
	}

	self->m_startframe = param;
	self->ResetStartTime();
	return PY_SET_ATTR_SUCCESS;
}

PyObject* KX_IpoActuator::pyattr_get_frame_end(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_IpoActuator* self= static_cast<KX_IpoActuator*>(self_v);
	return PyFloat_FromDouble(self->m_endframe);
}

int KX_IpoActuator::pyattr_set_frame_end(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_IpoActuator* self= static_cast<KX_IpoActuator*>(self_v);
	float param = PyFloat_AsDouble(value);

	if (PyErr_Occurred()) {
		PyErr_SetString(PyExc_AttributeError, "frameEnd = float: KX_IpoActuator, expected a float value");
		return PY_SET_ATTR_FAIL;
	}

	self->m_endframe = param;
	self->ResetStartTime();
	return PY_SET_ATTR_SUCCESS;
}

#endif // WITH_PYTHON

/* eof */
