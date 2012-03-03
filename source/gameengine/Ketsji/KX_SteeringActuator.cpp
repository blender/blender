/*
 * Add steering behaviors
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
 */

#include "BLI_math.h"
#include "KX_SteeringActuator.h"
#include "KX_GameObject.h"
#include "KX_NavMeshObject.h"
#include "KX_ObstacleSimulation.h"
#include "KX_PythonInit.h"
#include "KX_PyMath.h"
#include "Recast.h"

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

KX_SteeringActuator::KX_SteeringActuator(SCA_IObject *gameobj, 
                                         int mode,
                                         KX_GameObject *target,
                                         KX_GameObject *navmesh,
                                         float distance,
                                         float velocity,
                                         float acceleration,
                                         float turnspeed,
                                         bool  isSelfTerminated,
                                         int pathUpdatePeriod,
                                         KX_ObstacleSimulation* simulation,
                                         short facingmode,
                                         bool normalup,
                                         bool enableVisualization)
    : SCA_IActuator(gameobj, KX_ACT_STEERING),
      m_target(target),
      m_mode(mode),
      m_distance(distance),
      m_velocity(velocity),
      m_acceleration(acceleration),
      m_turnspeed(turnspeed),
      m_simulation(simulation),
      m_updateTime(0),
      m_obstacle(NULL),
      m_isActive(false),
      m_isSelfTerminated(isSelfTerminated),
      m_enableVisualization(enableVisualization),
      m_facingMode(facingmode),
      m_normalUp(normalup),
      m_pathLen(0),
      m_pathUpdatePeriod(pathUpdatePeriod),
      m_wayPointIdx(-1),
      m_steerVec(MT_Vector3(0, 0, 0))
{
	m_navmesh = static_cast<KX_NavMeshObject*>(navmesh);
	if (m_navmesh)
		m_navmesh->RegisterActuator(this);
	if (m_target)
		m_target->RegisterActuator(this);
	
	if (m_simulation)
		m_obstacle = m_simulation->GetObstacle((KX_GameObject*)gameobj);
	KX_GameObject* parent = ((KX_GameObject*)gameobj)->GetParent();
	if (m_facingMode>0 && parent)
	{
		m_parentlocalmat = parent->GetSGNode()->GetLocalOrientation();
	}
	else
		m_parentlocalmat.setIdentity();
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
		m_target = NULL;
		return true;
	}
	else if (clientobj == m_navmesh)
	{
		m_navmesh = NULL;
		return true;
	}
	return false;
}

void KX_SteeringActuator::Relink(CTR_Map<CTR_HashedPtr, void*> *obj_map)
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
		MT_Vector3 vectotarg2d = vectotarg;
		vectotarg2d.z() = 0;
		m_steerVec = MT_Vector3(0, 0, 0);
		bool apply_steerforce = false;
		bool terminate = true;

		switch (m_mode) {
			case KX_STEERING_SEEK:
				if (vectotarg2d.length2()>m_distance*m_distance)
				{
					terminate = false;
					m_steerVec = vectotarg;
					m_steerVec.normalize();
					apply_steerforce = true;
				}
				break;
			case KX_STEERING_FLEE:
				if (vectotarg2d.length2()<m_distance*m_distance)
				{
					terminate = false;
					m_steerVec = -vectotarg;
					m_steerVec.normalize();
					apply_steerforce = true;
				}
				break;
			case KX_STEERING_PATHFOLLOWING:
				if (m_navmesh && vectotarg.length2()>m_distance*m_distance)
				{
					terminate = false;

					static const MT_Scalar WAYPOINT_RADIUS(0.25);

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

						m_steerVec = waypoint - mypos;
						apply_steerforce = true;

						
						if (m_enableVisualization)
						{
							//debug draw
							static const MT_Vector3 PATH_COLOR(1,0,0);
							m_navmesh->DrawPath(m_path, m_pathLen, PATH_COLOR);
						}
					}	
					
				}
				break;
		}

		if (apply_steerforce)
		{
			bool isdyna = obj->IsDynamic();
			if (isdyna)
				m_steerVec.z() = 0;
			if (!m_steerVec.fuzzyZero())
				m_steerVec.normalize();
			MT_Vector3 newvel = m_velocity*m_steerVec;

			//adjust velocity to avoid obstacles
			if (m_simulation && m_obstacle /*&& !newvel.fuzzyZero()*/)
			{
				if (m_enableVisualization)
					KX_RasterizerDrawDebugLine(mypos, mypos + newvel, MT_Vector3(1.,0.,0.));
				m_simulation->AdjustObstacleVelocity(m_obstacle, m_mode!=KX_STEERING_PATHFOLLOWING ? m_navmesh : NULL, 
								newvel, m_acceleration*delta, m_turnspeed/180.0f*M_PI*delta);
				if (m_enableVisualization)
					KX_RasterizerDrawDebugLine(mypos, mypos + newvel, MT_Vector3(0.,1.,0.));
			}

			HandleActorFace(newvel);
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
		else
		{
			if (m_simulation && m_obstacle)
			{
				m_obstacle->dvel[0] = 0.f;
				m_obstacle->dvel[1] = 0.f;
			}
			
		}

		if (terminate && m_isSelfTerminated)
			return false;
	}

	return true;
}

const MT_Vector3& KX_SteeringActuator::GetSteeringVec()
{
	static MT_Vector3 ZERO_VECTOR(0, 0, 0);
	if (m_isActive)
		return m_steerVec;
	else
		return ZERO_VECTOR;
}

inline float vdot2(const float* a, const float* b)
{
	return a[0]*b[0] + a[2]*b[2];
}
static bool barDistSqPointToTri(const float* p, const float* a, const float* b, const float* c)
{
	float v0[3], v1[3], v2[3];
	rcVsub(v0, c,a);
	rcVsub(v1, b,a);
	rcVsub(v2, p,a);

	const float dot00 = vdot2(v0, v0);
	const float dot01 = vdot2(v0, v1);
	const float dot02 = vdot2(v0, v2);
	const float dot11 = vdot2(v1, v1);
	const float dot12 = vdot2(v1, v2);

	// Compute barycentric coordinates
	float invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
	float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
	float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

	float ud = u<0.f ? -u : (u>1.f ? u-1.f : 0.f);
	float vd = v<0.f ? -v : (v>1.f ? v-1.f : 0.f);
	return ud * ud + vd * vd;
}

inline void flipAxes(float* vec)
{
	std::swap(vec[1],vec[2]);
}

static bool getNavmeshNormal(dtStatNavMesh* navmesh, const MT_Vector3& pos, MT_Vector3& normal)
{
	static const float polyPickExt[3] = {2, 4, 2};
	float spos[3];
	pos.getValue(spos);	
	flipAxes(spos);	
	dtStatPolyRef sPolyRef = navmesh->findNearestPoly(spos, polyPickExt);
	if (sPolyRef == 0)
		return false;
	const dtStatPoly* p = navmesh->getPoly(sPolyRef-1);
	const dtStatPolyDetail* pd = navmesh->getPolyDetail(sPolyRef-1);

	float distMin = FLT_MAX;
	int idxMin = -1;
	for (int i = 0; i < pd->ntris; ++i)
	{
		const unsigned char* t = navmesh->getDetailTri(pd->tbase+i);
		const float* v[3];
		for (int j = 0; j < 3; ++j)
		{
			if (t[j] < p->nv)
				v[j] = navmesh->getVertex(p->v[t[j]]);
			else
				v[j] = navmesh->getDetailVertex(pd->vbase+(t[j]-p->nv));
		}
		float dist = barDistSqPointToTri(spos, v[0], v[1], v[2]);
		if (dist<distMin)
		{
			distMin = dist;
			idxMin = i;
		}			
	}

	if (idxMin>=0)
	{
		const unsigned char* t = navmesh->getDetailTri(pd->tbase+idxMin);
		const float* v[3];
		for (int j = 0; j < 3; ++j)
		{
			if (t[j] < p->nv)
				v[j] = navmesh->getVertex(p->v[t[j]]);
			else
				v[j] = navmesh->getDetailVertex(pd->vbase+(t[j]-p->nv));
		}
		MT_Vector3 tri[3];
		for (size_t j=0; j<3; j++)
			tri[j].setValue(v[j][0],v[j][2],v[j][1]);
		MT_Vector3 a,b;
		a = tri[1]-tri[0];
		b = tri[2]-tri[0];
		normal = b.cross(a).safe_normalized();
		return true;
	}

	return false;
}

void KX_SteeringActuator::HandleActorFace(MT_Vector3& velocity)
{
	if (m_facingMode==0 && (!m_navmesh || !m_normalUp))
		return;
	KX_GameObject* curobj = (KX_GameObject*) GetParent();
	MT_Vector3 dir = m_facingMode==0 ?  curobj->NodeGetLocalOrientation().getColumn(1) : velocity;
	if (dir.fuzzyZero())
		return;	
	dir.normalize();
	MT_Vector3 up(0,0,1);
	MT_Vector3 left;
	MT_Matrix3x3 mat;
	
	if (m_navmesh && m_normalUp)
	{
		dtStatNavMesh* navmesh =  m_navmesh->GetNavMesh();
		MT_Vector3 normal;
		MT_Vector3 trpos = m_navmesh->TransformToLocalCoords(curobj->NodeGetWorldPosition());
		if (getNavmeshNormal(navmesh, trpos, normal))
		{

			left = (dir.cross(up)).safe_normalized();
			dir = (-left.cross(normal)).safe_normalized();			
			up = normal;
		}
	}

	switch (m_facingMode)
	{
	case 1: // TRACK X
		{
			left  = dir.safe_normalized();
			dir = -(left.cross(up)).safe_normalized();
			break;
		};
	case 2:	// TRACK Y
		{
			left  = (dir.cross(up)).safe_normalized();
			break;
		}

	case 3: // track Z
		{
			left = up.safe_normalized();
			up = dir.safe_normalized();
			dir = left;
			left  = (dir.cross(up)).safe_normalized();
			break;
		}

	case 4: // TRACK -X
		{
			left  = -dir.safe_normalized();
			dir = -(left.cross(up)).safe_normalized();
			break;
		};
	case 5: // TRACK -Y
		{
			left  = (-dir.cross(up)).safe_normalized();
			dir = -dir;
			break;
		}
	case 6: // track -Z
		{
			left = up.safe_normalized();
			up = -dir.safe_normalized();
			dir = left;
			left  = (dir.cross(up)).safe_normalized();
			break;
		}
	}

	mat.setValue (
		left[0], dir[0],up[0], 
		left[1], dir[1],up[1],
		left[2], dir[2],up[2]
	);

	
	
	KX_GameObject* parentObject = curobj->GetParent();
	if(parentObject)
	{ 
		MT_Point3 localpos;
		localpos = curobj->GetSGNode()->GetLocalPosition();
		MT_Matrix3x3 parentmatinv;
		parentmatinv = parentObject->NodeGetWorldOrientation ().inverse ();				
		mat = parentmatinv * mat;
		mat = m_parentlocalmat * mat;
		curobj->NodeSetLocalOrientation(mat);
		curobj->NodeSetLocalPosition(localpos);
	}
	else
	{
		curobj->NodeSetLocalOrientation(mat);
	}

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
	KX_PYATTRIBUTE_INT_RW("behavior", KX_STEERING_NODEF+1, KX_STEERING_MAX-1, true, KX_SteeringActuator, m_mode),
	KX_PYATTRIBUTE_RW_FUNCTION("target", KX_SteeringActuator, pyattr_get_target, pyattr_set_target),
	KX_PYATTRIBUTE_RW_FUNCTION("navmesh", KX_SteeringActuator, pyattr_get_navmesh, pyattr_set_navmesh),
	KX_PYATTRIBUTE_FLOAT_RW("distance", 0.0f, 1000.0f, KX_SteeringActuator, m_distance),
	KX_PYATTRIBUTE_FLOAT_RW("velocity", 0.0f, 1000.0f, KX_SteeringActuator, m_velocity),
	KX_PYATTRIBUTE_FLOAT_RW("acceleration", 0.0f, 1000.0f, KX_SteeringActuator, m_acceleration),
	KX_PYATTRIBUTE_FLOAT_RW("turnspeed", 0.0f, 720.0f, KX_SteeringActuator, m_turnspeed),
	KX_PYATTRIBUTE_BOOL_RW("selfterminated", KX_SteeringActuator, m_isSelfTerminated),
	KX_PYATTRIBUTE_BOOL_RW("enableVisualization", KX_SteeringActuator, m_enableVisualization),
	KX_PYATTRIBUTE_RO_FUNCTION("steeringVec", KX_SteeringActuator, pyattr_get_steeringVec),	
	KX_PYATTRIBUTE_SHORT_RW("facingMode", 0, 6, true, KX_SteeringActuator, m_facingMode),
	KX_PYATTRIBUTE_INT_RW("pathUpdatePeriod", -1, 100000, true, KX_SteeringActuator, m_pathUpdatePeriod),
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

PyObject* KX_SteeringActuator::pyattr_get_navmesh(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_SteeringActuator* actuator = static_cast<KX_SteeringActuator*>(self);
	if (!actuator->m_navmesh)	
		Py_RETURN_NONE;
	else
		return actuator->m_navmesh->GetProxy();
}

int KX_SteeringActuator::pyattr_set_navmesh(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_SteeringActuator* actuator = static_cast<KX_SteeringActuator*>(self);
	KX_GameObject *gameobj;

	if (!ConvertPythonToGameObject(value, &gameobj, true, "actuator.object = value: KX_SteeringActuator"))
		return PY_SET_ATTR_FAIL; // ConvertPythonToGameObject sets the error

	if (!PyObject_TypeCheck(value, &KX_NavMeshObject::Type))
	{
		PyErr_Format(PyExc_TypeError, "KX_NavMeshObject is expected");
		return PY_SET_ATTR_FAIL;
	}

	if (actuator->m_navmesh != NULL)
		actuator->m_navmesh->UnregisterActuator(actuator);	

	actuator->m_navmesh = static_cast<KX_NavMeshObject*>(gameobj);

	if (actuator->m_navmesh)
		actuator->m_navmesh->RegisterActuator(actuator);

	return PY_SET_ATTR_SUCCESS;
}

PyObject* KX_SteeringActuator::pyattr_get_steeringVec(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_SteeringActuator* actuator = static_cast<KX_SteeringActuator*>(self);
	const MT_Vector3& steeringVec = actuator->GetSteeringVec();
	return PyObjectFrom(steeringVec);
}

#endif // DISABLE_PYTHON

/* eof */

