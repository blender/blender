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
#include "KX_NavMeshObject.h"
#include "KX_PythonInit.h"
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

inline float perp(const MT_Vector2& a, const MT_Vector2& b) { return a.x()*b.y() - a.y()*b.x(); }


int sweepCircleSegment(const MT_Vector3& pos0, const MT_Scalar r0, const MT_Vector2& v,
					   const MT_Vector3& pa, const MT_Vector3& pb, const MT_Scalar sr,
					   float& tmin, float &tmax)
{
	// equation parameters
	MT_Vector2 c0(pos0.x(), pos0.y());
	MT_Vector2 sa(pa.x(), pa.y());
	MT_Vector2 sb(pb.x(), pb.y());
	MT_Vector2 L = sb-sa;
	MT_Vector2 H = c0-sa;
	MT_Scalar radius = r0+sr;
	float l2 = L.length2();
	float r2 = radius * radius;
	float dl = perp(v, L);
	float hl = perp(H, L);
	float a = dl * dl;
	float b = 2.0f * hl * dl;
	float c = hl * hl - (r2 * l2);
	float d = (b*b) - (4.0f * a * c);

	// infinite line missed by infinite ray.
	if (d < 0.0f)
		return 0;

	d = sqrtf(d);
	tmin = (-b - d) / (2.0f * a);
	tmax = (-b + d) / (2.0f * a);

	// line missed by ray range.
	/*	if (tmax < 0.0f || tmin > 1.0f)
	return 0;*/

	// find what part of the ray was collided.
	MT_Vector2 Pedge;
	Pedge = c0+v*tmin;
	H = Pedge - sa;
	float e0 = MT_dot(H, L) / l2;
	Pedge = c0 + v*tmax;
	H = Pedge - sa;
	float e1 = MT_dot(H, L) / l2;

	if (e0 < 0.0f || e1 < 0.0f)
	{
		float ctmin, ctmax;
		if (sweepCircleCircle(pos0, r0, v, pa, sr, ctmin, ctmax))
		{
			if (e0 < 0.0f && ctmin > tmin)
				tmin = ctmin;
			if (e1 < 0.0f && ctmax < tmax)
				tmax = ctmax;
		}
		else
		{
			return 0;
		}
	}

	if (e0 > 1.0f || e1 > 1.0f)
	{
		float ctmin, ctmax;
		if (sweepCircleCircle(pos0, r0, v, pb, sr, ctmin, ctmax))
		{
			if (e0 > 1.0f && ctmin > tmin)
				tmin = ctmin;
			if (e1 > 1.0f && ctmax < tmax)
				tmax = ctmax;
		}
		else
		{
			return 0;
		}
	}

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
KX_Obstacle* KX_ObstacleSimulation::CreateObstacle()
{
	KX_Obstacle* obstacle = new KX_Obstacle();
	m_obstacles.push_back(obstacle);
	return obstacle;
}

void KX_ObstacleSimulation::AddObstacleForObj(KX_GameObject* gameobj)
{
	KX_Obstacle* obstacle = CreateObstacle();
	struct Object* blenderobject = gameobj->GetBlenderObject();
	obstacle->m_type = KX_OBSTACLE_OBJ;
	obstacle->m_shape = KX_OBSTACLE_CIRCLE;
	obstacle->m_rad = blenderobject->inertia; //.todo use radius of collision shape bound sphere 
	obstacle->m_gameObj = gameobj;
	
}

void KX_ObstacleSimulation::AddObstaclesForNavMesh(KX_NavMeshObject* navmeshobj)
{	
	dtStatNavMesh* navmesh = navmeshobj->GetNavMesh();
	if (navmesh)
	{
		int npoly = navmesh->getPolyCount();
		for (int pi=0; pi<npoly; pi++)
		{
			const dtStatPoly* poly = navmesh->getPoly(pi);

			for (int i = 0, j = (int)poly->nv-1; i < (int)poly->nv; j = i++)
			{	
				if (poly->n[j]) continue;
				const float* vj = navmesh->getVertex(poly->v[j]);
				const float* vi = navmesh->getVertex(poly->v[i]);
		
				KX_Obstacle* obstacle = CreateObstacle();
				obstacle->m_type = KX_OBSTACLE_NAV_MESH;
				obstacle->m_shape = KX_OBSTACLE_SEGMENT;
				obstacle->m_gameObj = navmeshobj;
				obstacle->m_pos = MT_Vector3(vj[0], vj[2], vj[1]);
				obstacle->m_pos2 = MT_Vector3(vi[0], vi[2], vi[1]);
				obstacle->m_rad = 0;
				obstacle->m_vel = MT_Vector2(0,0);
			}
		}
	}
}

void KX_ObstacleSimulation::UpdateObstacles()
{
	for (size_t i=0; i<m_obstacles.size(); i++)
	{
		if (m_obstacles[i]->m_shape==KX_OBSTACLE_NAV_MESH || m_obstacles[i]->m_shape==KX_OBSTACLE_SEGMENT)
			continue;

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

void KX_ObstacleSimulation::AdjustObstacleVelocity(KX_Obstacle* activeObst, KX_NavMeshObject* activeNavMeshObj, 
													MT_Vector3& velocity)
{
}

void KX_ObstacleSimulation::DrawObstacles()
{
	static const MT_Vector3 bluecolor(0,0,1);
	for (size_t i=0; i<m_obstacles.size(); i++)
	{
		if (m_obstacles[i]->m_shape==KX_OBSTACLE_SEGMENT)
		{
			KX_RasterizerDrawDebugLine(m_obstacles[i]->m_pos, m_obstacles[i]->m_pos2, bluecolor);
		}
	}	
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

KX_Obstacle* KX_ObstacleSimulationTOI::CreateObstacle()
{
	KX_Obstacle* obstacle = KX_ObstacleSimulation::CreateObstacle();
	m_toiCircles.push_back(new TOICircle());
	return obstacle;
}

void KX_ObstacleSimulationTOI::AdjustObstacleVelocity(KX_Obstacle* activeObst, KX_NavMeshObject* activeNavMeshObj, MT_Vector3& velocity)
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
			KX_Obstacle* ob = m_obstacles[i];
			if ( (ob==activeObst) ||
				 (ob->m_type==KX_OBSTACLE_NAV_MESH && ob->m_gameObj!=activeNavMeshObj)	)
				continue;

			float htmin,htmax;

			if (ob->m_type == KX_OBSTACLE_CIRCLE)
			{
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
			}
			else if (ob->m_type == KX_OBSTACLE_SEGMENT)
			{
				if (!sweepCircleSegment(activeObst->m_pos, activeObst->m_rad, svel, 
										ob->m_pos, ob->m_pos2, ob->m_rad, htmin, htmax))
					continue;
			}

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