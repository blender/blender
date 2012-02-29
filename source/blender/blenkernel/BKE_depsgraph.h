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

#ifndef __BKE_DEPSGRAPH_H__
#define __BKE_DEPSGRAPH_H__

/** \file BKE_depsgraph.h
 *  \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

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
#define DAG_RL_SCENE		(1<<0)
	/* object link to data */
#define DAG_RL_DATA			(1<<1)

	/* object changes object (parent, track, constraints) */
#define DAG_RL_OB_OB		(1<<2)
	/* object changes obdata (hooks, constraints) */
#define DAG_RL_OB_DATA		(1<<3)
	/* data changes object (vertex parent) */
#define DAG_RL_DATA_OB		(1<<4)
	/* data changes data (deformers) */
#define DAG_RL_DATA_DATA	(1<<5)

#define DAG_NO_RELATION		(1<<6)

#define DAG_RL_ALL_BUT_DATA (DAG_RL_SCENE|DAG_RL_OB_OB|DAG_RL_OB_DATA|DAG_RL_DATA_OB|DAG_RL_DATA_DATA)
#define DAG_RL_ALL			(DAG_RL_ALL_BUT_DATA|DAG_RL_DATA)


typedef void (*graph_action_func)(void * ob, void **data);

// queues are returned by all BFS & DFS queries
// opaque type
void	*pop_ob_queue(struct DagNodeQueue *queue);
int		queue_count(struct DagNodeQueue *queue);
void	queue_delete(struct DagNodeQueue *queue);

// queries
struct DagForest	*build_dag(struct Main *bmain, struct Scene *sce, short mask);
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

/* ********** API *************** */
/* Note that the DAG never executes changes in Objects, only sets flags in Objects */

		/* (re)-create dependency graph for scene */
void	DAG_scene_sort(struct Main *bmain, struct Scene *sce);

		/* flag all objects that need recalc because they're animated */
void	DAG_scene_update_flags(struct Main *bmain, struct Scene *sce, unsigned int lay, const short do_time);
		/* flushes all recalc flags in objects down the dependency tree */
void	DAG_scene_flush_update(struct Main *bmain, struct Scene *sce, unsigned int lay, const short do_time);
		/* tag objects for update on file load */
void	DAG_on_visible_update(struct Main *bmain, const short do_time);

		/* when setting manual RECALC flags, call this afterwards */
void	DAG_ids_flush_update(struct Main *bmain, int time);

		/* tag datablock to get updated for the next redraw */
void	DAG_id_tag_update(struct ID *id, short flag);
		/* flush all tagged updates */
void	DAG_ids_flush_tagged(struct Main *bmain);
		/* check and clear ID recalc flags */
void	DAG_ids_check_recalc(struct Main *bmain, struct Scene *scene, int time);
void	DAG_ids_clear_recalc(struct Main *bmain);
		/* test if any of this id type is tagged for update */
void	DAG_id_type_tag(struct Main *bmain, short idtype);
int		DAG_id_type_tagged(struct Main *bmain, short idtype);

		/* (re)-create dependency graph for armature pose */
void	DAG_pose_sort(struct Object *ob);

		/* callback for editors module to do updates */
void	DAG_editors_update_cb(void (*id_func)(struct Main *bmain, struct ID *id),
                              void (*scene_func)(struct Main *bmain, struct Scene *scene, int updated));

		/* debugging */
void	DAG_print_dependencies(struct Main *bmain, struct Scene *scene, struct Object *ob);

#ifdef __cplusplus
}
#endif
		
#endif
