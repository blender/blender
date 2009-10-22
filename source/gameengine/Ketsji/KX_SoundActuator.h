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

#include "AUD_C-API.h"
#include "BKE_sound.h"

typedef struct KX_3DSoundSettings
{
	float min_gain;
	float max_gain;
	float reference_distance;
	float max_distance;
	float rolloff_factor;
	float cone_inner_angle;
	float cone_outer_angle;
	float cone_outer_gain;
} KX_3DSoundSettings;

class KX_SoundActuator : public SCA_IActuator
{
        Py_Header;
        bool					m_isplaying;
	AUD_Sound*				m_sound;
	float					m_volume;
	float					m_pitch;
	bool					m_is3d;
	KX_3DSoundSettings		m_3d;
        AUD_Handle*				m_handle;

	void play();

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
					 AUD_Sound* sound,
					 float volume,
					 float pitch,
					 bool is3d,
					 KX_3DSoundSettings settings,
					 KX_SOUNDACT_TYPE type);

	~KX_SoundActuator();

	virtual bool Update(double curtime, bool frame);

	CValue* GetReplica();
	void ProcessReplica();

#ifndef DISABLE_PYTHON

	/* -------------------------------------------------------------------- */
	/* Python interface --------------------------------------------------- */
	/* -------------------------------------------------------------------- */

	KX_PYMETHOD_DOC_NOARGS(KX_SoundActuator, startSound);
	KX_PYMETHOD_DOC_NOARGS(KX_SoundActuator, pauseSound);
	KX_PYMETHOD_DOC_NOARGS(KX_SoundActuator, stopSound);

	static int pyattr_set_gain(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static int pyattr_set_pitch(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static int pyattr_set_rollOffFactor(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static int pyattr_set_type(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);

	static PyObject* pyattr_get_gain(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject* pyattr_get_pitch(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject* pyattr_get_rollOffFactor(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject* pyattr_get_type(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);

#endif // DISABLE_PYTHON

};

#endif //__KX_SOUNDACTUATOR

