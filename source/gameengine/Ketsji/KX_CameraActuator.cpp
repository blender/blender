/*
 * KX_CameraActuator.cpp
 *
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
 *
 */

/** \file gameengine/Ketsji/KX_CameraActuator.cpp
 *  \ingroup ketsji
 */

#include "BLI_math_vector.h"

#include "KX_CameraActuator.h"
#include <math.h>
#include <float.h>
#include "KX_GameObject.h"

#include "EXP_PyObjectPlus.h" 

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

KX_CameraActuator::KX_CameraActuator(
	SCA_IObject* gameobj, 
	SCA_IObject *obj,
	float hght,
	float minhght,
	float maxhght,
	short axis,
	float damping
): 
	SCA_IActuator(gameobj, KX_ACT_CAMERA),
	m_ob (obj),
	m_height (hght),
	m_minHeight (minhght),
	m_maxHeight (maxhght),
	m_axis(axis),
	m_damping (damping)
{
	if (m_ob)
		m_ob->RegisterActuator(this);
}

KX_CameraActuator::~KX_CameraActuator()
{
	if (m_ob)
		m_ob->UnregisterActuator(this);
}

	CValue* 
KX_CameraActuator::
GetReplica(
) {
	KX_CameraActuator* replica = new KX_CameraActuator(*this);
	replica->ProcessReplica();
	return replica;
};

void KX_CameraActuator::ProcessReplica()
{
	if (m_ob)
		m_ob->RegisterActuator(this);
	SCA_IActuator::ProcessReplica();
}

bool KX_CameraActuator::UnlinkObject(SCA_IObject* clientobj)
{
	if (clientobj == m_ob)
	{
		// this object is being deleted, we cannot continue to track it.
		m_ob = NULL;
		return true;
	}
	return false;
}


void KX_CameraActuator::Relink(CTR_Map<CTR_HashedPtr, void*> *obj_map)
{
	void **h_obj = (*obj_map)[m_ob];
	if (h_obj) {
		if (m_ob)
			m_ob->UnregisterActuator(this);
		m_ob = (SCA_IObject*)(*h_obj);
		m_ob->RegisterActuator(this);
	}
}

/* copied from blender BLI_math ... don't know if there's an equivalent */

static void Kx_VecUpMat3(float vec[3], float mat[3][3], short axis)
{

	// Construct a camera matrix s.t. the specified axis

	// maps to the given vector (*vec). Also defines the rotation

	// about this axis by mapping one of the other axis to the y-axis.


	float inp;
	short cox = 0, coy = 0, coz = 0;
	
	/* up range has no meaning, is not really up!
	 * see: VecUpMat3old
	 */

	if (axis==0) {
		cox= 0; coy= 1; coz= 2;		/* Y up Z tr */
	}
	if (axis==1) {
		cox= 1; coy= 2; coz= 0;		/* Z up X tr */
	}
	if (axis==2) {
		cox= 2; coy= 0; coz= 1;		/* X up Y tr */
	}
	if (axis==3) {
		cox= 0; coy= 1; coz= 2;		/* Y op -Z tr */
		vec[0] = -vec[0];
		vec[1] = -vec[1];
		vec[2] = -vec[2];
	}
	if (axis==4) {
		cox= 1; coy= 0; coz= 2;		/*  */
	}
	if (axis==5) {
		cox= 2; coy= 1; coz= 0;		/* Y up X tr */
	}

	mat[coz][0] = vec[0];
	mat[coz][1] = vec[1];
	mat[coz][2] = vec[2];
	if (normalize_v3((float *)mat[coz]) == 0.f) {
		/* this is a very abnormal situation: the camera has reach the object center exactly
		 * We will choose a completely arbitrary direction */
		mat[coz][0] = 1.0f;
		mat[coz][1] = 0.0f;
		mat[coz][2] = 0.0f;
	}
	
	inp = mat[coz][2];
	mat[coy][0] =      - inp * mat[coz][0];
	mat[coy][1] =      - inp * mat[coz][1];
	mat[coy][2] = 1.0f - inp * mat[coz][2];

	if (normalize_v3((float *)mat[coy]) == 0.f) {
		/* the camera is vertical, chose the y axis arbitrary */
		mat[coy][0] = 0.f;
		mat[coy][1] = 1.f;
		mat[coy][2] = 0.f;
	}
	
	cross_v3_v3v3(mat[cox], mat[coy], mat[coz]);
}

bool KX_CameraActuator::Update(double curtime, bool frame)
{
	/* wondering... is it really necessary/desirable to suppress negative    */
	/* events here?                                                          */
	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();

	if (bNegativeEvent || !m_ob) 
		return false;
	
	KX_GameObject *obj = (KX_GameObject*) GetParent();
	MT_Point3 from = obj->NodeGetWorldPosition();
	MT_Matrix3x3 frommat = obj->NodeGetWorldOrientation();
	/* These casts are _very_ dangerous!!! */
	MT_Point3 lookat = ((KX_GameObject*)m_ob)->NodeGetWorldPosition();
	MT_Matrix3x3 actormat = ((KX_GameObject*)m_ob)->NodeGetWorldOrientation();

	float fp1[3]={0}, fp2[3]={0}, rc[3];
	float inp, fac; //, factor = 0.0; /* some factor...                                    */
	float mindistsq, maxdistsq, distsq;
	float mat[3][3];
	
	/* The rules:                                                            */
	/* CONSTRAINT 1: not implemented */
	/* CONSTRAINT 2: can camera see actor?              */
	/* CONSTRAINT 3: fixed height relative to floor below actor.             */
	/* CONSTRAINT 4: camera rotates behind actor                              */
	/* CONSTRAINT 5: minimum / maximum distance                             */
	/* CONSTRAINT 6: again: fixed height relative to floor below actor        */
	/* CONSTRAINT 7: track to floor below actor                               */
	/* CONSTRAINT 8: look a little bit left or right, depending on how the
	 *
	 * character is looking (horizontal x)
	 */

	/* ...and then set the camera position. Since we assume the parent of    */
	/* this actuator is always a camera, just set the parent position and    */
	/* rotation. We do not check whether we really have a camera as parent.  */
	/* It may be better to turn this into a general tracking actuator later  */
	/* on, since lots of plausible relations can be filled in here.          */

	/* ... set up some parameters ...                                        */
	/* missing here: the 'floorloc' of the actor's shadow */

	mindistsq= m_minHeight*m_minHeight;
	maxdistsq= m_maxHeight*m_maxHeight;

	/* C1: not checked... is a future option                                 */

	/* C2: blender test_visibility function. Can this be a ray-test?         */

	/* C3: fixed height  */
	from[2] = (15.0f * from[2] + lookat[2] + m_height) / 16.0f;


	/* C4: camera behind actor   */
	switch (m_axis) {
		case OB_POSX:
			/* X */
			fp1[0] = actormat[0][0];
			fp1[1] = actormat[1][0];
			fp1[2] = actormat[2][0];

			fp2[0] = frommat[0][0];
			fp2[1] = frommat[1][0];
			fp2[2] = frommat[2][0];
			break;
		case OB_POSY:
			/* Y */
			fp1[0] = actormat[0][1];
			fp1[1] = actormat[1][1];
			fp1[2] = actormat[2][1];

			fp2[0] = frommat[0][1];
			fp2[1] = frommat[1][1];
			fp2[2] = frommat[2][1];
			break;
		case OB_NEGX:
			/* -X */
			fp1[0] = -actormat[0][0];
			fp1[1] = -actormat[1][0];
			fp1[2] = -actormat[2][0];

			fp2[0] = frommat[0][0];
			fp2[1] = frommat[1][0];
			fp2[2] = frommat[2][0];
			break;
		case OB_NEGY:
			/* -Y */
			fp1[0] = -actormat[0][1];
			fp1[1] = -actormat[1][1];
			fp1[2] = -actormat[2][1];

			fp2[0] = frommat[0][1];
			fp2[1] = frommat[1][1];
			fp2[2] = frommat[2][1];
			break;
		default:
			assert(0);
			break;
	}

	inp = fp1[0]*fp2[0] + fp1[1]*fp2[1] + fp1[2]*fp2[2];
	fac = (-1.0f + inp) * m_damping;

	from[0] += fac * fp1[0];
	from[1] += fac * fp1[1];
	from[2] += fac * fp1[2];
	
	/* only for it lies: cross test and perpendicular bites up */
	if (inp < 0.0f) {
		/* Don't do anything if the cross product is too small.
		 * The camera up-axis becomes unstable and starts to oscillate.
		 * The 0.01f threshold is arbitrary but seems to work well in practice. */
		float cross = fp1[0] * fp2[1] - fp1[1] * fp2[0];
		if (cross > 0.01f) {
			from[0] -= fac * fp1[1];
			from[1] += fac * fp1[0];
		}
		else if (cross < -0.01f) {
			from[0] += fac * fp1[1];
			from[1] -= fac * fp1[0];
		}
	}

	/* CONSTRAINT 5: minimum / maximum distance */

	rc[0] = (lookat[0]-from[0]);
	rc[1] = (lookat[1]-from[1]);
	rc[2] = (lookat[2]-from[2]);
	distsq = rc[0]*rc[0] + rc[1]*rc[1] + rc[2]*rc[2];

	if (distsq > maxdistsq) {
		distsq = 0.15f * (distsq - maxdistsq) / distsq;
		
		from[0] += distsq*rc[0];
		from[1] += distsq*rc[1];
		from[2] += distsq*rc[2];
	}
	else if (distsq < mindistsq) {
		distsq = 0.15f * (mindistsq - distsq) / mindistsq;
		
		from[0] -= distsq*rc[0];
		from[1] -= distsq*rc[1];
		from[2] -= distsq*rc[2];
	}


	/* CONSTRAINT 7: track to floor below actor */
	rc[0] = (lookat[0]-from[0]);
	rc[1] = (lookat[1]-from[1]);
	rc[2] = (lookat[2]-from[2]);
	Kx_VecUpMat3(rc, mat, 3);	/* y up Track -z */
	



	/* now set the camera position and rotation */
	
	obj->NodeSetLocalPosition(from);
	
	actormat[0][0] = mat[0][0]; actormat[0][1] = mat[1][0]; actormat[0][2] = mat[2][0];
	actormat[1][0] = mat[0][1]; actormat[1][1] = mat[1][1]; actormat[1][2] = mat[2][1];
	actormat[2][0] = mat[0][2]; actormat[2][1] = mat[1][2]; actormat[2][2] = mat[2][2];
	obj->NodeSetLocalOrientation(actormat);

	return true;
}

CValue *KX_CameraActuator::findObject(const char *obName)
{
	/* hook to object system */
	return NULL;
}

#ifdef WITH_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_CameraActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_CameraActuator",
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

PyMethodDef KX_CameraActuator::Methods[] = {
	{NULL, NULL} //Sentinel
};

PyAttributeDef KX_CameraActuator::Attributes[] = {
	KX_PYATTRIBUTE_FLOAT_RW("min",-FLT_MAX,FLT_MAX,KX_CameraActuator,m_minHeight),
	KX_PYATTRIBUTE_FLOAT_RW("max",-FLT_MAX,FLT_MAX,KX_CameraActuator,m_maxHeight),
	KX_PYATTRIBUTE_FLOAT_RW("height",-FLT_MAX,FLT_MAX,KX_CameraActuator,m_height),
	KX_PYATTRIBUTE_SHORT_RW("axis", 0, 5, true, KX_CameraActuator, m_axis),
	KX_PYATTRIBUTE_RW_FUNCTION("object", KX_CameraActuator, pyattr_get_object, pyattr_set_object),
	KX_PYATTRIBUTE_FLOAT_RW("damping",0.f,10.f,KX_CameraActuator,m_damping),
	{NULL}
};

PyObject *KX_CameraActuator::pyattr_get_object(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_CameraActuator* self = static_cast<KX_CameraActuator*>(self_v);
	if (self->m_ob==NULL)
		Py_RETURN_NONE;
	else
		return self->m_ob->GetProxy();
}

int KX_CameraActuator::pyattr_set_object(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_CameraActuator* self = static_cast<KX_CameraActuator*>(self_v);
	KX_GameObject *gameobj;
	
	if (!ConvertPythonToGameObject(self->GetLogicManager(), value, &gameobj, true, "actuator.object = value: KX_CameraActuator"))
		return PY_SET_ATTR_FAIL; // ConvertPythonToGameObject sets the error
	
	if (self->m_ob)
		self->m_ob->UnregisterActuator(self);

	if ((self->m_ob = (SCA_IObject*)gameobj))
		self->m_ob->RegisterActuator(self);
	
	return PY_SET_ATTR_SUCCESS;
}

#endif // WITH_PYTHON

/* eof */
