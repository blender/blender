/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file BL_ShapeActionActuator.h
 *  \ingroup bgeconv
 */

#ifndef __BL_SHAPEACTIONACTUATOR_H__
#define __BL_SHAPEACTIONACTUATOR_H__

#include "CTR_HashedPtr.h"
#include "SCA_IActuator.h"
#include "BL_ActionActuator.h"
#include "MT_Point3.h"
#include <vector>

struct Key;
class BL_ShapeActionActuator : public SCA_IActuator  
{
public:
	Py_Header
	BL_ShapeActionActuator(SCA_IObject* gameobj,
						const STR_String& propname,
						const STR_String& framepropname,
						float starttime,
						float endtime,
						struct bAction *action,
						short	playtype,
						short	blendin,
						short	priority,
						float	stride);
	virtual ~BL_ShapeActionActuator();
	virtual	bool Update(double curtime, bool frame);
	virtual CValue* GetReplica();
	virtual void ProcessReplica();
	
	void SetBlendTime (float newtime);
	void BlendShape(struct Key* key, float weight);
	
	bAction*	GetAction() { return m_action; }
	void		SetAction(bAction* act) { m_action= act; }

#ifdef WITH_PYTHON

	static PyObject*	pyattr_get_action(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_action(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);

	static int CheckBlendTime(void *self, const PyAttributeDef*)
	{
		BL_ShapeActionActuator* act = reinterpret_cast<BL_ShapeActionActuator*>(self);

		if (act->m_blendframe > act->m_blendin)
			act->m_blendframe = act->m_blendin;

		return 0;
	}
	static int CheckFrame(void *self, const PyAttributeDef*)
	{
		BL_ShapeActionActuator* act = reinterpret_cast<BL_ShapeActionActuator*>(self);

		if (act->m_localtime < act->m_startframe)
			act->m_localtime = act->m_startframe;
		else if (act->m_localtime > act->m_endframe)
			act->m_localtime = act->m_endframe;

		return 0;
	}
	static int CheckType(void *self, const PyAttributeDef*)
	{
		BL_ShapeActionActuator* act = reinterpret_cast<BL_ShapeActionActuator*>(self);

		switch (act->m_playtype) {
			case ACT_ACTION_PLAY:
			case ACT_ACTION_PINGPONG:
			case ACT_ACTION_FLIPPER:
			case ACT_ACTION_LOOP_STOP:
			case ACT_ACTION_LOOP_END:
			case ACT_ACTION_FROM_PROP:
				return 0;
			default:
				PyErr_SetString(PyExc_ValueError, "Shape Action Actuator, invalid play type supplied");
				return 1;
		}

	}

#endif  /* WITH_PYTHON */

protected:

	void SetStartTime(float curtime);
	void SetLocalTime(float curtime);
	bool ClampLocalTime();

	MT_Point3	m_lastpos;
	float	m_blendframe;
	int		m_flag;
	/** The frame this action starts */
	float	m_startframe;
	/** The frame this action ends */
	float	m_endframe;
	/** The time this action started */
	float	m_starttime;
	/** The current time of the action */
	float	m_localtime;
	
	float	m_lastUpdate;
	float	m_blendin;
	float	m_blendstart;
	float	m_stridelength;
	short	m_playtype;
	short	m_priority;
	struct bAction *m_action;
	STR_String	m_framepropname;
	STR_String	m_propname;
	vector<float> m_blendshape;
	struct PointerRNA *m_idptr;
};

#endif  /* __BL_SHAPEACTIONACTUATOR_H__ */
