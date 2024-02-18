/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 *
 * Public API for Querying Depsgraph.
 */

#pragma once

#include "BLI_function_ref.hh"
#include "BLI_iterator.h"
#include "BLI_utildefines.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

/* Needed for the instance iterator. */
#include "DNA_object_types.h"

#include "BKE_object_types.hh"

struct BLI_Iterator;
struct CustomData_MeshMasks;
struct Depsgraph;
struct DupliObject;
struct ID;
struct ListBase;
struct PointerRNA;
struct Scene;
struct ViewLayer;
struct ViewerPath;

/* -------------------------------------------------------------------- */
/** \name DEG input data
 * \{ */

/** Get scene that depsgraph was built for. */
Scene *DEG_get_input_scene(const Depsgraph *graph);

/** Get view layer that depsgraph was built for. */
ViewLayer *DEG_get_input_view_layer(const Depsgraph *graph);

/** Get bmain that depsgraph was built for. */
Main *DEG_get_bmain(const Depsgraph *graph);

/** Get evaluation mode that depsgraph was built for. */
eEvaluationMode DEG_get_mode(const Depsgraph *graph);

/** Get time that depsgraph is being evaluated or was last evaluated at. */
float DEG_get_ctime(const Depsgraph *graph);

/** \} */

/* -------------------------------------------------------------------- */
/** \name DEG evaluated data
 * \{ */

/** Check if given ID type was tagged for update. */
bool DEG_id_type_updated(const Depsgraph *depsgraph, short id_type);
bool DEG_id_type_any_updated(const Depsgraph *depsgraph);

/** Check if given ID type is present in the depsgraph */
bool DEG_id_type_any_exists(const Depsgraph *depsgraph, short id_type);

/** Get additional evaluation flags for the given ID. */
uint32_t DEG_get_eval_flags_for_id(const Depsgraph *graph, const ID *id);

/** Get additional mesh CustomData_MeshMasks flags for the given object. */
void DEG_get_customdata_mask_for_object(const Depsgraph *graph,
                                        Object *object,
                                        CustomData_MeshMasks *r_mask);

/**
 * Get scene at its evaluated state.
 *
 * Technically, this is a copied-on-written and fully evaluated version of the input scene.
 * This function will check that the data-block has been expanded (and copied) from the original
 * one. Assert will happen if it's not.
 */
Scene *DEG_get_evaluated_scene(const Depsgraph *graph);

/**
 * Get view layer at its evaluated state.
 * This is a shortcut for accessing active view layer from evaluated scene.
 */
ViewLayer *DEG_get_evaluated_view_layer(const Depsgraph *graph);

/** Get evaluated version of object for given original one. */
Object *DEG_get_evaluated_object(const Depsgraph *depsgraph, Object *object);

/** Get evaluated version of given ID data-block. */
ID *DEG_get_evaluated_id(const Depsgraph *depsgraph, ID *id);

/** Get evaluated version of data pointed to by RNA pointer */
void DEG_get_evaluated_rna_pointer(const Depsgraph *depsgraph,
                                   PointerRNA *ptr,
                                   PointerRNA *r_ptr_eval);

/** Get original version of object for given evaluated one. */
Object *DEG_get_original_object(Object *object);

/** Get original version of given evaluated ID data-block. */
ID *DEG_get_original_id(ID *id);

/**
 * Check whether given ID is an original.
 *
 * Original IDs are considered all the IDs which are not covered by copy-on-write system and are
 * not out-of-main localized data-blocks.
 */
bool DEG_is_original_id(const ID *id);
bool DEG_is_original_object(const Object *object);

/* Opposite of the above.
 *
 * If the data-block is not original it must be evaluated, and vice versa. */

bool DEG_is_evaluated_id(const ID *id);
bool DEG_is_evaluated_object(const Object *object);

/**
 * Check whether depsgraph is fully evaluated. This includes the following checks:
 * - Relations are up-to-date.
 * - Nothing is tagged for update.
 */
bool DEG_is_fully_evaluated(const Depsgraph *depsgraph);

/** \} */

/* -------------------------------------------------------------------- */
/** \name DEG object iterators
 * \{ */

enum DegIterFlag {
  DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY = (1 << 0),
  DEG_ITER_OBJECT_FLAG_LINKED_INDIRECTLY = (1 << 1),
  DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET = (1 << 2),
  DEG_ITER_OBJECT_FLAG_VISIBLE = (1 << 3),
  DEG_ITER_OBJECT_FLAG_DUPLI = (1 << 4),
};
ENUM_OPERATORS(DegIterFlag, DEG_ITER_OBJECT_FLAG_DUPLI)

struct DEGObjectIterSettings {
  Depsgraph *depsgraph;
  /**
   * Bit-field of the #DegIterFlag.
   *
   * NOTE: Be careful with #DEG_ITER_OBJECT_FLAG_LINKED_INDIRECTLY objects.
   * Although they are available they have no overrides (collection_properties)
   * and will crash if you try to access it.
   */
  uint32_t flags;

  /**
   * When set, the final evaluated geometry of the corresponding object is omitted. Instead the
   * geometry for the viewer path included in the iterator.
   */
  const ViewerPath *viewer_path;
};

/**
 * Flags to get objects for draw manager and final render.
 */
#define DEG_OBJECT_ITER_FOR_RENDER_ENGINE_FLAGS \
  DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY | DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET | \
      DEG_ITER_OBJECT_FLAG_VISIBLE | DEG_ITER_OBJECT_FLAG_DUPLI

struct DEGObjectIterData {
  DEGObjectIterSettings *settings;
  Depsgraph *graph;
  int flag;

  Scene *scene;

  eEvaluationMode eval_mode;

  /** Object whose preview instead of evaluated geometry should be part of the iterator. */
  Object *object_orig_with_preview;

  Object *next_object;

  /* **** Iteration over dupli-list. *** */

  /* Object which created the dupli-list. */
  Object *dupli_parent;
  /* List of duplicated objects. */
  ListBase *dupli_list;
  /* Next duplicated object to step into. */
  DupliObject *dupli_object_next;
  /* Corresponds to current object: current iterator object is evaluated from
   * this duplicated object. */
  DupliObject *dupli_object_current;
  /* Temporary storage to report fully populated DNA to the render engine or
   * other users of the iterator. */
  Object temp_dupli_object;
  blender::bke::ObjectRuntime temp_dupli_object_runtime;

  /* **** Iteration over ID nodes **** */
  size_t id_node_index;
  size_t num_id_nodes;
  DEGObjectIterData &operator=(const DEGObjectIterData &other);
};

void DEG_iterator_objects_begin(BLI_Iterator *iter, DEGObjectIterData *data);
void DEG_iterator_objects_next(BLI_Iterator *iter);
void DEG_iterator_objects_end(BLI_Iterator *iter);

#define DEG_OBJECT_ITER_BEGIN(settings_, instance_) \
  { \
    DEGObjectIterData data_ = { \
        (settings_), \
        (settings_)->depsgraph, \
        (int)(settings_)->flags, \
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name DEG ID iterators
 * \{ */

struct DEGIDIterData {
  Depsgraph *graph;
  bool only_updated;

  size_t id_node_index;
  size_t num_id_nodes;
};

void DEG_iterator_ids_begin(BLI_Iterator *iter, DEGIDIterData *data);
void DEG_iterator_ids_next(BLI_Iterator *iter);
void DEG_iterator_ids_end(BLI_Iterator *iter);

/** \} */

/* -------------------------------------------------------------------- */
/** \name DEG traversal
 * \{ */

using DEGForeachIDCallback = blender::FunctionRef<void(ID *id)>;
using DEGForeachIDComponentCallback =
    blender::FunctionRef<void(ID *id, eDepsObjectComponentType component)>;

/**
 * \note Modifies runtime flags in depsgraph nodes,
 * so can not be used in parallel. Keep an eye on that!
 */
void DEG_foreach_ancestor_ID(const Depsgraph *depsgraph,
                             const ID *id,
                             DEGForeachIDCallback callback);
void DEG_foreach_dependent_ID(const Depsgraph *depsgraph,
                              const ID *id,
                              DEGForeachIDCallback callback);

/**
 * Starts traversal from given component of the given ID, invokes callback for every other
 * component  which is directly on indirectly dependent on the source one.
 */
enum {
  /* Ignore transform solvers which depends on multiple inputs and affects final transform.
   * Is used for cases like snapping objects which are part of a rigid body simulation:
   * without this there will be "false-positive" dependencies between transform components of
   * objects:
   *
   *     object 1 transform before solver ---> solver ------> object 1 final transform
   *     object 2 transform before solver -----^     \------> object 2 final transform
   */
  DEG_FOREACH_COMPONENT_IGNORE_TRANSFORM_SOLVERS = (1 << 0),
};
void DEG_foreach_dependent_ID_component(const Depsgraph *depsgraph,
                                        const ID *id,
                                        eDepsObjectComponentType source_component_type,
                                        int flags,
                                        DEGForeachIDComponentCallback callback);

void DEG_foreach_ID(const Depsgraph *depsgraph, DEGForeachIDCallback callback);

/** \} */
