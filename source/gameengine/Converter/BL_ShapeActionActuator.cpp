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

/** \file gameengine/Converter/BL_ShapeActionActuator.cpp
 *  \ingroup bgeconv
 */


#include <cmath>

#include "SCA_LogicManager.h"
#include "BL_ShapeActionActuator.h"
#include "BL_ShapeDeformer.h"
#include "KX_GameObject.h"
#include "STR_HashedString.h"
#include "DNA_nla_types.h"
#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "BKE_action.h"
#include "DNA_armature_types.h"
#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "MT_Matrix4x4.h"

#include "EXP_FloatValue.h"
#include "EXP_PyObjectPlus.h"

extern "C" {
	#include "BKE_animsys.h"
	#include "BKE_key.h"
	#include "RNA_access.h"
}

BL_ShapeActionActuator::BL_ShapeActionActuator(SCA_IObject* gameobj,
					const STR_String& propname,
					const STR_String& framepropname,
					float starttime,
					float endtime,
					struct bAction *action,
					short	playtype,
					short	blendin,
					short	priority,
					float	stride) 
	: SCA_IActuator(gameobj, KX_ACT_SHAPEACTION),
		
	m_lastpos(0, 0, 0),
	m_blendframe(0),
	m_flag(0),
	m_startframe (starttime),
	m_endframe(endtime) ,
	m_starttime(0),
	m_localtime(starttime),
	m_lastUpdate(-1),
	m_blendin(blendin),
	m_blendstart(0),
	m_stridelength(stride),
	m_playtype(playtype),
	m_priority(priority),
	m_action(action),
	m_framepropname(framepropname),
	m_propname(propname)
{
	m_idptr = new PointerRNA();
	BL_DeformableGameObject *obj = (BL_DeformableGameObject*)GetParent();
	BL_ShapeDeformer *shape_deformer = dynamic_cast<BL_ShapeDeformer*>(obj->GetDeformer());
	RNA_id_pointer_create(&shape_deformer->GetKey()->id, m_idptr);
};

BL_ShapeActionActuator::~BL_ShapeActionActuator()
{
	if (m_idptr)
		delete m_idptr;
}

void BL_ShapeActionActuator::ProcessReplica()
{
	SCA_IActuator::ProcessReplica();
	m_localtime=m_startframe;
	m_lastUpdate=-1;
}

void BL_ShapeActionActuator::SetBlendTime(float newtime)
{
	m_blendframe = newtime;
}

CValue* BL_ShapeActionActuator::GetReplica() 
{
	BL_ShapeActionActuator* replica = new BL_ShapeActionActuator(*this);//m_float,GetName());
	replica->ProcessReplica();
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
	KeyBlock *kb;
	
	dstweight = 1.0F - srcweight;

	for (it=m_blendshape.begin(), kb = (KeyBlock *)key->block.first; 
	     kb && it != m_blendshape.end();
	     kb = (KeyBlock *)kb->next, it++)
	{
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

	curtime -= KX_KetsjiEngine::GetSuspendedDelta();
	
	// result = true if animation has to be continued, false if animation stops
	// maybe there are events for us in the queue !
	if (frame)
	{
		bNegativeEvent = m_negevent;
		bPositiveEvent = m_posevent;
		RemoveAllEvents();
		
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
	
	/* Determine pre-incrementation behavior and set appropriate flags */
	switch (m_playtype) {
	case ACT_ACTION_MOTION:
		if (bNegativeEvent) {
			keepgoing=false;
			apply=false;
		};
		break;
	case ACT_ACTION_FROM_PROP:
		if (bNegativeEvent) {
			apply=false;
			keepgoing=false;
		}
		break;
	case ACT_ACTION_LOOP_END:
		if (bPositiveEvent) {
			if (!(m_flag & ACT_FLAG_LOCKINPUT)) {
				m_flag &= ~ACT_FLAG_KEYUP;
				m_flag &= ~ACT_FLAG_REVERSE;
				m_flag |= ACT_FLAG_LOCKINPUT;
				m_localtime = m_startframe;
				m_starttime = curtime;
			}
		}
		if (bNegativeEvent) {
			m_flag |= ACT_FLAG_KEYUP;
		}
		break;
	case ACT_ACTION_LOOP_STOP:
		if (bPositiveEvent) {
			if (!(m_flag & ACT_FLAG_LOCKINPUT)) {
				m_flag &= ~ACT_FLAG_REVERSE;
				m_flag &= ~ACT_FLAG_KEYUP;
				m_flag |= ACT_FLAG_LOCKINPUT;
				SetStartTime(curtime);
			}
		}
		if (bNegativeEvent) {
			m_flag |= ACT_FLAG_KEYUP;
			m_flag &= ~ACT_FLAG_LOCKINPUT;
			keepgoing=false;
			apply=false;
		}
		break;
	case ACT_ACTION_PINGPONG:
		if (bPositiveEvent) {
			if (!(m_flag & ACT_FLAG_LOCKINPUT)) {
				m_flag &= ~ACT_FLAG_KEYUP;
				m_localtime = m_starttime;
				m_starttime = curtime;
				m_flag |= ACT_FLAG_LOCKINPUT;
			}
		}
		break;
	case ACT_ACTION_FLIPPER:
		if (bPositiveEvent) {
			if (!(m_flag & ACT_FLAG_LOCKINPUT)) {
				m_flag &= ~ACT_FLAG_REVERSE;
				m_flag |= ACT_FLAG_LOCKINPUT;
				SetStartTime(curtime);
			}
		}
		else if (bNegativeEvent) {
			m_flag |= ACT_FLAG_REVERSE;
			m_flag &= ~ACT_FLAG_LOCKINPUT;
			SetStartTime(curtime);
		}
		break;
	case ACT_ACTION_PLAY:
		if (bPositiveEvent) {
			if (!(m_flag & ACT_FLAG_LOCKINPUT)) {
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
	if (keepgoing) {
		if (m_playtype == ACT_ACTION_MOTION) {
			MT_Point3	newpos;
			MT_Point3	deltapos;
			
			newpos = obj->NodeGetWorldPosition();
			
			/* Find displacement */
			deltapos = newpos-m_lastpos;
			m_localtime += (length/m_stridelength) * deltapos.length();
			m_lastpos = newpos;
		}
		else {
			SetLocalTime(curtime);
		}
	}
	
	/* Check if a wrapping response is needed */
	if (length) {
		if (m_localtime < m_startframe || m_localtime > m_endframe)
		{
			m_localtime = m_startframe + fmod(m_localtime, length);
			wrap = true;
		}
	}
	else
		m_localtime = m_startframe;
	
	/* Perform post-increment tasks */
	switch (m_playtype) {
	case ACT_ACTION_FROM_PROP:
		{
			CValue* propval = GetParent()->GetProperty(m_propname);
			if (propval)
				m_localtime = propval->GetNumber();
			
			if (bNegativeEvent) {
				keepgoing=false;
			}
		}
		break;
	case ACT_ACTION_MOTION:
		break;
	case ACT_ACTION_LOOP_STOP:
		break;
	case ACT_ACTION_PINGPONG:
		if (wrap) {
			if (!(m_flag & ACT_FLAG_REVERSE))
				m_localtime = m_endframe;
			else 
				m_localtime = m_startframe;

			m_flag &= ~ACT_FLAG_LOCKINPUT;
			m_flag ^= ACT_FLAG_REVERSE; //flip direction
			keepgoing = false;
		}
		break;
	case ACT_ACTION_FLIPPER:
		if (wrap) {
			if (!(m_flag & ACT_FLAG_REVERSE)) {
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
		if (wrap) {
			if (m_flag & ACT_FLAG_KEYUP) {
				keepgoing = false;
				m_localtime = m_endframe;
				m_flag &= ~ACT_FLAG_LOCKINPUT;
			}
			SetStartTime(curtime);
		}
		break;
	case ACT_ACTION_PLAY:
		if (wrap) {
			m_localtime = m_endframe;
			keepgoing = false;
			m_flag &= ~ACT_FLAG_LOCKINPUT;
		}
		break;
	default:
		keepgoing = false;
		break;
	}
	
	/* Set the property if its defined */
	if (m_framepropname[0] != '\0') {
		CValue* propowner = GetParent();
		CValue* oldprop = propowner->GetProperty(m_framepropname);
		CValue* newval = new CFloatValue(m_localtime);
		if (oldprop) {
			oldprop->SetValue(newval);
		} else {
			propowner->SetProperty(m_framepropname, newval);
		}
		newval->Release();
	}
	
	if (bNegativeEvent)
		m_blendframe=0.0f;
	
	/* Apply the pose if necessary*/
	if (apply) {

		/* Priority test */
		if (obj->SetActiveAction(this, priority, curtime)) {
			BL_ShapeDeformer *shape_deformer = dynamic_cast<BL_ShapeDeformer*>(obj->GetDeformer());
			Key *key = NULL;

			if (shape_deformer)
				key = shape_deformer->GetKey();

			if (!key) {
				// this could happen if the mesh was changed in the middle of an action
				// and the new mesh has no key, stop the action
				keepgoing = false;
			}
			else {
				ListBase tchanbase= {NULL, NULL};
			
				if (m_blendin && m_blendframe==0.0f) {
					// this is the start of the blending, remember the startup shape
					obj->GetShape(m_blendshape);
					m_blendstart = curtime;
				}

				KeyBlock *kb;
				// We go through and clear out the keyblocks so there isn't any interference
				// from other shape actions
				for (kb=(KeyBlock *)key->block.first; kb; kb=(KeyBlock *)kb->next)
					kb->curval = 0.f;

				animsys_evaluate_action(m_idptr, m_action, NULL, m_localtime);

				// XXX - in 2.5 theres no way to do this. possibly not that important to support - Campbell
				if (0) { // XXX !execute_ipochannels(&tchanbase)) {
					// no update, this is possible if action does not match the keys, stop the action
					keepgoing = false;
				} 
				else {
					// the key have changed, apply blending if needed
					if (m_blendin && (m_blendframe<m_blendin)) {
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
		else {
			m_blendframe = 0.0f;
		}
	}
	
	if (!keepgoing) {
		m_blendframe = 0.0f;
	}
	return keepgoing;
};

#ifdef WITH_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */

PyTypeObject BL_ShapeActionActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"BL_ShapeActionActuator",
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


PyMethodDef BL_ShapeActionActuator::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef BL_ShapeActionActuator::Attributes[] = {
	KX_PYATTRIBUTE_FLOAT_RW("frameStart", 0, MAXFRAMEF, BL_ShapeActionActuator, m_startframe),
	KX_PYATTRIBUTE_FLOAT_RW("frameEnd", 0, MAXFRAMEF, BL_ShapeActionActuator, m_endframe),
	KX_PYATTRIBUTE_FLOAT_RW("blendIn", 0, MAXFRAMEF, BL_ShapeActionActuator, m_blendin),
	KX_PYATTRIBUTE_RW_FUNCTION("action", BL_ShapeActionActuator, pyattr_get_action, pyattr_set_action),
	KX_PYATTRIBUTE_SHORT_RW("priority", 0, 100, false, BL_ShapeActionActuator, m_priority),
	KX_PYATTRIBUTE_FLOAT_RW_CHECK("frame", 0, MAXFRAMEF, BL_ShapeActionActuator, m_localtime, CheckFrame),
	KX_PYATTRIBUTE_STRING_RW("propName", 0, MAX_PROP_NAME, false, BL_ShapeActionActuator, m_propname),
	KX_PYATTRIBUTE_STRING_RW("framePropName", 0, MAX_PROP_NAME, false, BL_ShapeActionActuator, m_framepropname),
	KX_PYATTRIBUTE_FLOAT_RW_CHECK("blendTime", 0, MAXFRAMEF, BL_ShapeActionActuator, m_blendframe, CheckBlendTime),
	KX_PYATTRIBUTE_SHORT_RW_CHECK("mode",0,100,false,BL_ShapeActionActuator,m_playtype,CheckType),
	{ NULL }	//Sentinel
};

PyObject *BL_ShapeActionActuator::pyattr_get_action(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	BL_ShapeActionActuator* self = static_cast<BL_ShapeActionActuator*>(self_v);
	return PyUnicode_FromString(self->GetAction() ? self->GetAction()->id.name+2 : "");
}

int BL_ShapeActionActuator::pyattr_set_action(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	BL_ShapeActionActuator* self = static_cast<BL_ShapeActionActuator*>(self_v);
	/* exact copy of BL_ActionActuator's function from here down */
	if (!PyUnicode_Check(value))
	{
		PyErr_SetString(PyExc_ValueError, "actuator.action = val: Shape Action Actuator, expected the string name of the action");
		return PY_SET_ATTR_FAIL;
	}

	bAction *action= NULL;
	STR_String val = _PyUnicode_AsString(value);
	
	if (val != "")
	{
		action= (bAction*)self->GetLogicManager()->GetActionByName(val);
		if (action==NULL)
		{
			PyErr_SetString(PyExc_ValueError, "actuator.action = val: Shape Action Actuator, action not found!");
			return PY_SET_ATTR_FAIL;
		}
	}
	
	self->SetAction(action);
	return PY_SET_ATTR_SUCCESS;

}

#endif // WITH_PYTHON
