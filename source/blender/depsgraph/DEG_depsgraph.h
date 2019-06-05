/*
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
 */

/** \file
 * \ingroup depsgraph
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

#include "DNA_ID.h"

/* Dependency Graph */
typedef struct Depsgraph Depsgraph;

/* ------------------------------------------------ */

struct Main;

struct PointerRNA;
struct PropertyRNA;
struct RenderEngineType;
struct Scene;
struct ViewLayer;

typedef enum eEvaluationMode {
  DAG_EVAL_VIEWPORT = 0, /* evaluate for OpenGL viewport */
  DAG_EVAL_RENDER = 1,   /* evaluate for render purposes */
} eEvaluationMode;

/* DagNode->eval_flags */
enum {
  /* Regardless to curve->path animation flag path is to be evaluated anyway,
   * to meet dependencies with such a things as curve modifier and other guys
   * who're using curve deform, where_on_path and so. */
  DAG_EVAL_NEED_CURVE_PATH = (1 << 0),
  /* A shrinkwrap modifier or constraint targeting this mesh needs information
   * about non-manifold boundary edges for the Target Normal Project mode. */
  DAG_EVAL_NEED_SHRINKWRAP_BOUNDARY = (1 << 1),
};

#ifdef __cplusplus
extern "C" {
#endif

/* ************************************************ */
/* Depsgraph API */

/* CRUD ------------------------------------------- */

// Get main depsgraph instance from context!

/* Create new Depsgraph instance */
// TODO: what args are needed here? What's the building-graph entry point?
Depsgraph *DEG_graph_new(struct Scene *scene, struct ViewLayer *view_layer, eEvaluationMode mode);

/* Free Depsgraph itself and all its data */
void DEG_graph_free(Depsgraph *graph);

/* Node Types Registry ---------------------------- */

/* Register all node types */
void DEG_register_node_types(void);

/* Free node type registry on exit */
void DEG_free_node_types(void);

/* Update Tagging -------------------------------- */

/* Update dependency graph when visible scenes/layers changes. */
void DEG_graph_on_visible_update(struct Main *bmain, Depsgraph *depsgraph, const bool do_time);

/* Update all dependency graphs when visible scenes/layers changes. */
void DEG_on_visible_update(struct Main *bmain, const bool do_time);

const char *DEG_update_tag_as_string(IDRecalcFlag flag);

void DEG_id_tag_update(struct ID *id, int flag);
void DEG_id_tag_update_ex(struct Main *bmain, struct ID *id, int flag);

void DEG_graph_id_tag_update(struct Main *bmain,
                             struct Depsgraph *depsgraph,
                             struct ID *id,
                             int flag);

/* Mark a particular datablock type as having changing. This does
 * not cause any updates but is used by external render engines to detect if for
 * example a datablock was removed. */
void DEG_graph_id_type_tag(struct Depsgraph *depsgraph, short id_type);
void DEG_id_type_tag(struct Main *bmain, short id_type);

void DEG_ids_clear_recalc(struct Main *bmain, Depsgraph *depsgraph);

/* Update Flushing ------------------------------- */

/* Flush updates for IDs in a single scene. */
void DEG_graph_flush_update(struct Main *bmain, Depsgraph *depsgraph);

/* Check if something was changed in the database and inform
 * editors about this.
 */
void DEG_ids_check_recalc(struct Main *bmain,
                          struct Depsgraph *depsgraph,
                          struct Scene *scene,
                          struct ViewLayer *view_layer,
                          bool time);

/* ************************************************ */
/* Evaluation Engine API */

/* Graph Evaluation  ----------------------------- */

/* Frame changed recalculation entry point
 * < context_type: context to perform evaluation for
 * < ctime: (frame) new frame to evaluate values on
 */
void DEG_evaluate_on_framechange(struct Main *bmain, Depsgraph *graph, float ctime);

/* Data changed recalculation entry point.
 * < context_type: context to perform evaluation for
 */
void DEG_evaluate_on_refresh(Depsgraph *graph);

bool DEG_needs_eval(Depsgraph *graph);

/* Editors Integration  -------------------------- */

/* Mechanism to allow editors to be informed of depsgraph updates,
 * to do their own updates based on changes.
 */

typedef struct DEGEditorUpdateContext {
  struct Main *bmain;
  struct Depsgraph *depsgraph;
  struct Scene *scene;
  struct ViewLayer *view_layer;
} DEGEditorUpdateContext;

typedef void (*DEG_EditorUpdateIDCb)(const DEGEditorUpdateContext *update_ctx, struct ID *id);
typedef void (*DEG_EditorUpdateSceneCb)(const DEGEditorUpdateContext *update_ctx, int updated);

/* Set callbacks which are being called when depsgraph changes. */
void DEG_editors_set_update_cb(DEG_EditorUpdateIDCb id_func, DEG_EditorUpdateSceneCb scene_func);

/* Evaluation  ----------------------------------- */

bool DEG_is_active(const struct Depsgraph *depsgraph);
void DEG_make_active(struct Depsgraph *depsgraph);
void DEG_make_inactive(struct Depsgraph *depsgraph);

/* Evaluation Debug ------------------------------ */

bool DEG_debug_is_evaluating(struct Depsgraph *depsgraph);

void DEG_debug_print_begin(struct Depsgraph *depsgraph);

void DEG_debug_print_eval(struct Depsgraph *depsgraph,
                          const char *function_name,
                          const char *object_name,
                          const void *object_address);

void DEG_debug_print_eval_subdata(struct Depsgraph *depsgraph,
                                  const char *function_name,
                                  const char *object_name,
                                  const void *object_address,
                                  const char *subdata_comment,
                                  const char *subdata_name,
                                  const void *subdata_address);

void DEG_debug_print_eval_subdata_index(struct Depsgraph *depsgraph,
                                        const char *function_name,
                                        const char *object_name,
                                        const void *object_address,
                                        const char *subdata_comment,
                                        const char *subdata_name,
                                        const void *subdata_address,
                                        const int subdata_index);

void DEG_debug_print_eval_parent_typed(struct Depsgraph *depsgraph,
                                       const char *function_name,
                                       const char *object_name,
                                       const void *object_address,
                                       const char *parent_comment,
                                       const char *parent_name,
                                       const void *parent_address);

void DEG_debug_print_eval_time(struct Depsgraph *depsgraph,
                               const char *function_name,
                               const char *object_name,
                               const void *object_address,
                               float time);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __DEG_DEPSGRAPH_H__ */
