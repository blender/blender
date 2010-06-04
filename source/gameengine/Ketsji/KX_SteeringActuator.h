/**
* Add steering behaviors
*
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

#ifndef __KX_STEERINGACTUATOR
#define __KX_STEERINGACTUATOR

#include "SCA_IActuator.h"
#include "SCA_LogicManager.h"

class KX_GameObject;
class KX_NavMeshObject;
struct KX_Obstacle;
class KX_ObstacleSimulation;

class KX_SteeringActuator : public SCA_IActuator
{
	Py_Header;

	/** Target object */
	KX_GameObject *m_target;
	KX_NavMeshObject *m_navmesh;
	int	m_mode;
	MT_Scalar m_distance;
	MT_Scalar m_velocity;
	KX_ObstacleSimulation* m_simulation;
	
	KX_Obstacle* m_obstacle;
	double m_updateTime;
	bool m_isActive;
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
						MT_Scalar movement, 
						MT_Scalar distance,
						KX_ObstacleSimulation* simulation);
	virtual ~KX_SteeringActuator();
	virtual bool Update(double curtime, bool frame);

	virtual CValue* GetReplica();
	virtual void ProcessReplica();
	virtual void Relink(GEN_Map<GEN_HashedPtr, void*> *obj_map);
	virtual bool UnlinkObject(SCA_IObject* clientobj);

#ifndef DISABLE_PYTHON

	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */

	/* These are used to get and set m_target */
	static PyObject* pyattr_get_target(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static int pyattr_set_target(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);

#endif // DISABLE_PYTHON

}; /* end of class KX_SteeringActuator : public SCA_PropertyActuator */

#endif

