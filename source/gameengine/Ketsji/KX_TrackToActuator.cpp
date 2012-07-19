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

/** \file gameengine/Ketsji/KX_TrackToActuator.cpp
 *  \ingroup ketsji
 *
 * Replace the mesh for this actuator's parent
 */

/* todo: not all trackflags / upflags are implemented/tested !
 * m_trackflag is used to determine the forward tracking direction
 * m_upflag for the up direction
 * normal situation is +y for forward, +z for up */

#include "MT_Scalar.h"
#include "SCA_IActuator.h"
#include "KX_TrackToActuator.h"
#include "SCA_IScene.h"
#include "SCA_LogicManager.h"
#include <math.h>
#include <iostream>
#include "KX_GameObject.h"

#include "PyObjectPlus.h"

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

KX_TrackToActuator::KX_TrackToActuator(SCA_IObject *gameobj, 
                                       SCA_IObject *ob,
                                       int time,
                                       bool allow3D,
                                       int trackflag,
                                       int upflag)
    : SCA_IActuator(gameobj, KX_ACT_TRACKTO)
{
	m_time = time;
	m_allow3D = allow3D;
	m_object = ob;
	m_trackflag = trackflag;
	m_upflag = upflag;
	m_parentobj = 0;
	
	if (m_object)
		m_object->RegisterActuator(this);

	{
		// if the object is vertex parented, don't check parent orientation as the link is broken
		if (!((KX_GameObject*)gameobj)->IsVertexParent()) {
			m_parentobj = ((KX_GameObject*)gameobj)->GetParent(); // check if the object is parented 
			if (m_parentobj) {  
				// if so, store the initial local rotation
				// this is needed to revert the effect of the parent inverse node (TBC)
				m_parentlocalmat = m_parentobj->GetSGNode()->GetLocalOrientation();
				// use registration mechanism rather than AddRef, it creates zombie objects
				m_parentobj->RegisterActuator(this);
				// GetParent did AddRef, undo here
				m_parentobj->Release();
			}
		}
	}

} /* End of constructor */



/* old function from Blender */
MT_Matrix3x3 EulToMat3(float *eul)
{
	MT_Matrix3x3 mat;
	float ci, cj, ch, si, sj, sh, cc, cs, sc, ss;
	
	ci = cos(eul[0]); 
	cj = cos(eul[1]); 
	ch = cos(eul[2]);
	si = sin(eul[0]); 
	sj = sin(eul[1]); 
	sh = sin(eul[2]);
	cc = ci*ch; 
	cs = ci*sh; 
	sc = si*ch; 
	ss = si*sh;

	mat[0][0] = cj*ch; 
	mat[1][0] = sj*sc-cs; 
	mat[2][0] = sj*cc+ss;
	mat[0][1] = cj*sh; 
	mat[1][1] = sj*ss+cc; 
	mat[2][1] = sj*cs-sc;
	mat[0][2] = -sj;	 
	mat[1][2] = cj*si;    
	mat[2][2] = cj*ci;

	return mat;
}



/* old function from Blender */
void Mat3ToEulOld(MT_Matrix3x3 mat, float eul[3])
{
	const float cy = sqrtf(mat[0][0] * mat[0][0] + mat[0][1] * mat[0][1]);

	if (cy > (float)(16.0f * FLT_EPSILON)) {
		eul[0] = atan2f( mat[1][2], mat[2][2]);
		eul[1] = atan2f(-mat[0][2], cy);
		eul[2] = atan2f( mat[0][1], mat[0][0]);
	}
	else {
		eul[0] = atan2f(-mat[2][1], mat[1][1]);
		eul[1] = atan2f(-mat[0][2], cy);
		eul[2] = 0.0;
	}
}



/* old function from Blender */
void compatible_eulFast(float *eul, float *oldrot)
{
	float dx, dy, dz;
	
	/* angular difference of 360 degrees */

	dx = eul[0] - oldrot[0];
	dy = eul[1] - oldrot[1];
	dz = eul[2] - oldrot[2];

	if (fabsf(dx) > (float)MT_PI) {
		if (dx > 0.0f) eul[0] -= (float)MT_2_PI; else eul[0] += (float)MT_2_PI;
	}
	if (fabsf(dy) > (float)MT_PI) {
		if (dy > 0.0f) eul[1] -= (float)MT_2_PI; else eul[1] += (float)MT_2_PI;
	}
	if (fabsf(dz) > (float)MT_PI) {
		if (dz > 0.0f) eul[2] -= (float)MT_2_PI; else eul[2] += (float)MT_2_PI;
	}
}



MT_Matrix3x3 matrix3x3_interpol(MT_Matrix3x3 oldmat, MT_Matrix3x3 mat, int m_time)
{
	float eul[3], oldeul[3];	

	Mat3ToEulOld(oldmat, oldeul);
	Mat3ToEulOld(mat, eul);
	compatible_eulFast(eul, oldeul);
	
	eul[0] = (m_time * oldeul[0] + eul[0]) / (1.0f + m_time);
	eul[1] = (m_time * oldeul[1] + eul[1]) / (1.0f + m_time);
	eul[2] = (m_time * oldeul[2] + eul[2]) / (1.0f + m_time);
	
	return EulToMat3(eul);
}



KX_TrackToActuator::~KX_TrackToActuator()
{
	if (m_object)
		m_object->UnregisterActuator(this);
	if (m_parentobj)
		m_parentobj->UnregisterActuator(this);
} /* end of destructor */

void KX_TrackToActuator::ProcessReplica()
{
	// the replica is tracking the same object => register it
	if (m_object)
		m_object->RegisterActuator(this);
	if (m_parentobj)
		m_parentobj->RegisterActuator(this);
	SCA_IActuator::ProcessReplica();
}


bool KX_TrackToActuator::UnlinkObject(SCA_IObject* clientobj)
{
	if (clientobj == m_object)
	{
		// this object is being deleted, we cannot continue to track it.
		m_object = NULL;
		return true;
	}
	if (clientobj == m_parentobj)
	{
		m_parentobj = NULL;
		return true;
	}
	return false;
}

void KX_TrackToActuator::Relink(CTR_Map<CTR_HashedPtr, void*> *obj_map)
{
	void **h_obj = (*obj_map)[m_object];
	if (h_obj) {
		if (m_object)
			m_object->UnregisterActuator(this);
		m_object = (SCA_IObject*)(*h_obj);
		m_object->RegisterActuator(this);
	}

	void **h_parobj = (*obj_map)[m_parentobj];
	if (h_parobj) {
		if (m_parentobj)
			m_parentobj->UnregisterActuator(this);
		m_parentobj= (KX_GameObject*)(*h_parobj);
		m_parentobj->RegisterActuator(this);
	}
}


bool KX_TrackToActuator::Update(double curtime, bool frame)
{
	bool result = false;	
	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();

	if (bNegativeEvent)
	{
		// do nothing on negative events
	}
	else if (m_object)
	{
		KX_GameObject* curobj = (KX_GameObject*) GetParent();
		MT_Vector3 dir = ((KX_GameObject*)m_object)->NodeGetWorldPosition() - curobj->NodeGetWorldPosition();
		if (dir.length2())
			dir.normalize();
		MT_Vector3 up(0,0,1);
		
		
#ifdef DSADSA
		switch (m_upflag)
		{
		case 0:
			{
				up.setValue(1.0,0,0);
				break;
			} 
		case 1:
			{
				up.setValue(0,1.0,0);
				break;
			}
		case 2:
		default:
			{
				up.setValue(0,0,1.0);
			}
		}
#endif 
		if (m_allow3D)
		{
			up = (up - up.dot(dir) * dir).safe_normalized();
			
		}
		else
		{
			dir = (dir - up.dot(dir)*up).safe_normalized();
		}
		
		MT_Vector3 left;
		MT_Matrix3x3 mat;
		
		switch (m_trackflag)
		{
		case 0: // TRACK X
			{
				// (1.0 , 0.0 , 0.0 ) x direction is forward, z (0.0 , 0.0 , 1.0 ) up
				left  = dir.safe_normalized();
				dir = (left.cross(up)).safe_normalized();
				mat.setValue (
					left[0], dir[0],up[0], 
					left[1], dir[1],up[1],
					left[2], dir[2],up[2]
					);
				
				break;
			};
		case 1:	// TRACK Y
			{
				// (0.0 , 1.0 , 0.0 ) y direction is forward, z (0.0 , 0.0 , 1.0 ) up
				left  = (dir.cross(up)).safe_normalized();
				mat.setValue (
					left[0], dir[0],up[0], 
					left[1], dir[1],up[1],
					left[2], dir[2],up[2]
					);
				
				break;
			}
			
		case 2: // track Z
			{
				left = up.safe_normalized();
				up = dir.safe_normalized();
				dir = left;
				left  = (dir.cross(up)).safe_normalized();
				mat.setValue (
					left[0], dir[0],up[0], 
					left[1], dir[1],up[1],
					left[2], dir[2],up[2]
					);
				break;
			}
			
		case 3: // TRACK -X
			{
				// (1.0 , 0.0 , 0.0 ) x direction is forward, z (0.0 , 0.0 , 1.0 ) up
				left  = -dir.safe_normalized();
				dir = -(left.cross(up)).safe_normalized();
				mat.setValue (
					left[0], dir[0],up[0], 
					left[1], dir[1],up[1],
					left[2], dir[2],up[2]
					);
				
				break;
			};
		case 4: // TRACK -Y
			{
				// (0.0 , -1.0 , 0.0 ) -y direction is forward, z (0.0 , 0.0 , 1.0 ) up
				left  = (-dir.cross(up)).safe_normalized();
				mat.setValue (
					left[0], -dir[0],up[0], 
					left[1], -dir[1],up[1],
					left[2], -dir[2],up[2]
					);
				break;
			}
		case 5: // track -Z
			{
				left = up.safe_normalized();
				up = -dir.safe_normalized();
				dir = left;
				left  = (dir.cross(up)).safe_normalized();
				mat.setValue (
					left[0], dir[0],up[0], 
					left[1], dir[1],up[1],
					left[2], dir[2],up[2]
					);
				
				break;
			}
			
		default:
			{
				// (1.0 , 0.0 , 0.0 ) -x direction is forward, z (0.0 , 0.0 , 1.0 ) up
				left  = -dir.safe_normalized();
				dir = -(left.cross(up)).safe_normalized();
				mat.setValue (
					left[0], dir[0],up[0], 
					left[1], dir[1],up[1],
					left[2], dir[2],up[2]
					);
			}
		}
		
		MT_Matrix3x3 oldmat;
		oldmat= curobj->NodeGetWorldOrientation();
		
		/* erwin should rewrite this! */
		mat= matrix3x3_interpol(oldmat, mat, m_time);
		

		if (m_parentobj) { // check if the model is parented and calculate the child transform
				
			MT_Point3 localpos;
			localpos = curobj->GetSGNode()->GetLocalPosition();
			// Get the inverse of the parent matrix
			MT_Matrix3x3 parentmatinv;
			parentmatinv = m_parentobj->NodeGetWorldOrientation ().inverse ();				
			// transform the local coordinate system into the parents system
			mat = parentmatinv * mat;
			// append the initial parent local rotation matrix
			mat = m_parentlocalmat * mat;

			// set the models tranformation properties
			curobj->NodeSetLocalOrientation(mat);
			curobj->NodeSetLocalPosition(localpos);
			//curobj->UpdateTransform();
		}
		else
		{
			curobj->NodeSetLocalOrientation(mat);
		}

		result = true;
	}

	return result;
}

#ifdef WITH_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_TrackToActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_TrackToActuator",
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

PyMethodDef KX_TrackToActuator::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_TrackToActuator::Attributes[] = {
	KX_PYATTRIBUTE_INT_RW("time",0,1000,true,KX_TrackToActuator,m_time),
	KX_PYATTRIBUTE_BOOL_RW("use3D",KX_TrackToActuator,m_allow3D),
	KX_PYATTRIBUTE_RW_FUNCTION("object", KX_TrackToActuator, pyattr_get_object, pyattr_set_object),

	{ NULL }	//Sentinel
};

PyObject* KX_TrackToActuator::pyattr_get_object(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_TrackToActuator* actuator = static_cast<KX_TrackToActuator*>(self);
	if (!actuator->m_object)	
		Py_RETURN_NONE;
	else
		return actuator->m_object->GetProxy();
}

int KX_TrackToActuator::pyattr_set_object(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_TrackToActuator* actuator = static_cast<KX_TrackToActuator*>(self);
	KX_GameObject *gameobj;
		
	if (!ConvertPythonToGameObject(value, &gameobj, true, "actuator.object = value: KX_TrackToActuator"))
		return PY_SET_ATTR_FAIL; // ConvertPythonToGameObject sets the error
		
	if (actuator->m_object != NULL)
		actuator->m_object->UnregisterActuator(actuator);	

	actuator->m_object = (SCA_IObject*) gameobj;
		
	if (actuator->m_object)
		actuator->m_object->RegisterActuator(actuator);
		
	return PY_SET_ATTR_SUCCESS;
}

#endif // WITH_PYTHON

/* eof */
