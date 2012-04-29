/*
 * Add steering behaviors
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __KX_STEERINGACTUATOR_H__
#define __KX_STEERINGACTUATOR_H__

#include "SCA_IActuator.h"
#include "SCA_LogicManager.h"
#include "MT_Matrix3x3.h"

class KX_GameObject;
class KX_NavMeshObject;
struct KX_Obstacle;
class KX_ObstacleSimulation;
const int MAX_PATH_LENGTH  = 128;

class KX_SteeringActuator : public SCA_IActuator
{
	Py_Header

	/** Target object */
	KX_GameObject *m_target;
	KX_NavMeshObject *m_navmesh;
	int	m_mode;
	float m_distance;
	float m_velocity;
	float m_acceleration;
	float m_turnspeed;
	KX_ObstacleSimulation* m_simulation;
	
	double m_updateTime;
	KX_Obstacle* m_obstacle;
	bool m_isActive;
	bool m_isSelfTerminated;
	bool m_enableVisualization;
	short m_facingMode;
	bool m_normalUp;
	float m_path[MAX_PATH_LENGTH*3];
	int m_pathLen;
	int m_pathUpdatePeriod;
	double m_pathUpdateTime;
	int m_wayPointIdx;
	MT_Matrix3x3 m_parentlocalmat;
	MT_Vector3 m_steerVec;
	void HandleActorFace(MT_Vector3& velocity);
public:
	enum KX_STEERINGACT_MODE
	{
		KX_STEERING_NODEF = 0,
		KX_STEERING_SEEK,
		KX_STEERING_FLEE,
		KX_STEERING_PATHFOLLOWING,
		KX_STEERING_MAX
	};

	KX_SteeringActuator(class SCA_IObject* gameobj,
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
						bool enableVisualization);
	virtual ~KX_SteeringActuator();
	virtual bool Update(double curtime, bool frame);

	virtual CValue* GetReplica();
	virtual void ProcessReplica();
	virtual void ReParent(SCA_IObject* parent);
	virtual void Relink(CTR_Map<CTR_HashedPtr, void*> *obj_map);
	virtual bool UnlinkObject(SCA_IObject* clientobj);
	const MT_Vector3& GetSteeringVec();

#ifdef WITH_PYTHON

	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */

	/* These are used to get and set m_target */
	static PyObject* pyattr_get_target(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static int pyattr_set_target(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static PyObject* pyattr_get_navmesh(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static int pyattr_set_navmesh(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static PyObject* pyattr_get_steeringVec(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	

#endif // WITH_PYTHON

}; /* end of class KX_SteeringActuator : public SCA_PropertyActuator */

#endif

