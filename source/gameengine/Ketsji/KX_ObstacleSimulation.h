/*
 * Simulation for obstacle avoidance behavior 
 * (based on Cane Project - http://code.google.com/p/cane  by Mikko Mononen (c) 2009)
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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __KX_OBSTACLESIMULATION_H__
#define __KX_OBSTACLESIMULATION_H__

#include <vector>
#include "MT_Point2.h"
#include "MT_Point3.h"

class KX_GameObject;
class KX_NavMeshObject;

enum KX_OBSTACLE_TYPE
{	
	KX_OBSTACLE_OBJ, 
	KX_OBSTACLE_NAV_MESH,
};

enum KX_OBSTACLE_SHAPE
{	
	KX_OBSTACLE_CIRCLE, 
	KX_OBSTACLE_SEGMENT,
};

#define VEL_HIST_SIZE 6
struct KX_Obstacle
{
	KX_OBSTACLE_TYPE m_type;
	KX_OBSTACLE_SHAPE m_shape;
	MT_Point3 m_pos;
	MT_Point3 m_pos2;
	MT_Scalar m_rad;
	
	float vel[2];
	float pvel[2];
	float dvel[2];
	float nvel[2];
	float hvel[VEL_HIST_SIZE*2];
	int hhead;

	
	KX_GameObject* m_gameObj;
};
typedef std::vector<KX_Obstacle*> KX_Obstacles;

class KX_ObstacleSimulation
{
protected:
	KX_Obstacles m_obstacles;

	MT_Scalar m_levelHeight;
	bool m_enableVisualization;

	KX_Obstacle* CreateObstacle(KX_GameObject* gameobj);
public:
	KX_ObstacleSimulation(MT_Scalar levelHeight, bool enableVisualization);
	virtual ~KX_ObstacleSimulation();

	void DrawObstacles();
	//void DebugDraw();

	void AddObstacleForObj(KX_GameObject* gameobj);
	void DestroyObstacleForObj(KX_GameObject* gameobj);
	void AddObstaclesForNavMesh(KX_NavMeshObject* navmesh);
	KX_Obstacle* GetObstacle(KX_GameObject* gameobj);
	void UpdateObstacles();	
	virtual void AdjustObstacleVelocity(KX_Obstacle* activeObst, KX_NavMeshObject* activeNavMeshObj, 
								MT_Vector3& velocity, MT_Scalar maxDeltaSpeed,MT_Scalar maxDeltaAngle);

};
class KX_ObstacleSimulationTOI: public KX_ObstacleSimulation
{
protected:
	int m_maxSamples;				// Number of sample steps
	float m_minToi;					// Min TOI
	float m_maxToi;					// Max TOI
	float m_velWeight;				// Sample selection angle weight
	float m_curVelWeight;			// Sample selection current velocity weight
	float m_toiWeight;				// Sample selection TOI weight
	float m_collisionWeight;		// Sample selection collision weight

	virtual void sampleRVO(KX_Obstacle* activeObst, KX_NavMeshObject* activeNavMeshObj, 
							const float maxDeltaAngle) = 0;
public:
	KX_ObstacleSimulationTOI(MT_Scalar levelHeight, bool enableVisualization);
	virtual void AdjustObstacleVelocity(KX_Obstacle* activeObst, KX_NavMeshObject* activeNavMeshObj, 
		MT_Vector3& velocity, MT_Scalar maxDeltaSpeed,MT_Scalar maxDeltaAngle);
};

class KX_ObstacleSimulationTOI_rays: public KX_ObstacleSimulationTOI
{
protected:
	virtual void sampleRVO(KX_Obstacle* activeObst, KX_NavMeshObject* activeNavMeshObj, 
							const float maxDeltaAngle);
public:
	KX_ObstacleSimulationTOI_rays(MT_Scalar levelHeight, bool enableVisualization);
};

class KX_ObstacleSimulationTOI_cells: public KX_ObstacleSimulationTOI
{
protected:
	float m_bias;
	bool m_adaptive;
	int m_sampleRadius;
	virtual void sampleRVO(KX_Obstacle* activeObst, KX_NavMeshObject* activeNavMeshObj, 
							const float maxDeltaAngle);
public:
	KX_ObstacleSimulationTOI_cells(MT_Scalar levelHeight, bool enableVisualization);
};

#endif
