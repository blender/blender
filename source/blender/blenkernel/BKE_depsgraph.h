/**
 * $Id$
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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef DEPSGRAPH_API
#define DEPSGRAPH_API

/*
#define DEPS_DEBUG
*/

struct ID;
struct Main;
struct Scene;
struct DagNodeQueue;
struct DagForest;
struct DagNode;
struct GHash;

/* **** DAG relation types *** */

	/* scene link to object */
#define DAG_RL_SCENE		1
	/* object link to data */
#define DAG_RL_DATA			2

	/* object changes object (parent, track, constraints) */
#define DAG_RL_OB_OB		4
	/* object changes obdata (hooks, constraints) */
#define DAG_RL_OB_DATA		8
	/* data changes object (vertex parent) */
#define DAG_RL_DATA_OB		16
	/* data changes data (deformers) */
#define DAG_RL_DATA_DATA	32

#define DAG_NO_RELATION		64
#define DAG_RL_ALL			63
#define DAG_RL_ALL_BUT_DATA 61


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
short		are_obs_related(struct DagForest	*dag, void *ob1, void *ob2);
int					is_acyclic(struct DagForest	*dag); //
//int					get_cycles(struct DagForest	*dag, struct DagNodeQueue **queues, int *count); //

void	boundbox_deps(void);
void	draw_all_deps(void);

/* ********** API *************** */
/* Note that the DAG never executes changes in Objects, only sets flags in Objects */

		/* (re)-create dependency graph for scene */
void	DAG_scene_sort(struct Scene *sce);

		/* flag all objects that need recalc because they're animated */
void	DAG_scene_update_flags(struct Scene *sce, unsigned int lay);
		/* flushes all recalc flags in objects down the dependency tree */
void	DAG_scene_flush_update(struct Scene *sce, unsigned int lay, int time);
		/* tag objects for update on file load */
void	DAG_on_load_update(void);

		/* flag all IDs that need recalc because they're animated, influencing
		   this ID only. only for objects currently */
void	DAG_id_update_flags(struct ID *id);
		/* flushes all recalc flags for this object down the dependency tree,
		   but note the DAG only supports objects and object data currently */
void	DAG_id_flush_update(struct ID *id, short flag);
		/* when setting manual RECALC flags, call this afterwards */
void	DAG_ids_flush_update(int time);

		/* (re)-create dependency graph for armature pose */
void	DAG_pose_sort(struct Object *ob);

		/* callback for editors module to do updates */
void	DAG_editors_update_cb(void (*func)(struct Main *bmain, struct ID *id));
		
#endif
