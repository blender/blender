/**
 * $Id:
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Martin Poirier
 *
 * ***** END GPL LICENSE BLOCK *****
 * autoarmature.c: Interface for automagically manipulating armature (retarget, created, ...)
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> 

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "DNA_ID.h"
#include "DNA_armature_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_ghash.h"
#include "BLI_graph.h"
#include "BLI_rand.h"

#include "BDR_editobject.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "BIF_editarmature.h"
#include "BIF_space.h"

#include "PIL_time.h"

#include "mydevice.h"
#include "reeb.h" // FIX ME
#include "blendef.h"

/************ RIG RETARGET DATA STRUCTURES ***************/

struct RigJoint;
struct RigGraph;
struct RigNode;
struct RigArc;
struct RigEdge;

typedef struct RigGraph {
	ListBase	arcs;
	ListBase	nodes;
	
	float length;
	
	FreeArc			free_arc;
	FreeNode		free_node;
	RadialSymmetry	radial_symmetry;
	AxialSymmetry	axial_symmetry;
	/*********************************/

	struct RigNode *head;
	ReebGraph *link_mesh;
} RigGraph;

typedef struct RigNode {
	void *next, *prev;
	float p[3];
	int flag;

	int degree;
	struct BArc **arcs;

	int symmetry_level;
	int symmetry_flag;
	float symmetry_axis[3];
	/*********************************/

	ReebNode *link_mesh;
} RigNode;

typedef struct RigArc {
	void *next, *prev;
	RigNode *head, *tail;
	int flag;

	float length;

	int symmetry_level;
	int symmetry_group;
	int symmetry_flag;
	/*********************************/
	
	ListBase edges;
	int count;
	ReebArc *link_mesh;
} RigArc;

typedef struct RigEdge {
	struct RigEdge *next, *prev;
	float head[3], tail[3];
	float length;
	float angle;
	EditBone *bone;
} RigEdge;

/*******************************************************************************************************/

static void RIG_calculateEdgeAngle(RigEdge *edge_first, RigEdge *edge_second);

/*********************************** EDITBONE UTILS ****************************************************/

int countEditBoneChildren(ListBase *list, EditBone *parent)
{
	EditBone *ebone;
	int count = 0;
	
	for (ebone = list->first; ebone; ebone = ebone->next)
	{
		if (ebone->parent == parent)
		{
			count++;
		}
	}
	
	return count;
}

EditBone* nextEditBoneChild(ListBase *list, EditBone *parent, int n)
{
	EditBone *ebone;
	
	for (ebone = list->first; ebone; ebone = ebone->next)
	{
		if (ebone->parent == parent)
		{
			if (n == 0)
			{
				return ebone;
			}
			n--;
		}
	}
	
	return NULL;
}


/************************************ DESTRUCTORS ******************************************************/

void RIG_freeRigArc(BArc *arc)
{
	BLI_freelistN(&((RigArc*)arc)->edges);
}

void RIG_freeRigGraph(BGraph *rg)
{
	BNode *node;
	BArc *arc;
	
	for (arc = rg->arcs.first; arc; arc = arc->next)
	{
		RIG_freeRigArc(arc);
	}
	BLI_freelistN(&rg->arcs);
	
	for (node = rg->nodes.first; node; node = node->next)
	{
		BLI_freeNode((BGraph*)rg, (BNode*)node);
	}
	BLI_freelistN(&rg->nodes);
	
	MEM_freeN(rg);
}

/************************************* ALLOCATORS ******************************************************/

static RigGraph *newRigGraph()
{
	RigGraph *rg;
	rg = MEM_callocN(sizeof(RigGraph), "rig graph");
	
	rg->head = NULL;
	
	rg->free_arc = RIG_freeRigArc;
	rg->free_node = NULL;
	
	return rg;
}

static RigArc *newRigArc(RigGraph *rg)
{
	RigArc *arc;
	
	arc = MEM_callocN(sizeof(RigArc), "rig arc");
	arc->count = 0;
	
	BLI_addtail(&rg->arcs, arc);
	
	return arc;
}

static RigNode *newRigNodeHead(RigGraph *rg, RigArc *arc, float p[3])
{
	RigNode *node;
	node = MEM_callocN(sizeof(RigNode), "rig node");
	BLI_addtail(&rg->nodes, node);

	VECCOPY(node->p, p);
	node->degree = 1;
	node->arcs = NULL;
	
	arc->head = node;
	
	return node;
}

static void addRigNodeHead(RigGraph *rg, RigArc *arc, RigNode *node)
{
	node->degree++;

	arc->head = node;
}

static RigNode *newRigNodeTail(RigGraph *rg, RigArc *arc, float p[3])
{
	RigNode *node;
	node = MEM_callocN(sizeof(RigNode), "rig node");
	BLI_addtail(&rg->nodes, node);

	VECCOPY(node->p, p);
	node->degree = 1;
	node->arcs = NULL;
	
	arc->tail = node;

	return node;
}

static void RIG_addEdgeToArc(RigArc *arc, float tail[3], EditBone *bone)
{
	RigEdge *edge;

	edge = MEM_callocN(sizeof(RigEdge), "rig edge");
	BLI_addtail(&arc->edges, edge);


	VECCOPY(edge->tail, tail);
	edge->bone = bone;
	
	if (edge->prev == NULL)
	{
		VECCOPY(edge->head, arc->head->p);
	}
	else
	{
		RigEdge *last_edge = edge->prev;
		VECCOPY(edge->head, last_edge->tail);
		RIG_calculateEdgeAngle(last_edge, edge);
	}
	
	edge->length = VecLenf(edge->head, edge->tail);
	
	arc->length += edge->length;
	
	arc->count += 1;
}


/*******************************************************************************************************/

static void RIG_calculateEdgeAngle(RigEdge *edge_first, RigEdge *edge_second)
{
	float vec_first[3], vec_second[3];
	
	VecSubf(vec_first, edge_first->tail, edge_first->head); 
	VecSubf(vec_second, edge_second->tail, edge_second->head);

	Normalize(vec_first);
	Normalize(vec_second);
	
	edge_first->angle = saacos(Inpf(vec_first, vec_second));
}

/*******************************************************************************************************/

static void RIG_arcFromBoneChain(RigGraph *rg, ListBase *list, EditBone *root_bone, RigNode *starting_node)
{
	EditBone *bone, *last_bone = NULL;
	RigArc *arc;
	int contain_head = 0;
	
	arc = newRigArc(rg);
	
	if (starting_node == NULL)
	{
		starting_node = newRigNodeHead(rg, arc, root_bone->head);
	}
	else
	{
		addRigNodeHead(rg, arc, starting_node);
	}
	
	for(bone = root_bone; bone; bone = nextEditBoneChild(list, bone, 0))
	{
		int nb_children;
		
		if (bone->parent && (bone->flag & BONE_CONNECTED) == 0)
		{
			RIG_addEdgeToArc(arc, bone->head, NULL);
		}
		
		RIG_addEdgeToArc(arc, bone->tail, bone);
		
		if (strcmp(bone->name, "head") == 0)
		{
			contain_head = 1;
		}
		
		nb_children = countEditBoneChildren(list, bone);
		if (nb_children > 1)
		{
			RigNode *end_node = newRigNodeTail(rg, arc, bone->tail);
			int i;
			
			for (i = 0; i < nb_children; i++)
			{
				root_bone = nextEditBoneChild(list, bone, i);
				RIG_arcFromBoneChain(rg, list, root_bone, end_node);
			}
			
			/* arc ends here, break */
			break;
		}
		last_bone = bone;
	}
	
	/* If the loop exited without forking */
	if (bone == NULL)
	{
		newRigNodeTail(rg, arc, last_bone->tail);
	}

	if (contain_head)
	{
		rg->head = arc->tail;
	}
}

/*******************************************************************************************************/
static void RIG_findHead(RigGraph *rg)
{
	if (rg->head == NULL)
	{
		if (BLI_countlist(&rg->arcs) == 1)
		{
			RigArc *arc = rg->arcs.first;
			
			rg->head = (RigNode*)arc->head;
		}
		else
		{
			RigArc *arc;
			
			for (arc = rg->arcs.first; arc; arc = arc->next)
			{
				RigEdge *edge = arc->edges.last;
				
				if (edge->bone->flag & (BONE_TIPSEL|BONE_SELECTED))
				{
					rg->head = arc->tail;
					break;
				}
			}
		}
		
		if (rg->head == NULL)
		{
			rg->head = rg->nodes.first;
		}
	}
}

/*******************************************************************************************************/

void RIG_printNode(RigNode *node, char name[])
{
	printf("%s %p %i <%0.3f, %0.3f, %0.3f>\n", name, node, node->degree, node->p[0], node->p[1], node->p[2]);
	
	if (node->symmetry_flag & SYM_TOPOLOGICAL)
	{
		if (node->symmetry_flag & SYM_AXIAL)
			printf("Symmetry AXIAL\n");
		else if (node->symmetry_flag & SYM_RADIAL)
			printf("Symmetry RADIAL\n");
			
		printvecf("symmetry axis", node->symmetry_axis);
	}
}

void RIG_printArcBones(RigArc *arc)
{
	RigEdge *edge;

	for (edge = arc->edges.first; edge; edge = edge->next)
	{
		if (edge->bone)
			printf("%s ", edge->bone->name);
		else
			printf("---- ");
	}
	printf("\n");
}

void RIG_printArc(RigArc *arc)
{
	RigEdge *edge;

	printf("\n");

	RIG_printNode((RigNode*)arc->head, "head");

	for (edge = arc->edges.first; edge; edge = edge->next)
	{
		printf("\tinner joints %0.3f %0.3f %0.3f\n", edge->tail[0], edge->tail[1], edge->tail[2]);
		printf("\t\tlength %f\n", edge->length);
		printf("\t\tangle %f\n", edge->angle * 180 / M_PI);
		if (edge->bone)
			printf("\t\t%s\n", edge->bone->name);
	}	
	printf("symmetry level: %i\n", arc->symmetry_level);

	RIG_printNode((RigNode*)arc->tail, "tail");
}

void RIG_printGraph(RigGraph *rg)
{
	RigArc *arc;

	for (arc = rg->arcs.first; arc; arc = arc->next)
	{
		RIG_printArc(arc);	
	}
	
	if (rg->head)
	{
		RIG_printNode(rg->head, "HEAD NODE:");
	}
	else
	{
		printf("HEAD NODE: NONE\n");
	}	
}

/*******************************************************************************************************/

static RigGraph *armatureToGraph(ListBase *list)
{
	EditBone *ebone;
	RigGraph *rg;
 	
	rg = newRigGraph();

	/* Do the rotations */
	for (ebone = list->first; ebone; ebone=ebone->next){
		if (ebone->parent == NULL)
		{
			RIG_arcFromBoneChain(rg, list, ebone, NULL);
		}
	}
	
	BLI_removeDoubleNodes((BGraph*)rg, 0.001);
	
	BLI_buildAdjacencyList((BGraph*)rg);
	
	RIG_findHead(rg);
	
	return rg;
}

/************************************ RETARGETTING *****************************************************/

typedef enum 
{
	RETARGET_LENGTH,
	RETARGET_AGGRESSIVE
} RetargetMode; 

static RetargetMode detectArcRetargetMode(RigArc *arc);
static void retargetArctoArcLength(RigArc *iarc);


static RetargetMode detectArcRetargetMode(RigArc *iarc)
{
	RetargetMode mode = RETARGET_AGGRESSIVE;
	ReebArc *earc = iarc->link_mesh;
	RigEdge *edge;
	int large_angle = 0;
	float avg_angle = 0;
	float avg_length = 0;
	int nb_edges = 0;
	
	
	for (edge = iarc->edges.first; edge; edge = edge->next)
	{
		avg_angle += edge->angle;
		nb_edges++;
	}
	
	avg_angle /= nb_edges - 1; /* -1 because last edge doesn't have an angle */

	avg_length = iarc->length / nb_edges;
	
	
	if (nb_edges > 2)
	{
		for (edge = iarc->edges.first; edge; edge = edge->next)
		{
			if (fabs(edge->angle - avg_angle) > M_PI / 6)
			{
				large_angle = 1;
			}
		}
	}
	else if (nb_edges == 2 && avg_angle > 0)
	{
		large_angle = 1;
	}
		
	
	if (large_angle == 0)
	{
		mode = RETARGET_LENGTH;
	}
	
	if (earc->bcount <= (iarc->count - 1))
	{
		mode = RETARGET_LENGTH;
	}
	
	mode = RETARGET_AGGRESSIVE;
	
	return mode;
}

static void printPositions(int *positions, int nb_positions)
{
	int i;
	
	for (i = 0; i < nb_positions; i++)
	{
		printf("%i ", positions[i]);
	}
	printf("\n");
}

#define MAX_COST 100 /* FIX ME */

static float costDistance(ReebArcIterator *iter, float *vec0, float *vec1, int i0, int i1)
{
	EmbedBucket *bucket = NULL;
	float max_dist = 0;
	float v1[3], v2[3], c[3];
	float v1_inpf;

	if (G.scene->toolsettings->skgen_retarget_distance_weight > 0)
	{
		VecSubf(v1, vec0, vec1);
		
		v1_inpf = Inpf(v1, v1);
		
		if (v1_inpf > 0)
		{
			int j;
			for (j = i0 + 1; j < i1 - 1; j++)
			{
				float dist;
				
				bucket = peekBucket(iter, j);
	
				VecSubf(v2, bucket->p, vec1);
		
				Crossf(c, v1, v2);
				
				dist = Inpf(c, c) / v1_inpf;
				
				max_dist = dist > max_dist ? dist : max_dist;
			}
			
			return max_dist;
		}
		else
		{
			return MAX_COST;
		}
		
		return G.scene->toolsettings->skgen_retarget_distance_weight * max_dist;
	}
	else
	{
		return 0;
	}
}

static float costAngle(float original_angle, float vec_first[3], float vec_second[3], float length1, float length2)
{
	if (G.scene->toolsettings->skgen_retarget_angle_weight > 0)
	{
		float current_angle;
		
		if (length1 > 0 && length2 > 0)
		{
			current_angle = saacos(Inpf(vec_first, vec_second));

			if (original_angle > 0)
			{
				return G.scene->toolsettings->skgen_retarget_angle_weight * fabs((current_angle - original_angle) / original_angle);
			}
			else
			{
				return G.scene->toolsettings->skgen_retarget_angle_weight * fabs(current_angle);
			}
		}
		else
		{
			return G.scene->toolsettings->skgen_retarget_angle_weight * M_PI;
		}
	}
	else
	{
		return 0;
	}
}

static float costLength(float original_length, float current_length)
{
	if (current_length == 0)
	{
		return MAX_COST;
	}
	else
	{
		float length_ratio = fabs((current_length - original_length) / original_length);
		return G.scene->toolsettings->skgen_retarget_length_weight * length_ratio * length_ratio;
	}
}

static float calcCost(ReebArcIterator *iter, RigEdge *e1, RigEdge *e2, float *vec0, float *vec1, float *vec2, int i0, int i1, int i2)
{
	float vec_second[3], vec_first[3];
	float length1, length2;
	float new_cost = 0;

	VecSubf(vec_second, vec2, vec1);
	length2 = Normalize(vec_second);

	VecSubf(vec_first, vec1, vec0); 
	length1 = Normalize(vec_first);

	/* Angle cost */	
	new_cost += costAngle(e1->angle, vec_first, vec_second, length1, length2);

	/* Length cost */
	new_cost += costLength(e1->length, length1);
	new_cost += costLength(e2->length, length2);

	/* Distance cost */
	new_cost += costDistance(iter, vec0, vec1, i0, i1);
	new_cost += costDistance(iter, vec1, vec2, i1, i2);

	return new_cost;
}

static void calcGradient(RigEdge *e1, RigEdge *e2, ReebArcIterator *iter, int index, int nb_joints, float *cost_cube, int *positions, float **vec_cache)
{
	EmbedBucket *bucket = NULL;
	float *vec0, *vec1, *vec2;
	float current_cost;
	int i0, i1, i2;
	int next_position;

	vec0 = vec_cache[index];
	vec1 = vec_cache[index + 1];
	vec2 = vec_cache[index + 2];
	
	if (index == 0)
	{
		i0 = iter->start;
	}
	else
	{
		i0 = positions[index - 1];
	}
	
	i1 = positions[index];
	
	if (index +1 == nb_joints)
	{
		i2 = iter->end;
	}
	else
	{
		i2 = positions[index + 1];
	}


	current_cost = calcCost(iter, e1, e2, vec0, vec1, vec2, i0, i1, i2);
	cost_cube[index * 3 + 1] = current_cost;
	
	next_position = positions[index] + 1;
	
	if (index + 1 < nb_joints && next_position == positions[index + 1])
	{
		cost_cube[index * 3 + 2] = MAX_COST;
	}
	else
	{
		bucket = peekBucket(iter, next_position);
		
		if (bucket == NULL)
		{
			cost_cube[index * 3 + 2] = MAX_COST;
		}
		else
		{
			vec1 = bucket->p;
			
			cost_cube[index * 3 + 2] = calcCost(iter, e1, e2, vec0, vec1, vec2, i0, next_position, i2) - current_cost;
		}
	}

	next_position = positions[index] - 1;
	
	if (index - 1 > -1 && next_position == positions[index - 1])
	{
		cost_cube[index * 3] = MAX_COST;
	}
	else
	{
		bucket = peekBucket(iter, next_position);
		
		if (bucket == NULL)
		{
			cost_cube[index * 3] = MAX_COST;
		}
		else
		{
			vec1 = bucket->p;
			
			cost_cube[index * 3] = calcCost(iter, e1, e2, vec0, vec1, vec2, i0, next_position, i2) - current_cost;
		}
	}
}

static float probability(float delta_cost, float temperature)
{
	if (delta_cost < 0)
	{
		return 1;
	}
	else
	{
		return (float)exp(delta_cost) * temperature;
	}
}

static int neighbour(int nb_joints, float *cost_cube, int *moving_joint, int *moving_direction)
{
	int total = 0;
	int chosen = 0;
	int i;
	
	for (i = 0; i < nb_joints; i++)
	{
		if (cost_cube[i * 3] < MAX_COST)
		{
			total++;
		}
		
		if (cost_cube[i * 3 + 2] < MAX_COST)
		{
			total++;
		}
	}
	
	if (total == 0)
	{
		return 0;
	}
	
	chosen = (int)(BLI_drand() * total);
	
	for (i = 0; i < nb_joints; i++)
	{
		if (cost_cube[i * 3] < MAX_COST)
		{
			if (chosen == 0)
			{
				*moving_joint = i;
				*moving_direction = -1;
				break;
			}
			chosen--;
		}
		
		if (cost_cube[i * 3 + 2] < MAX_COST)
		{
			if (chosen == 0)
			{
				*moving_joint = i;
				*moving_direction = 1;
				break;
			}
			chosen--;
		}
	}
	
	return 1;
}

static void retargetArctoArcAggresive(RigArc *iarc)
{
	ReebArcIterator iter;
	RigEdge *edge;
	EmbedBucket *bucket = NULL;
	ReebNode *node_start, *node_end;
	ReebArc *earc = iarc->link_mesh;
	float min_cost = FLT_MAX;
	float *vec0, *vec1, *vec2;
	float **vec_cache;
	float *cost_cache;
	int *best_positions;
	int *positions;
	int nb_edges = BLI_countlist(&iarc->edges);
	int nb_joints = nb_edges - 1;
	int symmetry_axis = 0;
	int last_index = 0;
	int first_pass = 1;
	int must_move = nb_joints - 1;
	int i;
	
	printf("aggressive\n");

	positions = MEM_callocN(sizeof(int) * nb_joints, "Aggresive positions");
	best_positions = MEM_callocN(sizeof(int) * nb_joints, "Best Aggresive positions");
	cost_cache = MEM_callocN(sizeof(float) * nb_edges, "Cost cache");
	vec_cache = MEM_callocN(sizeof(float*) * (nb_edges + 1), "Vec cache");
	
	/* symmetry axis */
	if (earc->symmetry_level == 1 && iarc->symmetry_level == 1)
	{
		symmetry_axis = 1;
		node_start = earc->tail;
		node_end = earc->head;
	}
	else
	{
		node_start = earc->head;
		node_end = earc->tail;
	}
	
	/* init with first values */
	for (i = 0; i < nb_joints; i++)
	{
		positions[i] = i + 1;
		//positions[i] = (earc->bcount / nb_edges) * (i + 1);
	}
	
	/* init cost cache */
	for (i = 0; i < nb_edges; i++)
	{
		cost_cache[i] = 0;
	}
	
	vec_cache[0] = node_start->p;
	vec_cache[nb_edges] = node_end->p;

	/* BRUTE FORCE */
	if (G.scene->toolsettings->skgen_optimisation_method == 0)
	{
		while(1)
		{
			float cost = 0;
			int need_calc = 0;
			
			/* increment to next possible solution */
			
			i = nb_joints - 1;
	
			if (first_pass)
			{
				need_calc = 0;
				first_pass = 0;
			}
			else
			{
				/* increment positions, starting from the last one
				 * until a valid increment is found
				 * */
				for (i = must_move; i >= 0; i--)
				{
					int remaining_joints = nb_joints - (i + 1); 
					
					positions[i] += 1;
					need_calc = i;
					
					if (positions[i] + remaining_joints <= earc->bcount)
					{
						break;
					}
				}
			}
	
			if (i == -1)
			{
				break;
			}
			
			/* reset joints following the last increment*/
			for (i = i + 1; i < nb_joints; i++)
			{
				positions[i] = positions[i - 1] + 1;
			}
		
			/* calculating cost */
			initArcIterator(&iter, earc, node_start);
			
			vec0 = NULL;
			vec1 = node_start->p;
			vec2 = NULL;
			
			for (edge = iarc->edges.first, i = 0, last_index = 0;
				 edge;
				 edge = edge->next, i += 1)
			{
	
				if (i >= need_calc)
				{ 
					float vec_first[3], vec_second[3];
					float length1, length2;
					float new_cost = 0;
					int i1, i2;
					
					if (i < nb_joints)
					{
						i2 = positions[i];
						bucket = peekBucket(&iter, positions[i]);
						vec2 = bucket->p;
						vec_cache[i + 1] = vec2; /* update cache for updated position */
					}
					else
					{
						i2 = iter.length;
						vec2 = node_end->p;
					}
					
					if (i > 0)
					{
						i1 = positions[i - 1];
					}
					else
					{
						i1 = 1;
					}
					
					vec1 = vec_cache[i];
					
	
					VecSubf(vec_second, vec2, vec1);
					length2 = Normalize(vec_second);
		
					/* check angle */
					if (i != 0 && G.scene->toolsettings->skgen_retarget_angle_weight > 0)
					{
						RigEdge *previous = edge->prev;
						
						vec0 = vec_cache[i - 1];
						VecSubf(vec_first, vec1, vec0); 
						length1 = Normalize(vec_first);
						
						/* Angle cost */	
						new_cost += costAngle(previous->angle, vec_first, vec_second, length1, length2);
					}
		
					/* Length Cost */
					new_cost += costLength(edge->length, length2);
					
					/* Distance Cost */
					new_cost += costDistance(&iter, vec1, vec2, i1, i2);
					
					cost_cache[i] = new_cost;
				}
				
				cost += cost_cache[i];
				
				if (cost > min_cost)
				{
					must_move = i;
					break;
				}
			}
			
			if (must_move != i || must_move > nb_joints - 1)
			{
				must_move = nb_joints - 1;
			}
	
			/* cost optimizing */
			if (cost < min_cost)
			{
				min_cost = cost;
				memcpy(best_positions, positions, sizeof(int) * nb_joints);
			}
		}
	}
	/* SIMULATED ANNEALING */
#define ANNEALING_ITERATION
	else if (G.scene->toolsettings->skgen_optimisation_method == 1)
	{
		RigEdge *previous;
		float *cost_cube;
#ifdef ANNEALING_ITERATION
		int k, kmax = 100000;
#else
		double time_start, time_current, time_length = 3;
		int k;
#endif
		
		BLI_srand(nb_joints);
		
		/* [joint: index][position: -1, 0, +1] */
		cost_cube = MEM_callocN(sizeof(float) * 3 * nb_joints, "Cost Cube");
		
		initArcIterator(&iter, earc, node_start);

		/* init vec_cache */
		for (i = 0; i < nb_joints; i++)
		{
			bucket = peekBucket(&iter, positions[i]);
			vec_cache[i + 1] = bucket->p;
		}
		
		min_cost = 0;

		/* init cost cube */
		for (previous = iarc->edges.first, edge = previous->next, i = 0;
			 edge;
			 previous = edge, edge = edge->next, i += 1)
		{
			calcGradient(previous, edge, &iter, i, nb_joints, cost_cube, positions, vec_cache);
			
			min_cost += cost_cube[3 * i + 1];
		}
		
		printf("initial cost: %f\n", min_cost);
		
#ifdef ANNEALING_ITERATION
		for (k = 0; k < kmax; k++)
#else
		for (time_start = PIL_check_seconds_timer(), time_current = time_start, k = 0;
			 time_current - time_start < time_length;
			 time_current = PIL_check_seconds_timer(), k++)
#endif
		{
			int status;
			int moving_joint = -1;
			int move_direction = -1;
			float delta_cost;
			float temperature;
			
			status = neighbour(nb_joints, cost_cube, &moving_joint, &move_direction);
			
			if (status == 0)
			{
				break;
			}
			
			delta_cost = cost_cube[moving_joint * 3 + (1 + move_direction)];

#ifdef ANNEALING_ITERATION
			temperature = 1 - (float)k / (float)kmax;
#else
			temperature = 1 - (float)((time_current - time_start) / time_length);
			temperature = temperature * temperature;
#endif
			if (probability(delta_cost, temperature) > BLI_frand())
			{
				/* update position */			
				positions[moving_joint] += move_direction;
				
				/* update vector cache */
				bucket = peekBucket(&iter, positions[moving_joint]);
				vec_cache[moving_joint + 1] = bucket->p;
				
				min_cost += delta_cost;

				//printf("%i: %0.3f\n", k, delta_cost);
	
				/* update cost cube */			
				for (previous = iarc->edges.first, edge = previous->next, i = 0;
					 edge;
					 previous = edge, edge = edge->next, i += 1)
				{
					if (i == moving_joint - 1 ||
						i == moving_joint ||
						i == moving_joint + 1)
					{
						calcGradient(previous, edge, &iter, i, nb_joints, cost_cube, positions, vec_cache);
					}
				}
			}
		}
		
		printf("k = %i\n", k);
		
		memcpy(best_positions, positions, sizeof(int) * nb_joints);
		
		MEM_freeN(cost_cube);
	}	
	/* GRADIENT DESCENT*/
	else if (G.scene->toolsettings->skgen_optimisation_method == 2)
	{
		RigEdge *previous;
		float *cost_cube;
		
		/* [joint: index][position: -1, 0, +1] */
		cost_cube = MEM_callocN(sizeof(float) * 3 * nb_joints, "Cost Cube");
		
		initArcIterator(&iter, earc, node_start);

		/* init vec_cache */
		for (i = 0; i < nb_joints; i++)
		{
			bucket = peekBucket(&iter, positions[i]);
			vec_cache[i + 1] = bucket->p;
		}

		/* init cost cube */
		for (previous = iarc->edges.first, edge = previous->next, i = 0;
			 edge;
			 previous = edge, edge = edge->next, i += 1)
		{
			calcGradient(previous, edge, &iter, i, nb_joints, cost_cube, positions, vec_cache);
		}

		while(1)
		{
			float min_gradient = 0;
			int moving_joint = -1;
			int move_direction = -1;
		
			printf("-----------------\n");
		
			for (i = 0; i < nb_joints; i++)
			{
				printf("%i[%i]: %f\t\t(%f)\t\t%f\n", i, positions[i], cost_cube[i * 3], cost_cube[i * 3 + 1], cost_cube[i * 3 + 2]); 
				if (cost_cube[i * 3] < min_gradient)
				{
					min_gradient = cost_cube[i * 3]; 
					moving_joint = i;
					move_direction = -1;
				}
				
				if  (cost_cube[i * 3 + 2] < min_gradient)
				{
					min_gradient = cost_cube[i * 3 + 2]; 
					moving_joint = i;
					move_direction = 1;
				}
			}
			
			if (moving_joint == -1)
			{
				break;
			}
			
			positions[moving_joint] += move_direction;
			
			/* update vector cache */
			bucket = peekBucket(&iter, positions[moving_joint]);
			vec_cache[moving_joint + 1] = bucket->p;

			/* update cost cube */			
			for (previous = iarc->edges.first, edge = previous->next, i = 0;
				 edge;
				 previous = edge, edge = edge->next, i += 1)
			{
				if (i == moving_joint - 1 ||
					i == moving_joint ||
					i == moving_joint + 1)
				{
					calcGradient(previous, edge, &iter, i, nb_joints, cost_cube, positions, vec_cache);
				}
			}
			
			
		}
		
		memcpy(best_positions, positions, sizeof(int) * nb_joints);
		
		MEM_freeN(cost_cube);
	}

	vec0 = node_start->p;
	initArcIterator(&iter, earc, node_start);
	
	printPositions(best_positions, nb_joints);
	printf("min_cost %f\n", min_cost);
	printf("buckets: %i\n", earc->bcount);

	/* set joints to best position */
	for (edge = iarc->edges.first, i = 0, last_index = 0;
		 edge;
		 edge = edge->next, i++)
	{
		EditBone *bone = edge->bone;
		
		if (i < nb_joints)
		{
			bucket = peekBucket(&iter, best_positions[i]);
			vec1 = bucket->p;
		}
		else
		{
			vec1 = node_end->p;
		}
		
		if (bone)
		{
			VECCOPY(bone->head, vec0);
			VECCOPY(bone->tail, vec1);
			printf("===\n");
			printvecf("vec0", vec0);
			printvecf("vec1", vec1);
			if (i < nb_joints)
				printf("position: %i\n", best_positions[i]);
			printf("last_index: %i\n", last_index);
		}
		
		vec0 = vec1;
	}
	
	MEM_freeN(positions);
	MEM_freeN(best_positions);
	MEM_freeN(cost_cache);
	MEM_freeN(vec_cache);
}

static void retargetArctoArcLength(RigArc *iarc)
{
	ReebArcIterator iter;
	ReebArc *earc = iarc->link_mesh;
	ReebNode *node_start, *node_end;
	RigEdge *edge;
	EmbedBucket *bucket = NULL;
	float embedding_length = 0;
	float *vec0 = NULL;
	float *vec1 = NULL;
	float *previous_vec = NULL;
	int symmetry_axis = 0;

	
	/* symmetry axis */
	if (earc->symmetry_level == 1 && iarc->symmetry_level == 1)
	{
		symmetry_axis = 1;
		node_start = (ReebNode*)earc->tail;
		node_end = (ReebNode*)earc->head;
	}
	else
	{
		node_start = (ReebNode*)earc->head;
		node_end = (ReebNode*)earc->tail;
	}
	
	initArcIterator(&iter, earc, node_start);

	bucket = nextBucket(&iter);
	
	vec0 = node_start->p;
	
	while (bucket != NULL)
	{
		vec1 = bucket->p;
		
		embedding_length += VecLenf(vec0, vec1);
		
		vec0 = vec1;
		bucket = nextBucket(&iter);
	}
	
	embedding_length += VecLenf(node_end->p, vec1);
	
	/* fit bones */
	initArcIterator(&iter, earc, node_start);

	bucket = nextBucket(&iter);

	vec0 = node_start->p;
	previous_vec = vec0;
	vec1 = bucket->p;
	
	printf("arc: %f embedding %f\n",  iarc->length, embedding_length);
	
	for (edge = iarc->edges.first; edge; edge = edge->next)
	{
		EditBone *bone = edge->bone;
		float new_bone_length = edge->length / iarc->length * embedding_length;

#if 0		
		while (bucket && new_bone_length > VecLenf(vec0, vec1))
		{
			bucket = nextBucket(&iter);
			previous_vec = vec1;
			vec1 = bucket->p;
		}
		
		if (bucket == NULL)
		{
			vec1 = node_end->p;
		}
		
		if (embedding_length < VecLenf(vec0, vec1))
		{
			float dv[3], off[3];
			float a, b, c, f;
			
			/* Solve quadratic distance equation */
			VecSubf(dv, vec1, previous_vec);
			a = Inpf(dv, dv);
			
			VecSubf(off, previous_vec, vec0);
			b = 2 * Inpf(dv, off);
			
			c = Inpf(off, off) - (new_bone_length * new_bone_length);
			
			f = (-b + (float)sqrt(b * b - 4 * a * c)) / (2 * a);
			
			if (isnan(f) == 0 && f < 1.0f)
			{
				VECCOPY(vec1, dv);
				VecMulf(vec1, f);
				VecAddf(vec1,vec1, vec0);
			}
		}
#else
		float length = 0;

		while (bucket && new_bone_length > length)
		{
			length += VecLenf(previous_vec, vec1);
			bucket = nextBucket(&iter);
			previous_vec = vec1;
			vec1 = bucket->p;
		}
		
		if (bucket == NULL)
		{
			vec1 = node_end->p;
		}
#endif

		/* no need to move virtual edges (space between unconnected bones) */		
		if (bone)
		{
			printf("BONE: %s\n", bone->name);
			VECCOPY(bone->head, vec0);
			VECCOPY(bone->tail, vec1);
		}
		printvecf("vec0", vec0);
		printvecf("vec1", vec1);
		printf("old: %f target: %f new: %f\n", edge->length, new_bone_length, VecLenf(vec0, vec1));
		
		vec0 = vec1;
		previous_vec = vec1;
	}
}

static void retargetArctoArc(RigArc *iarc)
{
	ReebArc *earc = iarc->link_mesh;
	
	if (BLI_countlist(&iarc->edges) == 1)
	{
		RigEdge *edge = iarc->edges.first;
		EditBone *bone = edge->bone;
		
		/* symmetry axis */
		if (earc->symmetry_level == 1 && iarc->symmetry_level == 1)
		{
			VECCOPY(bone->head, earc->tail->p);
			VECCOPY(bone->tail, earc->head->p);
		}
		/* or not */
		else
		{
			VECCOPY(bone->head, earc->head->p);
			VECCOPY(bone->tail, earc->tail->p);
		}
	}
	else
	{
		RetargetMode mode = detectArcRetargetMode(iarc);
		
		if (mode == RETARGET_AGGRESSIVE)
		{
			retargetArctoArcAggresive(iarc);
		}
		else
		{		
			retargetArctoArcLength(iarc);
		}
	}
}

static void matchMultiResolutionArc(RigNode *start_node, RigArc *next_iarc, ReebArc *next_earc)
{
	ReebNode *enode = next_earc->head;
	int ishape, eshape;
	int shape_levels = SHAPE_RADIX * SHAPE_RADIX; /* two levels */

	ishape = BLI_subtreeShape((BNode*)start_node, (BArc*)next_iarc, 1) % shape_levels;
	eshape = BLI_subtreeShape((BNode*)enode, (BArc*)next_earc, 1) % shape_levels;
	
	while (ishape != eshape && next_earc->link_up)
	{
		next_earc->flag = 1; // mark previous as taken, to prevent backtrack on lower levels
		
		next_earc = next_earc->link_up;
		enode = next_earc->head;
		eshape = BLI_subtreeShape((BNode*)enode, (BArc*)next_earc, 1) % shape_levels;
	} 

	next_earc->flag = 1; // mark as taken
	next_iarc->link_mesh = next_earc;
}

static void matchMultiResolutionStartingNode(ReebGraph *reebg, RigNode *inode)
{
	ReebNode *enode;
	int ishape, eshape;
	int shape_levels = SHAPE_RADIX * SHAPE_RADIX; /* two levels */
	
	enode = reebg->nodes.first;
	
	ishape = BLI_subtreeShape((BNode*)inode, NULL, 0) % shape_levels;
	eshape = BLI_subtreeShape((BNode*)enode, NULL, 0) % shape_levels;
	
	while (ishape != eshape && reebg->link_up)
	{
		reebg = reebg->link_up;
		
		enode = reebg->nodes.first;
		
		eshape = BLI_subtreeShape((BNode*)enode, NULL, 0) % shape_levels;
	} 

	inode->link_mesh = enode;
}

static void findCorrespondingArc(RigArc *start_arc, RigNode *start_node, RigArc *next_iarc)
{
	ReebNode *enode = start_node->link_mesh;
	ReebArc *next_earc;
	int symmetry_level = next_iarc->symmetry_level;
	int symmetry_group = next_iarc->symmetry_group;
	int symmetry_flag = next_iarc->symmetry_flag;
	int i;
	
	next_iarc->link_mesh = NULL;
		
	for(i = 0; i < enode->degree; i++)
	{
		next_earc = (ReebArc*)enode->arcs[i];
		if (next_earc->flag == 0 && /* not already taken */
			next_earc->symmetry_flag == symmetry_flag &&
			next_earc->symmetry_group == symmetry_group &&
			next_earc->symmetry_level == symmetry_level)
		{
			printf("-----------------------\n");
			printf("CORRESPONDING ARC FOUND\n");
			RIG_printArcBones(next_iarc);
			printf("flag %i -- symmetry level %i -- symmetry flag %i\n", next_earc->flag, next_earc->symmetry_level, next_earc->symmetry_flag);
			
			matchMultiResolutionArc(start_node, next_iarc, next_earc);
			break;
		}
	}
	
	/* not found, try at higher nodes (lower node might have filtered internal arcs, messing shape of tree */
	if (next_iarc->link_mesh == NULL)
	{
		if (enode->link_up)
		{
			start_node->link_mesh = enode->link_up;
			findCorrespondingArc(start_arc, start_node, next_iarc);
		}
	}

	/* still not found, print debug info */
	if (next_iarc->link_mesh == NULL)
	{
		printf("--------------------------\n");
		printf("NO CORRESPONDING ARC FOUND\n");
		RIG_printArcBones(next_iarc);
		
		printf("LOOKING FOR\n");
		printf("flag %i -- symmetry level %i -- symmetry flag %i\n", 0, symmetry_level, symmetry_flag);
		
		printf("CANDIDATES\n");
		for(i = 0; i < enode->degree; i++)
		{
			next_earc = (ReebArc*)enode->arcs[i];
			printf("flag %i -- symmetry level %i -- symmetry flag %i\n", next_earc->flag, next_earc->symmetry_level, next_earc->symmetry_flag);
		}
	}

}

static void retargetSubgraph(RigGraph *rigg, RigArc *start_arc, RigNode *start_node)
{
	RigNode *inode = start_node;
	int i;

	/* no start arc on first node */
	if (start_arc)
	{		
		ReebNode *enode = start_node->link_mesh;
		ReebArc *earc = start_arc->link_mesh;
		
		retargetArctoArc(start_arc);
		
		enode = BIF_otherNodeFromIndex(earc, enode);
		inode = (RigNode*)BLI_otherNode((BArc*)start_arc, (BNode*)inode);
	
		/* Link with lowest possible node
		 * Enabling going back to lower levels for each arc
		 * */
		inode->link_mesh = BIF_lowestLevelNode(enode);
	}
	
	for(i = 0; i < inode->degree; i++)
	{
		RigArc *next_iarc = (RigArc*)inode->arcs[i];
		
		/* no back tracking */
		if (next_iarc != start_arc)
		{
			findCorrespondingArc(start_arc, inode, next_iarc);
			if (next_iarc->link_mesh)
			{
				retargetSubgraph(rigg, next_iarc, inode);
			}
		}
	}
}

static void retargetGraphs(RigGraph *rigg)
{
	ReebGraph *reebg = rigg->link_mesh;
	RigNode *inode;
	
	/* flag all ReebArcs as not taken */
	BIF_flagMultiArcs(reebg, 0);
	
	/* return to first level */
	reebg = rigg->link_mesh;
	
	inode = rigg->head;
	
	matchMultiResolutionStartingNode(reebg, inode);

	retargetSubgraph(rigg, NULL, inode);
}

void BIF_retargetArmature()
{
	Object *ob;
	Base *base;
	ReebGraph *reebg;
	
	reebg = BIF_ReebGraphMultiFromEditMesh();
	
	
	printf("Reeb Graph created\n");

	base= FIRSTBASE;
	for (base = FIRSTBASE; base; base = base->next)
	{
		if TESTBASELIB(base) {
			ob = base->object;

			if (ob->type==OB_ARMATURE)
			{
				RigGraph *rigg;
				ListBase  list;
				bArmature *arm;
			 	
				arm = ob->data;
			
				/* Put the armature into editmode */
				list.first= list.last = NULL;
				make_boneList(&list, &arm->bonebase, NULL);
			
				rigg = armatureToGraph(&list);
				
				BLI_markdownSymmetry((BGraph*)rigg, (BNode*)rigg->head, G.scene->toolsettings->skgen_symmetry_limit);
				
				printf("Armature graph created\n");
		
				RIG_printGraph(rigg);
				
				rigg->link_mesh = reebg;
				
				printf("retargetting %s\n", ob->id.name);
				
				retargetGraphs(rigg);
				
				/* Turn the list into an armature */
				editbones_to_armature(&list, ob);
				
				BLI_freelistN(&list);

				RIG_freeRigGraph((BGraph*)rigg);
			}
		}
	}

	REEB_freeGraph(reebg);
	
	BIF_undo_push("Retarget Skeleton");
	
	exit_editmode(EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR); // freedata, and undo

	allqueue(REDRAWVIEW3D, 0);
}
