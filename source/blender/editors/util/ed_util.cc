/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edutil
 */

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "BKE_collection.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_multires.h"
#include "BKE_object.h"
#include "BKE_packedFile.h"
#include "BKE_paint.h"
#include "BKE_screen.h"
#include "BKE_undo_system.h"

#include "DEG_depsgraph.h"

#include "DNA_gpencil_legacy_types.h"

#include "ED_armature.h"
#include "ED_asset.h"
#include "ED_gpencil_legacy.h"
#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_paint.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_util.h"

#include "GPU_immediate.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "WM_api.h"
#include "WM_types.h"

/* ********* general editor util functions, not BKE stuff please! ********* */

void ED_editors_init_for_undo(Main *bmain)
{
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    Scene *scene = WM_window_get_active_scene(win);
    ViewLayer *view_layer = WM_window_get_active_view_layer(win);
    BKE_view_layer_synced_ensure(scene, view_layer);
    Object *ob = BKE_view_layer_active_object_get(view_layer);
    if (ob && (ob->mode & OB_MODE_TEXTURE_PAINT)) {
      BKE_texpaint_slots_refresh_object(scene, ob);
      ED_paint_proj_mesh_data_check(scene, ob, nullptr, nullptr, nullptr, nullptr);
    }
  }
}

void ED_editors_init(bContext *C)
{
  Depsgraph *depsgraph = CTX_data_expect_evaluated_depsgraph(C);
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  wmWindowManager *wm = CTX_wm_manager(C);

  /* This is called during initialization, so we don't want to store any reports */
  ReportList *reports = CTX_wm_reports(C);
  int reports_flag_prev = reports->flag & ~RPT_STORE;

  SWAP(int, reports->flag, reports_flag_prev);

  /* Don't do undo pushes when calling an operator. */
  wm->op_undo_depth++;

  /* toggle on modes for objects that were saved with these enabled. for
   * e.g. linked objects we have to ensure that they are actually the
   * active object in this scene. */
  Object *obact = CTX_data_active_object(C);
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    int mode = ob->mode;
    if (mode == OB_MODE_OBJECT) {
      continue;
    }
    if (BKE_object_has_mode_data(ob, eObjectMode(mode))) {
      /* For multi-edit mode we may already have mode data. */
      continue;
    }
    if (ob->type == OB_GPENCIL_LEGACY) {
      /* Grease pencil does not need a toggle of mode. However we may have a non-active object
       * stuck in a grease-pencil edit mode. */
      if (ob != obact) {
        bGPdata *gpd = (bGPdata *)ob->data;
        gpd->flag &= ~(GP_DATA_STROKE_PAINTMODE | GP_DATA_STROKE_EDITMODE |
                       GP_DATA_STROKE_SCULPTMODE | GP_DATA_STROKE_WEIGHTMODE |
                       GP_DATA_STROKE_VERTEXMODE);
        ob->mode = OB_MODE_OBJECT;
        DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
      }
      else if (mode & OB_MODE_ALL_PAINT_GPENCIL) {
        ED_gpencil_toggle_brush_cursor(C, true, nullptr);
      }
      continue;
    }

    /* Reset object to Object mode, so that code below can properly re-switch it to its
     * previous mode if possible, re-creating its mode data, etc. */
    ID *ob_data = static_cast<ID *>(ob->data);
    ob->mode = OB_MODE_OBJECT;
    DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);

    /* Object mode is enforced if there is no active object, or if the active object's type is
     * different. */
    if (obact == nullptr || ob->type != obact->type) {
      continue;
    }
    /* Object mode is enforced for non-editable data (or their obdata). */
    if (!BKE_id_is_editable(bmain, &ob->id) ||
        (ob_data != nullptr && !BKE_id_is_editable(bmain, ob_data)))
    {
      continue;
    }

    /* Pose mode is very similar to Object one, we can apply it even on objects not in current
     * scene. */
    if (mode == OB_MODE_POSE) {
      ED_object_posemode_enter_ex(bmain, ob);
    }

    /* Other edit/paint/etc. modes are only settable for objects visible in active scene currently.
     * Otherwise, they (and their obdata) may not be (fully) evaluated, which is mandatory for some
     * modes like Sculpt.
     * Ref. #98225. */
    if (!BKE_collection_has_object_recursive(scene->master_collection, ob) ||
        !BKE_scene_has_object(scene, ob) || (ob->visibility_flag & OB_HIDE_VIEWPORT) != 0)
    {
      continue;
    }

    if (mode == OB_MODE_EDIT) {
      ED_object_editmode_enter_ex(bmain, scene, ob, 0);
    }
    else if (mode & OB_MODE_ALL_SCULPT) {
      if (obact == ob) {
        if (mode == OB_MODE_SCULPT) {
          ED_object_sculptmode_enter_ex(bmain, depsgraph, scene, ob, true, reports);
        }
        else if (mode == OB_MODE_VERTEX_PAINT) {
          ED_object_vpaintmode_enter_ex(bmain, depsgraph, scene, ob);
        }
        else if (mode == OB_MODE_WEIGHT_PAINT) {
          ED_object_wpaintmode_enter_ex(bmain, depsgraph, scene, ob);
        }
        else {
          BLI_assert_unreachable();
        }
      }
      else {
        /* Create data for non-active objects which need it for
         * mode-switching but don't yet support multi-editing. */
        if (mode & OB_MODE_ALL_SCULPT) {
          ob->mode = mode;
          BKE_object_sculpt_data_create(ob);
        }
      }
    }
    else {
      /* TODO(@ideasman42): avoid operator calls. */
      if (obact == ob) {
        ED_object_mode_set(C, eObjectMode(mode));
      }
    }
  }

  /* image editor paint mode */
  if (scene) {
    ED_space_image_paint_update(bmain, wm, scene);
  }

  /* Enforce a full redraw for the first time areas/regions get drawn. Further region init/refresh
   * just triggers non-rebuild redraws (#RGN_DRAW_NO_REBUILD). Usually a full redraw would be
   * triggered by a `NC_WM | ND_FILEREAD` notifier, but if a startup script calls an operator that
   * redraws the window, notifiers are not handled before the operator runs. See #98461. */
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    const bScreen *screen = WM_window_get_active_screen(win);

    ED_screen_areas_iter (win, screen, area) {
      ED_area_tag_redraw(area);
    }
  }

  ED_assetlist_storage_tag_main_data_dirty();

  SWAP(int, reports->flag, reports_flag_prev);
  wm->op_undo_depth--;
}

void ED_editors_exit(Main *bmain, bool do_undo_system)
{
  if (!bmain) {
    return;
  }

  /* Frees all edit-mode undo-steps. */
  if (do_undo_system && G_MAIN->wm.first) {
    wmWindowManager *wm = static_cast<wmWindowManager *>(G_MAIN->wm.first);
    /* normally we don't check for null undo stack,
     * do here since it may run in different context. */
    if (wm->undo_stack) {
      BKE_undosys_stack_destroy(wm->undo_stack);
      wm->undo_stack = nullptr;
    }
  }

  /* On undo, tag for update so the depsgraph doesn't use stale edit-mode data,
   * this is possible when mixing edit-mode and memory-file undo.
   *
   * By convention, objects are not left in edit-mode - so this isn't often problem in practice,
   * since exiting edit-mode will tag the objects too.
   *
   * However there is no guarantee the active object _never_ changes while in edit-mode.
   * Python for example can do this, some callers to #ED_object_base_activate
   * don't handle modes either (doing so isn't always practical).
   *
   * To reproduce the problem where stale data is used, see: #84920. */
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    if (ED_object_editmode_free_ex(bmain, ob)) {
      if (do_undo_system == false) {
        DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
      }
    }
  }

  /* global in meshtools... */
  ED_mesh_mirror_spatial_table_end(nullptr);
  ED_mesh_mirror_topo_table_end(nullptr);
}

bool ED_editors_flush_edits_for_object_ex(Main *bmain,
                                          Object *ob,
                                          bool for_render,
                                          bool check_needs_flush)
{
  bool has_edited = false;
  if (ob->mode & OB_MODE_SCULPT) {
    /* Don't allow flushing while in the middle of a stroke (frees data in use).
     * Auto-save prevents this from happening but scripts
     * may cause a flush on saving: #53986. */
    if (ob->sculpt != nullptr && ob->sculpt->cache == nullptr) {
      char *needs_flush_ptr = &ob->sculpt->needs_flush_to_id;
      if (check_needs_flush && (*needs_flush_ptr == 0)) {
        return false;
      }
      *needs_flush_ptr = 0;

      /* flush multires changes (for sculpt) */
      multires_flush_sculpt_updates(ob);
      has_edited = true;

      if (for_render) {
        /* flush changes from dynamic topology sculpt */
        BKE_sculptsession_bm_to_me_for_render(ob);
      }
      else {
        /* Set reorder=false so that saving the file doesn't reorder
         * the BMesh's elements */
        BKE_sculptsession_bm_to_me(ob, false);
      }
    }
  }
  else if (ob->mode & OB_MODE_EDIT) {

    char *needs_flush_ptr = BKE_object_data_editmode_flush_ptr_get(static_cast<ID *>(ob->data));
    if (needs_flush_ptr != nullptr) {
      if (check_needs_flush && (*needs_flush_ptr == 0)) {
        return false;
      }
      *needs_flush_ptr = 0;
    }

    /* get editmode results */
    has_edited = true;
    ED_object_editmode_load(bmain, ob);
  }
  return has_edited;
}

bool ED_editors_flush_edits_for_object(Main *bmain, Object *ob)
{
  return ED_editors_flush_edits_for_object_ex(bmain, ob, false, false);
}

bool ED_editors_flush_edits_ex(Main *bmain, bool for_render, bool check_needs_flush)
{
  bool has_edited = false;

  /* loop through all data to find edit mode or object mode, because during
   * exiting we might not have a context for edit object and multiple sculpt
   * objects can exist at the same time */
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    has_edited |= ED_editors_flush_edits_for_object_ex(bmain, ob, for_render, check_needs_flush);
  }

  bmain->is_memfile_undo_flush_needed = false;

  return has_edited;
}

bool ED_editors_flush_edits(Main *bmain)
{
  return ED_editors_flush_edits_ex(bmain, false, false);
}

/* ***** XXX: functions are using old blender names, cleanup later ***** */

void apply_keyb_grid(
    bool shift, bool ctrl, float *val, float fac1, float fac2, float fac3, int invert)
{
  /* fac1 is for 'nothing', fac2 for CTRL, fac3 for SHIFT */
  if (invert) {
    ctrl = !ctrl;
  }

  if (ctrl && shift) {
    if (fac3 != 0.0f) {
      *val = fac3 * floorf(*val / fac3 + 0.5f);
    }
  }
  else if (ctrl) {
    if (fac2 != 0.0f) {
      *val = fac2 * floorf(*val / fac2 + 0.5f);
    }
  }
  else {
    if (fac1 != 0.0f) {
      *val = fac1 * floorf(*val / fac1 + 0.5f);
    }
  }
}

void unpack_menu(bContext *C,
                 const char *opname,
                 const char *id_name,
                 const char *abs_name,
                 const char *folder,
                 PackedFile *pf)
{
  Main *bmain = CTX_data_main(C);
  PointerRNA props_ptr;
  uiPopupMenu *pup;
  uiLayout *layout;
  char line[FILE_MAX + 100];
  wmOperatorType *ot = WM_operatortype_find(opname, true);
  const char *blendfile_path = BKE_main_blendfile_path(bmain);

  pup = UI_popup_menu_begin(C, IFACE_("Unpack File"), ICON_NONE);
  layout = UI_popup_menu_layout(pup);

  uiItemFullO_ptr(
      layout, ot, IFACE_("Remove Pack"), ICON_NONE, nullptr, WM_OP_EXEC_DEFAULT, 0, &props_ptr);
  RNA_enum_set(&props_ptr, "method", PF_REMOVE);
  RNA_string_set(&props_ptr, "id", id_name);

  if (blendfile_path[0] != '\0') {
    char local_name[FILE_MAXDIR + FILE_MAX], fi[FILE_MAX];

    BLI_path_split_file_part(abs_name, fi, sizeof(fi));
    BLI_path_join(local_name, sizeof(local_name), "//", folder, fi);
    if (!STREQ(abs_name, local_name)) {
      switch (BKE_packedfile_compare_to_file(blendfile_path, local_name, pf)) {
        case PF_CMP_NOFILE:
          SNPRINTF(line, TIP_("Create %s"), local_name);
          uiItemFullO_ptr(layout, ot, line, ICON_NONE, nullptr, WM_OP_EXEC_DEFAULT, 0, &props_ptr);
          RNA_enum_set(&props_ptr, "method", PF_WRITE_LOCAL);
          RNA_string_set(&props_ptr, "id", id_name);

          break;
        case PF_CMP_EQUAL:
          SNPRINTF(line, TIP_("Use %s (identical)"), local_name);
          // uiItemEnumO_ptr(layout, ot, line, 0, "method", PF_USE_LOCAL);
          uiItemFullO_ptr(layout, ot, line, ICON_NONE, nullptr, WM_OP_EXEC_DEFAULT, 0, &props_ptr);
          RNA_enum_set(&props_ptr, "method", PF_USE_LOCAL);
          RNA_string_set(&props_ptr, "id", id_name);

          break;
        case PF_CMP_DIFFERS:
          SNPRINTF(line, TIP_("Use %s (differs)"), local_name);
          // uiItemEnumO_ptr(layout, ot, line, 0, "method", PF_USE_LOCAL);
          uiItemFullO_ptr(layout, ot, line, ICON_NONE, nullptr, WM_OP_EXEC_DEFAULT, 0, &props_ptr);
          RNA_enum_set(&props_ptr, "method", PF_USE_LOCAL);
          RNA_string_set(&props_ptr, "id", id_name);

          SNPRINTF(line, TIP_("Overwrite %s"), local_name);
          // uiItemEnumO_ptr(layout, ot, line, 0, "method", PF_WRITE_LOCAL);
          uiItemFullO_ptr(layout, ot, line, ICON_NONE, nullptr, WM_OP_EXEC_DEFAULT, 0, &props_ptr);
          RNA_enum_set(&props_ptr, "method", PF_WRITE_LOCAL);
          RNA_string_set(&props_ptr, "id", id_name);
          break;
      }
    }
  }

  switch (BKE_packedfile_compare_to_file(blendfile_path, abs_name, pf)) {
    case PF_CMP_NOFILE:
      SNPRINTF(line, TIP_("Create %s"), abs_name);
      // uiItemEnumO_ptr(layout, ot, line, 0, "method", PF_WRITE_ORIGINAL);
      uiItemFullO_ptr(layout, ot, line, ICON_NONE, nullptr, WM_OP_EXEC_DEFAULT, 0, &props_ptr);
      RNA_enum_set(&props_ptr, "method", PF_WRITE_ORIGINAL);
      RNA_string_set(&props_ptr, "id", id_name);
      break;
    case PF_CMP_EQUAL:
      SNPRINTF(line, TIP_("Use %s (identical)"), abs_name);
      // uiItemEnumO_ptr(layout, ot, line, 0, "method", PF_USE_ORIGINAL);
      uiItemFullO_ptr(layout, ot, line, ICON_NONE, nullptr, WM_OP_EXEC_DEFAULT, 0, &props_ptr);
      RNA_enum_set(&props_ptr, "method", PF_USE_ORIGINAL);
      RNA_string_set(&props_ptr, "id", id_name);
      break;
    case PF_CMP_DIFFERS:
      SNPRINTF(line, TIP_("Use %s (differs)"), abs_name);
      // uiItemEnumO_ptr(layout, ot, line, 0, "method", PF_USE_ORIGINAL);
      uiItemFullO_ptr(layout, ot, line, ICON_NONE, nullptr, WM_OP_EXEC_DEFAULT, 0, &props_ptr);
      RNA_enum_set(&props_ptr, "method", PF_USE_ORIGINAL);
      RNA_string_set(&props_ptr, "id", id_name);

      SNPRINTF(line, TIP_("Overwrite %s"), abs_name);
      // uiItemEnumO_ptr(layout, ot, line, 0, "method", PF_WRITE_ORIGINAL);
      uiItemFullO_ptr(layout, ot, line, ICON_NONE, nullptr, WM_OP_EXEC_DEFAULT, 0, &props_ptr);
      RNA_enum_set(&props_ptr, "method", PF_WRITE_ORIGINAL);
      RNA_string_set(&props_ptr, "id", id_name);
      break;
  }

  UI_popup_menu_end(C, pup);
}

void ED_spacedata_id_remap(ScrArea *area, SpaceLink *sl, const IDRemapper *mappings)
{
  SpaceType *st = BKE_spacetype_from_id(sl->spacetype);
  if (st && st->id_remap) {
    st->id_remap(area, sl, mappings);
  }
}

void ED_spacedata_id_remap_single(ScrArea *area, SpaceLink *sl, ID *old_id, ID *new_id)
{
  SpaceType *st = BKE_spacetype_from_id(sl->spacetype);

  if (st && st->id_remap) {
    IDRemapper *mappings = BKE_id_remapper_create();
    BKE_id_remapper_add(mappings, old_id, new_id);
    st->id_remap(area, sl, mappings);
    BKE_id_remapper_free(mappings);
  }
}
