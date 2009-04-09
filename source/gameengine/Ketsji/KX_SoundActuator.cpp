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
#include "KX_PyMath.h" // needed for PyObjectFrom()
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
	if (m_soundObject)
	{
		m_soundScene->RemoveActiveObject(m_soundObject);
		m_soundScene->DeleteObject(m_soundObject);
	}
}



CValue* KX_SoundActuator::GetReplica()
{
	KX_SoundActuator* replica = new KX_SoundActuator(*this);
	replica->ProcessReplica();
	if (m_soundObject)
	{
	    SND_SoundObject* soundobj = new SND_SoundObject(*m_soundObject);
		replica->setSoundObject(soundobj);
		m_soundScene->AddObject(soundobj);
	}
	
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

	if (!m_soundObject)
		return false;

	// actual audio device playing state
	bool isplaying = (m_soundObject->GetPlaystate() != SND_STOPPED) ? true : false;

	if (m_pino)
	{
		bNegativeEvent = true;
		m_pino = false;
	}

	if (bNegativeEvent)
	{	
		// here must be a check if it is still playing
		if (m_isplaying && isplaying) 
		{
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
			case KX_SOUNDACT_LOOPEND:
			case KX_SOUNDACT_LOOPBIDIRECTIONAL:
				{
					m_soundObject->SetLoopMode(SND_LOOP_OFF);
					m_soundObject->SetPlaystate(SND_MUST_STOP_WHEN_FINISHED);
					break;
				}
			default:
				// implement me !!
				break;
			}
		}
		// remember that we tried to stop the actuator
		m_isplaying = false;
	}
	else
	{
		if (!m_isplaying)
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
	// verify that the sound is still playing
	isplaying = (m_soundObject->GetPlaystate() != SND_STOPPED) ? true : false;

	if (isplaying)
	{
		m_soundObject->SetPosition(((KX_GameObject*)this->GetParent())->NodeGetWorldPosition());
		m_soundObject->SetVelocity(((KX_GameObject*)this->GetParent())->GetLinearVelocity());
		m_soundObject->SetOrientation(((KX_GameObject*)this->GetParent())->NodeGetWorldOrientation());
		result = true;
	}
	else
	{
		m_isplaying = false;
		result = false;
	}
	/*
	if (result && (m_soundObject->IsLifeSpanOver(curtime)) && ((m_type == KX_SOUNDACT_PLAYEND) || (m_type == KX_SOUNDACT_PLAYSTOP)))
	{
		m_pino = true;
	}
	*/
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
	PyObject_HEAD_INIT(NULL)
		0,
		"KX_SoundActuator",
		sizeof(KX_SoundActuator),
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



PyParentObject KX_SoundActuator::Parents[] = {
	&KX_SoundActuator::Type,
		&SCA_IActuator::Type,
		&SCA_ILogicBrick::Type,
		&CValue::Type,
		NULL
};



PyMethodDef KX_SoundActuator::Methods[] = {
	// Deprecated ----->
	{"setFilename", (PyCFunction) KX_SoundActuator::sPySetFilename, METH_VARARGS,NULL},
	{"getFilename", (PyCFunction) KX_SoundActuator::sPyGetFilename, METH_VARARGS,NULL},
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
	// <-----

	KX_PYMETHODTABLE_NOARGS(KX_SoundActuator, startSound),
	KX_PYMETHODTABLE_NOARGS(KX_SoundActuator, pauseSound),
	KX_PYMETHODTABLE_NOARGS(KX_SoundActuator, stopSound),
	{NULL,NULL,NULL,NULL} //Sentinel
};

PyAttributeDef KX_SoundActuator::Attributes[] = {
	KX_PYATTRIBUTE_RW_FUNCTION("filename", KX_SoundActuator, pyattr_get_filename, pyattr_set_filename),
	KX_PYATTRIBUTE_RW_FUNCTION("volume", KX_SoundActuator, pyattr_get_gain, pyattr_set_gain),
	KX_PYATTRIBUTE_RW_FUNCTION("pitch", KX_SoundActuator, pyattr_get_pitch, pyattr_set_pitch),
	KX_PYATTRIBUTE_RW_FUNCTION("rollOffFactor", KX_SoundActuator, pyattr_get_rollOffFactor, pyattr_set_rollOffFactor),
	KX_PYATTRIBUTE_RW_FUNCTION("looping", KX_SoundActuator, pyattr_get_looping, pyattr_set_looping),
	KX_PYATTRIBUTE_RW_FUNCTION("position", KX_SoundActuator, pyattr_get_position, pyattr_set_position),
	KX_PYATTRIBUTE_RW_FUNCTION("velocity", KX_SoundActuator, pyattr_get_velocity, pyattr_set_velocity),
	KX_PYATTRIBUTE_RW_FUNCTION("orientation", KX_SoundActuator, pyattr_get_orientation, pyattr_set_orientation),
	KX_PYATTRIBUTE_ENUM_RW("type",KX_SoundActuator::KX_SOUNDACT_NODEF+1,KX_SoundActuator::KX_SOUNDACT_MAX-1,false,KX_SoundActuator,m_type),
	{ NULL }	//Sentinel
};

/* Methods ----------------------------------------------------------------- */
KX_PYMETHODDEF_DOC_NOARGS(KX_SoundActuator, startSound, 
"startSound()\n"
"\tStarts the sound.\n")
{
	if (m_soundObject)
		// This has no effect if the actuator is not active.
		// To start the sound you must activate the actuator. 
		// This function is to restart the sound.
		m_soundObject->StartSound();	
	Py_RETURN_NONE;
}         

KX_PYMETHODDEF_DOC_NOARGS(KX_SoundActuator, pauseSound,
"pauseSound()\n"
"\tPauses the sound.\n")
{
	if (m_soundObject)
		// unfortunately, openal does not implement pause correctly, it is equivalent to a stop
		m_soundObject->PauseSound();	
	Py_RETURN_NONE;
} 

KX_PYMETHODDEF_DOC_NOARGS(KX_SoundActuator, stopSound,
"stopSound()\n"
"\tStops the sound.\n")
{
	if (m_soundObject)
		m_soundObject->StopSound();	
	Py_RETURN_NONE;
}

/* Atribute setting and getting -------------------------------------------- */
PyObject* KX_SoundActuator::py_getattro(PyObject *attr)
{
	py_getattro_up(SCA_IActuator);
}

int KX_SoundActuator::py_setattro(PyObject *attr, PyObject* value) {
	py_setattro_up(SCA_IActuator);
}

PyObject* KX_SoundActuator::pyattr_get_filename(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	if (!actuator->m_soundObject)
	{
		return PyString_FromString("");
	}
	STR_String objectname = actuator->m_soundObject->GetObjectName();
	char* name = objectname.Ptr();
	
	if (!name) {
		PyErr_SetString(PyExc_RuntimeError, "Unable to get sound filename");
		return NULL;
	} else
		return PyString_FromString(name);
}

PyObject* KX_SoundActuator::pyattr_get_gain(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	float gain = (actuator->m_soundObject) ? actuator->m_soundObject->GetGain() : 1.0f;

	PyObject* result = PyFloat_FromDouble(gain);
	
	return result;
}

PyObject* KX_SoundActuator::pyattr_get_pitch(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	float pitch = (actuator->m_soundObject) ? actuator->m_soundObject->GetPitch() : 1.0;
	PyObject* result = PyFloat_FromDouble(pitch);
	
	return result;
}

PyObject* KX_SoundActuator::pyattr_get_rollOffFactor(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	float rollofffactor = (actuator->m_soundObject) ? actuator->m_soundObject->GetRollOffFactor() : 1.0;
	PyObject* result = PyFloat_FromDouble(rollofffactor);
	
	return result;
}

PyObject* KX_SoundActuator::pyattr_get_looping(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	int looping = (actuator->m_soundObject) ? actuator->m_soundObject->GetLoopMode() : (int)SND_LOOP_OFF;
	PyObject* result = PyInt_FromLong(looping);
	
	return result;
}

PyObject* KX_SoundActuator::pyattr_get_position(void * self, const struct KX_PYATTRIBUTE_DEF *attrdef) 
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	MT_Vector3 pos(0.0, 0.0, 0.0);

	if (actuator->m_soundObject)
		pos = actuator->m_soundObject->GetPosition();

	PyObject * result = PyObjectFrom(pos);
	return result;
}

PyObject* KX_SoundActuator::pyattr_get_velocity(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	MT_Vector3 vel;

	if (actuator->m_soundObject)
		vel = actuator->m_soundObject->GetVelocity();

	PyObject * result = PyObjectFrom(vel);
	return result;
}

PyObject* KX_SoundActuator::pyattr_get_orientation(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef) 
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	MT_Matrix3x3 ori;

	if (actuator->m_soundObject)
		ori = actuator->m_soundObject->GetOrientation();

	PyObject * result = PyObjectFrom(ori);
	return result;
}

int KX_SoundActuator::pyattr_set_filename(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	char *soundName = NULL;
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator*> (self);
	// void *soundPointer = NULL; /*unused*/
	
	if (!PyArg_Parse(value, "s", &soundName))
		return 1;

	if (actuator->m_soundObject) {
		actuator->m_soundObject->SetObjectName(soundName);
	}
	
	return 0;
}


int KX_SoundActuator::pyattr_set_gain(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	float gain = 1.0;
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	if (!PyArg_Parse(value, "f", &gain))
		return 1;
	
	if (actuator->m_soundObject)
		actuator->m_soundObject->SetGain(gain);
	
	return 0;
}         

int KX_SoundActuator::pyattr_set_pitch(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	float pitch = 1.0;
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	if (!PyArg_Parse(value, "f", &pitch))
		return 1;
	
	if (actuator->m_soundObject)
		actuator->m_soundObject->SetPitch(pitch);
	
	return 0;
}         

int KX_SoundActuator::pyattr_set_rollOffFactor(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	float rollofffactor = 1.0;
	if (!PyArg_Parse(value, "f", &rollofffactor))
		return 1;
	
	if (actuator->m_soundObject)
		actuator->m_soundObject->SetRollOffFactor(rollofffactor);

	return 0;
}         

int KX_SoundActuator::pyattr_set_looping(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	int looping = 1;
	if (!PyArg_Parse(value, "i", &looping))
		return 1;
	
	if (actuator->m_soundObject)
		actuator->m_soundObject->SetLoopMode(looping);
	
	return 0;
}         

int KX_SoundActuator::pyattr_set_position(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	float pos[3];

	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);

	if (!PyArg_ParseTuple(value, "fff", &pos[0], &pos[1], &pos[2]))
		return 1;
	
	if (actuator->m_soundObject)
		actuator->m_soundObject->SetPosition(MT_Vector3(pos));
	
	return 0;
}         

int KX_SoundActuator::pyattr_set_velocity(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	float vel[3];
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);


	if (!PyArg_ParseTuple(value, "fff", &vel[0], &vel[1], &vel[2]))
		return 1;
	
	if (actuator->m_soundObject)
		actuator->m_soundObject->SetVelocity(MT_Vector3(vel));
	
	return 0;

}         

int KX_SoundActuator::pyattr_set_orientation(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{

	MT_Matrix3x3 rot;
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);

	if (!PySequence_Check(value)) {
		PyErr_SetString(PyExc_AttributeError, "'orientation' attribute needs to be a sequence");
		return 1;
	}

	if (!actuator->m_soundObject)
		return 0; /* Since not having m_soundObject didn't do anything in the old version,
				  * it probably should be kept that way  */

	if (PyMatTo(value, rot))
	{
		actuator->m_soundObject->SetOrientation(rot);
		return 0;
	}
	PyErr_Clear();


	if (PySequence_Size(value) == 4)
	{
		MT_Quaternion qrot;
		if (PyVecTo(value, qrot))
		{
			rot.setRotation(qrot);
			actuator->m_soundObject->SetOrientation(rot);
			return 0;
		}
		return 1;
	}

	if (PySequence_Size(value) == 3)
	{
		MT_Vector3 erot;
		if (PyVecTo(value, erot))
		{
			rot.setEuler(erot);
			actuator->m_soundObject->SetOrientation(rot);
			return 0;
		}
		return 1;
	}

	PyErr_SetString(PyExc_AttributeError, "could not set the orientation from a 3x3 matrix, quaternion or euler sequence");
	return 1;

}

// Deprecated ----->
PyObject* KX_SoundActuator::PySetFilename(PyObject* self, PyObject* args, PyObject* kwds)
{
	char *soundName = NULL;
	ShowDeprecationWarning("setFilename()", "the filename property");
	// void *soundPointer = NULL; /*unused*/
	
	if (!PyArg_ParseTuple(args, "s", &soundName))
		return NULL;

	Py_RETURN_NONE;
}

PyObject* KX_SoundActuator::PyGetFilename(PyObject* self, PyObject* args, PyObject* kwds)
{
	ShowDeprecationWarning("getFilename()", "the filename property");
	if (!m_soundObject)
	{
		return PyString_FromString("");
	}
	STR_String objectname = m_soundObject->GetObjectName();
	char* name = objectname.Ptr();
	
	if (!name) {
		PyErr_SetString(PyExc_RuntimeError, "Unable to get sound filename");
		return NULL;
	} else
		return PyString_FromString(name);
}

PyObject* KX_SoundActuator::PySetGain(PyObject* self, PyObject* args, PyObject* kwds)
{
	ShowDeprecationWarning("setGain()", "the volume property");
	float gain = 1.0;
	if (!PyArg_ParseTuple(args, "f", &gain))
		return NULL;
	
	if (m_soundObject)
		m_soundObject->SetGain(gain);
	
	Py_RETURN_NONE;
}         



PyObject* KX_SoundActuator::PyGetGain(PyObject* self, PyObject* args, PyObject* kwds)
{
	ShowDeprecationWarning("getGain()", "the volume property");
	float gain = (m_soundObject) ? m_soundObject->GetGain() : 1.0f;
	PyObject* result = PyFloat_FromDouble(gain);
	
	return result;
}



PyObject* KX_SoundActuator::PySetPitch(PyObject* self, PyObject* args, PyObject* kwds)
{
	ShowDeprecationWarning("setPitch()", "the pitch property");
	float pitch = 1.0;
	if (!PyArg_ParseTuple(args, "f", &pitch))
		return NULL;
	
	if (m_soundObject)
		m_soundObject->SetPitch(pitch);
	
	Py_RETURN_NONE;
}         



PyObject* KX_SoundActuator::PyGetPitch(PyObject* self, PyObject* args, PyObject* kwds)
{
	ShowDeprecationWarning("getPitch()", "the pitch property");
	float pitch = (m_soundObject) ? m_soundObject->GetPitch() : 1.0;
	PyObject* result = PyFloat_FromDouble(pitch);
	
	return result;
}



PyObject* KX_SoundActuator::PySetRollOffFactor(PyObject* self, PyObject* args, PyObject* kwds)
{
	ShowDeprecationWarning("setRollOffFactor()", "the rollOffFactor property");
	float rollofffactor = 1.0;
	if (!PyArg_ParseTuple(args, "f", &rollofffactor))
		return NULL;
	
	if (m_soundObject)
		m_soundObject->SetRollOffFactor(rollofffactor);

	Py_RETURN_NONE;
}         



PyObject* KX_SoundActuator::PyGetRollOffFactor(PyObject* self, PyObject* args, PyObject* kwds)
{
	ShowDeprecationWarning("getRollOffFactor()", "the rollOffFactor property");
	float rollofffactor = (m_soundObject) ? m_soundObject->GetRollOffFactor() : 1.0;
	PyObject* result = PyFloat_FromDouble(rollofffactor);
	
	return result;
}



PyObject* KX_SoundActuator::PySetLooping(PyObject* self, PyObject* args, PyObject* kwds)
{
	ShowDeprecationWarning("setLooping()", "the looping property");
	bool looping = 1;
	if (!PyArg_ParseTuple(args, "i", &looping))
		return NULL;
	
	if (m_soundObject)
		m_soundObject->SetLoopMode(looping);
	
	Py_RETURN_NONE;
}         



PyObject* KX_SoundActuator::PyGetLooping(PyObject* self, PyObject* args, PyObject* kwds)
{
	ShowDeprecationWarning("getLooping()", "the looping property");
	int looping = (m_soundObject) ? m_soundObject->GetLoopMode() : (int)SND_LOOP_OFF;
	PyObject* result = PyInt_FromLong(looping);
	
	return result;
}



PyObject* KX_SoundActuator::PySetPosition(PyObject* self, PyObject* args, PyObject* kwds)
{
	MT_Point3 pos;
	ShowDeprecationWarning("setPosition()", "the position property");
	pos[0] = 0.0;
	pos[1] = 0.0;
	pos[2] = 0.0;

	if (!PyArg_ParseTuple(args, "fff", &pos[0], &pos[1], &pos[2]))
		return NULL;
	
	if (m_soundObject)
		m_soundObject->SetPosition(pos);
	
	Py_RETURN_NONE;
}         



PyObject* KX_SoundActuator::PySetVelocity(PyObject* self, PyObject* args, PyObject* kwds)
{
	MT_Vector3 vel;
	ShowDeprecationWarning("setVelocity()", "the velocity property");
	vel[0] = 0.0;
	vel[1] = 0.0;
	vel[2] = 0.0;

	if (!PyArg_ParseTuple(args, "fff", &vel[0], &vel[1], &vel[2]))
		return NULL;
	
	if (m_soundObject)
		m_soundObject->SetVelocity(vel);
	
	Py_RETURN_NONE;
}         



PyObject* KX_SoundActuator::PySetOrientation(PyObject* self, PyObject* args, PyObject* kwds)
{
	MT_Matrix3x3 ori;
	ShowDeprecationWarning("setOrientation()", "the orientation property");
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
	
	if (m_soundObject)
		m_soundObject->SetOrientation(ori);
	
	Py_RETURN_NONE;
}

PyObject* KX_SoundActuator::PySetType(PyObject* self, PyObject* args, PyObject* kwds)
{
	int typeArg;
	ShowDeprecationWarning("setType()", "the type property");

	if (!PyArg_ParseTuple(args, "i", &typeArg)) {
		return NULL;
	}

	if ( (typeArg > KX_SOUNDACT_NODEF)
	  && (typeArg < KX_SOUNDACT_MAX) ) {
		m_type = (KX_SOUNDACT_TYPE) typeArg;
	}

	Py_RETURN_NONE;
}

PyObject* KX_SoundActuator::PyGetType(PyObject* self, PyObject* args, PyObject* kwds)
{
	ShowDeprecationWarning("getType()", "the type property");
	return PyInt_FromLong(m_type);
}
// <-----

