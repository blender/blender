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

/** \file BL_ArmatureChannel.h
 *  \ingroup bgeconv
 */

#ifndef __BL_ARMATURECHANNEL_H__
#define __BL_ARMATURECHANNEL_H__

#include "DNA_action_types.h"
#include "CTR_HashedPtr.h"
#include "CTR_Map.h"
#include "PyObjectPlus.h"

class SCA_IObject;
class KX_GameObject;
class BL_ArmatureObject;
struct bConstraint;
struct bPoseChannel;
struct Object;
struct bPose;

class BL_ArmatureChannel : public PyObjectPlus
{
	// use Py_HeaderPtr since we use generic pointer in proxy
	Py_HeaderPtr;

private:
	friend class BL_ArmatureObject;
	struct bPoseChannel* m_posechannel;
	BL_ArmatureObject* m_armature;

public:
	BL_ArmatureChannel(class BL_ArmatureObject *armature, 
						struct bPoseChannel *posechannel);
	virtual ~BL_ArmatureChannel();

#ifdef WITH_PYTHON
	// Python access
	virtual PyObject* py_repr(void);

	static PyObject* py_attr_getattr(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static int py_attr_setattr(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static PyObject* py_attr_get_joint_rotation(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static int py_attr_set_joint_rotation(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
#endif // WITH_PYTHON
};

/* this is a factory class to access bBone data field in the GE.
 * It's not supposed to be instantiated, we only need it for the Attributes and Method array.
 * The actual proxy object will be manually created using NewProxyPtr */
class BL_ArmatureBone : public PyObjectPlus
{
	// use Py_HeaderPtr since we use generic pointer in proxy
	Py_HeaderPtr;
private:
	// make constructor private to make sure no one tries to instantiate this class
	BL_ArmatureBone() {}
	virtual ~BL_ArmatureBone() {}

public:

#ifdef WITH_PYTHON
	static PyObject *py_bone_repr(PyObject *self);
	static PyObject *py_bone_get_parent(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject *py_bone_get_children(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
#endif

};


#endif //__BL_ARMATURECHANNEL_H__

