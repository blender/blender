/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Function in this file copy functionality from Blender's context functions.
 *
 * The regular context functions, like #CTX_data_selected_objects(), cannot be used by the anim
 * filter system. This is due to a limitation in Blender's context code (#ctx_data_get() in
 * blenkernel/intern/context.cc), which prevents infinite recursion by stepping through four
 * 'sources' of a context member (context overrides store on the window manager, area, region,
 * screen). Any further call is rejected. This means that the anim filter code cannot call
 * #CTX_data_selected_objects() to get the selected objects, as the filter code itself is also in
 * use to construct screen context members.
 */

#include "anim_filter_source_intern.hh"

#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BLI_listbase.hh"

#include "WM_api.hh"

#include "DNA_scene_types.h" /* For the BASE_SELECTED() macro. */

namespace blender::ed::animfilter {

void foreach_base_in_view_layer(const bContext &C,
                                FunctionRef<void(View3D *v3d, Base &base)> visit_fn)
{
  const Main *bmain = CTX_data_main(&C);
  wmWindow *win = CTX_wm_window(&C);
  View3D *v3d = CTX_wm_view3d(&C); /* Can be nullptr in many cases. */
  Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  BKE_view_layer_synced_ensure(*bmain, scene, view_layer);

  for (Base &base : *BKE_view_layer_object_bases_get(view_layer)) {
    visit_fn(v3d, base);
  }
}

/* This copies #screen_ctx_selected_objects() from screen_context.cc. */
Vector<Object *> fake_ctx_selected_objects(const bContext &C)
{
  Vector<Object *> selected_objects;
  foreach_base_in_view_layer(C, [&](View3D *v3d, Base &base) {
    if (BASE_SELECTED(v3d, &base)) {
      selected_objects.append(base.object);
    }
  });
  return selected_objects;
}

/* This copies #screen_ctx_visible_objects() from screen_context.cc. */
Vector<Object *> fake_ctx_visible_objects(const bContext &C)
{
  Vector<Object *> selected_objects;
  foreach_base_in_view_layer(C, [&](View3D *v3d, Base &base) {
    if (BASE_VISIBLE(v3d, &base)) {
      selected_objects.append(base.object);
    }
  });
  return selected_objects;
}

}  // namespace blender::ed::animfilter
