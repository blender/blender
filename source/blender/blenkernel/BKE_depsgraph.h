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

/* Dependency Graph
 *
 * The dependency graph tracks relations between datablocks, and is used to
 * determine which datablocks need to be update based on dependencies and
 * visibility.
 *
 * It does not itself execute changes in objects, but rather sorts the objects
 * in the appropriate order and sets flags indicating they should be updated.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct ID;
struct Main;
struct Object;
struct Scene;
struct ListBase;

/* Dependency graph evaluation context
 *
 * This structure stores all the local dependency graph data,
 * which is needed for it's evaluation,
 */
typedef struct EvaluationContext {
	bool for_render;  /* Set to true if evaluation shall be performed for render purposes,
	                     keep at false if update shall happen for the viewport. */
} EvaluationContext;

/* DagNode->eval_flags */
enum {
	/* Regardless to curve->path animation flag path is to be evaluated anyway,
	 * to meet dependencies with such a things as curve modifier and other guys
	 * who're using curve deform, where_on_path and so.
	 */
	DAG_EVAL_NEED_CURVE_PATH = 1,
};

/* Global initialization/deinitialization */
void DAG_init(void);
void DAG_exit(void);

/* Build and Update
 *
 * DAG_scene_relations_update will rebuild the dependency graph for a given
 * scene if needed, and sort objects in the scene.
 *
 * DAG_relations_tag_update will clear all dependency graphs and mark them to
 * be rebuilt later. The graph is not rebuilt immediately to avoid slowdowns
 * when this function is call multiple times from different operators.
 *
 * DAG_scene_relations_rebuild forces an immediaterebuild of the dependency
 * graph, this is only needed in rare cases
 */

void DAG_scene_relations_update(struct Main *bmain, struct Scene *sce);
void DAG_relations_tag_update(struct Main *bmain);
void DAG_scene_relations_rebuild(struct Main *bmain, struct Scene *scene);
void DAG_scene_free(struct Scene *sce);

/* Update Tagging
 *
 * DAG_scene_update_flags will mark all objects that depend on time (animation,
 * physics, ..) to be recalculated, used when changing the current frame.
 * 
 * DAG_on_visible_update will mark all objects that are visible for the first
 * time to be updated, for example on file load or changing layer visibility.
 *
 * DAG_id_tag_update will mark a given datablock to be updated. The flag indicates
 * a specific subset to be update (only object transform and data for now).
 *
 * DAG_id_type_tag marks a particular datablock type as having changing. This does
 * not cause any updates but is used by external render engines to detect if for
 * example a datablock was removed. */

void DAG_scene_update_flags(struct Main *bmain, struct Scene *sce, unsigned int lay, const bool do_time, const bool do_invisible_flush);
void DAG_on_visible_update(struct Main *bmain, const bool do_time);

void DAG_id_tag_update(struct ID *id, short flag);
void DAG_id_tag_update_ex(struct Main *bmain, struct ID *id, short flag);
void DAG_id_type_tag(struct Main *bmain, short idtype);
int  DAG_id_type_tagged(struct Main *bmain, short idtype);

/* Flushing Tags
 *
 * DAG_scene_flush_update flushes object recalculation flags immediately to other
 * dependencies. Do not use outside of depsgraph.c, this will be removed.
 *
 * DAG_ids_flush_tagged will flush datablock update flags flags to dependencies,
 * use this right before updating to mark all the needed datablocks for update.
 *
 * DAG_ids_check_recalc and DAG_ids_clear_recalc are used for external render
 * engines to detect changes. */

void DAG_scene_flush_update(struct Main *bmain, struct Scene *sce, unsigned int lay, const short do_time);
void DAG_ids_flush_tagged(struct Main *bmain);
void DAG_ids_check_recalc(struct Main *bmain, struct Scene *scene, bool time);
void DAG_ids_clear_recalc(struct Main *bmain);

/* Armature: sorts the bones according to dependencies between them */

void DAG_pose_sort(struct Object *ob);

/* Editors: callbacks to notify editors of datablock changes */

void DAG_editors_update_cb(void (*id_func)(struct Main *bmain, struct ID *id),
                           void (*scene_func)(struct Main *bmain, struct Scene *scene, int updated));

/* ** Threaded update ** */

/* Initialize the DAG for threaded update. */
void DAG_threaded_update_begin(struct Scene *scene,
                               void (*func)(void *node, void *user_data),
                               void *user_data);

void DAG_threaded_update_handle_node_updated(void *node_v,
                                             void (*func)(void *node, void *user_data),
                                             void *user_data);

/* Debugging: print dependency graph for scene or armature object to console */

void DAG_print_dependencies(struct Main *bmain, struct Scene *scene, struct Object *ob);

/* ************************ DAG querying ********************* */

struct Object *DAG_get_node_object(void *node_v);
const char *DAG_get_node_name(struct Scene *scene, void *node_v);
short DAG_get_eval_flags_for_object(struct Scene *scene, void *object);
bool DAG_is_acyclic(struct Scene *scene);

#ifdef __cplusplus
}
#endif
		
#endif
