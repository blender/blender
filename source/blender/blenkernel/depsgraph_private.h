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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/depsgraph_private.h
 *  \ingroup bke
 */

#ifndef __DEPSGRAPH_PRIVATE_H__
#define __DEPSGRAPH_PRIVATE_H__

#include "BKE_depsgraph.h"
#include "DNA_constraint_types.h"
#include "BKE_constraint.h"


#define DEPSX	5.0f
#define DEPSY	1.8f

#define DAGQUEUEALLOC 50

enum {
	DAG_WHITE = 0,
	DAG_GRAY = 1,
	DAG_BLACK = 2
};



typedef struct DagAdjList
{
	struct DagNode *node;
	short type;
	int count;			// number of identical arcs
	unsigned int lay;   // for flushing redraw/rebuild events
	const char *name;
	struct DagAdjList *next;
} DagAdjList;


typedef struct DagNode 
{
	int color;
	short type;
	float x, y, k;	
	void * ob;
	void * first_ancestor;
	int ancestor_count;
	unsigned int lay;				// accumulated layers of its relations + itself
	unsigned int scelay;			// layers due to being in scene
	uint64_t customdata_mask;	// customdata mask
	int lasttime;		// if lasttime != DagForest->time, this node was not evaluated yet for flushing
	int BFS_dist;		// BFS distance
	int DFS_dist;		// DFS distance
	int DFS_dvtm;		// DFS discovery time
	int DFS_fntm;		// DFS Finishing time
	struct DagAdjList *child;
	struct DagAdjList *parent;
	struct DagNode *next;
} DagNode;

typedef struct DagNodeQueueElem {
	struct DagNode *node;
	struct DagNodeQueueElem *next;
} DagNodeQueueElem;

typedef struct DagNodeQueue
{
	DagNodeQueueElem *first;
	DagNodeQueueElem *last;
	int count;
	int maxlevel;
	struct DagNodeQueue *freenodes;
} DagNodeQueue;

// forest as we may have more than one DAG unnconected
typedef struct DagForest 
{
	ListBase DagNode;
	struct GHash *nodeHash;
	int numNodes;
	int is_acyclic;
	int time;		// for flushing/tagging, compare with node->lasttime
} DagForest;


// queue operations
DagNodeQueue * queue_create (int slots);
void queue_raz(DagNodeQueue *queue);
void push_queue(DagNodeQueue *queue, DagNode *node);
void push_stack(DagNodeQueue *queue, DagNode *node);
DagNode * pop_queue(DagNodeQueue *queue);
DagNode * get_top_node_queue(DagNodeQueue *queue);

// Dag management
DagForest *getMainDag(void);
void setMainDag(DagForest *dag);
DagForest * dag_init(void);
DagNode * dag_find_node (DagForest *forest, void * fob);
DagNode * dag_add_node (DagForest *forest, void * fob);
DagNode * dag_get_node (DagForest *forest, void * fob);
DagNode * dag_get_sub_node (DagForest *forest, void * fob);
void dag_add_relation(DagForest *forest, DagNode *fob1, DagNode *fob2, short rel, const char *name);

void graph_bfs(void);

DagNodeQueue * graph_dfs(void);

void set_node_xy(DagNode *node, float x, float y);
void graph_print_queue(DagNodeQueue *nqueue);
void graph_print_queue_dist(DagNodeQueue *nqueue);
void graph_print_adj_list(void);

#endif /* __DEPSGRAPH_PRIVATE_H__ */
