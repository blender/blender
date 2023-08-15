/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Experimental tool-system>
 */

#include <cstring>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_ID.h"
#include "DNA_brush_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "BKE_brush.hh"
#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_paint.hh"
#include "BKE_workspace.h"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_toolsystem.h" /* own include */
#include "WM_types.hh"

static void toolsystem_reinit_with_toolref(bContext *C, WorkSpace * /*workspace*/, bToolRef *tref);
static bToolRef *toolsystem_reinit_ensure_toolref(bContext *C,
                                                  WorkSpace *workspace,
                                                  const bToolKey *tkey,
                                                  const char *default_tool);
static void toolsystem_refresh_screen_from_active_tool(Main *bmain,
                                                       WorkSpace *workspace,
                                                       bToolRef *tref);

/* -------------------------------------------------------------------- */
/** \name Tool Reference API
 * \{ */

bToolRef *WM_toolsystem_ref_from_context(bContext *C)
{
  WorkSpace *workspace = CTX_wm_workspace(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  ScrArea *area = CTX_wm_area(C);
  if ((area == nullptr) || ((1 << area->spacetype) & WM_TOOLSYSTEM_SPACE_MASK) == 0) {
    return nullptr;
  }
  bToolKey tkey{};
  tkey.space_type = area->spacetype;
  tkey.mode = WM_toolsystem_mode_from_spacetype(scene, view_layer, area, area->spacetype);
  bToolRef *tref = WM_toolsystem_ref_find(workspace, &tkey);
  /* We could return 'area->runtime.tool' in this case. */
  if (area->runtime.is_tool_set) {
    BLI_assert(tref == area->runtime.tool);
  }
  return tref;
}

bToolRef_Runtime *WM_toolsystem_runtime_from_context(bContext *C)
{
  bToolRef *tref = WM_toolsystem_ref_from_context(C);
  return tref ? tref->runtime : nullptr;
}

bToolRef *WM_toolsystem_ref_find(WorkSpace *workspace, const bToolKey *tkey)
{
  BLI_assert((1 << tkey->space_type) & WM_TOOLSYSTEM_SPACE_MASK);
  LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
    if ((tref->space_type == tkey->space_type) && (tref->mode == tkey->mode)) {
      return tref;
    }
  }
  return nullptr;
}

bToolRef_Runtime *WM_toolsystem_runtime_find(WorkSpace *workspace, const bToolKey *tkey)
{
  bToolRef *tref = WM_toolsystem_ref_find(workspace, tkey);
  return tref ? tref->runtime : nullptr;
}

bool WM_toolsystem_ref_ensure(WorkSpace *workspace, const bToolKey *tkey, bToolRef **r_tref)
{
  bToolRef *tref = WM_toolsystem_ref_find(workspace, tkey);
  if (tref) {
    *r_tref = tref;
    return false;
  }
  tref = static_cast<bToolRef *>(MEM_callocN(sizeof(*tref), __func__));
  BLI_addhead(&workspace->tools, tref);
  tref->space_type = tkey->space_type;
  tref->mode = tkey->mode;
  *r_tref = tref;
  return true;
}

/** \} */

static void toolsystem_unlink_ref(bContext *C, WorkSpace * /*workspace*/, bToolRef *tref)
{
  bToolRef_Runtime *tref_rt = tref->runtime;

  if (tref_rt->gizmo_group[0]) {
    wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(tref_rt->gizmo_group, false);
    if (gzgt != nullptr) {
      Main *bmain = CTX_data_main(C);
      WM_gizmo_group_remove_by_tool(C, bmain, gzgt, tref);
    }
  }
}
void WM_toolsystem_unlink(bContext *C, WorkSpace *workspace, const bToolKey *tkey)
{
  bToolRef *tref = WM_toolsystem_ref_find(workspace, tkey);
  if (tref && tref->runtime) {
    toolsystem_unlink_ref(C, workspace, tref);
  }
}

static void toolsystem_ref_link(bContext *C, WorkSpace *workspace, bToolRef *tref)
{
  bToolRef_Runtime *tref_rt = tref->runtime;
  if (tref_rt->gizmo_group[0]) {
    const char *idname = tref_rt->gizmo_group;
    wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(idname, false);
    if (gzgt != nullptr) {
      if ((gzgt->flag & WM_GIZMOGROUPTYPE_TOOL_INIT) == 0) {
        if (!WM_gizmo_group_type_ensure_ptr(gzgt)) {
          /* Even if the group-type was has been linked, it's possible the space types
           * were not previously using it. (happens with multiple windows.) */
          wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(&gzgt->gzmap_params);
          WM_gizmoconfig_update_tag_group_type_init(gzmap_type, gzgt);
        }
      }
    }
    else {
      CLOG_WARN(WM_LOG_TOOLS, "'%s' widget not found", idname);
    }
  }

  if (tref_rt->data_block[0]) {
    Main *bmain = CTX_data_main(C);

    if ((tref->space_type == SPACE_VIEW3D) && (tref->mode == CTX_MODE_PARTICLE)) {
      const EnumPropertyItem *items = rna_enum_particle_edit_hair_brush_items;
      const int i = RNA_enum_from_identifier(items, tref_rt->data_block);
      if (i != -1) {
        const int value = items[i].value;
        wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
        LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
          if (workspace == WM_window_get_active_workspace(win)) {
            Scene *scene = WM_window_get_active_scene(win);
            ToolSettings *ts = scene->toolsettings;
            ts->particle.brushtype = value;
          }
        }
      }
    }
    else {
      const ePaintMode paint_mode = BKE_paintmode_get_from_tool(tref);
      const eObjectMode ob_paint_mode = BKE_paint_object_mode_from_paintmode(paint_mode);
      BLI_assert(paint_mode != PAINT_MODE_INVALID);
      const EnumPropertyItem *items = BKE_paint_get_tool_enum_from_paintmode(paint_mode);
      BLI_assert(items != nullptr);

      const int i = items ? RNA_enum_from_identifier(items, tref_rt->data_block) : -1;
      if (i != -1) {
        const int slot_index = items[i].value;
        wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
        LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
          if (workspace == WM_window_get_active_workspace(win)) {
            Scene *scene = WM_window_get_active_scene(win);
            BKE_paint_ensure_from_paintmode(scene, paint_mode);
            Paint *paint = BKE_paint_get_active_from_paintmode(scene, paint_mode);
            Brush *brush = BKE_paint_toolslots_brush_get(paint, slot_index);
            if (brush == nullptr) {
              /* Could make into a function. */
              brush = (Brush *)BKE_libblock_find_name(bmain, ID_BR, items[i].name);
              if (brush && (brush->ob_mode & ob_paint_mode) &&
                  slot_index == BKE_brush_tool_get(brush, paint)) {
                /* pass */
              }
              else {
                brush = BKE_brush_add(bmain, items[i].name, eObjectMode(paint->runtime.ob_mode));

                BKE_brush_tool_set(brush, paint, slot_index);

                if (paint_mode == PAINT_MODE_SCULPT) {
                  BKE_brush_sculpt_reset(brush);
                }
              }
              BKE_paint_brush_set(paint, brush);
            }
            BKE_paint_brush_set(paint, brush);
          }
        }
      }
    }
  }
}

static void toolsystem_refresh_ref(bContext *C, WorkSpace *workspace, bToolRef *tref)
{
  if (tref->runtime == nullptr) {
    return;
  }
  /* currently same operation. */
  toolsystem_ref_link(C, workspace, tref);
}
void WM_toolsystem_refresh(bContext *C, WorkSpace *workspace, const bToolKey *tkey)
{
  bToolRef *tref = WM_toolsystem_ref_find(workspace, tkey);
  if (tref) {
    toolsystem_refresh_ref(C, workspace, tref);
  }
}

static void toolsystem_reinit_ref(bContext *C, WorkSpace *workspace, bToolRef *tref)
{
  toolsystem_reinit_with_toolref(C, workspace, tref);
}
void WM_toolsystem_reinit(bContext *C, WorkSpace *workspace, const bToolKey *tkey)
{
  bToolRef *tref = WM_toolsystem_ref_find(workspace, tkey);
  if (tref) {
    toolsystem_reinit_ref(C, workspace, tref);
  }
}

void WM_toolsystem_unlink_all(bContext *C, WorkSpace *workspace)
{
  LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
    tref->tag = 0;
  }

  LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
    if (tref->runtime) {
      if (tref->tag == 0) {
        toolsystem_unlink_ref(C, workspace, tref);
        tref->tag = 1;
      }
    }
  }
}

void WM_toolsystem_refresh_all(bContext *C, WorkSpace *workspace)
{
  BLI_assert(0);
  LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
    toolsystem_refresh_ref(C, workspace, tref);
  }
}
void WM_toolsystem_reinit_all(bContext *C, wmWindow *win)
{
  bScreen *screen = WM_window_get_active_screen(win);
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    if (((1 << area->spacetype) & WM_TOOLSYSTEM_SPACE_MASK) == 0) {
      continue;
    }

    WorkSpace *workspace = WM_window_get_active_workspace(win);
    bToolKey tkey{};
    tkey.space_type = area->spacetype;
    tkey.mode = WM_toolsystem_mode_from_spacetype(scene, view_layer, area, area->spacetype);
    bToolRef *tref = WM_toolsystem_ref_find(workspace, &tkey);
    if (tref) {
      if (tref->tag == 0) {
        toolsystem_reinit_ref(C, workspace, tref);
        tref->tag = 1;
      }
    }
  }
}

void WM_toolsystem_ref_set_from_runtime(bContext *C,
                                        WorkSpace *workspace,
                                        bToolRef *tref,
                                        const bToolRef_Runtime *tref_rt,
                                        const char *idname)
{
  Main *bmain = CTX_data_main(C);

  if (tref->runtime) {
    toolsystem_unlink_ref(C, workspace, tref);
  }

  STRNCPY(tref->idname, idname);

  if (tref->runtime == nullptr) {
    tref->runtime = static_cast<bToolRef_Runtime *>(MEM_callocN(sizeof(*tref->runtime), __func__));
  }

  if (tref_rt != tref->runtime) {
    *tref->runtime = *tref_rt;
  }

  /* Ideally Python could check this gizmo group flag and not
   * pass in the argument to begin with. */
  bool use_fallback_keymap = false;

  if (tref->idname_fallback[0] || tref->runtime->keymap_fallback[0]) {
    if (tref_rt->flag & TOOLREF_FLAG_FALLBACK_KEYMAP) {
      use_fallback_keymap = true;
    }
    else if (tref_rt->gizmo_group[0]) {
      wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(tref_rt->gizmo_group, false);
      if (gzgt) {
        if (gzgt->flag & WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP) {
          use_fallback_keymap = true;
        }
      }
    }
  }
  if (use_fallback_keymap == false) {
    tref->idname_fallback[0] = '\0';
    tref->runtime->keymap_fallback[0] = '\0';
  }

  toolsystem_ref_link(C, workspace, tref);

  toolsystem_refresh_screen_from_active_tool(bmain, workspace, tref);

  /* Set the cursor if possible, if not - it's fine as entering the region will refresh it. */
  {
    wmWindow *win = CTX_wm_window(C);
    if (win != nullptr) {
      win->addmousemove = true;
      win->tag_cursor_refresh = true;
    }
  }

  {
    wmMsgBus *mbus = CTX_wm_message_bus(C);
    WM_msg_publish_rna_prop(mbus, &workspace->id, workspace, WorkSpace, tools);
  }
}

void WM_toolsystem_ref_sync_from_context(Main *bmain, WorkSpace *workspace, bToolRef *tref)
{
  bToolRef_Runtime *tref_rt = tref->runtime;
  if ((tref_rt == nullptr) || (tref_rt->data_block[0] == '\0')) {
    return;
  }
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    if (workspace != WM_window_get_active_workspace(win)) {
      continue;
    }

    Scene *scene = WM_window_get_active_scene(win);
    ToolSettings *ts = scene->toolsettings;
    ViewLayer *view_layer = WM_window_get_active_view_layer(win);
    BKE_view_layer_synced_ensure(scene, view_layer);
    const Object *ob = BKE_view_layer_active_object_get(view_layer);
    if (ob == nullptr) {
      /* pass */
    }
    if ((tref->space_type == SPACE_VIEW3D) && (tref->mode == CTX_MODE_PARTICLE)) {
      if (ob->mode & OB_MODE_PARTICLE_EDIT) {
        const EnumPropertyItem *items = rna_enum_particle_edit_hair_brush_items;
        const int i = RNA_enum_from_value(items, ts->particle.brushtype);
        const EnumPropertyItem *item = &items[i];
        if (!STREQ(tref_rt->data_block, item->identifier)) {
          STRNCPY(tref_rt->data_block, item->identifier);
          SNPRINTF(tref->idname, "builtin_brush.%s", item->name);
        }
      }
    }
    else {
      const ePaintMode paint_mode = BKE_paintmode_get_from_tool(tref);
      Paint *paint = BKE_paint_get_active_from_paintmode(scene, paint_mode);
      const EnumPropertyItem *items = BKE_paint_get_tool_enum_from_paintmode(paint_mode);
      if (paint && paint->brush && items) {
        const ID *brush = (ID *)paint->brush;
        const char tool_type = BKE_brush_tool_get((Brush *)brush, paint);
        const int i = RNA_enum_from_value(items, tool_type);
        /* Possible when loading files from the future. */
        if (i != -1) {
          const char *name = items[i].name;
          const char *identifier = items[i].identifier;
          if (!STREQ(tref_rt->data_block, identifier)) {
            STRNCPY(tref_rt->data_block, identifier);
            SNPRINTF(tref->idname, "builtin_brush.%s", name);
          }
        }
      }
    }
  }
}

void WM_toolsystem_init(bContext *C)
{
  Main *bmain = CTX_data_main(C);

  BLI_assert(CTX_wm_window(C) == nullptr);

  LISTBASE_FOREACH (WorkSpace *, workspace, &bmain->workspaces) {
    LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
      MEM_SAFE_FREE(tref->runtime);
    }
  }

  /* Rely on screen initialization for gizmos. */
}

static bool toolsystem_key_ensure_check(const bToolKey *tkey)
{
  switch (tkey->space_type) {
    case SPACE_VIEW3D:
      return true;
    case SPACE_IMAGE:
      if (ELEM(tkey->mode, SI_MODE_PAINT, SI_MODE_UV)) {
        return true;
      }
      break;
    case SPACE_NODE:
      return true;
    case SPACE_SEQ:
      return true;
  }
  return false;
}

int WM_toolsystem_mode_from_spacetype(const Scene *scene,
                                      ViewLayer *view_layer,
                                      ScrArea *area,
                                      int space_type)
{
  int mode = -1;
  switch (space_type) {
    case SPACE_VIEW3D: {
      /* 'area' may be nullptr in this case. */
      BKE_view_layer_synced_ensure(scene, view_layer);
      Object *obact = BKE_view_layer_active_object_get(view_layer);
      if (obact != nullptr) {
        Object *obedit = OBEDIT_FROM_OBACT(obact);
        mode = CTX_data_mode_enum_ex(obedit, obact, eObjectMode(obact->mode));
      }
      else {
        mode = CTX_MODE_OBJECT;
      }
      break;
    }
    case SPACE_IMAGE: {
      SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
      mode = sima->mode;
      break;
    }
    case SPACE_NODE: {
      mode = 0;
      break;
    }
    case SPACE_SEQ: {
      SpaceSeq *sseq = static_cast<SpaceSeq *>(area->spacedata.first);
      mode = sseq->view;
      break;
    }
  }
  return mode;
}

bool WM_toolsystem_key_from_context(const Scene *scene,
                                    ViewLayer *view_layer,
                                    ScrArea *area,
                                    bToolKey *tkey)
{
  int space_type = SPACE_EMPTY;
  int mode = -1;

  if (area != nullptr) {
    space_type = area->spacetype;
    mode = WM_toolsystem_mode_from_spacetype(scene, view_layer, area, space_type);
  }

  if (mode != -1) {
    tkey->space_type = space_type;
    tkey->mode = mode;
    return true;
  }
  return false;
}

void WM_toolsystem_refresh_active(bContext *C)
{
  Main *bmain = CTX_data_main(C);

  struct {
    wmWindow *win;
    ScrArea *area;
    ARegion *region;
    bool is_set;
  } context_prev = {nullptr};

  for (wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first); wm;
       wm = static_cast<wmWindowManager *>(wm->id.next))
  {
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      WorkSpace *workspace = WM_window_get_active_workspace(win);
      bScreen *screen = WM_window_get_active_screen(win);
      const Scene *scene = WM_window_get_active_scene(win);
      ViewLayer *view_layer = WM_window_get_active_view_layer(win);
      /* Could skip loop for modes that don't depend on space type. */
      int space_type_mask_handled = 0;
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        /* Don't change the space type of the active tool, only update its mode. */
        const int space_type_mask = (1 << area->spacetype);
        if ((space_type_mask & WM_TOOLSYSTEM_SPACE_MASK) &&
            ((space_type_mask_handled & space_type_mask) == 0))
        {
          space_type_mask_handled |= space_type_mask;
          bToolKey tkey{};
          tkey.space_type = area->spacetype;
          tkey.mode = WM_toolsystem_mode_from_spacetype(scene, view_layer, area, area->spacetype);
          bToolRef *tref = WM_toolsystem_ref_find(workspace, &tkey);
          if (tref != area->runtime.tool) {
            if (context_prev.is_set == false) {
              context_prev.win = CTX_wm_window(C);
              context_prev.area = CTX_wm_area(C);
              context_prev.region = CTX_wm_region(C);
              context_prev.is_set = true;
            }

            CTX_wm_window_set(C, win);
            CTX_wm_area_set(C, area);

            toolsystem_reinit_ensure_toolref(C, workspace, &tkey, nullptr);
          }
        }
      }
    }
  }

  if (context_prev.is_set) {
    CTX_wm_window_set(C, context_prev.win);
    CTX_wm_area_set(C, context_prev.area);
    CTX_wm_region_set(C, context_prev.region);
  }

  BKE_workspace_id_tag_all_visible(bmain, LIB_TAG_DOIT);

  LISTBASE_FOREACH (WorkSpace *, workspace, &bmain->workspaces) {
    if (workspace->id.tag & LIB_TAG_DOIT) {
      workspace->id.tag &= ~LIB_TAG_DOIT;
      /* Refresh to ensure data is initialized.
       * This is needed because undo can load a state which no longer has the underlying DNA data
       * needed for the tool (un-initialized paint-slots for eg), see: #64339. */
      LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
        toolsystem_refresh_ref(C, workspace, tref);
      }
    }
  }
}

void WM_toolsystem_refresh_screen_area(WorkSpace *workspace,
                                       const Scene *scene,
                                       ViewLayer *view_layer,
                                       ScrArea *area)
{
  area->runtime.tool = nullptr;
  area->runtime.is_tool_set = true;
  const int mode = WM_toolsystem_mode_from_spacetype(scene, view_layer, area, area->spacetype);
  LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
    if (tref->space_type == area->spacetype) {
      if (tref->mode == mode) {
        area->runtime.tool = tref;
        break;
      }
    }
  }
}

void WM_toolsystem_refresh_screen_window(wmWindow *win)
{
  WorkSpace *workspace = WM_window_get_active_workspace(win);
  bool space_type_has_tools[SPACE_TYPE_NUM] = {false};
  LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
    space_type_has_tools[tref->space_type] = true;
  }
  bScreen *screen = WM_window_get_active_screen(win);
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    area->runtime.tool = nullptr;
    area->runtime.is_tool_set = true;
    if (space_type_has_tools[area->spacetype]) {
      WM_toolsystem_refresh_screen_area(workspace, scene, view_layer, area);
    }
  }
}

void WM_toolsystem_refresh_screen_all(Main *bmain)
{
  /* Update all ScrArea's tools */
  for (wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first); wm;
       wm = static_cast<wmWindowManager *>(wm->id.next))
  {
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      WM_toolsystem_refresh_screen_window(win);
    }
  }
}

static void toolsystem_refresh_screen_from_active_tool(Main *bmain,
                                                       WorkSpace *workspace,
                                                       bToolRef *tref)
{
  /* Update all ScrArea's tools */
  for (wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first); wm;
       wm = static_cast<wmWindowManager *>(wm->id.next))
  {
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      if (workspace == WM_window_get_active_workspace(win)) {
        bScreen *screen = WM_window_get_active_screen(win);
        const Scene *scene = WM_window_get_active_scene(win);
        ViewLayer *view_layer = WM_window_get_active_view_layer(win);
        LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
          if (area->spacetype == tref->space_type) {
            int mode = WM_toolsystem_mode_from_spacetype(scene, view_layer, area, area->spacetype);
            if (mode == tref->mode) {
              area->runtime.tool = tref;
              area->runtime.is_tool_set = true;
            }
          }
        }
      }
    }
  }
}

bToolRef *WM_toolsystem_ref_set_by_id_ex(
    bContext *C, WorkSpace *workspace, const bToolKey *tkey, const char *name, bool cycle)
{
  wmOperatorType *ot = WM_operatortype_find("WM_OT_tool_set_by_id", false);
  /* On startup, Python operators are not yet loaded. */
  if (ot == nullptr) {
    return nullptr;
  }

/* Some contexts use the current space type (image editor for e.g.),
 * ensure this is set correctly or there is no area. */
#ifndef NDEBUG
  /* Exclude this check for some space types where the space type isn't used. */
  if ((1 << tkey->space_type) & WM_TOOLSYSTEM_SPACE_MASK_MODE_FROM_SPACE) {
    ScrArea *area = CTX_wm_area(C);
    BLI_assert(area == nullptr || area->spacetype == tkey->space_type);
  }
#endif

  PointerRNA op_props;
  WM_operator_properties_create_ptr(&op_props, ot);
  RNA_string_set(&op_props, "name", name);

  BLI_assert((1 << tkey->space_type) & WM_TOOLSYSTEM_SPACE_MASK);

  RNA_enum_set(&op_props, "space_type", tkey->space_type);
  RNA_boolean_set(&op_props, "cycle", cycle);

  WM_operator_name_call_ptr(C, ot, WM_OP_EXEC_DEFAULT, &op_props, nullptr);
  WM_operator_properties_free(&op_props);

  bToolRef *tref = WM_toolsystem_ref_find(workspace, tkey);

  if (tref) {
    Main *bmain = CTX_data_main(C);
    toolsystem_refresh_screen_from_active_tool(bmain, workspace, tref);
  }

  return (tref && STREQ(tref->idname, name)) ? tref : nullptr;
}

bToolRef *WM_toolsystem_ref_set_by_id(bContext *C, const char *name)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  ScrArea *area = CTX_wm_area(C);
  bToolKey tkey;
  if (WM_toolsystem_key_from_context(scene, view_layer, area, &tkey)) {
    WorkSpace *workspace = CTX_wm_workspace(C);
    return WM_toolsystem_ref_set_by_id_ex(C, workspace, &tkey, name, false);
  }
  return nullptr;
}

static void toolsystem_reinit_with_toolref(bContext *C, WorkSpace *workspace, bToolRef *tref)
{
  bToolKey tkey{};
  tkey.space_type = tref->space_type;
  tkey.mode = tref->mode;
  WM_toolsystem_ref_set_by_id_ex(C, workspace, &tkey, tref->idname, false);
}

static const char *toolsystem_default_tool(const bToolKey *tkey)
{
  switch (tkey->space_type) {
    case SPACE_VIEW3D:
      switch (tkey->mode) {
        /* Use the names of the enums for each brush tool. */
        case CTX_MODE_SCULPT:
        case CTX_MODE_PAINT_VERTEX:
        case CTX_MODE_PAINT_WEIGHT:
        case CTX_MODE_PAINT_TEXTURE:
        case CTX_MODE_PAINT_GPENCIL_LEGACY:
        case CTX_MODE_PAINT_GREASE_PENCIL:
          return "builtin_brush.Draw";
        case CTX_MODE_SCULPT_GPENCIL_LEGACY:
          return "builtin_brush.Push";
        case CTX_MODE_WEIGHT_GPENCIL_LEGACY:
          return "builtin_brush.Weight";
        case CTX_MODE_VERTEX_GPENCIL_LEGACY:
          return "builtin_brush.Draw";
        case CTX_MODE_SCULPT_CURVES:
          return "builtin_brush.Density";
          /* end temporary hack. */

        case CTX_MODE_PARTICLE:
          return "builtin_brush.Comb";
        case CTX_MODE_EDIT_TEXT:
          return "builtin.select_text";
      }
      break;
    case SPACE_IMAGE:
      switch (tkey->mode) {
        case SI_MODE_PAINT:
          return "builtin_brush.Draw";
      }
      break;
    case SPACE_NODE: {
      return "builtin.select_box";
    }
    case SPACE_SEQ: {
      switch (tkey->mode) {
        case SEQ_VIEW_SEQUENCE:
          return "builtin.select";
        case SEQ_VIEW_PREVIEW:
          return "builtin.sample";
        case SEQ_VIEW_SEQUENCE_PREVIEW:
          return "builtin.select";
      }
      return "builtin.select_box";
    }
  }

  return "builtin.select_box";
}

/**
 * Run after changing modes.
 */
static bToolRef *toolsystem_reinit_ensure_toolref(bContext *C,
                                                  WorkSpace *workspace,
                                                  const bToolKey *tkey,
                                                  const char *default_tool)
{
  bToolRef *tref;
  if (WM_toolsystem_ref_ensure(workspace, tkey, &tref)) {
    if (default_tool == nullptr) {
      default_tool = toolsystem_default_tool(tkey);
    }
    STRNCPY(tref->idname, default_tool);
  }
  toolsystem_reinit_with_toolref(C, workspace, tref);
  return tref;
}

static void wm_toolsystem_update_from_context_view3d_impl(bContext *C, WorkSpace *workspace)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  int space_type = SPACE_VIEW3D;
  bToolKey tkey{};
  tkey.space_type = space_type;
  tkey.mode = WM_toolsystem_mode_from_spacetype(scene, view_layer, nullptr, space_type);
  toolsystem_reinit_ensure_toolref(C, workspace, &tkey, nullptr);
}

void WM_toolsystem_update_from_context_view3d(bContext *C)
{
  WorkSpace *workspace = CTX_wm_workspace(C);
  wm_toolsystem_update_from_context_view3d_impl(C, workspace);

  /* Multi window support. */
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
  if (!BLI_listbase_is_single(&wm->windows)) {
    wmWindow *win_prev = CTX_wm_window(C);
    ScrArea *area_prev = CTX_wm_area(C);
    ARegion *region_prev = CTX_wm_region(C);

    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      if (win != win_prev) {
        WorkSpace *workspace_iter = WM_window_get_active_workspace(win);
        if (workspace_iter != workspace) {

          CTX_wm_window_set(C, win);

          wm_toolsystem_update_from_context_view3d_impl(C, workspace_iter);

          CTX_wm_window_set(C, win_prev);
          CTX_wm_area_set(C, area_prev);
          CTX_wm_region_set(C, region_prev);
        }
      }
    }
  }
}

void WM_toolsystem_update_from_context(
    bContext *C, WorkSpace *workspace, const Scene *scene, ViewLayer *view_layer, ScrArea *area)
{
  bToolKey tkey{};
  tkey.space_type = area->spacetype;
  tkey.mode = WM_toolsystem_mode_from_spacetype(scene, view_layer, area, area->spacetype);
  if (toolsystem_key_ensure_check(&tkey)) {
    toolsystem_reinit_ensure_toolref(C, workspace, &tkey, nullptr);
  }
}

bool WM_toolsystem_active_tool_is_brush(const bContext *C)
{
  bToolRef_Runtime *tref_rt = WM_toolsystem_runtime_from_context((bContext *)C);
  return tref_rt && (tref_rt->data_block[0] != '\0');
}

void WM_toolsystem_do_msg_notify_tag_refresh(bContext *C,
                                             wmMsgSubscribeKey * /*msg_key*/,
                                             wmMsgSubscribeValue *msg_val)
{
  ScrArea *area = static_cast<ScrArea *>(msg_val->user_data);
  Main *bmain = CTX_data_main(C);
  wmWindow *win = static_cast<wmWindow *>(((wmWindowManager *)bmain->wm.first)->windows.first);
  if (win->next != nullptr) {
    do {
      bScreen *screen = WM_window_get_active_screen(win);
      if (BLI_findindex(&screen->areabase, area) != -1) {
        break;
      }
    } while ((win = win->next));
  }

  WorkSpace *workspace = WM_window_get_active_workspace(win);
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);

  bToolKey tkey{};
  tkey.space_type = area->spacetype;
  tkey.mode = WM_toolsystem_mode_from_spacetype(scene, view_layer, area, area->spacetype);
  WM_toolsystem_refresh(C, workspace, &tkey);
  WM_toolsystem_refresh_screen_area(workspace, scene, view_layer, area);
}

static IDProperty *idprops_ensure_named_group(IDProperty *group, const char *idname)
{
  IDProperty *prop = IDP_GetPropertyFromGroup(group, idname);
  if ((prop == nullptr) || (prop->type != IDP_GROUP)) {
    IDPropertyTemplate val = {0};
    prop = IDP_New(IDP_GROUP, &val, __func__);
    STRNCPY(prop->name, idname);
    IDP_ReplaceInGroup_ex(group, prop, nullptr);
  }
  return prop;
}

IDProperty *WM_toolsystem_ref_properties_get_idprops(bToolRef *tref)
{
  IDProperty *group = tref->properties;
  if (group == nullptr) {
    return nullptr;
  }
  return IDP_GetPropertyFromGroup(group, tref->idname);
}

IDProperty *WM_toolsystem_ref_properties_ensure_idprops(bToolRef *tref)
{
  if (tref->properties == nullptr) {
    IDPropertyTemplate val = {0};
    tref->properties = IDP_New(IDP_GROUP, &val, __func__);
  }
  return idprops_ensure_named_group(tref->properties, tref->idname);
}

bool WM_toolsystem_ref_properties_get_ex(bToolRef *tref,
                                         const char *idname,
                                         StructRNA *type,
                                         PointerRNA *r_ptr)
{
  IDProperty *group = WM_toolsystem_ref_properties_get_idprops(tref);
  IDProperty *prop = group ? IDP_GetPropertyFromGroup(group, idname) : nullptr;
  RNA_pointer_create(nullptr, type, prop, r_ptr);
  return (prop != nullptr);
}

void WM_toolsystem_ref_properties_ensure_ex(bToolRef *tref,
                                            const char *idname,
                                            StructRNA *type,
                                            PointerRNA *r_ptr)
{
  IDProperty *group = WM_toolsystem_ref_properties_ensure_idprops(tref);
  IDProperty *prop = idprops_ensure_named_group(group, idname);
  RNA_pointer_create(nullptr, type, prop, r_ptr);
}

void WM_toolsystem_ref_properties_init_for_keymap(bToolRef *tref,
                                                  PointerRNA *dst_ptr,
                                                  PointerRNA *src_ptr,
                                                  wmOperatorType *ot)
{
  *dst_ptr = *src_ptr;
  if (dst_ptr->data) {
    dst_ptr->data = IDP_CopyProperty(static_cast<const IDProperty *>(dst_ptr->data));
  }
  else {
    IDPropertyTemplate val = {0};
    dst_ptr->data = IDP_New(IDP_GROUP, &val, "wmOpItemProp");
  }
  IDProperty *group = WM_toolsystem_ref_properties_get_idprops(tref);
  if (group != nullptr) {
    IDProperty *prop = IDP_GetPropertyFromGroup(group, ot->idname);
    if (prop) {
      /* Important key-map items properties don't get overwritten by the tools.
       * - When a key-map item doesn't set a property, the tool-systems is used.
       * - When it does, it overrides the tool-system.
       *
       * This way the default action can be to follow the top-bar tool-settings &
       * modifier keys can be used to perform different actions that aren't clobbered here.
       */
      IDP_MergeGroup(static_cast<IDProperty *>(dst_ptr->data), prop, false);
    }
  }
}
