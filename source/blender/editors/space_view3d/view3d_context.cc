/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_screen.hh"

#include "ED_view3d.hh"

#include "view3d_intern.hh"

/* -------------------------------------------------------------------- */
/** \name View3D Context Callback
 * \{ */

const char *view3d_context_dir[] = {
    "active_object",
    "selected_ids",
    nullptr,
};

int view3d_context(const bContext *C, const char *member, bContextDataResult *result)
{
  /* fall back to the scene layer,
   * allows duplicate and other object operators to run outside the 3d view */

  if (CTX_data_dir(member)) {
    CTX_data_dir_set(result, view3d_context_dir);
    return CTX_RESULT_OK;
  }
  if (CTX_data_equals(member, "active_object")) {
    /* In most cases the active object is the `view_layer->basact->object`.
     * For the 3D view however it can be nullptr when hidden.
     *
     * This is ignored in the case the object is in any mode (besides object-mode),
     * since the object's mode impacts the current tool, cursor, gizmos etc.
     * If we didn't have this exception, changing visibility would need to perform
     * many of the same updates as changing the objects mode.
     *
     * Further, there are multiple ways to hide objects - by collection, by object type, etc.
     * it's simplest if all these methods behave consistently - respecting the object-mode
     * without showing the object.
     *
     * See #85532 for alternatives that were considered. */
    const Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    BKE_view_layer_synced_ensure(scene, view_layer);
    Base *base = BKE_view_layer_active_base_get(view_layer);
    if (base) {
      Object *ob = base->object;
      /* if hidden but in edit mode, we still display, can happen with animation */
      if ((base->flag & BASE_ENABLED_AND_MAYBE_VISIBLE_IN_VIEWPORT) != 0 ||
          (ob->mode != OB_MODE_OBJECT))
      {
        CTX_data_id_pointer_set(result, &ob->id);
      }
    }

    return CTX_RESULT_OK;
  }
  if (CTX_data_equals(member, "selected_ids")) {
    blender::Vector<PointerRNA> selected_objects;
    CTX_data_selected_objects(C, &selected_objects);
    for (const PointerRNA &ptr : selected_objects) {
      ID *selected_id = ptr.owner_id;
      CTX_data_id_list_add(result, selected_id);
    }
    CTX_data_type_set(result, ContextDataType::Collection);
    return CTX_RESULT_OK;
  }

  return CTX_RESULT_MEMBER_NOT_FOUND;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View3D Context Queries
 * \{ */

RegionView3D *ED_view3d_context_rv3d(bContext *C)
{
  RegionView3D *rv3d = CTX_wm_region_view3d(C);

  if (rv3d == nullptr) {
    ScrArea *area = CTX_wm_area(C);
    if (area && area->spacetype == SPACE_VIEW3D) {
      ARegion *region = BKE_area_find_region_active_win(area);
      if (region) {
        rv3d = static_cast<RegionView3D *>(region->regiondata);
      }
    }
  }
  return rv3d;
}

bool ED_view3d_context_user_region(bContext *C, View3D **r_v3d, ARegion **r_region)
{
  ScrArea *area = CTX_wm_area(C);

  *r_v3d = nullptr;
  *r_region = nullptr;

  if (area && area->spacetype == SPACE_VIEW3D) {
    ARegion *region = CTX_wm_region(C);
    View3D *v3d = (View3D *)area->spacedata.first;

    if (region) {
      RegionView3D *rv3d;
      if ((region->regiontype == RGN_TYPE_WINDOW) &&
          (rv3d = static_cast<RegionView3D *>(region->regiondata)) &&
          (rv3d->viewlock & RV3D_LOCK_ROTATION) == 0)
      {
        *r_v3d = v3d;
        *r_region = region;
        return true;
      }

      if (ED_view3d_area_user_region(area, v3d, r_region)) {
        *r_v3d = v3d;
        return true;
      }
    }
  }

  return false;
}

/** \} */
