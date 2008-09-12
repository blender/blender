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
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
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
#include "BLI_threads.h"

#include "BDR_editobject.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"
#include "BKE_constraint.h"
#include "BKE_armature.h"

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

#define NB_THREADS 4
#define USE_THREADS

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
	
	ListBase *editbones;
	
	ListBase controls;
	struct ThreadedWorker *worker;
	
	GHash *bones_map;	/* map of editbones by name */
	GHash *controls_map;	/* map of rigcontrols by bone pointer */
	
	Object *ob;
} RigGraph;

typedef struct RigNode {
	void *next, *prev;
	float p[3];
	int flag;

	int degree;
	struct BArc **arcs;

	int subgraph_index;

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
	float up_axis[3];
} RigEdge;

/* Control flags */
#define RIG_CTRL_DONE			1
#define RIG_CTRL_PARENT_DEFORM	2
#define RIG_CTRL_FIT_ROOT		4
#define RIG_CTRL_FIT_BONE		8

typedef struct RigControl {
	struct RigControl *next, *prev;
	EditBone *bone;
	EditBone *link;
	float	up_axis[3];
	float	offset[3];
	int		flag;
} RigControl;

typedef struct MemoNode {
	float	weight;
	int		*positions;
} MemoNode;

typedef struct RetargetParam {
	RigGraph	*rigg;
	RigArc		*iarc;
	RigNode		*inode_start;
} RetargetParam;

typedef enum 
{
	RETARGET_LENGTH,
	RETARGET_AGGRESSIVE
} RetargetMode; 

typedef enum
{
	METHOD_BRUTE_FORCE = 0,
	METHOD_MEMOIZE = 1,
	METHOD_ANNEALING = 2
} RetargetMethod;

typedef enum
{
	ARC_FREE = 0,
	ARC_TAKEN = 1,
	ARC_USED = 2
} ArcUsageFlags;

/*******************************************************************************************************/

void *exec_retargetArctoArc(void *param);

static void RIG_calculateEdgeAngle(RigEdge *edge_first, RigEdge *edge_second);

/* two levels */
#define SHAPE_LEVELS (SHAPE_RADIX * SHAPE_RADIX) 

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

void getEditBoneRollUpAxis(EditBone *bone, float roll, float up_axis[3])
{
	float mat[3][3], nor[3];

	VecSubf(nor, bone->tail, bone->head);
	
	vec_roll_to_mat3(nor, roll, mat);
	VECCOPY(up_axis, mat[2]);
}

float getNewBoneRoll(EditBone *bone, float old_up_axis[3], float quat[4])
{
	float mat[3][3];
	float nor[3], up_axis[3], new_up_axis[3], vec[3];
	float roll;
	
	VECCOPY(new_up_axis, old_up_axis);
	QuatMulVecf(quat, new_up_axis);

	VecSubf(nor, bone->tail, bone->head);
	
	vec_roll_to_mat3(nor, 0, mat);
	VECCOPY(up_axis, mat[2]);
	
	roll = NormalizedVecAngle2(new_up_axis, up_axis);
	
	Crossf(vec, up_axis, new_up_axis);
	
	if (Inpf(vec, nor) < 0)
	{
		roll = -roll;
	}
	
	return roll;
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
	
	BLI_freelistN(&((RigGraph*)rg)->controls);

	BLI_ghash_free(((RigGraph*)rg)->bones_map, NULL, NULL);
	BLI_ghash_free(((RigGraph*)rg)->controls_map, NULL, NULL);
	
	MEM_freeN(rg);
}

/************************************* ALLOCATORS ******************************************************/

static RigGraph *newRigGraph()
{
	RigGraph *rg;
	rg = MEM_callocN(sizeof(RigGraph), "rig graph");
	
	rg->head = NULL;
	
	rg->bones_map = BLI_ghash_new(BLI_ghashutil_strhash, BLI_ghashutil_strcmp);
	rg->controls_map = BLI_ghash_new(BLI_ghashutil_strhash, BLI_ghashutil_strcmp);
	
	rg->free_arc = RIG_freeRigArc;
	rg->free_node = NULL;
	
#ifdef USE_THREADS
	rg->worker = BLI_create_worker(exec_retargetArctoArc, NB_THREADS, 20); /* fix number of threads */
#endif
	
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

static RigControl *newRigControl(RigGraph *rg)
{
	RigControl *ctrl;
	
	ctrl = MEM_callocN(sizeof(RigControl), "rig control");
	
	BLI_addtail(&rg->controls, ctrl);
	
	return ctrl;
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

static RigNode *newRigNode(RigGraph *rg, float p[3])
{
	RigNode *node;
	node = MEM_callocN(sizeof(RigNode), "rig node");
	BLI_addtail(&rg->nodes, node);

	VECCOPY(node->p, p);
	node->degree = 0;
	node->arcs = NULL;
	
	return node;
}

static RigNode *newRigNodeTail(RigGraph *rg, RigArc *arc, float p[3])
{
	RigNode *node = newRigNode(rg, p);
	
	node->degree = 1;
	arc->tail = node;

	return node;
}

static void RIG_appendEdgeToArc(RigArc *arc, RigEdge *edge)
{
	BLI_addtail(&arc->edges, edge);

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

static void RIG_addEdgeToArc(RigArc *arc, float tail[3], EditBone *bone)
{
	RigEdge *edge;

	edge = MEM_callocN(sizeof(RigEdge), "rig edge");

	VECCOPY(edge->tail, tail);
	edge->bone = bone;
	
	if (bone)
	{
		getEditBoneRollUpAxis(bone, bone->roll, edge->up_axis);
	}
	
	RIG_appendEdgeToArc(arc, edge);
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

/************************************ CONTROL BONES ****************************************************/

static void RIG_addControlBone(RigGraph *rg, EditBone *bone)
{
	RigControl *ctrl = newRigControl(rg);
	ctrl->bone = bone;
	getEditBoneRollUpAxis(bone, bone->roll, ctrl->up_axis);
	
	BLI_ghash_insert(rg->controls_map, bone->name, ctrl);
}

static int RIG_parentControl(RigControl *ctrl, EditBone *link)
{
	if (link)
	{
		float offset[3];
		int flag = 0;
		
		VecSubf(offset, ctrl->bone->head, link->head);

		/* if root matches, check for direction too */		
		if (Inpf(offset, offset) < 0.0001)
		{
			float vbone[3], vparent[3];
			
			flag |= RIG_CTRL_FIT_ROOT;
			
			VecSubf(vbone, ctrl->bone->tail, ctrl->bone->head);
			VecSubf(vparent, link->tail, link->head);
			
			/* test for opposite direction */
			if (Inpf(vbone, vparent) > 0)
			{
				float nor[3];
				float len;
				
				Crossf(nor, vbone, vparent);
				
				len = Inpf(nor, nor);
				if (len < 0.0001)
				{
					flag |= RIG_CTRL_FIT_BONE;
				}
			}
		}
		
		/* Bail out if old one is automatically better */
		if (flag < ctrl->flag)
		{
			return 0;
		}
		
		/* if there's already a link
		 * 	overwrite only if new link is higher in the chain */
		if (ctrl->link && flag == ctrl->flag)
		{
			EditBone *bone = NULL;
			
			for (bone = ctrl->link; bone; bone = bone->parent)
			{
				/* if link is in the chain, break and use that one */
				if (bone == link)
				{
					break;
				}
			}
			
			/* not in chain, don't update link */
			if (bone == NULL)
			{
				return 0;
			}
		}
		
		
		ctrl->link = link;
		ctrl->flag = flag;
		
		VECCOPY(ctrl->offset, offset);
		
		return 1;
	}
	
	return 0;
}

static void RIG_reconnectControlBones(RigGraph *rg)
{
	RigControl *ctrl;
	int change = 1;
	
	/* first pass, link to deform bones */
	for (ctrl = rg->controls.first; ctrl; ctrl = ctrl->next)
	{
		bPoseChannel *pchan;
		bConstraint *con;
		int found = 0;
		
		/* DO SOME MAGIC HERE */
		for (pchan= rg->ob->pose->chanbase.first; pchan; pchan= pchan->next)
		{
			for (con= pchan->constraints.first; con; con= con->next) 
			{
				bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
				ListBase targets = {NULL, NULL};
				bConstraintTarget *ct;
				
				/* constraint targets */
				if (cti && cti->get_constraint_targets)
				{
					cti->get_constraint_targets(con, &targets);
					
					for (ct= targets.first; ct; ct= ct->next)
					{
						if ((ct->tar == rg->ob) && strcmp(ct->subtarget, ctrl->bone->name) == 0)
						{
							/* SET bone link to bone corresponding to pchan */
							EditBone *link = BLI_ghash_lookup(rg->bones_map, pchan->name);
							
							found = RIG_parentControl(ctrl, link);
						}
					}
					
					if (cti->flush_constraint_targets)
						cti->flush_constraint_targets(con, &targets, 0);
				}
			}
		}

		/* if not found yet, check parent */
		if (found == 0)
		{		
			if (ctrl->bone->parent)
			{
				/* make sure parent is a deforming bone
				 * NULL if not
				 *  */
				EditBone *link = BLI_ghash_lookup(rg->bones_map, ctrl->bone->parent->name);
				
				found = RIG_parentControl(ctrl, link);
			}
			
			/* check if bone is not superposed on another one */
			{
				RigArc *arc;
				RigArc *best_arc = NULL;
				EditBone *link = NULL;
				
				for (arc = rg->arcs.first; arc; arc = arc->next)
				{
					RigEdge *edge;
					for (edge = arc->edges.first; edge; edge = edge->next)
					{
						if (edge->bone)
						{
							int fit = 0;
							
							fit = VecLenf(ctrl->bone->head, edge->bone->head) < 0.0001;
							fit = fit || VecLenf(ctrl->bone->tail, edge->bone->tail) < 0.0001;
							
							if (fit)
							{
								/* pick the bone on the arc with the lowest symmetry level
								 * means you connect control to the trunk of the skeleton */
								if (best_arc == NULL || arc->symmetry_level < best_arc->symmetry_level)
								{
									best_arc = arc;
									link = edge->bone;
								}
							}
						}
					}
				}
				
				found = RIG_parentControl(ctrl, link);
			}
		}
		
		/* if not found yet, check child */		
		if (found == 0)
		{
			RigArc *arc;
			RigArc *best_arc = NULL;
			EditBone *link = NULL;
			
			for (arc = rg->arcs.first; arc; arc = arc->next)
			{
				RigEdge *edge;
				for (edge = arc->edges.first; edge; edge = edge->next)
				{
					if (edge->bone && edge->bone->parent == ctrl->bone)
					{
						/* pick the bone on the arc with the lowest symmetry level
						 * means you connect control to the trunk of the skeleton */
						if (best_arc == NULL || arc->symmetry_level < best_arc->symmetry_level)
						{
							best_arc = arc;
							link = edge->bone;
						}
					}
				}
			}
			
			found = RIG_parentControl(ctrl, link);
		}

	}
	
	
	/* second pass, make chains in control bones */
	while (change)
	{
		change = 0;
		
		printf("-------------------------\n");
		
		for (ctrl = rg->controls.first; ctrl; ctrl = ctrl->next)
		{
			/* if control is not linked yet */
			if (ctrl->link == NULL)
			{
				bPoseChannel *pchan;
				bConstraint *con;
				RigControl *ctrl_parent = NULL;
				RigControl *ctrl_child;
				int found = 0;

				if (ctrl->bone->parent)
				{
					ctrl_parent = BLI_ghash_lookup(rg->controls_map, ctrl->bone->parent->name);
				}

				/* check constraints first */
				
				/* DO SOME MAGIC HERE */
				for (pchan= rg->ob->pose->chanbase.first; pchan; pchan= pchan->next)
				{
					for (con= pchan->constraints.first; con; con= con->next) 
					{
						bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
						ListBase targets = {NULL, NULL};
						bConstraintTarget *ct;
						
						/* constraint targets */
						if (cti && cti->get_constraint_targets)
						{
							cti->get_constraint_targets(con, &targets);
							
							for (ct= targets.first; ct; ct= ct->next)
							{
								if ((ct->tar == rg->ob) && strcmp(ct->subtarget, ctrl->bone->name) == 0)
								{
									/* SET bone link to ctrl corresponding to pchan */
									RigControl *link = BLI_ghash_lookup(rg->controls_map, pchan->name);

									/* if owner is a control bone, link with it */									
									if (link && link->link)
									{
										printf("%s -constraint- %s\n", ctrl->bone->name, link->bone->name);
										RIG_parentControl(ctrl, link->bone);
										found = 1;
										break;
									}
								}
							}
							
							if (cti->flush_constraint_targets)
								cti->flush_constraint_targets(con, &targets, 0);
						}
					}
				}			

				if (found == 0)
				{
					/* check if parent is already linked */
					if (ctrl_parent && ctrl_parent->link)
					{
						printf("%s -parent- %s\n", ctrl->bone->name, ctrl_parent->bone->name);
						RIG_parentControl(ctrl, ctrl_parent->bone);
						change = 1;
					}
					else
					{
						/* check childs */
						for (ctrl_child = rg->controls.first; ctrl_child; ctrl_child = ctrl_child->next)
						{
							/* if a child is linked, link to that one */
							if (ctrl_child->link && ctrl_child->bone->parent == ctrl->bone)
							{
								printf("%s -child- %s\n", ctrl->bone->name, ctrl_child->bone->name);
								RIG_parentControl(ctrl, ctrl_child->bone);
								change = 1;
								break;
							}
						}
					}
				}
			}
		}
		
	}
}

/*******************************************************************************************************/

static void RIG_joinArcs(RigGraph *rg, RigNode *node, RigArc *joined_arc1, RigArc *joined_arc2)
{
	RigEdge *edge, *next_edge;
	
	/* ignore cases where joint is at start or end */
	if (joined_arc1->head == joined_arc2->head || joined_arc1->tail == joined_arc2->tail)
	{
		return;
	}
	
	/* swap arcs to make sure arc1 is before arc2 */
	if (joined_arc1->head == joined_arc2->tail)
	{
		RigArc *tmp = joined_arc1;
		joined_arc1 = joined_arc2;
		joined_arc2 = tmp;
	}
	
	for (edge = joined_arc2->edges.first; edge; edge = next_edge)
	{
		next_edge = edge->next;
		
		RIG_appendEdgeToArc(joined_arc1, edge);
	}
	
	joined_arc1->tail = joined_arc2->tail;
	
	joined_arc2->edges.first = joined_arc2->edges.last = NULL;
	
	BLI_removeArc((BGraph*)rg, (BArc*)joined_arc2);
	
	BLI_removeNode((BGraph*)rg, (BNode*)node);
}

static void RIG_removeNormalNodes(RigGraph *rg)
{
	RigNode *node, *next_node;
	
	for (node = rg->nodes.first; node; node = next_node)
	{
		next_node = node->next;
		
		if (node->degree == 2)
		{
			RigArc *arc, *joined_arc1 = NULL, *joined_arc2 = NULL;
			
			for (arc = rg->arcs.first; arc; arc = arc->next)
			{
				if (arc->head == node || arc->tail == node)
				{
					if (joined_arc1 == NULL)
					{
						joined_arc1 = arc;
					}
					else
					{
						joined_arc2 = arc;
						break;
					}
				}
			}
			
			RIG_joinArcs(rg, node, joined_arc1, joined_arc2);
		}
	}
}

static void RIG_removeUneededOffsets(RigGraph *rg)
{
	RigArc *arc;
	
	for (arc = rg->arcs.first; arc; arc = arc->next)
	{
		RigEdge *first_edge, *last_edge;
		
		first_edge = arc->edges.first;
		last_edge = arc->edges.last;
		
		if (first_edge->bone == NULL)
		{
			if (first_edge->bone == NULL && VecLenf(first_edge->tail, arc->head->p) <= 0.001)
			{
				BLI_remlink(&arc->edges, first_edge);
				MEM_freeN(first_edge);
			}
			else if (arc->head->degree == 1)
			{
				RigNode *new_node = (RigNode*)BLI_FindNodeByPosition((BGraph*)rg, first_edge->tail, 0.001);
				
				if (new_node)
				{
					BLI_remlink(&arc->edges, first_edge);
					MEM_freeN(first_edge);
					BLI_replaceNodeInArc((BGraph*)rg, (BArc*)arc, (BNode*)new_node, (BNode*)arc->head);
				}
				else
				{
					RigEdge *next_edge = first_edge->next;
	
					if (next_edge)
					{
						BLI_remlink(&arc->edges, first_edge);
						MEM_freeN(first_edge);
						
						VECCOPY(arc->head->p, next_edge->head);
					}
				}
			}
			else
			{
				/* check if all arc connected start with a null edge */
				RigArc *other_arc;
				for (other_arc = rg->arcs.first; other_arc; other_arc = other_arc->next)
				{
					if (other_arc != arc)
					{
						RigEdge *test_edge;
						if (other_arc->head == arc->head)
						{
							test_edge = other_arc->edges.first;
							
							if (test_edge->bone != NULL)
							{
								break;
							}
						}
						else if (other_arc->tail == arc->head)
						{
							test_edge = other_arc->edges.last;
							
							if (test_edge->bone != NULL)
							{
								break;
							}
						}
					}
				}
				
				if (other_arc == NULL)
				{
					RigNode *new_node = (RigNode*)BLI_FindNodeByPosition((BGraph*)rg, first_edge->tail, 0.001);
					
					if (new_node)
					{
						/* remove null edge in other arcs too */
						for (other_arc = rg->arcs.first; other_arc; other_arc = other_arc->next)
						{
							if (other_arc != arc)
							{
								RigEdge *test_edge;
								if (other_arc->head == arc->head)
								{
									BLI_replaceNodeInArc((BGraph*)rg, (BArc*)other_arc, (BNode*)new_node, (BNode*)other_arc->head);
									test_edge = other_arc->edges.first;
									BLI_remlink(&other_arc->edges, test_edge);
									MEM_freeN(test_edge);
								}
								else if (other_arc->tail == arc->head)
								{
									BLI_replaceNodeInArc((BGraph*)rg, (BArc*)other_arc, (BNode*)new_node, (BNode*)other_arc->tail);
									test_edge = other_arc->edges.last;
									BLI_remlink(&other_arc->edges, test_edge);
									MEM_freeN(test_edge);
								}
							}
						}
						
						BLI_remlink(&arc->edges, first_edge);
						MEM_freeN(first_edge);
						BLI_replaceNodeInArc((BGraph*)rg, (BArc*)arc, (BNode*)new_node, (BNode*)arc->head);
					}
					else
					{
						RigEdge *next_edge = first_edge->next;
		
						if (next_edge)
						{
							BLI_remlink(&arc->edges, first_edge);
							MEM_freeN(first_edge);
							
							VECCOPY(arc->head->p, next_edge->head);
							
							/* remove null edge in other arcs too */
							for (other_arc = rg->arcs.first; other_arc; other_arc = other_arc->next)
							{
								if (other_arc != arc)
								{
									RigEdge *test_edge;
									if (other_arc->head == arc->head)
									{
										test_edge = other_arc->edges.first;
										BLI_remlink(&other_arc->edges, test_edge);
										MEM_freeN(test_edge);
									}
									else if (other_arc->tail == arc->head)
									{
										test_edge = other_arc->edges.last;
										BLI_remlink(&other_arc->edges, test_edge);
										MEM_freeN(test_edge);
									}
								}
							}
						}
					}
				}
			}
		}
		
		if (last_edge->bone == NULL)
		{
			if (VecLenf(last_edge->head, arc->tail->p) <= 0.001)
			{
				BLI_remlink(&arc->edges, last_edge);
				MEM_freeN(last_edge);
			}
			else if (arc->tail->degree == 1)
			{
				RigNode *new_node = (RigNode*)BLI_FindNodeByPosition((BGraph*)rg, last_edge->head, 0.001);
				
				if (new_node)
				{
					RigEdge *previous_edge = last_edge->prev;
					
					BLI_remlink(&arc->edges, last_edge);
					MEM_freeN(last_edge);
					BLI_replaceNodeInArc((BGraph*)rg, (BArc*)arc, (BNode*)new_node, (BNode*)arc->tail);
					
					/* set previous angle to 0, since there's no following edges */
					if (previous_edge)
					{
						previous_edge->angle = 0;
					}
				}
				else
				{
					RigEdge *previous_edge = last_edge->prev;
	
					if (previous_edge)
					{
						BLI_remlink(&arc->edges, last_edge);
						MEM_freeN(last_edge);
						
						VECCOPY(arc->tail->p, previous_edge->tail);
						previous_edge->angle = 0;
					}
				}
			}
		}
	}
}

static void RIG_arcFromBoneChain(RigGraph *rg, ListBase *list, EditBone *root_bone, RigNode *starting_node)
{
	EditBone *bone, *last_bone = root_bone;
	RigArc *arc = NULL;
	int contain_head = 0;
	
	for(bone = root_bone; bone; bone = nextEditBoneChild(list, bone, 0))
	{
		int nb_children;
		
		if ((bone->flag & BONE_NO_DEFORM) == 0)
		{
			BLI_ghash_insert(rg->bones_map, bone->name, bone);
		
			if (arc == NULL)
			{
				arc = newRigArc(rg);
				
				if (starting_node == NULL)
				{
					starting_node = newRigNodeHead(rg, arc, root_bone->head);
				}
				else
				{
					addRigNodeHead(rg, arc, starting_node);
				}
			}
			
			if (bone->parent && (bone->flag & BONE_CONNECTED) == 0)
			{
				RIG_addEdgeToArc(arc, bone->head, NULL);
			}
			
			RIG_addEdgeToArc(arc, bone->tail, bone);
			
			last_bone = bone;
			
			if (strcmp(bone->name, "head") == 0)
			{
				contain_head = 1;
			}
		}
		else if ((bone->flag & BONE_EDITMODE_LOCKED) == 0) /* ignore locked bones */
		{
			RIG_addControlBone(rg, bone);
		}
		
		nb_children = countEditBoneChildren(list, bone);
		if (nb_children > 1)
		{
			RigNode *end_node = NULL;
			int i;
			
			if (arc != NULL)
			{
				end_node = newRigNodeTail(rg, arc, bone->tail);
			}
			else
			{
				end_node = newRigNode(rg, bone->tail);
			}

			for (i = 0; i < nb_children; i++)
			{
				root_bone = nextEditBoneChild(list, bone, i);
				RIG_arcFromBoneChain(rg, list, root_bone, end_node);
			}
			
			/* arc ends here, break */
			break;
		}
	}
	
	/* If the loop exited without forking */
	if (arc != NULL && bone == NULL)
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

void RIG_printCtrl(RigControl *ctrl, char *indent)
{
	char text[128];
	
	printf("%sBone: %s\n", indent, ctrl->bone->name);
	printf("%sLink: %s\n", indent, ctrl->link ? ctrl->link->name : "!NONE!");
	
	sprintf(text, "%soffset", indent);
	printvecf(text, ctrl->offset);
	
	printf("%sFlag: %i\n", indent, ctrl->flag);
}

void RIG_printLinkedCtrl(RigGraph *rg, EditBone *bone, int tabs)
{
	RigControl *ctrl;
	char indent[64];
	char *s = indent;
	int i;
	
	for (i = 0; i < tabs; i++)
	{
		s[0] = '\t';
		s++;
	}
	s[0] = 0;
	
	for (ctrl = rg->controls.first; ctrl; ctrl = ctrl->next)
	{
		if (ctrl->link == bone)
		{
			RIG_printCtrl(ctrl, indent);
			RIG_printLinkedCtrl(rg, ctrl->bone, tabs + 1);
		}
	}
}

void RIG_printArc(RigGraph *rg, RigArc *arc)
{
	RigEdge *edge;

	RIG_printNode((RigNode*)arc->head, "head");

	for (edge = arc->edges.first; edge; edge = edge->next)
	{
		printf("\tinner joints %0.3f %0.3f %0.3f\n", edge->tail[0], edge->tail[1], edge->tail[2]);
		printf("\t\tlength %f\n", edge->length);
		printf("\t\tangle %f\n", edge->angle * 180 / M_PI);
		if (edge->bone)
		{
			printf("\t\t%s\n", edge->bone->name);
			RIG_printLinkedCtrl(rg, edge->bone, 3);
		}
	}	
	printf("symmetry level: %i flag: %i group %i\n", arc->symmetry_level, arc->symmetry_flag, arc->symmetry_group);

	RIG_printNode((RigNode*)arc->tail, "tail");
}

void RIG_printGraph(RigGraph *rg)
{
	RigArc *arc;

	printf("---- ARCS ----\n");
	for (arc = rg->arcs.first; arc; arc = arc->next)
	{
		RIG_printArc(rg, arc);	
		printf("\n");
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

static RigGraph *armatureToGraph(Object *ob, ListBase *list)
{
	EditBone *ebone;
	RigGraph *rg;
 	
	rg = newRigGraph();
	
	rg->editbones = list;	
	rg->ob = ob;

	/* Do the rotations */
	for (ebone = list->first; ebone; ebone=ebone->next){
		if (ebone->parent == NULL)
		{
			RIG_arcFromBoneChain(rg, list, ebone, NULL);
		}
	}
	
	BLI_removeDoubleNodes((BGraph*)rg, 0.001);
	
	RIG_removeNormalNodes(rg);
	
	RIG_removeUneededOffsets(rg);
	
	BLI_buildAdjacencyList((BGraph*)rg);
	
	RIG_findHead(rg);

	BLI_markdownSymmetry((BGraph*)rg, (BNode*)rg->head, G.scene->toolsettings->skgen_symmetry_limit);
	
	RIG_reconnectControlBones(rg); /* after symmetry, because we use levels to find best match */
	
	if (BLI_isGraphCyclic((BGraph*)rg))
	{
		printf("armature cyclic\n");
	}
	
	return rg;
}

/************************************ GENERATING *****************************************************/

static EditBone *add_editbonetolist(char *name, ListBase *list)
{
	EditBone *bone= MEM_callocN(sizeof(EditBone), "eBone");
	
	BLI_strncpy(bone->name, name, 32);
	unique_editbone_name(list, bone->name);
	
	BLI_addtail(list, bone);
	
	bone->flag |= BONE_TIPSEL;
	bone->weight= 1.0F;
	bone->dist= 0.25F;
	bone->xwidth= 0.1;
	bone->zwidth= 0.1;
	bone->ease1= 1.0;
	bone->ease2= 1.0;
	bone->rad_head= 0.10;
	bone->rad_tail= 0.05;
	bone->segments= 1;
	bone->layer=  1;//arm->layer;
	
	return bone;
}

EditBone * generateBonesForArc(RigGraph *rigg, ReebArc *arc, ReebNode *head, ReebNode *tail)
{
	ReebArcIterator iter;
	float n[3];
	float ADAPTIVE_THRESHOLD = G.scene->toolsettings->skgen_correlation_limit;
	EditBone *lastBone = NULL;
	
	/* init iterator to get start and end from head */
	initArcIterator(&iter, arc, head);
	
	/* Calculate overall */
	VecSubf(n, arc->buckets[iter.end].p, head->p);
	
	if (1 /* G.scene->toolsettings->skgen_options & SKGEN_CUT_CORRELATION */ )
	{
		EmbedBucket *bucket = NULL;
		EmbedBucket *previous = NULL;
		EditBone *child = NULL;
		EditBone *parent = NULL;
		float normal[3] = {0, 0, 0};
		float avg_normal[3];
		int total = 0;
		int boneStart = iter.start;
		
		parent = add_editbonetolist("Bone", rigg->editbones);
		parent->flag = BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL;
		VECCOPY(parent->head, head->p);
		
		for (previous = nextBucket(&iter), bucket = nextBucket(&iter);
			bucket;
			previous = bucket, bucket = nextBucket(&iter))
		{
			float btail[3];
			float value = 0;

			if (G.scene->toolsettings->skgen_options & SKGEN_STICK_TO_EMBEDDING)
			{
				VECCOPY(btail, bucket->p);
			}
			else
			{
				float length;
				
				/* Calculate normal */
				VecSubf(n, bucket->p, parent->head);
				length = Normalize(n);
				
				total += 1;
				VecAddf(normal, normal, n);
				VECCOPY(avg_normal, normal);
				VecMulf(avg_normal, 1.0f / total);
				 
				VECCOPY(btail, avg_normal);
				VecMulf(btail, length);
				VecAddf(btail, btail, parent->head);
			}

			if (G.scene->toolsettings->skgen_options & SKGEN_ADAPTIVE_DISTANCE)
			{
				value = calcDistance(arc, boneStart, iter.index, parent->head, btail);
			}
			else
			{
				float n[3];
				
				VecSubf(n, btail, parent->head);
				value = calcVariance(arc, boneStart, iter.index, parent->head, n);
			}

			if (value > ADAPTIVE_THRESHOLD)
			{
				VECCOPY(parent->tail, btail);

				child = add_editbonetolist("Bone", rigg->editbones);
				VECCOPY(child->head, parent->tail);
				child->parent = parent;
				child->flag |= BONE_CONNECTED|BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL;
				
				parent = child; // new child is next parent
				boneStart = iter.index; // start from end
				
				normal[0] = normal[1] = normal[2] = 0;
				total = 0;
			}
		}

		VECCOPY(parent->tail, tail->p);
		
		lastBone = parent; /* set last bone in the chain */
	}
	
	return lastBone;
}

void generateMissingArcsFromNode(RigGraph *rigg, ReebNode *node, int multi_level_limit)
{
	while (node->multi_level > multi_level_limit && node->link_up)
	{
		node = node->link_up;
	}
	
	while (node->multi_level < multi_level_limit && node->link_down)
	{
		node = node->link_down;
	}
	
	if (node->multi_level == multi_level_limit)
	{
		int i;
		
		for (i = 0; i < node->degree; i++)
		{
			ReebArc *earc = node->arcs[i];
			
			if (earc->flag == ARC_FREE && earc->head == node)
			{
				ReebNode *other = BIF_otherNodeFromIndex(earc, node);
				
				earc->flag = ARC_USED;
				
				generateBonesForArc(rigg, earc, node, other);
				generateMissingArcsFromNode(rigg, other, multi_level_limit);
			}
		}
	}
}

void generateMissingArcs(RigGraph *rigg)
{
	ReebGraph *reebg = rigg->link_mesh;
	int multi_level_limit = 5;
	
	for (reebg = rigg->link_mesh; reebg; reebg = reebg->link_up)
	{
		ReebArc *earc;
		
		for (earc = reebg->arcs.first; earc; earc = earc->next)
		{
			if (earc->flag == ARC_USED)
			{
				generateMissingArcsFromNode(rigg, earc->head, multi_level_limit);
				generateMissingArcsFromNode(rigg, earc->tail, multi_level_limit);
			}
		}
	}
}

/************************************ RETARGETTING *****************************************************/

static void repositionControl(RigGraph *rigg, RigControl *ctrl, float head[3], float tail[3], float qrot[4], float resize)
{
	RigControl *ctrl_child;
	float parent_offset[3], tail_offset[3];
	
	VecSubf(tail_offset, ctrl->bone->tail, ctrl->bone->head);
	VecMulf(tail_offset, resize);
	
	VECCOPY(parent_offset, ctrl->offset);
	VecMulf(parent_offset, resize);
	
	QuatMulVecf(qrot, parent_offset);
	QuatMulVecf(qrot, tail_offset);
	
	VecAddf(ctrl->bone->head, head, parent_offset); 
	VecAddf(ctrl->bone->tail, ctrl->bone->head, tail_offset);
	ctrl->bone->roll = getNewBoneRoll(ctrl->bone, ctrl->up_axis, qrot);
	
	ctrl->flag |= RIG_CTRL_DONE;

	/* Cascade to connected control bones */
	for (ctrl_child = rigg->controls.first; ctrl_child; ctrl_child = ctrl_child->next)
	{
		if (ctrl_child->link == ctrl->bone)
		{
			repositionControl(rigg, ctrl_child, ctrl->bone->head, ctrl->bone->tail, qrot, resize);
		}
	}

}

static void repositionBone(RigGraph *rigg, RigEdge *edge, float vec0[3], float vec1[3])
{
	EditBone *bone;
	RigControl *ctrl;
	float qrot[4], resize;
	float v1[3], v2[3];
	float l1, l2;
	
	bone = edge->bone;
	
	VecSubf(v1, bone->tail, bone->head);
	VecSubf(v2, vec1, vec0);
	
	l1 = Normalize(v1);
	l2 = Normalize(v2);

	resize = l2 / l1;
	
	RotationBetweenVectorsToQuat(qrot, v1, v2);
	
	for (ctrl = rigg->controls.first; ctrl; ctrl = ctrl->next)
	{
		if (ctrl->link == bone)
		{
			repositionControl(rigg, ctrl, vec0, vec1, qrot, resize);
		}
	}
	
	VECCOPY(bone->head, vec0);
	VECCOPY(bone->tail, vec1);
	bone->roll = getNewBoneRoll(bone, edge->up_axis, qrot);
}

static RetargetMode detectArcRetargetMode(RigArc *arc);
static void retargetArctoArcLength(RigGraph *rigg, RigArc *iarc, RigNode *inode_start);


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

#ifndef USE_THREADS
static void printCostCube(float *cost_cube, int nb_joints)
{
	int i;
	
	for (i = 0; i < nb_joints; i++)
	{
		printf("%0.3f ", cost_cube[3 * i]);
	}
	printf("\n");

	for (i = 0; i < nb_joints; i++)
	{
		printf("%0.3f ", cost_cube[3 * i + 1]);
	}
	printf("\n");

	for (i = 0; i < nb_joints; i++)
	{
		printf("%0.3f ", cost_cube[3 * i + 2]);
	}
	printf("\n");
}

static void printMovesNeeded(int *positions, int nb_positions)
{
	int moves = 0;
	int i;
	
	for (i = 0; i < nb_positions; i++)
	{
		moves += positions[i] - (i + 1);
	}
	
	printf("%i moves needed\n", moves);
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
#endif

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
			
			return G.scene->toolsettings->skgen_retarget_distance_weight * max_dist;
		}
		else
		{
			return MAX_COST;
		}
	}
	else
	{
		return 0;
	}
}

static float costAngle(float original_angle, float vec_first[3], float vec_second[3])
{
	if (G.scene->toolsettings->skgen_retarget_angle_weight > 0)
	{
		float current_angle;
		
		if (!VecIsNull(vec_first) && !VecIsNull(vec_second))
		{
			current_angle = saacos(Inpf(vec_first, vec_second));

			return G.scene->toolsettings->skgen_retarget_angle_weight * fabs(current_angle - original_angle);
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

static float calcCostLengthDistance(ReebArcIterator *iter, float **vec_cache, RigEdge *edge, float *vec1, float *vec2, int i1, int i2)
{
	float vec[3];
	float length;

	VecSubf(vec, vec2, vec1);
	length = Normalize(vec);

	return costLength(edge->length, length) + costDistance(iter, vec1, vec2, i1, i2);
}

static float calcCostAngleLengthDistance(ReebArcIterator *iter, float **vec_cache, RigEdge *edge, float *vec0, float *vec1, float *vec2, int i1, int i2)
{
	float vec_second[3], vec_first[3];
	float length2;
	float new_cost = 0;

	VecSubf(vec_second, vec2, vec1);
	length2 = Normalize(vec_second);


	/* Angle cost */	
	if (edge->prev)
	{
		VecSubf(vec_first, vec1, vec0); 
		Normalize(vec_first);
		
		new_cost += costAngle(edge->prev->angle, vec_first, vec_second);
	}

	/* Length cost */
	new_cost += costLength(edge->length, length2);

	/* Distance cost */
	new_cost += costDistance(iter, vec1, vec2, i1, i2);

	return new_cost;
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
	new_cost += costAngle(e1->angle, vec_first, vec_second);

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
		i0 = 0;
	}
	else
	{
		i0 = positions[index - 1];
	}
	
	i1 = positions[index];
	
	if (index +1 == nb_joints)
	{
		i2 = iter->length;
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
	else if (next_position > iter->length) /* positions are indexed at 1, so length is last */
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
	else if (next_position < 1) /* positions are indexed at 1, so 1 is first */
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
		return (float)exp(delta_cost / temperature);
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

static void copyMemoNode(MemoNode *dst, MemoNode *src, int size)
{
	if (size > 0)
	{
		memcpy(dst->positions + 1, src->positions, size * sizeof(int));
	}
}

static int indexMemoNode(int nb_positions, int previous, int current, int joints_done)
{
	return joints_done * nb_positions * nb_positions + current * nb_positions + previous;
}

static MemoNode * solveJoints(MemoNode *table, ReebArcIterator *iter, float **vec_cache, int nb_joints, int nb_positions, int previous, int current, RigEdge *edge, int joints_left)
{
	MemoNode *node;
	int index = indexMemoNode(nb_positions, previous, current, joints_left);
	
	node = table + index;
	
	if (node->weight != 0)
	{
		return node;
	}
	else if (joints_left == 0)
	{
		float *vec1 = vec_cache[current];
		float *vec2 = vec_cache[nb_positions + 1];

		node->weight = calcCostLengthDistance(iter, vec_cache, edge, vec1, vec2, current, iter->length);

		return node;
	}
	else
	{
		MemoNode *min_node = NULL;
		float *vec0 = vec_cache[previous];
		float *vec1 = vec_cache[current];
		float min_weight;
		int min_next;
		int next;
		
		for (next = current + 1; next <= nb_positions - (joints_left - 1); next++)
		{
			MemoNode *next_node;
			float *vec2 = vec_cache[next];
			float weight = 0;
			
			/* ADD WEIGHT OF PREVIOUS - CURRENT - NEXT triple */
			weight = calcCostAngleLengthDistance(iter, vec_cache, edge, vec0, vec1, vec2, current, next);
			
			if (weight >= MAX_COST)
			{
				continue;
			}
			
			/* add node weight */
			next_node = solveJoints(table, iter, vec_cache, nb_joints, nb_positions, current, next, edge->next, joints_left - 1);
			weight += next_node->weight;
			
			if (min_node == NULL || weight < min_weight)
			{
				min_weight = weight;
				min_node = next_node;
				min_next = next;
			}
		}
		
		if (min_node)
		{
			node->positions = MEM_callocN(sizeof(int) * joints_left, "Memoization indexes array");
			node->weight = min_weight;
			copyMemoNode(node, min_node, joints_left - 1);
			node->positions[0] = min_next;
			return node;
		}
		else
		{
			node->positions = MEM_callocN(sizeof(int) * joints_left, "Memoization indexes array");
			node->weight = MAX_COST;
			return node;
		}
	}
	
}

static int testFlipArc(RigArc *iarc, RigNode *inode_start)
{
	ReebArc *earc = iarc->link_mesh;
	ReebNode *enode_start = BIF_NodeFromIndex(earc, inode_start->link_mesh);
	
	/* no flip needed if both nodes are the same */
	if ((enode_start == earc->head && inode_start == iarc->head) || (enode_start == earc->tail && inode_start == iarc->tail))
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

static void retargetArctoArcAggresive(RigGraph *rigg, RigArc *iarc, RigNode *inode_start)
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
	RetargetMethod method = G.scene->toolsettings->skgen_optimisation_method;
	int i;
	
	if (nb_joints > earc->bcount)
	{
		printf("NOT ENOUGH BUCKETS!\n");
		return;
	}

	positions = MEM_callocN(sizeof(int) * nb_joints, "Aggresive positions");
	best_positions = MEM_callocN(sizeof(int) * nb_joints, "Best Aggresive positions");
	cost_cache = MEM_callocN(sizeof(float) * nb_edges, "Cost cache");
	vec_cache = MEM_callocN(sizeof(float*) * (nb_edges + 1), "Vec cache");
	
	if (testFlipArc(iarc, inode_start))
	{
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

	if (method == METHOD_MEMOIZE)
	{
		int nb_positions = earc->bcount;
		int nb_memo_nodes = nb_positions * nb_positions * (nb_joints + 1);
		MemoNode *table = MEM_callocN(nb_memo_nodes * sizeof(MemoNode), "memoization table");
		MemoNode *result;
		float **positions_cache = MEM_callocN(sizeof(float*) * (nb_positions + 2), "positions cache");
		int i;
		
		positions_cache[0] = node_start->p;
		positions_cache[nb_positions + 1] = node_end->p;
		
		initArcIterator(&iter, earc, node_start);

		for (i = 1; i <= nb_positions; i++)
		{
			EmbedBucket *bucket = peekBucket(&iter, i);
			positions_cache[i] = bucket->p;
		}

		result = solveJoints(table, &iter, positions_cache, nb_joints, earc->bcount, 0, 0, iarc->edges.first, nb_joints);
		
		min_cost = result->weight;
		memcpy(best_positions, result->positions, sizeof(int) * nb_joints);
		
		for ( i = 0; i < nb_memo_nodes; i++)
		{
			if (table[i].positions)
			{
				MEM_freeN(table[i].positions);
			}
		}
		
		MEM_freeN(table);
		MEM_freeN(positions_cache);
	}
	/* BRUTE FORCE */
	else if (method == METHOD_BRUTE_FORCE)
	{
		int last_index = 0;
		int first_pass = 1;
		int must_move = nb_joints - 1;
		
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
						new_cost += costAngle(previous->angle, vec_first, vec_second);
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
	else if (method == METHOD_ANNEALING)
	{
		RigEdge *previous;
		float *cost_cube;
		float cost;
		int k;
		int kmax;

		kmax = 100000;
		
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
		
		cost = 0;

		/* init cost cube */
		for (previous = iarc->edges.first, edge = previous->next, i = 0;
			 edge;
			 previous = edge, edge = edge->next, i += 1)
		{
			calcGradient(previous, edge, &iter, i, nb_joints, cost_cube, positions, vec_cache);
			
			cost += cost_cube[3 * i + 1];
		}
		
#ifndef USE_THREADS
		printf("initial cost: %f\n", cost);
		printf("kmax: %i\n", kmax);
#endif
		
		for (k = 0; k < kmax; k++)
		{
			int status;
			int moving_joint = -1;
			int move_direction = -1;
			float delta_cost;
			float temperature;
			
			status = neighbour(nb_joints, cost_cube, &moving_joint, &move_direction);
			
			if (status == 0)
			{
				/* if current state is still a minimum, copy it */
				if (cost < min_cost)
				{
					min_cost = cost;
					memcpy(best_positions, positions, sizeof(int) * nb_joints);
				}
				break;
			}
			
			delta_cost = cost_cube[moving_joint * 3 + (1 + move_direction)];

			temperature = 1 - (float)k / (float)kmax;
			if (probability(delta_cost, temperature) > BLI_frand())
			{
				/* update position */			
				positions[moving_joint] += move_direction;
				
				/* update vector cache */
				bucket = peekBucket(&iter, positions[moving_joint]);
				vec_cache[moving_joint + 1] = bucket->p;
				
				cost += delta_cost;
	
				/* cost optimizing */
				if (cost < min_cost)
				{
					min_cost = cost;
					memcpy(best_positions, positions, sizeof(int) * nb_joints);
				}

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

		//min_cost = cost;
		//memcpy(best_positions, positions, sizeof(int) * nb_joints);
		
//		printf("k = %i\n", k);
		
		
		MEM_freeN(cost_cube);
	}	


	vec0 = node_start->p;
	initArcIterator(&iter, earc, node_start);
	
#ifndef USE_THREADS
	printPositions(best_positions, nb_joints);
	printMovesNeeded(best_positions, nb_joints);
	printf("min_cost %f\n", min_cost);
	printf("buckets: %i\n", earc->bcount);
#endif

	/* set joints to best position */
	for (edge = iarc->edges.first, i = 0;
		 edge;
		 edge = edge->next, i++)
	{
		if (i < nb_joints)
		{
			bucket = peekBucket(&iter, best_positions[i]);
			vec1 = bucket->p;
		}
		else
		{
			vec1 = node_end->p;
		}
		
		if (edge->bone)
		{
			repositionBone(rigg, edge, vec0, vec1);
		}
		
		vec0 = vec1;
	}
	
	MEM_freeN(positions);
	MEM_freeN(best_positions);
	MEM_freeN(cost_cache);
	MEM_freeN(vec_cache);
}

static void retargetArctoArcLength(RigGraph *rigg, RigArc *iarc, RigNode *inode_start)
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

	
	if (testFlipArc(iarc, inode_start))
	{
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
	
	for (edge = iarc->edges.first; edge; edge = edge->next)
	{
		float new_bone_length = edge->length / iarc->length * embedding_length;

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

		/* no need to move virtual edges (space between unconnected bones) */		
		if (edge->bone)
		{
			repositionBone(rigg, edge, vec0, vec1);
		}
		
		vec0 = vec1;
		previous_vec = vec1;
	}
}

static void retargetArctoArc(RigGraph *rigg, RigArc *iarc, RigNode *inode_start)
{
#ifdef USE_THREADS
	RetargetParam *p = MEM_callocN(sizeof(RetargetParam), "RetargetParam");
	
	p->rigg = rigg;
	p->iarc = iarc;
	p->inode_start = inode_start;
	
	BLI_insert_work(rigg->worker, p);
#else
	RetargetParam p;

	p.rigg = rigg;
	p.iarc = iarc;
	p.inode_start = inode_start;
	
	exec_retargetArctoArc(&p);
#endif
}

void *exec_retargetArctoArc(void *param)
{
	RetargetParam *p = (RetargetParam*)param;
	RigGraph *rigg = p->rigg;
	RigArc *iarc = p->iarc;	
	RigNode *inode_start = p->inode_start;
	ReebArc *earc = iarc->link_mesh;
	
	if (BLI_countlist(&iarc->edges) == 1)
	{
		RigEdge *edge = iarc->edges.first;

		if (testFlipArc(iarc, inode_start))
		{
			repositionBone(rigg, edge, earc->tail->p, earc->head->p);
		}
		else
		{
			repositionBone(rigg, edge, earc->head->p, earc->tail->p);
		}
	}
	else
	{
		RetargetMode mode = detectArcRetargetMode(iarc);
		
		if (mode == RETARGET_AGGRESSIVE)
		{
			retargetArctoArcAggresive(rigg, iarc, inode_start);
		}
		else
		{		
			retargetArctoArcLength(rigg, iarc, inode_start);
		}
	}

#ifdef USE_THREADS
	MEM_freeN(p);
#endif
	
	return NULL;
}

static void matchMultiResolutionNode(RigGraph *rigg, RigNode *inode, ReebNode *top_node)
{
	ReebNode *enode = top_node;
	ReebGraph *reebg = BIF_graphForMultiNode(rigg->link_mesh, enode);
	int ishape, eshape;
	
	ishape = BLI_subtreeShape((BGraph*)rigg, (BNode*)inode, NULL, 0) % SHAPE_LEVELS;
	eshape = BLI_subtreeShape((BGraph*)reebg, (BNode*)enode, NULL, 0) % SHAPE_LEVELS;
	
	inode->link_mesh = enode;

	while (ishape == eshape && enode->link_down)
	{
		inode->link_mesh = enode;

		enode = enode->link_down;
		reebg = BIF_graphForMultiNode(rigg->link_mesh, enode); /* replace with call to link_down once that exists */
		eshape = BLI_subtreeShape((BGraph*)reebg, (BNode*)enode, NULL, 0) % SHAPE_LEVELS;
	} 
}

static void markMultiResolutionChildArc(ReebNode *end_enode, ReebNode *enode)
{
	int i;
	
	for(i = 0; i < enode->degree; i++)
	{
		ReebArc *earc = (ReebArc*)enode->arcs[i];
		
		if (earc->flag == ARC_FREE)
		{
			earc->flag = ARC_TAKEN;
			
			if (earc->tail->degree > 1 && earc->tail != end_enode)
			{
				markMultiResolutionChildArc(end_enode, earc->tail);
			}
			break;
		}
	}
}

static void markMultiResolutionArc(ReebArc *start_earc)
{
	if (start_earc->link_up)
	{
		ReebArc *earc;
		for (earc = start_earc->link_up ; earc; earc = earc->link_up)
		{
			earc->flag = ARC_TAKEN;
			
			if (earc->tail->index != start_earc->tail->index)
			{
				markMultiResolutionChildArc(earc->tail, earc->tail);
			}
		}
	}
}

static void matchMultiResolutionArc(RigGraph *rigg, RigNode *start_node, RigArc *next_iarc, ReebArc *next_earc)
{
	ReebNode *enode = next_earc->head;
	ReebGraph *reebg = BIF_graphForMultiNode(rigg->link_mesh, enode);
	int ishape, eshape;

	ishape = BLI_subtreeShape((BGraph*)rigg, (BNode*)start_node, (BArc*)next_iarc, 1) % SHAPE_LEVELS;
	eshape = BLI_subtreeShape((BGraph*)reebg, (BNode*)enode, (BArc*)next_earc, 1) % SHAPE_LEVELS;
	
	while (ishape != eshape && next_earc->link_up)
	{
		next_earc->flag = ARC_TAKEN; // mark previous as taken, to prevent backtrack on lower levels
		
		next_earc = next_earc->link_up;
		reebg = reebg->link_up;
		enode = next_earc->head;
		eshape = BLI_subtreeShape((BGraph*)reebg, (BNode*)enode, (BArc*)next_earc, 1) % SHAPE_LEVELS;
	} 

	next_earc->flag = ARC_USED;
	next_iarc->link_mesh = next_earc;
	
	/* mark all higher levels as taken too */
	markMultiResolutionArc(next_earc);
//	while (next_earc->link_up)
//	{
//		next_earc = next_earc->link_up;
//		next_earc->flag = ARC_TAKEN;
//	}
}

static void matchMultiResolutionStartingNode(RigGraph *rigg, ReebGraph *reebg, RigNode *inode)
{
	ReebNode *enode;
	int ishape, eshape;
	
	enode = reebg->nodes.first;
	
	ishape = BLI_subtreeShape((BGraph*)rigg, (BNode*)inode, NULL, 0) % SHAPE_LEVELS;
	eshape = BLI_subtreeShape((BGraph*)rigg->link_mesh, (BNode*)enode, NULL, 0) % SHAPE_LEVELS;
	
	while (ishape != eshape && reebg->link_up)
	{
		reebg = reebg->link_up;
		
		enode = reebg->nodes.first;
		
		eshape = BLI_subtreeShape((BGraph*)reebg, (BNode*)enode, NULL, 0) % SHAPE_LEVELS;
	} 

	inode->link_mesh = enode;
}

static void findCorrespondingArc(RigGraph *rigg, RigArc *start_arc, RigNode *start_node, RigArc *next_iarc, int root)
{
	ReebNode *enode = start_node->link_mesh;
	ReebArc *next_earc;
	int symmetry_level = next_iarc->symmetry_level;
	int symmetry_group = next_iarc->symmetry_group;
	int symmetry_flag = next_iarc->symmetry_flag;
	int i;
	
	next_iarc->link_mesh = NULL;
		
//	if (root)
//	{
//		printf("-----------------------\n");
//		printf("MATCHING LIMB\n");
//		RIG_printArcBones(next_iarc);
//	}
	
	for(i = 0; i < enode->degree; i++)
	{
		next_earc = (ReebArc*)enode->arcs[i];
		
//		if (next_earc->flag == ARC_FREE)
//		{
//			printf("candidate (level %i ?= %i) (flag %i ?= %i) (group %i ?= %i)\n",
//			symmetry_level, next_earc->symmetry_level,
//			symmetry_flag, next_earc->symmetry_flag, 
//			symmetry_group, next_earc->symmetry_flag);
//		}
		
		if (next_earc->flag == ARC_FREE &&
			next_earc->symmetry_flag == symmetry_flag &&
			next_earc->symmetry_group == symmetry_group &&
			next_earc->symmetry_level == symmetry_level)
		{
//			printf("CORRESPONDING ARC FOUND\n");
//			printf("flag %i -- level %i -- flag %i -- group %i\n", next_earc->flag, next_earc->symmetry_level, next_earc->symmetry_flag, next_earc->symmetry_group);
			
			matchMultiResolutionArc(rigg, start_node, next_iarc, next_earc);
			break;
		}
	}
	
	/* not found, try at higher nodes (lower node might have filtered internal arcs, messing shape of tree */
	if (next_iarc->link_mesh == NULL)
	{
//		printf("NO CORRESPONDING ARC FOUND - GOING TO HIGHER LEVELS\n");
		
		if (enode->link_up)
		{
			start_node->link_mesh = enode->link_up;
			findCorrespondingArc(rigg, start_arc, start_node, next_iarc, 0);
		}
	}

	/* still not found, print debug info */
	if (root && next_iarc->link_mesh == NULL)
	{
		start_node->link_mesh = enode; /* linking back with root node */
		
//		printf("NO CORRESPONDING ARC FOUND\n");
//		RIG_printArcBones(next_iarc);
//		
//		printf("ON NODE %i, multilevel %i\n", enode->index, enode->multi_level);
//		
//		printf("LOOKING FOR\n");
//		printf("flag %i -- level %i -- flag %i -- group %i\n", ARC_FREE, symmetry_level, symmetry_flag, symmetry_group);
//		
//		printf("CANDIDATES\n");
//		for(i = 0; i < enode->degree; i++)
//		{
//			next_earc = (ReebArc*)enode->arcs[i];
//			printf("flag %i -- level %i -- flag %i -- group %i\n", next_earc->flag, next_earc->symmetry_level, next_earc->symmetry_flag, next_earc->symmetry_group);
//		}
		
		/* Emergency matching */
		for(i = 0; i < enode->degree; i++)
		{
			next_earc = (ReebArc*)enode->arcs[i];
			
			if (next_earc->flag == ARC_FREE && next_earc->symmetry_level == symmetry_level)
			{
//				printf("USING: \n");
//				printf("flag %i -- level %i -- flag %i -- group %i\n", next_earc->flag, next_earc->symmetry_level, next_earc->symmetry_flag, next_earc->symmetry_group);
				matchMultiResolutionArc(rigg, start_node, next_iarc, next_earc);
				break;
			}
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
		
		retargetArctoArc(rigg, start_arc, start_node);
		
		enode = BIF_otherNodeFromIndex(earc, enode);
		inode = (RigNode*)BLI_otherNode((BArc*)start_arc, (BNode*)inode);
	
		/* match with lowest node with correct shape */
		matchMultiResolutionNode(rigg, inode, enode);
	}
	
	for(i = 0; i < inode->degree; i++)
	{
		RigArc *next_iarc = (RigArc*)inode->arcs[i];
		
		/* no back tracking */
		if (next_iarc != start_arc)
		{
			findCorrespondingArc(rigg, start_arc, inode, next_iarc, 1);
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
	
	/* flag all ReebArcs as free */
	BIF_flagMultiArcs(reebg, ARC_FREE);
	
	/* return to first level */
	reebg = rigg->link_mesh;
	
	inode = rigg->head;
	
	matchMultiResolutionStartingNode(rigg, reebg, inode);

	retargetSubgraph(rigg, NULL, inode);
	
	//generateMissingArcs(rigg);
	
#ifdef USE_THREADS
	BLI_destroy_worker(rigg->worker);
#endif
}

void BIF_retargetArmature()
{
	Object *ob;
	Base *base;
	ReebGraph *reebg;
	double start_time, end_time;
	
	start_time = PIL_check_seconds_timer();
	
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
			
				rigg = armatureToGraph(ob, &list);
				
				printf("Armature graph created\n");
		
				//RIG_printGraph(rigg);
				
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
	
	end_time = PIL_check_seconds_timer();

	printf("-----------\n");
	printf("runtime: %.3f\n", end_time - start_time);
	printf("-----------\n");
	
	BIF_undo_push("Retarget Skeleton");
	
	exit_editmode(EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR); // freedata, and undo

	allqueue(REDRAWVIEW3D, 0);
}
