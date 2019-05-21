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
 * Public API for Querying Depsgraph.
 */

#ifndef __DEG_DEPSGRAPH_QUERY_H__
#define __DEG_DEPSGRAPH_QUERY_H__

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

/* Needed for the instance iterator. */
#include "DNA_object_types.h"

struct ID;

struct BLI_Iterator;
struct Base;
struct CustomData_MeshMasks;
struct Depsgraph;
struct DupliObject;
struct ListBase;
struct Scene;
struct ViewLayer;

#ifdef __cplusplus
extern "C" {
#endif

/* *********************** DEG input data ********************* */

/* Get scene that depsgraph was built for. */
struct Scene *DEG_get_input_scene(const Depsgraph *graph);

/* Get view layer that depsgraph was built for. */
struct ViewLayer *DEG_get_input_view_layer(const Depsgraph *graph);

/* Get evaluation mode that depsgraph was built for. */
eEvaluationMode DEG_get_mode(const Depsgraph *graph);

/* Get time that depsgraph is being evaluated or was last evaluated at. */
float DEG_get_ctime(const Depsgraph *graph);

/* ********************* DEG evaluated data ******************* */

/* Check if given ID type was tagged for update. */
bool DEG_id_type_updated(const struct Depsgraph *depsgraph, short id_type);
bool DEG_id_type_any_updated(const struct Depsgraph *depsgraph);

/* Get additional evaluation flags for the given ID. */
uint32_t DEG_get_eval_flags_for_id(const struct Depsgraph *graph, struct ID *id);

/* Get additional mesh CustomData_MeshMasks flags for the given object. */
void DEG_get_customdata_mask_for_object(const struct Depsgraph *graph,
                                        struct Object *object,
                                        struct CustomData_MeshMasks *r_mask);

/* Get scene at its evaluated state.
 *
 * Technically, this is a copied-on-written and fully evaluated version of the input scene.
 * This function will check that the datablock has been expanded (and copied) from the original
 * one. Assert will happen if it's not. */
struct Scene *DEG_get_evaluated_scene(const struct Depsgraph *graph);

/* Get view layer at its evaluated state.
 * This is a shortcut for accessing active view layer from evaluated scene. */
struct ViewLayer *DEG_get_evaluated_view_layer(const struct Depsgraph *graph);

/* Get evaluated version of object for given original one. */
struct Object *DEG_get_evaluated_object(const struct Depsgraph *depsgraph, struct Object *object);

/* Get evaluated version of given ID datablock. */
struct ID *DEG_get_evaluated_id(const struct Depsgraph *depsgraph, struct ID *id);

/* Get evaluated version of data pointed to by RNA pointer */
void DEG_get_evaluated_rna_pointer(const struct Depsgraph *depsgraph,
                                   struct PointerRNA *ptr,
                                   struct PointerRNA *r_ptr_eval);

/* Get original version of object for given evaluated one. */
struct Object *DEG_get_original_object(struct Object *object);

/* Get original version of given evaluated ID datablock. */
struct ID *DEG_get_original_id(struct ID *id);

/* Check whether given ID is an original,
 *
 * Original IDs are considered all the IDs which are not covered by copy-on-write system and are
 * not out-of-main localized datablocks. */
bool DEG_is_original_id(struct ID *id);
bool DEG_is_original_object(struct Object *object);

/* Opposite of the above.
 *
 * If the datablock is not original it must be evaluated, and vice versa. */
bool DEG_is_evaluated_id(struct ID *id);
bool DEG_is_evaluated_object(struct Object *object);

/* ************************ DEG object iterators ********************* */

enum {
  DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY = (1 << 0),
  DEG_ITER_OBJECT_FLAG_LINKED_INDIRECTLY = (1 << 1),
  DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET = (1 << 2),
  DEG_ITER_OBJECT_FLAG_VISIBLE = (1 << 3),
  DEG_ITER_OBJECT_FLAG_DUPLI = (1 << 4),
};

typedef struct DEGObjectIterData {
  struct Depsgraph *graph;
  int flag;

  struct Scene *scene;

  eEvaluationMode eval_mode;

  /* **** Iteration over dupli-list. *** */

  /* Object which created the dupli-list. */
  struct Object *dupli_parent;
  /* List of duplicated objects. */
  struct ListBase *dupli_list;
  /* Next duplicated object to step into. */
  struct DupliObject *dupli_object_next;
  /* Corresponds to current object: current iterator object is evaluated from
   * this duplicated object. */
  struct DupliObject *dupli_object_current;
  /* Temporary storage to report fully populated DNA to the render engine or
   * other users of the iterator. */
  struct Object temp_dupli_object;

  /* **** Iteration over ID nodes **** */
  size_t id_node_index;
  size_t num_id_nodes;
} DEGObjectIterData;

void DEG_iterator_objects_begin(struct BLI_Iterator *iter, DEGObjectIterData *data);
void DEG_iterator_objects_next(struct BLI_Iterator *iter);
void DEG_iterator_objects_end(struct BLI_Iterator *iter);

/**
 * Note: Be careful with DEG_ITER_OBJECT_FLAG_LINKED_INDIRECTLY objects.
 * Although they are available they have no overrides (collection_properties)
 * and will crash if you try to access it.
 */
#define DEG_OBJECT_ITER_BEGIN(graph_, instance_, flag_) \
  { \
    DEGObjectIterData data_ = { \
        graph_, \
        flag_, \
    }; \
\
    ITER_BEGIN (DEG_iterator_objects_begin, \
                DEG_iterator_objects_next, \
                DEG_iterator_objects_end, \
                &data_, \
                Object *, \
                instance_)

#define DEG_OBJECT_ITER_END \
  ITER_END; \
  } \
  ((void)0)

/**
 * Depsgraph objects iterator for draw manager and final render
 */
#define DEG_OBJECT_ITER_FOR_RENDER_ENGINE_BEGIN(graph_, instance_) \
  DEG_OBJECT_ITER_BEGIN (graph_, \
                         instance_, \
                         DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY | \
                             DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET | DEG_ITER_OBJECT_FLAG_VISIBLE | \
                             DEG_ITER_OBJECT_FLAG_DUPLI)

#define DEG_OBJECT_ITER_FOR_RENDER_ENGINE_END DEG_OBJECT_ITER_END

/* ************************ DEG ID iterators ********************* */

typedef struct DEGIDIterData {
  struct Depsgraph *graph;
  bool only_updated;

  size_t id_node_index;
  size_t num_id_nodes;
} DEGIDIterData;

void DEG_iterator_ids_begin(struct BLI_Iterator *iter, DEGIDIterData *data);
void DEG_iterator_ids_next(struct BLI_Iterator *iter);
void DEG_iterator_ids_end(struct BLI_Iterator *iter);

/* ************************ DEG traversal ********************* */

typedef void (*DEGForeachIDCallback)(ID *id, void *user_data);

/* NOTE: Modifies runtime flags in depsgraph nodes, so can not be used in
 * parallel. Keep an eye on that!
 */
void DEG_foreach_ancestor_ID(const Depsgraph *depsgraph,
                             const ID *id,
                             DEGForeachIDCallback callback,
                             void *user_data);
void DEG_foreach_dependent_ID(const Depsgraph *depsgraph,
                              const ID *id,
                              DEGForeachIDCallback callback,
                              void *user_data);

void DEG_foreach_ID(const Depsgraph *depsgraph, DEGForeachIDCallback callback, void *user_data);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __DEG_DEPSGRAPH_QUERY_H__ */
