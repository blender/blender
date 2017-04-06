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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Joshua Leung
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/DEG_depsgraph.h
 *  \ingroup depsgraph
 *
 * Public API for Depsgraph
 *
 * Dependency Graph
 * ================
 *
 * The dependency graph tracks relations between various pieces of data in
 * a Blender file, but mainly just those which make up scene data. It is used
 * to determine the set of operations need to ensure that all data has been
 * correctly evaluated in response to changes, based on dependencies and visibility
 * of affected data.
 *
 *
 * Evaluation Engine
 * =================
 *
 * The evaluation takes the operation-nodes the Depsgraph has tagged for updating, 
 * and schedules them up for being evaluated/executed such that the all dependency
 * relationship constraints are satisfied. 
 */

/* ************************************************* */
/* Forward-defined typedefs for core types
 * - These are used in all depsgraph code and by all callers of Depsgraph API...
 */

#ifndef __DEG_DEPSGRAPH_H__
#define __DEG_DEPSGRAPH_H__

/* Dependency Graph */
typedef struct Depsgraph Depsgraph;

/* ------------------------------------------------ */

struct EvaluationContext;
struct Main;

struct PointerRNA;
struct PropertyRNA;

/* Dependency graph evaluation context
 *
 * This structure stores all the local dependency graph data,
 * which is needed for it's evaluation,
 */
typedef struct EvaluationContext {
	int mode;               /* evaluation mode */
	float ctime;            /* evaluation time */
} EvaluationContext;

typedef enum eEvaluationMode {
	DAG_EVAL_VIEWPORT       = 0,    /* evaluate for OpenGL viewport */
	DAG_EVAL_PREVIEW        = 1,    /* evaluate for render with preview settings */
	DAG_EVAL_RENDER         = 2,    /* evaluate for render purposes */
} eEvaluationMode;

/* DagNode->eval_flags */
enum {
	/* Regardless to curve->path animation flag path is to be evaluated anyway,
	 * to meet dependencies with such a things as curve modifier and other guys
	 * who're using curve deform, where_on_path and so.
	 */
	DAG_EVAL_NEED_CURVE_PATH = 1,
	/* Scene evaluation would need to have object's data on CPU,
	 * meaning no GPU shortcuts is allowed.
	 */
	DAG_EVAL_NEED_CPU        = 2,
};

#ifdef __cplusplus
extern "C" {
#endif

bool DEG_depsgraph_use_legacy(void);
void DEG_depsgraph_switch_to_legacy(void);
void DEG_depsgraph_switch_to_new(void);

/* ************************************************ */
/* Depsgraph API */

/* CRUD ------------------------------------------- */

// Get main depsgraph instance from context!

/* Create new Depsgraph instance */
// TODO: what args are needed here? What's the building-graph entry point?
Depsgraph *DEG_graph_new(void);

/* Free Depsgraph itself and all its data */
void DEG_graph_free(Depsgraph *graph);

/* Node Types Registry ---------------------------- */

/* Register all node types */
void DEG_register_node_types(void);

/* Free node type registry on exit */
void DEG_free_node_types(void);

/* Update Tagging -------------------------------- */

/* Tag node(s) associated with states such as time and visibility */
void DEG_scene_update_flags(Depsgraph *graph, const bool do_time);

/* Update dependency graph when visible scenes/layers changes. */
void DEG_graph_on_visible_update(struct Main *bmain, struct Scene *scene);

/* Update all dependency graphs when visible scenes/layers changes. */
void DEG_on_visible_update(struct Main *bmain, const bool do_time);

/* Tag node(s) associated with changed data for later updates */
void DEG_graph_id_tag_update(struct Main *bmain,
                             Depsgraph *graph,
                             struct ID *id);
void DEG_graph_data_tag_update(Depsgraph *graph, const struct PointerRNA *ptr);
void DEG_graph_property_tag_update(Depsgraph *graph, const struct PointerRNA *ptr, const struct PropertyRNA *prop);

/* Tag given ID for an update in all the dependency graphs. */
void DEG_id_tag_update(struct ID *id, short flag);
void DEG_id_tag_update_ex(struct Main *bmain,
                          struct ID *id,
                          short flag);

/* Tag given ID type for update.
 *
 * Used by all sort of render engines to quickly check if
 * IDs of a given type need to be checked for update.
 */
void DEG_id_type_tag(struct Main *bmain, short idtype);

void DEG_ids_clear_recalc(struct Main *bmain);

/* Update Flushing ------------------------------- */

/* Flush updates for all IDs */
void DEG_ids_flush_tagged(struct Main *bmain);

/* Check if something was changed in the database and inform
 * editors about this.
 */
void DEG_ids_check_recalc(struct Main *bmain,
                          struct Scene *scene,
                          bool time);

/* ************************************************ */
/* Evaluation Engine API */

/* Evaluation Context ---------------------------- */

/* Create new evaluation context. */
struct EvaluationContext *DEG_evaluation_context_new(int mode);

/* Initialize evaluation context.
 * Used by the areas which currently overrides the context or doesn't have
 * access to a proper one.
 */
void DEG_evaluation_context_init(struct EvaluationContext *eval_ctx, int mode);

/* Free evaluation context. */
void DEG_evaluation_context_free(struct EvaluationContext *eval_ctx);

/* Graph Evaluation  ----------------------------- */

/* Frame changed recalculation entry point
 * < context_type: context to perform evaluation for
 * < ctime: (frame) new frame to evaluate values on
 */
void DEG_evaluate_on_framechange(struct EvaluationContext *eval_ctx,
                                 struct Main *bmain,
                                 Depsgraph *graph,
                                 float ctime);

/* Data changed recalculation entry point.
 * < context_type: context to perform evaluation for
 */
void DEG_evaluate_on_refresh_ex(struct EvaluationContext *eval_ctx,
                                Depsgraph *graph);

/* Data changed recalculation entry point.
 * < context_type: context to perform evaluation for
 */
void DEG_evaluate_on_refresh(struct EvaluationContext *eval_ctx,
                             Depsgraph *graph,
                             struct Scene *scene);

bool DEG_needs_eval(Depsgraph *graph);

/* Editors Integration  -------------------------- */

/* Mechanism to allow editors to be informed of depsgraph updates,
 * to do their own updates based on changes.
 */

typedef void (*DEG_EditorUpdateIDCb)(struct Main *bmain, struct ID *id);
typedef void (*DEG_EditorUpdateSceneCb)(struct Main *bmain,
                                        struct Scene *scene,
                                        int updated);
typedef void (*DEG_EditorUpdateScenePreCb)(struct Main *bmain,
                                           struct Scene *scene,
                                           bool time);

/* Set callbacks which are being called when depsgraph changes. */
void DEG_editors_set_update_cb(DEG_EditorUpdateIDCb id_func,
                               DEG_EditorUpdateSceneCb scene_func,
                               DEG_EditorUpdateScenePreCb scene_pre_func);

void DEG_editors_update_pre(struct Main *bmain, struct Scene *scene, bool time);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  /* __DEG_DEPSGRAPH_H__ */
