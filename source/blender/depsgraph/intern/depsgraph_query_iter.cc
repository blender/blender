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

extern "C" {
#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BKE_anim.h"
#include "BKE_idprop.h"
#include "BKE_layer.h"
#include "BKE_node.h"
#include "BKE_object.h"
} /* extern "C" */

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

/* ************************ DEG ITERATORS ********************* */

namespace {

void deg_invalidate_iterator_work_data(DEGObjectIterData *data)
{
#ifdef INVALIDATE_WORK_DATA
  BLI_assert(data != NULL);
  memset(&data->temp_dupli_object, 0xff, sizeof(data->temp_dupli_object));
#else
  (void)data;
#endif
}

void verify_id_properties_freed(DEGObjectIterData *data)
{
  if (data->dupli_object_current == NULL) {
    // We didn't enter duplication yet, so we can't have any dangling
    // pointers.
    return;
  }
  const Object *dupli_object = data->dupli_object_current->ob;
  Object *temp_dupli_object = &data->temp_dupli_object;
  if (temp_dupli_object->id.properties == NULL) {
    // No ID properties in temp datablock -- no leak is possible.
    return;
  }
  if (temp_dupli_object->id.properties == dupli_object->id.properties) {
    // Temp copy of object did not modify ID properties.
    return;
  }
  // Free memory which is owned by temporary storage which is about to
  // get overwritten.
  IDP_FreeProperty(temp_dupli_object->id.properties);
  MEM_freeN(temp_dupli_object->id.properties);
  temp_dupli_object->id.properties = NULL;
}

static bool deg_object_hide_original(eEvaluationMode eval_mode, Object *ob, DupliObject *dob)
{
  /* Automatic hiding if this object is being instanced on verts/faces/frames
   * by its parent. Ideally this should not be needed, but due to the wrong
   * dependency direction in the data design there is no way to keep the object
   * visible otherwise. The better solution eventually would be for objects
   * to specify which object they instance, instead of through parenting. */
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

bool deg_objects_dupli_iterator_next(BLI_Iterator *iter)
{
  DEGObjectIterData *data = (DEGObjectIterData *)iter->data;
  while (data->dupli_object_next != NULL) {
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
    temp_dupli_object->dt = MIN2(temp_dupli_object->dt, dupli_parent->dt);
    copy_v4_v4(temp_dupli_object->color, dupli_parent->color);

    /* Duplicated elements shouldn't care whether their original collection is visible or not. */
    temp_dupli_object->base_flag |= BASE_VISIBLE;

    int ob_visibility = BKE_object_visibility(temp_dupli_object, data->eval_mode);
    if (ob_visibility == 0) {
      continue;
    }

    copy_m4_m4(data->temp_dupli_object.obmat, dob->mat);
    iter->current = &data->temp_dupli_object;
    BLI_assert(DEG::deg_validate_copy_on_write_datablock(&data->temp_dupli_object.id));
    return true;
  }

  return false;
}

void deg_iterator_objects_step(BLI_Iterator *iter, DEG::IDNode *id_node)
{
  /* Set it early in case we need to exit and we are running from within a loop. */
  iter->skip = true;

  if (!id_node->is_directly_visible) {
    return;
  }

  DEGObjectIterData *data = (DEGObjectIterData *)iter->data;
  const ID_Type id_type = GS(id_node->id_orig->name);

  if (id_type != ID_OB) {
    return;
  }

  switch (id_node->linked_state) {
    case DEG::DEG_ID_LINKED_DIRECTLY:
      if ((data->flag & DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY) == 0) {
        return;
      }
      break;
    case DEG::DEG_ID_LINKED_VIA_SET:
      if ((data->flag & DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET) == 0) {
        return;
      }
      break;
    case DEG::DEG_ID_LINKED_INDIRECTLY:
      if ((data->flag & DEG_ITER_OBJECT_FLAG_LINKED_INDIRECTLY) == 0) {
        return;
      }
      break;
  }

  Object *object = (Object *)id_node->id_cow;
  BLI_assert(DEG::deg_validate_copy_on_write_datablock(&object->id));

  int ob_visibility = OB_VISIBLE_ALL;
  if (data->flag & DEG_ITER_OBJECT_FLAG_VISIBLE) {
    ob_visibility = BKE_object_visibility(object, data->eval_mode);

    if (deg_object_hide_original(data->eval_mode, object, NULL)) {
      return;
    }
  }

  if (ob_visibility & OB_VISIBLE_INSTANCES) {
    if ((data->flag & DEG_ITER_OBJECT_FLAG_DUPLI) && (object->transflag & OB_DUPLI)) {
      data->dupli_parent = object;
      data->dupli_list = object_duplilist(data->graph, data->scene, object);
      data->dupli_object_next = (DupliObject *)data->dupli_list->first;
    }
  }

  if (ob_visibility & (OB_VISIBLE_SELF | OB_VISIBLE_PARTICLES)) {
    iter->current = object;
    iter->skip = false;
  }
}

}  // namespace

void DEG_iterator_objects_begin(BLI_Iterator *iter, DEGObjectIterData *data)
{
  Depsgraph *depsgraph = data->graph;
  DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(depsgraph);
  const size_t num_id_nodes = deg_graph->id_nodes.size();

  iter->data = data;

  if (num_id_nodes == 0) {
    iter->valid = false;
    return;
  }

  data->dupli_parent = NULL;
  data->dupli_list = NULL;
  data->dupli_object_next = NULL;
  data->dupli_object_current = NULL;
  data->scene = DEG_get_evaluated_scene(depsgraph);
  data->id_node_index = 0;
  data->num_id_nodes = num_id_nodes;
  data->eval_mode = DEG_get_mode(depsgraph);
  deg_invalidate_iterator_work_data(data);

  DEG::IDNode *id_node = deg_graph->id_nodes[data->id_node_index];
  deg_iterator_objects_step(iter, id_node);

  if (iter->skip) {
    DEG_iterator_objects_next(iter);
  }
}

void DEG_iterator_objects_next(BLI_Iterator *iter)
{
  DEGObjectIterData *data = (DEGObjectIterData *)iter->data;
  Depsgraph *depsgraph = data->graph;
  DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(depsgraph);
  do {
    iter->skip = false;
    if (data->dupli_list) {
      if (deg_objects_dupli_iterator_next(iter)) {
        return;
      }
      else {
        verify_id_properties_freed(data);
        free_object_duplilist(data->dupli_list);
        data->dupli_parent = NULL;
        data->dupli_list = NULL;
        data->dupli_object_next = NULL;
        data->dupli_object_current = NULL;
        deg_invalidate_iterator_work_data(data);
      }
    }

    ++data->id_node_index;
    if (data->id_node_index == data->num_id_nodes) {
      iter->valid = false;
      return;
    }

    DEG::IDNode *id_node = deg_graph->id_nodes[data->id_node_index];
    deg_iterator_objects_step(iter, id_node);
  } while (iter->skip);
}

void DEG_iterator_objects_end(BLI_Iterator *iter)
{
  DEGObjectIterData *data = (DEGObjectIterData *)iter->data;
  if (data != NULL) {
    /* Force crash in case the iterator data is referenced and accessed down
     * the line. (T51718) */
    deg_invalidate_iterator_work_data(data);
  }
}

/* ************************ DEG ID ITERATOR ********************* */

static void DEG_iterator_ids_step(BLI_Iterator *iter, DEG::IDNode *id_node, bool only_updated)
{
  ID *id_cow = id_node->id_cow;

  if (!id_node->is_directly_visible) {
    iter->skip = true;
    return;
  }
  else if (only_updated && !(id_cow->recalc & ID_RECALC_ALL)) {
    bNodeTree *ntree = ntreeFromID(id_cow);

    /* Nodetree is considered part of the datablock. */
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
  DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(depsgraph);
  const size_t num_id_nodes = deg_graph->id_nodes.size();

  iter->data = data;

  if ((num_id_nodes == 0) || (data->only_updated && !DEG_id_type_any_updated(depsgraph))) {
    iter->valid = false;
    return;
  }

  data->id_node_index = 0;
  data->num_id_nodes = num_id_nodes;

  DEG::IDNode *id_node = deg_graph->id_nodes[data->id_node_index];
  DEG_iterator_ids_step(iter, id_node, data->only_updated);

  if (iter->skip) {
    DEG_iterator_ids_next(iter);
  }
}

void DEG_iterator_ids_next(BLI_Iterator *iter)
{
  DEGIDIterData *data = (DEGIDIterData *)iter->data;
  Depsgraph *depsgraph = data->graph;
  DEG::Depsgraph *deg_graph = reinterpret_cast<DEG::Depsgraph *>(depsgraph);

  do {
    iter->skip = false;

    ++data->id_node_index;
    if (data->id_node_index == data->num_id_nodes) {
      iter->valid = false;
      return;
    }

    DEG::IDNode *id_node = deg_graph->id_nodes[data->id_node_index];
    DEG_iterator_ids_step(iter, id_node, data->only_updated);
  } while (iter->skip);
}

void DEG_iterator_ids_end(BLI_Iterator *UNUSED(iter))
{
}
