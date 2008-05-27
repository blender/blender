/**
 * $Id: editarmature.c 14848 2008-05-15 08:05:56Z aligorith $
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
	ListBase arcs;
	ListBase nodes;
	struct RigNode *head;
	
	ReebGraph *link;
} RigGraph;

typedef struct RigNode {
	struct RigNode *next, *prev;
	float p[3];
	int degree;
	struct RigArc **arcs;
	int flag;
	
	int symmetry_level;
	int symmetry_flag;
	float symmetry_axis[3];

	ReebNode *link;
} RigNode;

typedef struct RigArc {
	struct RigArc *next, *prev;
	RigNode *head, *tail;
	ListBase edges;
	float length;
	int flag;

	int symmetry_level;
	int symmetry_flag;
	
	int count;
	ReebArc *link;
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
void RIG_markdownSymmetry(RigGraph *rg);
void RIG_markdownSymmetryArc(RigArc *arc, RigNode *node, int level);
void RIG_markdownSecondarySymmetry(RigNode *node, int depth, int level);


/*******************************************************************************************************/
static RigNode *RIG_otherNode(RigArc *arc, RigNode *node)
{
	if (arc->head == node)
		return arc->tail;
	else
		return arc->head;
}

static void RIG_flagNodes(RigGraph *rg, int flag)
{
	RigNode *node;
	
	for(node = rg->nodes.first; node; node = node->next)
	{
		node->flag = flag;
	}
}

static void RIG_flagArcs(RigGraph *rg, int flag)
{
	RigArc *arc;
	
	for(arc = rg->arcs.first; arc; arc = arc->next)
	{
		arc->flag = flag;
	}
}

static void RIG_addArcToNodeAdjacencyList(RigNode *node, RigArc *arc)
{
	node->arcs[node->degree] = arc;
	node->degree++;
}
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

/************************************* ALLOCATORS ******************************************************/

static RigGraph *newRigGraph()
{
	RigGraph *rg;
	rg = MEM_callocN(sizeof(RigGraph), "rig graph");
	
	rg->head = NULL;
	
	return rg;
}

static RigArc *newRigArc(RigGraph *rg)
{
	RigArc *arc;
	
	arc = MEM_callocN(sizeof(RigArc), "rig arc");
	arc->length = 0;
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

/************************************ DESTRUCTORS ******************************************************/

static void RIG_freeRigNode(RigNode *node)
{
	if (node->arcs)
	{
		MEM_freeN(node->arcs);
	}
}

static void RIG_freeRigArc(RigArc *arc)
{
	BLI_freelistN(&arc->edges);
}

static void RIG_freeRigGraph(RigGraph *rg)
{
	RigNode *node;
	RigArc *arc;
	
	for (arc = rg->arcs.first; arc; arc = arc->next)
	{
		RIG_freeRigArc(arc);
	}
	BLI_freelistN(&rg->arcs);
	
	for (node = rg->nodes.first; node; node = node->next)
	{
		RIG_freeRigNode(node);
	}
	BLI_freelistN(&rg->nodes);
	
	MEM_freeN(rg);
}

/*******************************************************************************************************/

static void RIG_buildAdjacencyList(RigGraph *rg)
{
	RigNode *node;
	RigArc *arc;

	for(node = rg->nodes.first; node; node = node->next)
	{
		if (node->arcs != NULL)
		{
			MEM_freeN(node->arcs);
		}
		
		node->arcs = MEM_callocN((node->degree + 1) * sizeof(RigArc*), "adjacency list");
		
		/* temporary use to indicate the first index available in the lists */
		node->degree = 0;
	}

	for(arc = rg->arcs.first; arc; arc= arc->next)
	{
		RIG_addArcToNodeAdjacencyList(arc->head, arc);
		RIG_addArcToNodeAdjacencyList(arc->tail, arc);
	}
}

static void RIG_replaceNode(RigGraph *rg, RigNode *node_src, RigNode *node_replaced)
{
	RigArc *arc, *next_arc;
	
	for (arc = rg->arcs.first; arc; arc = next_arc)
	{
		next_arc = arc->next;
		
		if (arc->head == node_replaced)
		{
			arc->head = node_src;
			node_src->degree++;
		}

		if (arc->tail == node_replaced)
		{
			arc->tail = node_src;
			node_src->degree++;
		}
		
		if (arc->head == arc->tail)
		{
			node_src->degree -= 2;
			
			RIG_freeRigArc(arc);
			BLI_freelinkN(&rg->arcs, arc);
		}
	}
}

static void RIG_removeDoubleNodes(RigGraph *rg, float limit)
{
	RigNode *node_src, *node_replaced;
	
	for(node_src = rg->nodes.first; node_src; node_src = node_src->next)
	{
		for(node_replaced = rg->nodes.first; node_replaced; node_replaced = node_replaced->next)
		{
			if (node_replaced != node_src && VecLenf(node_replaced->p, node_src->p) <= limit)
			{
				RIG_replaceNode(rg, node_src, node_replaced);
			}
		}
	}
	
}

static void RIG_calculateEdgeAngle(RigEdge *edge_first, RigEdge *edge_second)
{
	float vec_first[3], vec_second[3];
	
	VecSubf(vec_first, edge_first->tail, edge_first->head); 
	VecSubf(vec_second, edge_second->tail, edge_second->head);

	Normalize(vec_first);
	Normalize(vec_second);
	
	edge_first->angle = saacos(Inpf(vec_first, vec_second));
}

/*********************************** GRAPH AS TREE FUNCTIONS *******************************************/

int RIG_subtreeDepth(RigNode *node, RigArc *rootArc)
{
	int depth = 0;
	
	/* Base case, no arcs leading away */
	if (node->arcs == NULL || *(node->arcs) == NULL)
	{
		return 0;
	}
	else
	{
		RigArc ** pArc;

		for(pArc = node->arcs; *pArc; pArc++)
		{
			RigArc *arc = *pArc;
			
			/* only arcs that go down the tree */
			if (arc != rootArc)
			{
				RigNode *newNode = RIG_otherNode(arc, node);
				depth = MAX2(depth, RIG_subtreeDepth(newNode, arc));
			}
		}
	}
	
	return depth + BLI_countlist(&rootArc->edges);
}

int RIG_countConnectedArcs(RigGraph *rg, RigNode *node)
{
	int count = 0;
	
	/* use adjacency list if present */
	if (node->arcs)
	{
		RigArc **arcs;
	
		for(arcs = node->arcs; *arcs; arcs++)
		{
			count++;
		}
	}
	else
	{
		RigArc *arc;
		for(arc = rg->arcs.first; arc; arc = arc->next)
		{
			if (arc->head == node || arc->tail == node)
			{
				count++;
			}
		}
	}
	
	return count;
}

/********************************* SYMMETRY DETECTION **************************************************/

static void mirrorAlongAxis(float v[3], float center[3], float axis[3])
{
	float dv[3], pv[3];
	
	VecSubf(dv, v, center);
	Projf(pv, dv, axis);
	VecMulf(pv, -2);
	VecAddf(v, v, pv);
}

/* Helper structure for radial symmetry */
typedef struct RadialArc
{
	RigArc *arc; 
	float n[3]; /* normalized vector joining the nodes of the arc */
} RadialArc;

void RIG_markRadialSymmetry(RigNode *node, int depth, float axis[3])
{
	RadialArc *ring = NULL;
	RadialArc *unit;
	float limit = G.scene->toolsettings->skgen_symmetry_limit;
	int symmetric = 1;
	int count = 0;
	int i;

	/* mark topological symmetry */
	node->symmetry_flag |= SYM_TOPOLOGICAL;

	/* count the number of arcs in the symmetry ring */
	for (i = 0; node->arcs[i] != NULL; i++)
	{
		RigArc *connectedArc = node->arcs[i];
		
		/* depth is store as a negative in flag. symmetry level is positive */
		if (connectedArc->symmetry_level == -depth)
		{
			count++;
		}
	}

	ring = MEM_callocN(sizeof(RadialArc) * count, "radial symmetry ring");
	unit = ring;

	/* fill in the ring */
	for (unit = ring, i = 0; node->arcs[i] != NULL; i++)
	{
		RigArc *connectedArc = node->arcs[i];
		
		/* depth is store as a negative in flag. symmetry level is positive */
		if (connectedArc->symmetry_level == -depth)
		{
			RigNode *otherNode = RIG_otherNode(connectedArc, node);
			float vec[3];

			unit->arc = connectedArc;

			/* project the node to node vector on the symmetry plane */
			VecSubf(unit->n, otherNode->p, node->p);
			Projf(vec, unit->n, axis);
			VecSubf(unit->n, unit->n, vec);

			Normalize(unit->n);

			unit++;
		}
	}

	/* sort ring */
	for (i = 0; i < count - 1; i++)
	{
		float minAngle = 3; /* arbitrary high value, higher than 2, at least */
		int minIndex = -1;
		int j;

		for (j = i + 1; j < count; j++)
		{
			float angle = Inpf(ring[i].n, ring[j].n);

			/* map negative values to 1..2 */
			if (angle < 0)
			{
				angle = 1 - angle;
			}

			if (angle < minAngle)
			{
				minIndex = j;
				minAngle = angle;
			}
		}

		/* swap if needed */
		if (minIndex != i + 1)
		{
			RadialArc tmp;
			tmp = ring[i + 1];
			ring[i + 1] = ring[minIndex];
			ring[minIndex] = tmp;
		}
	}

	for (i = 0; i < count && symmetric; i++)
	{
		RigNode *node1, *node2;
		float tangent[3];
		float normal[3];
		float p[3];
		int j = (i + 1) % count; /* next arc in the circular list */

		VecAddf(tangent, ring[i].n, ring[j].n);
		Crossf(normal, tangent, axis);
		
		node1 = RIG_otherNode(ring[i].arc, node);
		node2 = RIG_otherNode(ring[j].arc, node);

		VECCOPY(p, node2->p);
		mirrorAlongAxis(p, node->p, normal);
		
		/* check if it's within limit before continuing */
		if (VecLenf(node1->p, p) > limit)
		{
			symmetric = 0;
		}

	}

	if (symmetric)
	{
		/* mark node as symmetric physically */
		VECCOPY(node->symmetry_axis, axis);
		node->symmetry_flag |= SYM_PHYSICAL;
		node->symmetry_flag |= SYM_RADIAL;
	}

	MEM_freeN(ring);
}

static void setSideAxialSymmetry(RigNode *root_node, RigNode *end_node, RigArc *arc)
{
	float vec[3];
	
	VecSubf(vec, end_node->p, root_node->p);
	
	if (Inpf(vec, root_node->symmetry_axis) < 0)
	{
		arc->symmetry_flag |= SYM_SIDE_NEGATIVE;
	}
	else
	{
		arc->symmetry_flag |= SYM_SIDE_POSITIVE;
	}
}

void RIG_markAxialSymmetry(RigNode *node, int depth, float axis[3])
{
	RigArc *arc1 = NULL;
	RigArc *arc2 = NULL;
	RigNode *node1 = NULL, *node2 = NULL;
	float limit = G.scene->toolsettings->skgen_symmetry_limit;
	float nor[3], vec[3], p[3];
	int i;
	
	/* mark topological symmetry */
	node->symmetry_flag |= SYM_TOPOLOGICAL;

	for (i = 0; node->arcs[i] != NULL; i++)
	{
		RigArc *connectedArc = node->arcs[i];
		
		/* depth is store as a negative in flag. symmetry level is positive */
		if (connectedArc->symmetry_level == -depth)
		{
			if (arc1 == NULL)
			{
				arc1 = connectedArc;
				node1 = RIG_otherNode(arc1, node);
			}
			else
			{
				arc2 = connectedArc;
				node2 = RIG_otherNode(arc2, node);
				break; /* Can stop now, the two arcs have been found */
			}
		}
	}
	
	/* shouldn't happen, but just to be sure */
	if (node1 == NULL || node2 == NULL)
	{
		return;
	}
	
	VecSubf(vec, node1->p, node->p);
	Normalize(vec);
	VecSubf(p, node->p, node2->p);
	Normalize(p);
	VecAddf(p, p, vec);

	Crossf(vec, p, axis);
	Crossf(nor, vec, axis);
	
	/* mirror node2 along axis */
	VECCOPY(p, node2->p);
	mirrorAlongAxis(p, node->p, nor);
	
	/* check if it's within limit before continuing */
	if (VecLenf(node1->p, p) <= limit)
	{
		/* mark node as symmetric physically */
		VECCOPY(node->symmetry_axis, nor);
		node->symmetry_flag |= SYM_PHYSICAL;
		node->symmetry_flag |= SYM_AXIAL;

		/* set side on arcs */
		setSideAxialSymmetry(node, node1, arc1);
		setSideAxialSymmetry(node, node2, arc2);
		printf("flag: %i <-> %i\n", arc1->symmetry_flag, arc2->symmetry_flag);
	}
	else
	{
		printf("NOT SYMMETRIC!\n");
		printf("%f <= %f\n", VecLenf(node1->p, p), limit);
		printvecf("axis", nor);
	}
}

void RIG_markdownSecondarySymmetry(RigNode *node, int depth, int level)
{
	float axis[3] = {0, 0, 0};
	int count = 0;
	int i;

	/* count the number of branches in this symmetry group
	 * and determinte the axis of symmetry
	 *  */	
	for (i = 0; node->arcs[i] != NULL; i++)
	{
		RigArc *connectedArc = node->arcs[i];
		
		/* depth is store as a negative in flag. symmetry level is positive */
		if (connectedArc->symmetry_level == -depth)
		{
			count++;
		}
		/* If arc is on the axis */
		else if (connectedArc->symmetry_level == level)
		{
			VecAddf(axis, axis, connectedArc->head->p);
			VecSubf(axis, axis, connectedArc->tail->p);
		}
	}

	Normalize(axis);

	/* Split between axial and radial symmetry */
	if (count == 2)
	{
		RIG_markAxialSymmetry(node, depth, axis);
	}
	else
	{
		RIG_markRadialSymmetry(node, depth, axis);
	}
	
	/* markdown secondary symetries */	
	for (i = 0; node->arcs[i] != NULL; i++)
	{
		RigArc *connectedArc = node->arcs[i];
		
		if (connectedArc->symmetry_level == -depth)
		{
			/* markdown symmetry for branches corresponding to the depth */
			RIG_markdownSymmetryArc(connectedArc, node, level + 1);
		}
	}
}

void RIG_markdownSymmetryArc(RigArc *arc, RigNode *node, int level)
{
	int i;
	arc->symmetry_level = level;
	
	node = RIG_otherNode(arc, node);
	
	for (i = 0; node->arcs[i] != NULL; i++)
	{
		RigArc *connectedArc = node->arcs[i];
		
		if (connectedArc != arc)
		{
			RigNode *connectedNode = RIG_otherNode(connectedArc, node);
			
			/* symmetry level is positive value, negative values is subtree depth */
			connectedArc->symmetry_level = -RIG_subtreeDepth(connectedNode, connectedArc);
		}
	}

	arc = NULL;

	for (i = 0; node->arcs[i] != NULL; i++)
	{
		int issymmetryAxis = 0;
		RigArc *connectedArc = node->arcs[i];
		
		/* only arcs not already marked as symetric */
		if (connectedArc->symmetry_level < 0)
		{
			int j;
			
			/* true by default */
			issymmetryAxis = 1;
			
			for (j = 0; node->arcs[j] != NULL && issymmetryAxis == 1; j++)
			{
				RigArc *otherArc = node->arcs[j];
				
				/* different arc, same depth */
				if (otherArc != connectedArc && otherArc->symmetry_level == connectedArc->symmetry_level)
				{
					/* not on the symmetry axis */
					issymmetryAxis = 0;
				} 
			}
		}
		
		/* arc could be on the symmetry axis */
		if (issymmetryAxis == 1)
		{
			/* no arc as been marked previously, keep this one */
			if (arc == NULL)
			{
				arc = connectedArc;
			}
			else
			{
				/* there can't be more than one symmetry arc */
				arc = NULL;
				break;
			}
		}
	}
	
	/* go down the arc continuing the symmetry axis */
	if (arc)
	{
		RIG_markdownSymmetryArc(arc, node, level);
	}

	
	/* secondary symmetry */
	for (i = 0; node->arcs[i] != NULL; i++)
	{
		RigArc *connectedArc = node->arcs[i];
		
		/* only arcs not already marked as symetric and is not the next arc on the symmetry axis */
		if (connectedArc->symmetry_level < 0)
		{
			/* subtree depth is store as a negative value in the symmetry */
			RIG_markdownSecondarySymmetry(node, -connectedArc->symmetry_level, level);
		}
	}
}

void RIG_markdownSymmetry(RigGraph *rg)
{
	RigNode *node;
	RigArc *arc;
	
	/* mark down all arcs as non-symetric */
	RIG_flagArcs(rg, 0);
	
	/* mark down all nodes as not on the symmetry axis */
	RIG_flagNodes(rg, 0);

	if (rg->head)
	{
		node = rg->head;
	}
	else
	{
		/* !TODO! DO SOMETHING SMART HERE */
		return;
	}
	
	/* only work on acyclic graphs and if only one arc is incident on the first node */
	if (RIG_countConnectedArcs(rg, node) == 1)
	{
		arc = node->arcs[0];
		
		RIG_markdownSymmetryArc(arc, node, 1);

		/* mark down non-symetric arcs */
		for (arc = rg->arcs.first; arc; arc = arc->next)
		{
			if (arc->symmetry_level < 0)
			{
				arc->symmetry_level = 0;
			}
			else
			{
				/* mark down nodes with the lowest level symmetry axis */
				if (arc->head->symmetry_level == 0 || arc->head->symmetry_level > arc->symmetry_level)
				{
					arc->head->symmetry_level = arc->symmetry_level;
				}
				if (arc->tail->symmetry_level == 0 || arc->tail->symmetry_level > arc->symmetry_level)
				{
					arc->tail->symmetry_level = arc->symmetry_level;
				}
			}
		}
	}
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
			
			rg->head = arc->head;
		}
	}
}

/*******************************************************************************************************/

static void RIG_printNode(RigNode *node, char name[])
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

static void RIG_printArcBones(RigArc *arc)
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

static void RIG_printArc(RigArc *arc)
{
	RigEdge *edge;

	printf("\n");

	RIG_printNode(arc->head, "head");

	for (edge = arc->edges.first; edge; edge = edge->next)
	{
		printf("\tinner joints %0.3f %0.3f %0.3f\n", edge->tail[0], edge->tail[1], edge->tail[2]);
		printf("\t\tlength %f\n", edge->length);
		printf("\t\tangle %f\n", edge->angle * 180 / M_PI);
		if (edge->bone)
			printf("\t\t%s\n", edge->bone->name);
	}	
	printf("symmetry level: %i\n", arc->symmetry_level);

	RIG_printNode(arc->tail, "tail");
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
	
	RIG_removeDoubleNodes(rg, 0);
	
	RIG_buildAdjacencyList(rg);
	
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
	ReebArc *earc = iarc->link;
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

static void retargetArctoArcAggresive(RigArc *iarc)
{
	ReebArcIterator iter;
	RigEdge *edge;
	EmbedBucket *bucket = NULL;
	ReebNode *node_start, *node_end;
	ReebArc *earc = iarc->link;
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

	positions = MEM_callocN(sizeof(int) * nb_joints, "Aggresive positions");
	best_positions = MEM_callocN(sizeof(int) * nb_joints, "Best Aggresive positions");
	cost_cache = MEM_callocN(sizeof(float) * nb_edges, "Cost cache");
	vec_cache = MEM_callocN(sizeof(float*) * (nb_edges + 1), "Vec cache");
	
	/* symmetry axis */
	if (earc->symmetry_level == 1 && iarc->symmetry_level == 1)
	{
		symmetry_axis = 1;
		node_start = earc->v2;
		node_end = earc->v1;
	}
	else
	{
		node_start = earc->v1;
		node_end = earc->v2;
	}
	
	/* init with first values */
	for (i = 0; i < nb_joints; i++)
	{
		positions[i] = i + 1;
	}
	
	/* init cost cache */
	for (i = 0; i < nb_edges; i++)
	{
		cost_cache[i] = 0;
	}
	
	vec_cache[0] = node_start->p;
	vec_cache[nb_edges] = node_end->p;

	while(1)
	{
		float cost = 0;
		int need_calc = 0;
		
		/* increment to next possible solution */
		
		i = nb_joints - 1;

		/* increment positions, starting from the last one
		 * until a valid increment is found
		 * */
		for (i = must_move; i >= 0; i--)
		{
			int remaining_joints = nb_joints - (i + 1); 
			
			positions[i] += 1;
			need_calc = i;
			if (positions[i] + remaining_joints < earc->bcount)
			{
				break;
			}
		}
		
		if (first_pass)
		{
			need_calc = 0;
			first_pass = 0;
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
				
				if (i < nb_joints)
				{
					bucket = nextNBucket(&iter, positions[i] - last_index);
					vec2 = bucket->p;
					vec_cache[i + 1] = vec2; /* update cache for updated position */
				}
				else
				{
					vec2 = node_end->p;
				}
				
				vec1 = vec_cache[i];
				

				VecSubf(vec_second, vec2, vec1);
				length2 = Normalize(vec_second);
	
				/* check angle */
				if (i != 0)
				{
					RigEdge *previous = edge->prev;
					float angle = previous->angle;
					float test_angle = previous->angle;
					
					vec0 = vec_cache[i - 1];
					VecSubf(vec_first, vec1, vec0); 
					length1 = Normalize(vec_first);
					
					if (length1 > 0 && length2 > 0)
					{
						test_angle = saacos(Inpf(vec_first, vec_second));
						/* ANGLE COST HERE */
						new_cost += G.scene->toolsettings->skgen_retarget_angle_weight * fabs((test_angle - angle) / test_angle);
					}
					else
					{
						new_cost += M_PI;
					}
				}
	
				/* LENGTH COST HERE */
				new_cost += G.scene->toolsettings->skgen_retarget_length_weight * fabs((length2 - edge->length) / edge->length);
				cost_cache[i] = new_cost;
				
				last_index =  positions[i];
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

	vec0 = node_start->p;
	initArcIterator(&iter, earc, node_start);
	
	printPositions(best_positions, nb_joints);
	printf("buckets: %i\n", earc->bcount);

	/* set joints to best position */
	for (edge = iarc->edges.first, i = 0, last_index = 0;
		 edge;
		 edge = edge->next, i++)
	{
		EditBone *bone = edge->bone;
		
		if (i < nb_joints)
		{
			bucket = nextNBucket(&iter, best_positions[i] - last_index);
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
			printf("position: %i\n", best_positions[i]);
			printf("last_index: %i\n", last_index);
		}
		
		vec0 = vec1;
		last_index =  best_positions[i];
	}
	
	MEM_freeN(positions);
	MEM_freeN(best_positions);
	MEM_freeN(cost_cache);
	MEM_freeN(vec_cache);
}

static void retargetArctoArcLength(RigArc *iarc)
{
	ReebArcIterator iter;
	ReebArc *earc = iarc->link;
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
		node_start = earc->v2;
		node_end = earc->v1;
	}
	else
	{
		node_start = earc->v1;
		node_end = earc->v2;
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
	ReebArc *earc = iarc->link;
	
	if (BLI_countlist(&iarc->edges) == 1)
	{
		RigEdge *edge = iarc->edges.first;
		EditBone *bone = edge->bone;
		
		/* symmetry axis */
		if (earc->symmetry_level == 1 && iarc->symmetry_level == 1)
		{
			VECCOPY(bone->head, earc->v2->p);
			VECCOPY(bone->tail, earc->v1->p);
		}
		/* or not */
		else
		{
			VECCOPY(bone->head, earc->v1->p);
			VECCOPY(bone->tail, earc->v2->p);
		}
	}
	else
	{
		RetargetMode mode = detectArcRetargetMode(iarc);
		
		if (mode == RETARGET_AGGRESSIVE)
		{
			printf("aggresive\n");
			retargetArctoArcAggresive(iarc);
		}
		else
		{		
			retargetArctoArcLength(iarc);
		}
	}
}

static void findCorrespondingArc(RigArc *start_arc, RigNode *start_node, RigArc *next_iarc)
{
	ReebNode *enode = start_node->link;
	ReebArc *next_earc;
	int symmetry_level = next_iarc->symmetry_level;
	int symmetry_flag = next_iarc->symmetry_flag;
	int i;
	
	next_iarc->link = NULL;
		
	for(i = 0, next_earc = enode->arcs[i]; next_earc; i++, next_earc = enode->arcs[i])
	{
		if (next_earc->flag == 0 && /* not already taken */
			next_earc->symmetry_flag == symmetry_flag &&
			next_earc->symmetry_level == symmetry_level)
		{
			printf("-----------------------\n");
			printf("CORRESPONDING ARC FOUND\n");
			RIG_printArcBones(next_iarc);

			next_earc->flag = 1; // mark as taken
			next_iarc->link = next_earc;
			break;
		}
	}
	
	if (next_iarc->link == NULL)
	{
		printf("--------------------------\n");
		printf("NO CORRESPONDING ARC FOUND\n");
		RIG_printArcBones(next_iarc);
		
		printf("LOOKING FOR\n");
		printf("flag %i -- symmetry level %i -- symmetry flag %i\n", 0, symmetry_level, symmetry_flag);
		
		printf("CANDIDATES\n");
		for(i = 0, next_earc = enode->arcs[i]; next_earc; i++, next_earc = enode->arcs[i])
		{
			printf("flag %i -- symmetry level %i -- symmetry flag %i\n", next_earc->flag, next_earc->symmetry_level, next_earc->symmetry_flag);
		}
	}
}

static void retargetSubgraph(RigGraph *rigg, RigArc *start_arc, RigNode *start_node)
{
	RigArc *iarc = start_arc;
	ReebArc *earc = start_arc->link;
	RigNode *inode = start_node;
	ReebNode *enode = start_node->link;
	RigArc *next_iarc;
	int i;
		
	retargetArctoArc(iarc);
	
	enode = OTHER_NODE(earc, enode);
	inode = RIG_otherNode(iarc, inode);
	
	inode->link = enode;
	
	for(i = 0, next_iarc = inode->arcs[i]; next_iarc; i++, next_iarc = inode->arcs[i])
	{
		/* no back tracking */
		if (next_iarc != iarc)
		{
			findCorrespondingArc(iarc, inode, next_iarc);
			if (next_iarc->link)
			{
				retargetSubgraph(rigg, next_iarc, inode);
			}
		}
	}
}

static void retargetGraphs(RigGraph *rigg)
{
	ReebGraph *reebg = rigg->link;
	ReebArc *earc;
	RigArc *iarc;
	ReebNode *enode;
	RigNode *inode;
	
	/* flag all ReebArcs as not taken */
	for (earc = reebg->arcs.first; earc; earc = earc->next)
	{
		earc->flag = 0;
	}
	
	earc = reebg->arcs.first;
	iarc = rigg->head->arcs[0];
	
	iarc->link = earc;
	earc->flag = 1;
	
	enode = earc->v1;
	inode = iarc->tail;

	inode->link = enode;

	retargetSubgraph(rigg, iarc, inode);
}

void BIF_retargetArmature()
{
	Object *ob;
	Base *base;
	ReebGraph *reebg;
	
	reebg = BIF_ReebGraphFromEditMesh();
	
	markdownSymmetry(reebg);
	
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
				
				printf("Armature graph created\n");
		
				RIG_markdownSymmetry(rigg);
				
				RIG_printGraph(rigg);
				
				rigg->link = reebg;
				
				printf("retargetting %s\n", ob->id.name);
				
				retargetGraphs(rigg);
				
				/* Turn the list into an armature */
				editbones_to_armature(&list, ob);
				
				BLI_freelistN(&list);

				RIG_freeRigGraph(rigg);
			}
		}
	}

	REEB_freeGraph(reebg);
	
	BIF_undo_push("Retarget Skeleton");
	
	exit_editmode(EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR); // freedata, and undo

	allqueue(REDRAWVIEW3D, 0);
}
