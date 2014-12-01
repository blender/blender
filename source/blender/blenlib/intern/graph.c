/*
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
 * Contributor(s): Martin Poirier
 *
 * ***** END GPL LICENSE BLOCK *****
 * graph.c: Common graph interface and methods
 */

/** \file blender/blenlib/intern/graph.c
 *  \ingroup bli
 */

#include <float.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_graph.h"
#include "BLI_math.h"


static void testRadialSymmetry(BGraph *graph, BNode *root_node, RadialArc *ring, int total, float axis[3], float limit, int group);

static void handleAxialSymmetry(BGraph *graph, BNode *root_node, int depth, float axis[3], float limit);
static void testAxialSymmetry(BGraph *graph, BNode *root_node, BNode *node1, BNode *node2, BArc *arc1, BArc *arc2, float axis[3], float limit, int group);
static void flagAxialSymmetry(BNode *root_node, BNode *end_node, BArc *arc, int group);

void BLI_freeNode(BGraph *graph, BNode *node)
{
	if (node->arcs) {
		MEM_freeN(node->arcs);
	}
	
	if (graph->free_node) {
		graph->free_node(node);
	}
}

void BLI_removeNode(BGraph *graph, BNode *node)
{
	BLI_freeNode(graph, node);
	BLI_freelinkN(&graph->nodes, node);
}

BNode *BLI_otherNode(BArc *arc, BNode *node)
{
	return (arc->head == node) ? arc->tail : arc->head;
}

void BLI_removeArc(BGraph *graph, BArc *arc)
{
	if (graph->free_arc) {
		graph->free_arc(arc);
	}

	BLI_freelinkN(&graph->arcs, arc);
}

void BLI_flagNodes(BGraph *graph, int flag)
{
	BNode *node;
	
	for (node = graph->nodes.first; node; node = node->next) {
		node->flag = flag;
	}
}

void BLI_flagArcs(BGraph *graph, int flag)
{
	BArc *arc;
	
	for (arc = graph->arcs.first; arc; arc = arc->next) {
		arc->flag = flag;
	}
}

static void addArcToNodeAdjacencyList(BNode *node, BArc *arc)
{
	node->arcs[node->flag] = arc;
	node->flag++;
}

void BLI_buildAdjacencyList(BGraph *graph)
{
	BNode *node;
	BArc *arc;

	for (node = graph->nodes.first; node; node = node->next) {
		if (node->arcs != NULL) {
			MEM_freeN(node->arcs);
		}
		
		node->arcs = MEM_callocN((node->degree) * sizeof(BArc *), "adjacency list");
		
		/* temporary use to indicate the first index available in the lists */
		node->flag = 0;
	}

	for (arc = graph->arcs.first; arc; arc = arc->next) {
		addArcToNodeAdjacencyList(arc->head, arc);
		addArcToNodeAdjacencyList(arc->tail, arc);
	}

	for (node = graph->nodes.first; node; node = node->next) {
		if (node->degree != node->flag) {
			printf("error in node [%p]. Added only %i arcs out of %i\n", (void *)node, node->flag, node->degree);
		}
	}
}

void BLI_rebuildAdjacencyListForNode(BGraph *graph, BNode *node)
{
	BArc *arc;

	if (node->arcs != NULL) {
		MEM_freeN(node->arcs);
	}
	
	node->arcs = MEM_callocN((node->degree) * sizeof(BArc *), "adjacency list");
	
	/* temporary use to indicate the first index available in the lists */
	node->flag = 0;

	for (arc = graph->arcs.first; arc; arc = arc->next) {
		if (arc->head == node) {
			addArcToNodeAdjacencyList(arc->head, arc);
		}
		else if (arc->tail == node) {
			addArcToNodeAdjacencyList(arc->tail, arc);
		}
	}

	if (node->degree != node->flag) {
		printf("error in node [%p]. Added only %i arcs out of %i\n", (void *)node, node->flag, node->degree);
	}
}

void BLI_freeAdjacencyList(BGraph *graph)
{
	BNode *node;

	for (node = graph->nodes.first; node; node = node->next) {
		if (node->arcs != NULL) {
			MEM_freeN(node->arcs);
			node->arcs = NULL;
		}
	}
}

bool BLI_hasAdjacencyList(BGraph *graph)
{
	BNode *node;
	
	for (node = graph->nodes.first; node; node = node->next) {
		if (node->arcs == NULL) {
			return false;
		}
	}
	
	return true;
}

void BLI_replaceNodeInArc(BGraph *graph, BArc *arc, BNode *node_src, BNode *node_replaced)
{
	if (arc->head == node_replaced) {
		arc->head = node_src;
		node_src->degree++;
	}

	if (arc->tail == node_replaced) {
		arc->tail = node_src;
		node_src->degree++;
	}
	
	if (arc->head == arc->tail) {
		node_src->degree -= 2;
		
		graph->free_arc(arc);
		BLI_freelinkN(&graph->arcs, arc);
	}

	if (node_replaced->degree == 0) {
		BLI_removeNode(graph, node_replaced);
	}
}

void BLI_replaceNode(BGraph *graph, BNode *node_src, BNode *node_replaced)
{
	BArc *arc, *next_arc;
	
	for (arc = graph->arcs.first; arc; arc = next_arc) {
		next_arc = arc->next;
		
		if (arc->head == node_replaced) {
			arc->head = node_src;
			node_replaced->degree--;
			node_src->degree++;
		}

		if (arc->tail == node_replaced) {
			arc->tail = node_src;
			node_replaced->degree--;
			node_src->degree++;
		}
		
		if (arc->head == arc->tail) {
			node_src->degree -= 2;
			
			graph->free_arc(arc);
			BLI_freelinkN(&graph->arcs, arc);
		}
	}
	
	if (node_replaced->degree == 0) {
		BLI_removeNode(graph, node_replaced);
	}
}

void BLI_removeDoubleNodes(BGraph *graph, float limit)
{
	const float limit_sq = limit * limit;
	BNode *node_src, *node_replaced;
	
	for (node_src = graph->nodes.first; node_src; node_src = node_src->next) {
		for (node_replaced = graph->nodes.first; node_replaced; node_replaced = node_replaced->next) {
			if (node_replaced != node_src && len_squared_v3v3(node_replaced->p, node_src->p) <= limit_sq) {
				BLI_replaceNode(graph, node_src, node_replaced);
			}
		}
	}
	
}

BNode *BLI_FindNodeByPosition(BGraph *graph, const float p[3], const float limit)
{
	const float limit_sq = limit * limit;
	BNode *closest_node = NULL, *node;
	float min_distance = 0.0f;
	
	for (node = graph->nodes.first; node; node = node->next) {
		float distance = len_squared_v3v3(p, node->p);
		if (distance <= limit_sq && (closest_node == NULL || distance < min_distance)) {
			closest_node = node;
			min_distance = distance;
		}
	}
	
	return closest_node;
}
/************************************* SUBGRAPH DETECTION **********************************************/

static void flagSubgraph(BNode *node, int subgraph)
{
	if (node->subgraph_index == 0) {
		BArc *arc;
		int i;
		
		node->subgraph_index = subgraph;
		
		for (i = 0; i < node->degree; i++) {
			arc = node->arcs[i];
			flagSubgraph(BLI_otherNode(arc, node), subgraph);
		}
	}
} 

int BLI_FlagSubgraphs(BGraph *graph)
{
	BNode *node;
	int subgraph = 0;

	if (BLI_hasAdjacencyList(graph) == 0) {
		BLI_buildAdjacencyList(graph);
	}
	
	for (node = graph->nodes.first; node; node = node->next) {
		node->subgraph_index = 0;
	}
	
	for (node = graph->nodes.first; node; node = node->next) {
		if (node->subgraph_index == 0) {
			subgraph++;
			flagSubgraph(node, subgraph);
		}
	}
	
	return subgraph;
}

void BLI_ReflagSubgraph(BGraph *graph, int old_subgraph, int new_subgraph)
{
	BNode *node;

	for (node = graph->nodes.first; node; node = node->next) {
		if (node->flag == old_subgraph) {
			node->flag = new_subgraph;
		}
	}
}

/*************************************** CYCLE DETECTION ***********************************************/

static bool detectCycle(BNode *node, BArc *src_arc)
{
	bool value = false;
	
	if (node->flag == 0) {
		int i;

		/* mark node as visited */
		node->flag = 1;

		for (i = 0; i < node->degree && value == 0; i++) {
			BArc *arc = node->arcs[i];
			
			/* don't go back on the source arc */
			if (arc != src_arc) {
				value = detectCycle(BLI_otherNode(arc, node), arc);
			}
		}
	}
	else {
		value = true;
	}
	
	return value;
}

bool BLI_isGraphCyclic(BGraph *graph)
{
	BNode *node;
	bool value = false;
	
	/* NEED TO CHECK IF ADJACENCY LIST EXIST */
	
	/* Mark all nodes as not visited */
	BLI_flagNodes(graph, 0);

	/* detectCycles in subgraphs */
	for (node = graph->nodes.first; node && value == false; node = node->next) {
		/* only for nodes in subgraphs that haven't been visited yet */
		if (node->flag == 0) {
			value = value || detectCycle(node, NULL);
		}
	}
	
	return value;
}

BArc *BLI_findConnectedArc(BGraph *graph, BArc *arc, BNode *v)
{
	BArc *nextArc;
	
	for (nextArc = graph->arcs.first; nextArc; nextArc = nextArc->next) {
		if (arc != nextArc && (nextArc->head == v || nextArc->tail == v)) {
			break;
		}
	}
	
	return nextArc;
}

/*********************************** GRAPH AS TREE FUNCTIONS *******************************************/

static int subtreeShape(BNode *node, BArc *rootArc, int include_root)
{
	int depth = 0;
	
	node->flag = 1;
	
	if (include_root) {
		BNode *newNode = BLI_otherNode(rootArc, node);
		return subtreeShape(newNode, rootArc, 0);
	}
	else {
		/* Base case, no arcs leading away */
		if (node->arcs == NULL || *(node->arcs) == NULL) {
			return 0;
		}
		else {
			int i;
	
			for (i = 0; i < node->degree; i++) {
				BArc *arc = node->arcs[i];
				BNode *newNode = BLI_otherNode(arc, node);
				
				/* stop immediate and cyclic backtracking */
				if (arc != rootArc && newNode->flag == 0) {
					depth += subtreeShape(newNode, arc, 0);
				}
			}
		}
		
		return SHAPE_RADIX * depth + 1;
	}
}

int BLI_subtreeShape(BGraph *graph, BNode *node, BArc *rootArc, int include_root)
{
	BLI_flagNodes(graph, 0);
	return subtreeShape(node, rootArc, include_root);
}

float BLI_subtreeLength(BNode *node)
{
	float length = 0;
	int i;

	node->flag = 0; /* flag node as visited */

	for (i = 0; i < node->degree; i++) {
		BArc *arc = node->arcs[i];
		BNode *other_node = BLI_otherNode(arc, node);
		
		if (other_node->flag != 0) {
			float subgraph_length = arc->length + BLI_subtreeLength(other_node); 
			length = MAX2(length, subgraph_length);
		}
	}
	
	return length;
}

void BLI_calcGraphLength(BGraph *graph)
{
	float length = 0;
	int nb_subgraphs;
	int i;
	
	nb_subgraphs = BLI_FlagSubgraphs(graph);
	
	for (i = 1; i <= nb_subgraphs; i++) {
		BNode *node;
		
		for (node = graph->nodes.first; node; node = node->next) {
			/* start on an external node  of the subgraph */
			if (node->subgraph_index == i && node->degree == 1) {
				float subgraph_length = BLI_subtreeLength(node);
				length = MAX2(length, subgraph_length);
				break;
			}
		}
	}
	
	graph->length = length;
}

/********************************* SYMMETRY DETECTION **************************************************/

static void markdownSymmetryArc(BGraph *graph, BArc *arc, BNode *node, int level, float limit);

void BLI_mirrorAlongAxis(float v[3], float center[3], float axis[3])
{
	float dv[3], pv[3];
	
	sub_v3_v3v3(dv, v, center);
	project_v3_v3v3(pv, dv, axis);
	mul_v3_fl(pv, -2);
	add_v3_v3(v, pv);
}

static void testRadialSymmetry(BGraph *graph, BNode *root_node, RadialArc *ring, int total, float axis[3], float limit, int group)
{
	const float limit_sq = limit * limit;
	int symmetric = 1;
	int i;
	
	/* sort ring by angle */
	for (i = 0; i < total - 1; i++) {
		float minAngle = FLT_MAX;
		int minIndex = -1;
		int j;

		for (j = i + 1; j < total; j++) {
			float angle = dot_v3v3(ring[i].n, ring[j].n);

			/* map negative values to 1..2 */
			if (angle < 0) {
				angle = 1 - angle;
			}

			if (angle < minAngle) {
				minIndex = j;
				minAngle = angle;
			}
		}

		/* swap if needed */
		if (minIndex != i + 1) {
			RadialArc tmp;
			tmp = ring[i + 1];
			ring[i + 1] = ring[minIndex];
			ring[minIndex] = tmp;
		}
	}

	for (i = 0; i < total && symmetric; i++) {
		BNode *node1, *node2;
		float tangent[3];
		float normal[3];
		float p[3];
		int j = (i + 1) % total; /* next arc in the circular list */

		add_v3_v3v3(tangent, ring[i].n, ring[j].n);
		cross_v3_v3v3(normal, tangent, axis);
		
		node1 = BLI_otherNode(ring[i].arc, root_node);
		node2 = BLI_otherNode(ring[j].arc, root_node);

		copy_v3_v3(p, node2->p);
		BLI_mirrorAlongAxis(p, root_node->p, normal);
		
		/* check if it's within limit before continuing */
		if (len_squared_v3v3(node1->p, p) > limit_sq) {
			symmetric = 0;
		}

	}

	if (symmetric) {
		/* mark node as symmetric physically */
		copy_v3_v3(root_node->symmetry_axis, axis);
		root_node->symmetry_flag |= SYM_PHYSICAL;
		root_node->symmetry_flag |= SYM_RADIAL;
		
		/* FLAG SYMMETRY GROUP */
		for (i = 0; i < total; i++) {
			ring[i].arc->symmetry_group = group;
			ring[i].arc->symmetry_flag = SYM_SIDE_RADIAL + i;
		}

		if (graph->radial_symmetry) {
			graph->radial_symmetry(root_node, ring, total);
		}
	}
}

static void handleRadialSymmetry(BGraph *graph, BNode *root_node, int depth, float axis[3], float limit)
{
	RadialArc *ring = NULL;
	RadialArc *unit;
	int total = 0;
	int group;
	int first;
	int i;

	/* mark topological symmetry */
	root_node->symmetry_flag |= SYM_TOPOLOGICAL;

	/* total the number of arcs in the symmetry ring */
	for (i = 0; i < root_node->degree; i++) {
		BArc *connectedArc = root_node->arcs[i];
		
		/* depth is store as a negative in flag. symmetry level is positive */
		if (connectedArc->symmetry_level == -depth) {
			total++;
		}
	}

	ring = MEM_callocN(sizeof(RadialArc) * total, "radial symmetry ring");
	unit = ring;

	/* fill in the ring */
	for (unit = ring, i = 0; i < root_node->degree; i++) {
		BArc *connectedArc = root_node->arcs[i];
		
		/* depth is store as a negative in flag. symmetry level is positive */
		if (connectedArc->symmetry_level == -depth) {
			BNode *otherNode = BLI_otherNode(connectedArc, root_node);
			float vec[3];

			unit->arc = connectedArc;

			/* project the node to node vector on the symmetry plane */
			sub_v3_v3v3(unit->n, otherNode->p, root_node->p);
			project_v3_v3v3(vec, unit->n, axis);
			sub_v3_v3v3(unit->n, unit->n, vec);

			normalize_v3(unit->n);

			unit++;
		}
	}

	/* sort ring by arc length
	 * using a rather bogus insertion sort
	 * but rings will never get too big to matter
	 * */
	for (i = 0; i < total; i++) {
		int j;

		for (j = i - 1; j >= 0; j--) {
			BArc *arc1, *arc2;
			
			arc1 = ring[j].arc;
			arc2 = ring[j + 1].arc;
			
			if (arc1->length > arc2->length) {
				/* swap with smaller */
				RadialArc tmp;
				
				tmp = ring[j + 1];
				ring[j + 1] = ring[j];
				ring[j] = tmp;
			}
			else {
				break;
			}
		}
	}

	/* Dispatch to specific symmetry tests */
	first = 0;
	group = 0;
	
	for (i = 1; i < total; i++) {
		int dispatch = 0;
		int last = i - 1;
		
		if (fabsf(ring[first].arc->length - ring[i].arc->length) > limit) {
			dispatch = 1;
		}

		/* if not dispatching already and on last arc
		 * Dispatch using current arc as last
		 */
		if (dispatch == 0 && i == total - 1) {
			last = i;
			dispatch = 1;
		}
		
		if (dispatch) {
			int sub_total = last - first + 1; 

			group += 1;

			if (sub_total == 1) {
				group -= 1; /* not really a group so decrement */
				/* NOTHING TO DO */
			}
			else if (sub_total == 2) {
				BArc *arc1, *arc2;
				BNode *node1, *node2;
				
				arc1 = ring[first].arc;
				arc2 = ring[last].arc;
				
				node1 = BLI_otherNode(arc1, root_node);
				node2 = BLI_otherNode(arc2, root_node);
				
				testAxialSymmetry(graph, root_node, node1, node2, arc1, arc2, axis, limit, group);
			}
			else if (sub_total != total) /* allocate a new sub ring if needed */ {
				RadialArc *sub_ring = MEM_callocN(sizeof(RadialArc) * sub_total, "radial symmetry ring");
				int sub_i;
				
				/* fill in the sub ring */
				for (sub_i = 0; sub_i < sub_total; sub_i++) {
					sub_ring[sub_i] = ring[first + sub_i];
				}
				
				testRadialSymmetry(graph, root_node, sub_ring, sub_total, axis, limit, group);
			
				MEM_freeN(sub_ring);
			}
			else if (sub_total == total) {
				testRadialSymmetry(graph, root_node, ring, total, axis, limit, group);
			}
			
			first = i;
		}
	}


	MEM_freeN(ring);
}

static void flagAxialSymmetry(BNode *root_node, BNode *end_node, BArc *arc, int group)
{
	float vec[3];
	
	arc->symmetry_group = group;
	
	sub_v3_v3v3(vec, end_node->p, root_node->p);
	
	if (dot_v3v3(vec, root_node->symmetry_axis) < 0) {
		arc->symmetry_flag |= SYM_SIDE_NEGATIVE;
	}
	else {
		arc->symmetry_flag |= SYM_SIDE_POSITIVE;
	}
}

static void testAxialSymmetry(BGraph *graph, BNode *root_node, BNode *node1, BNode *node2, BArc *arc1, BArc *arc2, float axis[3], float limit, int group)
{
	const float limit_sq = limit * limit;
	float nor[3], vec[3], p[3];

	sub_v3_v3v3(p, node1->p, root_node->p);
	cross_v3_v3v3(nor, p, axis);

	sub_v3_v3v3(p, root_node->p, node2->p);
	cross_v3_v3v3(vec, p, axis);
	add_v3_v3(vec, nor);
	
	cross_v3_v3v3(nor, vec, axis);
	
	if (fabsf(nor[0]) > fabsf(nor[1]) && fabsf(nor[0]) > fabsf(nor[2]) && nor[0] < 0) {
		negate_v3(nor);
	}
	else if (fabsf(nor[1]) > fabsf(nor[0]) && fabsf(nor[1]) > fabsf(nor[2]) && nor[1] < 0) {
		negate_v3(nor);
	}
	else if (fabsf(nor[2]) > fabsf(nor[1]) && fabsf(nor[2]) > fabsf(nor[0]) && nor[2] < 0) {
		negate_v3(nor);
	}
	
	/* mirror node2 along axis */
	copy_v3_v3(p, node2->p);
	BLI_mirrorAlongAxis(p, root_node->p, nor);
	
	/* check if it's within limit before continuing */
	if (len_squared_v3v3(node1->p, p) <= limit_sq) {
		/* mark node as symmetric physically */
		copy_v3_v3(root_node->symmetry_axis, nor);
		root_node->symmetry_flag |= SYM_PHYSICAL;
		root_node->symmetry_flag |= SYM_AXIAL;

		/* flag side on arcs */
		flagAxialSymmetry(root_node, node1, arc1, group);
		flagAxialSymmetry(root_node, node2, arc2, group);
		
		if (graph->axial_symmetry) {
			graph->axial_symmetry(root_node, node1, node2, arc1, arc2);
		}
	}
	else {
		/* NOT SYMMETRIC */
	}
}

static void handleAxialSymmetry(BGraph *graph, BNode *root_node, int depth, float axis[3], float limit)
{
	BArc *arc1 = NULL, *arc2 = NULL;
	BNode *node1 = NULL, *node2 = NULL;
	int i;
	
	/* mark topological symmetry */
	root_node->symmetry_flag |= SYM_TOPOLOGICAL;

	for (i = 0; i < root_node->degree; i++) {
		BArc *connectedArc = root_node->arcs[i];
		
		/* depth is store as a negative in flag. symmetry level is positive */
		if (connectedArc->symmetry_level == -depth) {
			if (arc1 == NULL) {
				arc1 = connectedArc;
				node1 = BLI_otherNode(arc1, root_node);
			}
			else {
				arc2 = connectedArc;
				node2 = BLI_otherNode(arc2, root_node);
				break; /* Can stop now, the two arcs have been found */
			}
		}
	}
	
	/* shouldn't happen, but just to be sure */
	if (node1 == NULL || node2 == NULL) {
		return;
	}
	
	testAxialSymmetry(graph, root_node, node1, node2, arc1, arc2, axis, limit, 1);
}

static void markdownSecondarySymmetry(BGraph *graph, BNode *node, int depth, int level, float limit)
{
	float axis[3] = {0, 0, 0};
	int count = 0;
	int i;
	
	/* count the number of branches in this symmetry group
	 * and determinate the axis of symmetry
	 */
	for (i = 0; i < node->degree; i++) {
		BArc *connectedArc = node->arcs[i];
		
		/* depth is store as a negative in flag. symmetry level is positive */
		if (connectedArc->symmetry_level == -depth) {
			count++;
		}
		/* If arc is on the axis */
		else if (connectedArc->symmetry_level == level) {
			add_v3_v3(axis, connectedArc->head->p);
			sub_v3_v3v3(axis, axis, connectedArc->tail->p);
		}
	}

	normalize_v3(axis);

	/* Split between axial and radial symmetry */
	if (count == 2) {
		handleAxialSymmetry(graph, node, depth, axis, limit);
	}
	else {
		handleRadialSymmetry(graph, node, depth, axis, limit);
	}
		
	/* markdown secondary symetries */
	for (i = 0; i < node->degree; i++) {
		BArc *connectedArc = node->arcs[i];
		
		if (connectedArc->symmetry_level == -depth) {
			/* markdown symmetry for branches corresponding to the depth */
			markdownSymmetryArc(graph, connectedArc, node, level + 1, limit);
		}
	}
}

static void markdownSymmetryArc(BGraph *graph, BArc *arc, BNode *node, int level, float limit)
{
	int i;

	/* if arc is null, we start straight from a node */
	if (arc) {
		arc->symmetry_level = level;
		
		node = BLI_otherNode(arc, node);
	}
	
	for (i = 0; i < node->degree; i++) {
		BArc *connectedArc = node->arcs[i];
		
		if (connectedArc != arc) {
			BNode *connectedNode = BLI_otherNode(connectedArc, node);
			
			/* symmetry level is positive value, negative values is subtree depth */
			connectedArc->symmetry_level = -BLI_subtreeShape(graph, connectedNode, connectedArc, 0);
		}
	}

	arc = NULL;

	for (i = 0; i < node->degree; i++) {
		int issymmetryAxis = 0;
		BArc *connectedArc = node->arcs[i];
		
		/* only arcs not already marked as symetric */
		if (connectedArc->symmetry_level < 0) {
			int j;
			
			/* true by default */
			issymmetryAxis = 1;
			
			for (j = 0; j < node->degree; j++) {
				BArc *otherArc = node->arcs[j];
				
				/* different arc, same depth */
				if (otherArc != connectedArc && otherArc->symmetry_level == connectedArc->symmetry_level) {
					/* not on the symmetry axis */
					issymmetryAxis = 0;
					break;
				}
			}
		}
		
		/* arc could be on the symmetry axis */
		if (issymmetryAxis == 1) {
			/* no arc as been marked previously, keep this one */
			if (arc == NULL) {
				arc = connectedArc;
			}
			else if (connectedArc->symmetry_level < arc->symmetry_level) {
				/* go with more complex subtree as symmetry arc */
				arc = connectedArc;
			}
		}
	}
	
	/* go down the arc continuing the symmetry axis */
	if (arc) {
		markdownSymmetryArc(graph, arc, node, level, limit);
	}

	
	/* secondary symmetry */
	for (i = 0; i < node->degree; i++) {
		BArc *connectedArc = node->arcs[i];
		
		/* only arcs not already marked as symetric and is not the next arc on the symmetry axis */
		if (connectedArc->symmetry_level < 0) {
			/* subtree depth is store as a negative value in the symmetry */
			markdownSecondarySymmetry(graph, node, -connectedArc->symmetry_level, level, limit);
		}
	}
}

void BLI_markdownSymmetry(BGraph *graph, BNode *root_node, float limit)
{
	BNode *node;
	BArc *arc;
	
	if (root_node == NULL) {
		return;
	}
	
	if (BLI_isGraphCyclic(graph)) {
		return;
	}
	
	/* mark down all arcs as non-symetric */
	BLI_flagArcs(graph, 0);
	
	/* mark down all nodes as not on the symmetry axis */
	BLI_flagNodes(graph, 0);

	node = root_node;
	
	/* sanity check REMOVE ME */
	if (node->degree > 0) {
		arc = node->arcs[0];
		
		if (node->degree == 1) {
			markdownSymmetryArc(graph, arc, node, 1, limit);
		}
		else {
			markdownSymmetryArc(graph, NULL, node, 1, limit);
		}
		


		/* mark down non-symetric arcs */
		for (arc = graph->arcs.first; arc; arc = arc->next) {
			if (arc->symmetry_level < 0) {
				arc->symmetry_level = 0;
			}
			else {
				/* mark down nodes with the lowest level symmetry axis */
				if (arc->head->symmetry_level == 0 || arc->head->symmetry_level > arc->symmetry_level) {
					arc->head->symmetry_level = arc->symmetry_level;
				}
				if (arc->tail->symmetry_level == 0 || arc->tail->symmetry_level > arc->symmetry_level) {
					arc->tail->symmetry_level = arc->symmetry_level;
				}
			}
		}
	}
}

void *IT_head(void *arg)
{
	BArcIterator *iter = (BArcIterator *)arg;
	return iter->head(iter);
}

void *IT_tail(void *arg)
{
	BArcIterator *iter = (BArcIterator *)arg;
	return iter->tail(iter); 
}

void *IT_peek(void *arg, int n)
{
	BArcIterator *iter = (BArcIterator *)arg;
	
	if (iter->index + n < 0) {
		return iter->head(iter);
	}
	else if (iter->index + n >= iter->length) {
		return iter->tail(iter);
	}
	else {
		return iter->peek(iter, n);
	}
}

void *IT_next(void *arg)
{
	BArcIterator *iter = (BArcIterator *)arg;
	return iter->next(iter);
}

void *IT_nextN(void *arg, int n)
{
	BArcIterator *iter = (BArcIterator *)arg;
	return iter->nextN(iter, n);
}

void *IT_previous(void *arg)
{
	BArcIterator *iter = (BArcIterator *)arg;
	return iter->previous(iter);
}

int   IT_stopped(void *arg)
{
	BArcIterator *iter = (BArcIterator *)arg;
	return iter->stopped(iter);
}
