/*  collision.c      
* 
*
* ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
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
* Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
* The Original Code is Copyright (C) Blender Foundation
* All rights reserved.
*
* The Original Code is: all of this file.
*
* Contributor(s): none yet.
*
* ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "MEM_guardedalloc.h"
/* types */
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_cloth_types.h"	
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_lattice_types.h"
#include "DNA_scene_types.h"
#include "DNA_modifier_types.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_edgehash.h"
#include "BLI_linklist.h"
#include "BKE_curve.h"
#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_cloth.h"
#include "BKE_modifier.h"
#include "BKE_utildefines.h"
#include "BKE_DerivedMesh.h"
#include "DNA_screen_types.h"
#include "BSE_headerbuttons.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "mydevice.h"

#include "Bullet-C-Api.h"


#define DERANDOMIZE 1


enum TRIANGLE_MARK 
{ 
	TM_MV = 1,
	TM_ME = 2,
	TM_V1 = 4,
	TM_V2 = 8,
	TM_V3 = 16,
	TM_E1 = 32,
	TM_E2 = 64,
	TM_E3 = 128 
};

DO_INLINE int hasTriangleMark(unsigned char mark, unsigned char bit) { return mark & bit; }
DO_INLINE void setTriangleMark(unsigned char *mark, unsigned char bit) { mark[0] |= bit; }
DO_INLINE void clearTriangleMark(unsigned char *mark, unsigned char bit) { mark[0] &= ~bit; }


void generateTriangleMarks() 
{
	/*
	unsigned int firstEdge = 0;
	
	// 1. Initialization
	memset(m_triangleMarks, 0, sizeof(unsigned char) * m_triangleCount);

	// 2. The Marking Process
	
	// 2.1 Randomly mark triangles for covering vertices.
	for (unsigned int v = 0; v < m_vertexCount; ++v) 
	{
		if (vertexCover(v) == 0) 
		{

			// Randomly select an edge whose first triangle we're going to flag. 

#ifndef DERANDOMIZE
			firstEdge = (unsigned int)((float)(random() & 0x7FFFFFFF) /
					(float)(0x80000000) *
					(float)(m_vertices[v].getEdgeCount()));
#endif
			for (unsigned int ofs = 0; ofs < m_vertices[v].getEdgeCount(); ++ofs) 
			{
				unsigned int edgeIdx = (firstEdge + ofs) % m_vertices[v].getEdgeCount();
				if (m_edges[m_vertices[v].getEdge(edgeIdx)].getTriangleCount())
					setTriangleMark(m_triangleMarks[m_edges[m_vertices[v].getEdge(edgeIdx)].getTriangle(0)], TM_MV);
			}
		}
	}
	*/
	/* If the Cloth is malformed (vertices without adjacent triangles) there might still be uncovered vertices. (Bad luck.) */
	/*
	// 2.2 Randomly mark triangles for covering edges.
	for (unsigned int e = 0; e < m_edgeCount; ++e) 
	{
		if (m_edges[e].getTriangleCount() && (edgeCover(e) == 0)) 
		{
#ifndef DERANDOMIZE
			setTriangleMark(m_triangleMarks[m_edges[e].getTriangle(static_cast<UINT32>((float)(random() & 0x7FFFFFFF) /
					(float)(0x80000000) *
					(float)(m_edges[e].getTriangleCount())))], TM_ME);
#else
			setTriangleMark(m_triangleMarks[m_edges[e].getTriangle(0)], TM_ME);
#endif
		}
	}

	
	// 3. The Unmarking Process
	for (unsigned int t = 0; (t < m_triangleCount); ++t) 
	{
		bool overCoveredVertices = true;
		bool overCoveredEdges = true;
		for (unsigned char i = 0; (i < 3) && (overCoveredVertices || overCoveredEdges); ++i) 
		{

			if (vertexCover(m_triangles[t].getVertex(i)) == 1)
				overCoveredVertices = false;
			if (edgeCover(m_triangles[t].getEdge(i)) == 1)
				overCoveredEdges = false;

			assert(vertexCover(m_triangles[t].getVertex(i)) > 0);
			assert(edgeCover(m_triangles[t].getEdge(i)) > 0);
		}
		if (overCoveredVertices)
			clearTriangleMark(m_triangleMarks[t], TM_MV);
		if (overCoveredEdges)
			clearTriangleMark(m_triangleMarks[t], TM_ME);
	}


	// 4. The Bit Masking Process
	vector<bool> vertexAssigned(m_vertexCount, false);
	vector<bool> edgeAssigned(m_edgeCount, false);
	for (unsigned int t = 0; (t < m_triangleCount); ++t) 
	{
		for (unsigned char i = 0; i < 3; ++i) 
		{
			if (!vertexAssigned[m_triangles[t].getVertex(i)]) 
			{
				vertexAssigned[m_triangles[t].getVertex(i)] = true;
				setTriangleMark(m_triangleMarks[t], 1 << (2 + i));
			}
			if (!edgeAssigned[m_triangles[t].getEdge(i)]) 
			{
				edgeAssigned[m_triangles[t].getEdge(i)] = true;
				setTriangleMark(m_triangleMarks[t], 1 << (5 + i));
			}
		}
	}
	*/
}


void bvh_compute_barycentric (float pv[3], float p1[3], float p2[3], float p3[3], double *w1, double *w2, double *w3)
{
	float	tempV1[3], tempV2[3], tempV4[3];
	double	a,b,c,e,f;

	VECSUB (tempV1, p1, p3);	/* x1 - x3 */
	VECSUB (tempV2, p2, p3);	/* x2 - x3 */
	VECSUB (tempV4, pv, p3);	/* pv - x3 */
	
	a = INPR (tempV1, tempV1);	
	b = INPR (tempV1, tempV2);	
	c = INPR (tempV2, tempV2);	
	e = INPR (tempV1, tempV4);	
	f = INPR (tempV2, tempV4);	
	
	
	w1[0] = (e * c - b * f) / (a * c - b * b);
	w2[0] = (f - b * w1[0]) / c;
	w3[0] = 1.0 - w1[0] - w2[0];
}

DO_INLINE void interpolateOnTriangle(float to[3], float v1[3], float v2[3], float v3[3], double w1, double w2, double w3) 
{
	to[0] = to[1] = to[2] = 0;
	VECADDMUL(to, v1, w1);
	VECADDMUL(to, v2, w2);
	VECADDMUL(to, v3, w3);
}

DO_INLINE void calculateFrictionImpulse(float to[3], float vrel[3], float normal[3], double normalVelocity,
	double frictionConstant, double delta_V_n) 
{
	float vrel_t_pre[3];
	float vrel_t[3];
	VECSUBS(vrel_t_pre, vrel, normal, normalVelocity);
	VECCOPY(vrel_t, vrel_t_pre);
	VecMulf(vrel_t, MAX2(1.0f - frictionConstant * delta_V_n / INPR(vrel_t_pre,vrel_t_pre), 0.0f));
	VECSUB(to, vrel_t_pre, vrel_t);
	VecMulf(to, 1.0f / 2.0f);
}

int collision_static(ClothModifierData *clmd, ClothModifierData *coll_clmd, LinkNode **collision_list)
{
	unsigned int i = 0, numverts=0;
	int result = 0;
	LinkNode *search = NULL;
	CollPair *collpair = NULL;
	Cloth *cloth1, *cloth2;
	MFace *face1, *face2;
	double w1, w2, w3, u1, u2, u3;
	float v1[3], v2[3], relativeVelocity[3];
	float magrelVel;
	
	cloth1 = clmd->clothObject;
	cloth2 = coll_clmd->clothObject;
	
	numverts = clmd->clothObject->numverts;
	
	/*
	for(i = 0; i < LIST_LENGTH; i++)
	{
		// calc SIP-code
		//  TODO for later: calculateSipCode()
	
		// calc distance (?)
		
		// calc impulse
		
		// apply impulse
	}
	*/
	
	for(i = 0; i < numverts; i++)
	{
		search = collision_list[i];
		
		while(search)
		{
			collpair = search->link;
			
			face1 = &(cloth1->mfaces[collpair->face1]);
			face2 = &(cloth2->mfaces[collpair->face2]);
			
			// compute barycentric coordinates for both collision points
			if(!collpair->quadA)
				bvh_compute_barycentric(collpair->p1,
							cloth1->verts[face1->v1].txold,
							cloth1->verts[face1->v2].txold,
        						cloth1->verts[face1->v3].txold, 
       							&w1, &w2, &w3);
			else
				bvh_compute_barycentric(collpair->p1,
							cloth1->verts[face1->v4].txold,
							cloth1->verts[face1->v1].txold,
							cloth1->verts[face1->v3].txold, 
							&w1, &w2, &w3);
			
			if(!collpair->quadB)
				bvh_compute_barycentric(collpair->p2,
							cloth2->verts[face2->v1].txold,
							cloth2->verts[face2->v2].txold,
							cloth2->verts[face2->v3].txold, 
							&u1, &u2, &u3);
			else
				bvh_compute_barycentric(collpair->p2,
							cloth2->verts[face2->v4].txold,
							cloth2->verts[face2->v1].txold,
							cloth2->verts[face2->v3].txold, 
							&u1, &u2, &u3);
			
			// Calculate relative velocity.
			if(!collpair->quadA)
				interpolateOnTriangle(v1, cloth1->verts[face1->v1].v, cloth1->verts[face1->v2].v, cloth1->verts[face1->v3].v, w1, w2, w3);
			else
				interpolateOnTriangle(v1, cloth1->verts[face1->v4].v, cloth1->verts[face1->v1].v, cloth1->verts[face1->v3].v, w1, w2, w3);
			
			if(!collpair->quadB)
				interpolateOnTriangle(v2, cloth2->verts[face2->v1].v, cloth2->verts[face2->v2].v, cloth2->verts[face2->v3].v, u1, u2, u3);
			else
				interpolateOnTriangle(v2, cloth2->verts[face2->v4].v, cloth2->verts[face2->v1].v, cloth2->verts[face2->v3].v, u1, u2, u3);
			
			VECSUB(relativeVelocity, v1, v2);
				
			// Calculate the normal component of the relative velocity (actually only the magnitude - the direction is stored in 'normal').
			magrelVel = INPR(relativeVelocity, collpair->normal);
					
			// Calculate masses of points.
			
			// printf("relativeVelocity -> x: %f, y: %f, z: %f\n", relativeVelocity[0], relativeVelocity[1],relativeVelocity[2]); 
			
			// If v_n_mag > 0 the edges are approaching each other.
			if(magrelVel > ALMOST_ZERO)
			{
				// Calculate Impulse magnitude to stop all motion in normal direction.
				// const double I_mag = v_n_mag / (1/m1 + 1/m2);
				float magnitude_i = magrelVel / 2.0f; // TODO implement masses
				float tangential[3], magtangent;
				
				calculateFrictionImpulse(tangential, relativeVelocity, collpair->normal, magrelVel, clmd->coll_parms.friction*0.01, magrelVel);
				
				magtangent = INPR(tangential, tangential);
				
				// Apply friction impulse.
				if (magtangent > ALMOST_ZERO) 
				{
					/*
					printf("friction applied: %f\n", magtangent);
					// TODO check original code 
					VECSUB(cloth1->verts[face1->v1].tv, cloth1->verts[face1->v1].tv,tangential);
					VECSUB(cloth1->verts[face1->v1].tv, cloth1->verts[face1->v2].tv,tangential);
					VECSUB(cloth1->verts[face1->v1].tv, cloth1->verts[face1->v3].tv,tangential);
					VECSUB(cloth1->verts[face1->v1].tv, cloth1->verts[face1->v4].tv,tangential);
					*/
				}
			
				// printf("magnitude_i: %f\n", magnitude_i); // negative before collision in my case
				
				// Apply the impulse and increase impulse counters.
				/*
				VECADDMUL(cloth1->verts[face1->v1].tv,collpair->normal, -magnitude_i);
				VECADDMUL(cloth1->verts[face1->v2].tv,collpair->normal, -magnitude_i);
				VECADDMUL(cloth1->verts[face1->v3].tv,collpair->normal, -magnitude_i);
				VECADDMUL(cloth1->verts[face1->v4].tv,collpair->normal, -magnitude_i);
				*/
				
				// my try
				magtangent = INPR(cloth1->verts[face1->v1].tv, collpair->normal);
				VECADDMUL(cloth1->verts[face1->v1].tv, collpair->normal, -magtangent); 
				
				magtangent = INPR(cloth1->verts[face1->v2].tv, collpair->normal);
				VECADDMUL(cloth1->verts[face1->v2].tv, collpair->normal, -magtangent); 
				
				magtangent = INPR(cloth1->verts[face1->v3].tv, collpair->normal);
				VECADDMUL(cloth1->verts[face1->v3].tv, collpair->normal, -magtangent); 
				
				magtangent = INPR(cloth1->verts[face1->v4].tv, collpair->normal);
				VECADDMUL(cloth1->verts[face1->v4].tv, collpair->normal, -magtangent); 
				
				result = 1;
				
			}
			
			search = search->next;
		}
	}
		
	return result;
}

// return distance between two triangles using bullet engine
double implicit_tri_check_coherence (ClothModifierData *clmd, ClothModifierData *coll_clmd, unsigned int tri_index1, unsigned int tri_index2, float pa[3], float pb[3], float normal[3], int quadA, int quadB)
{
	MFace *face1=NULL, *face2=NULL;
	float  a[3][3];
	float  b[3][3];
	double distance=0, tempdistance=0;
	Cloth *cloth1=NULL, *cloth2=NULL;
	float tpa[3], tpb[3], tnormal[3];
	unsigned int indexA=0, indexB=0, indexC=0, indexD=0, indexE=0, indexF=0;
	int i = 0;
	
	cloth1 = clmd->clothObject;
	cloth2 = coll_clmd->clothObject;
	
	face1 = &(cloth1->mfaces[tri_index1]);
	face2 = &(cloth2->mfaces[tri_index2]);
	
	// face a1 + face b1
	VECCOPY(a[0], cloth1->verts[face1->v1].txold);
	VECCOPY(a[1], cloth1->verts[face1->v2].txold);
	VECCOPY(a[2], cloth1->verts[face1->v3].txold);
	
	
	VECCOPY(b[0], cloth2->verts[face2->v1].txold);
	VECCOPY(b[1], cloth2->verts[face2->v2].txold);
	VECCOPY(b[2], cloth2->verts[face2->v3].txold);
#pragma omp critical
	distance = plNearestPoints(a,b,pa,pb,normal);
	
	quadA = quadB = 0;
	
	for(i = 0; i < 3; i++)
	{
		if(i == 0)
		{
			indexA = face1->v4;
			indexB = face1->v1;
			indexC = face1->v3;
			
			indexD = face2->v1;
			indexE = face2->v2;
			indexF = face2->v3;
		}
		else if(i == 1)
		{
			indexA = face1->v4;
			indexB = face1->v1;
			indexC = face1->v3;
		
			indexD = face2->v4;
			indexE = face2->v1;
			indexF = face2->v3;
		}
		else if(i == 2)
		{
			indexA = face1->v1;
			indexB = face1->v2;
			indexC = face1->v3;
		
			indexD = face2->v4;
			indexE = face2->v1;
			indexF = face2->v3;
		}
		
		// face a2 + face b1
		VECCOPY(a[0], cloth1->verts[indexA].txold);
		VECCOPY(a[1], cloth1->verts[indexB].txold);
		VECCOPY(a[2], cloth1->verts[indexC].txold);
		
		
		VECCOPY(b[0], cloth2->verts[indexD].txold);
		VECCOPY(b[1], cloth2->verts[indexE].txold);
		VECCOPY(b[2], cloth2->verts[indexF].txold);
#pragma omp critical		
		tempdistance = plNearestPoints(a,b,tpa,tpb,tnormal);
		
		if(tempdistance < distance)
		{
			VECCOPY(pa, tpa);
			VECCOPY(pb, tpb);
			VECCOPY(normal, tnormal);
			distance = tempdistance;
			
			if(i == 0)
			{
				quadA = 1; quadB = 0;
			}
			else if(i == 1)
			{
				quadA = quadB = 1;
			}
			else if(i == 2)
			{
				quadA = 0; quadB = 1;
			}
		}
	}
	return distance;
}

void bvh_collision_response(ClothModifierData *clmd, ClothModifierData *coll_clmd, Tree * tree1, Tree * tree2)
{
	CollPair *collpair = NULL;
	LinkNode **linknode;
	double distance = 0;
	float epsilon = clmd->coll_parms.epsilon;

	collpair = (CollPair *)MEM_callocN(sizeof(CollPair), "cloth coll pair");
	linknode = clmd->coll_parms.temp;
	
	// calc SIPcode (?)
	
	// calc distance + normal 	
	distance = implicit_tri_check_coherence(clmd, coll_clmd, tree1->tri_index, tree2->tri_index, collpair->p1, collpair->p2, collpair->vector, collpair->quadA, collpair->quadB);
	
	if (ABS(distance) <= (epsilon + ALMOST_ZERO))
	{
		// printf("distance: %f, epsilon: %f\n", (float)distance, epsilon + ALMOST_ZERO);
			
		collpair->face1 = tree1->tri_index;
		collpair->face2 = tree2->tri_index;
		
		VECCOPY(collpair->normal, collpair->vector);
		Normalize(collpair->normal);
		
		// printf("normal x: %f, y: %f, z: %f\n", collpair->normal[0], collpair->normal[1], collpair->normal[2]);
		
		collpair->distance = distance;
		BLI_linklist_append(&linknode[tree1->tri_index], collpair);	
	}
	else
	{
		MEM_freeN(collpair);
	}
}


int cloth_bvh_objcollision(ClothModifierData * clmd, float step, CM_COLLISION_RESPONSE collision_response, float dt)
{
	Base *base=NULL;
	ClothModifierData *coll_clmd=NULL;
	Cloth *cloth=NULL;
	Object *coll_ob=NULL;
	BVH *cloth_bvh=NULL;
	unsigned int i=0, numverts=0;
	int result = 0;

	if ((clmd->sim_parms.flags & CSIMSETT_FLAG_COLLOBJ) || !(((Cloth *)clmd->clothObject)->tree))
	{
		return 0;
	}
	cloth = clmd->clothObject;
	cloth_bvh = (BVH *) cloth->tree;
	numverts = clmd->clothObject->numverts;

	////////////////////////////////////////////////////////////
	// static collisions
	////////////////////////////////////////////////////////////

	// update cloth bvh
	bvh_update_static(clmd, cloth_bvh);

	// search all objects for collision object
	for (base = G.scene->base.first; base; base = base->next)
	{

		coll_ob = base->object;
		coll_clmd = (ClothModifierData *) modifiers_findByType (coll_ob, eModifierType_Cloth);
		if (!coll_clmd)
			continue;

		// if collision object go on
		if (coll_clmd->sim_parms.flags & CSIMSETT_FLAG_COLLOBJ)
		{
			if (coll_clmd->clothObject && coll_clmd->clothObject->tree) 
			{			
				unsigned int coll_numverts = coll_clmd->clothObject->numverts;
				Cloth *coll_cloth = coll_clmd->clothObject;

				LinkNode **collision_list = MEM_callocN (sizeof(LinkNode *)*numverts, "collision_list");
				BVH *coll_bvh = coll_clmd->clothObject->tree;

				if(collision_list)
				{					
					// memset(collision_list, 0, sizeof(LinkNode *)*numverts); 
					
					for(i = 0; i < numverts; i++)
					{
						collision_list[i] = NULL;
					}

					clmd->coll_parms.temp = collision_list;
					
					// update position of collision object
					for(i = 0; i < coll_numverts; i++)
					{
						VECCOPY(coll_cloth->verts[i].txold, coll_cloth->verts[i].tx);

						VECADDS(coll_cloth->verts[i].tx, coll_cloth->verts[i].xold, coll_cloth->verts[i].v, step);
						
						VECSUB(coll_cloth->verts[i].tv, coll_cloth->verts[i].tx, coll_cloth->verts[i].txold);
					}
							
					// update BVH of collision object
					bvh_update_static(coll_clmd, coll_bvh);

					bvh_traverse(clmd, coll_clmd, cloth_bvh->root, coll_bvh->root, step, collision_response);
					
					result += collision_static(clmd, coll_clmd, collision_list);
					
					// calculate velocities
					
					// free temporary list 
					for(i = 0; i < numverts; i++)
					{
						LinkNode *search = collision_list[i];
						while(search)
						{
							LinkNode *next= search->next;
							CollPair *collpair = search->link;
							
							if(collpair)
								MEM_freeN(collpair);	

							search = next;
						}

						BLI_linklist_free(collision_list[i],NULL); 
					}
					if(collision_list)
						MEM_freeN(collision_list);

					clmd->coll_parms.temp = NULL;
				}
				

			}
			else
				printf ("cloth_bvh_objcollision: found a collision object with clothObject or collData NULL.\n");
		}
	}

	////////////////////////////////////////////////////////////
	// update positions + velocities
	////////////////////////////////////////////////////////////

	// TODO 


	////////////////////////////////////////////////////////////
	// moving collisions
	////////////////////////////////////////////////////////////

	// TODO 
	// bvh_update_moving(clmd, clmd->clothObject->tree);

	return MIN2(result, 1);
}
