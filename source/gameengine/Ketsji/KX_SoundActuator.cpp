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
#include "KX_GameObject.h"
#include "KX_PyMath.h" // needed for PyObjectFrom()
#include <iostream>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */
KX_SoundActuator::KX_SoundActuator(SCA_IObject* gameobj,
								   AUD_Sound* sound,
								   float volume,
								   float pitch,
								   bool is3d,
								   KX_3DSoundSettings settings,
								   KX_SOUNDACT_TYPE type)//,
								   : SCA_IActuator(gameobj, KX_ACT_SOUND)
{
	m_sound = sound;
	m_volume = volume;
	m_pitch = pitch;
	m_is3d = is3d;
	m_3d = settings;
	m_handle = NULL;
	m_type = type;
	m_isplaying = false;
}



KX_SoundActuator::~KX_SoundActuator()
{
	if(m_handle)
		AUD_stop(m_handle);
}

void KX_SoundActuator::play()
{
	if(m_handle)
		AUD_stop(m_handle);

	if(!m_sound)
		return;

	// this is the sound that will be played and not deleted afterwards
	AUD_Sound* sound = m_sound;
	// this sounds are for temporary stacked sounds, will be deleted if not NULL
	AUD_Sound* sound2 = NULL;
	AUD_Sound* sound3 = NULL;

	switch (m_type)
	{
	case KX_SOUNDACT_LOOPBIDIRECTIONAL:
	case KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP:
		// create a ping pong sound on sound2 stacked on the orignal sound
		sound2 = AUD_pingpongSound(sound);
		// create a loop sound on sound3 stacked on the pingpong sound and let that one play (save it to sound)
		sound = sound3 = AUD_loopSound(sound2);
		break;
	case KX_SOUNDACT_LOOPEND:
	case KX_SOUNDACT_LOOPSTOP:
		// create a loop sound on sound2 stacked on the pingpong sound and let that one play (save it to sound)
		sound = sound2 = AUD_loopSound(sound);
		break;
	case KX_SOUNDACT_PLAYSTOP:
	case KX_SOUNDACT_PLAYEND:
	default:
		break;
	}

	if(m_is3d)
	{
		// sound shall be played 3D
		m_handle = AUD_play3D(sound, 0);

		AUD_set3DSourceSetting(m_handle, AUD_3DSS_MAX_GAIN, m_3d.max_gain);
		AUD_set3DSourceSetting(m_handle, AUD_3DSS_MIN_GAIN, m_3d.min_gain);
		AUD_set3DSourceSetting(m_handle, AUD_3DSS_REFERENCE_DISTANCE, m_3d.reference_distance);
		AUD_set3DSourceSetting(m_handle, AUD_3DSS_MAX_DISTANCE, m_3d.max_distance);
		AUD_set3DSourceSetting(m_handle, AUD_3DSS_ROLLOFF_FACTOR, m_3d.rolloff_factor);
		AUD_set3DSourceSetting(m_handle, AUD_3DSS_CONE_INNER_ANGLE, m_3d.cone_inner_angle);
		AUD_set3DSourceSetting(m_handle, AUD_3DSS_CONE_OUTER_ANGLE, m_3d.cone_outer_angle);
		AUD_set3DSourceSetting(m_handle, AUD_3DSS_CONE_OUTER_GAIN, m_3d.cone_outer_gain);
	}
	else
		m_handle = AUD_play(sound, 0);

	AUD_setSoundPitch(m_handle, m_pitch);
	AUD_setSoundVolume(m_handle, m_volume);
	m_isplaying = true;

	// now we unload the pingpong and loop sounds, as we don't need them anymore
	// the started sound will continue playing like it was created, don't worry!
	if(sound3)
		AUD_unload(sound3);
	if(sound2)
		AUD_unload(sound2);
}

CValue* KX_SoundActuator::GetReplica()
{
	KX_SoundActuator* replica = new KX_SoundActuator(*this);
	replica->ProcessReplica();
	return replica;
};

void KX_SoundActuator::ProcessReplica()
{
	SCA_IActuator::ProcessReplica();
	m_handle = 0;
}

bool KX_SoundActuator::Update(double curtime, bool frame)
{
	if (!frame)
		return true;
	bool result = false;

	// do nothing on negative events, otherwise sounds are played twice!
	bool bNegativeEvent = IsNegativeEvent();
	bool bPositiveEvent = m_posevent;
	
	RemoveAllEvents();

	if(!m_sound)
		return false;

	// actual audio device playing state
	bool isplaying = AUD_getStatus(m_handle) == AUD_STATUS_PLAYING;

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
					// stop immediately
					AUD_stop(m_handle);
					break;
				}
			case KX_SOUNDACT_PLAYEND:
				{
					// do nothing, sound will stop anyway when it's finished
					break;
				}
			case KX_SOUNDACT_LOOPEND:
			case KX_SOUNDACT_LOOPBIDIRECTIONAL:
				{
					// stop the looping so that the sound stops when it finished
					AUD_stopLoop(m_handle);
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
	
#if 1
	// Warning: when de-activating the actuator, after a single negative event this runs again with...
	// m_posevent==false && m_posevent==false, in this case IsNegativeEvent() returns false 
	// and assumes this is a positive event.
	// check that we actually have a positive event so as not to play sounds when being disabled.
	else if(bPositiveEvent) { // <- added since 2.49
#else
	else {	// <- works in most cases except a loop-end sound will never stop unless
			// the negative pulse is done continuesly
#endif
		if (!m_isplaying)
			play();
	}
	// verify that the sound is still playing
	isplaying = AUD_getStatus(m_handle) == AUD_STATUS_PLAYING ? true : false;

	if (isplaying)
	{
		if(m_is3d)
		{
			AUD_3DData data;
			float f;
			((KX_GameObject*)this->GetParent())->NodeGetWorldPosition().getValue(data.position);
			((KX_GameObject*)this->GetParent())->GetLinearVelocity().getValue(data.velocity);
			((KX_GameObject*)this->GetParent())->NodeGetWorldOrientation().getValue3x3(data.orientation);

			/*
			 * The 3D data from blender has to be transformed for OpenAL:
			 *  - In blender z is up and y is forwards
			 *  - In OpenAL y is up and z is backwards
			 * We have to do that for all 5 vectors.
			 */
			f = data.position[1];
			data.position[1] = data.position[2];
			data.position[2] = -f;

			f = data.velocity[1];
			data.velocity[1] = data.velocity[2];
			data.velocity[2] = -f;

			f = data.orientation[1];
			data.orientation[1] = data.orientation[2];
			data.orientation[2] = -f;

			f = data.orientation[4];
			data.orientation[4] = data.orientation[5];
			data.orientation[5] = -f;

			f = data.orientation[7];
			data.orientation[7] = data.orientation[8];
			data.orientation[8] = -f;

			AUD_update3DSource(m_handle, &data);
		}
		result = true;
	}
	else
	{
		m_isplaying = false;
		result = false;
	}
	return result;
}


#ifndef DISABLE_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */



/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_SoundActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_SoundActuator",
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

PyMethodDef KX_SoundActuator::Methods[] = {
	KX_PYMETHODTABLE_NOARGS(KX_SoundActuator, startSound),
	KX_PYMETHODTABLE_NOARGS(KX_SoundActuator, pauseSound),
	KX_PYMETHODTABLE_NOARGS(KX_SoundActuator, stopSound),
	{NULL,NULL,NULL,NULL} //Sentinel
};

PyAttributeDef KX_SoundActuator::Attributes[] = {
	KX_PYATTRIBUTE_BOOL_RO("is3D", KX_SoundActuator, m_is3d),
	KX_PYATTRIBUTE_RW_FUNCTION("maxGain3D", KX_SoundActuator, pyattr_get_3d_property, pyattr_set_3d_property),
	KX_PYATTRIBUTE_RW_FUNCTION("minGain3D", KX_SoundActuator, pyattr_get_3d_property, pyattr_set_3d_property),
	KX_PYATTRIBUTE_RW_FUNCTION("referenceDistance3D", KX_SoundActuator, pyattr_get_3d_property, pyattr_set_3d_property),
	KX_PYATTRIBUTE_RW_FUNCTION("maxDistance3D", KX_SoundActuator, pyattr_get_3d_property, pyattr_set_3d_property),
	KX_PYATTRIBUTE_RW_FUNCTION("rolloffFactor3D", KX_SoundActuator, pyattr_get_3d_property, pyattr_set_3d_property),
	KX_PYATTRIBUTE_RW_FUNCTION("coneInnerAngle3D", KX_SoundActuator, pyattr_get_3d_property, pyattr_set_3d_property),
	KX_PYATTRIBUTE_RW_FUNCTION("coneOuterAngle3D", KX_SoundActuator, pyattr_get_3d_property, pyattr_set_3d_property),
	KX_PYATTRIBUTE_RW_FUNCTION("coneOuterGain3D", KX_SoundActuator, pyattr_get_3d_property, pyattr_set_3d_property),
	
	KX_PYATTRIBUTE_RW_FUNCTION("time", KX_SoundActuator, pyattr_get_audposition, pyattr_set_audposition),
	KX_PYATTRIBUTE_RW_FUNCTION("volume", KX_SoundActuator, pyattr_get_gain, pyattr_set_gain),
	KX_PYATTRIBUTE_RW_FUNCTION("pitch", KX_SoundActuator, pyattr_get_pitch, pyattr_set_pitch),
	KX_PYATTRIBUTE_RW_FUNCTION("rollOffFactor", KX_SoundActuator, pyattr_get_rollOffFactor, pyattr_set_rollOffFactor),
	KX_PYATTRIBUTE_ENUM_RW("mode",KX_SoundActuator::KX_SOUNDACT_NODEF+1,KX_SoundActuator::KX_SOUNDACT_MAX-1,false,KX_SoundActuator,m_type),
	{ NULL }	//Sentinel
};

/* Methods ----------------------------------------------------------------- */
KX_PYMETHODDEF_DOC_NOARGS(KX_SoundActuator, startSound,
"startSound()\n"
"\tStarts the sound.\n")
{
	switch(AUD_getStatus(m_handle))
	{
	case AUD_STATUS_PLAYING:
		break;
	case AUD_STATUS_PAUSED:
		AUD_resume(m_handle);
		break;
	default:
		play();
	}
	Py_RETURN_NONE;
}

KX_PYMETHODDEF_DOC_NOARGS(KX_SoundActuator, pauseSound,
"pauseSound()\n"
"\tPauses the sound.\n")
{
	AUD_pause(m_handle);
	Py_RETURN_NONE;
}

KX_PYMETHODDEF_DOC_NOARGS(KX_SoundActuator, stopSound,
"stopSound()\n"
"\tStops the sound.\n")
{
	AUD_stop(m_handle);
	Py_RETURN_NONE;
}

/* Atribute setting and getting -------------------------------------------- */
PyObject* KX_SoundActuator::pyattr_get_3d_property(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	const char* prop = attrdef->m_name;
	float result_value = 0.0;

	if(!strcmp(prop, "maxGain3D")) {
		result_value = actuator->m_3d.max_gain;

	} else if (!strcmp(prop, "minGain3D")) {
		result_value = actuator->m_3d.min_gain;

	} else if (!strcmp(prop, "referenceDistance3D")) {
		result_value = actuator->m_3d.reference_distance;

	} else if (!strcmp(prop, "maxDistance3D")) {
		result_value = actuator->m_3d.max_distance;

	} else if (!strcmp(prop, "rolloffFactor3D")) {
		result_value = actuator->m_3d.rolloff_factor;

	} else if (!strcmp(prop, "coneInnerAngle3D")) {
		result_value = actuator->m_3d.cone_inner_angle;

	} else if (!strcmp(prop, "coneOuterAngle3D")) {
		result_value = actuator->m_3d.cone_outer_angle;

	} else if (!strcmp(prop, "coneOuterGain3D")) {
		result_value = actuator->m_3d.cone_outer_gain;

	} else {
		Py_RETURN_NONE;
	}

	PyObject* result = PyFloat_FromDouble(result_value);
	return result;
}

PyObject* KX_SoundActuator::pyattr_get_audposition(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	float position = 0.0;

	if(actuator->m_handle)
		position = AUD_getPosition(actuator->m_handle);

	PyObject* result = PyFloat_FromDouble(position);

	return result;
}

PyObject* KX_SoundActuator::pyattr_get_gain(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	float gain = actuator->m_volume;

	PyObject* result = PyFloat_FromDouble(gain);

	return result;
}

PyObject* KX_SoundActuator::pyattr_get_pitch(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	float pitch = actuator->m_pitch;

	PyObject* result = PyFloat_FromDouble(pitch);

	return result;
}

PyObject* KX_SoundActuator::pyattr_get_rollOffFactor(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	float rollofffactor = actuator->m_3d.rolloff_factor;
	PyObject* result = PyFloat_FromDouble(rollofffactor);

	return result;
}

int KX_SoundActuator::pyattr_set_3d_property(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	const char* prop = attrdef->m_name;
	float prop_value = 0.0;
	AUD_3DSourceSetting setting = AUD_3DSS_NONE;

	if (!PyArg_Parse(value, "f", &prop_value))
		return PY_SET_ATTR_FAIL;

	// update the internal value
	if(!strcmp(prop, "maxGain3D")) {
		actuator->m_3d.max_gain = prop_value;
		setting = AUD_3DSS_MAX_GAIN;

	} else if (!strcmp(prop, "minGain3D")) {
		actuator->m_3d.min_gain = prop_value;
		setting = AUD_3DSS_MIN_GAIN;

	} else if (!strcmp(prop, "referenceDistance3D")) {
		actuator->m_3d.reference_distance = prop_value;
		setting = AUD_3DSS_REFERENCE_DISTANCE;

	} else if (!strcmp(prop, "maxDistance3D")) {
		actuator->m_3d.max_distance = prop_value;
		setting = AUD_3DSS_MAX_DISTANCE;

	} else if (!strcmp(prop, "rolloffFactor3D")) {
		actuator->m_3d.rolloff_factor = prop_value;
		setting = AUD_3DSS_ROLLOFF_FACTOR;

	} else if (!!strcmp(prop, "coneInnerAngle3D")) {
		actuator->m_3d.cone_inner_angle = prop_value;
		setting = AUD_3DSS_CONE_INNER_ANGLE;

	} else if (!strcmp(prop, "coneOuterAngle3D")) {
		actuator->m_3d.cone_outer_angle = prop_value;
		setting = AUD_3DSS_CONE_OUTER_ANGLE;

	} else if (!strcmp(prop, "coneOuterGain3D")) {
		actuator->m_3d.cone_outer_gain = prop_value;
		setting = AUD_3DSS_CONE_OUTER_GAIN;

	} else {
		return PY_SET_ATTR_FAIL;
	}	
	
	// if sound is working and 3D, set the new setting
	if(actuator->m_handle && actuator->m_is3d && setting != AUD_3DSS_NONE)
		AUD_set3DSourceSetting(actuator->m_handle, setting, prop_value);

	return PY_SET_ATTR_SUCCESS;
}

int KX_SoundActuator::pyattr_set_audposition(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);

	float position = 1.0;
	if (!PyArg_Parse(value, "f", &position))
		return PY_SET_ATTR_FAIL;

	if(actuator->m_handle)
		AUD_seek(actuator->m_handle, position);
	return PY_SET_ATTR_SUCCESS;
}

int KX_SoundActuator::pyattr_set_gain(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	float gain = 1.0;
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	if (!PyArg_Parse(value, "f", &gain))
		return PY_SET_ATTR_FAIL;

	actuator->m_volume = gain;
	if(actuator->m_handle)
		AUD_setSoundVolume(actuator->m_handle, gain);

	return PY_SET_ATTR_SUCCESS;
}

int KX_SoundActuator::pyattr_set_pitch(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	float pitch = 1.0;
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	if (!PyArg_Parse(value, "f", &pitch))
		return PY_SET_ATTR_FAIL;

	actuator->m_pitch = pitch;
	if(actuator->m_handle)
		AUD_setSoundPitch(actuator->m_handle, pitch);

	return PY_SET_ATTR_SUCCESS;
}

int KX_SoundActuator::pyattr_set_rollOffFactor(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_SoundActuator * actuator = static_cast<KX_SoundActuator *> (self);
	float rollofffactor = 1.0;
	if (!PyArg_Parse(value, "f", &rollofffactor))
		return PY_SET_ATTR_FAIL;

	actuator->m_3d.rolloff_factor = rollofffactor;
	if(actuator->m_handle)
		AUD_set3DSourceSetting(actuator->m_handle, AUD_3DSS_ROLLOFF_FACTOR, rollofffactor);

	return PY_SET_ATTR_SUCCESS;
}

#endif // DISABLE_PYTHON
