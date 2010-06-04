/**
* Simulation for obstacle avoidance behavior
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

#include "KX_ObstacleSimulation.h"
#include "KX_GameObject.h"
#include "DNA_object_types.h"
#include "math.h"
#define M_PI       3.14159265358979323846

int sweepCircleCircle(const MT_Vector3& pos0, const MT_Scalar r0, const MT_Vector2& v,
					  const MT_Vector3& pos1, const MT_Scalar r1,
					  float& tmin, float& tmax)
{
	static const float EPS = 0.0001f;
	MT_Vector2 c0(pos0.x(), pos0.y());
	MT_Vector2 c1(pos1.x(), pos1.y());
	MT_Vector2 s = c1 - c0;
	MT_Scalar  r = r0+r1;
	float c = s.length2() - r*r;
	float a = v.length2();
	if (a < EPS) return 0;	// not moving

	// Overlap, calc time to exit.
	float b = MT_dot(v,s);
	float d = b*b - a*c;
	if (d < 0.0f) return 0; // no intersection.
	tmin = (b - sqrtf(d)) / a;
	tmax = (b + sqrtf(d)) / a;
	return 1;
}


KX_ObstacleSimulation::KX_ObstacleSimulation()
{

}

KX_ObstacleSimulation::~KX_ObstacleSimulation()
{
	for (size_t i=0; i<m_obstacles.size(); i++)
	{
		KX_Obstacle* obs = m_obstacles[i];
		delete obs;
	}
	m_obstacles.clear();
}
void KX_ObstacleSimulation::AddObstacleForObj(KX_GameObject* gameobj)
{
	KX_Obstacle* obstacle = new KX_Obstacle();
	struct Object* blenderobject = gameobj->GetBlenderObject();
	obstacle->m_rad = blenderobject->inertia; //.todo use radius of collision shape bound sphere 
	obstacle->m_gameObj = gameobj;
	m_obstacles.push_back(obstacle);
}

void KX_ObstacleSimulation::UpdateObstacles()
{
	for (size_t i=0; i<m_obstacles.size(); i++)
	{
		KX_Obstacle* obs = m_obstacles[i];
		obs->m_pos = obs->m_gameObj->NodeGetWorldPosition();
		obs->m_vel.x() = obs->m_gameObj->GetLinearVelocity().x();
		obs->m_vel.y() = obs->m_gameObj->GetLinearVelocity().y();
	}
}

KX_Obstacle* KX_ObstacleSimulation::GetObstacle(KX_GameObject* gameobj)
{
	for (size_t i=0; i<m_obstacles.size(); i++)
	{
		if (m_obstacles[i]->m_gameObj == gameobj)
			return m_obstacles[i];
	}

	return NULL;
}

void KX_ObstacleSimulation::AdjustObstacleVelocity(KX_Obstacle* activeObst, MT_Vector3& velocity)
{
}

KX_ObstacleSimulationTOI::KX_ObstacleSimulationTOI():
	m_avoidSteps(32),
	m_minToi(0.5f),
	m_maxToi(1.2f),
	m_angleWeight(4.0f),
	m_toiWeight(1.0f),
	m_collisionWeight(100.0f)
{
	
}

KX_ObstacleSimulationTOI::~KX_ObstacleSimulationTOI()
{
	for (size_t i=0; i<m_toiCircles.size(); i++)
	{
		TOICircle* toi = m_toiCircles[i];
		delete toi;
	}
	m_toiCircles.clear();
}

void KX_ObstacleSimulationTOI::AddObstacleForObj(KX_GameObject* gameobj)
{
	KX_ObstacleSimulation::AddObstacleForObj(gameobj);
	m_toiCircles.push_back(new TOICircle());
}

void KX_ObstacleSimulationTOI::AdjustObstacleVelocity(KX_Obstacle* activeObst, MT_Vector3& velocity)
{
	int nobs = m_obstacles.size();
	int obstidx = std::find(m_obstacles.begin(), m_obstacles.end(), activeObst) - m_obstacles.begin();
	if (obstidx == nobs)
		return; 
	TOICircle* tc = m_toiCircles[obstidx];

	MT_Vector2 vel(velocity.x(), velocity.y());
	float vmax = (float) velocity.length();
	float odir = (float) atan2(velocity.y(), velocity.x());

	MT_Vector2 ddir = vel;
	ddir.normalize();

	float bestScore = FLT_MAX;
	float bestDir = odir;
	float bestToi = 0;

	tc->n = m_avoidSteps;
	tc->minToi = m_minToi;
	tc->maxToi = m_maxToi;

	const int iforw = m_avoidSteps/2;
	const float aoff = (float)iforw / (float)m_avoidSteps;

	for (int iter = 0; iter < m_avoidSteps; ++iter)
	{
		// Calculate sample velocity
		const float ndir = ((float)iter/(float)m_avoidSteps) - aoff;
		const float dir = odir+ndir*M_PI*2;
		MT_Vector2 svel;
		svel.x() = cosf(dir) * vmax;
		svel.y() = sinf(dir) * vmax;

		// Find min time of impact and exit amongst all obstacles.
		float tmin = m_maxToi;
		float tmine = 0;
		for (int i = 0; i < nobs; ++i)
		{
			if (i==obstidx)
				continue;
			KX_Obstacle* ob = m_obstacles[i];

			float htmin,htmax;

			MT_Vector2 vab;
			if (ob->m_vel.length2() < 0.01f*0.01f)
			{
				// Stationary, use VO
				vab = svel;
			}
			else
			{
				// Moving, use RVO
				vab = 2*svel - vel - ob->m_vel;
			}

			if (!sweepCircleCircle(activeObst->m_pos, activeObst->m_rad, 
									vab, ob->m_pos, ob->m_rad, htmin, htmax))
				continue;

			if (htmin > 0.0f)
			{
				// The closest obstacle is somewhere ahead of us, keep track of nearest obstacle.
				if (htmin < tmin)
					tmin = htmin;
			}
			else if	(htmax > 0.0f)
			{
				// The agent overlaps the obstacle, keep track of first safe exit.
				if (htmax > tmine)
					tmine = htmax;
			}
		}

		// Calculate sample penalties and final score.
		const float apen = m_angleWeight * fabsf(ndir);
		const float tpen = m_toiWeight * (1.0f/(0.0001f+tmin/m_maxToi));
		const float cpen = m_collisionWeight * (tmine/m_minToi)*(tmine/m_minToi);
		const float score = apen + tpen + cpen;

		// Update best score.
		if (score < bestScore)
		{
			bestDir = dir;
			bestToi = tmin;
			bestScore = score;
		}

		tc->dir[iter] = dir;
		tc->toi[iter] = tmin;
		tc->toie[iter] = tmine;
	}

	// Adjust speed when time of impact is less than min TOI.
	if (bestToi < m_minToi)
		vmax *= bestToi/m_minToi;

	// New steering velocity.
	vel.x() = cosf(bestDir) * vmax;
	vel.y() = sinf(bestDir) * vmax;

	velocity.x() = vel.x();
	velocity.y() = vel.y();
}