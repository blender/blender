/**
 * Do Ipo stuff
 *
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "KX_IpoActuator.h"
#include "KX_GameObject.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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
/** Another poltergeist? This seems to be a very transient class... */
class CIpoAction : public CAction
{
	float		m_curtime;
	bool		m_resurse;
	KX_GameObject* m_gameobj;
	bool        m_ipo_as_force;
	bool        m_force_ipo_local;

public:
	CIpoAction(KX_GameObject* gameobj,
		float curtime,
		bool recurse, 
		bool ipo_as_force,
		bool force_ipo_local) :
	  m_curtime(curtime) ,
	  m_resurse(recurse),
	  m_gameobj(gameobj),
	  m_ipo_as_force(ipo_as_force),
	  m_force_ipo_local(force_ipo_local) 
	  {
		  /* intentionally empty */
	  };

	virtual void Execute() const
	{
		m_gameobj->UpdateIPO(
			m_curtime, 
			m_resurse, 
			m_ipo_as_force, 
			m_force_ipo_local);
	};

};


KX_IpoActuator::KX_IpoActuator(SCA_IObject* gameobj,
							   const STR_String& propname,
							   float starttime,
							   float endtime,
							   bool recurse,
							   int acttype,
							   bool ipo_as_force,
							   bool force_ipo_local,
							   PyTypeObject* T) 
	: SCA_IActuator(gameobj,T),
	m_starttime (starttime),
	m_endtime(endtime) ,
	m_localtime(starttime),
	m_recurse(recurse),
	m_type((IpoActType)acttype) ,
	m_direction(1),
	m_bNegativeEvent(false),
	m_propname(propname),
	m_ipo_as_force(ipo_as_force),
	m_force_ipo_local(force_ipo_local)
{
	// intentionally empty
}

void KX_IpoActuator::SetStart(float starttime) 
{ 
	m_starttime=starttime;
}

void KX_IpoActuator::SetEnd(float endtime) 
{ 
	m_endtime=endtime;
}


bool KX_IpoActuator::Update(double curtime,double delta_time)
{
	SCA_IActuator::Update(curtime,delta_time);
	// result = true if animation has to be continued, false if animation stops
	// maybe there are events for us in the queue !
	

	bool bNegativeEvent = false;
	int numevents = m_events.size();

	for (vector<CValue*>::iterator i=m_events.end(); !(i==m_events.begin());)
	{
		i--;
		if ((*i)->GetNumber() == 0.0f)
		{
			int ka=0;
			bNegativeEvent = true;
		}
		(*i)->Release();
		m_events.pop_back();
	}

	if (bNegativeEvent)
	{
		RemoveAllEvents();
	}
	

	double  start_smaller_then_end = ( m_starttime < m_endtime ? 1.0 : -1.0);

	double deltaframetime = start_smaller_then_end  * delta_time * KX_FIXED_FRAME_PER_SEC;

	bool result=true;
	
	switch (m_type)
	{
		
	case KX_ACT_IPO_PLAY:
		{
			
			if (start_smaller_then_end > 0.0)
				result = (m_localtime < m_endtime && !(m_localtime == m_starttime && bNegativeEvent));
			else
				result = (m_localtime > m_endtime && !(m_localtime == m_starttime && bNegativeEvent));
			if (result)
			{
				m_localtime += m_direction * deltaframetime;
				
				/* Perform clamping */
				if ((m_localtime*start_smaller_then_end)>(m_endtime*start_smaller_then_end))
					m_localtime=m_endtime;

				CIpoAction ipoaction(
					(KX_GameObject*)GetParent(), 
					m_localtime, 
					m_recurse, 
					m_ipo_as_force,
					m_force_ipo_local);
				GetParent()->Execute(ipoaction);
			} else
			{
				m_localtime=m_starttime;
				m_direction=1;
			}
			break;
		}
	case KX_ACT_IPO_PINGPONG:
		{
			result = true;
			if (bNegativeEvent && ((m_localtime == m_starttime )|| (m_localtime == m_endtime)))
			{
				result = false;
			} else
			{
				m_localtime += m_direction * deltaframetime;
			}
			
			if (m_localtime*start_smaller_then_end < m_starttime*start_smaller_then_end)
			{
				m_localtime = m_starttime;
				result = false;
				m_direction = 1;
			}else
			if (m_localtime*start_smaller_then_end > m_endtime*start_smaller_then_end)
			{
				m_localtime = m_endtime;
				result = false;
				m_direction = -1;
			} 
			
			CIpoAction ipoaction(
				(KX_GameObject*) GetParent(),
				m_localtime,
				m_recurse, 
				m_ipo_as_force,
				m_force_ipo_local);
			GetParent()->Execute(ipoaction);
			break;
		}

	case KX_ACT_IPO_FLIPPER:
		{
			result = true;
			if (numevents)
			{
				if (bNegativeEvent)
					m_direction = -1;
				else
					m_direction = 1;
			}

			m_localtime += m_direction * deltaframetime;

			if (m_localtime*start_smaller_then_end > m_endtime*start_smaller_then_end)
			{
				m_localtime = m_endtime;
			} else
			 if (m_localtime*start_smaller_then_end < m_starttime*start_smaller_then_end)
			 {
				m_localtime = m_starttime;
				result = false;
			 }
			
			CIpoAction ipoaction(
				(KX_GameObject*) GetParent(),
				m_localtime,
				m_recurse,
				m_ipo_as_force,
				m_force_ipo_local);
			GetParent()->Execute(ipoaction);
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
			} // fall through to loopend, and quit the ipo animation immediatly 
		}
	
		case KX_ACT_IPO_LOOPEND:
			{
				if (numevents){
					if (bNegativeEvent){
						m_bNegativeEvent = true;
					}
				}
				
				if (bNegativeEvent && m_localtime == m_starttime){
					result = false;
				} 
				else{
					if (m_localtime*start_smaller_then_end < m_endtime*start_smaller_then_end){
						m_localtime += m_direction * deltaframetime;
					}
					else{
						if (!m_bNegativeEvent){
							/* Perform wraparound */
							float slop = m_localtime-m_endtime;
							float length = fabs(m_starttime-m_endtime);
							m_localtime = m_starttime + (slop - (int(slop/length)*(int(length))));
							
						}
						else
						{	
							/* Perform clamping */
							if ((m_localtime*start_smaller_then_end)>(m_endtime*start_smaller_then_end))
								m_localtime=m_endtime;
							
							result = false;
							m_bNegativeEvent = false;
						}
					}
				}	
				CIpoAction ipoaction(
					(KX_GameObject*) GetParent(),
					m_localtime,
					m_recurse,
					m_ipo_as_force,
					m_force_ipo_local);
				GetParent()->Execute(ipoaction);
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
				CIpoAction ipoaction(
					(KX_GameObject*) GetParent(),
					m_localtime,
					m_recurse,
					m_ipo_as_force,
					m_force_ipo_local);
				GetParent()->Execute(ipoaction);

			 } else
			 {
				 result = false;
			 }
			break;
		}
		
	default:
		{
			result = false;
		}
	}

	return result;
}

KX_IpoActuator::IpoActType KX_IpoActuator::string2mode(char* modename) {
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
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"KX_IpoActuator",
	sizeof(KX_IpoActuator),
	0,
	PyDestructor,
	0,
	__getattr,
	__setattr,
	0, //&MyPyCompare,
	__repr,
	0, //&cvalue_as_number,
	0,
	0,
	0,
	0
};

PyParentObject KX_IpoActuator::Parents[] = {
	&KX_IpoActuator::Type,
	&SCA_IActuator::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef KX_IpoActuator::Methods[] = {
	{"set", (PyCFunction) KX_IpoActuator::sPySet, 
		METH_VARARGS, Set_doc},
	{"setProperty", (PyCFunction) KX_IpoActuator::sPySetProperty, 
		METH_VARARGS, SetProperty_doc},
	{"setStart", (PyCFunction) KX_IpoActuator::sPySetStart, 
		METH_VARARGS, SetStart_doc},
	{"getStart", (PyCFunction) KX_IpoActuator::sPyGetStart, 
		METH_VARARGS, GetStart_doc},
	{"setEnd", (PyCFunction) KX_IpoActuator::sPySetEnd, 
		METH_VARARGS, SetEnd_doc},
	{"getEnd", (PyCFunction) KX_IpoActuator::sPyGetEnd, 
		METH_VARARGS, GetEnd_doc},
	{"setIpoAsForce", (PyCFunction) KX_IpoActuator::sPySetIpoAsForce, 
		METH_VARARGS, SetIpoAsForce_doc},
	{"getIpoAsForce", (PyCFunction) KX_IpoActuator::sPyGetIpoAsForce, 
		METH_VARARGS, GetIpoAsForce_doc},
	{"setType", (PyCFunction) KX_IpoActuator::sPySetType, 
		METH_VARARGS, SetType_doc},
	{"getType", (PyCFunction) KX_IpoActuator::sPyGetType, 
		METH_VARARGS, GetType_doc},	
	{NULL,NULL} //Sentinel
};

PyObject* KX_IpoActuator::_getattr(char* attr) {
	_getattr_up(SCA_IActuator);
}



/* set --------------------------------------------------------------------- */
char KX_IpoActuator::Set_doc[] = 
"set(mode, startframe, endframe, force?)\n"
"\t - mode:       Play, PingPong, Flipper, LoopStop, LoopEnd or FromProp (string)\n"
"\t - startframe: first frame to use (int)\n"
"\t - endframe  : last frame to use (int)\n"
"\t - force?    : interpret this ipo as a force? (KX_TRUE, KX_FALSE)"
"\tSet the properties of the actuator.\n";
PyObject* KX_IpoActuator::PySet(PyObject* self, 
								PyObject* args, 
								PyObject* kwds) {
	/* sets modes PLAY, PINGPONG, FLIPPER, LOOPSTOP, LOOPEND                 */
	/* arg 1 = mode string, arg 2 = startframe, arg3 = stopframe,            */
	/* arg4 = force toggle                                                   */
	char* mode;
	int forceToggle;
	IpoActType modenum;
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
		m_starttime    = startFrame;
		m_endtime      = stopFrame;
		m_ipo_as_force = PyArgToBool(forceToggle);
		break;
	default:
		; /* error */
	}

	Py_Return;
}

/* set property  ----------------------------------------------------------- */
char KX_IpoActuator::SetProperty_doc[] = 
"setProperty(propname)\n"
"\t - propname: name of the property (string)\n"
"\tSet the property to be used in FromProp mode.\n";
PyObject* KX_IpoActuator::PySetProperty(PyObject* self, 
										PyObject* args, 
										PyObject* kwds) {
	/* mode is implicit here, but not supported yet... */
	/* args: property */
	char *propertyName;
	if(!PyArg_ParseTuple(args, "s", &propertyName)) {
		return NULL;
	}

	Py_Return;
}

/* 4. setStart:                                                              */
char KX_IpoActuator::SetStart_doc[] = 
"setStart(frame)\n"
"\t - frame: first frame to use (int)\n"
"\tSet the frame from which the ipo starts playing.\n";
PyObject* KX_IpoActuator::PySetStart(PyObject* self, 
									 PyObject* args, 
									 PyObject* kwds) {
	float startArg;
	if(!PyArg_ParseTuple(args, "f", &startArg)) {
		return NULL;		
	}
	
	m_starttime = startArg;

	Py_Return;
}
/* 5. getStart:                                                              */
char KX_IpoActuator::GetStart_doc[] = 
"getStart()\n"
"\tReturns the frame from which the ipo starts playing.\n";
PyObject* KX_IpoActuator::PyGetStart(PyObject* self, 
									 PyObject* args, 
									 PyObject* kwds) {
	return PyFloat_FromDouble(m_starttime);
}

/* 6. setEnd:                                                                */
char KX_IpoActuator::SetEnd_doc[] = 
"setEnd(frame)\n"
"\t - frame: last frame to use (int)\n"
"\tSet the frame at which the ipo stops playing.\n";
PyObject* KX_IpoActuator::PySetEnd(PyObject* self, 
								   PyObject* args, 
								   PyObject* kwds) {
	float endArg;
	if(!PyArg_ParseTuple(args, "f", &endArg)) {
		return NULL;		
	}
	
	m_endtime = endArg;

	Py_Return;
}
/* 7. getEnd:                                                                */
char KX_IpoActuator::GetEnd_doc[] = 
"getEnd()\n"
"\tReturns the frame at which the ipo stops playing.\n";
PyObject* KX_IpoActuator::PyGetEnd(PyObject* self, 
								   PyObject* args, 
								   PyObject* kwds) {
	return PyFloat_FromDouble(m_endtime);
}

/* 6. setIpoAsForce:                                                           */
char KX_IpoActuator::SetIpoAsForce_doc[] = 
"setIpoAsForce(force?)\n"
"\t - force?    : interpret this ipo as a force? (KX_TRUE, KX_FALSE)\n"
"\tSet whether to interpret the ipo as a force rather than a displacement.\n";
PyObject* KX_IpoActuator::PySetIpoAsForce(PyObject* self, 
										  PyObject* args, 
										  PyObject* kwds) { 
	int boolArg;
	
	if (!PyArg_ParseTuple(args, "i", &boolArg)) {
		return NULL;
	}

	m_ipo_as_force = PyArgToBool(boolArg);
	
	Py_Return;	
}
/* 7. getIpoAsForce:                                                         */
char KX_IpoActuator::GetIpoAsForce_doc[] = 
"getIpoAsForce()\n"
"\tReturns whether to interpret the ipo as a force rather than a displacement.\n";
PyObject* KX_IpoActuator::PyGetIpoAsForce(PyObject* self, 
									   	  PyObject* args, 
										  PyObject* kwds) {
	return BoolToPyArg(m_ipo_as_force);
}

/* 8. setType:                                                               */
char KX_IpoActuator::SetType_doc[] = 
"setType(mode)\n"
"\t - mode: Play, PingPong, Flipper, LoopStop, LoopEnd or FromProp (string)\n"
"\tSet the operation mode of the actuator.\n";
PyObject* KX_IpoActuator::PySetType(PyObject* self, 
									PyObject* args, 
									PyObject* kwds) {
	int typeArg;
	
	if (!PyArg_ParseTuple(args, "i", &typeArg)) {
		return NULL;
	}
	
	if ( (typeArg > KX_ACT_IPO_NODEF) 
		 && (typeArg < KX_ACT_IPO_KEY2KEY) ) {
		m_type = (IpoActType) typeArg;
	}
	
	Py_Return;
}
/* 9. getType:                                                               */
char KX_IpoActuator::GetType_doc[] = 
"getType()\n"
"\tReturns the operation mode of the actuator.\n";
PyObject* KX_IpoActuator::PyGetType(PyObject* self, 
									PyObject* args, 
									PyObject* kwds) {
	return PyInt_FromLong(m_type);
}

/* 10. setForceIpoActsLocal:                                                 */
char KX_IpoActuator::SetForceIpoActsLocal_doc[] = 
"setForceIpoActsLocal(local?)\n"
"\t - local?    : Apply the ipo-as-force in the object's local\n"
"\t               coordinates? (KX_TRUE, KX_FALSE)\n"
"\tSet whether to apply the force in the object's local\n"
"\tcoordinates rather than the world global coordinates.\n";
PyObject* KX_IpoActuator::PySetForceIpoActsLocal(PyObject* self, 
										         PyObject* args, 
						       				     PyObject* kwds) { 
	int boolArg;
	
	if (!PyArg_ParseTuple(args, "i", &boolArg)) {
		return NULL;
	}

	m_force_ipo_local = PyArgToBool(boolArg);
	
	Py_Return;	
}
/* 11. getForceIpoActsLocal:                                                */
char KX_IpoActuator::GetForceIpoActsLocal_doc[] = 
"getForceIpoActsLocal()\n"
"\tReturn whether to apply the force in the object's local\n"
"\tcoordinates rather than the world global coordinates.\n";
PyObject* KX_IpoActuator::PyGetForceIpoActsLocal(PyObject* self, 
									   	         PyObject* args, 
										         PyObject* kwds) {
	return BoolToPyArg(m_force_ipo_local);
}


/* eof */
