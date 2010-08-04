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
#include "BLI_math.h"

namespace
{
	inline float perp(const MT_Vector2& a, const MT_Vector2& b) { return a.x()*b.y() - a.y()*b.x(); }

	inline float sqr(float x) { return x*x; }
	inline float lerp(float a, float b, float t) { return a + (b-a)*t; }
	inline float clamp(float a, float mn, float mx) { return a < mn ? mn : (a > mx ? mx : a); }

	inline float vdistsqr(const float* a, const float* b) { return sqr(b[0]-a[0]) + sqr(b[1]-a[1]); }
	inline float vdist(const float* a, const float* b) { return sqrtf(vdistsqr(a,b)); }
	inline void vcpy(float* a, const float* b) { a[0]=b[0]; a[1]=b[1]; }
	inline float vdot(const float* a, const float* b) { return a[0]*b[0] + a[1]*b[1]; }
	inline float vperp(const float* a, const float* b) { return a[0]*b[1] - a[1]*b[0]; }
	inline void vsub(float* v, const float* a, const float* b) { v[0] = a[0]-b[0]; v[1] = a[1]-b[1]; }
	inline void vadd(float* v, const float* a, const float* b) { v[0] = a[0]+b[0]; v[1] = a[1]+b[1]; }
	inline void vscale(float* v, const float* a, const float s) { v[0] = a[0]*s; v[1] = a[1]*s; }
	inline void vset(float* v, float x, float y) { v[0]=x; v[1]=y; }
	inline float vlensqr(const float* v) { return vdot(v,v); }
	inline float vlen(const float* v) { return sqrtf(vlensqr(v)); }
	inline void vlerp(float* v, const float* a, const float* b, float t) { v[0] = lerp(a[0], b[0], t); v[1] = lerp(a[1], b[1], t); }
	inline void vmad(float* v, const float* a, const float* b, float s) { v[0] = a[0] + b[0]*s; v[1] = a[1] + b[1]*s; }
	inline void vnorm(float* v)
	{
		float d = vlen(v);
		if (d > 0.0001f)
		{
			d = 1.0f/d;
			v[0] *= d;
			v[1] *= d;
		}
	}
}
inline float triarea(const float* a, const float* b, const float* c)
{
	return (b[0]*a[1] - a[0]*b[1]) + (c[0]*b[1] - b[0]*c[1]) + (a[0]*c[1] - c[0]*a[1]);
}

static void closestPtPtSeg(const float* pt,
					const float* sp, const float* sq,
					float& t)
{
	float dir[2],diff[3];
	vsub(dir,sq,sp);
	vsub(diff,pt,sp);
	t = vdot(diff,dir);
	if (t <= 0.0f) { t = 0; return; }
	float d = vdot(dir,dir);
	if (t >= d) { t = 1; return; }
	t /= d;
}

static float distPtSegSqr(const float* pt, const float* sp, const float* sq)
{
	float t;
	closestPtPtSeg(pt, sp,sq, t);
	float np[2];
	vlerp(np, sp,sq, t);
	return vdistsqr(pt,np);
}

static int sweepCircleCircle(const MT_Vector3& pos0, const MT_Scalar r0, const MT_Vector2& v,
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

static int sweepCircleSegment(const MT_Vector3& pos0, const MT_Scalar r0, const MT_Vector2& v,
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

static bool inBetweenAngle(float a, float amin, float amax, float& t)
{
	if (amax < amin) amax += (float)M_PI*2;
	if (a < amin-(float)M_PI) a += (float)M_PI*2;
	if (a > amin+(float)M_PI) a -= (float)M_PI*2;
	if (a >= amin && a < amax)
	{
		t = (a-amin) / (amax-amin);
		return true;
	}
	return false;
}

KX_ObstacleSimulation::KX_ObstacleSimulation(MT_Scalar levelHeight, bool enableVisualization)
:	m_levelHeight(levelHeight)
,	m_enableVisualization(enableVisualization)
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
KX_Obstacle* KX_ObstacleSimulation::CreateObstacle(KX_GameObject* gameobj)
{
	KX_Obstacle* obstacle = new KX_Obstacle();
	obstacle->m_gameObj = gameobj;

	vset(obstacle->vel, 0,0);
	vset(obstacle->pvel, 0,0);
	vset(obstacle->dvel, 0,0);
	vset(obstacle->nvel, 0,0);
	for (int i = 0; i < VEL_HIST_SIZE; ++i)
		vset(&obstacle->hvel[i*2], 0,0);
	obstacle->hhead = 0;

	gameobj->RegisterObstacle(this);
	m_obstacles.push_back(obstacle);
	return obstacle;
}

void KX_ObstacleSimulation::AddObstacleForObj(KX_GameObject* gameobj)
{
	KX_Obstacle* obstacle = CreateObstacle(gameobj);
	struct Object* blenderobject = gameobj->GetBlenderObject();
	obstacle->m_type = KX_OBSTACLE_OBJ;
	obstacle->m_shape = KX_OBSTACLE_CIRCLE;
	obstacle->m_rad = blenderobject->obstacleRad;
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
		
				KX_Obstacle* obstacle = CreateObstacle(navmeshobj);
				obstacle->m_type = KX_OBSTACLE_NAV_MESH;
				obstacle->m_shape = KX_OBSTACLE_SEGMENT;
				obstacle->m_pos = MT_Point3(vj[0], vj[2], vj[1]);
				obstacle->m_pos2 = MT_Point3(vi[0], vi[2], vi[1]);
				obstacle->m_rad = 0;
			}
		}
	}
}

void KX_ObstacleSimulation::DestroyObstacleForObj(KX_GameObject* gameobj)
{
	for (size_t i=0; i<m_obstacles.size(); )
	{
		if (m_obstacles[i]->m_gameObj == gameobj)
		{
			KX_Obstacle* obstacle = m_obstacles[i];
			obstacle->m_gameObj->UnregisterObstacle();
			m_obstacles[i] = m_obstacles.back();
			m_obstacles.pop_back();
			delete obstacle;
		}
		else
			i++;
	}
}

void KX_ObstacleSimulation::UpdateObstacles()
{
	for (size_t i=0; i<m_obstacles.size(); i++)
	{
		if (m_obstacles[i]->m_type==KX_OBSTACLE_NAV_MESH || m_obstacles[i]->m_shape==KX_OBSTACLE_SEGMENT)
			continue;

		KX_Obstacle* obs = m_obstacles[i];
		obs->m_pos = obs->m_gameObj->NodeGetWorldPosition();
		obs->vel[0] = obs->m_gameObj->GetLinearVelocity().x();
		obs->vel[1] = obs->m_gameObj->GetLinearVelocity().y();

		// Update velocity history and calculate perceived (average) velocity.
		vcpy(&obs->hvel[obs->hhead*2], obs->vel);
		obs->hhead = (obs->hhead+1) % VEL_HIST_SIZE;
		vset(obs->pvel,0,0);
		for (int j = 0; j < VEL_HIST_SIZE; ++j)
			vadd(obs->pvel, obs->pvel, &obs->hvel[j*2]);
		vscale(obs->pvel, obs->pvel, 1.0f/VEL_HIST_SIZE);
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
										MT_Vector3& velocity, MT_Scalar maxDeltaSpeed,MT_Scalar maxDeltaAngle)
{
}

void KX_ObstacleSimulation::DrawObstacles()
{
	if (!m_enableVisualization)
		return;
	static const MT_Vector3 bluecolor(0,0,1);
	static const MT_Vector3 normal(0.,0.,1.);
	static const int SECTORS_NUM = 32;
	for (size_t i=0; i<m_obstacles.size(); i++)
	{
		if (m_obstacles[i]->m_shape==KX_OBSTACLE_SEGMENT)
		{
			MT_Point3 p1 = m_obstacles[i]->m_pos;
			MT_Point3 p2 = m_obstacles[i]->m_pos2;
			//apply world transform
			if (m_obstacles[i]->m_type == KX_OBSTACLE_NAV_MESH)
			{
				KX_NavMeshObject* navmeshobj = static_cast<KX_NavMeshObject*>(m_obstacles[i]->m_gameObj);
				p1 = navmeshobj->TransformToWorldCoords(p1);
				p2 = navmeshobj->TransformToWorldCoords(p2);
			}

			KX_RasterizerDrawDebugLine(p1, p2, bluecolor);
		}
		else if (m_obstacles[i]->m_shape==KX_OBSTACLE_CIRCLE)
		{
			KX_RasterizerDrawDebugCircle(m_obstacles[i]->m_pos, m_obstacles[i]->m_rad, bluecolor,
										normal, SECTORS_NUM);
		}
	}	
}

static MT_Point3 nearestPointToObstacle(MT_Point3& pos ,KX_Obstacle* obstacle)
{
	switch (obstacle->m_shape)
	{
	case KX_OBSTACLE_SEGMENT :
	{
		MT_Vector3 ab = obstacle->m_pos2 - obstacle->m_pos;
		if (!ab.fuzzyZero())
		{
			MT_Vector3 abdir = ab.normalized();
			MT_Vector3  v = pos - obstacle->m_pos;
			MT_Scalar proj = abdir.dot(v);
			CLAMP(proj, 0, ab.length());
			MT_Point3 res = obstacle->m_pos + abdir*proj;
			return res;
		}		
	}
	case KX_OBSTACLE_CIRCLE :
	default:
		return obstacle->m_pos;
	}
}

static bool filterObstacle(KX_Obstacle* activeObst, KX_NavMeshObject* activeNavMeshObj, KX_Obstacle* otherObst,
							float levelHeight)
{
	//filter obstacles by type
	if ( (otherObst == activeObst) ||
		(otherObst->m_type==KX_OBSTACLE_NAV_MESH && otherObst->m_gameObj!=activeNavMeshObj)	)
		return false;

	//filter obstacles by position
	MT_Point3 p = nearestPointToObstacle(activeObst->m_pos, otherObst);
	if ( fabs(activeObst->m_pos.z() - p.z()) > levelHeight)
		return false;

	return true;
}

KX_ObstacleSimulationTOI::KX_ObstacleSimulationTOI(MT_Scalar levelHeight, bool enableVisualization):
	KX_ObstacleSimulation(levelHeight, enableVisualization),
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

KX_Obstacle* KX_ObstacleSimulationTOI::CreateObstacle(KX_GameObject* gameobj)
{
	KX_Obstacle* obstacle = KX_ObstacleSimulation::CreateObstacle(gameobj);
	m_toiCircles.push_back(new TOICircle());
	return obstacle;
}

static const float VEL_WEIGHT = 2.0f;
static const float CUR_VEL_WEIGHT = 0.75f;
static const float SIDE_WEIGHT = 0.75f;
static const float TOI_WEIGHT = 2.5f;

static void processSamples(KX_Obstacle* activeObst, KX_NavMeshObject* activeNavMeshObj, 
						   KX_Obstacles& obstacles,  float levelHeight, const float vmax,
						   const float* spos, const float cs, const int nspos,
						   float* res)
{
	vset(res, 0,0);

	const float ivmax = 1.0f / vmax;

	// Max time of collision to be considered.
	const float maxToi = 1.5f;

	float adir[2], adist;
	vcpy(adir, activeObst->pvel);
	if (vlen(adir) > 0.01f)
		vnorm(adir);
	else
		vset(adir,0,0);
	float activeObstPos[2];
	vset(activeObstPos, activeObst->m_pos.x(), activeObst->m_pos.y()); 
	adist = vdot(adir, activeObstPos);

	float minPenalty = FLT_MAX;

	for (int n = 0; n < nspos; ++n)
	{
		float vcand[2];
		vcpy(vcand, &spos[n*2]);		

		// Find min time of impact and exit amongst all obstacles.
		float tmin = maxToi;
		float side = 0;
		int nside = 0;

		for (int i = 0; i < obstacles.size(); ++i)
		{
			KX_Obstacle* ob = obstacles[i];
			bool res = filterObstacle(activeObst, activeNavMeshObj, ob, levelHeight);
			if (!res)
				continue;
			float htmin, htmax;

			if (ob->m_shape==KX_OBSTACLE_CIRCLE)
			{
				float vab[2];

				// Moving, use RVO
				vscale(vab, vcand, 2);
				vsub(vab, vab, activeObst->vel);
				vsub(vab, vab, ob->vel);

				// Side
				// NOTE: dp, and dv are constant over the whole calculation,
				// they can be precomputed per object. 
				const float* pa = activeObstPos;
				float pb[2];
				vset(pb, ob->m_pos.x(), ob->m_pos.y());

				const float orig[2] = {0,0};
				float dp[2],dv[2],np[2];
				vsub(dp,pb,pa);
				vnorm(dp);
				vsub(dv,ob->dvel, activeObst->dvel);

				const float a = triarea(orig, dp,dv);
				if (a < 0.01f)
				{
					np[0] = -dp[1];
					np[1] = dp[0];
				}
				else
				{
					np[0] = dp[1];
					np[1] = -dp[0];
				}

				side += clamp(min(vdot(dp,vab)*2,vdot(np,vab)*2), 0.0f, 1.0f);
				nside++;

				if (!sweepCircleCircle(activeObst->m_pos, activeObst->m_rad, vab, ob->m_pos, ob->m_rad, 
					htmin, htmax))
					continue;

				// Handle overlapping obstacles.
				if (htmin < 0.0f && htmax > 0.0f)
				{
					// Avoid more when overlapped.
					htmin = -htmin * 0.5f;
				}
			}
			else if (ob->m_shape == KX_OBSTACLE_SEGMENT)
			{
				MT_Point3 p1 = ob->m_pos;
				MT_Point3 p2 = ob->m_pos2;
				//apply world transform
				if (ob->m_type == KX_OBSTACLE_NAV_MESH)
				{
					KX_NavMeshObject* navmeshobj = static_cast<KX_NavMeshObject*>(ob->m_gameObj);
					p1 = navmeshobj->TransformToWorldCoords(p1);
					p2 = navmeshobj->TransformToWorldCoords(p2);
				}
				float p[2], q[2];
				vset(p, p1.x(), p1.y());
				vset(q, p2.x(), p2.y());

				// NOTE: the segments are assumed to come from a navmesh which is shrunken by
				// the agent radius, hence the use of really small radius.
				// This can be handle more efficiently by using seg-seg test instead.
				// If the whole segment is to be treated as obstacle, use agent->rad instead of 0.01f!
				const float r = 0.01f; // agent->rad
				if (distPtSegSqr(activeObstPos, p, q) < sqr(r+ob->m_rad))
				{
					float sdir[2], snorm[2];
					vsub(sdir, q, p);
					snorm[0] = sdir[1];
					snorm[1] = -sdir[0];
					// If the velocity is pointing towards the segment, no collision.
					if (vdot(snorm, vcand) < 0.0f)
						continue;
					// Else immediate collision.
					htmin = 0.0f;
					htmax = 10.0f;
				}
				else
				{
					if (!sweepCircleSegment(activeObstPos, r, vcand, p, q, ob->m_rad, htmin, htmax))
						continue;
				}

				// Avoid less when facing walls.
				htmin *= 2.0f;
			}

			if (htmin >= 0.0f)
			{
				// The closest obstacle is somewhere ahead of us, keep track of nearest obstacle.
				if (htmin < tmin)
					tmin = htmin;
			}
		}

		// Normalize side bias, to prevent it dominating too much.
		if (nside)
			side /= nside;

		const float vpen = VEL_WEIGHT * (vdist(vcand, activeObst->dvel) * ivmax);
		const float vcpen = CUR_VEL_WEIGHT * (vdist(vcand, activeObst->vel) * ivmax);
		const float spen = SIDE_WEIGHT * side;
		const float tpen = TOI_WEIGHT * (1.0f/(0.1f+tmin/maxToi));

		const float penalty = vpen + vcpen + spen + tpen;

		if (penalty < minPenalty)
		{
			minPenalty = penalty;
			vcpy(res, vcand);
		}
	}
}

static const int RVO_SAMPLE_RAD = 15;
static const int MAX_RVO_SAMPLES = (RVO_SAMPLE_RAD*2+1)*(RVO_SAMPLE_RAD*2+1) + 100;

static void sampleRVO(KX_Obstacle* activeObst, KX_NavMeshObject* activeNavMeshObj, 
					  KX_Obstacles& obstacles, const float levelHeight,const float bias)
{
	float spos[2*MAX_RVO_SAMPLES];
	int nspos = 0;

	const float cvx = activeObst->dvel[0]*bias;
	const float cvy = activeObst->dvel[1]*bias;
	float vmax = vlen(activeObst->dvel);
	const float vrange = vmax*(1-bias);
	const float cs = 1.0f / (float)RVO_SAMPLE_RAD*vrange;

	for (int y = -RVO_SAMPLE_RAD; y <= RVO_SAMPLE_RAD; ++y)
	{
		for (int x = -RVO_SAMPLE_RAD; x <= RVO_SAMPLE_RAD; ++x)
		{
			if (nspos < MAX_RVO_SAMPLES)
			{
				const float vx = cvx + (float)(x+0.5f)*cs;
				const float vy = cvy + (float)(y+0.5f)*cs;
				if (vx*vx+vy*vy > sqr(vmax+cs/2)) continue;
				spos[nspos*2+0] = vx;
				spos[nspos*2+1] = vy;
				nspos++;
			}
		}
	}
	processSamples(activeObst, activeNavMeshObj, obstacles, levelHeight, vmax, spos, cs/2, 
					nspos,  activeObst->nvel);
}

static void sampleRVOAdaptive(KX_Obstacle* activeObst, KX_NavMeshObject* activeNavMeshObj, 
					   KX_Obstacles& obstacles, const float levelHeight,const float bias)
{
	vset(activeObst->nvel, 0.f, 0.f);
	float vmax = vlen(activeObst->dvel);

	float spos[2*MAX_RVO_SAMPLES];
	int nspos = 0;

	int rad;
	float res[2];
	float cs;

	// First sample location.
	rad = 4;
	res[0] = activeObst->dvel[0]*bias;
	res[1] = activeObst->dvel[1]*bias;
	cs = vmax*(2-bias*2) / (float)(rad-1);

	for (int k = 0; k < 5; ++k)
	{
		const float half = (rad-1)*cs*0.5f;

		nspos = 0;
		for (int y = 0; y < rad; ++y)
		{
			for (int x = 0; x < rad; ++x)
			{
				const float vx = res[0] + x*cs - half;
				const float vy = res[1] + y*cs - half;
				if (vx*vx+vy*vy > sqr(vmax+cs/2)) continue;
				spos[nspos*2+0] = vx;
				spos[nspos*2+1] = vy;
				nspos++;
			}
		}

		processSamples(activeObst, activeNavMeshObj, obstacles, levelHeight, vmax, spos, cs/2, 
			nspos,  res);

		cs *= 0.5f;
	}

	vcpy(activeObst->nvel, res);
}


void KX_ObstacleSimulationTOI::AdjustObstacleVelocity(KX_Obstacle* activeObst, KX_NavMeshObject* activeNavMeshObj, 
													MT_Vector3& velocity, MT_Scalar maxDeltaSpeed, MT_Scalar maxDeltaAngle)
{
	int nobs = m_obstacles.size();
	int obstidx = std::find(m_obstacles.begin(), m_obstacles.end(), activeObst) - m_obstacles.begin();
	if (obstidx == nobs)
		return;
	
	vset(activeObst->dvel, velocity.x(), velocity.y());

	//apply RVO
	const float bias = 0.4f;
	//sampleRVO(activeObst, activeNavMeshObj, m_obstacles, m_levelHeight, bias);
	sampleRVOAdaptive(activeObst, activeNavMeshObj, m_obstacles, m_levelHeight, bias);

	// Fake dynamic constraint.
	float dv[2];
	float vel[2];
	vsub(dv, activeObst->nvel, activeObst->vel);
	float ds = vlen(dv);
	if (ds > maxDeltaSpeed || ds<-maxDeltaSpeed)
		vscale(dv, dv, fabs(maxDeltaSpeed/ds));
	vadd(vel, activeObst->vel, dv);
	
	velocity.x() = vel[0];
	velocity.y() = vel[1];
/*	printf("dvel: %f, nvel: %f, vel: %f\n", vlen(activeObst->dvel), vlen(activeObst->nvel),
											vlen(vel));*/

}
/*
#include "GL/glew.h"
void KX_ObstacleSimulation::DebugDraw()
{
	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, 100.0, 0.0, 100.0, -1.0, 1.0);
	glBegin(GL_QUADS);
	glColor4ub(255,0,0,255);
	glVertex2f(0.f, 0.f);
	glVertex2f(100.f, 25.f);
	glVertex2f(100.f, 75.f);
	glVertex2f(25.f, 75.f);
	glEnd(); 

}*/