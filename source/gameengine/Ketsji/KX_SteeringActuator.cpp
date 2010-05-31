/**
* Add steering behaviors
*
* $Id$
*
* ***** BEGIN GPL LICENSE BLOCK *****
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version. The Blender
* Foundation also sells licenses for use in proprietary software under
* the Blender License.  See http://www.blender.org/BL/ for information
* about this.
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

#include "KX_SteeringActuator.h"
#include "KX_GameObject.h"
#include "KX_NavMeshObject.h"

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

KX_SteeringActuator::KX_SteeringActuator(SCA_IObject *gameobj, 
									int mode,
									KX_GameObject *target,
									KX_GameObject *navmesh,
									MT_Scalar movement, 
									MT_Scalar distance)	 : 
	SCA_IActuator(gameobj, KX_ACT_STEERING),
	m_mode(mode),
	m_target(target),
	m_movement(movement),
	m_distance(distance)
{
	m_navmesh = static_cast<KX_NavMeshObject*>(navmesh);
	if (m_navmesh)
		m_navmesh->RegisterActuator(this);
	if (m_target)
		m_target->RegisterActuator(this);
} 

KX_SteeringActuator::~KX_SteeringActuator()
{
	if (m_navmesh)
		m_navmesh->UnregisterActuator(this);
	if (m_target)
		m_target->UnregisterActuator(this);
} 

CValue* KX_SteeringActuator::GetReplica()
{
	KX_SteeringActuator* replica = new KX_SteeringActuator(*this);
	// replication just copy the m_base pointer => common random generator
	replica->ProcessReplica();
	return replica;
}

void KX_SteeringActuator::ProcessReplica()
{
	if (m_target)
		m_target->RegisterActuator(this);
	if (m_navmesh)
		m_navmesh->RegisterActuator(this);
	SCA_IActuator::ProcessReplica();
}


bool KX_SteeringActuator::UnlinkObject(SCA_IObject* clientobj)
{
	if (clientobj == m_target)
	{
		// this object is being deleted, we cannot continue to track it.
		m_target = NULL;
		return true;
	}
	else if (clientobj == m_navmesh)
	{
		// this object is being deleted, we cannot continue to track it.
		m_navmesh = NULL;
		return true;
	}
	return false;
}

void KX_SteeringActuator::Relink(GEN_Map<GEN_HashedPtr, void*> *obj_map)
{
	void **h_obj = (*obj_map)[m_target];
	if (h_obj) {
		if (m_target)
			m_target->UnregisterActuator(this);
		m_target = (KX_GameObject*)(*h_obj);
		m_target->RegisterActuator(this);
	}

	h_obj = (*obj_map)[m_navmesh];
	if (h_obj) {
		if (m_navmesh)
			m_navmesh->UnregisterActuator(this);
		m_navmesh = (KX_NavMeshObject*)(*h_obj);
		m_navmesh->RegisterActuator(this);
	}
}

bool KX_SteeringActuator::Update()
{
	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();

	if (bNegativeEvent)
		return false; // do nothing on negative events

	KX_GameObject *obj = (KX_GameObject*) GetParent();
	const MT_Point3& mypos = obj->NodeGetWorldPosition();
	const MT_Point3& targpos = m_target->NodeGetWorldPosition();
	MT_Vector3 vectotarg = targpos - mypos;
	MT_Vector3 steervec = MT_Vector3(0, 0, 0);
	bool apply_steerforce = false;

	switch (m_mode) {
		case KX_STEERING_SEEK:
			if (vectotarg.length2()>m_distance*m_distance)
			{
				apply_steerforce = true;
				steervec = vectotarg;
				steervec.normalize();
			}
			break;
		case KX_STEERING_FLEE:
			if (vectotarg.length2()<m_distance*m_distance)
			{
				apply_steerforce = true;
				steervec = -vectotarg;
				steervec.normalize();
			}
		case KX_STEERING_PATHFOLLOWING:
			if (m_navmesh && vectotarg.length2()>m_distance*m_distance)
			{
				static const int MAX_PATH_LENGTH  = 128;
				static const MT_Vector3 PATH_COLOR(1,0,0);

				float path[MAX_PATH_LENGTH*3];
				int pathlen = m_navmesh->FindPath(mypos, targpos, path, MAX_PATH_LENGTH);
				if (pathlen > 1)
				{
					//debug draw
					m_navmesh->DrawPath(path, pathlen, PATH_COLOR);

					apply_steerforce = true;
					MT_Vector3 waypoint(&path[3]);
					steervec = waypoint - mypos;
					steervec.z() = 0;
					steervec.normalize();
				}
			}
			break;
	}

	if (apply_steerforce)
	{
		MT_Vector3 vel = m_movement*steervec;
		obj->ApplyMovement(vel, false);
	}

	return true;
}

#ifndef DISABLE_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_SteeringActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_SteeringActuator",
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

PyMethodDef KX_SteeringActuator::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_SteeringActuator::Attributes[] = {
	KX_PYATTRIBUTE_RW_FUNCTION("target", KX_SteeringActuator, pyattr_get_target, pyattr_set_target),
	{ NULL }	//Sentinel
};

PyObject* KX_SteeringActuator::pyattr_get_target(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_SteeringActuator* actuator = static_cast<KX_SteeringActuator*>(self);
	if (!actuator->m_target)	
		Py_RETURN_NONE;
	else
		return actuator->m_target->GetProxy();
}

int KX_SteeringActuator::pyattr_set_target(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_SteeringActuator* actuator = static_cast<KX_SteeringActuator*>(self);
	KX_GameObject *gameobj;

	if (!ConvertPythonToGameObject(value, &gameobj, true, "actuator.object = value: KX_SteeringActuator"))
		return PY_SET_ATTR_FAIL; // ConvertPythonToGameObject sets the error

	if (actuator->m_target != NULL)
		actuator->m_target->UnregisterActuator(actuator);	

	actuator->m_target = (KX_GameObject*) gameobj;

	if (actuator->m_target)
		actuator->m_target->RegisterActuator(actuator);

	return PY_SET_ATTR_SUCCESS;
}

#endif // DISABLE_PYTHON

/* eof */

