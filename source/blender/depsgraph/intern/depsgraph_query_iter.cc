/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 *
 * Implementation of Querying and Filtering API's
 */

/* Silence warnings from copying deprecated fields. */
#define DNA_DEPRECATED_ALLOW

#include "MEM_guardedalloc.h"

#include "BKE_duplilist.hh"
#include "BKE_geometry_set.hh"
#include "BKE_idprop.hh"
#include "BKE_layer.hh"
#include "BKE_modifier.hh"
#include "BKE_node.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "intern/depsgraph.hh"
#include "intern/node/deg_node_id.hh"

#ifndef NDEBUG
#  include "intern/eval/deg_eval_copy_on_write.h"
#endif

/* If defined, all working data will be set to an invalid state, helping
 * to catch issues when areas accessing data which is considered to be no
 * longer available. */
#undef INVALIDATE_WORK_DATA

#ifndef NDEBUG
#  define INVALIDATE_WORK_DATA
#endif

namespace deg = blender::deg;

/* ************************ DEG ITERATORS ********************* */

namespace {

void deg_invalidate_iterator_work_data(DEGObjectIterData *data)
{
#ifdef INVALIDATE_WORK_DATA
  BLI_assert(data != nullptr);
  memset((void *)&data->temp_dupli_object, 0xff, sizeof(data->temp_dupli_object));
#else
  (void)data;
#endif
}

void ensure_id_properties_freed(const Object *dupli_object, Object *temp_dupli_object)
{
  if (temp_dupli_object->id.properties == nullptr) {
    /* No ID properties in temp data-block -- no leak is possible. */
    return;
  }
  if (temp_dupli_object->id.properties == dupli_object->id.properties) {
    /* Temp copy of object did not modify ID properties. */
    return;
  }
  /* Free memory which is owned by temporary storage which is about to get overwritten. */
  IDP_FreeProperty(temp_dupli_object->id.properties);
  temp_dupli_object->id.properties = nullptr;
}

void free_owned_memory(DEGObjectIterData *data)
{
  if (data->dupli_object_current == nullptr) {
    /* We didn't enter duplication yet, so we can't have any dangling pointers. */
    return;
  }

  const Object *dupli_object = data->dupli_object_current->ob;
  Object *temp_dupli_object = &data->temp_dupli_object;

  ensure_id_properties_freed(dupli_object, temp_dupli_object);
}

bool deg_object_hide_original(eEvaluationMode eval_mode, Object *ob, DupliObject *dob)
{
  /* Automatic hiding if this object is being instanced on verts/faces/frames
   * by its parent. Ideally this should not be needed, but due to the wrong
   * dependency direction in the data design there is no way to keep the object
   * visible otherwise. The better solution eventually would be for objects
   * to specify which object they instance, instead of through parenting.
   *
   * This function should not be used for meta-balls. They have custom visibility rules, as hiding
   * the base meta-ball will also hide all the other balls in the group. */
  if (eval_mode == DAG_EVAL_RENDER || dob) {
    const int hide_original_types = OB_DUPLIVERTS | OB_DUPLIFACES;

    if (!dob || !(dob->type & hide_original_types)) {
      if (ob->parent && (ob->parent->transflag & hide_original_types)) {
        return true;
      }
    }
  }

  return false;
}

void deg_iterator_duplis_init(DEGObjectIterData *data, Object *object, ListBase *duplis)
{
  data->dupli_parent = object;
  data->dupli_list = duplis;
  data->dupli_object_next = static_cast<DupliObject *>(duplis->first);
}

/* Returns false when iterator is exhausted. */
bool deg_iterator_duplis_step(DEGObjectIterData *data)
{
  if (data->dupli_list == nullptr) {
    return false;
  }

  while (data->dupli_object_next != nullptr) {
    DupliObject *dob = data->dupli_object_next;
    Object *obd = dob->ob;

    data->dupli_object_next = data->dupli_object_next->next;

    if (dob->no_draw) {
      continue;
    }
    if (dob->ob_data && GS(dob->ob_data->name) == ID_MB) {
      continue;
    }
    if (obd->type != OB_MBALL && deg_object_hide_original(data->eval_mode, dob->ob, dob)) {
      continue;
    }

    free_owned_memory(data);

    data->dupli_object_current = dob;

    /* Temporary object to evaluate. */
    Object *dupli_parent = data->dupli_parent;
    Object *temp_dupli_object = &data->temp_dupli_object;

    *temp_dupli_object = blender::dna::shallow_copy(*dob->ob);
    temp_dupli_object->runtime = &data->temp_dupli_object_runtime;
    *temp_dupli_object->runtime = *dob->ob->runtime;

    temp_dupli_object->base_flag = dupli_parent->base_flag | BASE_FROM_DUPLI;
    temp_dupli_object->base_local_view_bits = dupli_parent->base_local_view_bits;
    temp_dupli_object->runtime->local_collections_bits =
        dupli_parent->runtime->local_collections_bits;
    temp_dupli_object->dt = std::min(temp_dupli_object->dt, dupli_parent->dt);
    copy_v4_v4(temp_dupli_object->color, dupli_parent->color);
    temp_dupli_object->runtime->select_id = dupli_parent->runtime->select_id;
    if (dob->ob->data != dob->ob_data) {
      BKE_object_replace_data_on_shallow_copy(temp_dupli_object, dob->ob_data);
    }

    /* Duplicated elements shouldn't care whether their original collection is visible or not. */
    temp_dupli_object->base_flag |= BASE_ENABLED_AND_MAYBE_VISIBLE_IN_VIEWPORT;

    int ob_visibility = BKE_object_visibility(temp_dupli_object, data->eval_mode);
    if ((ob_visibility & (OB_VISIBLE_SELF | OB_VISIBLE_PARTICLES)) == 0) {
      continue;
    }

    /* This could be avoided by refactoring make_dupli() in order to track all negative scaling
     * recursively. */
    bool is_neg_scale = is_negative_m4(dob->mat);
    SET_FLAG_FROM_TEST(data->temp_dupli_object.transflag, is_neg_scale, OB_NEG_SCALE);

    copy_m4_m4(data->temp_dupli_object.runtime->object_to_world.ptr(), dob->mat);
    invert_m4_m4(data->temp_dupli_object.runtime->world_to_object.ptr(),
                 data->temp_dupli_object.object_to_world().ptr());
    data->next_object = &data->temp_dupli_object;
    BLI_assert(deg::deg_validate_eval_copy_datablock(&data->temp_dupli_object.id));
    return true;
  }

  free_owned_memory(data);
  free_object_duplilist(data->dupli_list);
  data->dupli_parent = nullptr;
  data->dupli_list = nullptr;
  data->dupli_object_next = nullptr;
  data->dupli_object_current = nullptr;
  deg_invalidate_iterator_work_data(data);
  return false;
}

/* Returns false when iterator is exhausted. */
bool deg_iterator_objects_step(DEGObjectIterData *data)
{
  deg::Depsgraph *deg_graph = reinterpret_cast<deg::Depsgraph *>(data->graph);

  for (; data->id_node_index < data->num_id_nodes; data->id_node_index++) {
    deg::IDNode *id_node = deg_graph->id_nodes[data->id_node_index];

    /* Use the build time visibility so that the ID is not appearing/disappearing throughout
     * animation export. */
    if (!id_node->is_visible_on_build) {
      continue;
    }

    const ID_Type id_type = GS(id_node->id_orig->name);

    if (id_type != ID_OB) {
      continue;
    }

    switch (id_node->linked_state) {
      case deg::DEG_ID_LINKED_DIRECTLY:
        if ((data->flag & DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY) == 0) {
          continue;
        }
        break;
      case deg::DEG_ID_LINKED_VIA_SET:
        if ((data->flag & DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET) == 0) {
          continue;
        }
        break;
      case deg::DEG_ID_LINKED_INDIRECTLY:
        if ((data->flag & DEG_ITER_OBJECT_FLAG_LINKED_INDIRECTLY) == 0) {
          continue;
        }
        break;
    }

    Object *object = (Object *)id_node->id_cow;
    Object *object_orig = DEG_get_original_object(object);

    /* NOTE: The object might be invisible after the latest depsgraph evaluation, in which case
     * going into its evaluated state might not be safe. For example, its evaluated mesh state
     * might point to a freed data-block if the mesh is animated.
     * So it is required to perform the visibility checks prior to looking into any deeper into the
     * object. */

    BLI_assert(deg::deg_eval_copy_is_expanded(&object->id));

    object->runtime->select_id = object_orig->runtime->select_id;

    const bool use_preview = object_orig == data->object_orig_with_preview;
    if (use_preview) {
      ListBase *preview_duplis = object_duplilist_preview(
          data->graph, data->scene, object, data->settings->viewer_path);
      deg_iterator_duplis_init(data, object, preview_duplis);
      data->id_node_index++;
      return true;
    }

    int ob_visibility = OB_VISIBLE_ALL;
    if (data->flag & DEG_ITER_OBJECT_FLAG_VISIBLE) {
      ob_visibility = BKE_object_visibility(object, data->eval_mode);

      if (object->type != OB_MBALL && deg_object_hide_original(data->eval_mode, object, nullptr)) {
        continue;
      }
    }

    if (ob_visibility & OB_VISIBLE_INSTANCES) {
      if ((data->flag & DEG_ITER_OBJECT_FLAG_DUPLI) &&
          ((object->transflag & OB_DUPLI) || object->runtime->geometry_set_eval != nullptr))
      {
        BLI_assert(deg::deg_validate_eval_copy_datablock(&object->id));
        ListBase *duplis = object_duplilist(data->graph, data->scene, object);
        deg_iterator_duplis_init(data, object, duplis);
      }
    }

    if (ob_visibility & (OB_VISIBLE_SELF | OB_VISIBLE_PARTICLES)) {
      BLI_assert(deg::deg_validate_eval_copy_datablock(&object->id));
      data->next_object = object;
    }
    data->id_node_index++;
    return true;
  }
  return false;
}

}  // namespace

DEGObjectIterData &DEGObjectIterData::operator=(const DEGObjectIterData &other)
{
  if (this != &other) {
    this->settings = other.settings;
    this->graph = other.graph;
    this->flag = other.flag;
    this->scene = other.scene;
    this->eval_mode = other.eval_mode;
    this->object_orig_with_preview = other.object_orig_with_preview;
    this->next_object = other.next_object;
    this->dupli_parent = other.dupli_parent;
    this->dupli_list = other.dupli_list;
    this->dupli_object_next = other.dupli_object_next;
    this->dupli_object_current = other.dupli_object_current;
    this->temp_dupli_object = blender::dna::shallow_copy(other.temp_dupli_object);
    this->temp_dupli_object_runtime = other.temp_dupli_object_runtime;
    this->temp_dupli_object.runtime = &temp_dupli_object_runtime;
    this->id_node_index = other.id_node_index;
    this->num_id_nodes = other.num_id_nodes;
  }
  return *this;
}

static Object *find_object_with_preview_geometry(const ViewerPath &viewer_path)
{
  if (BLI_listbase_is_empty(&viewer_path.path)) {
    return nullptr;
  }
  const ViewerPathElem *elem = static_cast<const ViewerPathElem *>(viewer_path.path.first);
  if (elem->type != VIEWER_PATH_ELEM_TYPE_ID) {
    return nullptr;
  }
  const IDViewerPathElem *id_elem = reinterpret_cast<const IDViewerPathElem *>(elem);
  if (id_elem->id == nullptr) {
    return nullptr;
  }
  if (GS(id_elem->id->name) != ID_OB) {
    return nullptr;
  }
  Object *object = reinterpret_cast<Object *>(id_elem->id);
  if (elem->next->type != VIEWER_PATH_ELEM_TYPE_MODIFIER) {
    return nullptr;
  }
  const ModifierViewerPathElem *modifier_elem = reinterpret_cast<const ModifierViewerPathElem *>(
      elem->next);
  ModifierData *md = BKE_modifiers_findby_name(object, modifier_elem->modifier_name);
  if (md == nullptr) {
    return nullptr;
  }
  if (!(md->mode & eModifierMode_Realtime)) {
    return nullptr;
  }
  return reinterpret_cast<Object *>(id_elem->id);
}

void DEG_iterator_objects_begin(BLI_Iterator *iter, DEGObjectIterData *data)
{
  Depsgraph *depsgraph = data->graph;
  deg::Depsgraph *deg_graph = reinterpret_cast<deg::Depsgraph *>(depsgraph);
  const size_t num_id_nodes = deg_graph->id_nodes.size();

  iter->data = data;

  if (num_id_nodes == 0) {
    iter->valid = false;
    return;
  }

  data->next_object = nullptr;
  data->dupli_parent = nullptr;
  data->dupli_list = nullptr;
  data->dupli_object_next = nullptr;
  data->dupli_object_current = nullptr;
  data->scene = DEG_get_evaluated_scene(depsgraph);
  data->id_node_index = 0;
  data->num_id_nodes = num_id_nodes;
  data->eval_mode = DEG_get_mode(depsgraph);
  deg_invalidate_iterator_work_data(data);

  /* Determine if the preview of any object should be in the iterator. */
  const ViewerPath *viewer_path = data->settings->viewer_path;
  if (viewer_path != nullptr) {
    data->object_orig_with_preview = find_object_with_preview_geometry(*viewer_path);
  }

  DEG_iterator_objects_next(iter);
}

void DEG_iterator_objects_next(BLI_Iterator *iter)
{
  DEGObjectIterData *data = (DEGObjectIterData *)iter->data;
  while (true) {
    if (data->next_object != nullptr) {
      iter->current = data->next_object;
      data->next_object = nullptr;
      return;
    }
    if (deg_iterator_duplis_step(data)) {
      continue;
    }
    if (deg_iterator_objects_step(data)) {
      continue;
    }
    iter->valid = false;
    break;
  }
}

void DEG_iterator_objects_end(BLI_Iterator *iter)
{
  DEGObjectIterData *data = (DEGObjectIterData *)iter->data;
  if (data != nullptr) {
    /* Force crash in case the iterator data is referenced and accessed down
     * the line. (#51718) */
    deg_invalidate_iterator_work_data(data);
  }
}

/* ************************ DEG ID ITERATOR ********************* */

static void DEG_iterator_ids_step(BLI_Iterator *iter, deg::IDNode *id_node, bool only_updated)
{
  ID *id_cow = id_node->id_cow;

  /* Use the build time visibility so that the ID is not appearing/disappearing throughout
   * animation export.
   * When the dependency graph is asked for updates report all IDs, as the user of those updates
   * might need to react to updates coming from IDs which do change visibility throughout the
   * life-time of the graph. */
  if (!only_updated && !id_node->is_visible_on_build) {
    iter->skip = true;
    return;
  }

  if (only_updated && !(id_cow->recalc & ID_RECALC_ALL)) {
    /* Node-tree is considered part of the data-block. */
    bNodeTree *ntree = blender::bke::ntreeFromID(id_cow);
    if (ntree == nullptr) {
      iter->skip = true;
      return;
    }
    if ((ntree->id.recalc & ID_RECALC_NTREE_OUTPUT) == 0) {
      iter->skip = true;
      return;
    }
  }

  iter->current = id_cow;
  iter->skip = false;
}

void DEG_iterator_ids_begin(BLI_Iterator *iter, DEGIDIterData *data)
{
  Depsgraph *depsgraph = data->graph;
  deg::Depsgraph *deg_graph = reinterpret_cast<deg::Depsgraph *>(depsgraph);
  const size_t num_id_nodes = deg_graph->id_nodes.size();

  iter->data = data;

  if ((num_id_nodes == 0) || (data->only_updated && !DEG_id_type_any_updated(depsgraph))) {
    iter->valid = false;
    return;
  }

  data->id_node_index = 0;
  data->num_id_nodes = num_id_nodes;

  deg::IDNode *id_node = deg_graph->id_nodes[data->id_node_index];
  DEG_iterator_ids_step(iter, id_node, data->only_updated);

  if (iter->skip) {
    DEG_iterator_ids_next(iter);
  }
}

void DEG_iterator_ids_next(BLI_Iterator *iter)
{
  DEGIDIterData *data = (DEGIDIterData *)iter->data;
  Depsgraph *depsgraph = data->graph;
  deg::Depsgraph *deg_graph = reinterpret_cast<deg::Depsgraph *>(depsgraph);

  do {
    iter->skip = false;

    ++data->id_node_index;
    if (data->id_node_index == data->num_id_nodes) {
      iter->valid = false;
      return;
    }

    deg::IDNode *id_node = deg_graph->id_nodes[data->id_node_index];
    DEG_iterator_ids_step(iter, id_node, data->only_updated);
  } while (iter->skip);
}

void DEG_iterator_ids_end(BLI_Iterator * /*iter*/) {}
