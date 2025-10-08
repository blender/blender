/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstring>

#include "BKE_collection.hh"
#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_layer.hh"
#include "BKE_mesh_types.hh"

#include "DNA_ID.h"
#include "DNA_layer_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

/* -------------------------------------------------------------------- */
/** \name Selected Object Array
 * \{ */

using blender::Vector;

Vector<Object *> BKE_view_layer_array_selected_objects_params(
    ViewLayer *view_layer, const View3D *v3d, const ObjectsInViewLayerParams *params)
{
  if (params->no_dup_data) {
    FOREACH_SELECTED_OBJECT_BEGIN (view_layer, v3d, ob_iter) {
      ID *id = static_cast<ID *>(ob_iter->data);
      if (id) {
        id->tag |= ID_TAG_DOIT;
      }
    }
    FOREACH_SELECTED_OBJECT_END;
  }

  Vector<Object *> objects;

  FOREACH_SELECTED_OBJECT_BEGIN (view_layer, v3d, ob_iter) {
    if (params->filter_fn) {
      if (!params->filter_fn(ob_iter, params->filter_userdata)) {
        continue;
      }
    }

    if (params->no_dup_data) {
      ID *id = static_cast<ID *>(ob_iter->data);
      if (id) {
        if (id->tag & ID_TAG_DOIT) {
          id->tag &= ~ID_TAG_DOIT;
        }
        else {
          continue;
        }
      }
    }

    objects.append(ob_iter);
  }
  FOREACH_SELECTED_OBJECT_END;

  return objects;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Objects in Mode Array
 * \{ */

Vector<Base *> BKE_view_layer_array_from_bases_in_mode_params(const Scene *scene,
                                                              ViewLayer *view_layer,
                                                              const View3D *v3d,
                                                              const ObjectsInModeParams *params)
{
  if (params->no_dup_data) {
    FOREACH_BASE_IN_MODE_BEGIN (scene, view_layer, v3d, -1, params->object_mode, base_iter) {
      ID *id = static_cast<ID *>(base_iter->object->data);
      if (id) {
        id->tag |= ID_TAG_DOIT;
      }
    }
    FOREACH_BASE_IN_MODE_END;
  }

  Vector<Base *> bases;

  FOREACH_BASE_IN_MODE_BEGIN (scene, view_layer, v3d, -1, params->object_mode, base_iter) {
    if (params->filter_fn) {
      if (!params->filter_fn(base_iter->object, params->filter_userdata)) {
        continue;
      }
    }
    if (params->no_dup_data) {
      ID *id = static_cast<ID *>(base_iter->object->data);
      if (id) {
        if (id->tag & ID_TAG_DOIT) {
          id->tag &= ~ID_TAG_DOIT;
        }
        else {
          continue;
        }
      }
    }
    bases.append(base_iter);
  }
  FOREACH_BASE_IN_MODE_END;

  return bases;
}

Vector<Object *> BKE_view_layer_array_from_objects_in_mode_params(
    const Scene *scene,
    ViewLayer *view_layer,
    const View3D *v3d,
    const ObjectsInModeParams *params)
{
  const Vector<Base *> bases = BKE_view_layer_array_from_bases_in_mode_params(
      scene, view_layer, v3d, params);
  Vector<Object *> objects(bases.size());
  std::transform(
      bases.begin(), bases.end(), objects.begin(), [](Base *base) { return base->object; });
  return objects;
}

Vector<Object *> BKE_view_layer_array_from_objects_in_edit_mode(const Scene *scene,
                                                                ViewLayer *view_layer,
                                                                const View3D *v3d)
{
  ObjectsInModeParams params = {0};
  params.object_mode = OB_MODE_EDIT;
  return BKE_view_layer_array_from_objects_in_mode_params(scene, view_layer, v3d, &params);
}

Vector<Base *> BKE_view_layer_array_from_bases_in_edit_mode(const Scene *scene,
                                                            ViewLayer *view_layer,
                                                            const View3D *v3d)
{
  ObjectsInModeParams params = {0};
  params.object_mode = OB_MODE_EDIT;
  return BKE_view_layer_array_from_bases_in_mode_params(scene, view_layer, v3d, &params);
}

Vector<Object *> BKE_view_layer_array_from_objects_in_edit_mode_unique_data(const Scene *scene,
                                                                            ViewLayer *view_layer,
                                                                            const View3D *v3d)
{
  ObjectsInModeParams params = {0};
  params.object_mode = OB_MODE_EDIT;
  params.no_dup_data = true;
  return BKE_view_layer_array_from_objects_in_mode_params(scene, view_layer, v3d, &params);
}

Vector<Base *> BKE_view_layer_array_from_bases_in_edit_mode_unique_data(const Scene *scene,
                                                                        ViewLayer *view_layer,
                                                                        const View3D *v3d)
{
  ObjectsInModeParams params = {0};
  params.object_mode = OB_MODE_EDIT;
  params.no_dup_data = true;
  return BKE_view_layer_array_from_bases_in_mode_params(scene, view_layer, v3d, &params);
}

Vector<Object *> BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
    const Scene *scene, ViewLayer *view_layer, const View3D *v3d)
{
  ObjectsInModeParams params = {0};
  params.object_mode = OB_MODE_EDIT;
  params.no_dup_data = true;
  params.filter_fn = BKE_view_layer_filter_edit_mesh_has_uvs;
  return BKE_view_layer_array_from_objects_in_mode_params(scene, view_layer, v3d, &params);
}

Vector<Object *> BKE_view_layer_array_from_objects_in_mode_unique_data(const Scene *scene,
                                                                       ViewLayer *view_layer,
                                                                       const View3D *v3d,
                                                                       const eObjectMode mode)
{
  ObjectsInModeParams params = {0};
  params.object_mode = mode;
  params.no_dup_data = true;
  return BKE_view_layer_array_from_objects_in_mode_params(scene, view_layer, v3d, &params);
}

ListBase *BKE_view_layer_object_bases_unsynced_get(ViewLayer *view_layer)
{
  return &view_layer->object_bases;
}

ListBase *BKE_view_layer_object_bases_get(ViewLayer *view_layer)
{
  BLI_assert_msg((view_layer->flag & VIEW_LAYER_OUT_OF_SYNC) == 0,
                 "Object Bases out of sync, invoke BKE_view_layer_synced_ensure.");
  return BKE_view_layer_object_bases_unsynced_get(view_layer);
}

Base *BKE_view_layer_active_base_get(ViewLayer *view_layer)
{
  BLI_assert_msg((view_layer->flag & VIEW_LAYER_OUT_OF_SYNC) == 0,
                 "Active Base out of sync, invoke BKE_view_layer_synced_ensure.");
  return view_layer->basact;
}

LayerCollection *BKE_view_layer_active_collection_get(ViewLayer *view_layer)
{
  BLI_assert_msg((view_layer->flag & VIEW_LAYER_OUT_OF_SYNC) == 0,
                 "Active Collection out of sync, invoke BKE_view_layer_synced_ensure.");
  return view_layer->active_collection;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Filter Functions
 * \{ */

bool BKE_view_layer_filter_edit_mesh_has_uvs(const Object *ob, void * /*user_data*/)
{
  if (ob->type == OB_MESH) {
    const Mesh *mesh = static_cast<const Mesh *>(ob->data);
    if (const BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
      if (CustomData_has_layer(&em->bm->ldata, CD_PROP_FLOAT2)) {
        return true;
      }
    }
  }
  return false;
}

bool BKE_view_layer_filter_edit_mesh_has_edges(const Object *ob, void * /*user_data*/)
{
  if (ob->type == OB_MESH) {
    const Mesh *mesh = static_cast<const Mesh *>(ob->data);
    if (const BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
      if (em->bm->totedge != 0) {
        return true;
      }
    }
  }
  return false;
}

Object *BKE_view_layer_non_active_selected_object(const Scene *scene,
                                                  ViewLayer *view_layer,
                                                  const View3D *v3d)
{
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob_active = BKE_view_layer_active_object_get(view_layer);
  Object *ob_result = nullptr;
  FOREACH_SELECTED_OBJECT_BEGIN (view_layer, v3d, ob_iter) {
    if (ob_iter == ob_active) {
      continue;
    }

    if (ob_result == nullptr) {
      ob_result = ob_iter;
    }
    else {
      ob_result = nullptr;
      break;
    }
  }
  FOREACH_SELECTED_OBJECT_END;
  return ob_result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Active object accessors.
 * \{ */

Object *BKE_view_layer_active_object_get(const ViewLayer *view_layer)
{
  Base *base = BKE_view_layer_active_base_get((ViewLayer *)view_layer);
  return base ? base->object : nullptr;
}

Object *BKE_view_layer_edit_object_get(const ViewLayer *view_layer)
{
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  if (ob == nullptr) {
    return nullptr;
  }
  if (!(ob->mode & OB_MODE_EDIT)) {
    return nullptr;
  }
  return ob;
}

/** \} */
