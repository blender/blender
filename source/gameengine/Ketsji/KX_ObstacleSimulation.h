/**
* Simulation for obstacle avoidance behavior 
* (based on Cane Project - http://code.google.com/p/cane  by Mikko Mononen (c) 2009)
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

#ifndef __KX_OBSTACLESIMULATION
#define __KX_OBSTACLESIMULATION

#include <vector>
#include "MT_Point2.h"
#include "MT_Point3.h"

class KX_GameObject;

struct KX_Obstacle
{
	MT_Point3 m_pos;
	MT_Scalar m_rad;
	MT_Vector2 m_vel;
	KX_GameObject* m_gameObj;
};

class KX_ObstacleSimulation
{
protected:
	std::vector<KX_Obstacle*>	m_obstacles;
public:
	KX_ObstacleSimulation();
	virtual ~KX_ObstacleSimulation();

	virtual void AddObstacleForObj(KX_GameObject* gameobj);
	KX_Obstacle* GetObstacle(KX_GameObject* gameobj);
	void UpdateObstacles();	
	virtual void AdjustObstacleVelocity(KX_Obstacle* activeObst, MT_Vector3& velocity);

}; /* end of class KX_ObstacleSimulation*/

static const int AVOID_MAX_STEPS = 128;
struct TOICircle
{
	TOICircle() : n(0), minToi(0), maxToi(1) {}
	float	toi[AVOID_MAX_STEPS];	// Time of impact (seconds)
	float	toie[AVOID_MAX_STEPS];	// Time of exit (seconds)
	float	dir[AVOID_MAX_STEPS];	// Direction (radians)
	int		n;						// Number of samples
	float	minToi, maxToi;			// Min/max TOI (seconds)
};

class KX_ObstacleSimulationTOI: public KX_ObstacleSimulation
{
protected:
	int m_avoidSteps;				// Number of sample steps
	float m_minToi;					// Min TOI
	float m_maxToi;					// Max TOI
	float m_angleWeight;			// Sample selection angle weight
	float m_toiWeight;				// Sample selection TOI weight
	float m_collisionWeight;		// Sample selection collision weight

	std::vector<TOICircle*>	m_toiCircles; // TOI circles (one per active agent)
public:
	KX_ObstacleSimulationTOI();
	~KX_ObstacleSimulationTOI();
	virtual void AddObstacleForObj(KX_GameObject* gameobj);
	virtual void AdjustObstacleVelocity(KX_Obstacle* activeObst, MT_Vector3& velocity);
};

#endif
