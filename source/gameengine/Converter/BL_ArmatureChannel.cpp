/**
 * $Id$
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "DNA_armature_types.h"
#include "BL_ArmatureChannel.h"
#include "BL_ArmatureObject.h"
#include "BL_ArmatureConstraint.h"
#include "BLI_math.h"
#include "BLI_string.h"

#ifndef DISABLE_PYTHON

PyTypeObject BL_ArmatureChannel::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"BL_ArmatureChannel",
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
	&CValue::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyObject* BL_ArmatureChannel::py_repr(void)
{
	return PyUnicode_FromString(m_posechannel->name);
}

PyObject *BL_ArmatureChannel::GetProxy()
{
	return GetProxyPlus_Ext(this, &Type, m_posechannel);
}

PyObject *BL_ArmatureChannel::NewProxy(bool py_owns)
{
	return NewProxyPlus_Ext(this, &Type, m_posechannel, py_owns);
}

#endif // DISABLE_PYTHON

BL_ArmatureChannel::BL_ArmatureChannel(
	BL_ArmatureObject *armature, 
	bPoseChannel *posechannel)
	: PyObjectPlus(), m_posechannel(posechannel), m_armature(armature)
{
}

BL_ArmatureChannel::~BL_ArmatureChannel()
{
}

#ifndef DISABLE_PYTHON

// PYTHON

PyMethodDef BL_ArmatureChannel::Methods[] = {
  {NULL,NULL} //Sentinel
};

// order of definition of attributes, must match Attributes[] array
#define BCA_BONE		0
#define BCA_PARENT		1

PyAttributeDef BL_ArmatureChannel::Attributes[] = {
	// Keep these attributes in order of BCA_ defines!!! used by py_attr_getattr and py_attr_setattr
	KX_PYATTRIBUTE_RO_FUNCTION("bone",BL_ArmatureChannel,py_attr_getattr),	
	KX_PYATTRIBUTE_RO_FUNCTION("parent",BL_ArmatureChannel,py_attr_getattr),	
	
	{ NULL }	//Sentinel
};

/* attributes directly taken from bPoseChannel */
PyAttributeDef BL_ArmatureChannel::AttributesPtr[] = {
	KX_PYATTRIBUTE_CHAR_RO("name",bPoseChannel,name),
	KX_PYATTRIBUTE_FLAG_RO("has_ik",bPoseChannel,flag, POSE_CHAIN),
	KX_PYATTRIBUTE_FLAG_NEGATIVE_RO("ik_dof_x",bPoseChannel,ikflag, BONE_IK_NO_XDOF),
	KX_PYATTRIBUTE_FLAG_NEGATIVE_RO("ik_dof_y",bPoseChannel,ikflag, BONE_IK_NO_YDOF),
	KX_PYATTRIBUTE_FLAG_NEGATIVE_RO("ik_dof_z",bPoseChannel,ikflag, BONE_IK_NO_ZDOF),
	KX_PYATTRIBUTE_FLAG_RO("ik_limit_x",bPoseChannel,ikflag, BONE_IK_XLIMIT),
	KX_PYATTRIBUTE_FLAG_RO("ik_limit_y",bPoseChannel,ikflag, BONE_IK_YLIMIT),
	KX_PYATTRIBUTE_FLAG_RO("ik_limit_z",bPoseChannel,ikflag, BONE_IK_ZLIMIT),
	KX_PYATTRIBUTE_FLAG_RO("ik_rot_control",bPoseChannel,ikflag, BONE_IK_ROTCTL),
	KX_PYATTRIBUTE_FLAG_RO("ik_lin_control",bPoseChannel,ikflag, BONE_IK_LINCTL),
	KX_PYATTRIBUTE_FLOAT_VECTOR_RW("location",-FLT_MAX,FLT_MAX,bPoseChannel,loc,3),
	KX_PYATTRIBUTE_FLOAT_VECTOR_RW("scale",-FLT_MAX,FLT_MAX,bPoseChannel,size,3),
	KX_PYATTRIBUTE_FLOAT_VECTOR_RW("rotation_quaternion",-1.0f,1.0f,bPoseChannel,quat,4),
	KX_PYATTRIBUTE_FLOAT_VECTOR_RW("rotaion_euler",-10.f,10.f,bPoseChannel,eul,3),
	KX_PYATTRIBUTE_SHORT_RW("rotation_mode",0,ROT_MODE_MAX-1,false,bPoseChannel,rotmode),
	KX_PYATTRIBUTE_FLOAT_MATRIX_RO("channel_matrix",bPoseChannel,chan_mat,4),
	KX_PYATTRIBUTE_FLOAT_MATRIX_RO("pose_matrix",bPoseChannel,pose_mat,4),
	KX_PYATTRIBUTE_FLOAT_VECTOR_RO("pose_head",bPoseChannel,pose_head,3),
	KX_PYATTRIBUTE_FLOAT_VECTOR_RO("pose_tail",bPoseChannel,pose_tail,3),
	KX_PYATTRIBUTE_FLOAT_RO("ik_min_x",bPoseChannel,limitmin[0]),
	KX_PYATTRIBUTE_FLOAT_RO("ik_max_x",bPoseChannel,limitmax[0]),
	KX_PYATTRIBUTE_FLOAT_RO("ik_min_y",bPoseChannel,limitmin[1]),
	KX_PYATTRIBUTE_FLOAT_RO("ik_max_y",bPoseChannel,limitmax[1]),
	KX_PYATTRIBUTE_FLOAT_RO("ik_min_z",bPoseChannel,limitmin[2]),
	KX_PYATTRIBUTE_FLOAT_RO("ik_max_z",bPoseChannel,limitmax[2]),
	KX_PYATTRIBUTE_FLOAT_RO("ik_stiffness_x",bPoseChannel,stiffness[0]),
	KX_PYATTRIBUTE_FLOAT_RO("ik_stiffness_y",bPoseChannel,stiffness[1]),
	KX_PYATTRIBUTE_FLOAT_RO("ik_stiffness_z",bPoseChannel,stiffness[2]),
	KX_PYATTRIBUTE_FLOAT_RO("ik_stretch",bPoseChannel,ikstretch),
	KX_PYATTRIBUTE_FLOAT_RW("ik_rot_weight",0,1.0f,bPoseChannel,ikrotweight),
	KX_PYATTRIBUTE_FLOAT_RW("ik_lin_weight",0,1.0f,bPoseChannel,iklinweight),
	KX_PYATTRIBUTE_RW_FUNCTION("joint_rotation",BL_ArmatureChannel,py_attr_get_joint_rotation,py_attr_set_joint_rotation),
	{ NULL }	//Sentinel
};

PyObject* BL_ArmatureChannel::py_attr_getattr(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	BL_ArmatureChannel* self= static_cast<BL_ArmatureChannel*>(self_v);
	bPoseChannel* channel = self->m_posechannel;
	int attr_order = attrdef-Attributes;

	if (!channel) {
		PyErr_SetString(PyExc_AttributeError, "channel is NULL");
		return NULL;
	}

	switch (attr_order) {
	case BCA_BONE:
		// bones are standalone proxy
		return NewProxyPlus_Ext(NULL,&BL_ArmatureBone::Type,channel->bone,false);
	case BCA_PARENT:
		{
			BL_ArmatureChannel* parent = self->m_armature->GetChannel(channel->parent);
			if (parent)
				return parent->GetProxy();
			else
				Py_RETURN_NONE;
		}
	}
	PyErr_SetString(PyExc_AttributeError, "channel unknown attribute");
	return NULL;
}

int BL_ArmatureChannel::py_attr_setattr(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	BL_ArmatureChannel* self= static_cast<BL_ArmatureChannel*>(self_v);
	bPoseChannel* channel = self->m_posechannel;
	int attr_order = attrdef-Attributes;

//	int ival;
//	double dval;
//	char* sval;
//	KX_GameObject *oval;

	if (!channel) {
		PyErr_SetString(PyExc_AttributeError, "channel is NULL");
		return PY_SET_ATTR_FAIL;
	}
	
	switch (attr_order) {
	default:
		break;
	}

	PyErr_SetString(PyExc_AttributeError, "channel unknown attribute");
	return PY_SET_ATTR_FAIL;
}

PyObject* BL_ArmatureChannel::py_attr_get_joint_rotation(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	BL_ArmatureChannel* self= static_cast<BL_ArmatureChannel*>(self_v);
	bPoseChannel* pchan = self->m_posechannel;
	// decompose the pose matrix in euler rotation
	float rest_mat[3][3];
	float pose_mat[3][3];
	float joint_mat[3][3];
	float joints[3];
	float norm;
	double sa, ca;
	// get rotation in armature space
	copy_m3_m4(pose_mat, pchan->pose_mat);
	normalize_m3(pose_mat);
	if (pchan->parent) {
		// bone has a parent, compute the rest pose of the bone taking actual pose of parent
		mul_m3_m3m4(rest_mat, pchan->bone->bone_mat, pchan->parent->pose_mat);
		normalize_m3(rest_mat);
	} else {
		// otherwise, the bone matrix in armature space is the rest pose
		copy_m3_m4(rest_mat, pchan->bone->arm_mat);
	}
	// remove the rest pose to get the joint movement
	transpose_m3(rest_mat);
	mul_m3_m3m3(joint_mat, rest_mat, pose_mat);		
	joints[0] = joints[1] = joints[2] = 0.f;
	// returns a 3 element list that gives corresponding joint
	int flag = 0;
	if (!(pchan->ikflag & BONE_IK_NO_XDOF))
		flag |= 1;
	if (!(pchan->ikflag & BONE_IK_NO_YDOF))
		flag |= 2;
	if (!(pchan->ikflag & BONE_IK_NO_ZDOF))
		flag |= 4;
	switch (flag) {
	case 0:	// fixed joint
		break;
	case 1:	// X only
		mat3_to_eulO( joints, EULER_ORDER_XYZ,joint_mat);
		joints[1] = joints[2] = 0.f;
		break;
	case 2:	// Y only
		mat3_to_eulO( joints, EULER_ORDER_XYZ,joint_mat);
		joints[0] = joints[2] = 0.f;
		break;
	case 3:	// X+Y
		mat3_to_eulO( joints, EULER_ORDER_ZYX,joint_mat);
		joints[2] = 0.f;
		break;
	case 4:	// Z only
		mat3_to_eulO( joints, EULER_ORDER_XYZ,joint_mat);
		joints[0] = joints[1] = 0.f;
		break;
	case 5:	// X+Z
		// decompose this as an equivalent rotation vector in X/Z plane
		joints[0] = joint_mat[1][2];
		joints[2] = -joint_mat[1][0];
		norm = normalize_v3(joints);
		if (norm < FLT_EPSILON) {
			norm = (joint_mat[1][1] < 0.f) ? M_PI : 0.f;
		} else {
			norm = acos(joint_mat[1][1]);
		}
		mul_v3_fl(joints, norm);
		break;
	case 6:	// Y+Z
		mat3_to_eulO( joints, EULER_ORDER_XYZ,joint_mat);
		joints[0] = 0.f;
		break;
	case 7: // X+Y+Z
		// equivalent axis
		joints[0] = (joint_mat[1][2]-joint_mat[2][1])*0.5f;
		joints[1] = (joint_mat[2][0]-joint_mat[0][2])*0.5f;
		joints[2] = (joint_mat[0][1]-joint_mat[1][0])*0.5f;
		sa = len_v3(joints);
		ca = (joint_mat[0][0]+joint_mat[1][1]+joint_mat[1][1]-1.0f)*0.5f;
		if (sa > FLT_EPSILON) {
			norm = atan2(sa,ca)/sa;
		} else {
		   if (ca < 0.0) {
			   norm = M_PI;
			   mul_v3_fl(joints,0.f);
			   if (joint_mat[0][0] > 0.f) {
				   joints[0] = 1.0f;
			   } else if (joint_mat[1][1] > 0.f) {
				   joints[1] = 1.0f;
			   } else {
				   joints[2] = 1.0f;
			   }
		   } else {
			   norm = 0.0;
		   }
		}
		mul_v3_fl(joints,norm);
		break;
	}
	return newVectorObject(joints, 3, Py_NEW, NULL);
}

int BL_ArmatureChannel::py_attr_set_joint_rotation(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	BL_ArmatureChannel* self= static_cast<BL_ArmatureChannel*>(self_v);
	bPoseChannel* pchan = self->m_posechannel;
	PyObject *item;
	float joints[3];
	float quat[4];

	if (!PySequence_Check(value) || PySequence_Size(value) != 3) {
		PyErr_SetString(PyExc_AttributeError, "expected a sequence of [3] floats");
		return PY_SET_ATTR_FAIL;
	}
	for (int i=0; i<3; i++) {
		item = PySequence_GetItem(value, i); /* new ref */
		joints[i] = PyFloat_AsDouble(item);
		Py_DECREF(item);
		if (joints[i] == -1.0f && PyErr_Occurred()) {
			PyErr_SetString(PyExc_AttributeError, "expected a sequence of [3] floats");
			return PY_SET_ATTR_FAIL;
		}
	}

	int flag = 0;
	if (!(pchan->ikflag & BONE_IK_NO_XDOF))
		flag |= 1;
	if (!(pchan->ikflag & BONE_IK_NO_YDOF))
		flag |= 2;
	if (!(pchan->ikflag & BONE_IK_NO_ZDOF))
		flag |= 4;
	unit_qt(quat);
	switch (flag) {
	case 0:	// fixed joint
		break;
	case 1:	// X only
		joints[1] = joints[2] = 0.f;
		eulO_to_quat( quat,joints, EULER_ORDER_XYZ);
		break;
	case 2:	// Y only
		joints[0] = joints[2] = 0.f;
		eulO_to_quat( quat,joints, EULER_ORDER_XYZ);
		break;
	case 3:	// X+Y
		joints[2] = 0.f;
		eulO_to_quat( quat,joints, EULER_ORDER_ZYX);
		break;
	case 4:	// Z only
		joints[0] = joints[1] = 0.f;
		eulO_to_quat( quat,joints, EULER_ORDER_XYZ);
		break;
	case 5:	// X+Z
		// X and Z are components of an equivalent rotation axis
		joints[1] = 0;
		axis_angle_to_quat( quat,joints, len_v3(joints));
		break;
	case 6:	// Y+Z
		joints[0] = 0.f;
		eulO_to_quat( quat,joints, EULER_ORDER_XYZ);
		break;
	case 7: // X+Y+Z
		// equivalent axis
		axis_angle_to_quat( quat,joints, len_v3(joints));
		break;
	}
	if (pchan->rotmode > 0) {
		quat_to_eulO( joints, pchan->rotmode,quat);
		copy_v3_v3(pchan->eul, joints);
	} else
		copy_qt_qt(pchan->quat, quat);
	return PY_SET_ATTR_SUCCESS;
}

// *************************
// BL_ArmatureBone
//
// Access to Bone structure
PyTypeObject BL_ArmatureBone::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"BL_ArmatureBone",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_bone_repr,
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&CValue::Type,
	0,0,0,0,0,0,
	py_base_new
};

// not used since this class is never instantiated
PyObject *BL_ArmatureBone::GetProxy() 
{ 
	return NULL; 
}
PyObject *BL_ArmatureBone::NewProxy(bool py_owns) 
{ 
	return NULL; 
}

PyObject *BL_ArmatureBone::py_bone_repr(PyObject *self)
{
	Bone* bone = static_cast<Bone*>BGE_PROXY_PTR(self);
	return PyUnicode_FromString(bone->name);
}

PyMethodDef BL_ArmatureBone::Methods[] = {
	{NULL,NULL} //Sentinel
};

/* no attributes on C++ class since it is never instantiated */
PyAttributeDef BL_ArmatureBone::Attributes[] = {
	{ NULL }	//Sentinel
};

// attributes that work on proxy ptr (points to a Bone structure)
PyAttributeDef BL_ArmatureBone::AttributesPtr[] = {
	KX_PYATTRIBUTE_CHAR_RO("name",Bone,name),
	KX_PYATTRIBUTE_FLAG_RO("connected",Bone,flag, BONE_CONNECTED),
	KX_PYATTRIBUTE_FLAG_RO("hinge",Bone,flag, BONE_HINGE),
	KX_PYATTRIBUTE_FLAG_NEGATIVE_RO("inherit_scale",Bone,flag, BONE_NO_SCALE),
	KX_PYATTRIBUTE_SHORT_RO("bbone_segments",Bone,segments),
	KX_PYATTRIBUTE_FLOAT_RO("roll",Bone,roll),
	KX_PYATTRIBUTE_FLOAT_VECTOR_RO("head",Bone,head,3),
	KX_PYATTRIBUTE_FLOAT_VECTOR_RO("tail",Bone,tail,3),
	KX_PYATTRIBUTE_FLOAT_RO("length",Bone,length),
	KX_PYATTRIBUTE_FLOAT_VECTOR_RO("arm_head",Bone,arm_head,3),
	KX_PYATTRIBUTE_FLOAT_VECTOR_RO("arm_tail",Bone,arm_tail,3),
	KX_PYATTRIBUTE_FLOAT_MATRIX_RO("arm_mat",Bone,arm_mat,4),
	KX_PYATTRIBUTE_FLOAT_MATRIX_RO("bone_mat",Bone,bone_mat,4),
	KX_PYATTRIBUTE_RO_FUNCTION("parent",BL_ArmatureBone,py_bone_get_parent),
	KX_PYATTRIBUTE_RO_FUNCTION("children",BL_ArmatureBone,py_bone_get_parent),
	{ NULL }	//Sentinel
};

PyObject *BL_ArmatureBone::py_bone_get_parent(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	Bone* bone = reinterpret_cast<Bone*>BGE_PROXY_PTR(self);
	if (bone->parent) {
		// create a proxy unconnected to any GE object
		return NewProxyPlus_Ext(NULL,&Type,bone->parent,false);
	}
	Py_RETURN_NONE;
}

PyObject *BL_ArmatureBone::py_bone_get_children(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	Bone* bone = reinterpret_cast<Bone*>BGE_PROXY_PTR(self);
	Bone* child;
	int count = 0;
	for (child=(Bone*)bone->childbase.first; child; child=(Bone*)child->next)
		count++;

	PyObject* childrenlist = PyList_New(count);

	for (count = 0, child=(Bone*)bone->childbase.first; child; child=(Bone*)child->next, ++count)
		PyList_SET_ITEM(childrenlist,count,NewProxyPlus_Ext(NULL,&Type,child,false));

	return childrenlist;
}

#endif // DISABLE_PYTHON
