/**
 * KX_SoundActuator.cpp
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

#include "KX_SoundActuator.h"
#include "SND_SoundObject.h"
#include "KX_GameObject.h"
#include "SND_SoundObject.h"
#include "SND_Scene.h" // needed for replication
#include <iostream>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */
KX_SoundActuator::KX_SoundActuator(SCA_IObject* gameobj,
								   SND_SoundObject* sndobj,
								   SND_Scene*	sndscene,
								   KX_SOUNDACT_TYPE type,
								   short start,
								   short end,
								   PyTypeObject* T)
								   : SCA_IActuator(gameobj,T)
{
	m_soundObject = sndobj;
	m_soundScene = sndscene;
	m_type = type;
	m_lastEvent = true;
	m_isplaying = false;
	m_startFrame = start;
	m_endFrame = end;
	m_pino = false;
	

}



KX_SoundActuator::~KX_SoundActuator()
{
	//m_soundScene->RemoveObject(this->m_soundObject);
	//(this->m_soundObject)->DeleteWhenFinished();
	m_soundScene->RemoveActiveObject(m_soundObject);
//	m_soundScene->DeleteObjectWhenFinished(m_soundObject);
	m_soundScene->DeleteObject(m_soundObject);
}



CValue* KX_SoundActuator::GetReplica()
{
	KX_SoundActuator* replica = new KX_SoundActuator(*this);
	replica->ProcessReplica();
	SND_SoundObject* soundobj = new SND_SoundObject(*m_soundObject);
	replica->setSoundObject(soundobj);
	m_soundScene->AddObject(soundobj);
	
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);
	return replica;
};



bool KX_SoundActuator::Update(double curtime, bool frame)
{
	if (!frame)
		return true;
	bool result = false;

	// do nothing on negative events, otherwise sounds are played twice!
	bool bNegativeEvent = IsNegativeEvent();

	RemoveAllEvents();

	if (m_pino)
	{
		bNegativeEvent = true;
		m_pino = false;
	}

	if (bNegativeEvent)
	{	
		// here must be a check if it is still playing
		m_isplaying = false;

		switch (m_type)
		{
		case KX_SOUNDACT_PLAYSTOP:
		case KX_SOUNDACT_LOOPSTOP:
		case KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP:
			{
				m_soundScene->RemoveActiveObject(m_soundObject);
				break;
			}
		case KX_SOUNDACT_PLAYEND:
			{
				m_soundObject->SetPlaystate(SND_MUST_STOP_WHEN_FINISHED);
				break;
			}
		default:
			// implement me !!
			break;
		}
	}
	else
	{
		if (m_soundObject && !m_isplaying)
		{
			switch (m_type)
			{
			case KX_SOUNDACT_LOOPBIDIRECTIONAL:
			case KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP:
				{
					m_soundObject->SetLoopMode(SND_LOOP_BIDIRECTIONAL);
					m_soundScene->AddActiveObject(m_soundObject, curtime);
					m_isplaying = true;
					result = true;
					break;
				}
			case KX_SOUNDACT_LOOPEND:
			case KX_SOUNDACT_LOOPSTOP:
				{
					m_soundObject->SetLoopMode(SND_LOOP_NORMAL);
					m_soundScene->AddActiveObject(m_soundObject, curtime);
					m_isplaying = true;
					result = true;
					break;
				}
			case KX_SOUNDACT_PLAYSTOP:
			case KX_SOUNDACT_PLAYEND:
				{
					m_soundObject->SetLoopMode(SND_LOOP_OFF);
					m_soundScene->AddActiveObject(m_soundObject, curtime);
					m_isplaying = true;
					result = true;
					break;
				}
			default:
				// implement me !!
				break;
			}
		}
	}

	if (m_isplaying)
	{
		m_soundObject->SetPosition(((KX_GameObject*)this->GetParent())->NodeGetWorldPosition());
		m_soundObject->SetVelocity(((KX_GameObject*)this->GetParent())->GetLinearVelocity());
		m_soundObject->SetOrientation(((KX_GameObject*)this->GetParent())->NodeGetWorldOrientation());
		result = true;
	}
	else
	{
		result = false;
	}

	if (result && (m_soundObject->IsLifeSpanOver(curtime)) && ((m_type == KX_SOUNDACT_PLAYEND) || (m_type == KX_SOUNDACT_PLAYSTOP)))
	{
		m_pino = true;
	}

	return result;
}



void KX_SoundActuator::setSoundObject(class SND_SoundObject* soundobject)
{
	m_soundObject = soundobject;
}



/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */



/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_SoundActuator::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
		0,
		"KX_SoundActuator",
		sizeof(KX_SoundActuator),
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



PyParentObject KX_SoundActuator::Parents[] = {
	&KX_SoundActuator::Type,
		&SCA_IActuator::Type,
		&SCA_ILogicBrick::Type,
		&CValue::Type,
		NULL
};



PyMethodDef KX_SoundActuator::Methods[] = {
	{"setFilename", (PyCFunction) KX_SoundActuator::sPySetFilename, METH_VARARGS,NULL},
	{"getFilename", (PyCFunction) KX_SoundActuator::sPyGetFilename, METH_VARARGS,NULL},
	{"startSound",(PyCFunction) KX_SoundActuator::sPyStartSound,METH_VARARGS,NULL},
	{"pauseSound",(PyCFunction) KX_SoundActuator::sPyPauseSound,METH_VARARGS,NULL},
	{"stopSound",(PyCFunction) KX_SoundActuator::sPyStopSound,METH_VARARGS,NULL},
	{"setGain",(PyCFunction) KX_SoundActuator::sPySetGain,METH_VARARGS,NULL},
	{"getGain",(PyCFunction) KX_SoundActuator::sPyGetGain,METH_VARARGS,NULL},
	{"setPitch",(PyCFunction) KX_SoundActuator::sPySetPitch,METH_VARARGS,NULL},
	{"getPitch",(PyCFunction) KX_SoundActuator::sPyGetPitch,METH_VARARGS,NULL},
	{"setRollOffFactor",(PyCFunction) KX_SoundActuator::sPySetRollOffFactor,METH_VARARGS,NULL},
	{"getRollOffFactor",(PyCFunction) KX_SoundActuator::sPyGetRollOffFactor,METH_VARARGS,NULL},
	{"setLooping",(PyCFunction) KX_SoundActuator::sPySetLooping,METH_VARARGS,NULL},
	{"getLooping",(PyCFunction) KX_SoundActuator::sPyGetLooping,METH_VARARGS,NULL},
	{"setPosition",(PyCFunction) KX_SoundActuator::sPySetPosition,METH_VARARGS,NULL},
	{"setVelocity",(PyCFunction) KX_SoundActuator::sPySetVelocity,METH_VARARGS,NULL},
	{"setOrientation",(PyCFunction) KX_SoundActuator::sPySetOrientation,METH_VARARGS,NULL},
	{"setType",(PyCFunction) KX_SoundActuator::sPySetType,METH_VARARGS,NULL},
	{"getType",(PyCFunction) KX_SoundActuator::sPyGetType,METH_VARARGS,NULL},
	{NULL,NULL,NULL,NULL} //Sentinel
};



PyObject* KX_SoundActuator::_getattr(const STR_String& attr)
{
	_getattr_up(SCA_IActuator);
}



PyObject* KX_SoundActuator::PySetFilename(PyObject* self, PyObject* args, PyObject* kwds)
{
	char *soundName = NULL;
	// void *soundPointer = NULL; /*unused*/
	
	if (!PyArg_ParseTuple(args, "s", &soundName))
		return NULL;

	Py_Return;
}



PyObject* KX_SoundActuator::PyGetFilename(PyObject* self, PyObject* args, PyObject* kwds)
{
	STR_String objectname = m_soundObject->GetObjectName();
	char* name = objectname.Ptr();
	
	if (!name) {
		PyErr_SetString(PyExc_RuntimeError, "Unable to get sound filename");
		return NULL;
	} else
		return PyString_FromString(name);
}



PyObject* KX_SoundActuator::PyStartSound(PyObject* self, PyObject* args, PyObject* kwds)
{
	m_soundObject->StartSound();	
	Py_Return;
}         



PyObject* KX_SoundActuator::PyPauseSound(PyObject* self, PyObject* args, PyObject* kwds)
{
	m_soundObject->PauseSound();	
	Py_Return;
} 



PyObject* KX_SoundActuator::PyStopSound(PyObject* self, PyObject* args, PyObject* kwds)
{
	m_soundObject->StopSound();	
	Py_Return;
}



PyObject* KX_SoundActuator::PySetGain(PyObject* self, PyObject* args, PyObject* kwds)
{
	float gain = 1.0;
	if (!PyArg_ParseTuple(args, "f", &gain))
		return NULL;
	
	m_soundObject->SetGain(gain);
	
	Py_Return;
}         



PyObject* KX_SoundActuator::PyGetGain(PyObject* self, PyObject* args, PyObject* kwds)
{
	float gain = m_soundObject->GetGain();
	PyObject* result = PyFloat_FromDouble(gain);
	
	return result;
}



PyObject* KX_SoundActuator::PySetPitch(PyObject* self, PyObject* args, PyObject* kwds)
{
	float pitch = 1.0;
	if (!PyArg_ParseTuple(args, "f", &pitch))
		return NULL;
	
	m_soundObject->SetPitch(pitch);
	
	Py_Return;
}         



PyObject* KX_SoundActuator::PyGetPitch(PyObject* self, PyObject* args, PyObject* kwds)
{
	float pitch = m_soundObject->GetPitch();
	PyObject* result = PyFloat_FromDouble(pitch);
	
	return result;
}



PyObject* KX_SoundActuator::PySetRollOffFactor(PyObject* self, PyObject* args, PyObject* kwds)
{
	float rollofffactor = 1.0;
	if (!PyArg_ParseTuple(args, "f", &rollofffactor))
		return NULL;
	
	m_soundObject->SetRollOffFactor(rollofffactor);

	Py_Return;
}         



PyObject* KX_SoundActuator::PyGetRollOffFactor(PyObject* self, PyObject* args, PyObject* kwds)
{
	float rollofffactor = m_soundObject->GetRollOffFactor();
	PyObject* result = PyFloat_FromDouble(rollofffactor);
	
	return result;
}



PyObject* KX_SoundActuator::PySetLooping(PyObject* self, PyObject* args, PyObject* kwds)
{
	bool looping = 1;
	if (!PyArg_ParseTuple(args, "i", &looping))
		return NULL;
	
	m_soundObject->SetLoopMode(looping);
	
	Py_Return;
}         



PyObject* KX_SoundActuator::PyGetLooping(PyObject* self, PyObject* args, PyObject* kwds)
{
	int looping = m_soundObject->GetLoopMode();
	PyObject* result = PyInt_FromLong(looping);
	
	return result;
}



PyObject* KX_SoundActuator::PySetPosition(PyObject* self, PyObject* args, PyObject* kwds)
{
	MT_Point3 pos;
	pos[0] = 0.0;
	pos[1] = 0.0;
	pos[2] = 0.0;

	if (!PyArg_ParseTuple(args, "fff", &pos[0], &pos[1], &pos[2]))
		return NULL;
	
	m_soundObject->SetPosition(pos);
	
	Py_Return;
}         



PyObject* KX_SoundActuator::PySetVelocity(PyObject* self, PyObject* args, PyObject* kwds)
{
	MT_Vector3 vel;
	vel[0] = 0.0;
	vel[1] = 0.0;
	vel[2] = 0.0;

	if (!PyArg_ParseTuple(args, "fff", &vel[0], &vel[1], &vel[2]))
		return NULL;
	
	m_soundObject->SetVelocity(vel);
	
	Py_Return;
}         



PyObject* KX_SoundActuator::PySetOrientation(PyObject* self, PyObject* args, PyObject* kwds)
{
	MT_Matrix3x3 ori;
	ori[0][0] = 1.0;
	ori[0][1] = 0.0;
	ori[0][2] = 0.0;
	ori[1][0] = 0.0;
	ori[1][1] = 1.0;
	ori[1][2] = 0.0;
	ori[2][0] = 0.0;
	ori[2][1] = 0.0;
	ori[2][2] = 1.0;

	if (!PyArg_ParseTuple(args, "fffffffff", &ori[0][0], &ori[0][1], &ori[0][2], &ori[1][0], &ori[1][1], &ori[1][2], &ori[2][0], &ori[2][1], &ori[2][2]))
		return NULL;
	
	m_soundObject->SetOrientation(ori);
	
	Py_Return;
}

PyObject* KX_SoundActuator::PySetType(PyObject* self, PyObject* args, PyObject* kwds)
{
	int typeArg;

	if (!PyArg_ParseTuple(args, "i", &typeArg)) {
		return NULL;
	}

	if ( (typeArg > KX_SOUNDACT_NODEF)
	  && (typeArg < KX_SOUNDACT_MAX) ) {
		m_type = (KX_SOUNDACT_TYPE) typeArg;
	}

	Py_Return;
}

PyObject* KX_SoundActuator::PyGetType(PyObject* self, PyObject* args, PyObject* kwds)
{
	return PyInt_FromLong(m_type);
}



