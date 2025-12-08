/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Pie Menu Region
 */

#include <cstdarg>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"
#include "BLI_time.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_layout.hh"

#include "BLT_translation.hh"

#include "interface_intern.hh"
#include "interface_regions_intern.hh"

namespace blender::ui {

/* -------------------------------------------------------------------- */
/** \name Pie Menu
 * \{ */

struct PieMenu {
  Block *pie_block; /* radial block of the pie menu (more could be added later) */
  Layout *layout;
  int mx, my;
};

static Block *block_func_PIE(bContext * /*C*/, PopupBlockHandle *handle, void *arg_pie)
{
  Block *block;
  PieMenu *pie = static_cast<PieMenu *>(arg_pie);
  int minwidth;

  minwidth = UI_MENU_WIDTH_MIN;
  block = pie->pie_block;

  /* in some cases we create the block before the region,
   * so we set it delayed here if necessary */
  if (BLI_findindex(&handle->region->runtime->uiblocks, block) == -1) {
    block_region_set(block, handle->region);
  }

  block_layout_resolve(block);

  block_flag_enable(block, BLOCK_LOOP | BLOCK_NUMSELECT);
  block_theme_style_set(block, BLOCK_THEME_STYLE_POPUP);

  block->minbounds = minwidth;
  block->bounds = 1;
  block->bounds_offset[0] = 0;
  block->bounds_offset[1] = 0;
  block->bounds_type = BLOCK_BOUNDS_PIE_CENTER;

  block->pie_data.pie_center_spawned[0] = pie->mx;
  block->pie_data.pie_center_spawned[1] = pie->my;

  return pie->pie_block;
}

static float ui_pie_menu_title_width(const char *name, int icon)
{
  const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
  return (fontstyle_string_width(fstyle, name) + (UI_UNIT_X * (1.50f + (icon ? 0.25f : 0.0f))));
}

PieMenu *pie_menu_begin(bContext *C, const char *title, int icon, const wmEvent *event)
{
  const uiStyle *style = style_get_dpi();
  short event_type;

  wmWindow *win = CTX_wm_window(C);

  PieMenu *pie = MEM_callocN<PieMenu>(__func__);

  pie->pie_block = block_begin(C, nullptr, __func__, EmbossType::Emboss);
  /* may be useful later to allow spawning pies
   * from old positions */
  // pie->pie_block->flag |= BLOCK_POPUP_MEMORY;
  pie->pie_block->puphash = ui_popup_menu_hash(title);
  pie->pie_block->flag |= BLOCK_PIE_MENU;

  /* if pie is spawned by a left click, release or click event,
   * it is always assumed to be click style */
  if (event->type == LEFTMOUSE || ELEM(event->val, KM_RELEASE, KM_CLICK)) {
    pie->pie_block->pie_data.flags |= PIE_CLICK_STYLE;
    pie->pie_block->pie_data.event_type = EVENT_NONE;
    win->pie_event_type_lock = EVENT_NONE;
  }
  else {
    if (win->pie_event_type_last != EVENT_NONE) {
      /* original pie key has been released, so don't propagate the event */
      if (win->pie_event_type_lock == EVENT_NONE) {
        event_type = EVENT_NONE;
        pie->pie_block->pie_data.flags |= PIE_CLICK_STYLE;
      }
      else {
        event_type = win->pie_event_type_last;
      }
    }
    else {
      event_type = event->type;
    }

    pie->pie_block->pie_data.event_type = event_type;
    win->pie_event_type_lock = event_type;
  }

  pie->layout = &block_layout(
      pie->pie_block, LayoutDirection::Vertical, LayoutType::PieMenu, 0, 0, 200, 0, 0, style);

  /* NOTE: #wmEvent.xy is where we started dragging in case of #KM_PRESS_DRAG. */
  pie->mx = event->xy[0];
  pie->my = event->xy[1];

  /* create title button */
  if (title[0]) {
    Button *but;
    char titlestr[256];
    int w;
    if (icon) {
      SNPRINTF_UTF8(titlestr, " %s", title);
      w = ui_pie_menu_title_width(titlestr, icon);
      but = uiDefIconTextBut(
          pie->pie_block, ButtonType::Label, icon, titlestr, 0, 0, w, UI_UNIT_Y, nullptr, "");
    }
    else {
      w = ui_pie_menu_title_width(title, 0);
      but = uiDefBut(
          pie->pie_block, ButtonType::Label, title, 0, 0, w, UI_UNIT_Y, nullptr, 0.0, 0.0, "");
    }
    /* do not align left */
    but->drawflag &= ~BUT_TEXT_LEFT;
    pie->pie_block->pie_data.title = but->str.c_str();
    pie->pie_block->pie_data.icon = icon;
  }

  return pie;
}

void pie_menu_end(bContext *C, PieMenu *pie)
{
  wmWindow *window = CTX_wm_window(C);

  PopupBlockHandle *menu = popup_block_create(
      C, nullptr, nullptr, nullptr, block_func_PIE, pie, nullptr, false);
  menu->popup = true;
  menu->towardstime = BLI_time_now_seconds();

  popup_handlers_add(C, &window->runtime->modalhandlers, menu, WM_HANDLER_ACCEPT_DBL_CLICK);
  WM_event_add_mousemove(window);

  MEM_freeN(pie);
}

Layout *pie_menu_layout(PieMenu *pie)
{
  return pie->layout;
}

wmOperatorStatus pie_menu_invoke(bContext *C, const char *idname, const wmEvent *event)
{
  MenuType *mt = WM_menutype_find(idname, true);

  if (mt == nullptr) {
    printf("%s: named menu \"%s\" not found\n", __func__, idname);
    return OPERATOR_CANCELLED;
  }

  if (WM_menutype_poll(C, mt) == false) {
    /* cancel but allow event to pass through, just like operators do */
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }

  PieMenu *pie = pie_menu_begin(
      C, CTX_IFACE_(mt->translation_context, mt->label), ICON_NONE, event);
  Layout *layout = pie_menu_layout(pie);

  menutype_draw(C, mt, layout);

  pie_menu_end(C, pie);

  return OPERATOR_INTERFACE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pie Menu Levels
 *
 * Pie menus can't contain more than 8 items (yet).
 * When using ##Layout::operator_enum, a "More" button is created that calls
 * a new pie menu if the enum has too many items. We call this a new "level".
 * Indirect recursion is used, so that a theoretically unlimited number of items is supported.
 *
 * This is a implementation specifically for operator enums,
 * needed since the object mode pie now has more than 8 items.
 * Ideally we'd have some way of handling this for all kinds of pie items, but that's tricky.
 *
 * - Julian (Feb 2016)
 * \{ */

struct PieMenuLevelData {
  char title[UI_MAX_NAME_STR]; /* parent pie title, copied for level */
  int icon;                    /* parent pie icon, copied for level */
  int totitem;                 /* total count of *remaining* items */

  /* needed for calling #Layout::operator_enum_items again for new level */
  wmOperatorType *ot;
  StringRefNull propname;
  IDProperty *properties;
  wm::OpCallContext context;
  eUI_Item_Flag flag;
};

/**
 * Invokes a new pie menu for a new level.
 */
static void ui_pie_menu_level_invoke(bContext *C, void *argN, void *arg2)
{
  EnumPropertyItem *item_array = (EnumPropertyItem *)argN;
  PieMenuLevelData *lvl = (PieMenuLevelData *)arg2;
  wmWindow *win = CTX_wm_window(C);

  PieMenu *pie = pie_menu_begin(C, IFACE_(lvl->title), lvl->icon, win->runtime->eventstate);
  Layout &layout = pie_menu_layout(pie)->menu_pie();

  PointerRNA ptr = WM_operator_properties_create_ptr(lvl->ot);
  /* So the context is passed to `itemf` functions (some need it). */
  WM_operator_properties_sanitize(&ptr, false);
  PropertyRNA *prop = RNA_struct_find_property(&ptr, lvl->propname.c_str());

  if (prop) {
    layout.op_enum_items(
        lvl->ot, ptr, prop, lvl->properties, lvl->context, lvl->flag, item_array, lvl->totitem);
  }
  else {
    RNA_warning("%s.%s not found", RNA_struct_identifier(ptr.type), lvl->propname.c_str());
  }

  pie_menu_end(C, pie);
}

void pie_menu_level_create(Block *block,
                           wmOperatorType *ot,
                           const StringRefNull propname,
                           IDProperty *properties,
                           const EnumPropertyItem *items,
                           int totitem,
                           const wm::OpCallContext context,
                           const eUI_Item_Flag flag)
{
  const int totitem_parent = PIE_MAX_ITEMS - 1;
  const int totitem_remain = totitem - totitem_parent;
  const size_t array_size = sizeof(EnumPropertyItem) * totitem_remain;

  /* used as but->func_argN so freeing is handled elsewhere */
  EnumPropertyItem *remaining = static_cast<EnumPropertyItem *>(
      MEM_mallocN(array_size + sizeof(EnumPropertyItem), "pie_level_item_array"));
  memcpy(remaining, items + totitem_parent, array_size);
  /* A null terminating sentinel element is required. */
  memset(&remaining[totitem_remain], 0, sizeof(EnumPropertyItem));

  /* yuk, static... issue is we can't reliably free this without doing dangerous changes */
  static PieMenuLevelData lvl;
  STRNCPY_UTF8(lvl.title, block->pie_data.title);
  lvl.totitem = totitem_remain;
  lvl.ot = ot;
  lvl.propname = propname;
  lvl.properties = properties;
  lvl.context = context;
  lvl.flag = flag;

  /* add a 'more' menu entry */
  Button *but = uiDefIconTextBut(block,
                                 ButtonType::But,
                                 ICON_PLUS,
                                 "More",
                                 0,
                                 0,
                                 UI_UNIT_X * 3,
                                 UI_UNIT_Y,
                                 nullptr,
                                 "Show more items of this menu");
  button_funcN_set(but, ui_pie_menu_level_invoke, remaining, &lvl);
}

/** \} */ /* Pie Menu Levels */

}  // namespace blender::ui
