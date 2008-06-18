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

#if defined (__sgi)
#include <math.h>
#else
#include <cmath>
#endif

#include "SCA_LogicManager.h"
#include "BL_ShapeActionActuator.h"
#include "BL_ActionActuator.h"
#include "BL_ShapeDeformer.h"
#include "KX_GameObject.h"
#include "STR_HashedString.h"
#include "DNA_action_types.h"
#include "DNA_nla_types.h"
#include "DNA_actuator_types.h"
#include "BKE_action.h"
#include "DNA_armature_types.h"
#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "MT_Matrix4x4.h"
#include "BKE_utildefines.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

BL_ShapeActionActuator::~BL_ShapeActionActuator()
{
}

void BL_ShapeActionActuator::ProcessReplica()
{
	m_localtime=m_startframe;
	m_lastUpdate=-1;
}

void BL_ShapeActionActuator::SetBlendTime (float newtime)
{
	m_blendframe = newtime;
}

CValue* BL_ShapeActionActuator::GetReplica() 
{
	BL_ShapeActionActuator* replica = new BL_ShapeActionActuator(*this);//m_float,GetName());
	replica->ProcessReplica();
	
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);
	return replica;
}

bool BL_ShapeActionActuator::ClampLocalTime()
{
	if (m_startframe < m_endframe)	{
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

void BL_ShapeActionActuator::SetStartTime(float curtime)
{
	float direction = m_startframe < m_endframe ? 1.0 : -1.0;
	
	if (!(m_flag & ACT_FLAG_REVERSE))
		m_starttime = curtime - direction*(m_localtime - m_startframe)/KX_KetsjiEngine::GetAnimFrameRate();
	else
		m_starttime = curtime - direction*(m_endframe - m_localtime)/KX_KetsjiEngine::GetAnimFrameRate();
}

void BL_ShapeActionActuator::SetLocalTime(float curtime)
{
	float delta_time = (curtime - m_starttime)*KX_KetsjiEngine::GetAnimFrameRate();
	
	if (m_endframe < m_startframe)
		delta_time = -delta_time;

	if (!(m_flag & ACT_FLAG_REVERSE))
		m_localtime = m_startframe + delta_time;
	else
		m_localtime = m_endframe - delta_time;
}

void BL_ShapeActionActuator::BlendShape(Key* key, float srcweight)
{
	vector<float>::const_iterator it;
	float dstweight;
	int i;
	KeyBlock *kb;
	
	dstweight = 1.0F - srcweight;

	for (it=m_blendshape.begin(), kb = (KeyBlock*)key->block.first; 
		 kb && it != m_blendshape.end(); 
		 kb = (KeyBlock*)kb->next, it++) {
		kb->curval = kb->curval * dstweight + (*it) * srcweight;
	}
}

bool BL_ShapeActionActuator::Update(double curtime, bool frame)
{
	bool bNegativeEvent = false;
	bool bPositiveEvent = false;
	bool keepgoing = true;
	bool wrap = false;
	bool apply=true;
	int	priority;
	float newweight;
	
	// result = true if animation has to be continued, false if animation stops
	// maybe there are events for us in the queue !
	if (frame)
	{
		for (vector<CValue*>::iterator i=m_events.begin(); !(i==m_events.end());i++)
		{
			if ((*i)->GetNumber() == 0.0f)
				bNegativeEvent = true;
			else
				bPositiveEvent= true;
			(*i)->Release();
		
		}
		m_events.clear();
		
		if (bPositiveEvent)
			m_flag |= ACT_FLAG_ACTIVE;
		
		if (bNegativeEvent)
		{
			if (!(m_flag & ACT_FLAG_ACTIVE))
				return false;
			m_flag &= ~ACT_FLAG_ACTIVE;
		}
	}
	
	/*	This action can only be attached to a deform object */
	BL_DeformableGameObject *obj = (BL_DeformableGameObject*)GetParent();
	float length = m_endframe - m_startframe;
	
	priority = m_priority;
	
	/* Determine pre-incrementation behaviour and set appropriate flags */
	switch (m_playtype){
	case ACT_ACTION_MOTION:
		if (bNegativeEvent){
			keepgoing=false;
			apply=false;
		};
		break;
	case ACT_ACTION_FROM_PROP:
		if (bNegativeEvent){
			apply=false;
			keepgoing=false;
		}
		break;
	case ACT_ACTION_LOOP_END:
		if (bPositiveEvent){
			if (!(m_flag & ACT_FLAG_LOCKINPUT)){
				m_flag &= ~ACT_FLAG_KEYUP;
				m_flag &= ~ACT_FLAG_REVERSE;
				m_flag |= ACT_FLAG_LOCKINPUT;
				m_localtime = m_startframe;
				m_starttime = curtime;
			}
		}
		if (bNegativeEvent){
			m_flag |= ACT_FLAG_KEYUP;
		}
		break;
	case ACT_ACTION_LOOP_STOP:
		if (bPositiveEvent){
			if (!(m_flag & ACT_FLAG_LOCKINPUT)){
				m_flag &= ~ACT_FLAG_REVERSE;
				m_flag &= ~ACT_FLAG_KEYUP;
				m_flag |= ACT_FLAG_LOCKINPUT;
				SetStartTime(curtime);
			}
		}
		if (bNegativeEvent){
			m_flag |= ACT_FLAG_KEYUP;
			m_flag &= ~ACT_FLAG_LOCKINPUT;
			keepgoing=false;
			apply=false;
		}
		break;
	case ACT_ACTION_FLIPPER:
		if (bPositiveEvent){
			if (!(m_flag & ACT_FLAG_LOCKINPUT)){
				m_flag &= ~ACT_FLAG_REVERSE;
				m_flag |= ACT_FLAG_LOCKINPUT;
				SetStartTime(curtime);
			}
		}
		else if (bNegativeEvent){
			m_flag |= ACT_FLAG_REVERSE;
			m_flag &= ~ACT_FLAG_LOCKINPUT;
			SetStartTime(curtime);
		}
		break;
	case ACT_ACTION_PLAY:
		if (bPositiveEvent){
			if (!(m_flag & ACT_FLAG_LOCKINPUT)){
				m_flag &= ~ACT_FLAG_REVERSE;
				m_localtime = m_starttime;
				m_starttime = curtime;
				m_flag |= ACT_FLAG_LOCKINPUT;
			}
		}
		break;
	default:
		break;
	}
	
	/* Perform increment */
	if (keepgoing){
		if (m_playtype == ACT_ACTION_MOTION){
			MT_Point3	newpos;
			MT_Point3	deltapos;
			
			newpos = obj->NodeGetWorldPosition();
			
			/* Find displacement */
			deltapos = newpos-m_lastpos;
			m_localtime += (length/m_stridelength) * deltapos.length();
			m_lastpos = newpos;
		}
		else{
			SetLocalTime(curtime);
		}
	}
	
	/* Check if a wrapping response is needed */
	if (length){
		if (m_localtime < m_startframe || m_localtime > m_endframe)
		{
			m_localtime = m_startframe + fmod(m_localtime, length);
			wrap = true;
		}
	}
	else
		m_localtime = m_startframe;
	
	/* Perform post-increment tasks */
	switch (m_playtype){
	case ACT_ACTION_FROM_PROP:
		{
			CValue* propval = GetParent()->GetProperty(m_propname);
			if (propval)
				m_localtime = propval->GetNumber();
			
			if (bNegativeEvent){
				keepgoing=false;
			}
		}
		break;
	case ACT_ACTION_MOTION:
		break;
	case ACT_ACTION_LOOP_STOP:
		break;
	case ACT_ACTION_FLIPPER:
		if (wrap){
			if (!(m_flag & ACT_FLAG_REVERSE)){
				m_localtime=m_endframe;
				//keepgoing = false;
			}
			else {
				m_localtime=m_startframe;
				keepgoing = false;
			}
		}
		break;
	case ACT_ACTION_LOOP_END:
		if (wrap){
			if (m_flag & ACT_FLAG_KEYUP){
				keepgoing = false;
				m_localtime = m_endframe;
				m_flag &= ~ACT_FLAG_LOCKINPUT;
			}
			SetStartTime(curtime);
		}
		break;
	case ACT_ACTION_PLAY:
		if (wrap){
			m_localtime = m_endframe;
			keepgoing = false;
			m_flag &= ~ACT_FLAG_LOCKINPUT;
		}
		break;
	default:
		keepgoing = false;
		break;
	}
	
	
	if (bNegativeEvent)
		m_blendframe=0.0f;
	
	/* Apply the pose if necessary*/
	if (apply) {

		/* Priority test */
		if (obj->SetActiveAction(this, priority, curtime)){
			Key *key = obj->GetKey();

			if (!key) {
				// this could happen if the mesh was changed in the middle of an action
				// and the new mesh has no key, stop the action
				keepgoing = false;
			}
			else {
				ListBase tchanbase= {NULL, NULL};
			
				if (m_blendin && m_blendframe==0.0f){
					// this is the start of the blending, remember the startup shape
					obj->GetShape(m_blendshape);
					m_blendstart = curtime;
				}
				// only interested in shape channel
				extract_ipochannels_from_action(&tchanbase, &key->id, m_action, "Shape", m_localtime);
		
				if (!execute_ipochannels(&tchanbase)) {
					// no update, this is possible if action does not match the keys, stop the action
					keepgoing = false;
				} 
				else {
					// the key have changed, apply blending if needed
					if (m_blendin && (m_blendframe<m_blendin)){
						newweight = (m_blendframe/(float)m_blendin);

						BlendShape(key, 1.0f - newweight);

						/* Increment current blending percentage */
						m_blendframe = (curtime - m_blendstart)*KX_KetsjiEngine::GetAnimFrameRate();
						if (m_blendframe>m_blendin)
							m_blendframe = m_blendin;
					}
					m_lastUpdate = m_localtime;
				}
				BLI_freelistN(&tchanbase);
			}
		}
		else{
			m_blendframe = 0.0f;
		}
	}
	
	if (!keepgoing){
		m_blendframe = 0.0f;
	}
	return keepgoing;
};

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */

PyTypeObject BL_ShapeActionActuator::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
		0,
		"BL_ShapeActionActuator",
		sizeof(BL_ShapeActionActuator),
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

PyParentObject BL_ShapeActionActuator::Parents[] = {
	&BL_ShapeActionActuator::Type,
		&SCA_IActuator::Type,
		&SCA_ILogicBrick::Type,
		&CValue::Type,
		NULL
};

PyMethodDef BL_ShapeActionActuator::Methods[] = {
	{"setAction", (PyCFunction) BL_ShapeActionActuator::sPySetAction, METH_VARARGS, SetAction_doc},
	{"setStart", (PyCFunction) BL_ShapeActionActuator::sPySetStart, METH_VARARGS, SetStart_doc},
	{"setEnd", (PyCFunction) BL_ShapeActionActuator::sPySetEnd, METH_VARARGS, SetEnd_doc},
	{"setBlendin", (PyCFunction) BL_ShapeActionActuator::sPySetBlendin, METH_VARARGS, SetBlendin_doc},
	{"setPriority", (PyCFunction) BL_ShapeActionActuator::sPySetPriority, METH_VARARGS, SetPriority_doc},
	{"setFrame", (PyCFunction) BL_ShapeActionActuator::sPySetFrame, METH_VARARGS, SetFrame_doc},
	{"setProperty", (PyCFunction) BL_ShapeActionActuator::sPySetProperty, METH_VARARGS, SetProperty_doc},
	{"setBlendtime", (PyCFunction) BL_ShapeActionActuator::sPySetBlendtime, METH_VARARGS, SetBlendtime_doc},

	{"getAction", (PyCFunction) BL_ShapeActionActuator::sPyGetAction, METH_VARARGS, GetAction_doc},
	{"getStart", (PyCFunction) BL_ShapeActionActuator::sPyGetStart, METH_VARARGS, GetStart_doc},
	{"getEnd", (PyCFunction) BL_ShapeActionActuator::sPyGetEnd, METH_VARARGS, GetEnd_doc},
	{"getBlendin", (PyCFunction) BL_ShapeActionActuator::sPyGetBlendin, METH_VARARGS, GetBlendin_doc},
	{"getPriority", (PyCFunction) BL_ShapeActionActuator::sPyGetPriority, METH_VARARGS, GetPriority_doc},
	{"getFrame", (PyCFunction) BL_ShapeActionActuator::sPyGetFrame, METH_VARARGS, GetFrame_doc},
	{"getProperty", (PyCFunction) BL_ShapeActionActuator::sPyGetProperty, METH_VARARGS, GetProperty_doc},
	{"getType", (PyCFunction) BL_ShapeActionActuator::sPyGetType, METH_VARARGS, GetType_doc},	
	{"setType", (PyCFunction) BL_ShapeActionActuator::sPySetType, METH_VARARGS, SetType_doc},
	{NULL,NULL} //Sentinel
};

PyObject* BL_ShapeActionActuator::_getattr(const STR_String& attr) {
	_getattr_up(SCA_IActuator);
}

/*     setStart                                                              */
char BL_ShapeActionActuator::GetAction_doc[] = 
"getAction()\n"
"\tReturns a string containing the name of the current action.\n";

PyObject* BL_ShapeActionActuator::PyGetAction(PyObject* self, 
											  PyObject* args, 
											  PyObject* kwds) {
	PyObject *result;
	
	if (m_action){
		result = Py_BuildValue("s", m_action->id.name+2);
	}
	else{
		Py_INCREF(Py_None);
		result = Py_None;
	}
	
	return result;
}

/*     getProperty                                                             */
char BL_ShapeActionActuator::GetProperty_doc[] = 
"getProperty()\n"
"\tReturns the name of the property to be used in FromProp mode.\n";

PyObject* BL_ShapeActionActuator::PyGetProperty(PyObject* self, 
												PyObject* args, 
												PyObject* kwds) {
	PyObject *result;
	
	result = Py_BuildValue("s", (const char *)m_propname);
	
	return result;
}

/*     getFrame                                                              */
char BL_ShapeActionActuator::GetFrame_doc[] = 
"getFrame()\n"
"\tReturns the current frame number.\n";

PyObject* BL_ShapeActionActuator::PyGetFrame(PyObject* self, 
											 PyObject* args, 
											 PyObject* kwds) {
	PyObject *result;
	
	result = Py_BuildValue("f", m_localtime);
	
	return result;
}

/*     getEnd                                                                */
char BL_ShapeActionActuator::GetEnd_doc[] = 
"getEnd()\n"
"\tReturns the last frame of the action.\n";

PyObject* BL_ShapeActionActuator::PyGetEnd(PyObject* self, 
										   PyObject* args, 
										   PyObject* kwds) {
	PyObject *result;
	
	result = Py_BuildValue("f", m_endframe);
	
	return result;
}

/*     getStart                                                              */
char BL_ShapeActionActuator::GetStart_doc[] = 
"getStart()\n"
"\tReturns the starting frame of the action.\n";

PyObject* BL_ShapeActionActuator::PyGetStart(PyObject* self, 
											 PyObject* args, 
											 PyObject* kwds) {
	PyObject *result;
	
	result = Py_BuildValue("f", m_startframe);
	
	return result;
}

/*     getBlendin                                                            */
char BL_ShapeActionActuator::GetBlendin_doc[] = 
"getBlendin()\n"
"\tReturns the number of interpolation animation frames to be\n"
"\tgenerated when this actuator is triggered.\n";

PyObject* BL_ShapeActionActuator::PyGetBlendin(PyObject* self, 
											   PyObject* args, 
											   PyObject* kwds) {
	PyObject *result;
	
	result = Py_BuildValue("f", m_blendin);
	
	return result;
}

/*     getPriority                                                           */
char BL_ShapeActionActuator::GetPriority_doc[] = 
"getPriority()\n"
"\tReturns the priority for this actuator.  Actuators with lower\n"
"\tPriority numbers will override actuators with higher numbers.\n";

PyObject* BL_ShapeActionActuator::PyGetPriority(PyObject* self, 
											    PyObject* args, 
												PyObject* kwds) {
	PyObject *result;
	
	result = Py_BuildValue("i", m_priority);
	
	return result;
}

/*     setAction                                                             */
char BL_ShapeActionActuator::SetAction_doc[] = 
"setAction(action, (reset))\n"
"\t - action    : The name of the action to set as the current action.\n"
"\t               Should be an action with Shape channels.\n"
"\t - reset     : Optional parameter indicating whether to reset the\n"
"\t               blend timer or not.  A value of 1 indicates that the\n"
"\t               timer should be reset.  A value of 0 will leave it\n"
"\t               unchanged.  If reset is not specified, the timer will"
"\t	              be reset.\n";

PyObject* BL_ShapeActionActuator::PySetAction(PyObject* self, 
											  PyObject* args, 
											  PyObject* kwds) {
	char *string;
	int	reset = 1;

	if (PyArg_ParseTuple(args,"s|i",&string, &reset))
	{
		bAction *action;
		
		action = (bAction*)SCA_ILogicBrick::m_sCurrentLogicManager->GetActionByName(STR_String(string));
		
		if (!action){
			/* NOTE!  Throw an exception or something */
			//			printf ("setAction failed: Action not found\n", string);
		}
		else{
			m_action=action;
			if (reset)
				m_blendframe = 0.f;
		}
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}

/*     setStart                                                              */
char BL_ShapeActionActuator::SetStart_doc[] = 
"setStart(start)\n"
"\t - start     : Specifies the starting frame of the animation.\n";

PyObject* BL_ShapeActionActuator::PySetStart(PyObject* self, 
											 PyObject* args, 
											 PyObject* kwds) {
	float start;
	
	if (PyArg_ParseTuple(args,"f",&start))
	{
		m_startframe = start;
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}

/*     setEnd                                                                */
char BL_ShapeActionActuator::SetEnd_doc[] = 
"setEnd(end)\n"
"\t - end       : Specifies the ending frame of the animation.\n";

PyObject* BL_ShapeActionActuator::PySetEnd(PyObject* self, 
										   PyObject* args, 
										   PyObject* kwds) {
	float end;
	
	if (PyArg_ParseTuple(args,"f",&end))
	{
		m_endframe = end;
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}

/*     setBlendin                                                            */
char BL_ShapeActionActuator::SetBlendin_doc[] = 
"setBlendin(blendin)\n"
"\t - blendin   : Specifies the number of frames of animation to generate\n"
"\t               when making transitions between actions.\n";

PyObject* BL_ShapeActionActuator::PySetBlendin(PyObject* self, 
											   PyObject* args, 
											   PyObject* kwds) {
	float blendin;
	
	if (PyArg_ParseTuple(args,"f",&blendin))
	{
		m_blendin = blendin;
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}

/*     setBlendtime                                                          */
char BL_ShapeActionActuator::SetBlendtime_doc[] = 
"setBlendtime(blendtime)\n"
"\t - blendtime : Allows the script to directly modify the internal timer\n"
"\t               used when generating transitions between actions.  This\n"
"\t               parameter must be in the range from 0.0 to 1.0.\n";

PyObject* BL_ShapeActionActuator::PySetBlendtime(PyObject* self, 
												 PyObject* args, 
												   PyObject* kwds) {
	float blendframe;
	
	if (PyArg_ParseTuple(args,"f",&blendframe))
	{
		m_blendframe = blendframe * m_blendin;
		if (m_blendframe<0.f)
			m_blendframe = 0.f;
		if (m_blendframe>m_blendin)
			m_blendframe = m_blendin;
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}

/*     setPriority                                                           */
char BL_ShapeActionActuator::SetPriority_doc[] = 
"setPriority(priority)\n"
"\t - priority  : Specifies the new priority.  Actuators will lower\n"
"\t               priority numbers will override actuators with higher\n"
"\t               numbers.\n";

PyObject* BL_ShapeActionActuator::PySetPriority(PyObject* self, 
												PyObject* args, 
												PyObject* kwds) {
	int priority;
	
	if (PyArg_ParseTuple(args,"i",&priority))
	{
		m_priority = priority;
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}

/*     setFrame                                                              */
char BL_ShapeActionActuator::SetFrame_doc[] = 
"setFrame(frame)\n"
"\t - frame     : Specifies the new current frame for the animation\n";

PyObject* BL_ShapeActionActuator::PySetFrame(PyObject* self, 
											 PyObject* args, 
											 PyObject* kwds) {
	float frame;
	
	if (PyArg_ParseTuple(args,"f",&frame))
	{
		m_localtime = frame;
		if (m_localtime<m_startframe)
			m_localtime=m_startframe;
		else if (m_localtime>m_endframe)
			m_localtime=m_endframe;
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}

/*     setProperty                                                           */
char BL_ShapeActionActuator::SetProperty_doc[] = 
"setProperty(prop)\n"
"\t - prop      : A string specifying the property name to be used in\n"
"\t               FromProp playback mode.\n";

PyObject* BL_ShapeActionActuator::PySetProperty(PyObject* self, 
												PyObject* args, 
												PyObject* kwds) {
	char *string;
	
	if (PyArg_ParseTuple(args,"s",&string))
	{
		m_propname = string;
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}

/* getType */
char BL_ShapeActionActuator::GetType_doc[] =
"getType()\n"
"\tReturns the operation mode of the actuator.\n";
PyObject* BL_ShapeActionActuator::PyGetType(PyObject* self,
											PyObject* args, 
											PyObject* kwds) {
    return Py_BuildValue("h", m_playtype);
}

/* setType */
char BL_ShapeActionActuator::SetType_doc[] =
"setType(mode)\n"
"\t - mode: Play (0), Flipper (2), LoopStop (3), LoopEnd (4) or Property (6)\n"
"\tSet the operation mode of the actuator.\n";
PyObject* BL_ShapeActionActuator::PySetType(PyObject* self,
                                            PyObject* args,
                                            PyObject* kwds) {
	short typeArg;
                                                                                                             
    if (!PyArg_ParseTuple(args, "h", &typeArg)) {
        return NULL;
    }

	switch (typeArg) {
	case ACT_ACTION_PLAY:
	case ACT_ACTION_FLIPPER:
	case ACT_ACTION_LOOP_STOP:
	case ACT_ACTION_LOOP_END:
	case ACT_ACTION_FROM_PROP:
		m_playtype = typeArg;
		break;
	default:
		printf("Invalid type for action actuator: %d\n", typeArg); /* error */
    }
	
    Py_Return;
}

