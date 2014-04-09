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

/* **** DAG relation types *** */

/* scene link to object */
#define DAG_RL_SCENE        (1 << 0)
/* object link to data */
#define DAG_RL_DATA         (1 << 1)

/* object changes object (parent, track, constraints) */
#define DAG_RL_OB_OB        (1 << 2)
/* object changes obdata (hooks, constraints) */
#define DAG_RL_OB_DATA      (1 << 3)
/* data changes object (vertex parent) */
#define DAG_RL_DATA_OB      (1 << 4)
/* data changes data (deformers) */
#define DAG_RL_DATA_DATA    (1 << 5)

#define DAG_NO_RELATION     (1 << 6)

#define DAG_RL_ALL_BUT_DATA (DAG_RL_SCENE | DAG_RL_OB_OB | DAG_RL_OB_DATA | DAG_RL_DATA_OB | DAG_RL_DATA_DATA)
#define DAG_RL_ALL          (DAG_RL_ALL_BUT_DATA | DAG_RL_DATA)


#define DAGQUEUEALLOC 50

enum {
	DAG_WHITE = 0,
	DAG_GRAY = 1,
	DAG_BLACK = 2
};

typedef struct DagAdjList {
	struct DagNode *node;
	short type;
	int count;  /* number of identical arcs */
	unsigned int lay;   // for flushing redraw/rebuild events
	const char *name;
	struct DagAdjList *next;
} DagAdjList;


typedef struct DagNode {
	int color;
	short type;
	float x, y, k;
	void *ob;
	void *first_ancestor;
	int ancestor_count;
	unsigned int lay;               /* accumulated layers of its relations + itself */
	unsigned int scelay;            /* layers due to being in scene */
	uint64_t customdata_mask;       /* customdata mask */
	int lasttime;       /* if lasttime != DagForest->time, this node was not evaluated yet for flushing */
	int BFS_dist;       /* BFS distance */
	int DFS_dist;       /* DFS distance */
	int DFS_dvtm;       /* DFS discovery time */
	int DFS_fntm;       /* DFS Finishing time */
	struct DagAdjList *child;
	struct DagAdjList *parent;
	struct DagNode *next;

	/* Threaded evaluation routines */
	uint32_t num_pending_parents;  /* number of parents which are not updated yet
	                                * this node has got.
	                                * Used by threaded update for faster detect whether node could be
	                                * updated aready.
	                                */
	bool scheduled;

	/* Runtime flags mainly used to determine which extra data is to be evaluated
	 * during object_handle_update(). Such an extra data is what depends on the
	 * DAG topology, meaning this flags indicates the data evaluation of which
	 * depends on the node dependencies.
	 */
	short eval_flags;
} DagNode;

typedef struct DagNodeQueueElem {
	struct DagNode *node;
	struct DagNodeQueueElem *next;
} DagNodeQueueElem;

typedef struct DagNodeQueue {
	DagNodeQueueElem *first;
	DagNodeQueueElem *last;
	int count;
	int maxlevel;
	struct DagNodeQueue *freenodes;
} DagNodeQueue;

/* forest as we may have more than one DAG unconnected */
typedef struct DagForest {
	ListBase DagNode;
	struct GHash *nodeHash;
	int numNodes;
	bool is_acyclic;
	int time;  /* for flushing/tagging, compare with node->lasttime */
	bool ugly_hack_sorry;  /* prevent type check */
} DagForest;

// queue operations
DagNodeQueue *queue_create(int slots);
void queue_raz(DagNodeQueue *queue);
void push_queue(DagNodeQueue *queue, DagNode *node);
void push_stack(DagNodeQueue *queue, DagNode *node);
DagNode *pop_queue(DagNodeQueue *queue);
DagNode *get_top_node_queue(DagNodeQueue *queue);
void queue_delete(DagNodeQueue *queue);

// Dag management
DagForest *dag_init(void);
DagForest *build_dag(struct Main *bmain, struct Scene *sce, short mask);
void free_forest(struct DagForest *Dag);
DagNode *dag_find_node(DagForest *forest, void *fob);
DagNode *dag_add_node(DagForest *forest, void *fob);
DagNode *dag_get_node(DagForest *forest, void *fob);
DagNode *dag_get_sub_node(DagForest *forest, void *fob);
void dag_add_relation(DagForest *forest, DagNode *fob1, DagNode *fob2, short rel, const char *name);

void graph_print_queue(DagNodeQueue *nqueue);
void graph_print_queue_dist(DagNodeQueue *nqueue);
void graph_print_adj_list(DagForest *dag);

#endif /* __DEPSGRAPH_PRIVATE_H__ */
