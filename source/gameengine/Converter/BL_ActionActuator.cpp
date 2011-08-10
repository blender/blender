/*
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

/** \file gameengine/Converter/BL_ActionActuator.cpp
 *  \ingroup bgeconv
 */


#include "SCA_LogicManager.h"
#include "BL_ActionActuator.h"
#include "BL_ArmatureObject.h"
#include "BL_SkinDeformer.h"
#include "BL_Action.h"
#include "KX_GameObject.h"
#include "STR_HashedString.h"
#include "MEM_guardedalloc.h"
#include "DNA_nla_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_scene_types.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "MT_Matrix4x4.h"

#include "BKE_action.h"
#include "FloatValue.h"
#include "PyObjectPlus.h"
#include "KX_PyMath.h"

extern "C" {
#include "BKE_animsys.h"
#include "BKE_action.h"
#include "RNA_access.h"
#include "RNA_define.h"
}

BL_ActionActuator::BL_ActionActuator(SCA_IObject* gameobj,
					const STR_String& propname,
					const STR_String& framepropname,
					float starttime,
					float endtime,
					struct bAction *action,
					short	playtype,
					short	blendin,
					short	priority,
					short	layer,
					float	layer_weight,
					short	ipo_flags,
					short	end_reset,
					float	stride) 
	: SCA_IActuator(gameobj, KX_ACT_ACTION),
		
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
	m_layer(layer),
	m_layer_weight(layer_weight),
	m_ipo_flags(ipo_flags),
	m_pose(NULL),
	m_blendpose(NULL),
	m_userpose(NULL),
	m_action(action),
	m_propname(propname),
	m_framepropname(framepropname)		
{
	if (!end_reset)
		m_flag |= ACT_FLAG_CONTINUE;
};

BL_ActionActuator::~BL_ActionActuator()
{
	if (m_pose)
		game_free_pose(m_pose);
	if (m_userpose)
		game_free_pose(m_userpose);
	if (m_blendpose)
		game_free_pose(m_blendpose);
}

void BL_ActionActuator::ProcessReplica()
{
	SCA_IActuator::ProcessReplica();
	
	m_pose = NULL;
	m_blendpose = NULL;
	m_localtime=m_startframe;
	m_lastUpdate=-1;
	
}

void BL_ActionActuator::SetBlendTime (float newtime){
	m_blendframe = newtime;
}

CValue* BL_ActionActuator::GetReplica() {
	BL_ActionActuator* replica = new BL_ActionActuator(*this);//m_float,GetName());
	replica->ProcessReplica();
	return replica;
}

bool BL_ActionActuator::Update(double curtime, bool frame)
{
	bool bNegativeEvent = false;
	bool bPositiveEvent = false;
	bool use_continue = false;
	KX_GameObject *obj = (KX_GameObject*)GetParent();
	short play_mode = BL_Action::ACT_MODE_PLAY;
	float start = m_startframe, end = m_endframe;

	// If we don't have an action, we can't do anything
	if (!m_action)
		return false;

	// Convert playmode
	if (m_playtype == ACT_ACTION_LOOP_END)
		play_mode = BL_Action::ACT_MODE_LOOP;
	else if (m_playtype == ACT_ACTION_LOOP_STOP)
		play_mode = BL_Action::ACT_MODE_LOOP;
	else if (m_playtype == ACT_ACTION_PINGPONG)
	{
		// We handle ping pong ourselves to increase compabitility with the pre-Pepper actuator
		play_mode = BL_Action::ACT_MODE_PLAY;
		
		if (m_flag & ACT_FLAG_REVERSE)
		{
			float tmp = start;
			start = end;
			end = tmp;
			m_localtime = end;
		}
	}
	else if (m_playtype == ACT_ACTION_FROM_PROP)
	{
		CValue* prop = GetParent()->GetProperty(m_propname);

		play_mode = BL_Action::ACT_MODE_PLAY;
		start = end = prop->GetNumber();
	}

	// Continue only really makes sense for play stop and flipper. All other modes go until they are complete.
	if (m_flag & ACT_FLAG_CONTINUE &&
		(m_playtype == ACT_ACTION_LOOP_STOP ||
		m_playtype == ACT_ACTION_FLIPPER))
		use_continue = true;
	
	
	// Handle events
	if (frame)
	{
		bNegativeEvent = m_negevent;
		bPositiveEvent = m_posevent;
		RemoveAllEvents();
	}

	if (use_continue && m_flag & ACT_FLAG_ACTIVE)
		m_localtime = obj->GetActionFrame(m_layer);
	
	if (bPositiveEvent)
	{
		if (obj->PlayAction(m_action->id.name+2, start, end, m_layer, m_priority, m_blendin, play_mode, m_layer_weight, m_ipo_flags))
		{
			m_flag |= ACT_FLAG_ACTIVE;
			if (use_continue)
				obj->SetActionFrame(m_layer, m_localtime);

			if (m_playtype == ACT_ACTION_PLAY)
				m_flag |= ACT_FLAG_PLAY_END;
			else
				m_flag &= ~ACT_FLAG_PLAY_END;
		}
		else
			return false;
	}
	else if ((m_flag & ACT_FLAG_ACTIVE) && bNegativeEvent)
	{	
		bAction *curr_action = obj->GetCurrentAction(m_layer);
		if (curr_action && curr_action != m_action)
		{
			// Someone changed the action on us, so we wont mess with it
			// Hopefully there wont be too many problems with two actuators using
			// the same action...
			m_flag &= ~ACT_FLAG_ACTIVE;
			return false;
		}

		
		m_localtime = obj->GetActionFrame(m_layer);
		if (m_localtime < min(m_startframe, m_endframe) || m_localtime > max(m_startframe, m_endframe))
			m_localtime = m_startframe;

		if (m_playtype == ACT_ACTION_LOOP_STOP)
		{
			obj->StopAction(m_layer); // Stop the action after getting the frame

			// We're done
			m_flag &= ~ACT_FLAG_ACTIVE;
			return false;
		}
		else if (m_playtype == ACT_ACTION_LOOP_END || m_playtype == ACT_ACTION_PINGPONG)
		{
			// Convert into a play and let it finish
			obj->SetPlayMode(m_layer, BL_Action::ACT_MODE_PLAY);

			m_flag |= ACT_FLAG_PLAY_END;

			if (m_playtype == ACT_ACTION_PINGPONG)
				m_flag ^= ACT_FLAG_REVERSE;
		}
		else if (m_playtype == ACT_ACTION_FLIPPER)
		{
			// Convert into a play action and play back to the beginning
			end = start;
			start = obj->GetActionFrame(m_layer);
			obj->PlayAction(m_action->id.name+2, start, end, m_layer, m_priority, 0, BL_Action::ACT_MODE_PLAY, m_layer_weight, m_ipo_flags);

			m_flag |= ACT_FLAG_PLAY_END;
		}
	}

	// Handle a frame property if it's defined
	if ((m_flag & ACT_FLAG_ACTIVE) && m_framepropname[0] != 0)
	{
		CValue* oldprop = obj->GetProperty(m_framepropname);
		CValue* newval = new CFloatValue(obj->GetActionFrame(m_layer));
		if (oldprop)
			oldprop->SetValue(newval);
		else
			obj->SetProperty(m_framepropname, newval);

		newval->Release();
	}

	// Handle a finished animation
	if ((m_flag & ACT_FLAG_PLAY_END) && obj->IsActionDone(m_layer))
	{
		m_flag &= ~ACT_FLAG_ACTIVE;
		obj->StopAction(m_layer);
		return false;
	}

	return true;
}

#ifdef WITH_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

PyObject* BL_ActionActuator::PyGetChannel(PyObject* value) {
	char *string= _PyUnicode_AsString(value);
	
	if (!string) {
		PyErr_SetString(PyExc_TypeError, "expected a single string");
		return NULL;
	}
	
	bPoseChannel *pchan;
	
	if(m_userpose==NULL && m_pose==NULL) {
		BL_ArmatureObject *obj = (BL_ArmatureObject*)GetParent();
		obj->GetPose(&m_pose); /* Get the underlying pose from the armature */
	}
	
	// get_pose_channel accounts for NULL pose, run on both incase one exists but
	// the channel doesnt
	if(		!(pchan=get_pose_channel(m_userpose, string)) &&
			!(pchan=get_pose_channel(m_pose, string))  )
	{
		PyErr_SetString(PyExc_ValueError, "channel doesnt exist");
		return NULL;
	}

	PyObject *ret = PyTuple_New(3);
	
	PyObject *list = PyList_New(3); 
	PyList_SET_ITEM(list, 0, PyFloat_FromDouble(pchan->loc[0]));
	PyList_SET_ITEM(list, 1, PyFloat_FromDouble(pchan->loc[1]));
	PyList_SET_ITEM(list, 2, PyFloat_FromDouble(pchan->loc[2]));
	PyTuple_SET_ITEM(ret, 0, list);
	
	list = PyList_New(3);
	PyList_SET_ITEM(list, 0, PyFloat_FromDouble(pchan->size[0]));
	PyList_SET_ITEM(list, 1, PyFloat_FromDouble(pchan->size[1]));
	PyList_SET_ITEM(list, 2, PyFloat_FromDouble(pchan->size[2]));
	PyTuple_SET_ITEM(ret, 1, list);
	
	list = PyList_New(4);
	PyList_SET_ITEM(list, 0, PyFloat_FromDouble(pchan->quat[0]));
	PyList_SET_ITEM(list, 1, PyFloat_FromDouble(pchan->quat[1]));
	PyList_SET_ITEM(list, 2, PyFloat_FromDouble(pchan->quat[2]));
	PyList_SET_ITEM(list, 3, PyFloat_FromDouble(pchan->quat[3]));
	PyTuple_SET_ITEM(ret, 2, list);

	return ret;
/*
	return Py_BuildValue("([fff][fff][ffff])",
		pchan->loc[0], pchan->loc[1], pchan->loc[2],
		pchan->size[0], pchan->size[1], pchan->size[2],
		pchan->quat[0], pchan->quat[1], pchan->quat[2], pchan->quat[3] );
*/
}

/*     setChannel                                                         */
KX_PYMETHODDEF_DOC(BL_ActionActuator, setChannel,
"setChannel(channel, matrix)\n"
"\t - channel   : A string specifying the name of the bone channel.\n"
"\t - matrix    : A 4x4 matrix specifying the overriding transformation\n"
"\t               as an offset from the bone's rest position.\n")
{
	BL_ArmatureObject *obj = (BL_ArmatureObject*)GetParent();
	char *string;
	PyObject *pymat= NULL;
	PyObject *pyloc= NULL, *pysize= NULL, *pyquat= NULL;
	bPoseChannel *pchan;
	
	if(PyTuple_Size(args)==2) {
		if (!PyArg_ParseTuple(args,"sO:setChannel", &string, &pymat)) // matrix
			return NULL;
	}
	else if(PyTuple_Size(args)==4) {
		if (!PyArg_ParseTuple(args,"sOOO:setChannel", &string, &pyloc, &pysize, &pyquat)) // loc/size/quat
			return NULL;
	}
	else {
		PyErr_SetString(PyExc_ValueError, "Expected a string and a 4x4 matrix (2 args) or a string and loc/size/quat sequences (4 args)");
		return NULL;
	}
	
	if(pymat) {
		float matrix[4][4];
		MT_Matrix4x4 mat;
		
		if(!PyMatTo(pymat, mat))
			return NULL;
		
		mat.getValue((float*)matrix);
		
		BL_ArmatureObject *obj = (BL_ArmatureObject*)GetParent();
		
		if (!m_userpose) {
			if(!m_pose)
				obj->GetPose(&m_pose); /* Get the underlying pose from the armature */
			game_copy_pose(&m_userpose, m_pose, 0);
		}
		// pchan= verify_pose_channel(m_userpose, string); // adds the channel if its not there.
		pchan= get_pose_channel(m_userpose, string); // adds the channel if its not there.
		
		if(pchan) {
			VECCOPY (pchan->loc, matrix[3]);
			mat4_to_size( pchan->size,matrix);
			mat4_to_quat( pchan->quat,matrix);
		}
	}
	else {
		MT_Vector3 loc;
		MT_Vector3 size;
		MT_Quaternion quat;
		
		if (!PyVecTo(pyloc, loc) || !PyVecTo(pysize, size) || !PyQuatTo(pyquat, quat))
			return NULL;
		
		// same as above
		if (!m_userpose) {
			if(!m_pose)
				obj->GetPose(&m_pose); /* Get the underlying pose from the armature */
			game_copy_pose(&m_userpose, m_pose, 0);
		}
		// pchan= verify_pose_channel(m_userpose, string);
		pchan= get_pose_channel(m_userpose, string); // adds the channel if its not there.
		
		// for some reason loc.setValue(pchan->loc) fails
		if(pchan) {
			pchan->loc[0]= loc[0]; pchan->loc[1]= loc[1]; pchan->loc[2]= loc[2];
			pchan->size[0]= size[0]; pchan->size[1]= size[1]; pchan->size[2]= size[2];
			pchan->quat[0]= quat[3]; pchan->quat[1]= quat[0]; pchan->quat[2]= quat[1]; pchan->quat[3]= quat[2]; /* notice xyzw -> wxyz is intentional */
		}
	}
	
	if(pchan==NULL) {
		PyErr_SetString(PyExc_ValueError, "Channel could not be found, use the 'channelNames' attribute to get a list of valid channels");
		return NULL;
	}
	
	Py_RETURN_NONE;
}

/* ------------------------------------------------------------------------- */
/* Python Integration Hooks					                                 */
/* ------------------------------------------------------------------------- */

PyTypeObject BL_ActionActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"BL_ActionActuator",
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

PyMethodDef BL_ActionActuator::Methods[] = {
	{"getChannel", (PyCFunction) BL_ActionActuator::sPyGetChannel, METH_O},
	KX_PYMETHODTABLE(BL_ActionActuator, setChannel),
	{NULL,NULL} //Sentinel
};

PyAttributeDef BL_ActionActuator::Attributes[] = {
	KX_PYATTRIBUTE_FLOAT_RW("frameStart", 0, MAXFRAMEF, BL_ActionActuator, m_startframe),
	KX_PYATTRIBUTE_FLOAT_RW("frameEnd", 0, MAXFRAMEF, BL_ActionActuator, m_endframe),
	KX_PYATTRIBUTE_FLOAT_RW("blendIn", 0, MAXFRAMEF, BL_ActionActuator, m_blendin),
	KX_PYATTRIBUTE_RW_FUNCTION("action", BL_ActionActuator, pyattr_get_action, pyattr_set_action),
	KX_PYATTRIBUTE_RO_FUNCTION("channelNames", BL_ActionActuator, pyattr_get_channel_names),
	KX_PYATTRIBUTE_SHORT_RW("priority", 0, 100, false, BL_ActionActuator, m_priority),
	KX_PYATTRIBUTE_FLOAT_RW_CHECK("frame", 0, MAXFRAMEF, BL_ActionActuator, m_localtime, CheckFrame),
	KX_PYATTRIBUTE_STRING_RW("propName", 0, 31, false, BL_ActionActuator, m_propname),
	KX_PYATTRIBUTE_STRING_RW("framePropName", 0, 31, false, BL_ActionActuator, m_framepropname),
	KX_PYATTRIBUTE_RW_FUNCTION("useContinue", BL_ActionActuator, pyattr_get_use_continue, pyattr_set_use_continue),
	KX_PYATTRIBUTE_FLOAT_RW_CHECK("blendTime", 0, MAXFRAMEF, BL_ActionActuator, m_blendframe, CheckBlendTime),
	KX_PYATTRIBUTE_SHORT_RW_CHECK("mode",0,100,false,BL_ActionActuator,m_playtype,CheckType),
	{ NULL }	//Sentinel
};

PyObject* BL_ActionActuator::pyattr_get_action(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	BL_ActionActuator* self= static_cast<BL_ActionActuator*>(self_v);
	return PyUnicode_FromString(self->GetAction() ? self->GetAction()->id.name+2 : "");
}

int BL_ActionActuator::pyattr_set_action(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	BL_ActionActuator* self= static_cast<BL_ActionActuator*>(self_v);
	
	if (!PyUnicode_Check(value))
	{
		PyErr_SetString(PyExc_ValueError, "actuator.action = val: Action Actuator, expected the string name of the action");
		return PY_SET_ATTR_FAIL;
	}

	bAction *action= NULL;
	STR_String val = _PyUnicode_AsString(value);
	
	if (val != "")
	{
		action= (bAction*)SCA_ILogicBrick::m_sCurrentLogicManager->GetActionByName(val);
		if (!action)
		{
			PyErr_SetString(PyExc_ValueError, "actuator.action = val: Action Actuator, action not found!");
			return PY_SET_ATTR_FAIL;
		}
	}
	
	self->SetAction(action);
	return PY_SET_ATTR_SUCCESS;

}

PyObject* BL_ActionActuator::pyattr_get_channel_names(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	BL_ActionActuator* self= static_cast<BL_ActionActuator*>(self_v);
	PyObject *ret= PyList_New(0);
	PyObject *item;
	
	bPose *pose= ((BL_ArmatureObject*)self->GetParent())->GetOrigPose();
	
	if(pose) {
		bPoseChannel *pchan;
		for(pchan= (bPoseChannel *)pose->chanbase.first; pchan; pchan= (bPoseChannel *)pchan->next) {
			item= PyUnicode_FromString(pchan->name);
			PyList_Append(ret, item);
			Py_DECREF(item);
		}
	}
	
	return ret;
}

PyObject* BL_ActionActuator::pyattr_get_use_continue(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	BL_ActionActuator* self= static_cast<BL_ActionActuator*>(self_v);
	return PyBool_FromLong(self->m_flag & ACT_FLAG_CONTINUE);
}

int BL_ActionActuator::pyattr_set_use_continue(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	BL_ActionActuator* self= static_cast<BL_ActionActuator*>(self_v);
	
	if (PyObject_IsTrue(value))
		self->m_flag |= ACT_FLAG_CONTINUE;
	else
		self->m_flag &= ~ACT_FLAG_CONTINUE;
	
	return PY_SET_ATTR_SUCCESS;
}

#endif // WITH_PYTHON
