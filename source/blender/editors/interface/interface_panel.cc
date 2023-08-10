/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

/* a full doc with API notes can be found in
 * bf-blender/trunk/blender/doc/guides/interface_API.txt */

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "RNA_access.hh"

#include "BLF_api.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_screen.hh"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "GPU_batch_presets.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "interface_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Defines & Structs
 * \{ */

#define ANIMATION_TIME 0.30
#define ANIMATION_INTERVAL 0.02

enum uiPanelRuntimeFlag {
  PANEL_LAST_ADDED = (1 << 0),
  PANEL_ACTIVE = (1 << 2),
  PANEL_WAS_ACTIVE = (1 << 3),
  PANEL_ANIM_ALIGN = (1 << 4),
  PANEL_NEW_ADDED = (1 << 5),
  PANEL_SEARCH_FILTER_MATCH = (1 << 7),
  /**
   * Use the status set by property search (#PANEL_SEARCH_FILTER_MATCH)
   * instead of #PNL_CLOSED. Set to true on every property search update.
   */
  PANEL_USE_CLOSED_FROM_SEARCH = (1 << 8),
  /** The Panel was before the start of the current / latest layout pass. */
  PANEL_WAS_CLOSED = (1 << 9),
  /**
   * Set when the panel is being dragged and while it animates back to its aligned
   * position. Unlike #PANEL_STATE_ANIMATION, this is applied to sub-panels as well.
   */
  PANEL_IS_DRAG_DROP = (1 << 10),
  /** Draw a border with the active color around the panel. */
  PANEL_ACTIVE_BORDER = (1 << 11),
};

/* The state of the mouse position relative to the panel. */
enum uiPanelMouseState {
  PANEL_MOUSE_OUTSIDE,        /** Mouse is not in the panel. */
  PANEL_MOUSE_INSIDE_CONTENT, /** Mouse is in the actual panel content. */
  PANEL_MOUSE_INSIDE_HEADER,  /** Mouse is in the panel header. */
};

enum uiHandlePanelState {
  PANEL_STATE_DRAG,
  PANEL_STATE_ANIMATION,
  PANEL_STATE_EXIT,
};

struct uiHandlePanelData {
  uiHandlePanelState state;

  /* Animation. */
  wmTimer *animtimer;
  double starttime;

  /* Dragging. */
  int startx, starty;
  int startofsx, startofsy;
  float start_cur_xmin, start_cur_ymin;
};

struct PanelSort {
  Panel *panel;
  int new_offset_x;
  int new_offset_y;
};

static void panel_set_expansion_from_list_data(const bContext *C, Panel *panel);
static int get_panel_real_size_y(const Panel *panel);
static void panel_activate_state(const bContext *C, Panel *panel, const uiHandlePanelState state);
static int compare_panel(const void *a, const void *b);
static bool panel_type_context_poll(ARegion *region,
                                    const PanelType *panel_type,
                                    const char *context);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Local Functions
 * \{ */

static bool panel_active_animation_changed(ListBase *lb,
                                           Panel **r_panel_animation,
                                           bool *r_no_animation)
{
  LISTBASE_FOREACH (Panel *, panel, lb) {
    /* Detect panel active flag changes. */
    if (!(panel->type && panel->type->parent)) {
      if ((panel->runtime_flag & PANEL_WAS_ACTIVE) && !(panel->runtime_flag & PANEL_ACTIVE)) {
        return true;
      }
      if (!(panel->runtime_flag & PANEL_WAS_ACTIVE) && (panel->runtime_flag & PANEL_ACTIVE)) {
        return true;
      }
    }

    /* Detect changes in panel expansions. */
    if (bool(panel->runtime_flag & PANEL_WAS_CLOSED) != UI_panel_is_closed(panel)) {
      *r_panel_animation = panel;
      return false;
    }

    if ((panel->runtime_flag & PANEL_ACTIVE) && !UI_panel_is_closed(panel)) {
      if (panel_active_animation_changed(&panel->children, r_panel_animation, r_no_animation)) {
        return true;
      }
    }

    /* Detect animation. */
    if (panel->activedata) {
      uiHandlePanelData *data = static_cast<uiHandlePanelData *>(panel->activedata);
      if (data->state == PANEL_STATE_ANIMATION) {
        *r_panel_animation = panel;
      }
      else {
        /* Don't animate while handling other interaction. */
        *r_no_animation = true;
      }
    }
    if ((panel->runtime_flag & PANEL_ANIM_ALIGN) && !(*r_panel_animation)) {
      *r_panel_animation = panel;
    }
  }

  return false;
}

/**
 * \return True if the properties editor switch tabs since the last layout pass.
 */
static bool properties_space_needs_realign(const ScrArea *area, const ARegion *region)
{
  if (area->spacetype == SPACE_PROPERTIES && region->regiontype == RGN_TYPE_WINDOW) {
    const SpaceProperties *sbuts = static_cast<SpaceProperties *>(area->spacedata.first);

    if (sbuts->mainbo != sbuts->mainb) {
      return true;
    }
  }

  return false;
}

static bool panels_need_realign(const ScrArea *area, ARegion *region, Panel **r_panel_animation)
{
  *r_panel_animation = nullptr;

  if (properties_space_needs_realign(area, region)) {
    return true;
  }

  /* Detect if a panel was added or removed. */
  Panel *panel_animation = nullptr;
  bool no_animation = false;
  if (panel_active_animation_changed(&region->panels, &panel_animation, &no_animation)) {
    return true;
  }

  /* Detect panel marked for animation, if we're not already animating. */
  if (panel_animation) {
    if (!no_animation) {
      *r_panel_animation = panel_animation;
    }
    return true;
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Functions for Instanced Panels
 * \{ */

static Panel *panel_add_instanced(ListBase *panels, PanelType *panel_type, PointerRNA *custom_data)
{
  Panel *panel = MEM_cnew<Panel>(__func__);
  panel->type = panel_type;
  STRNCPY(panel->panelname, panel_type->idname);

  panel->runtime.custom_data_ptr = custom_data;
  panel->runtime_flag |= PANEL_NEW_ADDED;

  /* Add the panel's children too. Although they aren't instanced panels, we can still use this
   * function to create them, as UI_panel_begin does other things we don't need to do. */
  LISTBASE_FOREACH (LinkData *, child, &panel_type->children) {
    PanelType *child_type = static_cast<PanelType *>(child->data);
    panel_add_instanced(&panel->children, child_type, custom_data);
  }

  /* Make sure the panel is added to the end of the display-order as well. This is needed for
   * loading existing files.
   *
   * NOTE: We could use special behavior to place it after the panel that starts the list of
   * instanced panels, but that would add complexity that isn't needed for now. */
  int max_sortorder = 0;
  LISTBASE_FOREACH (Panel *, existing_panel, panels) {
    if (existing_panel->sortorder > max_sortorder) {
      max_sortorder = existing_panel->sortorder;
    }
  }
  panel->sortorder = max_sortorder + 1;

  BLI_addtail(panels, panel);

  return panel;
}

Panel *UI_panel_add_instanced(const bContext *C,
                              ARegion *region,
                              ListBase *panels,
                              const char *panel_idname,
                              PointerRNA *custom_data)
{
  ARegionType *region_type = region->type;

  PanelType *panel_type = static_cast<PanelType *>(
      BLI_findstring(&region_type->paneltypes, panel_idname, offsetof(PanelType, idname)));

  if (panel_type == nullptr) {
    printf("Panel type '%s' not found.\n", panel_idname);
    return nullptr;
  }

  Panel *new_panel = panel_add_instanced(panels, panel_type, custom_data);

  /* Do this after #panel_add_instatnced so all sub-panels are added. */
  panel_set_expansion_from_list_data(C, new_panel);

  return new_panel;
}

void UI_list_panel_unique_str(Panel *panel, char *r_name)
{
  /* The panel sort-order will be unique for a specific panel type because the instanced
   * panel list is regenerated for every change in the data order / length. */
  BLI_snprintf(r_name, INSTANCED_PANEL_UNIQUE_STR_SIZE, "%d", panel->sortorder);
}

/**
 * Free a panel and its children. Custom data is shared by the panel and its children
 * and is freed by #UI_panels_free_instanced.
 *
 * \note The only panels that should need to be deleted at runtime are panels with the
 * #PANEL_TYPE_INSTANCED flag set.
 */
static void panel_delete(const bContext *C, ARegion *region, ListBase *panels, Panel *panel)
{
  /* Recursively delete children. */
  LISTBASE_FOREACH_MUTABLE (Panel *, child, &panel->children) {
    panel_delete(C, region, &panel->children, child);
  }
  BLI_freelistN(&panel->children);

  BLI_remlink(panels, panel);
  if (panel->activedata) {
    MEM_freeN(panel->activedata);
  }
  MEM_freeN(panel);
}

void UI_panels_free_instanced(const bContext *C, ARegion *region)
{
  /* Delete panels with the instanced flag. */
  LISTBASE_FOREACH_MUTABLE (Panel *, panel, &region->panels) {
    if ((panel->type != nullptr) && (panel->type->flag & PANEL_TYPE_INSTANCED)) {
      /* Make sure the panel's handler is removed before deleting it. */
      if (C != nullptr && panel->activedata != nullptr) {
        panel_activate_state(C, panel, PANEL_STATE_EXIT);
      }

      /* Free panel's custom data. */
      if (panel->runtime.custom_data_ptr != nullptr) {
        MEM_freeN(panel->runtime.custom_data_ptr);
      }

      /* Free the panel and its sub-panels. */
      panel_delete(C, region, &region->panels, panel);
    }
  }
}

bool UI_panel_list_matches_data(ARegion *region,
                                ListBase *data,
                                uiListPanelIDFromDataFunc panel_idname_func)
{
  /* Check for nullptr data. */
  int data_len = 0;
  Link *data_link = nullptr;
  if (data == nullptr) {
    data_len = 0;
    data_link = nullptr;
  }
  else {
    data_len = BLI_listbase_count(data);
    data_link = static_cast<Link *>(data->first);
  }

  int i = 0;
  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    if (panel->type != nullptr && panel->type->flag & PANEL_TYPE_INSTANCED) {
      /* The panels were reordered by drag and drop. */
      if (panel->flag & PNL_INSTANCED_LIST_ORDER_CHANGED) {
        return false;
      }

      /* We reached the last data item before the last instanced panel. */
      if (data_link == nullptr) {
        return false;
      }

      /* Check if the panel type matches the panel type from the data item. */
      char panel_idname[MAX_NAME];
      panel_idname_func(data_link, panel_idname);
      if (!STREQ(panel_idname, panel->type->idname)) {
        return false;
      }

      data_link = data_link->next;
      i++;
    }
  }

  /* If we didn't make it to the last list item, the panel list isn't complete. */
  if (i != data_len) {
    return false;
  }

  return true;
}

static void reorder_instanced_panel_list(bContext *C, ARegion *region, Panel *drag_panel)
{
  /* Without a type we cannot access the reorder callback. */
  if (drag_panel->type == nullptr) {
    return;
  }
  /* Don't reorder if this instanced panel doesn't support drag and drop reordering. */
  if (drag_panel->type->reorder == nullptr) {
    return;
  }

  char *context = nullptr;
  if (!UI_panel_category_is_visible(region)) {
    context = drag_panel->type->context;
  }

  /* Find how many instanced panels with this context string. */
  int list_panels_len = 0;
  int start_index = -1;
  LISTBASE_FOREACH (const Panel *, panel, &region->panels) {
    if (panel->type) {
      if (panel->type->flag & PANEL_TYPE_INSTANCED) {
        if (panel_type_context_poll(region, panel->type, context)) {
          if (panel == drag_panel) {
            BLI_assert(start_index == -1); /* This panel should only appear once. */
            start_index = list_panels_len;
          }
          list_panels_len++;
        }
      }
    }
  }
  BLI_assert(start_index != -1); /* The drag panel should definitely be in the list. */

  /* Sort the matching instanced panels by their display order. */
  PanelSort *panel_sort = static_cast<PanelSort *>(
      MEM_callocN(list_panels_len * sizeof(*panel_sort), __func__));
  PanelSort *sort_index = panel_sort;
  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    if (panel->type) {
      if (panel->type->flag & PANEL_TYPE_INSTANCED) {
        if (panel_type_context_poll(region, panel->type, context)) {
          sort_index->panel = panel;
          sort_index++;
        }
      }
    }
  }
  qsort(panel_sort, list_panels_len, sizeof(*panel_sort), compare_panel);

  /* Find how many of those panels are above this panel. */
  int move_to_index = 0;
  for (; move_to_index < list_panels_len; move_to_index++) {
    if (panel_sort[move_to_index].panel == drag_panel) {
      break;
    }
  }

  MEM_freeN(panel_sort);

  if (move_to_index == start_index) {
    /* In this case, the reorder was not changed, so don't do any updates or call the callback. */
    return;
  }

  /* Set the bit to tell the interface to instanced the list. */
  drag_panel->flag |= PNL_INSTANCED_LIST_ORDER_CHANGED;

  CTX_store_set(C, drag_panel->runtime.context);

  /* Finally, move this panel's list item to the new index in its list. */
  drag_panel->type->reorder(C, drag_panel, move_to_index);

  CTX_store_set(C, nullptr);
}

/**
 * Recursive implementation for #panel_set_expansion_from_list_data.
 *
 * \return Whether the closed flag for the panel or any sub-panels changed.
 */
static bool panel_set_expand_from_list_data_recursive(Panel *panel, short flag, short *flag_index)
{
  const bool open = (flag & (1 << *flag_index));
  bool changed = (open == UI_panel_is_closed(panel));

  SET_FLAG_FROM_TEST(panel->flag, !open, PNL_CLOSED);

  LISTBASE_FOREACH (Panel *, child, &panel->children) {
    *flag_index = *flag_index + 1;
    changed |= panel_set_expand_from_list_data_recursive(child, flag, flag_index);
  }
  return changed;
}

/**
 * Set the expansion of the panel and its sub-panels from the flag stored in the
 * corresponding list data. The flag has expansion stored in each bit in depth first order.
 */
static void panel_set_expansion_from_list_data(const bContext *C, Panel *panel)
{
  BLI_assert(panel->type != nullptr);
  BLI_assert(panel->type->flag & PANEL_TYPE_INSTANCED);
  if (panel->type->get_list_data_expand_flag == nullptr) {
    /* Instanced panel doesn't support loading expansion. */
    return;
  }

  const short expand_flag = panel->type->get_list_data_expand_flag(C, panel);
  short flag_index = 0;

  /* Start panel animation if the open state was changed. */
  if (panel_set_expand_from_list_data_recursive(panel, expand_flag, &flag_index)) {
    panel_activate_state(C, panel, PANEL_STATE_ANIMATION);
  }
}

/**
 * Set expansion based on the data for instanced panels.
 */
static void region_panels_set_expansion_from_list_data(const bContext *C, ARegion *region)
{
  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    if (panel->runtime_flag & PANEL_ACTIVE) {
      PanelType *panel_type = panel->type;
      if (panel_type != nullptr && panel->type->flag & PANEL_TYPE_INSTANCED) {
        panel_set_expansion_from_list_data(C, panel);
      }
    }
  }
}

/**
 * Recursive implementation for #set_panels_list_data_expand_flag.
 */
static void get_panel_expand_flag(const Panel *panel, short *flag, short *flag_index)
{
  const bool open = !(panel->flag & PNL_CLOSED);
  SET_FLAG_FROM_TEST(*flag, open, (1 << *flag_index));

  LISTBASE_FOREACH (const Panel *, child, &panel->children) {
    *flag_index = *flag_index + 1;
    get_panel_expand_flag(child, flag, flag_index);
  }
}

/**
 * Call the callback to store the panel and sub-panel expansion settings in the list item that
 * corresponds to each instanced panel.
 *
 * \note This needs to iterate through all of the region's panels because the panel with changed
 * expansion might have been the sub-panel of an instanced panel, meaning it might not know
 * which list item it corresponds to.
 */
static void set_panels_list_data_expand_flag(const bContext *C, const ARegion *region)
{
  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    PanelType *panel_type = panel->type;
    if (panel_type == nullptr) {
      continue;
    }

    /* Check for #PANEL_ACTIVE so we only set the expand flag for active panels. */
    if (panel_type->flag & PANEL_TYPE_INSTANCED && panel->runtime_flag & PANEL_ACTIVE) {
      short expand_flag;
      short flag_index = 0;
      get_panel_expand_flag(panel, &expand_flag, &flag_index);
      if (panel->type->set_list_data_expand_flag) {
        panel->type->set_list_data_expand_flag(C, panel, expand_flag);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Panels
 * \{ */

static bool panel_custom_data_active_get(const Panel *panel)
{
  /* The caller should make sure the panel is active and has a type. */
  BLI_assert(UI_panel_is_active(panel));
  BLI_assert(panel->type != nullptr);

  if (panel->type->active_property[0] != '\0') {
    PointerRNA *ptr = UI_panel_custom_data_get(panel);
    if (ptr != nullptr && !RNA_pointer_is_null(ptr)) {
      return RNA_boolean_get(ptr, panel->type->active_property);
    }
  }

  return false;
}

static void panel_custom_data_active_set(Panel *panel)
{
  /* Since the panel is interacted with, it should be active and have a type. */
  BLI_assert(UI_panel_is_active(panel));
  BLI_assert(panel->type != nullptr);

  if (panel->type->active_property[0] != '\0') {
    PointerRNA *ptr = UI_panel_custom_data_get(panel);
    BLI_assert(RNA_struct_find_property(ptr, panel->type->active_property) != nullptr);
    if (ptr != nullptr && !RNA_pointer_is_null(ptr)) {
      RNA_boolean_set(ptr, panel->type->active_property, true);
    }
  }
}

/**
 * Set flag state for a panel and its sub-panels.
 */
static void panel_set_flag_recursive(Panel *panel, short flag, bool value)
{
  SET_FLAG_FROM_TEST(panel->flag, value, flag);

  LISTBASE_FOREACH (Panel *, child, &panel->children) {
    panel_set_flag_recursive(child, flag, value);
  }
}

/**
 * Set runtime flag state for a panel and its sub-panels.
 */
static void panel_set_runtime_flag_recursive(Panel *panel, short flag, bool value)
{
  SET_FLAG_FROM_TEST(panel->runtime_flag, value, flag);

  LISTBASE_FOREACH (Panel *, sub_panel, &panel->children) {
    panel_set_runtime_flag_recursive(sub_panel, flag, value);
  }
}

static void panels_collapse_all(ARegion *region, const Panel *from_panel)
{
  const bool has_category_tabs = UI_panel_category_is_visible(region);
  const char *category = has_category_tabs ? UI_panel_category_active_get(region, false) : nullptr;
  const PanelType *from_pt = from_panel->type;

  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    PanelType *pt = panel->type;

    /* Close panels with headers in the same context. */
    if (pt && from_pt && !(pt->flag & PANEL_TYPE_NO_HEADER)) {
      if (!pt->context[0] || !from_pt->context[0] || STREQ(pt->context, from_pt->context)) {
        if ((panel->flag & PNL_PIN) || !category || !pt->category[0] ||
            STREQ(pt->category, category)) {
          panel->flag |= PNL_CLOSED;
        }
      }
    }
  }
}

static bool panel_type_context_poll(ARegion *region,
                                    const PanelType *panel_type,
                                    const char *context)
{
  if (!BLI_listbase_is_empty(&region->panels_category)) {
    return STREQ(panel_type->category, UI_panel_category_active_get(region, false));
  }

  if (panel_type->context[0] && STREQ(panel_type->context, context)) {
    return true;
  }

  return false;
}

Panel *UI_panel_find_by_type(ListBase *lb, const PanelType *pt)
{
  const char *idname = pt->idname;

  LISTBASE_FOREACH (Panel *, panel, lb) {
    if (STREQLEN(panel->panelname, idname, sizeof(panel->panelname))) {
      return panel;
    }
  }
  return nullptr;
}

Panel *UI_panel_begin(
    ARegion *region, ListBase *lb, uiBlock *block, PanelType *pt, Panel *panel, bool *r_open)
{
  Panel *panel_last;
  const char *drawname = CTX_IFACE_(pt->translation_context, pt->label);
  const char *idname = pt->idname;
  const bool newpanel = (panel == nullptr);

  if (newpanel) {
    panel = MEM_cnew<Panel>(__func__);
    panel->type = pt;
    STRNCPY(panel->panelname, idname);

    if (pt->flag & PANEL_TYPE_DEFAULT_CLOSED) {
      panel->flag |= PNL_CLOSED;
      panel->runtime_flag |= PANEL_WAS_CLOSED;
    }

    panel->ofsx = 0;
    panel->ofsy = 0;
    panel->sizex = 0;
    panel->sizey = 0;
    panel->blocksizex = 0;
    panel->blocksizey = 0;
    panel->runtime_flag |= PANEL_NEW_ADDED;

    BLI_addtail(lb, panel);
  }
  else {
    /* Panel already exists. */
    panel->type = pt;
  }

  panel->runtime.block = block;

  STRNCPY(panel->drawname, drawname);

  /* If a new panel is added, we insert it right after the panel that was last added.
   * This way new panels are inserted in the right place between versions. */
  for (panel_last = static_cast<Panel *>(lb->first); panel_last; panel_last = panel_last->next) {
    if (panel_last->runtime_flag & PANEL_LAST_ADDED) {
      BLI_remlink(lb, panel);
      BLI_insertlinkafter(lb, panel_last, panel);
      break;
    }
  }

  if (newpanel) {
    panel->sortorder = (panel_last) ? panel_last->sortorder + 1 : 0;

    LISTBASE_FOREACH (Panel *, panel_next, lb) {
      if (panel_next != panel && panel_next->sortorder >= panel->sortorder) {
        panel_next->sortorder++;
      }
    }
  }

  if (panel_last) {
    panel_last->runtime_flag &= ~PANEL_LAST_ADDED;
  }

  /* Assign the new panel to the block. */
  block->panel = panel;
  panel->runtime_flag |= PANEL_ACTIVE | PANEL_LAST_ADDED;
  if (region->alignment == RGN_ALIGN_FLOAT) {
    UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);
  }

  *r_open = !UI_panel_is_closed(panel);

  return panel;
}

void UI_panel_header_buttons_begin(Panel *panel)
{
  uiBlock *block = panel->runtime.block;

  ui_block_new_button_group(block, UI_BUTTON_GROUP_LOCK | UI_BUTTON_GROUP_PANEL_HEADER);
}

void UI_panel_header_buttons_end(Panel *panel)
{
  uiBlock *block = panel->runtime.block;

  /* A button group should always be created in #UI_panel_header_buttons_begin. */
  BLI_assert(!block->button_groups.is_empty());

  uiButtonGroup &button_group = block->button_groups.last();
  button_group.flag &= ~UI_BUTTON_GROUP_LOCK;

  /* Repurpose the first header button group if it is empty, in case the first button added to
   * the panel doesn't add a new group (if the button is created directly rather than through an
   * interface layout call). */
  if (block->button_groups.size() > 0) {
    button_group.flag &= ~UI_BUTTON_GROUP_PANEL_HEADER;
  }
  else {
    /* Always add a new button group. Although this may result in many empty groups, without it,
     * new buttons in the panel body not protected with a #ui_block_new_button_group call would
     * end up in the panel header group. */
    ui_block_new_button_group(block, (uiButtonGroupFlag)0);
  }
}

static float panel_region_offset_x_get(const ARegion *region)
{
  if (UI_panel_category_is_visible(region)) {
    if (RGN_ALIGN_ENUM_FROM_MASK(region->alignment) != RGN_ALIGN_RIGHT) {
      return UI_PANEL_CATEGORY_MARGIN_WIDTH;
    }
  }

  return 0.0f;
}

/**
 * Starting from the "block size" set in #UI_panel_end, calculate the full size
 * of the panel including the sub-panel headers and buttons.
 */
static void panel_calculate_size_recursive(ARegion *region, Panel *panel)
{
  int width = panel->blocksizex;
  int height = panel->blocksizey;

  LISTBASE_FOREACH (Panel *, child_panel, &panel->children) {
    if (child_panel->runtime_flag & PANEL_ACTIVE) {
      panel_calculate_size_recursive(region, child_panel);
      width = max_ii(width, child_panel->sizex);
      height += get_panel_real_size_y(child_panel);
    }
  }

  /* Update total panel size. */
  if (panel->runtime_flag & PANEL_NEW_ADDED) {
    panel->runtime_flag &= ~PANEL_NEW_ADDED;
    panel->sizex = width;
    panel->sizey = height;
  }
  else {
    const int old_sizex = panel->sizex, old_sizey = panel->sizey;
    const int old_region_ofsx = panel->runtime.region_ofsx;

    /* Update width/height if non-zero. */
    if (width != 0) {
      panel->sizex = width;
    }
    if (height != 0 || !UI_panel_is_closed(panel)) {
      panel->sizey = height;
    }

    /* Check if we need to do an animation. */
    if (panel->sizex != old_sizex || panel->sizey != old_sizey) {
      panel->runtime_flag |= PANEL_ANIM_ALIGN;
      panel->ofsy += old_sizey - panel->sizey;
    }

    panel->runtime.region_ofsx = panel_region_offset_x_get(region);
    if (old_region_ofsx != panel->runtime.region_ofsx) {
      panel->runtime_flag |= PANEL_ANIM_ALIGN;
    }
  }
}

void UI_panel_end(Panel *panel, int width, int height)
{
  /* Store the size of the buttons layout in the panel. The actual panel size
   * (including sub-panels) is calculated in #UI_panels_end. */
  panel->blocksizex = width;
  panel->blocksizey = height;
}

static void ui_offset_panel_block(uiBlock *block)
{
  const uiStyle *style = UI_style_get_dpi();

  /* Compute bounds and offset. */
  ui_block_bounds_calc(block);

  const int ofsy = block->panel->sizey - style->panelspace;

  LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
    but->rect.ymin += ofsy;
    but->rect.ymax += ofsy;
  }

  block->rect.xmax = block->panel->sizex;
  block->rect.ymax = block->panel->sizey;
  block->rect.xmin = block->rect.ymin = 0.0;
}

void ui_panel_tag_search_filter_match(Panel *panel)
{
  panel->runtime_flag |= PANEL_SEARCH_FILTER_MATCH;
}

static void panel_matches_search_filter_recursive(const Panel *panel, bool *filter_matches)
{
  *filter_matches |= bool(panel->runtime_flag & PANEL_SEARCH_FILTER_MATCH);

  /* If the panel has no match we need to make sure that its children are too. */
  if (!*filter_matches) {
    LISTBASE_FOREACH (const Panel *, child_panel, &panel->children) {
      panel_matches_search_filter_recursive(child_panel, filter_matches);
    }
  }
}

bool UI_panel_matches_search_filter(const Panel *panel)
{
  bool search_filter_matches = false;
  panel_matches_search_filter_recursive(panel, &search_filter_matches);
  return search_filter_matches;
}

/**
 * Set the flag telling the panel to use its search result status for its expansion.
 */
static void panel_set_expansion_from_search_filter_recursive(const bContext *C,
                                                             Panel *panel,
                                                             const bool use_search_closed)
{
  /* This has to run on inactive panels that may not have a type,
   * but we can prevent running on header-less panels in some cases. */
  if (panel->type == nullptr || !(panel->type->flag & PANEL_TYPE_NO_HEADER)) {
    SET_FLAG_FROM_TEST(panel->runtime_flag, use_search_closed, PANEL_USE_CLOSED_FROM_SEARCH);
  }

  LISTBASE_FOREACH (Panel *, child_panel, &panel->children) {
    /* Don't check if the sub-panel is active, otherwise the
     * expansion won't be reset when the parent is closed. */
    panel_set_expansion_from_search_filter_recursive(C, child_panel, use_search_closed);
  }
}

/**
 * Set the flag telling every panel to override its expansion with its search result status.
 */
static void region_panels_set_expansion_from_search_filter(const bContext *C,
                                                           ARegion *region,
                                                           const bool use_search_closed)
{
  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    /* Don't check if the panel is active, otherwise the expansion won't
     * be correct when switching back to tab after exiting search. */
    panel_set_expansion_from_search_filter_recursive(C, panel, use_search_closed);
  }
  set_panels_list_data_expand_flag(C, region);
}

/**
 * Hide buttons in invisible layouts, which are created because buttons must be
 * added for all panels in order to search, even panels that will end up closed.
 */
static void panel_remove_invisible_layouts_recursive(Panel *panel, const Panel *parent_panel)
{
  uiBlock *block = panel->runtime.block;
  BLI_assert(block != nullptr);
  BLI_assert(block->active);
  if (parent_panel != nullptr && UI_panel_is_closed(parent_panel)) {
    /* The parent panel is closed, so this panel can be completely removed. */
    UI_block_set_search_only(block, true);
    LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
      but->flag |= UI_HIDDEN;
    }
  }
  else if (UI_panel_is_closed(panel)) {
    /* If sub-panels have no search results but the parent panel does, then the parent panel open
     * and the sub-panels will close. In that case there must be a way to hide the buttons in the
     * panel but keep the header buttons. */
    for (const uiButtonGroup &button_group : block->button_groups) {
      if (button_group.flag & UI_BUTTON_GROUP_PANEL_HEADER) {
        continue;
      }
      for (uiBut *but : button_group.buttons) {
        but->flag |= UI_HIDDEN;
      }
    }
  }

  LISTBASE_FOREACH (Panel *, child_panel, &panel->children) {
    if (child_panel->runtime_flag & PANEL_ACTIVE) {
      BLI_assert(child_panel->runtime.block != nullptr);
      panel_remove_invisible_layouts_recursive(child_panel, panel);
    }
  }
}

static void region_panels_remove_invisible_layouts(ARegion *region)
{
  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    if (panel->runtime_flag & PANEL_ACTIVE) {
      BLI_assert(panel->runtime.block != nullptr);
      panel_remove_invisible_layouts_recursive(panel, nullptr);
    }
  }
}

bool UI_panel_is_closed(const Panel *panel)
{
  /* Header-less panels can never be closed, otherwise they could disappear. */
  if (panel->type && panel->type->flag & PANEL_TYPE_NO_HEADER) {
    return false;
  }

  if (panel->runtime_flag & PANEL_USE_CLOSED_FROM_SEARCH) {
    return !UI_panel_matches_search_filter(panel);
  }

  return panel->flag & PNL_CLOSED;
}

bool UI_panel_is_active(const Panel *panel)
{
  return panel->runtime_flag & PANEL_ACTIVE;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drawing
 * \{ */

void UI_panels_draw(const bContext *C, ARegion *region)
{
  /* Draw in reverse order, because #uiBlocks are added in reverse order
   * and we need child panels to draw on top. */
  LISTBASE_FOREACH_BACKWARD (uiBlock *, block, &region->uiblocks) {
    if (block->active && block->panel && !UI_panel_is_dragging(block->panel) &&
        !UI_block_is_search_only(block))
    {
      UI_block_draw(C, block);
    }
  }

  LISTBASE_FOREACH_BACKWARD (uiBlock *, block, &region->uiblocks) {
    if (block->active && block->panel && UI_panel_is_dragging(block->panel) &&
        !UI_block_is_search_only(block))
    {
      UI_block_draw(C, block);
    }
  }
}

#define PNL_ICON UI_UNIT_X /* Could be UI_UNIT_Y too. */

void UI_panel_label_offset(const uiBlock *block, int *r_x, int *r_y)
{
  Panel *panel = block->panel;
  const bool is_subpanel = (panel->type && panel->type->parent);

  *r_x = UI_UNIT_X * 1.0f;
  *r_y = UI_UNIT_Y * 1.5f;

  if (is_subpanel) {
    *r_x += (0.7f * UI_UNIT_X);
  }
}

static void panel_title_color_get(const Panel *panel,
                                  const bool show_background,
                                  const bool region_search_filter_active,
                                  uchar r_color[4])
{
  if (!show_background) {
    /* Use menu colors for floating panels. */
    bTheme *btheme = UI_GetTheme();
    const uiWidgetColors *wcol = &btheme->tui.wcol_menu_back;
    copy_v4_v4_uchar(r_color, (const uchar *)wcol->text);
    return;
  }

  const bool search_match = UI_panel_matches_search_filter(panel);

  UI_GetThemeColor4ubv(TH_TITLE, r_color);
  if (region_search_filter_active && !search_match) {
    r_color[0] *= 0.5;
    r_color[1] *= 0.5;
    r_color[2] *= 0.5;
  }
}

static void panel_draw_highlight_border(const Panel *panel,
                                        const rcti *rect,
                                        const rcti *header_rect)
{
  const bool is_subpanel = panel->type->parent != nullptr;
  if (is_subpanel) {
    return;
  }

  const bTheme *btheme = UI_GetTheme();
  const float aspect = panel->runtime.block->aspect;
  const float radius = (btheme->tui.panel_roundness * U.widget_unit * 0.5f) / aspect;
  UI_draw_roundbox_corner_set(UI_CNR_ALL);

  rctf box_rect;
  box_rect.xmin = rect->xmin;
  box_rect.xmax = rect->xmax;
  box_rect.ymin = UI_panel_is_closed(panel) ? header_rect->ymin : rect->ymin;
  box_rect.ymax = header_rect->ymax;

  float color[4];
  UI_GetThemeColor4fv(TH_SELECT_ACTIVE, color);
  UI_draw_roundbox_4fv(&box_rect, false, radius, color);
}

static void panel_draw_aligned_widgets(const uiStyle *style,
                                       const Panel *panel,
                                       const rcti *header_rect,
                                       const float aspect,
                                       const bool show_pin,
                                       const bool show_background,
                                       const bool region_search_filter_active)
{
  const bool is_subpanel = panel->type->parent != nullptr;
  const uiFontStyle *fontstyle = (is_subpanel) ? &style->widgetlabel : &style->paneltitle;

  const int header_height = BLI_rcti_size_y(header_rect);
  const int scaled_unit = round_fl_to_int(UI_UNIT_X / aspect);

  /* Offset triangle and text to the right for sub-panels. */
  rcti widget_rect;
  widget_rect.xmin = header_rect->xmin + (is_subpanel ? scaled_unit * 0.7f : 0);
  widget_rect.xmax = header_rect->xmax;
  widget_rect.ymin = header_rect->ymin;
  widget_rect.ymax = header_rect->ymax;

  uchar title_color[4];
  panel_title_color_get(panel, show_background, region_search_filter_active, title_color);
  title_color[3] = 255;

  /* Draw collapse icon. */
  {
    const float size_y = BLI_rcti_size_y(&widget_rect);
    GPU_blend(GPU_BLEND_ALPHA);
    UI_icon_draw_ex(widget_rect.xmin + size_y * 0.2f,
                    widget_rect.ymin + size_y * 0.2f,
                    UI_panel_is_closed(panel) ? ICON_RIGHTARROW : ICON_DOWNARROW_HLT,
                    aspect * UI_INV_SCALE_FAC,
                    0.7f,
                    0.0f,
                    title_color,
                    false,
                    UI_NO_ICON_OVERLAY_TEXT);
    GPU_blend(GPU_BLEND_NONE);
  }

  /* Draw text label. */
  if (panel->drawname[0] != '\0') {
    rcti title_rect;
    title_rect.xmin = widget_rect.xmin + (panel->labelofs / aspect) + scaled_unit * 1.1f;
    title_rect.xmax = widget_rect.xmax;
    title_rect.ymin = widget_rect.ymin - 2.0f / aspect;
    title_rect.ymax = widget_rect.ymax;

    uiFontStyleDraw_Params params{};
    params.align = UI_STYLE_TEXT_LEFT;
    UI_fontstyle_draw(
        fontstyle, &title_rect, panel->drawname, sizeof(panel->drawname), title_color, &params);
  }

  /* Draw the pin icon. */
  if (show_pin && (panel->flag & PNL_PIN)) {
    GPU_blend(GPU_BLEND_ALPHA);
    UI_icon_draw_ex(widget_rect.xmax - scaled_unit * 2.2f,
                    widget_rect.ymin + 5.0f / aspect,
                    ICON_PINNED,
                    aspect * UI_INV_SCALE_FAC,
                    1.0f,
                    0.0f,
                    title_color,
                    false,
                    UI_NO_ICON_OVERLAY_TEXT);
    GPU_blend(GPU_BLEND_NONE);
  }

  /* Draw drag widget. */
  if (!is_subpanel && show_background) {
    const int drag_widget_size = header_height * 0.7f;
    GPU_matrix_push();
    /* The magic numbers here center the widget vertically and offset it to the left.
     * Currently this depends on the height of the header, although it could be independent. */
    GPU_matrix_translate_2f(widget_rect.xmax - scaled_unit * 1.15,
                            widget_rect.ymin + (header_height - drag_widget_size) * 0.5f);

    const int col_tint = 84;
    float color_high[4], color_dark[4];
    UI_GetThemeColorShade4fv(TH_PANEL_HEADER, col_tint, color_high);
    UI_GetThemeColorShade4fv(TH_PANEL_BACK, -col_tint, color_dark);

    GPUBatch *batch = GPU_batch_preset_panel_drag_widget(
        U.pixelsize, color_high, color_dark, drag_widget_size);
    GPU_batch_program_set_builtin(batch, GPU_SHADER_3D_FLAT_COLOR);
    GPU_batch_draw(batch);
    GPU_matrix_pop();
  }
}

static void panel_draw_aligned_backdrop(const Panel *panel,
                                        const rcti *rect,
                                        const rcti *header_rect)
{
  const bool is_subpanel = panel->type->parent != nullptr;
  const bool is_open = !UI_panel_is_closed(panel);

  if (is_subpanel && !is_open) {
    return;
  }

  const bTheme *btheme = UI_GetTheme();
  const float aspect = panel->runtime.block->aspect;
  const float radius = btheme->tui.panel_roundness * U.widget_unit * 0.5f / aspect;

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  GPU_blend(GPU_BLEND_ALPHA);

  /* Panel backdrop. */
  if (is_open || panel->type->flag & PANEL_TYPE_NO_HEADER) {
    float panel_backcolor[4];
    UI_draw_roundbox_corner_set(is_open ? UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT : UI_CNR_ALL);
    UI_GetThemeColor4fv((is_subpanel ? TH_PANEL_SUB_BACK : TH_PANEL_BACK), panel_backcolor);

    rctf box_rect;
    box_rect.xmin = rect->xmin;
    box_rect.xmax = rect->xmax;
    box_rect.ymin = rect->ymin;
    box_rect.ymax = rect->ymax;
    UI_draw_roundbox_4fv(&box_rect, true, radius, panel_backcolor);
  }

  /* Panel header backdrops for non sub-panels. */
  if (!is_subpanel) {
    float panel_headercolor[4];
    UI_GetThemeColor4fv(UI_panel_matches_search_filter(panel) ? TH_MATCH : TH_PANEL_HEADER,
                        panel_headercolor);
    UI_draw_roundbox_corner_set(is_open ? UI_CNR_TOP_RIGHT | UI_CNR_TOP_LEFT : UI_CNR_ALL);

    /* Change the width a little bit to line up with the sides. */
    rctf box_rect;
    box_rect.xmin = rect->xmin;
    box_rect.xmax = rect->xmax;
    box_rect.ymin = header_rect->ymin;
    box_rect.ymax = header_rect->ymax;
    UI_draw_roundbox_4fv(&box_rect, true, radius, panel_headercolor);
  }

  GPU_blend(GPU_BLEND_NONE);
  immUnbindProgram();
}

void ui_draw_aligned_panel(const uiStyle *style,
                           const uiBlock *block,
                           const rcti *rect,
                           const bool show_pin,
                           const bool show_background,
                           const bool region_search_filter_active)
{
  const Panel *panel = block->panel;

  /* Add 0.001f to prevent flicker from float inaccuracy. */
  const rcti header_rect = {
      rect->xmin,
      rect->xmax,
      rect->ymax,
      rect->ymax + int(floor(PNL_HEADER / block->aspect + 0.001f)),
  };

  if (show_background) {
    panel_draw_aligned_backdrop(panel, rect, &header_rect);
  }

  /* Draw the widgets and text in the panel header. */
  if (!(panel->type->flag & PANEL_TYPE_NO_HEADER)) {
    panel_draw_aligned_widgets(style,
                               panel,
                               &header_rect,
                               block->aspect,
                               show_pin,
                               show_background,
                               region_search_filter_active);
  }

  if (panel_custom_data_active_get(panel)) {
    panel_draw_highlight_border(panel, rect, &header_rect);
  }
}

bool UI_panel_should_show_background(const ARegion *region, const PanelType *panel_type)
{
  if (region->alignment == RGN_ALIGN_FLOAT) {
    return false;
  }

  if (panel_type && panel_type->flag & PANEL_TYPE_NO_HEADER) {
    if (region->regiontype == RGN_TYPE_TOOLS) {
      /* We never want a background around active tools. */
      return false;
    }
    /* Without a header there is no background except for region overlap. */
    return region->overlap != 0;
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Category Drawing (Tabs)
 * \{ */

#define TABS_PADDING_BETWEEN_FACTOR 4.0f
#define TABS_PADDING_TEXT_FACTOR 6.0f

void UI_panel_category_draw_all(ARegion *region, const char *category_id_active)
{
  // #define USE_FLAT_INACTIVE
  const bool is_left = RGN_ALIGN_ENUM_FROM_MASK(region->alignment) != RGN_ALIGN_RIGHT;
  View2D *v2d = &region->v2d;
  const uiStyle *style = UI_style_get();
  const uiFontStyle *fstyle = &style->widget;
  const int fontid = fstyle->uifont_id;
  float fstyle_points = fstyle->points;
  const float aspect = ((uiBlock *)region->uiblocks.first)->aspect;
  const float zoom = 1.0f / aspect;
  const int px = U.pixelsize;
  const int category_tabs_width = round_fl_to_int(UI_PANEL_CATEGORY_MARGIN_WIDTH * zoom);
  const float dpi_fac = UI_SCALE_FAC;
  /* Padding of tabs around text. */
  const int tab_v_pad_text = round_fl_to_int(TABS_PADDING_TEXT_FACTOR * dpi_fac * zoom) + 2 * px;
  /* Padding between tabs. */
  const int tab_v_pad = round_fl_to_int(TABS_PADDING_BETWEEN_FACTOR * dpi_fac * zoom);
  bTheme *btheme = UI_GetTheme();
  const float tab_curve_radius = btheme->tui.wcol_tab.roundness * U.widget_unit * zoom;
  const int roundboxtype = is_left ? (UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT) :
                                     (UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT);
  bool is_alpha;
#ifdef USE_FLAT_INACTIVE
  bool is_active_prev = false;
#endif
  /* Same for all tabs. */
  /* Intentionally don't scale by 'px'. */
  const int rct_xmin = is_left ? v2d->mask.xmin + 3 : (v2d->mask.xmax - category_tabs_width);
  const int rct_xmax = is_left ? v2d->mask.xmin + category_tabs_width : (v2d->mask.xmax - 3);
  const int text_v_ofs = (rct_xmax - rct_xmin) * 0.3f;

  int y_ofs = tab_v_pad;

  /* Primary theme colors. */
  uchar theme_col_back[4];
  uchar theme_col_text[3];
  uchar theme_col_text_hi[3];

  /* Tab colors. */
  uchar theme_col_tab_bg[4];
  float theme_col_tab_active[4];
  float theme_col_tab_inactive[4];
  float theme_col_tab_outline[4];

  UI_GetThemeColor4ubv(TH_BACK, theme_col_back);
  UI_GetThemeColor3ubv(TH_TEXT, theme_col_text);
  UI_GetThemeColor3ubv(TH_TEXT_HI, theme_col_text_hi);

  UI_GetThemeColor4ubv(TH_TAB_BACK, theme_col_tab_bg);
  UI_GetThemeColor4fv(TH_TAB_ACTIVE, theme_col_tab_active);
  UI_GetThemeColor4fv(TH_TAB_INACTIVE, theme_col_tab_inactive);
  UI_GetThemeColor4fv(TH_TAB_OUTLINE, theme_col_tab_outline);

  is_alpha = (region->overlap && (theme_col_back[3] != 255));

  BLF_enable(fontid, BLF_ROTATION);
  BLF_rotation(fontid, is_left ? M_PI_2 : -M_PI_2);
  ui_fontscale(&fstyle_points, aspect);
  BLF_size(fontid, fstyle_points * UI_SCALE_FAC);

  /* Check the region type supports categories to avoid an assert
   * for showing 3D view panels in the properties space. */
  if ((1 << region->regiontype) & RGN_TYPE_HAS_CATEGORY_MASK) {
    BLI_assert(UI_panel_category_is_visible(region));
  }

  /* Calculate tab rectangle for each panel. */
  LISTBASE_FOREACH (PanelCategoryDyn *, pc_dyn, &region->panels_category) {
    rcti *rct = &pc_dyn->rect;
    const char *category_id = pc_dyn->idname;
    const char *category_id_draw = IFACE_(category_id);
    const int category_width = BLF_width(fontid, category_id_draw, BLF_DRAW_STR_DUMMY_MAX);

    rct->xmin = rct_xmin;
    rct->xmax = rct_xmax;

    rct->ymin = v2d->mask.ymax - (y_ofs + category_width + (tab_v_pad_text * 2));
    rct->ymax = v2d->mask.ymax - (y_ofs);

    y_ofs += category_width + tab_v_pad + (tab_v_pad_text * 2);
  }

  const int max_scroll = max_ii(y_ofs - BLI_rcti_size_y(&v2d->mask), 0);
  const int scroll = clamp_i(region->category_scroll, 0, max_scroll);
  region->category_scroll = scroll;
  LISTBASE_FOREACH (PanelCategoryDyn *, pc_dyn, &region->panels_category) {
    rcti *rct = &pc_dyn->rect;
    rct->ymin += scroll;
    rct->ymax += scroll;
  }

  /* Begin drawing. */
  GPU_line_smooth(true);

  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  /* Draw the background. */
  if (is_alpha) {
    GPU_blend(GPU_BLEND_ALPHA);
    immUniformColor4ubv(theme_col_tab_bg);
  }
  else {
    immUniformColor3ubv(theme_col_tab_bg);
  }

  if (is_left) {
    immRecti(
        pos, v2d->mask.xmin, v2d->mask.ymin, v2d->mask.xmin + category_tabs_width, v2d->mask.ymax);
  }
  else {
    immRecti(
        pos, v2d->mask.xmax - category_tabs_width, v2d->mask.ymin, v2d->mask.xmax, v2d->mask.ymax);
  }

  if (is_alpha) {
    GPU_blend(GPU_BLEND_NONE);
  }

  immUnbindProgram();

  LISTBASE_FOREACH (PanelCategoryDyn *, pc_dyn, &region->panels_category) {
    const rcti *rct = &pc_dyn->rect;
    if (rct->ymin > v2d->mask.ymax) {
      /* Scrolled outside the top of the view, check the next tab. */
      continue;
    }
    if (rct->ymax < v2d->mask.ymin) {
      /* Scrolled past visible bounds, no need to draw other tabs. */
      break;
    }
    const char *category_id = pc_dyn->idname;
    const char *category_id_draw = IFACE_(category_id);
    size_t category_draw_len = BLF_DRAW_STR_DUMMY_MAX;
    const bool is_active = STREQ(category_id, category_id_active);

    GPU_blend(GPU_BLEND_ALPHA);

#ifdef USE_FLAT_INACTIVE
    /* Draw line between inactive tabs. */
    if (is_active == false && is_active_prev == false && pc_dyn->prev) {
      pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
      immUniformColor3fvAlpha(theme_col_tab_outline, 0.3f);
      immRecti(pos,
               is_left ? v2d->mask.xmin + (category_tabs_width / 5) :
                         v2d->mask.xmax - (category_tabs_width / 5),
               rct->ymax + px,
               is_left ? (v2d->mask.xmin + category_tabs_width) - (category_tabs_width / 5) :
                         (v2d->mask.xmax - category_tabs_width) + (category_tabs_width / 5),
               rct->ymax + (px * 3));
      immUnbindProgram();
    }

    is_active_prev = is_active;

    if (is_active)
#endif
    {
      /* Draw filled rectangle and outline for tab. */
      UI_draw_roundbox_corner_set(roundboxtype);
      rctf box_rect;
      box_rect.xmin = rct->xmin;
      box_rect.xmax = rct->xmax;
      box_rect.ymin = rct->ymin;
      box_rect.ymax = rct->ymax;

      UI_draw_roundbox_4fv(&box_rect,
                           true,
                           tab_curve_radius,
                           is_active ? theme_col_tab_active : theme_col_tab_inactive);
      UI_draw_roundbox_4fv(&box_rect, false, tab_curve_radius, theme_col_tab_outline);

      /* Disguise the outline on one side to join the tab to the panel. */
      pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

      immUniformColor4fv(is_active ? theme_col_tab_active : theme_col_tab_inactive);
      immRecti(pos,
               is_left ? rct->xmax - px : rct->xmin,
               rct->ymin + px,
               is_left ? rct->xmax : rct->xmin + px,
               rct->ymax - px);
      immUnbindProgram();
    }

    /* Tab titles. */

    BLF_position(fontid,
                 is_left ? rct->xmax - text_v_ofs : rct->xmin + text_v_ofs,
                 is_left ? rct->ymin + tab_v_pad_text : rct->ymax - tab_v_pad_text,
                 0.0f);
    BLF_color3ubv(fontid, is_active ? theme_col_text_hi : theme_col_text);
    BLF_draw(fontid, category_id_draw, category_draw_len);

    GPU_blend(GPU_BLEND_NONE);

    /* Not essential, but allows events to be handled right up to the region edge (#38171). */
    if (is_left) {
      pc_dyn->rect.xmin = v2d->mask.xmin;
    }
    else {
      pc_dyn->rect.xmax = v2d->mask.xmax;
    }
  }

  GPU_line_smooth(false);

  BLF_disable(fontid, BLF_ROTATION);
}

#undef TABS_PADDING_BETWEEN_FACTOR
#undef TABS_PADDING_TEXT_FACTOR

/** \} */

/* -------------------------------------------------------------------- */
/** \name Panel Alignment
 * \{ */

static int get_panel_size_y(const Panel *panel)
{
  if (panel->type && (panel->type->flag & PANEL_TYPE_NO_HEADER)) {
    return panel->sizey;
  }

  return PNL_HEADER + panel->sizey;
}

static int get_panel_real_size_y(const Panel *panel)
{
  const int sizey = UI_panel_is_closed(panel) ? 0 : panel->sizey;

  if (panel->type && (panel->type->flag & PANEL_TYPE_NO_HEADER)) {
    return sizey;
  }

  return PNL_HEADER + sizey;
}

int UI_panel_size_y(const Panel *panel)
{
  return get_panel_real_size_y(panel);
}

/**
 * This function is needed because #uiBlock and Panel itself don't
 * change #Panel.sizey or location when closed.
 */
static int get_panel_real_ofsy(Panel *panel)
{
  if (UI_panel_is_closed(panel)) {
    return panel->ofsy + panel->sizey;
  }
  return panel->ofsy;
}

bool UI_panel_is_dragging(const Panel *panel)
{
  return panel->runtime_flag & PANEL_IS_DRAG_DROP;
}

/**
 * \note about sorting:
 * The #Panel.sortorder has a lower value for new panels being added.
 * however, that only works to insert a single panel, when more new panels get
 * added the coordinates of existing panels and the previously stored to-be-inserted
 * panels do not match for sorting.
 */

static int find_highest_panel(const void *a, const void *b)
{
  const Panel *panel_a = ((PanelSort *)a)->panel;
  const Panel *panel_b = ((PanelSort *)b)->panel;

  /* Stick uppermost header-less panels to the top of the region -
   * prevent them from being sorted (multiple header-less panels have to be sorted though). */
  if (panel_a->type->flag & PANEL_TYPE_NO_HEADER && panel_b->type->flag & PANEL_TYPE_NO_HEADER) {
    /* Pass the no-header checks and check for `ofsy` and #Panel.sortorder below. */
  }
  else if (panel_a->type->flag & PANEL_TYPE_NO_HEADER) {
    return -1;
  }
  else if (panel_b->type->flag & PANEL_TYPE_NO_HEADER) {
    return 1;
  }

  if (panel_a->ofsy + panel_a->sizey < panel_b->ofsy + panel_b->sizey) {
    return 1;
  }
  if (panel_a->ofsy + panel_a->sizey > panel_b->ofsy + panel_b->sizey) {
    return -1;
  }
  if (panel_a->sortorder > panel_b->sortorder) {
    return 1;
  }
  if (panel_a->sortorder < panel_b->sortorder) {
    return -1;
  }

  return 0;
}

static int compare_panel(const void *a, const void *b)
{
  const Panel *panel_a = ((PanelSort *)a)->panel;
  const Panel *panel_b = ((PanelSort *)b)->panel;

  if (panel_a->sortorder > panel_b->sortorder) {
    return 1;
  }
  if (panel_a->sortorder < panel_b->sortorder) {
    return -1;
  }

  return 0;
}

static void align_sub_panels(Panel *panel)
{
  /* Position sub panels. */
  int ofsy = panel->ofsy + panel->sizey - panel->blocksizey;

  LISTBASE_FOREACH (Panel *, pachild, &panel->children) {
    if (pachild->runtime_flag & PANEL_ACTIVE) {
      pachild->ofsx = panel->ofsx;
      pachild->ofsy = ofsy - get_panel_size_y(pachild);
      ofsy -= get_panel_real_size_y(pachild);

      if (pachild->children.first) {
        align_sub_panels(pachild);
      }
    }
  }
}

/**
 * Calculate the position and order of panels as they are opened, closed, and dragged.
 */
static bool uiAlignPanelStep(ARegion *region, const float factor, const bool drag)
{
  /* Count active panels. */
  int active_panels_len = 0;
  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    if (panel->runtime_flag & PANEL_ACTIVE) {
      /* These panels should have types since they are currently displayed to the user. */
      BLI_assert(panel->type != nullptr);
      active_panels_len++;
    }
  }
  if (active_panels_len == 0) {
    return false;
  }

  /* Sort panels. */
  PanelSort *panel_sort = static_cast<PanelSort *>(
      MEM_mallocN(sizeof(PanelSort) * active_panels_len, __func__));
  {
    PanelSort *ps = panel_sort;
    LISTBASE_FOREACH (Panel *, panel, &region->panels) {
      if (panel->runtime_flag & PANEL_ACTIVE) {
        ps->panel = panel;
        ps++;
      }
    }
  }

  if (drag) {
    /* While dragging, sort based on location and update #Panel.sortorder. */
    qsort(panel_sort, active_panels_len, sizeof(PanelSort), find_highest_panel);
    for (int i = 0; i < active_panels_len; i++) {
      panel_sort[i].panel->sortorder = i;
    }
  }
  else {
    /* Otherwise use #Panel.sortorder. */
    qsort(panel_sort, active_panels_len, sizeof(PanelSort), compare_panel);
  }

  /* X offset. */
  const int region_offset_x = panel_region_offset_x_get(region);
  for (int i = 0; i < active_panels_len; i++) {
    PanelSort *ps = &panel_sort[i];
    const bool show_background = UI_panel_should_show_background(region, ps->panel->type);
    ps->panel->runtime.region_ofsx = region_offset_x;
    ps->new_offset_x = region_offset_x + (show_background ? UI_PANEL_MARGIN_X : 0);
  }

  /* Y offset. */
  for (int i = 0, y = 0; i < active_panels_len; i++) {
    PanelSort *ps = &panel_sort[i];
    const bool show_background = UI_panel_should_show_background(region, ps->panel->type);

    y -= get_panel_real_size_y(ps->panel);

    /* Separate panel boxes a bit further (if they are drawn). */
    if (show_background) {
      y -= UI_PANEL_MARGIN_Y;
    }
    ps->new_offset_y = y;
    /* The header still draws offset by the size of closed panels, so apply the offset here. */
    if (UI_panel_is_closed(ps->panel)) {
      panel_sort[i].new_offset_y -= ps->panel->sizey;
    }
  }

  /* Interpolate based on the input factor. */
  bool changed = false;
  for (int i = 0; i < active_panels_len; i++) {
    PanelSort *ps = &panel_sort[i];
    if (ps->panel->flag & PNL_SELECT) {
      continue;
    }

    if (ps->new_offset_x != ps->panel->ofsx) {
      const float x = interpf(float(ps->new_offset_x), float(ps->panel->ofsx), factor);
      ps->panel->ofsx = round_fl_to_int(x);
      changed = true;
    }
    if (ps->new_offset_y != ps->panel->ofsy) {
      const float y = interpf(float(ps->new_offset_y), float(ps->panel->ofsy), factor);
      ps->panel->ofsy = round_fl_to_int(y);
      changed = true;
    }
  }

  /* Set locations for tabbed and sub panels. */
  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    if (panel->runtime_flag & PANEL_ACTIVE) {
      if (panel->children.first) {
        align_sub_panels(panel);
      }
    }
  }

  MEM_freeN(panel_sort);

  return changed;
}

static void ui_panels_size(ARegion *region, int *r_x, int *r_y)
{
  int sizex = 0;
  int sizey = 0;
  bool has_panel_with_background = false;

  /* Compute size taken up by panels, for setting in view2d. */
  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    if (panel->runtime_flag & PANEL_ACTIVE) {
      const int pa_sizex = panel->ofsx + panel->sizex;
      const int pa_sizey = get_panel_real_ofsy(panel);

      sizex = max_ii(sizex, pa_sizex);
      sizey = min_ii(sizey, pa_sizey);
      if (UI_panel_should_show_background(region, panel->type)) {
        has_panel_with_background = true;
      }
    }
  }

  if (sizex == 0) {
    sizex = UI_PANEL_WIDTH;
  }
  if (sizey == 0) {
    sizey = -UI_PANEL_WIDTH;
  }
  /* Extra margin after the list so the view scrolls a few pixels further than the panel border.
   * Also makes the bottom match the top margin. */
  if (has_panel_with_background) {
    sizey -= UI_PANEL_MARGIN_Y;
  }

  *r_x = sizex;
  *r_y = sizey;
}

static void ui_do_animate(bContext *C, Panel *panel)
{
  uiHandlePanelData *data = static_cast<uiHandlePanelData *>(panel->activedata);
  ARegion *region = CTX_wm_region(C);

  float fac = (PIL_check_seconds_timer() - data->starttime) / ANIMATION_TIME;
  fac = min_ff(sqrtf(fac), 1.0f);

  if (uiAlignPanelStep(region, fac, false)) {
    ED_region_tag_redraw(region);
  }
  else {
    if (UI_panel_is_dragging(panel)) {
      /* NOTE: doing this in #panel_activate_state would require
       * removing `const` for context in many other places. */
      reorder_instanced_panel_list(C, region, panel);
    }

    panel_activate_state(C, panel, PANEL_STATE_EXIT);
  }
}

static void panels_layout_begin_clear_flags(ListBase *lb)
{
  LISTBASE_FOREACH (Panel *, panel, lb) {
    /* Flags to copy over to the next layout pass. */
    const short flag_copy = PANEL_USE_CLOSED_FROM_SEARCH | PANEL_IS_DRAG_DROP;

    const bool was_active = panel->runtime_flag & PANEL_ACTIVE;
    const bool was_closed = UI_panel_is_closed(panel);
    panel->runtime_flag &= flag_copy;
    SET_FLAG_FROM_TEST(panel->runtime_flag, was_active, PANEL_WAS_ACTIVE);
    SET_FLAG_FROM_TEST(panel->runtime_flag, was_closed, PANEL_WAS_CLOSED);

    panels_layout_begin_clear_flags(&panel->children);
  }
}

void UI_panels_begin(const bContext * /*C*/, ARegion *region)
{
  /* Set all panels as inactive, so that at the end we know which ones were used. Also
   * clear other flags so we know later that their values were set for the current redraw. */
  panels_layout_begin_clear_flags(&region->panels);
}

void UI_panels_end(const bContext *C, ARegion *region, int *r_x, int *r_y)
{
  ScrArea *area = CTX_wm_area(C);

  region_panels_set_expansion_from_list_data(C, region);

  const bool region_search_filter_active = region->flag & RGN_FLAG_SEARCH_FILTER_ACTIVE;

  if (properties_space_needs_realign(area, region)) {
    region_panels_set_expansion_from_search_filter(C, region, region_search_filter_active);
  }
  else if (region->flag & RGN_FLAG_SEARCH_FILTER_UPDATE) {
    region_panels_set_expansion_from_search_filter(C, region, region_search_filter_active);
  }

  if (region->flag & RGN_FLAG_SEARCH_FILTER_ACTIVE) {
    /* Clean up the extra panels and buttons created for searching. */
    region_panels_remove_invisible_layouts(region);
  }

  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    if (panel->runtime_flag & PANEL_ACTIVE) {
      BLI_assert(panel->runtime.block != nullptr);
      panel_calculate_size_recursive(region, panel);
    }
  }

  /* Offset contents. */
  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    if (block->active && block->panel) {
      ui_offset_panel_block(block);

      /* Update bounds for all "views" in this block. Usually this is done in #UI_block_end(), but
       * that wouldn't work because of the offset applied above. */
      ui_block_views_bounds_calc(block);
    }
  }

  /* Re-align, possibly with animation. */
  Panel *panel;
  if (panels_need_realign(area, region, &panel)) {
    if (panel) {
      panel_activate_state(C, panel, PANEL_STATE_ANIMATION);
    }
    else {
      uiAlignPanelStep(region, 1.0, false);
    }
  }

  /* Compute size taken up by panels. */
  ui_panels_size(region, r_x, r_y);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Panel Dragging
 * \{ */

#define DRAG_REGION_PAD (PNL_HEADER * 0.5)
static void ui_do_drag(const bContext *C, const wmEvent *event, Panel *panel)
{
  uiHandlePanelData *data = static_cast<uiHandlePanelData *>(panel->activedata);
  ARegion *region = CTX_wm_region(C);

  /* Keep the drag position in the region with a small pad to keep the panel visible. */
  const int y = clamp_i(event->xy[1], region->winrct.ymin, region->winrct.ymax + DRAG_REGION_PAD);

  float dy = float(y - data->starty);

  /* Adjust for region zoom. */
  dy *= BLI_rctf_size_y(&region->v2d.cur) / float(BLI_rcti_size_y(&region->winrct));

  /* Add the movement of the view due to edge scrolling while dragging. */
  dy += (float(region->v2d.cur.ymin) - data->start_cur_ymin);

  panel->ofsy = data->startofsy + round_fl_to_int(dy);

  uiAlignPanelStep(region, 0.2f, true);

  ED_region_tag_redraw(region);
}
#undef DRAG_REGION_PAD

/** \} */

/* -------------------------------------------------------------------- */
/** \name Region Level Panel Interaction
 * \{ */

static uiPanelMouseState ui_panel_mouse_state_get(const uiBlock *block,
                                                  const Panel *panel,
                                                  const int mx,
                                                  const int my)
{
  if (!IN_RANGE(float(mx), block->rect.xmin, block->rect.xmax)) {
    return PANEL_MOUSE_OUTSIDE;
  }

  if (IN_RANGE(float(my), block->rect.ymax, block->rect.ymax + PNL_HEADER)) {
    return PANEL_MOUSE_INSIDE_HEADER;
  }

  if (!UI_panel_is_closed(panel)) {
    if (IN_RANGE(float(my), block->rect.ymin, block->rect.ymax + PNL_HEADER)) {
      return PANEL_MOUSE_INSIDE_CONTENT;
    }
  }

  return PANEL_MOUSE_OUTSIDE;
}

struct uiPanelDragCollapseHandle {
  bool was_first_open;
  int xy_init[2];
};

static void ui_panel_drag_collapse_handler_remove(bContext * /*C*/, void *userdata)
{
  uiPanelDragCollapseHandle *dragcol_data = static_cast<uiPanelDragCollapseHandle *>(userdata);
  MEM_freeN(dragcol_data);
}

static void ui_panel_drag_collapse(const bContext *C,
                                   const uiPanelDragCollapseHandle *dragcol_data,
                                   const int xy_dst[2])
{
  ARegion *region = CTX_wm_region(C);

  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    float xy_a_block[2] = {float(dragcol_data->xy_init[0]), float(dragcol_data->xy_init[1])};
    float xy_b_block[2] = {float(xy_dst[0]), float(xy_dst[1])};
    Panel *panel = block->panel;

    if (panel == nullptr || (panel->type && (panel->type->flag & PANEL_TYPE_NO_HEADER))) {
      continue;
    }
    const int oldflag = panel->flag;

    /* Lock axis. */
    xy_b_block[0] = dragcol_data->xy_init[0];

    /* Use cursor coords in block space. */
    ui_window_to_block_fl(region, block, &xy_a_block[0], &xy_a_block[1]);
    ui_window_to_block_fl(region, block, &xy_b_block[0], &xy_b_block[1]);

    /* Set up `rect` to match header size. */
    rctf rect = block->rect;
    rect.ymin = rect.ymax;
    rect.ymax = rect.ymin + PNL_HEADER;

    /* Touch all panels between last mouse coordinate and the current one. */
    if (BLI_rctf_isect_segment(&rect, xy_a_block, xy_b_block)) {
      /* Force panel to open or close. */
      panel->runtime_flag &= ~PANEL_USE_CLOSED_FROM_SEARCH;
      SET_FLAG_FROM_TEST(panel->flag, dragcol_data->was_first_open, PNL_CLOSED);

      /* If panel->flag has changed this means a panel was opened/closed here. */
      if (panel->flag != oldflag) {
        panel_activate_state(C, panel, PANEL_STATE_ANIMATION);
      }
    }
  }
  /* Update the instanced panel data expand flags with the changes made here. */
  set_panels_list_data_expand_flag(C, region);
}

/**
 * Panel drag-collapse (modal handler).
 * Clicking and dragging over panels toggles their collapse state based on the panel
 * that was first dragged over. If it was open all affected panels including the initial
 * one are closed and vice versa.
 */
static int ui_panel_drag_collapse_handler(bContext *C, const wmEvent *event, void *userdata)
{
  wmWindow *win = CTX_wm_window(C);
  uiPanelDragCollapseHandle *dragcol_data = static_cast<uiPanelDragCollapseHandle *>(userdata);
  short retval = WM_UI_HANDLER_CONTINUE;

  switch (event->type) {
    case MOUSEMOVE:
      ui_panel_drag_collapse(C, dragcol_data, event->xy);

      retval = WM_UI_HANDLER_BREAK;
      break;
    case LEFTMOUSE:
      if (event->val == KM_RELEASE) {
        /* Done! */
        WM_event_remove_ui_handler(&win->modalhandlers,
                                   ui_panel_drag_collapse_handler,
                                   ui_panel_drag_collapse_handler_remove,
                                   dragcol_data,
                                   true);
        ui_panel_drag_collapse_handler_remove(C, dragcol_data);
      }
      /* Don't let any left-mouse event fall through! */
      retval = WM_UI_HANDLER_BREAK;
      break;
  }

  return retval;
}

static void ui_panel_drag_collapse_handler_add(const bContext *C, const bool was_open)
{
  wmWindow *win = CTX_wm_window(C);
  const wmEvent *event = win->eventstate;
  uiPanelDragCollapseHandle *dragcol_data = MEM_new<uiPanelDragCollapseHandle>(__func__);

  dragcol_data->was_first_open = was_open;
  copy_v2_v2_int(dragcol_data->xy_init, event->xy);

  WM_event_add_ui_handler(C,
                          &win->modalhandlers,
                          ui_panel_drag_collapse_handler,
                          ui_panel_drag_collapse_handler_remove,
                          dragcol_data,
                          eWM_EventHandlerFlag(0));
}

/**
 * Supposing the block has a panel and isn't a menu, handle opening, closing, pinning, etc.
 * Code currently assumes layout style for location of widgets
 *
 * \param mx: The mouse x coordinate, in panel space.
 */
static void ui_handle_panel_header(const bContext *C,
                                   const uiBlock *block,
                                   const int mx,
                                   const int event_type,
                                   const bool ctrl,
                                   const bool shift)
{
  Panel *panel = block->panel;
  ARegion *region = CTX_wm_region(C);

  BLI_assert(panel->type != nullptr);
  BLI_assert(!(panel->type->flag & PANEL_TYPE_NO_HEADER));

  const bool is_subpanel = (panel->type->parent != nullptr);
  const bool use_pin = UI_panel_category_is_visible(region) && UI_panel_can_be_pinned(panel);
  const bool show_pin = use_pin && (panel->flag & PNL_PIN);
  const bool show_drag = !is_subpanel;

  /* Handle panel pinning. */
  if (use_pin && ELEM(event_type, EVT_RETKEY, EVT_PADENTER, LEFTMOUSE) && shift) {
    panel->flag ^= PNL_PIN;
    ED_region_tag_redraw(region);
    return;
  }

  float expansion_area_xmax = block->rect.xmax;
  if (show_drag) {
    expansion_area_xmax -= (PNL_ICON * 1.5f);
  }
  if (show_pin) {
    expansion_area_xmax -= PNL_ICON;
  }

  /* Collapse and expand panels. */
  if (ELEM(event_type, EVT_RETKEY, EVT_PADENTER, EVT_AKEY) || mx < expansion_area_xmax) {
    if (ctrl && !is_subpanel) {
      /* For parent panels, collapse all other panels or toggle children. */
      if (UI_panel_is_closed(panel) || BLI_listbase_is_empty(&panel->children)) {
        panels_collapse_all(region, panel);

        /* Reset the view - we don't want to display a view without content. */
        UI_view2d_offset(&region->v2d, 0.0f, 1.0f);
      }
      else {
        /* If a panel has sub-panels and it's open, toggle the expansion
         * of the sub-panels (based on the expansion of the first sub-panel). */
        Panel *first_child = static_cast<Panel *>(panel->children.first);
        BLI_assert(first_child != nullptr);
        panel_set_flag_recursive(panel, PNL_CLOSED, !UI_panel_is_closed(first_child));
        panel->flag |= PNL_CLOSED;
      }
    }

    SET_FLAG_FROM_TEST(panel->flag, !UI_panel_is_closed(panel), PNL_CLOSED);

    if (event_type == LEFTMOUSE) {
      ui_panel_drag_collapse_handler_add(C, UI_panel_is_closed(panel));
    }

    /* Set panel custom data (modifier) active when expanding sub-panels, but not top-level
     * panels to allow collapsing and expanding without setting the active element. */
    if (is_subpanel) {
      panel_custom_data_active_set(panel);
    }

    set_panels_list_data_expand_flag(C, region);
    panel_activate_state(C, panel, PANEL_STATE_ANIMATION);
    return;
  }

  /* Handle panel dragging. For now don't allow dragging in floating regions. */
  if (show_drag && !(region->alignment == RGN_ALIGN_FLOAT)) {
    const float drag_area_xmin = block->rect.xmax - (PNL_ICON * 1.5f);
    const float drag_area_xmax = block->rect.xmax;
    if (IN_RANGE(mx, drag_area_xmin, drag_area_xmax)) {
      panel_activate_state(C, panel, PANEL_STATE_DRAG);
      return;
    }
  }

  /* Handle panel unpinning. */
  if (show_pin) {
    const float pin_area_xmin = expansion_area_xmax;
    const float pin_area_xmax = pin_area_xmin + PNL_ICON;
    if (IN_RANGE(mx, pin_area_xmin, pin_area_xmax)) {
      panel->flag ^= PNL_PIN;
      ED_region_tag_redraw(region);
      return;
    }
  }
}

bool UI_panel_category_is_visible(const ARegion *region)
{
  /* Check for more than one category. */
  return region->panels_category.first &&
         region->panels_category.first != region->panels_category.last;
}

PanelCategoryDyn *UI_panel_category_find(const ARegion *region, const char *idname)
{
  return static_cast<PanelCategoryDyn *>(
      BLI_findstring(&region->panels_category, idname, offsetof(PanelCategoryDyn, idname)));
}

PanelCategoryStack *UI_panel_category_active_find(ARegion *region, const char *idname)
{
  return static_cast<PanelCategoryStack *>(BLI_findstring(
      &region->panels_category_active, idname, offsetof(PanelCategoryStack, idname)));
}

static void ui_panel_category_active_set(ARegion *region, const char *idname, bool fallback)
{
  ListBase *lb = &region->panels_category_active;
  PanelCategoryStack *pc_act = UI_panel_category_active_find(region, idname);

  if (pc_act) {
    BLI_remlink(lb, pc_act);
  }
  else {
    pc_act = MEM_cnew<PanelCategoryStack>(__func__);
    STRNCPY(pc_act->idname, idname);
  }

  if (fallback) {
    /* For fall-backs, add at the end so explicitly chosen categories have priority. */
    BLI_addtail(lb, pc_act);
  }
  else {
    BLI_addhead(lb, pc_act);
  }

  /* Validate all active panels. We could do this on load, they are harmless -
   * but we should remove them somewhere.
   * (Add-ons could define panels and gather cruft over time). */
  {
    PanelCategoryStack *pc_act_next;
    /* intentionally skip first */
    pc_act_next = pc_act->next;
    while ((pc_act = pc_act_next)) {
      pc_act_next = pc_act->next;
      if (!BLI_findstring(
              &region->type->paneltypes, pc_act->idname, offsetof(PanelType, category))) {
        BLI_remlink(lb, pc_act);
        MEM_freeN(pc_act);
      }
    }
  }
}

void UI_panel_category_active_set(ARegion *region, const char *idname)
{
  ui_panel_category_active_set(region, idname, false);
}

void UI_panel_category_active_set_default(ARegion *region, const char *idname)
{
  if (!UI_panel_category_active_find(region, idname)) {
    ui_panel_category_active_set(region, idname, true);
  }
}

const char *UI_panel_category_active_get(ARegion *region, bool set_fallback)
{
  LISTBASE_FOREACH (PanelCategoryStack *, pc_act, &region->panels_category_active) {
    if (UI_panel_category_find(region, pc_act->idname)) {
      return pc_act->idname;
    }
  }

  if (set_fallback) {
    PanelCategoryDyn *pc_dyn = static_cast<PanelCategoryDyn *>(region->panels_category.first);
    if (pc_dyn) {
      ui_panel_category_active_set(region, pc_dyn->idname, true);
      return pc_dyn->idname;
    }
  }

  return nullptr;
}

static PanelCategoryDyn *panel_categories_find_mouse_over(ARegion *region, const wmEvent *event)
{
  LISTBASE_FOREACH (PanelCategoryDyn *, ptd, &region->panels_category) {
    if (BLI_rcti_isect_pt(&ptd->rect, event->mval[0], event->mval[1])) {
      return ptd;
    }
  }

  return nullptr;
}

void UI_panel_category_add(ARegion *region, const char *name)
{
  PanelCategoryDyn *pc_dyn = MEM_cnew<PanelCategoryDyn>(__func__);
  BLI_addtail(&region->panels_category, pc_dyn);

  STRNCPY(pc_dyn->idname, name);

  /* 'pc_dyn->rect' must be set on draw. */
}

void UI_panel_category_clear_all(ARegion *region)
{
  BLI_freelistN(&region->panels_category);
}

static int ui_handle_panel_category_cycling(const wmEvent *event,
                                            ARegion *region,
                                            const uiBut *active_but)
{
  const bool is_mousewheel = ELEM(event->type, WHEELUPMOUSE, WHEELDOWNMOUSE);
  const bool inside_tabregion =
      ((RGN_ALIGN_ENUM_FROM_MASK(region->alignment) != RGN_ALIGN_RIGHT) ?
           (event->mval[0] < ((PanelCategoryDyn *)region->panels_category.first)->rect.xmax) :
           (event->mval[0] > ((PanelCategoryDyn *)region->panels_category.first)->rect.xmin));

  /* If mouse is inside non-tab region, ctrl key is required. */
  if (is_mousewheel && (event->modifier & KM_CTRL) == 0 && !inside_tabregion) {
    return WM_UI_HANDLER_CONTINUE;
  }

  if (active_but && ui_but_supports_cycling(active_but)) {
    /* Skip - exception to make cycling buttons using ctrl+mousewheel work in tabbed regions. */
  }
  else {
    const char *category = UI_panel_category_active_get(region, false);
    if (LIKELY(category)) {
      PanelCategoryDyn *pc_dyn = UI_panel_category_find(region, category);
      /* Cyclic behavior between categories
       * using Ctrl+Tab (+Shift for backwards) or Ctrl+Wheel Up/Down. */
      if (LIKELY(pc_dyn) && (event->modifier & KM_CTRL)) {
        if (is_mousewheel) {
          /* We can probably get rid of this and only allow Ctrl-Tabbing. */
          pc_dyn = (event->type == WHEELDOWNMOUSE) ? pc_dyn->next : pc_dyn->prev;
        }
        else {
          const bool backwards = event->modifier & KM_SHIFT;
          pc_dyn = backwards ? pc_dyn->prev : pc_dyn->next;
          if (!pc_dyn) {
            /* Proper cyclic behavior, back to first/last category (only used for ctrl+tab). */
            pc_dyn = backwards ? static_cast<PanelCategoryDyn *>(region->panels_category.last) :
                                 static_cast<PanelCategoryDyn *>(region->panels_category.first);
          }
        }

        if (pc_dyn) {
          /* Intentionally don't reset scroll in this case,
           * allowing for quick browsing between tabs. */
          UI_panel_category_active_set(region, pc_dyn->idname);
          ED_region_tag_redraw(region);
        }
        return WM_UI_HANDLER_BREAK;
      }
    }
  }

  return WM_UI_HANDLER_CONTINUE;
}

int ui_handler_panel_region(bContext *C,
                            const wmEvent *event,
                            ARegion *region,
                            const uiBut *active_but)
{
  /* Mouse-move events are handled by separate handlers for dragging and drag collapsing. */
  if (ISMOUSE_MOTION(event->type)) {
    return WM_UI_HANDLER_CONTINUE;
  }

  /* We only use KM_PRESS events in this function, so it's simpler to return early. */
  if (event->val != KM_PRESS) {
    return WM_UI_HANDLER_CONTINUE;
  }

  /* Scroll-bars can overlap panels now, they have handling priority. */
  if (UI_view2d_mouse_in_scrollers(region, &region->v2d, event->xy)) {
    return WM_UI_HANDLER_CONTINUE;
  }

  int retval = WM_UI_HANDLER_CONTINUE;

  /* Handle category tabs. */
  if (UI_panel_category_is_visible(region)) {
    if (event->type == LEFTMOUSE) {
      PanelCategoryDyn *pc_dyn = panel_categories_find_mouse_over(region, event);
      if (pc_dyn) {
        UI_panel_category_active_set(region, pc_dyn->idname);
        ED_region_tag_redraw(region);

        /* Reset scroll to the top (#38348). */
        UI_view2d_offset(&region->v2d, -1.0f, 1.0f);

        retval = WM_UI_HANDLER_BREAK;
      }
    }
    else if (((event->type == EVT_TABKEY) && (event->modifier & KM_CTRL)) ||
             ELEM(event->type, WHEELUPMOUSE, WHEELDOWNMOUSE))
    {
      /* Cycle tabs. */
      retval = ui_handle_panel_category_cycling(event, region, active_but);
    }
  }

  if (retval == WM_UI_HANDLER_BREAK) {
    return retval;
  }

  const bool region_has_active_button = (ui_region_find_active_but(region) != nullptr);

  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    Panel *panel = block->panel;
    if (panel == nullptr || panel->type == nullptr) {
      continue;
    }
    /* We can't expand or collapse panels without headers, they would disappear. */
    if (panel->type->flag & PANEL_TYPE_NO_HEADER) {
      continue;
    }

    int mx = event->xy[0];
    int my = event->xy[1];
    ui_window_to_block(region, block, &mx, &my);

    const uiPanelMouseState mouse_state = ui_panel_mouse_state_get(block, panel, mx, my);

    if (mouse_state != PANEL_MOUSE_OUTSIDE) {
      /* Mark panels that have been interacted with so their expansion
       * doesn't reset when property search finishes. */
      SET_FLAG_FROM_TEST(panel->flag, UI_panel_is_closed(panel), PNL_CLOSED);
      panel->runtime_flag &= ~PANEL_USE_CLOSED_FROM_SEARCH;

      /* The panel collapse / expand key "A" is special as it takes priority over
       * active button handling. */
      if (event->type == EVT_AKEY &&
          ((event->modifier & (KM_SHIFT | KM_CTRL | KM_ALT | KM_OSKEY)) == 0)) {
        retval = WM_UI_HANDLER_BREAK;
        ui_handle_panel_header(
            C, block, mx, event->type, event->modifier & KM_CTRL, event->modifier & KM_SHIFT);
        break;
      }
    }

    /* Don't do any other panel handling with an active button. */
    if (region_has_active_button) {
      continue;
    }

    if (mouse_state == PANEL_MOUSE_INSIDE_HEADER) {
      /* All mouse clicks inside panel headers should return in break. */
      if (ELEM(event->type, EVT_RETKEY, EVT_PADENTER, LEFTMOUSE)) {
        retval = WM_UI_HANDLER_BREAK;
        ui_handle_panel_header(
            C, block, mx, event->type, event->modifier & KM_CTRL, event->modifier & KM_SHIFT);
      }
      else if (event->type == RIGHTMOUSE) {
        retval = WM_UI_HANDLER_BREAK;
        ui_popup_context_menu_for_panel(C, region, block->panel);
      }
      break;
    }
  }

  return retval;
}

static void ui_panel_custom_data_set_recursive(Panel *panel, PointerRNA *custom_data)
{
  panel->runtime.custom_data_ptr = custom_data;

  LISTBASE_FOREACH (Panel *, child_panel, &panel->children) {
    ui_panel_custom_data_set_recursive(child_panel, custom_data);
  }
}

void UI_panel_context_pointer_set(Panel *panel, const char *name, PointerRNA *ptr)
{
  uiLayoutSetContextPointer(panel->layout, name, ptr);
  panel->runtime.context = uiLayoutGetContextStore(panel->layout);
}

void UI_panel_custom_data_set(Panel *panel, PointerRNA *custom_data)
{
  BLI_assert(panel->type != nullptr);

  /* Free the old custom data, which should be shared among all of the panel's sub-panels. */
  if (panel->runtime.custom_data_ptr != nullptr) {
    MEM_freeN(panel->runtime.custom_data_ptr);
  }

  ui_panel_custom_data_set_recursive(panel, custom_data);
}

PointerRNA *UI_panel_custom_data_get(const Panel *panel)
{
  return panel->runtime.custom_data_ptr;
}

PointerRNA *UI_region_panel_custom_data_under_cursor(const bContext *C, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  if (region) {
    LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
      Panel *panel = block->panel;
      if (panel == nullptr) {
        continue;
      }

      int mx = event->xy[0];
      int my = event->xy[1];
      ui_window_to_block(region, block, &mx, &my);
      const int mouse_state = ui_panel_mouse_state_get(block, panel, mx, my);
      if (ELEM(mouse_state, PANEL_MOUSE_INSIDE_CONTENT, PANEL_MOUSE_INSIDE_HEADER)) {
        return UI_panel_custom_data_get(panel);
      }
    }
  }

  return nullptr;
}

bool UI_panel_can_be_pinned(const Panel *panel)
{
  return (panel->type->parent == nullptr) && !(panel->type->flag & PANEL_TYPE_INSTANCED);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window Level Modal Panel Interaction
 * \{ */

/* NOTE: this is modal handler and should not swallow events for animation. */
static int ui_handler_panel(bContext *C, const wmEvent *event, void *userdata)
{
  Panel *panel = static_cast<Panel *>(userdata);
  uiHandlePanelData *data = static_cast<uiHandlePanelData *>(panel->activedata);

  /* Verify if we can stop. */
  if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
    panel_activate_state(C, panel, PANEL_STATE_ANIMATION);
  }
  else if (event->type == MOUSEMOVE) {
    if (data->state == PANEL_STATE_DRAG) {
      ui_do_drag(C, event, panel);
    }
  }
  else if (event->type == TIMER && event->customdata == data->animtimer) {
    if (data->state == PANEL_STATE_ANIMATION) {
      ui_do_animate(C, panel);
    }
    else if (data->state == PANEL_STATE_DRAG) {
      ui_do_drag(C, event, panel);
    }
  }

  data = static_cast<uiHandlePanelData *>(panel->activedata);

  if (data && data->state == PANEL_STATE_ANIMATION) {
    return WM_UI_HANDLER_CONTINUE;
  }
  return WM_UI_HANDLER_BREAK;
}

static void ui_handler_remove_panel(bContext *C, void *userdata)
{
  Panel *panel = static_cast<Panel *>(userdata);

  panel_activate_state(C, panel, PANEL_STATE_EXIT);
}

static void panel_handle_data_ensure(const bContext *C,
                                     wmWindow *win,
                                     const ARegion *region,
                                     Panel *panel,
                                     const uiHandlePanelState state)
{
  if (panel->activedata == nullptr) {
    panel->activedata = MEM_callocN(sizeof(uiHandlePanelData), __func__);
    WM_event_add_ui_handler(C,
                            &win->modalhandlers,
                            ui_handler_panel,
                            ui_handler_remove_panel,
                            panel,
                            eWM_EventHandlerFlag(0));
  }

  uiHandlePanelData *data = static_cast<uiHandlePanelData *>(panel->activedata);

  data->animtimer = WM_event_timer_add(CTX_wm_manager(C), win, TIMER, ANIMATION_INTERVAL);

  data->state = state;
  data->startx = win->eventstate->xy[0];
  data->starty = win->eventstate->xy[1];
  data->startofsx = panel->ofsx;
  data->startofsy = panel->ofsy;
  data->start_cur_xmin = region->v2d.cur.xmin;
  data->start_cur_ymin = region->v2d.cur.ymin;
  data->starttime = PIL_check_seconds_timer();
}

/**
 * \note "select" and "drag drop" flags: First, the panel is "picked up" and both flags are set.
 * Then when the mouse releases and the panel starts animating to its aligned position, PNL_SELECT
 * is unset. When the animation finishes, PANEL_IS_DRAG_DROP is cleared.
 */
static void panel_activate_state(const bContext *C, Panel *panel, const uiHandlePanelState state)
{
  uiHandlePanelData *data = static_cast<uiHandlePanelData *>(panel->activedata);
  wmWindow *win = CTX_wm_window(C);
  ARegion *region = CTX_wm_region(C);

  if (data != nullptr && data->state == state) {
    return;
  }

  if (state == PANEL_STATE_DRAG) {
    panel_custom_data_active_set(panel);

    panel_set_flag_recursive(panel, PNL_SELECT, true);
    panel_set_runtime_flag_recursive(panel, PANEL_IS_DRAG_DROP, true);

    panel_handle_data_ensure(C, win, region, panel, state);

    /* Initiate edge panning during drags for scrolling beyond the initial region view. */
    wmOperatorType *ot = WM_operatortype_find("VIEW2D_OT_edge_pan", true);
    ui_handle_afterfunc_add_operator(ot, WM_OP_INVOKE_DEFAULT);
  }
  else if (state == PANEL_STATE_ANIMATION) {
    panel_set_flag_recursive(panel, PNL_SELECT, false);

    panel_handle_data_ensure(C, win, region, panel, state);
  }
  else if (state == PANEL_STATE_EXIT) {
    panel_set_runtime_flag_recursive(panel, PANEL_IS_DRAG_DROP, false);

    BLI_assert(data != nullptr);

    if (data->animtimer) {
      WM_event_timer_remove(CTX_wm_manager(C), win, data->animtimer);
      data->animtimer = nullptr;
    }

    MEM_freeN(data);
    panel->activedata = nullptr;

    WM_event_remove_ui_handler(
        &win->modalhandlers, ui_handler_panel, ui_handler_remove_panel, panel, false);
  }

  ED_region_tag_redraw(region);
}

/** \} */
