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
struct RenderEngineType;
struct Scene;
struct ViewLayer;

typedef enum eEvaluationMode {
	DAG_EVAL_VIEWPORT       = 0,    /* evaluate for OpenGL viewport */
	DAG_EVAL_PREVIEW        = 1,    /* evaluate for render with preview settings */
	DAG_EVAL_RENDER         = 2,    /* evaluate for render purposes */
} eEvaluationMode;

/* Dependency graph evaluation context
 *
 * This structure stores all the local dependency graph data,
 * which is needed for it's evaluation,
 */
typedef struct EvaluationContext {
	eEvaluationMode mode;
	float ctime;

	struct Depsgraph *depsgraph;
	struct ViewLayer *view_layer;
	struct RenderEngineType *engine_type;
} EvaluationContext;

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

bool DEG_depsgraph_use_copy_on_write(void);
void DEG_depsgraph_enable_copy_on_write(void);

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

/* Update dependency graph when visible scenes/layers changes. */
void DEG_graph_on_visible_update(struct Main *bmain, Depsgraph *depsgraph);

/* Update all dependency graphs when visible scenes/layers changes. */
void DEG_on_visible_update(struct Main *bmain, const bool do_time);

/* Tag given ID for an update in all the dependency graphs. */
enum {
	/* Object transformation changed, corresponds to OB_RECALC_OB. */
	DEG_TAG_TRANSFORM   = (1 << 0),
	/* Object geoemtry changed, corresponds to OB_RECALC_DATA. */
	DEG_TAG_GEOMETRY    = (1 << 1),
	/* Time changed and animation is to be re-evaluated, OB_RECALC_TIME. */
	DEG_TAG_TIME        = (1 << 2),
	/* Particle system changed. */
	DEG_TAG_PSYS_REDO   = (1 << 3),
	DEG_TAG_PSYS_RESET  = (1 << 4),
	DEG_TAG_PSYS_TYPE   = (1 << 5),
	DEG_TAG_PSYS_CHILD  = (1 << 6),
	DEG_TAG_PSYS_PHYS   = (1 << 7),
	DEG_TAG_PSYS_ALL    = (DEG_TAG_PSYS_REDO |
	                       DEG_TAG_PSYS_RESET |
	                       DEG_TAG_PSYS_TYPE |
	                       DEG_TAG_PSYS_CHILD |
	                       DEG_TAG_PSYS_PHYS),
	/* Update copy on write component without flushing down the road. */
	DEG_TAG_COPY_ON_WRITE = (1 << 8),
	/* Tag shading components for update.
	 * Only parameters of material changed).
	 */
	DEG_TAG_SHADING_UPDATE  = (1 << 9),
	DEG_TAG_SELECT_UPDATE   = (1 << 10),
	DEG_TAG_BASE_FLAGS_UPDATE = (1 << 11),
	/* Only inform editors about the change. Don't modify datablock itself. */
	DEG_TAG_EDITORS_UPDATE = (1 << 12),
};
void DEG_id_tag_update(struct ID *id, int flag);
void DEG_id_tag_update_ex(struct Main *bmain, struct ID *id, int flag);

void DEG_graph_id_tag_update(struct Main *bmain,
                             struct Depsgraph *depsgraph,
                             struct ID *id,
                             int flag);

/* Mark a particular datablock type as having changing. This does
 * not cause any updates but is used by external render engines to detect if for
 * example a datablock was removed.
 */
void DEG_id_type_tag(struct Main *bmain, short id_type);

void DEG_ids_clear_recalc(struct Main *bmain);

/* Update Flushing ------------------------------- */

/* Flush updates for IDs in a single scene. */
void DEG_graph_flush_update(struct Main *bmain, Depsgraph *depsgraph);

/* Check if something was changed in the database and inform
 * editors about this.
 */
void DEG_ids_check_recalc(struct Main *bmain,
                          struct Scene *scene,
                          struct ViewLayer *view_layer,
                          bool time);

/* ************************************************ */
/* Evaluation Engine API */

/* Evaluation Context ---------------------------- */

/* Create new evaluation context. */
struct EvaluationContext *DEG_evaluation_context_new(eEvaluationMode mode);

/* Initialize evaluation context.
 * Used by the areas which currently overrides the context or doesn't have
 * access to a proper one.
 */
void DEG_evaluation_context_init(struct EvaluationContext *eval_ctx,
                                 eEvaluationMode mode);
void DEG_evaluation_context_init_from_scene(struct EvaluationContext *eval_ctx,
                                            struct Scene *scene,
                                            struct ViewLayer *view_layer,
                                            struct RenderEngineType *engine_type,
                                            eEvaluationMode mode);

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
void DEG_evaluate_on_refresh(struct EvaluationContext *eval_ctx,
                             Depsgraph *graph);

bool DEG_needs_eval(Depsgraph *graph);

/* Editors Integration  -------------------------- */

/* Mechanism to allow editors to be informed of depsgraph updates,
 * to do their own updates based on changes.
 */

typedef struct DEGEditorUpdateContext {
	struct Main *bmain;
	struct Scene *scene;
	struct ViewLayer *view_layer;
} DEGEditorUpdateContext;

typedef void (*DEG_EditorUpdateIDCb)(
        const DEGEditorUpdateContext *update_ctx,
        struct ID *id);
typedef void (*DEG_EditorUpdateSceneCb)(
        const DEGEditorUpdateContext *update_ctx, int updated);

/* Set callbacks which are being called when depsgraph changes. */
void DEG_editors_set_update_cb(DEG_EditorUpdateIDCb id_func,
                               DEG_EditorUpdateSceneCb scene_func);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  /* __DEG_DEPSGRAPH_H__ */
