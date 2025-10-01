/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sptopbar
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_screen.hh"
#include "BKE_undo_system.hh"

#include "ED_screen.hh"
#include "ED_space_api.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "BLO_read_write.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_types.hh"

/* ******************** default callbacks for topbar space ***************** */

static SpaceLink *topbar_create(const ScrArea * /*area*/, const Scene * /*scene*/)
{
  ARegion *region;
  SpaceTopBar *stopbar;

  stopbar = MEM_callocN<SpaceTopBar>("init topbar");
  stopbar->spacetype = SPACE_TOPBAR;

  /* header */
  region = BKE_area_region_new();
  BLI_addtail(&stopbar->regionbase, region);
  region->regiontype = RGN_TYPE_HEADER;
  region->alignment = RGN_ALIGN_TOP;
  region = BKE_area_region_new();
  BLI_addtail(&stopbar->regionbase, region);
  region->regiontype = RGN_TYPE_HEADER;
  region->alignment = RGN_ALIGN_RIGHT | RGN_SPLIT_PREV;

  /* main regions */
  region = BKE_area_region_new();
  BLI_addtail(&stopbar->regionbase, region);
  region->regiontype = RGN_TYPE_WINDOW;

  return (SpaceLink *)stopbar;
}

/* Doesn't free the space-link itself. */
static void topbar_free(SpaceLink * /*sl*/) {}

/* spacetype; init callback */
static void topbar_init(wmWindowManager * /*wm*/, ScrArea * /*area*/) {}

static SpaceLink *topbar_duplicate(SpaceLink *sl)
{
  SpaceTopBar *stopbarn = static_cast<SpaceTopBar *>(MEM_dupallocN(sl));

  /* clear or remove stuff from old */

  return (SpaceLink *)stopbarn;
}

/* add handlers, stuff you only do once or on area/region changes */
static void topbar_main_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  /* force delayed UI_view2d_region_reinit call */
  if (ELEM(RGN_ALIGN_ENUM_FROM_MASK(region->alignment), RGN_ALIGN_RIGHT)) {
    region->flag |= RGN_FLAG_DYNAMIC_SIZE;
  }
  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_HEADER, region->winx, region->winy);

  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "View2D Buttons List", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->runtime->handlers, keymap);
}

static void topbar_operatortypes() {}

static void topbar_keymap(wmKeyConfig * /*keyconf*/) {}

/* add handlers, stuff you only do once or on area/region changes */
static void topbar_header_region_init(wmWindowManager * /*wm*/, ARegion *region)
{
  if (RGN_ALIGN_ENUM_FROM_MASK(region->alignment) == RGN_ALIGN_RIGHT) {
    region->flag |= RGN_FLAG_DYNAMIC_SIZE;
  }
  ED_region_header_init(region);
}

static void topbar_main_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_WM:
      if (wmn->data == ND_HISTORY) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SCENE:
      if (wmn->data == ND_MODE) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_VIEW3D) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_GPENCIL:
      if (wmn->data == ND_DATA) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

static void topbar_header_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_WM:
      if (wmn->data == ND_JOB) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_WORKSPACE:
      ED_region_tag_redraw(region);
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_INFO) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SCREEN:
      if (wmn->data == ND_LAYER) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SCENE:
      if (wmn->data == ND_SCENEBROWSE) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

static void topbar_header_region_message_subscribe(const wmRegionMessageSubscribeParams *params)
{
  wmMsgBus *mbus = params->message_bus;
  WorkSpace *workspace = params->workspace;
  ARegion *region = params->region;

  wmMsgSubscribeValue msg_sub_value_region_tag_redraw{};
  msg_sub_value_region_tag_redraw.owner = region;
  msg_sub_value_region_tag_redraw.user_data = region;
  msg_sub_value_region_tag_redraw.notify = ED_region_do_msg_notify_tag_redraw;

  WM_msg_subscribe_rna_prop(
      mbus, &workspace->id, workspace, WorkSpace, tools, &msg_sub_value_region_tag_redraw);
}

static void recent_files_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;
  layout->operator_context_set(blender::wm::OpCallContext::InvokeDefault);
  const bool is_menu_search = CTX_data_int_get(C, "is_menu_search").value_or(false);
  if (is_menu_search) {
    uiTemplateRecentFiles(layout, U.recent_files);
  }
  else {
    const int limit = std::min<int>(U.recent_files, 20);
    if (uiTemplateRecentFiles(layout, limit) != 0) {
      layout->separator();
      PointerRNA search_props = layout->op(
          "WM_OT_search_single_menu", IFACE_("More..."), ICON_VIEWZOOM);
      RNA_string_set(&search_props, "menu_idname", "TOPBAR_MT_file_open_recent");
      layout->op("WM_OT_clear_recent_files", IFACE_("Clear Recent Files List..."), ICON_TRASH);
    }
    else {
      layout->label(IFACE_("No Recent Files"), ICON_NONE);
    }
  }
}

static void recent_files_menu_register()
{
  MenuType *mt;

  mt = MEM_callocN<MenuType>("spacetype info menu recent files");
  STRNCPY_UTF8(mt->idname, "TOPBAR_MT_file_open_recent");
  STRNCPY_UTF8(mt->label, N_("Open Recent"));
  STRNCPY_UTF8(mt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  mt->draw = recent_files_menu_draw;
  WM_menutype_add(mt);
}

static void undo_history_draw_menu(const bContext *C, Menu *menu)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  if (wm->runtime->undo_stack == nullptr) {
    return;
  }

  int undo_step_count = 0;
  int undo_step_count_all = 0;
  LISTBASE_FOREACH_BACKWARD (UndoStep *, us, &wm->runtime->undo_stack->steps) {
    undo_step_count_all += 1;
    if (us->skip) {
      continue;
    }
    undo_step_count += 1;
  }

  uiLayout *split = &menu->layout->split(0.0f, false);
  uiLayout *column = nullptr;

  const int col_size = 20 + (undo_step_count / 12);

  undo_step_count = 0;

  /* Reverse the order so the most recent state is first in the menu. */
  int i = undo_step_count_all - 1;
  for (UndoStep *us = static_cast<UndoStep *>(wm->runtime->undo_stack->steps.last); us;
       us = us->prev, i--)
  {
    if (us->skip) {
      continue;
    }
    if (!(undo_step_count % col_size)) {
      column = &split->column(false);
    }
    const bool is_active = (us == wm->runtime->undo_stack->step_active);
    uiLayout *row = &column->row(false);
    row->enabled_set(!is_active);
    PointerRNA op_ptr = row->op("ED_OT_undo_history",
                                CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, us->name),
                                is_active ? ICON_LAYER_ACTIVE : ICON_NONE);
    RNA_int_set(&op_ptr, "item", i);
    undo_step_count += 1;
  }
}

static void undo_history_menu_register()
{
  MenuType *mt;

  mt = MEM_callocN<MenuType>(__func__);
  STRNCPY_UTF8(mt->idname, "TOPBAR_MT_undo_history");
  STRNCPY_UTF8(mt->label, N_("Undo History"));
  STRNCPY_UTF8(mt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  mt->draw = undo_history_draw_menu;
  WM_menutype_add(mt);
}

static void topbar_space_blend_write(BlendWriter *writer, SpaceLink *sl)
{
  BLO_write_struct(writer, SpaceTopBar, sl);
}

void ED_spacetype_topbar()
{
  std::unique_ptr<SpaceType> st = std::make_unique<SpaceType>();
  ARegionType *art;

  st->spaceid = SPACE_TOPBAR;
  STRNCPY_UTF8(st->name, "Top Bar");

  st->create = topbar_create;
  st->free = topbar_free;
  st->init = topbar_init;
  st->duplicate = topbar_duplicate;
  st->operatortypes = topbar_operatortypes;
  st->keymap = topbar_keymap;
  st->blend_write = topbar_space_blend_write;

  /* regions: main window */
  art = MEM_callocN<ARegionType>("spacetype topbar main region");
  art->regionid = RGN_TYPE_WINDOW;
  art->init = topbar_main_region_init;
  art->layout = ED_region_header_layout;
  art->draw = ED_region_header_draw;
  art->listener = topbar_main_region_listener;
  art->prefsizex = UI_UNIT_X * 5; /* Mainly to avoid glitches */
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;

  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_callocN<ARegionType>("spacetype topbar header region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->prefsizex = UI_UNIT_X * 5; /* Mainly to avoid glitches */
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;
  art->listener = topbar_header_listener;
  art->message_subscribe = topbar_header_region_message_subscribe;
  art->init = topbar_header_region_init;
  art->layout = ED_region_header_layout;
  art->draw = ED_region_header_draw;

  BLI_addhead(&st->regiontypes, art);

  recent_files_menu_register();
  undo_history_menu_register();

  BKE_spacetype_register(std::move(st));
}
