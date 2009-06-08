/**
 * KX_SoundActuator.h
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

#ifndef __KX_SOUNDACTUATOR
#define __KX_SOUNDACTUATOR

#include "SCA_IActuator.h"

class KX_SoundActuator : public SCA_IActuator
{
	Py_Header;
	bool					m_lastEvent;
	bool					m_isplaying;
	/* just some handles to the audio-data... */
	class SND_SoundObject*	m_soundObject;
	class SND_Scene*		m_soundScene;
	short					m_startFrame;
	short					m_endFrame;
	bool					m_pino;
public:

	enum KX_SOUNDACT_TYPE
	{
			KX_SOUNDACT_NODEF = 0,
			KX_SOUNDACT_PLAYSTOP,
			KX_SOUNDACT_PLAYEND,
			KX_SOUNDACT_LOOPSTOP,
			KX_SOUNDACT_LOOPEND,
			KX_SOUNDACT_LOOPBIDIRECTIONAL,
			KX_SOUNDACT_LOOPBIDIRECTIONAL_STOP,
			KX_SOUNDACT_MAX
	};

	KX_SOUNDACT_TYPE		m_type;

	KX_SoundActuator(SCA_IObject* gameobj,
					class SND_SoundObject* sndobj,
					class SND_Scene*	sndscene,
					KX_SOUNDACT_TYPE type,
					short start,
					short end,
					PyTypeObject* T=&Type);

	~KX_SoundActuator();

	void setSoundObject(class SND_SoundObject* soundobject);
	virtual bool Update(double curtime, bool frame);

	CValue* GetReplica();
	void ProcessReplica();

	/* -------------------------------------------------------------------- */
	/* Python interface --------------------------------------------------- */
	/* -------------------------------------------------------------------- */

	virtual PyObject* py_getattro(PyObject *attr);
	virtual PyObject* py_getattro_dict();
	virtual int py_setattro(PyObject *attr, PyObject* value);

	KX_PYMETHOD_DOC_NOARGS(KX_SoundActuator, startSound);
	KX_PYMETHOD_DOC_NOARGS(KX_SoundActuator, pauseSound);
	KX_PYMETHOD_DOC_NOARGS(KX_SoundActuator, stopSound);

	static int pyattr_set_filename(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static int pyattr_set_gain(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static int pyattr_set_pitch(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static int pyattr_set_rollOffFactor(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static int pyattr_set_looping(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static int pyattr_set_position(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static int pyattr_set_velocity(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static int pyattr_set_orientation(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static int pyattr_set_type(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);

	static PyObject* pyattr_get_filename(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject* pyattr_get_gain(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject* pyattr_get_pitch(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject* pyattr_get_looping(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject* pyattr_get_rollOffFactor(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject* pyattr_get_position(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject* pyattr_get_velocity(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject* pyattr_get_orientation(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject* pyattr_get_type(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);

	// Deprecated ----->
	KX_PYMETHOD_VARARGS(KX_SoundActuator,SetFilename);
	KX_PYMETHOD_NOARGS(KX_SoundActuator,GetFilename);
	KX_PYMETHOD_VARARGS(KX_SoundActuator,SetGain);
	KX_PYMETHOD_NOARGS(KX_SoundActuator,GetGain);
	KX_PYMETHOD_VARARGS(KX_SoundActuator,SetPitch);
	KX_PYMETHOD_NOARGS(KX_SoundActuator,GetPitch);
	KX_PYMETHOD_VARARGS(KX_SoundActuator,SetRollOffFactor);
	KX_PYMETHOD_NOARGS(KX_SoundActuator,GetRollOffFactor);
	KX_PYMETHOD_VARARGS(KX_SoundActuator,SetLooping);
	KX_PYMETHOD_NOARGS(KX_SoundActuator,GetLooping);
	KX_PYMETHOD_VARARGS(KX_SoundActuator,SetPosition);
	KX_PYMETHOD_VARARGS(KX_SoundActuator,SetVelocity);
	KX_PYMETHOD_VARARGS(KX_SoundActuator,SetOrientation);
	KX_PYMETHOD_VARARGS(KX_SoundActuator,SetType);
	KX_PYMETHOD_NOARGS(KX_SoundActuator,GetType);
	// <-----

};

#endif //__KX_SOUNDACTUATOR

