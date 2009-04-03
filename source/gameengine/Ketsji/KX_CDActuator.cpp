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
				//result = true;
				break;
			}
		case KX_CDACT_PLAY_TRACK:
			{
				SND_CDObject::Instance()->SetPlaymode(SND_CD_TRACK);
				SND_CDObject::Instance()->SetTrack(m_track);
				SND_CDObject::Instance()->SetPlaystate(SND_MUST_PLAY);
				//result = true;
				break;
			}
		case KX_CDACT_LOOP_TRACK:
			{
				SND_CDObject::Instance()->SetPlaymode(SND_CD_ALL);
				SND_CDObject::Instance()->SetTrack(m_track);
				SND_CDObject::Instance()->SetPlaystate(SND_MUST_PLAY);
				//result = true;
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
				//result = true;
				break;
			}
		case KX_CDACT_RESUME:
			{
				SND_CDObject::Instance()->SetPlaystate(SND_MUST_RESUME);
				//result = true;
				break;
			}
		case KX_CDACT_VOLUME:
			{
				SND_CDObject::Instance()->SetGain(m_gain);
				//result = true;
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
		0,
		__repr,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		Methods
};



PyParentObject KX_CDActuator::Parents[] = {
	&KX_CDActuator::Type,
		&SCA_IActuator::Type,
		&SCA_ILogicBrick::Type,
		&CValue::Type,
		NULL
};



PyMethodDef KX_CDActuator::Methods[] = {
	// Deprecated ----->
	{"setGain",(PyCFunction) KX_CDActuator::sPySetGain,METH_VARARGS,NULL},
	{"getGain",(PyCFunction) KX_CDActuator::sPyGetGain,METH_VARARGS,NULL},
	// <-----
	KX_PYMETHODTABLE_NOARGS(KX_CDActuator, startCD),
	KX_PYMETHODTABLE_NOARGS(KX_CDActuator, pauseCD),
	KX_PYMETHODTABLE_NOARGS(KX_CDActuator, resumeCD),
	KX_PYMETHODTABLE_NOARGS(KX_CDActuator, stopCD),
	KX_PYMETHODTABLE_NOARGS(KX_CDActuator, playAll),
	KX_PYMETHODTABLE_O(KX_CDActuator, playTrack),
	{NULL,NULL,NULL,NULL} //Sentinel
};

PyAttributeDef KX_CDActuator::Attributes[] = {
	KX_PYATTRIBUTE_FLOAT_RW_CHECK("volume", 0.0, 1.0, KX_CDActuator, m_gain,pyattr_setGain),
	KX_PYATTRIBUTE_INT_RW("track", 1, 99, false, KX_CDActuator, m_track),
	{ NULL }	//Sentinel
};

int KX_CDActuator::pyattr_setGain(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_CDActuator* act = static_cast<KX_CDActuator*>(self);
	SND_CDObject::Instance()->SetGain(act->m_gain);
	return 0;
}

PyObject* KX_CDActuator::_getattr(const char *attr)
{
	PyObject* object = _getattr_self(Attributes, this, attr);
	if (object != NULL)
		return object;
	_getattr_up(SCA_IActuator);
}

int KX_CDActuator::_setattr(const char *attr, PyObject *value)
{
	int ret = _setattr_self(Attributes, this, attr, value);
	if (ret >= 0)
		return ret;
	return SCA_IActuator::_setattr(attr, value);
}



KX_PYMETHODDEF_DOC_NOARGS(KX_CDActuator, startCD,
"startCD()\n"
"\tStarts the CD playing.\n")
{
	SND_CDObject::Instance()->SetPlaystate(SND_MUST_PLAY);
	Py_RETURN_NONE;
}         


KX_PYMETHODDEF_DOC_NOARGS(KX_CDActuator, pauseCD,
"pauseCD()\n"
"\tPauses the CD playing.\n")
{
	SND_CDObject::Instance()->SetPlaystate(SND_MUST_PAUSE);
	Py_RETURN_NONE;
} 


KX_PYMETHODDEF_DOC_NOARGS(KX_CDActuator, resumeCD,
"resumeCD()\n"
"\tResumes the CD playing.\n")
{
	SND_CDObject::Instance()->SetPlaystate(SND_MUST_RESUME);
	Py_RETURN_NONE;
} 


KX_PYMETHODDEF_DOC_NOARGS(KX_CDActuator, stopCD,
"stopCD()\n"
"\tStops the CD playing.\n")
{
	SND_CDObject::Instance()->SetPlaystate(SND_MUST_STOP);
	Py_RETURN_NONE;
}


KX_PYMETHODDEF_DOC_O(KX_CDActuator, playTrack,
"playTrack(trackNumber)\n"
"\tPlays the track selected.\n")
{
	if (PyInt_Check(value)) {
		int track = PyInt_AsLong(value);
		SND_CDObject::Instance()->SetPlaymode(SND_CD_TRACK);
		SND_CDObject::Instance()->SetTrack(track);
		SND_CDObject::Instance()->SetPlaystate(SND_MUST_PLAY);
	}
	Py_RETURN_NONE;
}



KX_PYMETHODDEF_DOC_NOARGS(KX_CDActuator, playAll,
"playAll()\n"
"\tPlays the CD from the beginning.\n")
{
	SND_CDObject::Instance()->SetPlaymode(SND_CD_ALL);
	SND_CDObject::Instance()->SetTrack(1);
	SND_CDObject::Instance()->SetPlaystate(SND_MUST_PLAY);
	Py_RETURN_NONE;
}     

// Deprecated ----->
PyObject* KX_CDActuator::PySetGain(PyObject* self, PyObject* args, PyObject* kwds)
{
	float gain = 1.0;
	ShowDeprecationWarning("setGain()", "the volume property");
	if (!PyArg_ParseTuple(args, "f", &gain))
		return NULL;
	
	SND_CDObject::Instance()->SetGain(gain);
	
	Py_RETURN_NONE;
}         



PyObject* KX_CDActuator::PyGetGain(PyObject* self, PyObject* args, PyObject* kwds)
{
	float gain = SND_CDObject::Instance()->GetGain();
	ShowDeprecationWarning("getGain()", "the volume property");
	PyObject* result = PyFloat_FromDouble(gain);
	
	return result;
}
// <-----
