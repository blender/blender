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
 * The Original Code is Copyright (C) 2017 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 *
 * Implementation of Querying and Filtering API's
 */

/* Silence warnings from copying deprecated fields. */
#define DNA_DEPRECATED_ALLOW

#include "MEM_guardedalloc.h"

#include "BKE_duplilist.h"
#include "BKE_geometry_set.hh"
#include "BKE_idprop.h"
#include "BKE_layer.h"
#include "BKE_node.h"
#include "BKE_object.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "intern/depsgraph.h"
#include "intern/node/deg_node_id.h"

#ifndef NDEBUG
#  include "intern/eval/deg_eval_copy_on_write.h"
#endif

// If defined, all working data will be set to an invalid state, helping
// to catch issues when areas accessing data which is considered to be no
// longer available.
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
  memset(&data->temp_dupli_object, 0xff, sizeof(data->temp_dupli_object));
#else
  (void)data;
#endif
}

void verify_id_properties_freed(DEGObjectIterData *data)
{
  if (data->dupli_object_current == nullptr) {
    // We didn't enter duplication yet, so we can't have any dangling
    // pointers.
    return;
  }
  const Object *dupli_object = data->dupli_object_current->ob;
  Object *temp_dupli_object = &data->temp_dupli_object;
  if (temp_dupli_object->id.properties == nullptr) {
    // No ID properties in temp data-block -- no leak is possible.
    return;
  }
  if (temp_dupli_object->id.properties == dupli_object->id.properties) {
    // Temp copy of object did not modify ID properties.
    return;
  }
  // Free memory which is owned by temporary storage which is about to
  // get overwritten.
  IDP_FreeProperty(temp_dupli_object->id.properties);
  temp_dupli_object->id.properties = nullptr;
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

void deg_iterator_components_init(DEGObjectIterData *data, Object *object)
{
  data->geometry_component_owner = object;
  data->geometry_component_id = 0;
}

/* Returns false when iterator is exhausted. */
bool deg_iterator_components_step(BLI_Iterator *iter)
{
  DEGObjectIterData *data = (DEGObjectIterData *)iter->data;
  if (data->geometry_component_owner == nullptr) {
    return false;
  }

  if (data->geometry_component_owner->runtime.geometry_set_eval == nullptr) {
    /* Return the object itself, if it does not have a geometry set yet. */
    iter->current = data->geometry_component_owner;
    data->geometry_component_owner = nullptr;
    return true;
  }

  GeometrySet *geometry_set = data->geometry_component_owner->runtime.geometry_set_eval;
  if (geometry_set == nullptr) {
    data->geometry_component_owner = nullptr;
    return false;
  }

  /* The mesh component. */
  if (data->geometry_component_id == 0) {
    data->geometry_component_id++;

    /* Don't use a temporary object for this component, when the owner is a mesh object. */
    if (data->geometry_component_owner->type == OB_MESH) {
      iter->current = data->geometry_component_owner;
      return true;
    }

    const Mesh *mesh = geometry_set->get_mesh_for_read();
    if (mesh != nullptr) {
      Object *temp_object = &data->temp_geometry_component_object;
      *temp_object = *data->geometry_component_owner;
      temp_object->type = OB_MESH;
      temp_object->data = (void *)mesh;
      temp_object->runtime.select_id = data->geometry_component_owner->runtime.select_id;
      iter->current = temp_object;
      return true;
    }
  }

  /* The pointcloud component. */
  if (data->geometry_component_id == 1) {
    data->geometry_component_id++;

    /* Don't use a temporary object for this component, when the owner is a point cloud object. */
    if (data->geometry_component_owner->type == OB_POINTCLOUD) {
      iter->current = data->geometry_component_owner;
      return true;
    }

    const PointCloud *pointcloud = geometry_set->get_pointcloud_for_read();
    if (pointcloud != nullptr) {
      Object *temp_object = &data->temp_geometry_component_object;
      *temp_object = *data->geometry_component_owner;
      temp_object->type = OB_POINTCLOUD;
      temp_object->data = (void *)pointcloud;
      temp_object->runtime.select_id = data->geometry_component_owner->runtime.select_id;
      iter->current = temp_object;
      return true;
    }
  }

  /* The volume component. */
  if (data->geometry_component_id == 2) {
    data->geometry_component_id++;

    /* Don't use a temporary object for this component, when the owner is a volume object. */
    if (data->geometry_component_owner->type == OB_VOLUME) {
      iter->current = data->geometry_component_owner;
      return true;
    }

    const VolumeComponent *component = geometry_set->get_component_for_read<VolumeComponent>();
    if (component != nullptr) {
      const Volume *volume = component->get_for_read();

      if (volume != nullptr) {
        Object *temp_object = &data->temp_geometry_component_object;
        *temp_object = *data->geometry_component_owner;
        temp_object->type = OB_VOLUME;
        temp_object->data = (void *)volume;
        temp_object->runtime.select_id = data->geometry_component_owner->runtime.select_id;
        iter->current = temp_object;
        return true;
      }
    }
  }

  data->geometry_component_owner = nullptr;
  return false;
}

void deg_iterator_duplis_init(DEGObjectIterData *data, Object *object)
{
  if ((data->flag & DEG_ITER_OBJECT_FLAG_DUPLI) &&
      ((object->transflag & OB_DUPLI) || object->runtime.geometry_set_eval != nullptr)) {
    data->dupli_parent = object;
    data->dupli_list = object_duplilist(data->graph, data->scene, object);
    data->dupli_object_next = (DupliObject *)data->dupli_list->first;
  }
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
    if (obd->type == OB_MBALL) {
      continue;
    }
    if (deg_object_hide_original(data->eval_mode, dob->ob, dob)) {
      continue;
    }

    verify_id_properties_freed(data);

    data->dupli_object_current = dob;

    /* Temporary object to evaluate. */
    Object *dupli_parent = data->dupli_parent;
    Object *temp_dupli_object = &data->temp_dupli_object;
    *temp_dupli_object = *dob->ob;
    temp_dupli_object->base_flag = dupli_parent->base_flag | BASE_FROM_DUPLI;
    temp_dupli_object->base_local_view_bits = dupli_parent->base_local_view_bits;
    temp_dupli_object->runtime.local_collections_bits =
        dupli_parent->runtime.local_collections_bits;
    temp_dupli_object->dt = MIN2(temp_dupli_object->dt, dupli_parent->dt);
    copy_v4_v4(temp_dupli_object->color, dupli_parent->color);
    temp_dupli_object->runtime.select_id = dupli_parent->runtime.select_id;

    /* Duplicated elements shouldn't care whether their original collection is visible or not. */
    temp_dupli_object->base_flag |= BASE_VISIBLE_DEPSGRAPH;

    int ob_visibility = BKE_object_visibility(temp_dupli_object, data->eval_mode);
    if ((ob_visibility & (OB_VISIBLE_SELF | OB_VISIBLE_PARTICLES)) == 0) {
      continue;
    }

    /* This could be avoided by refactoring make_dupli() in order to track all negative scaling
     * recursively. */
    bool is_neg_scale = is_negative_m4(dob->mat);
    SET_FLAG_FROM_TEST(data->temp_dupli_object.transflag, is_neg_scale, OB_NEG_SCALE);

    copy_m4_m4(data->temp_dupli_object.obmat, dob->mat);
    invert_m4_m4(data->temp_dupli_object.imat, data->temp_dupli_object.obmat);
    deg_iterator_components_init(data, &data->temp_dupli_object);
    BLI_assert(deg::deg_validate_copy_on_write_datablock(&data->temp_dupli_object.id));
    return true;
  }

  verify_id_properties_freed(data);
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

    if (!id_node->is_directly_visible) {
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
    BLI_assert(deg::deg_validate_copy_on_write_datablock(&object->id));

    int ob_visibility = OB_VISIBLE_ALL;
    if (data->flag & DEG_ITER_OBJECT_FLAG_VISIBLE) {
      ob_visibility = BKE_object_visibility(object, data->eval_mode);

      if (object->type != OB_MBALL && deg_object_hide_original(data->eval_mode, object, nullptr)) {
        continue;
      }
    }

    object->runtime.select_id = DEG_get_original_object(object)->runtime.select_id;
    if (ob_visibility & OB_VISIBLE_INSTANCES) {
      deg_iterator_duplis_init(data, object);
    }

    if (ob_visibility & (OB_VISIBLE_SELF | OB_VISIBLE_PARTICLES)) {
      deg_iterator_components_init(data, object);
    }
    data->id_node_index++;
    return true;
  }
  return false;
}

}  // namespace

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

  data->dupli_parent = nullptr;
  data->dupli_list = nullptr;
  data->dupli_object_next = nullptr;
  data->dupli_object_current = nullptr;
  data->scene = DEG_get_evaluated_scene(depsgraph);
  data->id_node_index = 0;
  data->num_id_nodes = num_id_nodes;
  data->eval_mode = DEG_get_mode(depsgraph);
  data->geometry_component_id = 0;
  data->geometry_component_owner = nullptr;
  deg_invalidate_iterator_work_data(data);

  DEG_iterator_objects_next(iter);
}

void DEG_iterator_objects_next(BLI_Iterator *iter)
{
  DEGObjectIterData *data = (DEGObjectIterData *)iter->data;
  while (true) {
    if (deg_iterator_components_step(iter)) {
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
     * the line. (T51718) */
    deg_invalidate_iterator_work_data(data);
  }
}

/* ************************ DEG ID ITERATOR ********************* */

static void DEG_iterator_ids_step(BLI_Iterator *iter, deg::IDNode *id_node, bool only_updated)
{
  ID *id_cow = id_node->id_cow;

  if (!id_node->is_directly_visible) {
    iter->skip = true;
    return;
  }
  if (only_updated && !(id_cow->recalc & ID_RECALC_ALL)) {
    bNodeTree *ntree = ntreeFromID(id_cow);

    /* Node-tree is considered part of the data-block. */
    if (!(ntree && (ntree->id.recalc & ID_RECALC_ALL))) {
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

void DEG_iterator_ids_end(BLI_Iterator *UNUSED(iter))
{
}
