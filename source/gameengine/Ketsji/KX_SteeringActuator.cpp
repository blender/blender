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

#include "BLI_math.h"
#include "KX_SteeringActuator.h"
#include "KX_GameObject.h"
#include "KX_NavMeshObject.h"
#include "KX_ObstacleSimulation.h"
#include "KX_PythonInit.h"


/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

KX_SteeringActuator::KX_SteeringActuator(SCA_IObject *gameobj, 
									int mode,
									KX_GameObject *target,
									KX_GameObject *navmesh,
									MT_Scalar distance,
									MT_Scalar velocity, 
									MT_Scalar acceleration,									
									MT_Scalar turnspeed,
									bool  isSelfTerminated,
									int pathUpdatePeriod,
									KX_ObstacleSimulation* simulation)	 : 
	SCA_IActuator(gameobj, KX_ACT_STEERING),
	m_mode(mode),
	m_target(target),
	m_distance(distance),
	m_velocity(velocity),
	m_acceleration(acceleration),
	m_turnspeed(turnspeed),
	m_isSelfTerminated(isSelfTerminated),
	m_pathUpdatePeriod(pathUpdatePeriod),
	m_updateTime(0),
	m_isActive(false),	
	m_simulation(simulation),	
	m_obstacle(NULL),
	m_pathLen(0),
	m_wayPointIdx(-1)
{
	m_navmesh = static_cast<KX_NavMeshObject*>(navmesh);
	if (m_navmesh)
		m_navmesh->RegisterActuator(this);
	if (m_target)
		m_target->RegisterActuator(this);
	
	if (m_simulation)
		m_obstacle = m_simulation->GetObstacle((KX_GameObject*)gameobj);
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
		// this object is being deleted, we cannot continue to use it.
		m_target = NULL;
		return true;
	}
	else if (clientobj == m_navmesh)
	{
		// this object is being deleted, we cannot continue to useit.
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

bool KX_SteeringActuator::Update(double curtime, bool frame)
{
	if (frame)
	{
		double delta =  curtime - m_updateTime;
		m_updateTime = curtime;
		
		if (m_posevent && !m_isActive)
		{
			delta = 0;
			m_pathUpdateTime = -1;
			m_updateTime = curtime;
			m_isActive = true;
		}
		bool bNegativeEvent = IsNegativeEvent();
		if (bNegativeEvent)
			m_isActive = false;

		RemoveAllEvents();

		if (!delta)
			return true;

		if (bNegativeEvent || !m_target)
			return false; // do nothing on negative events

		KX_GameObject *obj = (KX_GameObject*) GetParent();
		const MT_Point3& mypos = obj->NodeGetWorldPosition();
		const MT_Point3& targpos = m_target->NodeGetWorldPosition();
		MT_Vector3 vectotarg = targpos - mypos;
		MT_Vector3 steervec = MT_Vector3(0, 0, 0);
		bool apply_steerforce = false;
		bool terminate = true;

		switch (m_mode) {
			case KX_STEERING_SEEK:
				if (vectotarg.length2()>m_distance*m_distance)
				{
					terminate = false;
					steervec = vectotarg;
					steervec.normalize();
					apply_steerforce = true;
				}
				break;
			case KX_STEERING_FLEE:
				if (vectotarg.length2()<m_distance*m_distance)
				{
					terminate = false;
					steervec = -vectotarg;
					steervec.normalize();
					apply_steerforce = true;
				}
				break;
			case KX_STEERING_PATHFOLLOWING:
				if (m_navmesh && vectotarg.length2()>m_distance*m_distance)
				{
					terminate = false;

					static const MT_Scalar WAYPOINT_RADIUS(1.);

					if (m_pathUpdateTime<0 || (m_pathUpdatePeriod>=0 && 
												curtime - m_pathUpdateTime>((double)m_pathUpdatePeriod/1000)))
					{
						m_pathUpdateTime = curtime;
						m_pathLen = m_navmesh->FindPath(mypos, targpos, m_path, MAX_PATH_LENGTH);
						m_wayPointIdx = m_pathLen > 1 ? 1 : -1;
					}

					if (m_wayPointIdx>0)
					{
						MT_Vector3 waypoint(&m_path[3*m_wayPointIdx]);
						if ((waypoint-mypos).length2()<WAYPOINT_RADIUS*WAYPOINT_RADIUS)
						{
							m_wayPointIdx++;
							if (m_wayPointIdx>=m_pathLen)
							{
								m_wayPointIdx = -1;
								terminate = true;
							}
							else
								waypoint.setValue(&m_path[3*m_wayPointIdx]);
						}

						steervec = waypoint - mypos;
						apply_steerforce = true;

						//debug draw
						static const MT_Vector3 PATH_COLOR(1,0,0);
						m_navmesh->DrawPath(m_path, m_pathLen, PATH_COLOR);

					}	
					
				}
				break;
		}

		if (apply_steerforce)
		{
			bool isdyna = obj->IsDynamic();
			if (isdyna)
				steervec.z() = 0;
			if (!steervec.fuzzyZero())
				steervec.normalize();
			MT_Vector3 newvel = m_velocity*steervec;

			//adjust velocity to avoid obstacles
			if (m_simulation && m_obstacle && !newvel.fuzzyZero())
			{
				KX_RasterizerDrawDebugLine(mypos, mypos + newvel, MT_Vector3(1.,0.,0.));
				m_simulation->AdjustObstacleVelocity(m_obstacle, m_navmesh, newvel, 
								m_acceleration*delta, m_turnspeed/180.0f*M_PI*delta);
				KX_RasterizerDrawDebugLine(mypos, mypos + newvel, MT_Vector3(0.,1.,0.));
			}

			if (isdyna)
			{
				//temporary solution: set 2D steering velocity directly to obj
				//correct way is to apply physical force
				MT_Vector3 curvel = obj->GetLinearVelocity();
				newvel.z() = curvel.z();			
				obj->setLinearVelocity(newvel, false);
			}
			else
			{
				MT_Vector3 movement = delta*newvel;
				obj->ApplyMovement(movement, false);
			}
		}

		if (terminate && m_isSelfTerminated)
			return false;
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

