/**
 * $Id$
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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef DEPSGRAPH_API
#define DEPSGRAPH_API

/*
#define DEPS_DEBUG
*/

struct Scene;
struct DagNodeQueue;
struct DagForest;
struct DagNode;

typedef enum { 
	DAG_RL_SCENE				= 1,
	DAG_RL_DATA					= 2,
	DAG_RL_PARENT				= 4,
	DAG_RL_TRACK				= 8,
	DAG_RL_PATH					= 16,
	DAG_RL_CONSTRAINT			= 32,
	DAG_RL_HOOK					= 64,
	DAG_RL_DATA_CONSTRAINT		= 128,
	DAG_NO_RELATION				= 256
} dag_rel_type;


typedef enum {
	DAG_RL_SCENE_MASK			= 1,
	DAG_RL_DATA_MASK			= 2,
	DAG_RL_PARENT_MASK			= 4,
	DAG_RL_TRACK_MASK			= 8,
	DAG_RL_PATH_MASK			= 16,
	DAG_RL_CONSTRAINT_MASK		= 32,
	DAG_RL_HOOK_MASK			= 64,
	DAG_RL_DATA_CONSTRAINT_MASK	= 128,
	DAG_RL_ALL_BUT_DATA_MASK	= 253,
	DAG_RL_ALL_MASK				= 255
} dag_rel_type_mask;

typedef void (*graph_action_func)(void * ob, void **data);

// queues are returned by all BFS & DFS queries
// opaque type
void	*pop_ob_queue(struct DagNodeQueue *queue);
int		queue_count(struct DagNodeQueue *queue);
void	queue_delete(struct DagNodeQueue *queue);

// queries
struct DagForest	*build_dag(struct Scene *sce, short mask);
void				free_forest(struct DagForest *Dag);

// note :
// the meanings of the 2 returning values is a bit different :
// BFS return 1 for cross-edges and back-edges. the latter are considered harmfull, not the former
// DFS return 1 only for back-edges
int pre_and_post_BFS(struct DagForest *dag, short mask, graph_action_func pre_func, graph_action_func post_func, void **data);
int pre_and_post_DFS(struct DagForest *dag, short mask, graph_action_func pre_func, graph_action_func post_func, void **data);

int pre_and_post_source_BFS(struct DagForest *dag, short mask, struct DagNode *source, graph_action_func pre_func, graph_action_func post_func, void **data);
int pre_and_post_source_DFS(struct DagForest *dag, short mask, struct DagNode *source, graph_action_func pre_func, graph_action_func post_func, void **data);

struct DagNodeQueue *get_obparents(struct DagForest	*dag, void *ob); 
struct DagNodeQueue *get_first_ancestors(struct DagForest	*dag, void *ob); 
struct DagNodeQueue *get_all_childs(struct DagForest	*dag, void *ob); //
dag_rel_type		are_obs_related(struct DagForest	*dag, void *ob1, void *ob2);
int					is_acyclic(struct DagForest	*dag); //
//int					get_cycles(struct DagForest	*dag, struct DagNodeQueue **queues, int *count); //
void				topo_sort_baselist(struct Scene *sce);

void	boundbox_deps(void);
void	draw_all_deps(void);


#endif
