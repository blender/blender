/**
 * Do Ipo stuff
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

#if defined (__sgi)
#include <math.h>
#else
#include <cmath>
#endif
 
#include "KX_IpoActuator.h"
#include "KX_GameObject.h"
#include "FloatValue.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "KX_KetsjiEngine.h"

/* ------------------------------------------------------------------------- */
/* Type strings                                                              */
/* ------------------------------------------------------------------------- */

STR_String KX_IpoActuator::S_KX_ACT_IPO_PLAY_STRING      = "Play";
STR_String KX_IpoActuator::S_KX_ACT_IPO_PINGPONG_STRING  = "PingPong";
STR_String KX_IpoActuator::S_KX_ACT_IPO_FLIPPER_STRING   = "Flipper";
STR_String KX_IpoActuator::S_KX_ACT_IPO_LOOPSTOP_STRING  = "LoopStop";
STR_String KX_IpoActuator::S_KX_ACT_IPO_LOOPEND_STRING   = "LoopEnd";
STR_String KX_IpoActuator::S_KX_ACT_IPO_KEY2KEY_STRING   = "Key2key";
STR_String KX_IpoActuator::S_KX_ACT_IPO_FROM_PROP_STRING = "FromProp";

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
							   bool ipo_local,
							   PyTypeObject* T) 
	: SCA_IActuator(gameobj,T),
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
	m_starttime = -2.0*fabs(m_endframe - m_startframe) - 1.0;
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
	int numevents = 0;
	bool bIpoStart = false;

	curtime -= KX_KetsjiEngine::GetSuspendedDelta();

	if (frame)
	{
		numevents = m_events.size();
		bNegativeEvent = IsNegativeEvent();
		RemoveAllEvents();
	}
	
	float  start_smaller_then_end = ( m_startframe < m_endframe ? 1.0f : -1.0f);

	bool result=true;
	if (!bNegativeEvent)
	{
		if (m_starttime < -2.0f*start_smaller_then_end*(m_endframe - m_startframe))
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
				numevents = 0;
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
		if (numevents){
			if (bNegativeEvent && m_bIpoPlaying){
				m_bNegativeEvent = true;
			}
		}
		
		if (bNegativeEvent && !m_bIpoPlaying){
			result = false;
		} 
		else
		{
			if (m_localtime*start_smaller_then_end < m_endframe*start_smaller_then_end)
			{
				SetLocalTime(curtime);
			}
			else{
				if (!m_bNegativeEvent){
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
			m_starttime = -2.0*start_smaller_then_end*(m_endframe - m_startframe) - 1.0;
		m_bIpoPlaying = false;
	}

	return result;
}

int KX_IpoActuator::string2mode(char* modename) {
	IpoActType res = KX_ACT_IPO_NODEF;

	if (modename == S_KX_ACT_IPO_PLAY_STRING) { 
		res = KX_ACT_IPO_PLAY;
	} else if (modename == S_KX_ACT_IPO_PINGPONG_STRING) {
		res = KX_ACT_IPO_PINGPONG;
	} else if (modename == S_KX_ACT_IPO_FLIPPER_STRING) {
		res = KX_ACT_IPO_FLIPPER;
	} else if (modename == S_KX_ACT_IPO_LOOPSTOP_STRING) {
		res = KX_ACT_IPO_LOOPSTOP;
	} else if (modename == S_KX_ACT_IPO_LOOPEND_STRING) {
		res = KX_ACT_IPO_LOOPEND;
	} else if (modename == S_KX_ACT_IPO_KEY2KEY_STRING) {
		res = KX_ACT_IPO_KEY2KEY;
	} else if (modename == S_KX_ACT_IPO_FROM_PROP_STRING) {
		res = KX_ACT_IPO_FROM_PROP;
	}

	return res;
}

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */



/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_IpoActuator::Type = {
	PyObject_HEAD_INIT(NULL)
	0,
	"KX_IpoActuator",
	sizeof(KX_IpoActuator),
	0,
	PyDestructor,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,
	py_base_getattro,
	py_base_setattro,
	0,0,0,0,0,0,0,0,0,
	Methods
};

PyParentObject KX_IpoActuator::Parents[] = {
	&KX_IpoActuator::Type,
	&SCA_IActuator::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef KX_IpoActuator::Methods[] = {
	{"set", (PyCFunction) KX_IpoActuator::sPySet, METH_VARARGS, (PY_METHODCHAR)Set_doc},
	
	// deprecated 
	{"setProperty", (PyCFunction) KX_IpoActuator::sPySetProperty, METH_VARARGS, (PY_METHODCHAR)SetProperty_doc},
	{"setStart", (PyCFunction) KX_IpoActuator::sPySetStart, METH_VARARGS, (PY_METHODCHAR)SetStart_doc},
	{"getStart", (PyCFunction) KX_IpoActuator::sPyGetStart, METH_NOARGS, (PY_METHODCHAR)GetStart_doc},
	{"setEnd", (PyCFunction) KX_IpoActuator::sPySetEnd, METH_VARARGS, (PY_METHODCHAR)SetEnd_doc},
	{"getEnd", (PyCFunction) KX_IpoActuator::sPyGetEnd, METH_NOARGS, (PY_METHODCHAR)GetEnd_doc},
	{"setIpoAsForce", (PyCFunction) KX_IpoActuator::sPySetIpoAsForce, METH_VARARGS, (PY_METHODCHAR)SetIpoAsForce_doc},
	{"getIpoAsForce", (PyCFunction) KX_IpoActuator::sPyGetIpoAsForce, METH_NOARGS, (PY_METHODCHAR)GetIpoAsForce_doc},
	{"setIpoAdd", (PyCFunction) KX_IpoActuator::sPySetIpoAdd, METH_VARARGS, (PY_METHODCHAR)SetIpoAdd_doc},
	{"getIpoAdd", (PyCFunction) KX_IpoActuator::sPyGetIpoAdd, METH_NOARGS, (PY_METHODCHAR)GetIpoAdd_doc},
	{"setForceIpoActsLocal", (PyCFunction) KX_IpoActuator::sPySetForceIpoActsLocal, METH_VARARGS, (PY_METHODCHAR)SetForceIpoActsLocal_doc},
	{"getForceIpoActsLocal", (PyCFunction) KX_IpoActuator::sPyGetForceIpoActsLocal,	METH_NOARGS, (PY_METHODCHAR)GetForceIpoActsLocal_doc},
	{"setType", (PyCFunction) KX_IpoActuator::sPySetType, METH_VARARGS, (PY_METHODCHAR)SetType_doc},
	{"getType", (PyCFunction) KX_IpoActuator::sPyGetType, METH_NOARGS, (PY_METHODCHAR)GetType_doc},	
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_IpoActuator::Attributes[] = {
	KX_PYATTRIBUTE_FLOAT_RW("startFrame", 0, 300000, KX_IpoActuator, m_startframe),
	KX_PYATTRIBUTE_FLOAT_RW("endFrame", 0, 300000, KX_IpoActuator, m_endframe),
	KX_PYATTRIBUTE_STRING_RW("propName", 0, 64, false, KX_IpoActuator, m_propname),
	KX_PYATTRIBUTE_STRING_RW("framePropName", 0, 64, false, KX_IpoActuator, m_framepropname),
	KX_PYATTRIBUTE_INT_RW("type", KX_ACT_IPO_NODEF+1, KX_ACT_IPO_MAX-1, true, KX_IpoActuator, m_type),
	KX_PYATTRIBUTE_BOOL_RW("useIpoAsForce", KX_IpoActuator, m_ipo_as_force),
	KX_PYATTRIBUTE_BOOL_RW("useIpoAdd", KX_IpoActuator, m_ipo_add),
	KX_PYATTRIBUTE_BOOL_RW("useIpoLocal", KX_IpoActuator, m_ipo_local),
	KX_PYATTRIBUTE_BOOL_RW("useChildren", KX_IpoActuator, m_recurse),
	
	{ NULL }	//Sentinel
};

PyObject* KX_IpoActuator::py_getattro(PyObject *attr) {
	py_getattro_up(SCA_IActuator);
}

int KX_IpoActuator::py_setattro(PyObject *attr, PyObject *value)	// py_setattro method
{
	py_setattro_up(SCA_IActuator);
}

/* set --------------------------------------------------------------------- */
const char KX_IpoActuator::Set_doc[] = 
"set(type, startframe, endframe, mode?)\n"
"\t - type:       Play, PingPong, Flipper, LoopStop, LoopEnd or FromProp (string)\n"
"\t - startframe: first frame to use (int)\n"
"\t - endframe  : last frame to use (int)\n"
"\t - mode?     : special mode (0=normal, 1=interpret location as force, 2=additive)"
"\tSet the properties of the actuator.\n";
PyObject* KX_IpoActuator::PySet(PyObject* self, 
								PyObject* args, 
								PyObject* kwds) {
	
	ShowDeprecationWarning("set()", "a number properties");
									
	/* sets modes PLAY, PINGPONG, FLIPPER, LOOPSTOP, LOOPEND                 */
	/* arg 1 = mode string, arg 2 = startframe, arg3 = stopframe,            */
	/* arg4 = force toggle                                                   */
	char* mode;
	int forceToggle;
	int modenum;
	int startFrame, stopFrame;
	if(!PyArg_ParseTuple(args, "siii", &mode, &startFrame, 
						 &stopFrame, &forceToggle)) {
		return NULL;
	}
	modenum = string2mode(mode);
	
	switch (modenum) {
	case KX_ACT_IPO_PLAY:
	case KX_ACT_IPO_PINGPONG:
	case KX_ACT_IPO_FLIPPER:
	case KX_ACT_IPO_LOOPSTOP:
	case KX_ACT_IPO_LOOPEND:
		m_type         = modenum;
		m_startframe    = startFrame;
		m_endframe      = stopFrame;
		m_ipo_as_force = forceToggle == 1;
		m_ipo_add = forceToggle == 2;
		break;
	default:
		; /* error */
	}

	Py_RETURN_NONE;
}

/* set property  ----------------------------------------------------------- */
const char KX_IpoActuator::SetProperty_doc[] = 
"setProperty(propname)\n"
"\t - propname: name of the property (string)\n"
"\tSet the property to be used in FromProp mode.\n";
PyObject* KX_IpoActuator::PySetProperty(PyObject* self, 
										PyObject* args, 
										PyObject* kwds) {

	ShowDeprecationWarning("setProperty()", "the propName property");

	/* mode is implicit here, but not supported yet... */
	/* args: property */
	char *propertyName;
	if(!PyArg_ParseTuple(args, "s", &propertyName)) {
		return NULL;
	}

	m_propname = propertyName;
	
	Py_RETURN_NONE;
}

/* 4. setStart:                                                              */
const char KX_IpoActuator::SetStart_doc[] = 
"setStart(frame)\n"
"\t - frame: first frame to use (int)\n"
"\tSet the frame from which the ipo starts playing.\n";
PyObject* KX_IpoActuator::PySetStart(PyObject* self, 
									 PyObject* args, 
									 PyObject* kwds) {

	ShowDeprecationWarning("setStart()", "the startFrame property");

	float startArg;
	if(!PyArg_ParseTuple(args, "f", &startArg)) {
		return NULL;		
	}
	
	m_startframe = startArg;

	Py_RETURN_NONE;
}
/* 5. getStart:                                                              */
const char KX_IpoActuator::GetStart_doc[] = 
"getStart()\n"
"\tReturns the frame from which the ipo starts playing.\n";
PyObject* KX_IpoActuator::PyGetStart(PyObject* self) {
	ShowDeprecationWarning("getStart()", "the startFrame property");
	return PyFloat_FromDouble(m_startframe);
}

/* 6. setEnd:                                                                */
const char KX_IpoActuator::SetEnd_doc[] = 
"setEnd(frame)\n"
"\t - frame: last frame to use (int)\n"
"\tSet the frame at which the ipo stops playing.\n";
PyObject* KX_IpoActuator::PySetEnd(PyObject* self, 
								   PyObject* args, 
								   PyObject* kwds) {
	ShowDeprecationWarning("setEnd()", "the endFrame property");
	float endArg;
	if(!PyArg_ParseTuple(args, "f", &endArg)) {
		return NULL;		
	}
	
	m_endframe = endArg;

	Py_RETURN_NONE;
}
/* 7. getEnd:                                                                */
const char KX_IpoActuator::GetEnd_doc[] = 
"getEnd()\n"
"\tReturns the frame at which the ipo stops playing.\n";
PyObject* KX_IpoActuator::PyGetEnd(PyObject* self) {
	ShowDeprecationWarning("getEnd()", "the endFrame property");
	return PyFloat_FromDouble(m_endframe);
}

/* 6. setIpoAsForce:                                                           */
const char KX_IpoActuator::SetIpoAsForce_doc[] = 
"setIpoAsForce(force?)\n"
"\t - force?    : interpret this ipo as a force? (KX_TRUE, KX_FALSE)\n"
"\tSet whether to interpret the ipo as a force rather than a displacement.\n";
PyObject* KX_IpoActuator::PySetIpoAsForce(PyObject* self, 
										  PyObject* args, 
										  PyObject* kwds) { 
	ShowDeprecationWarning("setIpoAsForce()", "the useIpoAsForce property");
	int boolArg;
	
	if (!PyArg_ParseTuple(args, "i", &boolArg)) {
		return NULL;
	}

	m_ipo_as_force = PyArgToBool(boolArg);
	if (m_ipo_as_force)
		m_ipo_add = false;
	
	Py_RETURN_NONE;	
}
/* 7. getIpoAsForce:                                                         */
const char KX_IpoActuator::GetIpoAsForce_doc[] = 
"getIpoAsForce()\n"
"\tReturns whether to interpret the ipo as a force rather than a displacement.\n";
PyObject* KX_IpoActuator::PyGetIpoAsForce(PyObject* self) {
	ShowDeprecationWarning("getIpoAsForce()", "the useIpoAsForce property");
	return BoolToPyArg(m_ipo_as_force);
}

/* 6. setIpoAsForce:                                                           */
const char KX_IpoActuator::SetIpoAdd_doc[] = 
"setIpoAdd(add?)\n"
"\t - add?    : add flag (KX_TRUE, KX_FALSE)\n"
"\tSet whether to interpret the ipo as additive rather than absolute.\n";
PyObject* KX_IpoActuator::PySetIpoAdd(PyObject* self, 
									  PyObject* args, 
									  PyObject* kwds) { 
	ShowDeprecationWarning("setIpoAdd()", "the useIpoAdd property");
	int boolArg;
	
	if (!PyArg_ParseTuple(args, "i", &boolArg)) {
		return NULL;
	}

	m_ipo_add = PyArgToBool(boolArg);
	if (m_ipo_add)
		m_ipo_as_force = false;
	
	Py_RETURN_NONE;	
}
/* 7. getIpoAsForce:                                                         */
const char KX_IpoActuator::GetIpoAdd_doc[] = 
"getIpoAsAdd()\n"
"\tReturns whether to interpret the ipo as additive rather than absolute.\n";
PyObject* KX_IpoActuator::PyGetIpoAdd(PyObject* self) {
	ShowDeprecationWarning("getIpoAdd()", "the useIpoAdd property");
	return BoolToPyArg(m_ipo_add);
}

/* 8. setType:                                                               */
const char KX_IpoActuator::SetType_doc[] = 
"setType(mode)\n"
"\t - mode: Play, PingPong, Flipper, LoopStop, LoopEnd or FromProp (string)\n"
"\tSet the operation mode of the actuator.\n";
PyObject* KX_IpoActuator::PySetType(PyObject* self, 
									PyObject* args, 
									PyObject* kwds) {
	ShowDeprecationWarning("setType()", "the type property");
	int typeArg;
	
	if (!PyArg_ParseTuple(args, "i", &typeArg)) {
		return NULL;
	}
	
	if ( (typeArg > KX_ACT_IPO_NODEF) 
		 && (typeArg < KX_ACT_IPO_MAX) ) {
		m_type = typeArg;
	}
	
	Py_RETURN_NONE;
}
/* 9. getType:                                                               */
const char KX_IpoActuator::GetType_doc[] = 
"getType()\n"
"\tReturns the operation mode of the actuator.\n";
PyObject* KX_IpoActuator::PyGetType(PyObject* self) {
	ShowDeprecationWarning("getType()", "the type property");
	return PyInt_FromLong(m_type);
}

/* 10. setForceIpoActsLocal:                                                 */
const char KX_IpoActuator::SetForceIpoActsLocal_doc[] = 
"setForceIpoActsLocal(local?)\n"
"\t - local?    : Apply the ipo-as-force in the object's local\n"
"\t               coordinates? (KX_TRUE, KX_FALSE)\n"
"\tSet whether to apply the force in the object's local\n"
"\tcoordinates rather than the world global coordinates.\n";
PyObject* KX_IpoActuator::PySetForceIpoActsLocal(PyObject* self, 
										         PyObject* args, 
						       				     PyObject* kwds) { 
	ShowDeprecationWarning("setForceIpoActsLocal()", "the useIpoLocal property");
	int boolArg;
	
	if (!PyArg_ParseTuple(args, "i", &boolArg)) {
		return NULL;
	}

	m_ipo_local = PyArgToBool(boolArg);
	
	Py_RETURN_NONE;	
}
/* 11. getForceIpoActsLocal:                                                */
const char KX_IpoActuator::GetForceIpoActsLocal_doc[] = 
"getForceIpoActsLocal()\n"
"\tReturn whether to apply the force in the object's local\n"
"\tcoordinates rather than the world global coordinates.\n";
PyObject* KX_IpoActuator::PyGetForceIpoActsLocal(PyObject* self) {
	ShowDeprecationWarning("getForceIpoActsLocal()", "the useIpoLocal property");
	return BoolToPyArg(m_ipo_local);
}


/* eof */
