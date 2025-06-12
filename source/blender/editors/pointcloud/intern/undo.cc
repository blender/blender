/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edpointcloud
 */

#include "BLI_task.hh"

#include "BKE_attribute_storage.hh"
#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"
#include "BKE_pointcloud.hh"
#include "BKE_undo_system.hh"

#include "CLG_log.h"

#include "DEG_depsgraph.hh"

#include "ED_pointcloud.hh"
#include "ED_undo.hh"

#include "WM_api.hh"
#include "WM_types.hh"

static CLG_LogRef LOG = {"undo.pointcloud"};

namespace blender::ed::pointcloud {
namespace undo {

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 *
 * \note This is similar for all edit-mode types.
 * \{ */

struct StepObject {
  UndoRefID_Object obedit_ref = {};
  bke::AttributeStorage attribute_storage;
  int totpoint = 0;
  /* Store the bounds caches because they are small. */
  SharedCache<Bounds<float3>> bounds_cache;
  SharedCache<Bounds<float3>> bounds_with_radius_cache;
};

struct PointCloudUndoStep {
  UndoStep step;
  /** See #ED_undo_object_editmode_validate_scene_from_windows code comment for details. */
  UndoRefID_Scene scene_ref = {};
  Array<StepObject> objects;
};

static bool step_encode(bContext *C, Main *bmain, UndoStep *us_p)
{
  PointCloudUndoStep *us = reinterpret_cast<PointCloudUndoStep *>(us_p);

  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = ED_undo_editmode_objects_from_view_layer(scene, view_layer);

  us->scene_ref.ptr = scene;
  new (&us->objects) Array<StepObject>(objects.size());

  threading::parallel_for(us->objects.index_range(), 8, [&](const IndexRange range) {
    for (const int i : range) {
      Object *ob = objects[i];
      StepObject &object = us->objects[i];
      const PointCloud &pointcloud = *static_cast<const PointCloud *>(ob->data);
      object.obedit_ref.ptr = ob;
      object.attribute_storage.wrap() = pointcloud.attribute_storage.wrap();
      object.bounds_cache = pointcloud.runtime->bounds_cache;
      object.bounds_with_radius_cache = pointcloud.runtime->bounds_with_radius_cache;
      object.totpoint = pointcloud.totpoint;
    }
  });

  bmain->is_memfile_undo_flush_needed = true;

  return true;
}

static void step_decode(
    bContext *C, Main *bmain, UndoStep *us_p, const eUndoStepDir /*dir*/, bool /*is_final*/)
{
  PointCloudUndoStep *us = reinterpret_cast<PointCloudUndoStep *>(us_p);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  ED_undo_object_editmode_validate_scene_from_windows(
      CTX_wm_manager(C), us->scene_ref.ptr, &scene, &view_layer);
  ED_undo_object_editmode_restore_helper(scene,
                                         view_layer,
                                         &us->objects.first().obedit_ref.ptr,
                                         us->objects.size(),
                                         sizeof(decltype(us->objects)::value_type));

  BLI_assert(BKE_object_is_in_editmode(us->objects.first().obedit_ref.ptr));

  for (const StepObject &object : us->objects) {
    PointCloud &pointcloud = *static_cast<PointCloud *>(object.obedit_ref.ptr->data);

    const bool positions_changed = [&]() {
      const bke::Attribute *attr_a = pointcloud.attribute_storage.wrap().lookup("position");
      const bke::Attribute *attr_b = object.attribute_storage.wrap().lookup("position");
      if (!attr_b && !attr_a) {
        return false;
      }
      if (!attr_a || !attr_b) {
        return true;
      }
      return std::get<bke::Attribute::ArrayData>(attr_a->data()).data !=
             std::get<bke::Attribute::ArrayData>(attr_b->data()).data;
    }();

    pointcloud.attribute_storage.wrap() = object.attribute_storage.wrap();

    pointcloud.totpoint = object.totpoint;
    pointcloud.runtime->bounds_cache = object.bounds_cache;
    pointcloud.runtime->bounds_with_radius_cache = object.bounds_with_radius_cache;
    if (positions_changed) {
      pointcloud.runtime->bvh_cache.tag_dirty();
    }
    DEG_id_tag_update(&pointcloud.id, ID_RECALC_GEOMETRY);
  }

  ED_undo_object_set_active_or_warn(
      scene, view_layer, us->objects.first().obedit_ref.ptr, us_p->name, &LOG);

  bmain->is_memfile_undo_flush_needed = true;

  WM_event_add_notifier(C, NC_GEOM | ND_DATA, nullptr);
}

static void step_free(UndoStep *us_p)
{
  PointCloudUndoStep *us = reinterpret_cast<PointCloudUndoStep *>(us_p);
  us->objects.~Array();
}

static void foreach_ID_ref(UndoStep *us_p,
                           UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                           void *user_data)
{
  PointCloudUndoStep *us = reinterpret_cast<PointCloudUndoStep *>(us_p);

  foreach_ID_ref_fn(user_data, ((UndoRefID *)&us->scene_ref));
  for (const StepObject &object : us->objects) {
    foreach_ID_ref_fn(user_data, ((UndoRefID *)&object.obedit_ref));
  }
}

/** \} */

}  // namespace undo

void undosys_type_register(UndoType *ut)
{
  ut->name = "Edit Point Cloud";
  ut->poll = editable_pointcloud_in_edit_mode_poll;
  ut->step_encode = undo::step_encode;
  ut->step_decode = undo::step_decode;
  ut->step_free = undo::step_free;

  ut->step_foreach_ID_ref = undo::foreach_ID_ref;

  ut->flags = UNDOTYPE_FLAG_NEED_CONTEXT_FOR_ENCODE;

  ut->step_size = sizeof(undo::PointCloudUndoStep);
}

}  // namespace blender::ed::pointcloud
