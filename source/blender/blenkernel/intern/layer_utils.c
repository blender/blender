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
 */

/** \file
 * \ingroup bke
 */

#include <string.h>

#include "BLI_array.h"

#include "BKE_collection.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"

#include "DNA_ID.h"
#include "DNA_layer_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

/* -------------------------------------------------------------------- */
/** \name Selected Object Array
 * \{ */

Object **BKE_view_layer_array_selected_objects_params(
    struct ViewLayer *view_layer,
    const struct View3D *v3d,
    uint *r_len,
    const struct ObjectsInViewLayerParams *params)
{
  if (params->no_dup_data) {
    FOREACH_SELECTED_OBJECT_BEGIN (view_layer, v3d, ob_iter) {
      ID *id = ob_iter->data;
      if (id) {
        id->tag |= LIB_TAG_DOIT;
      }
    }
    FOREACH_SELECTED_OBJECT_END;
  }

  Object **object_array = NULL;
  BLI_array_declare(object_array);

  FOREACH_SELECTED_OBJECT_BEGIN (view_layer, v3d, ob_iter) {
    if (params->filter_fn) {
      if (!params->filter_fn(ob_iter, params->filter_userdata)) {
        continue;
      }
    }

    if (params->no_dup_data) {
      ID *id = ob_iter->data;
      if (id) {
        if (id->tag & LIB_TAG_DOIT) {
          id->tag &= ~LIB_TAG_DOIT;
        }
        else {
          continue;
        }
      }
    }

    BLI_array_append(object_array, ob_iter);
  }
  FOREACH_SELECTED_OBJECT_END;

  object_array = MEM_reallocN(object_array, sizeof(*object_array) * BLI_array_len(object_array));
  /* We always need a valid allocation (prevent crash on free). */
  if (object_array == NULL) {
    object_array = MEM_mallocN(0, __func__);
  }
  *r_len = BLI_array_len(object_array);
  return object_array;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Objects in Mode Array
 * \{ */

Base **BKE_view_layer_array_from_bases_in_mode_params(ViewLayer *view_layer,
                                                      const View3D *v3d,
                                                      uint *r_len,
                                                      const struct ObjectsInModeParams *params)
{
  if (params->no_dup_data) {
    FOREACH_BASE_IN_MODE_BEGIN (view_layer, v3d, -1, params->object_mode, base_iter) {
      ID *id = base_iter->object->data;
      if (id) {
        id->tag |= LIB_TAG_DOIT;
      }
    }
    FOREACH_BASE_IN_MODE_END;
  }

  Base **base_array = NULL;
  BLI_array_declare(base_array);

  FOREACH_BASE_IN_MODE_BEGIN (view_layer, v3d, -1, params->object_mode, base_iter) {
    if (params->filter_fn) {
      if (!params->filter_fn(base_iter->object, params->filter_userdata)) {
        continue;
      }
    }
    if (params->no_dup_data) {
      ID *id = base_iter->object->data;
      if (id) {
        if (id->tag & LIB_TAG_DOIT) {
          id->tag &= ~LIB_TAG_DOIT;
        }
        else {
          continue;
        }
      }
    }
    BLI_array_append(base_array, base_iter);
  }
  FOREACH_BASE_IN_MODE_END;

  base_array = MEM_reallocN(base_array, sizeof(*base_array) * BLI_array_len(base_array));
  /* We always need a valid allocation (prevent crash on free). */
  if (base_array == NULL) {
    base_array = MEM_mallocN(0, __func__);
  }
  *r_len = BLI_array_len(base_array);
  return base_array;
}

Object **BKE_view_layer_array_from_objects_in_mode_params(ViewLayer *view_layer,
                                                          const View3D *v3d,
                                                          uint *r_len,
                                                          const struct ObjectsInModeParams *params)
{
  Base **base_array = BKE_view_layer_array_from_bases_in_mode_params(
      view_layer, v3d, r_len, params);
  if (base_array != NULL) {
    for (uint i = 0; i < *r_len; i++) {
      ((Object **)base_array)[i] = base_array[i]->object;
    }
  }
  return (Object **)base_array;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Filter Functions
 * \{ */

bool BKE_view_layer_filter_edit_mesh_has_uvs(Object *ob, void *UNUSED(user_data))
{
  if (ob->type == OB_MESH) {
    Mesh *me = ob->data;
    BMEditMesh *em = me->edit_mesh;
    if (em != NULL) {
      if (CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV) != -1) {
        return true;
      }
    }
  }
  return false;
}

bool BKE_view_layer_filter_edit_mesh_has_edges(Object *ob, void *UNUSED(user_data))
{
  if (ob->type == OB_MESH) {
    Mesh *me = ob->data;
    BMEditMesh *em = me->edit_mesh;
    if (em != NULL) {
      if (em->bm->totedge != 0) {
        return true;
      }
    }
  }
  return false;
}

/**
 * Use this in rare cases we need to detect a pair of objects (active, selected).
 * This returns the other non-active selected object.
 *
 * Returns NULL with it finds multiple other selected objects
 * as behavior in this case would be random from the user perspective.
 */
Object *BKE_view_layer_non_active_selected_object(struct ViewLayer *view_layer,
                                                  const struct View3D *v3d)
{
  Object *ob_active = OBACT(view_layer);
  Object *ob_result = NULL;
  FOREACH_SELECTED_OBJECT_BEGIN (view_layer, v3d, ob_iter) {
    if (ob_iter == ob_active) {
      continue;
    }

    if (ob_result == NULL) {
      ob_result = ob_iter;
    }
    else {
      ob_result = NULL;
      break;
    }
  }
  FOREACH_SELECTED_OBJECT_END;
  return ob_result;
}
/** \} */
