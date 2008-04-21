/**
 * KX_CDActuator.cpp
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
 *
 */

#include "KX_CDActuator.h"
#include "SND_CDObject.h"
#include "KX_GameObject.h"
#include "SND_Scene.h" // needed for replication
#include <iostream>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */
KX_CDActuator::KX_CDActuator(SCA_IObject* gameobject,
							 SND_Scene* soundscene,
							 KX_CDACT_TYPE type,
							 int track,
							 short start,
							 short end,
							 PyTypeObject* T)
							 : SCA_IActuator(gameobject,T)
{
	m_soundscene = soundscene;
	m_type = type;
	m_track = track;
	m_lastEvent = true;
	m_isplaying = false;
	m_startFrame = start;
	m_endFrame = end;
	m_gain = SND_CDObject::Instance()->GetGain();
}



KX_CDActuator::~KX_CDActuator()
{
}


/* hmmm, do we want this? */
CValue* KX_CDActuator::GetReplica()
{
	KX_CDActuator* replica = new KX_CDActuator(*this);
	replica->ProcessReplica();
	
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);
	return replica;
};



bool KX_CDActuator::Update()
{
	bool result = false;
	bool bNegativeEvent = IsNegativeEvent();
	
	RemoveAllEvents();
	
	if (!bNegativeEvent)
	{
		switch (m_type)
		{
		case KX_CDACT_PLAY_ALL:
			{
				SND_CDObject::Instance()->SetPlaymode(SND_CD_ALL);
				SND_CDObject::Instance()->SetTrack(1);
				SND_CDObject::Instance()->SetPlaystate(SND_MUST_PLAY);
				result = true;
				break;
			}
		case KX_CDACT_PLAY_TRACK:
			{
				SND_CDObject::Instance()->SetPlaymode(SND_CD_TRACK);
				SND_CDObject::Instance()->SetTrack(m_track);
				SND_CDObject::Instance()->SetPlaystate(SND_MUST_PLAY);
				result = true;
				break;
			}
		case KX_CDACT_LOOP_TRACK:
			{
				SND_CDObject::Instance()->SetPlaymode(SND_CD_ALL);
				SND_CDObject::Instance()->SetTrack(m_track);
				SND_CDObject::Instance()->SetPlaystate(SND_MUST_PLAY);
				result = true;
				break;
			}
		case KX_CDACT_STOP:
			{
				SND_CDObject::Instance()->SetPlaystate(SND_MUST_STOP);
				break;
			}
		case KX_CDACT_PAUSE:
			{
				SND_CDObject::Instance()->SetPlaystate(SND_MUST_PAUSE);
				result = true;
				break;
			}
		case KX_CDACT_RESUME:
			{
				SND_CDObject::Instance()->SetPlaystate(SND_MUST_RESUME);
				result = true;
				break;
			}
		case KX_CDACT_VOLUME:
			{
				SND_CDObject::Instance()->SetGain(m_gain);
				result = true;
				break;
			}
		default:
			// implement me !!
			break;
		}
	}
	return result;
}



/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */



/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_CDActuator::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
		0,
		"KX_SoundActuator",
		sizeof(KX_CDActuator),
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



PyParentObject KX_CDActuator::Parents[] = {
	&KX_CDActuator::Type,
		&SCA_IActuator::Type,
		&SCA_ILogicBrick::Type,
		&CValue::Type,
		NULL
};



PyMethodDef KX_CDActuator::Methods[] = {
	{"startCD",(PyCFunction) KX_CDActuator::sPyStartCD,METH_VARARGS,NULL},
	{"pauseCD",(PyCFunction) KX_CDActuator::sPyPauseCD,METH_VARARGS,NULL},
	{"stopCD",(PyCFunction) KX_CDActuator::sPyStopCD,METH_VARARGS,NULL},
	{"setGain",(PyCFunction) KX_CDActuator::sPySetGain,METH_VARARGS,NULL},
	{"getGain",(PyCFunction) KX_CDActuator::sPyGetGain,METH_VARARGS,NULL},
	{NULL,NULL,NULL,NULL} //Sentinel
};



PyObject* KX_CDActuator::_getattr(const STR_String& attr)
{
	_getattr_up(SCA_IActuator);
}




PyObject* KX_CDActuator::PyStartCD(PyObject* self, PyObject* args, PyObject* kwds)
{
	SND_CDObject::Instance()->SetPlaystate(SND_MUST_PLAY);
	Py_Return;
}         



PyObject* KX_CDActuator::PyPauseCD(PyObject* self, PyObject* args, PyObject* kwds)
{
	SND_CDObject::Instance()->SetPlaystate(SND_MUST_PAUSE);
	Py_Return;
} 



PyObject* KX_CDActuator::PyStopCD(PyObject* self, PyObject* args, PyObject* kwds)
{
	SND_CDObject::Instance()->SetPlaystate(SND_MUST_STOP);
	Py_Return;
}



PyObject* KX_CDActuator::PySetGain(PyObject* self, PyObject* args, PyObject* kwds)
{
	float gain = 1.0;
	if (!PyArg_ParseTuple(args, "f", &gain))
		return NULL;
	
	SND_CDObject::Instance()->SetGain(gain);
	
	Py_Return;
}         



PyObject* KX_CDActuator::PyGetGain(PyObject* self, PyObject* args, PyObject* kwds)
{
	float gain = SND_CDObject::Instance()->GetGain();
	PyObject* result = PyFloat_FromDouble(gain);
	
	return result;
}
