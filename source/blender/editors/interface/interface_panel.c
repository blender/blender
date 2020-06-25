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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup edinterface
 */

/* a full doc with API notes can be found in
 * bf-blender/trunk/blender/doc/guides/interface_API.txt */

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "BLF_api.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "GPU_batch_presets.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "interface_intern.h"

/*********************** defines and structs ************************/

#define ANIMATION_TIME 0.30
#define ANIMATION_INTERVAL 0.02

#define PNL_LAST_ADDED 1
#define PNL_ACTIVE 2
#define PNL_WAS_ACTIVE 4
#define PNL_ANIM_ALIGN 8
#define PNL_NEW_ADDED 16
#define PNL_FIRST 32

/* only show pin header button for pinned panels */
#define USE_PIN_HIDDEN

/* the state of the mouse position relative to the panel */
typedef enum uiPanelMouseState {
  PANEL_MOUSE_OUTSIDE,        /* mouse is not in the panel */
  PANEL_MOUSE_INSIDE_CONTENT, /* mouse is in the actual panel content */
  PANEL_MOUSE_INSIDE_HEADER,  /* mouse is in the panel header */
  PANEL_MOUSE_INSIDE_SCALE,   /* mouse is inside panel scale widget */
} uiPanelMouseState;

typedef enum uiHandlePanelState {
  PANEL_STATE_DRAG,
  PANEL_STATE_DRAG_SCALE,
  PANEL_STATE_WAIT_UNTAB,
  PANEL_STATE_ANIMATION,
  PANEL_STATE_EXIT,
} uiHandlePanelState;

typedef struct uiHandlePanelData {
  uiHandlePanelState state;

  /* animation */
  wmTimer *animtimer;
  double starttime;

  /* dragging */
  bool is_drag_drop;
  int startx, starty;
  int startofsx, startofsy;
  int startsizex, startsizey;
  float start_cur_xmin, start_cur_ymin;
} uiHandlePanelData;

typedef struct PanelSort {
  Panel *panel, *orig;
} PanelSort;

static int get_panel_real_size_y(const Panel *panel);
static void panel_activate_state(const bContext *C, Panel *panel, uiHandlePanelState state);
static int compare_panel(const void *a1, const void *a2);
static bool panel_type_context_poll(PanelType *panel_type, const char *context);

static void panel_title_color_get(bool show_background, uchar color[4])
{
  if (show_background) {
    UI_GetThemeColor4ubv(TH_TITLE, color);
  }
  else {
    /* Use menu colors for floating panels. */
    bTheme *btheme = UI_GetTheme();
    const uiWidgetColors *wcol = &btheme->tui.wcol_menu_back;
    copy_v4_v4_uchar(color, (const uchar *)wcol->text);
  }
}

/*********************** space specific code ************************/
/* temporary code to remove all sbuts stuff from panel code         */

/* SpaceProperties.align */
typedef enum eSpaceButtons_Align {
  BUT_HORIZONTAL = 0,
  BUT_VERTICAL = 1,
  BUT_AUTO = 2,
} eSpaceButtons_Align;

static int panel_aligned(const ScrArea *area, const ARegion *region)
{
  if (area->spacetype == SPACE_PROPERTIES && region->regiontype == RGN_TYPE_WINDOW) {
    return BUT_VERTICAL;
  }
  else if (area->spacetype == SPACE_USERPREF && region->regiontype == RGN_TYPE_WINDOW) {
    return BUT_VERTICAL;
  }
  else if (area->spacetype == SPACE_FILE && region->regiontype == RGN_TYPE_CHANNELS) {
    return BUT_VERTICAL;
  }
  else if (area->spacetype == SPACE_IMAGE && region->regiontype == RGN_TYPE_PREVIEW) {
    return BUT_VERTICAL;
  }
  else if (ELEM(region->regiontype,
                RGN_TYPE_UI,
                RGN_TYPE_TOOLS,
                RGN_TYPE_TOOL_PROPS,
                RGN_TYPE_HUD,
                RGN_TYPE_NAV_BAR,
                RGN_TYPE_EXECUTE)) {
    return BUT_VERTICAL;
  }

  return 0;
}

static bool panel_active_animation_changed(ListBase *lb, Panel **pa_animation, bool *no_animation)
{
  LISTBASE_FOREACH (Panel *, panel, lb) {
    /* Detect panel active flag changes. */
    if (!(panel->type && panel->type->parent)) {
      if ((panel->runtime_flag & PNL_WAS_ACTIVE) && !(panel->runtime_flag & PNL_ACTIVE)) {
        return true;
      }
      if (!(panel->runtime_flag & PNL_WAS_ACTIVE) && (panel->runtime_flag & PNL_ACTIVE)) {
        return true;
      }
    }

    if ((panel->runtime_flag & PNL_ACTIVE) && !(panel->flag & PNL_CLOSED)) {
      if (panel_active_animation_changed(&panel->children, pa_animation, no_animation)) {
        return true;
      }
    }

    /* Detect animation. */
    if (panel->activedata) {
      uiHandlePanelData *data = panel->activedata;
      if (data->state == PANEL_STATE_ANIMATION) {
        *pa_animation = panel;
      }
      else {
        /* Don't animate while handling other interaction. */
        *no_animation = true;
      }
    }
    if ((panel->runtime_flag & PNL_ANIM_ALIGN) && !(*pa_animation)) {
      *pa_animation = panel;
    }
  }

  return false;
}

static bool panels_need_realign(ScrArea *area, ARegion *region, Panel **r_panel_animation)
{
  *r_panel_animation = NULL;

  if (area->spacetype == SPACE_PROPERTIES && region->regiontype == RGN_TYPE_WINDOW) {
    SpaceProperties *sbuts = area->spacedata.first;

    if (sbuts->mainbo != sbuts->mainb) {
      return true;
    }
  }
  else if (area->spacetype == SPACE_IMAGE && region->regiontype == RGN_TYPE_PREVIEW) {
    return true;
  }
  else if (area->spacetype == SPACE_FILE && region->regiontype == RGN_TYPE_CHANNELS) {
    return true;
  }

  /* Detect if a panel was added or removed. */
  Panel *panel_animation = NULL;
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

/********* Functions for instanced panels. ***********/

static Panel *UI_panel_add_instanced_ex(
    ScrArea *area, ARegion *region, ListBase *panels, PanelType *panel_type, int list_index)
{
  Panel *panel = MEM_callocN(sizeof(Panel), "instanced panel");
  panel->type = panel_type;
  BLI_strncpy(panel->panelname, panel_type->idname, sizeof(panel->panelname));

  panel->runtime.list_index = list_index;

  /* Add the panel's children too. Although they aren't instanced panels, we can still use this
   * function to create them, as UI_panel_begin does other things we don't need to do. */
  LISTBASE_FOREACH (LinkData *, child, &panel_type->children) {
    PanelType *child_type = child->data;
    UI_panel_add_instanced_ex(area, region, &panel->children, child_type, list_index);
  }

  /* Make sure the panel is added to the end of the display-order as well. This is needed for
   * loading existing files.
   *
   * Note: We could use special behavior to place it after the panel that starts the list of
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

/**
 * Called in situations where panels need to be added dynamically rather than having only one panel
 * corresponding to each PanelType.
 */
Panel *UI_panel_add_instanced(
    ScrArea *area, ARegion *region, ListBase *panels, char *panel_idname, int list_index)
{
  ARegionType *region_type = region->type;

  PanelType *panel_type = BLI_findstring(
      &region_type->paneltypes, panel_idname, offsetof(PanelType, idname));

  if (panel_type == NULL) {
    printf("Panel type '%s' not found.\n", panel_idname);
    return NULL;
  }

  return UI_panel_add_instanced_ex(area, region, panels, panel_type, list_index);
}

/**
 * Find a unique key to append to the idname for the lookup to the panel's #uiBlock. Needed for
 * instanced panels, where there can be multiple with the same type and idname.
 */
void UI_list_panel_unique_str(Panel *panel, char *r_name)
{
  snprintf(r_name, LIST_PANEL_UNIQUE_STR_LEN, "%d", panel->runtime.list_index);
}

/**
 * Remove the #uiBlock corresponding to a panel. The lookup is needed because panels don't store
 * a reference to their corresponding #uiBlock.
 */
static void panel_free_block(ARegion *region, Panel *panel)
{
  BLI_assert(panel->type);

  char block_name[BKE_ST_MAXNAME + LIST_PANEL_UNIQUE_STR_LEN];
  strncpy(block_name, panel->type->idname, BKE_ST_MAXNAME);
  char unique_panel_str[LIST_PANEL_UNIQUE_STR_LEN];
  UI_list_panel_unique_str(panel, unique_panel_str);
  strncat(block_name, unique_panel_str, LIST_PANEL_UNIQUE_STR_LEN);

  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    if (STREQ(block->name, block_name)) {
      BLI_remlink(&region->uiblocks, block);
      UI_block_free(NULL, block);
      break; /* Only delete one block for this panel. */
    }
  }
}

/**
 * Free a panel and it's children.
 *
 * \note The only panels that should need to be deleted at runtime are panels with the
 * #PNL_INSTANCED flag set.
 */
static void panel_delete(ARegion *region, ListBase *panels, Panel *panel)
{
  /* Recursively delete children. */
  LISTBASE_FOREACH_MUTABLE (Panel *, child, &panel->children) {
    panel_delete(region, &panel->children, child);
  }
  BLI_freelistN(&panel->children);

  panel_free_block(region, panel);

  BLI_remlink(panels, panel);
  if (panel->activedata) {
    MEM_freeN(panel->activedata);
  }
  MEM_freeN(panel);
}

/**
 * Remove instanced panels from the region's panel list.
 *
 * \note Can be called with NULL \a C, but it should be avoided because
 * handlers might not be removed.
 */
void UI_panels_free_instanced(bContext *C, ARegion *region)
{
  /* Delete panels with the instanced flag. */
  LISTBASE_FOREACH_MUTABLE (Panel *, panel, &region->panels) {
    if ((panel->type != NULL) && (panel->type->flag & PNL_INSTANCED)) {
      /* Make sure the panel's handler is removed before deleting it. */
      if (C != NULL && panel->activedata != NULL) {
        panel_activate_state(C, panel, PANEL_STATE_EXIT);
      }
      panel_delete(region, &region->panels, panel);
    }
  }
}

/**
 * Check if the instanced panels in the region's panels correspond to the list of data the panels
 * represent. Returns false if the panels have been reordered or if the types from the list data
 * don't match in any way.
 *
 * \param data: The list of data to check against the instanced panels.
 * \param panel_idname_func: Function to find the panel type idname for each item in the data list.
 * For a readability and generality, this lookup happens separately for each type of panel list.
 */
bool UI_panel_list_matches_data(ARegion *region,
                                ListBase *data,
                                uiListPanelIDFromDataFunc panel_idname_func)
{
  /* Check for NULL data. */
  int data_len = 0;
  Link *data_link = NULL;
  if (data == NULL) {
    data_len = 0;
    data_link = NULL;
  }
  else {
    data_len = BLI_listbase_count(data);
    data_link = data->first;
  }

  int i = 0;
  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    if (panel->type != NULL && panel->type->flag & PNL_INSTANCED) {
      /* The panels were reordered by drag and drop. */
      if (panel->flag & PNL_INSTANCED_LIST_ORDER_CHANGED) {
        return false;
      }

      /* We reached the last data item before the last instanced panel. */
      if (data_link == NULL) {
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
  if (drag_panel->type == NULL) {
    return;
  }
  /* Don't reorder if this instanced panel doesn't support drag and drop reordering. */
  if (drag_panel->type->reorder == NULL) {
    return;
  }

  char *context = drag_panel->type->context;

  /* Find how many instanced panels with this context string. */
  int list_panels_len = 0;
  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    if (panel->type) {
      if (panel_type_context_poll(panel->type, context)) {
        if (panel->type->flag & PNL_INSTANCED) {
          list_panels_len++;
        }
      }
    }
  }

  /* Sort the matching instanced panels by their display order. */
  PanelSort *panel_sort = MEM_callocN(list_panels_len * sizeof(*panel_sort), "instancedpanelsort");
  PanelSort *sort_index = panel_sort;
  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    if (panel->type) {
      if (panel_type_context_poll(panel->type, context)) {
        if (panel->type->flag & PNL_INSTANCED) {
          sort_index->panel = MEM_dupallocN(panel);
          sort_index->orig = panel;
          sort_index++;
        }
      }
    }
  }
  qsort(panel_sort, list_panels_len, sizeof(*panel_sort), compare_panel);

  /* Find how many of those panels are above this panel. */
  int move_to_index = 0;
  for (; move_to_index < list_panels_len; move_to_index++) {
    if (panel_sort[move_to_index].orig == drag_panel) {
      break;
    }
  }

  /* Free panel sort array. */
  int i = 0;
  for (sort_index = panel_sort; i < list_panels_len; i++, sort_index++) {
    MEM_freeN(sort_index->panel);
  }
  MEM_freeN(panel_sort);

  /* Don't reorder the panel didn't change order after being dropped. */
  if (move_to_index == drag_panel->runtime.list_index) {
    return;
  }

  /* Set the bit to tell the interface to instanced the list. */
  drag_panel->flag |= PNL_INSTANCED_LIST_ORDER_CHANGED;

  /* Finally, move this panel's list item to the new index in its list. */
  drag_panel->type->reorder(C, drag_panel, move_to_index);
}

/**
 * Recursive implementation for #UI_panel_set_expand_from_list_data.
 *
 * \return Whether the closed flag for the panel or any sub-panels changed.
 */
static bool panel_set_expand_from_list_data_recursive(Panel *panel, short flag, short *flag_index)
{
  bool open = (flag & (1 << *flag_index));
  bool changed = (open == (bool)(panel->flag & PNL_CLOSEDY));
  if (open) {
    panel->flag &= ~PNL_CLOSEDY;
  }
  else {
    panel->flag |= PNL_CLOSEDY;
  }
  LISTBASE_FOREACH (Panel *, child, &panel->children) {
    *flag_index = *flag_index + 1;
    changed |= panel_set_expand_from_list_data_recursive(child, flag, flag_index);
  }
  return changed;
}

/**
 * Set the expansion of the panel and its sub-panels from the flag stored by the list data
 * corresponding to this panel. The flag has expansion stored in each bit in depth first
 * order.
 */
void UI_panel_set_expand_from_list_data(const bContext *C, Panel *panel)
{
  BLI_assert(panel->type != NULL);
  BLI_assert(panel->type->flag & PNL_INSTANCED);
  if (panel->type->get_list_data_expand_flag == NULL) {
    /* Instanced panel doesn't support loading expansion. */
    return;
  }

  short expand_flag = panel->type->get_list_data_expand_flag(C, panel);
  short flag_index = 0;

  /* Start panel animation if the open state was changed. */
  if (panel_set_expand_from_list_data_recursive(panel, expand_flag, &flag_index)) {
    panel_activate_state(C, panel, PANEL_STATE_ANIMATION);
  }
}

/**
 * Recursive implementation for #set_panels_list_data_expand_flag.
 */
static void get_panel_expand_flag(Panel *panel, short *flag, short *flag_index)
{
  bool open = !(panel->flag & PNL_CLOSEDY);
  if (open) {
    *flag |= (1 << *flag_index);
  }
  else {
    *flag &= ~(1 << *flag_index);
  }
  LISTBASE_FOREACH (Panel *, child, &panel->children) {
    *flag_index = *flag_index + 1;
    get_panel_expand_flag(child, flag, flag_index);
  }
}

/**
 * Call the callback to store the panel and subpanel expansion settings in the list item that
 * corresponds to this panel.
 *
 * \note This needs to iterate through all of the regions panels because the panel with changed
 * expansion could have been the subpanel of a instanced panel, meaning it might not know
 * which list item it corresponds to.
 */
static void set_panels_list_data_expand_flag(const bContext *C, ARegion *region)
{
  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    PanelType *panel_type = panel->type;
    if (panel_type == NULL) {
      continue;
    }

    /* Check for #PNL_ACTIVE so we only set the expand flag for active panels. */
    if (panel_type->flag & PNL_INSTANCED && panel->runtime_flag & PNL_ACTIVE) {
      short expand_flag = 0; /* Initialize to quite complaining compiler, value not used. */
      short flag_index = 0;
      get_panel_expand_flag(panel, &expand_flag, &flag_index);
      if (panel->type->set_list_data_expand_flag) {
        panel->type->set_list_data_expand_flag(C, panel, expand_flag);
      }
    }
  }
}

/****************************** panels ******************************/

/**
 * Set flag state for a panel and its sub-panels.
 *
 * \return True if this function changed any of the flags, false if it didn't.
 */
static bool panel_set_flag_recursive(Panel *panel, int flag, bool value)
{
  short flag_original = panel->flag;

  SET_FLAG_FROM_TEST(panel->flag, value, flag);

  bool changed = (flag_original != panel->flag);

  LISTBASE_FOREACH (Panel *, child, &panel->children) {
    changed |= panel_set_flag_recursive(child, flag, value);
  }

  return changed;
}

static void panels_collapse_all(const bContext *C,
                                ScrArea *area,
                                ARegion *region,
                                const Panel *from_panel)
{
  const bool has_category_tabs = UI_panel_category_is_visible(region);
  const char *category = has_category_tabs ? UI_panel_category_active_get(region, false) : NULL;
  const int flag = ((panel_aligned(area, region) == BUT_HORIZONTAL) ? PNL_CLOSEDX : PNL_CLOSEDY);
  const PanelType *from_pt = from_panel->type;

  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    PanelType *pt = panel->type;

    /* close panels with headers in the same context */
    if (pt && from_pt && !(pt->flag & PNL_NO_HEADER)) {
      if (!pt->context[0] || !from_pt->context[0] || STREQ(pt->context, from_pt->context)) {
        if ((panel->flag & PNL_PIN) || !category || !pt->category[0] ||
            STREQ(pt->category, category)) {
          panel->flag &= ~PNL_CLOSED;
          panel->flag |= flag;
        }
      }
    }
  }
  set_panels_list_data_expand_flag(C, region);
}

static bool panel_type_context_poll(PanelType *panel_type, const char *context)
{
  if (panel_type->context[0] && STREQ(panel_type->context, context)) {
    return true;
  }
  return false;
}

Panel *UI_panel_find_by_type(ListBase *lb, PanelType *pt)
{
  const char *idname = pt->idname;

  LISTBASE_FOREACH (Panel *, panel, lb) {
    if (STREQLEN(panel->panelname, idname, sizeof(panel->panelname))) {
      return panel;
    }
  }
  return NULL;
}

/**
 * \note \a panel should be return value from #UI_panel_find_by_type and can be NULL.
 */
Panel *UI_panel_begin(ScrArea *area,
                      ARegion *region,
                      ListBase *lb,
                      uiBlock *block,
                      PanelType *pt,
                      Panel *panel,
                      bool *r_open)
{
  Panel *panel_last;
  const char *drawname = CTX_IFACE_(pt->translation_context, pt->label);
  const char *idname = pt->idname;
  const bool newpanel = (panel == NULL);
  int align = panel_aligned(area, region);

  if (!newpanel) {
    panel->type = pt;
  }
  else {
    /* new panel */
    panel = MEM_callocN(sizeof(Panel), "new panel");
    panel->type = pt;
    BLI_strncpy(panel->panelname, idname, sizeof(panel->panelname));

    if (pt->flag & PNL_DEFAULT_CLOSED) {
      if (align == BUT_VERTICAL) {
        panel->flag |= PNL_CLOSEDY;
      }
      else {
        panel->flag |= PNL_CLOSEDX;
      }
    }

    panel->ofsx = 0;
    panel->ofsy = 0;
    panel->sizex = 0;
    panel->sizey = 0;
    panel->blocksizex = 0;
    panel->blocksizey = 0;
    panel->runtime_flag |= PNL_NEW_ADDED;

    BLI_addtail(lb, panel);
  }

  /* Do not allow closed panels without headers! Else user could get "disappeared" UI! */
  if ((pt->flag & PNL_NO_HEADER) && (panel->flag & PNL_CLOSED)) {
    panel->flag &= ~PNL_CLOSED;
    /* Force update of panels' positions! */
    panel->sizex = 0;
    panel->sizey = 0;
    panel->blocksizex = 0;
    panel->blocksizey = 0;
  }

  BLI_strncpy(panel->drawname, drawname, sizeof(panel->drawname));

  /* if a new panel is added, we insert it right after the panel
   * that was last added. this way new panels are inserted in the
   * right place between versions */
  for (panel_last = lb->first; panel_last; panel_last = panel_last->next) {
    if (panel_last->runtime_flag & PNL_LAST_ADDED) {
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
    panel_last->runtime_flag &= ~PNL_LAST_ADDED;
  }

  /* assign to block */
  block->panel = panel;
  panel->runtime_flag |= PNL_ACTIVE | PNL_LAST_ADDED;
  if (region->alignment == RGN_ALIGN_FLOAT) {
    UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);
  }

  *r_open = false;

  if (panel->flag & PNL_CLOSED) {
    return panel;
  }

  *r_open = true;

  return panel;
}

static float panel_region_offset_x_get(const ARegion *region, int align)
{
  if (UI_panel_category_is_visible(region)) {
    if (align == BUT_VERTICAL &&
        (RGN_ALIGN_ENUM_FROM_MASK(region->alignment) != RGN_ALIGN_RIGHT)) {
      return UI_PANEL_CATEGORY_MARGIN_WIDTH;
    }
  }

  return 0;
}

void UI_panel_end(
    const ScrArea *area, const ARegion *region, uiBlock *block, int width, int height, bool open)
{
  Panel *panel = block->panel;

  /* Set panel size excluding children. */
  panel->blocksizex = width;
  panel->blocksizey = height;

  /* Compute total panel size including children. */
  LISTBASE_FOREACH (Panel *, pachild, &panel->children) {
    if (pachild->runtime_flag & PNL_ACTIVE) {
      width = max_ii(width, pachild->sizex);
      height += get_panel_real_size_y(pachild);
    }
  }

  /* Update total panel size. */
  if (panel->runtime_flag & PNL_NEW_ADDED) {
    panel->runtime_flag &= ~PNL_NEW_ADDED;
    panel->sizex = width;
    panel->sizey = height;
  }
  else {
    int old_sizex = panel->sizex, old_sizey = panel->sizey;
    int old_region_ofsx = panel->runtime.region_ofsx;

    /* update width/height if non-zero */
    if (width != 0) {
      panel->sizex = width;
    }
    if (height != 0 || open) {
      panel->sizey = height;
    }

    /* check if we need to do an animation */
    if (panel->sizex != old_sizex || panel->sizey != old_sizey) {
      panel->runtime_flag |= PNL_ANIM_ALIGN;
      panel->ofsy += old_sizey - panel->sizey;
    }

    int align = panel_aligned(area, region);
    panel->runtime.region_ofsx = panel_region_offset_x_get(region, align);
    if (old_region_ofsx != panel->runtime.region_ofsx) {
      panel->runtime_flag |= PNL_ANIM_ALIGN;
    }
  }
}

static void ui_offset_panel_block(uiBlock *block)
{
  const uiStyle *style = UI_style_get_dpi();

  /* compute bounds and offset */
  ui_block_bounds_calc(block);

  int ofsy = block->panel->sizey - style->panelspace;

  LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
    but->rect.ymin += ofsy;
    but->rect.ymax += ofsy;
  }

  block->rect.xmax = block->panel->sizex;
  block->rect.ymax = block->panel->sizey;
  block->rect.xmin = block->rect.ymin = 0.0;
}

/**************************** drawing *******************************/

/* triangle 'icon' for panel header */
void UI_draw_icon_tri(float x, float y, char dir, const float color[4])
{
  float f3 = 0.05 * U.widget_unit;
  float f5 = 0.15 * U.widget_unit;
  float f7 = 0.25 * U.widget_unit;

  if (dir == 'h') {
    UI_draw_anti_tria(x - f3, y - f5, x - f3, y + f5, x + f7, y, color);
  }
  else if (dir == 't') {
    UI_draw_anti_tria(x - f5, y - f7, x + f5, y - f7, x, y + f3, color);
  }
  else { /* 'v' = vertical, down */
    UI_draw_anti_tria(x - f5, y + f3, x + f5, y + f3, x, y - f7, color);
  }
}

static void ui_draw_anti_x(uint pos, float x1, float y1, float x2, float y2)
{

  /* set antialias line */
  GPU_line_smooth(true);
  GPU_blend(true);

  GPU_line_width(2.0);

  immBegin(GPU_PRIM_LINES, 4);

  immVertex2f(pos, x1, y1);
  immVertex2f(pos, x2, y2);

  immVertex2f(pos, x1, y2);
  immVertex2f(pos, x2, y1);

  immEnd();

  GPU_line_smooth(false);
  GPU_blend(false);
}

/* x 'icon' for panel header */
static void ui_draw_x_icon(uint pos, float x, float y)
{

  ui_draw_anti_x(pos, x, y, x + 9.375f, y + 9.375f);
}

#define PNL_ICON UI_UNIT_X /* could be UI_UNIT_Y too */

static void ui_draw_panel_scalewidget(uint pos, const rcti *rect)
{
  float xmin, xmax, dx;
  float ymin, ymax, dy;

  xmin = rect->xmax - PNL_HEADER + 2;
  xmax = rect->xmax - 3;
  ymin = rect->ymin + 3;
  ymax = rect->ymin + PNL_HEADER - 2;

  dx = 0.5f * (xmax - xmin);
  dy = 0.5f * (ymax - ymin);

  GPU_blend(true);
  immUniformColor4ub(255, 255, 255, 50);

  immBegin(GPU_PRIM_LINES, 4);

  immVertex2f(pos, xmin, ymin);
  immVertex2f(pos, xmax, ymax);

  immVertex2f(pos, xmin + dx, ymin);
  immVertex2f(pos, xmax, ymax - dy);

  immEnd();

  immUniformColor4ub(0, 0, 0, 50);

  immBegin(GPU_PRIM_LINES, 4);

  immVertex2f(pos, xmin, ymin + 1);
  immVertex2f(pos, xmax, ymax + 1);

  immVertex2f(pos, xmin + dx, ymin + 1);
  immVertex2f(pos, xmax, ymax - dy + 1);

  immEnd();

  GPU_blend(false);
}

/* For button layout next to label. */
void UI_panel_label_offset(uiBlock *block, int *r_x, int *r_y)
{
  Panel *panel = block->panel;
  const bool is_subpanel = (panel->type && panel->type->parent);

  *r_x = UI_UNIT_X * 1.0f;
  *r_y = UI_UNIT_Y * 1.5f;

  if (is_subpanel) {
    *r_x += (0.7f * UI_UNIT_X);
  }
}

static void ui_draw_aligned_panel_header(
    uiStyle *style, uiBlock *block, const rcti *rect, char dir, const bool show_background)
{
  Panel *panel = block->panel;
  rcti hrect;
  int pnl_icons;
  const char *activename = panel->drawname;
  const bool is_subpanel = (panel->type && panel->type->parent);
  uiFontStyle *fontstyle = (is_subpanel) ? &style->widgetlabel : &style->paneltitle;
  uchar col_title[4];

  /* + 0.001f to avoid flirting with float inaccuracy */
  if (panel->control & UI_PNL_CLOSE) {
    pnl_icons = (panel->labelofs + (2.0f * PNL_ICON)) / block->aspect + 0.001f;
  }
  else {
    pnl_icons = (panel->labelofs + (1.1f * PNL_ICON)) / block->aspect + 0.001f;
  }

  /* draw text label */
  panel_title_color_get(show_background, col_title);
  col_title[3] = 255;

  hrect = *rect;
  if (dir == 'h') {
    hrect.xmin = rect->xmin + pnl_icons;
    hrect.ymin -= 2.0f / block->aspect;
    UI_fontstyle_draw(fontstyle,
                      &hrect,
                      activename,
                      col_title,
                      &(struct uiFontStyleDraw_Params){
                          .align = UI_STYLE_TEXT_LEFT,
                      });
  }
  else {
    /* ignore 'pnl_icons', otherwise the text gets offset horizontally
     * + 0.001f to avoid flirting with float inaccuracy
     */
    hrect.xmin = rect->xmin + (PNL_ICON + 5) / block->aspect + 0.001f;
    UI_fontstyle_draw_rotated(fontstyle, &hrect, activename, col_title);
  }
}

/* panel integrated in buttonswindow, tool/property lists etc */
void ui_draw_aligned_panel(uiStyle *style,
                           uiBlock *block,
                           const rcti *rect,
                           const bool show_pin,
                           const bool show_background)
{
  Panel *panel = block->panel;
  rctf itemrect;
  float color[4];
  const bool is_closed_x = (panel->flag & PNL_CLOSEDX) ? true : false;
  const bool is_closed_y = (panel->flag & PNL_CLOSEDY) ? true : false;
  const bool is_subpanel = (panel->type && panel->type->parent);
  const bool show_drag = (!is_subpanel &&
                          /* FIXME(campbell): currently no background means floating panel which
                           * can't be dragged. This may be changed in future. */
                          show_background);
  const int panel_col = is_subpanel ? TH_PANEL_SUB_BACK : TH_PANEL_BACK;
  const bool draw_box_style = (panel->type && panel->type->flag & PNL_DRAW_BOX);

  /* Use the theme for box widgets for box-style panels. */
  uiWidgetColors *box_wcol = NULL;
  if (draw_box_style) {
    bTheme *btheme = UI_GetTheme();
    box_wcol = &btheme->tui.wcol_box;
  }

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  if (panel->type && (panel->type->flag & PNL_NO_HEADER)) {
    if (show_background) {
      immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
      immUniformThemeColor(panel_col);
      immRectf(pos, rect->xmin, rect->ymin, rect->xmax, rect->ymax);
      immUnbindProgram();
    }
    return;
  }

  /* Calculate header rect with + 0.001f to prevent flicker due to float inaccuracy */
  rcti headrect = {
      rect->xmin, rect->xmax, rect->ymax, rect->ymax + floor(PNL_HEADER / block->aspect + 0.001f)};

  /* Draw a panel and header backdrops with an opaque box backdrop for box style panels. */
  if (draw_box_style && !is_subpanel) {
    /* Expand the top a tiny bit to give header buttons equal size above and below. */
    rcti box_rect = {rect->xmin,
                     rect->xmax,
                     (is_closed_x || is_closed_y) ? headrect.ymin : rect->ymin,
                     headrect.ymax + U.pixelsize};
    ui_draw_box_opaque(&box_rect, UI_CNR_ALL);

    /* Mimick the border between aligned box widgets for the bottom of the header. */
    if (!(is_closed_x || is_closed_y)) {
      immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
      GPU_blend(true);

      immUniformColor4ubv(box_wcol->outline);
      immRectf(pos, rect->xmin, headrect.ymin - U.pixelsize, rect->xmax, headrect.ymin);
      uchar emboss_col[4];
      UI_GetThemeColor4ubv(TH_WIDGET_EMBOSS, emboss_col);
      immUniformColor4ubv(emboss_col);
      immRectf(pos,
               rect->xmin,
               headrect.ymin - U.pixelsize,
               rect->xmax,
               headrect.ymin - U.pixelsize - 1);

      GPU_blend(false);
      immUnbindProgram();
    }
  }

  /* Draw the header backdrop. */
  if (show_background && !is_subpanel && !draw_box_style) {
    float minx = rect->xmin;
    float maxx = is_closed_x ? (minx + PNL_HEADER / block->aspect) : rect->xmax;
    float y = headrect.ymax;

    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
    GPU_blend(true);

    /* draw with background color */
    immUniformThemeColor(TH_PANEL_HEADER);
    immRectf(pos, minx, headrect.ymin, maxx, y);

    immBegin(GPU_PRIM_LINES, 4);

    immVertex2f(pos, minx, y);
    immVertex2f(pos, maxx, y);

    immVertex2f(pos, minx, y);
    immVertex2f(pos, maxx, y);

    immEnd();

    GPU_blend(false);
    immUnbindProgram();
  }

/* draw optional pin icon */
#ifdef USE_PIN_HIDDEN
  if (show_pin && (block->panel->flag & PNL_PIN))
#else
  if (show_pin)
#endif
  {
    uchar col_title[4];
    panel_title_color_get(show_background, col_title);

    GPU_blend(true);
    UI_icon_draw_ex(headrect.xmax - ((PNL_ICON * 2.2f) / block->aspect),
                    headrect.ymin + (5.0f / block->aspect),
                    (panel->flag & PNL_PIN) ? ICON_PINNED : ICON_UNPINNED,
                    (block->aspect * U.inv_dpi_fac),
                    1.0f,
                    0.0f,
                    col_title,
                    false);
    GPU_blend(false);
  }

  /* horizontal title */
  rcti titlerect = headrect;
  if (is_subpanel) {
    titlerect.xmin += (0.7f * UI_UNIT_X) / block->aspect + 0.001f;
  }
  if (is_closed_x == false) {
    ui_draw_aligned_panel_header(style, block, &titlerect, 'h', show_background);

    if (show_drag) {
      /* itemrect smaller */
      const float scale = 0.7;
      itemrect.xmax = headrect.xmax - (0.2f * UI_UNIT_X);
      itemrect.xmin = itemrect.xmax - BLI_rcti_size_y(&headrect);
      itemrect.ymin = headrect.ymin;
      itemrect.ymax = headrect.ymax;
      BLI_rctf_scale(&itemrect, scale);

      GPU_matrix_push();
      GPU_matrix_translate_2f(itemrect.xmin, itemrect.ymin);

      const int col_tint = 84;
      float col_high[4], col_dark[4];
      UI_GetThemeColorShade4fv(TH_PANEL_HEADER, col_tint, col_high);
      UI_GetThemeColorShade4fv(TH_PANEL_BACK, -col_tint, col_dark);

      GPUBatch *batch = GPU_batch_preset_panel_drag_widget(
          U.pixelsize, col_high, col_dark, BLI_rcti_size_y(&headrect) * scale);
      GPU_batch_program_set_builtin(batch, GPU_SHADER_2D_FLAT_COLOR);
      GPU_batch_draw(batch);
      GPU_matrix_pop();
    }
  }

  /* Draw panel backdrop. */
  if (is_closed_y) {
    /* skip */
  }
  else if (is_closed_x) {
    /* draw vertical title */
    ui_draw_aligned_panel_header(style, block, &headrect, 'v', show_background);
    pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  }
  /* an open panel */
  else {
    /* in some occasions, draw a border */
    if (panel->flag & PNL_SELECT && !is_subpanel) {
      float radius;
      if (panel->control & UI_PNL_SOLID) {
        UI_draw_roundbox_corner_set(UI_CNR_ALL);
        radius = 8.0f;
      }
      else if (draw_box_style) {
        UI_draw_roundbox_corner_set(UI_CNR_ALL);
        radius = box_wcol->roundness * U.widget_unit;
      }
      else {
        UI_draw_roundbox_corner_set(UI_CNR_NONE);
        radius = 0.0f;
      }

      UI_GetThemeColorShade4fv(TH_BACK, -120, color);
      UI_draw_roundbox_aa(false,
                          0.5f + rect->xmin,
                          0.5f + rect->ymin,
                          0.5f + rect->xmax,
                          0.5f + headrect.ymax + 1,
                          radius,
                          color);
    }

    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
    GPU_blend(true);

    /* Draw panel backdrop if it wasn't already been drawn by the single opaque round box earlier.
     * Note: Sub-panels blend with panels, so they can't be opaque. */
    if (show_background && !(draw_box_style && !is_subpanel)) {
      /* Draw the bottom sub-panels. */
      if (draw_box_style) {
        if (panel->next) {
          immUniformThemeColor(panel_col);
          immRectf(
              pos, rect->xmin + U.pixelsize, rect->ymin, rect->xmax - U.pixelsize, rect->ymax);
        }
        else {
          /* Change the width a little bit to line up with sides. */
          UI_draw_roundbox_corner_set(UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT);
          UI_GetThemeColor4fv(panel_col, color);
          UI_draw_roundbox_aa(true,
                              rect->xmin + U.pixelsize,
                              rect->ymin + U.pixelsize,
                              rect->xmax - U.pixelsize,
                              rect->ymax,
                              box_wcol->roundness * U.widget_unit,
                              color);
        }
      }
      else {
        immUniformThemeColor(panel_col);
        immRectf(pos, rect->xmin, rect->ymin, rect->xmax, rect->ymax);
      }
    }

    if (panel->control & UI_PNL_SCALE) {
      ui_draw_panel_scalewidget(pos, rect);
    }

    immUnbindProgram();
  }

  uchar col_title[4];
  panel_title_color_get(show_background, col_title);

  /* draw optional close icon */

  if (panel->control & UI_PNL_CLOSE) {
    const int ofsx = 6;
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
    immUniformColor3ubv(col_title);
    ui_draw_x_icon(pos, rect->xmin + 2 + ofsx, rect->ymax + 2);
    immUnbindProgram();
  }

  /* draw collapse icon */

  /* itemrect smaller */
  itemrect.xmin = titlerect.xmin;
  itemrect.xmax = itemrect.xmin + BLI_rcti_size_y(&titlerect);
  itemrect.ymin = titlerect.ymin;
  itemrect.ymax = titlerect.ymax;

  BLI_rctf_scale(&itemrect, 0.25f);

  {
    float tria_color[4];
    rgb_uchar_to_float(tria_color, col_title);
    tria_color[3] = 1.0f;

    if (is_closed_y) {
      ui_draw_anti_tria_rect(&itemrect, 'h', tria_color);
    }
    else if (is_closed_x) {
      ui_draw_anti_tria_rect(&itemrect, 'h', tria_color);
    }
    else {
      ui_draw_anti_tria_rect(&itemrect, 'v', tria_color);
    }
  }
}

/************************** panel alignment *************************/

static int get_panel_header(const Panel *panel)
{
  if (panel->type && (panel->type->flag & PNL_NO_HEADER)) {
    return 0;
  }

  return PNL_HEADER;
}

static int get_panel_size_y(const Panel *panel)
{
  if (panel->type && (panel->type->flag & PNL_NO_HEADER)) {
    return panel->sizey;
  }

  return PNL_HEADER + panel->sizey;
}

static int get_panel_real_size_y(const Panel *panel)
{
  int sizey = (panel->flag & PNL_CLOSED) ? 0 : panel->sizey;

  if (panel->type && (panel->type->flag & PNL_NO_HEADER)) {
    return sizey;
  }

  return PNL_HEADER + sizey;
}

int UI_panel_size_y(const Panel *panel)
{
  return get_panel_real_size_y(panel);
}

/* this function is needed because uiBlock and Panel itself don't
 * change sizey or location when closed */
static int get_panel_real_ofsy(Panel *panel)
{
  if (panel->flag & PNL_CLOSEDY) {
    return panel->ofsy + panel->sizey;
  }
  else {
    return panel->ofsy;
  }
}

static int get_panel_real_ofsx(Panel *panel)
{
  if (panel->flag & PNL_CLOSEDX) {
    return panel->ofsx + get_panel_header(panel);
  }
  else {
    return panel->ofsx + panel->sizex;
  }
}

bool UI_panel_is_dragging(const struct Panel *panel)
{
  uiHandlePanelData *data = panel->activedata;
  if (!data) {
    return false;
  }

  return data->is_drag_drop;
}

/**
 * \note about sorting;
 * the sortorder has a lower value for new panels being added.
 * however, that only works to insert a single panel, when more new panels get
 * added the coordinates of existing panels and the previously stored to-be-inserted
 * panels do not match for sorting
 */

static int find_leftmost_panel(const void *a1, const void *a2)
{
  const PanelSort *ps1 = a1, *ps2 = a2;

  if (ps1->panel->ofsx > ps2->panel->ofsx) {
    return 1;
  }
  else if (ps1->panel->ofsx < ps2->panel->ofsx) {
    return -1;
  }
  else if (ps1->panel->sortorder > ps2->panel->sortorder) {
    return 1;
  }
  else if (ps1->panel->sortorder < ps2->panel->sortorder) {
    return -1;
  }

  return 0;
}

static int find_highest_panel(const void *a1, const void *a2)
{
  const PanelSort *ps1 = a1, *ps2 = a2;

  /* stick uppermost header-less panels to the top of the region -
   * prevent them from being sorted (multiple header-less panels have to be sorted though) */
  if (ps1->panel->type->flag & PNL_NO_HEADER && ps2->panel->type->flag & PNL_NO_HEADER) {
    /* skip and check for ofs and sortorder below */
  }
  else if (ps1->panel->type->flag & PNL_NO_HEADER) {
    return -1;
  }
  else if (ps2->panel->type->flag & PNL_NO_HEADER) {
    return 1;
  }

  if (ps1->panel->ofsy + ps1->panel->sizey < ps2->panel->ofsy + ps2->panel->sizey) {
    return 1;
  }
  else if (ps1->panel->ofsy + ps1->panel->sizey > ps2->panel->ofsy + ps2->panel->sizey) {
    return -1;
  }
  else if (ps1->panel->sortorder > ps2->panel->sortorder) {
    return 1;
  }
  else if (ps1->panel->sortorder < ps2->panel->sortorder) {
    return -1;
  }

  return 0;
}

static int compare_panel(const void *a1, const void *a2)
{
  const PanelSort *ps1 = a1, *ps2 = a2;

  if (ps1->panel->sortorder > ps2->panel->sortorder) {
    return 1;
  }
  else if (ps1->panel->sortorder < ps2->panel->sortorder) {
    return -1;
  }

  return 0;
}

static void align_sub_panels(Panel *panel)
{
  /* Position sub panels. */
  int ofsy = panel->ofsy + panel->sizey - panel->blocksizey;

  LISTBASE_FOREACH (Panel *, pachild, &panel->children) {
    if (pachild->runtime_flag & PNL_ACTIVE) {
      pachild->ofsx = panel->ofsx;
      pachild->ofsy = ofsy - get_panel_size_y(pachild);
      ofsy -= get_panel_real_size_y(pachild);

      if (pachild->children.first) {
        align_sub_panels(pachild);
      }
    }
  }
}

/* this doesn't draw */
/* returns 1 when it did something */
static bool uiAlignPanelStep(ScrArea *area, ARegion *region, const float fac, const bool drag)
{
  PanelSort *ps, *panelsort, *psnext;
  int a, tot = 0;
  bool done;
  int align = panel_aligned(area, region);

  /* count active, not tabbed panels */
  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    if (panel->runtime_flag & PNL_ACTIVE) {
      tot++;
    }
  }

  if (tot == 0) {
    return 0;
  }

  /* extra; change close direction? */
  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    if (panel->runtime_flag & PNL_ACTIVE) {
      if ((panel->flag & PNL_CLOSEDX) && (align == BUT_VERTICAL)) {
        panel->flag ^= PNL_CLOSED;
      }
      else if ((panel->flag & PNL_CLOSEDY) && (align == BUT_HORIZONTAL)) {
        panel->flag ^= PNL_CLOSED;
      }
    }
  }

  /* sort panels */
  panelsort = MEM_callocN(tot * sizeof(PanelSort), "panelsort");

  ps = panelsort;
  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    if (panel->runtime_flag & PNL_ACTIVE) {
      ps->panel = MEM_dupallocN(panel);
      ps->orig = panel;
      ps++;
    }
  }

  if (drag) {
    /* while we are dragging, we sort on location and update sortorder */
    if (align == BUT_VERTICAL) {
      qsort(panelsort, tot, sizeof(PanelSort), find_highest_panel);
    }
    else {
      qsort(panelsort, tot, sizeof(PanelSort), find_leftmost_panel);
    }

    for (ps = panelsort, a = 0; a < tot; a++, ps++) {
      ps->orig->sortorder = a;
    }
  }
  else {
    /* otherwise use sortorder */
    qsort(panelsort, tot, sizeof(PanelSort), compare_panel);
  }

  /* no smart other default start loc! this keeps switching f5/f6/etc compatible */
  ps = panelsort;
  ps->panel->runtime.region_ofsx = panel_region_offset_x_get(region, align);
  ps->panel->ofsx = 0;
  ps->panel->ofsy = -get_panel_size_y(ps->panel);
  ps->panel->ofsx += ps->panel->runtime.region_ofsx;
  /* Extra margin if the panel is a box style panel. */
  if (ps->panel->type && ps->panel->type->flag & PNL_DRAW_BOX) {
    ps->panel->ofsx += UI_PANEL_BOX_STYLE_MARGIN;
    ps->panel->ofsy -= UI_PANEL_BOX_STYLE_MARGIN;
  }

  for (a = 0; a < tot - 1; a++, ps++) {
    psnext = ps + 1;

    if (align == BUT_VERTICAL) {
      bool use_box = ps->panel->type && ps->panel->type->flag & PNL_DRAW_BOX;
      bool use_box_next = psnext->panel->type && psnext->panel->type->flag & PNL_DRAW_BOX;
      psnext->panel->ofsx = ps->panel->ofsx;
      psnext->panel->ofsy = get_panel_real_ofsy(ps->panel) - get_panel_size_y(psnext->panel);
      /* Extra margin for box style panels. */
      if (use_box || use_box_next) {
        psnext->panel->ofsy -= UI_PANEL_BOX_STYLE_MARGIN;
      }
      if (use_box && !use_box_next) {
        psnext->panel->ofsx -= UI_PANEL_BOX_STYLE_MARGIN;
      }
      else if (!use_box && use_box_next) {
        psnext->panel->ofsx += UI_PANEL_BOX_STYLE_MARGIN;
      }
    }
    else {
      psnext->panel->ofsx = get_panel_real_ofsx(ps->panel);
      psnext->panel->ofsy = ps->panel->ofsy + get_panel_size_y(ps->panel) -
                            get_panel_size_y(psnext->panel);
    }
  }

  /* we interpolate */
  done = false;
  ps = panelsort;
  for (a = 0; a < tot; a++, ps++) {
    if ((ps->panel->flag & PNL_SELECT) == 0) {
      if ((ps->orig->ofsx != ps->panel->ofsx) || (ps->orig->ofsy != ps->panel->ofsy)) {
        ps->orig->ofsx = round_fl_to_int(fac * (float)ps->panel->ofsx +
                                         (1.0f - fac) * (float)ps->orig->ofsx);
        ps->orig->ofsy = round_fl_to_int(fac * (float)ps->panel->ofsy +
                                         (1.0f - fac) * (float)ps->orig->ofsy);
        done = true;
      }
    }
  }

  /* set locations for tabbed and sub panels */
  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    if (panel->runtime_flag & PNL_ACTIVE) {
      if (panel->children.first) {
        align_sub_panels(panel);
      }
    }
  }

  /* free panelsort array */
  for (ps = panelsort, a = 0; a < tot; a++, ps++) {
    MEM_freeN(ps->panel);
  }
  MEM_freeN(panelsort);

  return done;
}

static void ui_panels_size(ScrArea *area, ARegion *region, int *r_x, int *r_y)
{
  int align = panel_aligned(area, region);
  int sizex = 0;
  int sizey = 0;

  /* compute size taken up by panels, for setting in view2d */
  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    if (panel->runtime_flag & PNL_ACTIVE) {
      int pa_sizex, pa_sizey;

      if (align == BUT_VERTICAL) {
        pa_sizex = panel->ofsx + panel->sizex;
        pa_sizey = get_panel_real_ofsy(panel);
      }
      else {
        pa_sizex = get_panel_real_ofsx(panel) + panel->sizex;
        pa_sizey = panel->ofsy + get_panel_size_y(panel);
      }

      sizex = max_ii(sizex, pa_sizex);
      sizey = min_ii(sizey, pa_sizey);
    }
  }

  if (sizex == 0) {
    sizex = UI_PANEL_WIDTH;
  }
  if (sizey == 0) {
    sizey = -UI_PANEL_WIDTH;
  }

  *r_x = sizex;
  *r_y = sizey;
}

static void ui_do_animate(bContext *C, Panel *panel)
{
  uiHandlePanelData *data = panel->activedata;
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  float fac;

  fac = (PIL_check_seconds_timer() - data->starttime) / ANIMATION_TIME;
  fac = min_ff(sqrtf(fac), 1.0f);

  /* for max 1 second, interpolate positions */
  if (uiAlignPanelStep(area, region, fac, false)) {
    ED_region_tag_redraw(region);
  }
  else {
    fac = 1.0f;
  }

  if (fac >= 1.0f) {
    /* Store before data is freed. */
    const bool is_drag_drop = data->is_drag_drop;

    panel_activate_state(C, panel, PANEL_STATE_EXIT);
    if (is_drag_drop) {
      /* Note: doing this in #panel_activate_state would require removing const for context in many
       * other places. */
      reorder_instanced_panel_list(C, region, panel);
    }
    return;
  }
}

static void panel_list_clear_active(ListBase *lb)
{
  /* set all panels as inactive, so that at the end we know
   * which ones were used */
  LISTBASE_FOREACH (Panel *, panel, lb) {
    if (panel->runtime_flag & PNL_ACTIVE) {
      panel->runtime_flag = PNL_WAS_ACTIVE;
    }
    else {
      panel->runtime_flag = 0;
    }

    panel_list_clear_active(&panel->children);
  }
}

void UI_panels_begin(const bContext *UNUSED(C), ARegion *region)
{
  panel_list_clear_active(&region->panels);
}

/* only draws blocks with panels */
void UI_panels_end(const bContext *C, ARegion *region, int *r_x, int *r_y)
{
  ScrArea *area = CTX_wm_area(C);
  Panel *panel, *panel_first;

  /* offset contents */
  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    if (block->active && block->panel) {
      ui_offset_panel_block(block);
    }
  }

  /* re-align, possibly with animation */
  if (panels_need_realign(area, region, &panel)) {
    if (panel) {
      panel_activate_state(C, panel, PANEL_STATE_ANIMATION);
    }
    else {
      uiAlignPanelStep(area, region, 1.0, false);
    }
  }

  /* tag first panel */
  panel_first = NULL;
  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    if (block->active && block->panel) {
      if (!panel_first || block->panel->sortorder < panel_first->sortorder) {
        panel_first = block->panel;
      }
    }
  }

  if (panel_first) {
    panel_first->runtime_flag |= PNL_FIRST;
  }

  /* compute size taken up by panel */
  ui_panels_size(area, region, r_x, r_y);
}

void UI_panels_draw(const bContext *C, ARegion *region)
{
  if (region->alignment != RGN_ALIGN_FLOAT) {
    UI_ThemeClearColor(TH_BACK);
  }

  /* Draw panels, selected on top. Also in reverse order, because
   * UI blocks are added in reverse order and we need child panels
   * to draw on top. */
  LISTBASE_FOREACH_BACKWARD (uiBlock *, block, &region->uiblocks) {
    if (block->active && block->panel && !(block->panel->flag & PNL_SELECT)) {
      UI_block_draw(C, block);
    }
  }

  LISTBASE_FOREACH_BACKWARD (uiBlock *, block, &region->uiblocks) {
    if (block->active && block->panel && (block->panel->flag & PNL_SELECT)) {
      UI_block_draw(C, block);
    }
  }
}

void UI_panels_scale(ARegion *region, float new_width)
{
  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    if (block->panel) {
      float fac = new_width / (float)block->panel->sizex;
      block->panel->sizex = new_width;

      LISTBASE_FOREACH (uiBut *, but, &block->buttons) {
        but->rect.xmin *= fac;
        but->rect.xmax *= fac;
      }
    }
  }
}

/* ------------ panel merging ---------------- */

static void check_panel_overlap(ARegion *region, Panel *panel)
{
  Panel *panel_list;

  /* also called with (panel == NULL) for clear */

  for (panel_list = region->panels.first; panel_list; panel_list = panel_list->next) {
    panel_list->flag &= ~PNL_OVERLAP;
    if (panel && (panel_list != panel)) {
      if (panel_list->runtime_flag & PNL_ACTIVE) {
        float safex = 0.2, safey = 0.2;

        if (panel_list->flag & PNL_CLOSEDX) {
          safex = 0.05;
        }
        else if (panel_list->flag & PNL_CLOSEDY) {
          safey = 0.05;
        }
        else if (panel->flag & PNL_CLOSEDX) {
          safex = 0.05;
        }
        else if (panel->flag & PNL_CLOSEDY) {
          safey = 0.05;
        }

        if (panel_list->ofsx > panel->ofsx - safex * panel->sizex) {
          if (panel_list->ofsx + panel_list->sizex < panel->ofsx + (1.0f + safex) * panel->sizex) {
            if (panel_list->ofsy > panel->ofsy - safey * panel->sizey) {
              if (panel_list->ofsy + panel_list->sizey <
                  panel->ofsy + (1.0f + safey) * panel->sizey) {
                panel_list->flag |= PNL_OVERLAP;
              }
            }
          }
        }
      }
    }
  }
}

/************************ panel dragging ****************************/

#define DRAG_REGION_PAD (PNL_HEADER * 0.5)
static void ui_do_drag(const bContext *C, const wmEvent *event, Panel *panel)
{
  uiHandlePanelData *data = panel->activedata;
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  short align = panel_aligned(area, region);

  /* Keep the drag position in the region with a small pad to keep the panel visible. */
  int x = clamp_i(event->x, region->winrct.xmin, region->winrct.xmax + DRAG_REGION_PAD);
  int y = clamp_i(event->y, region->winrct.ymin, region->winrct.ymax + DRAG_REGION_PAD);

  float dx = (float)(x - data->startx);
  float dy = (float)(y - data->starty);

  /* Adjust for region zoom. */
  dx *= (float)BLI_rctf_size_x(&region->v2d.cur) / (float)BLI_rcti_size_x(&region->winrct);
  dy *= (float)BLI_rctf_size_y(&region->v2d.cur) / (float)BLI_rcti_size_y(&region->winrct);

  if (data->state == PANEL_STATE_DRAG_SCALE) {
    panel->sizex = MAX2(data->startsizex + dx, UI_PANEL_MINX);

    if (data->startsizey - dy < UI_PANEL_MINY) {
      dy = -UI_PANEL_MINY + data->startsizey;
    }

    panel->sizey = data->startsizey - dy;
    panel->ofsy = data->startofsy + dy;
  }
  else {
    /* reset the panel snapping, to allow dragging away from snapped edges */
    panel->snap = PNL_SNAP_NONE;

    /* Add the movement of the view due to edge scrolling while dragging. */
    dx += ((float)region->v2d.cur.xmin - data->start_cur_xmin);
    dy += ((float)region->v2d.cur.ymin - data->start_cur_ymin);
    panel->ofsx = data->startofsx + round_fl_to_int(dx);
    panel->ofsy = data->startofsy + round_fl_to_int(dy);
    check_panel_overlap(region, panel);

    if (align) {
      uiAlignPanelStep(area, region, 0.2f, true);
    }
  }

  ED_region_tag_redraw(region);
}
#undef DRAG_REGION_PAD

/******************* region level panel interaction *****************/

static uiPanelMouseState ui_panel_mouse_state_get(const uiBlock *block,
                                                  const Panel *panel,
                                                  const int mx,
                                                  const int my)
{
  /* open panel */
  if (panel->flag & PNL_CLOSEDX) {
    if ((block->rect.xmin <= mx) && (block->rect.xmin + PNL_HEADER >= mx)) {
      return PANEL_MOUSE_INSIDE_HEADER;
    }
  }
  /* outside left/right side */
  else if ((block->rect.xmin > mx) || (block->rect.xmax < mx)) {
    /* pass */
  }
  else if ((block->rect.ymax <= my) && (block->rect.ymax + PNL_HEADER >= my)) {
    return PANEL_MOUSE_INSIDE_HEADER;
  }
  /* open panel */
  else if (!(panel->flag & PNL_CLOSEDY)) {
    if (panel->control & UI_PNL_SCALE) {
      if (block->rect.xmax - PNL_HEADER <= mx) {
        if (block->rect.ymin + PNL_HEADER >= my) {
          return PANEL_MOUSE_INSIDE_SCALE;
        }
      }
    }
    if ((block->rect.xmin <= mx) && (block->rect.xmax >= mx)) {
      if ((block->rect.ymin <= my) && (block->rect.ymax + PNL_HEADER >= my)) {
        return PANEL_MOUSE_INSIDE_CONTENT;
      }
    }
  }
  return PANEL_MOUSE_OUTSIDE;
}

typedef struct uiPanelDragCollapseHandle {
  bool was_first_open;
  int xy_init[2];
} uiPanelDragCollapseHandle;

static void ui_panel_drag_collapse_handler_remove(bContext *UNUSED(C), void *userdata)
{
  uiPanelDragCollapseHandle *dragcol_data = userdata;
  MEM_freeN(dragcol_data);
}

static void ui_panel_drag_collapse(bContext *C,
                                   uiPanelDragCollapseHandle *dragcol_data,
                                   const int xy_dst[2])
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
  Panel *panel;

  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    float xy_a_block[2] = {UNPACK2(dragcol_data->xy_init)};
    float xy_b_block[2] = {UNPACK2(xy_dst)};
    rctf rect = block->rect;
    int oldflag;
    const bool is_horizontal = (panel_aligned(area, region) == BUT_HORIZONTAL);

    if ((panel = block->panel) == 0 || (panel->type && (panel->type->flag & PNL_NO_HEADER))) {
      continue;
    }
    oldflag = panel->flag;

    /* lock one axis */
    if (is_horizontal) {
      xy_b_block[1] = dragcol_data->xy_init[1];
    }
    else {
      xy_b_block[0] = dragcol_data->xy_init[0];
    }

    /* use cursor coords in block space */
    ui_window_to_block_fl(region, block, &xy_a_block[0], &xy_a_block[1]);
    ui_window_to_block_fl(region, block, &xy_b_block[0], &xy_b_block[1]);

    /* set up rect to match header size */
    rect.ymin = rect.ymax;
    rect.ymax = rect.ymin + PNL_HEADER;
    if (panel->flag & PNL_CLOSEDX) {
      rect.xmax = rect.xmin + PNL_HEADER;
    }

    /* touch all panels between last mouse coord and the current one */
    if (BLI_rctf_isect_segment(&rect, xy_a_block, xy_b_block)) {
      /* force panel to close */
      if (dragcol_data->was_first_open == true) {
        panel->flag |= (is_horizontal ? PNL_CLOSEDX : PNL_CLOSEDY);
      }
      /* force panel to open */
      else {
        panel->flag &= ~PNL_CLOSED;
      }

      /* if panel->flag has changed this means a panel was opened/closed here */
      if (panel->flag != oldflag) {
        panel_activate_state(C, panel, PANEL_STATE_ANIMATION);
      }
    }
  }
  /* Update the instanced panel data expand flags with the changes made here. */
  set_panels_list_data_expand_flag(C, region);
}

/**
 * Panel drag-collapse (modal handler)
 * Clicking and dragging over panels toggles their collapse state based on the panel that was first
 * dragged over. If it was open all affected panels incl the initial one are closed and vice versa.
 */
static int ui_panel_drag_collapse_handler(bContext *C, const wmEvent *event, void *userdata)
{
  wmWindow *win = CTX_wm_window(C);
  uiPanelDragCollapseHandle *dragcol_data = userdata;
  short retval = WM_UI_HANDLER_CONTINUE;

  switch (event->type) {
    case MOUSEMOVE:
      ui_panel_drag_collapse(C, dragcol_data, &event->x);

      retval = WM_UI_HANDLER_BREAK;
      break;
    case LEFTMOUSE:
      if (event->val == KM_RELEASE) {
        /* done! */
        WM_event_remove_ui_handler(&win->modalhandlers,
                                   ui_panel_drag_collapse_handler,
                                   ui_panel_drag_collapse_handler_remove,
                                   dragcol_data,
                                   true);
        ui_panel_drag_collapse_handler_remove(C, dragcol_data);
      }
      /* don't let any left-mouse event fall through! */
      retval = WM_UI_HANDLER_BREAK;
      break;
  }

  return retval;
}

static void ui_panel_drag_collapse_handler_add(const bContext *C, const bool was_open)
{
  wmWindow *win = CTX_wm_window(C);
  const wmEvent *event = win->eventstate;
  uiPanelDragCollapseHandle *dragcol_data = MEM_mallocN(sizeof(*dragcol_data), __func__);

  dragcol_data->was_first_open = was_open;
  copy_v2_v2_int(dragcol_data->xy_init, &event->x);

  WM_event_add_ui_handler(C,
                          &win->modalhandlers,
                          ui_panel_drag_collapse_handler,
                          ui_panel_drag_collapse_handler_remove,
                          dragcol_data,
                          0);
}

/* this function is supposed to call general window drawing too */
/* also it supposes a block has panel, and isn't a menu */
static void ui_handle_panel_header(
    const bContext *C, uiBlock *block, int mx, int my, int event, short ctrl, short shift)
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);
#ifdef USE_PIN_HIDDEN
  const bool show_pin = UI_panel_category_is_visible(region) &&
                        (block->panel->type->parent == NULL) && (block->panel->flag & PNL_PIN);
#else
  const bool show_pin = UI_panel_category_is_visible(region) &&
                        (block->panel->type->parent == NULL);
#endif
  const bool is_subpanel = (block->panel->type && block->panel->type->parent);
  const bool show_drag = !is_subpanel;

  int align = panel_aligned(area, region), button = 0;

  rctf rect_drag, rect_pin;
  float rect_leftmost;

  /* drag and pin rect's */
  rect_drag = block->rect;
  rect_drag.xmin = block->rect.xmax - (PNL_ICON * 1.5f);
  rect_pin = rect_drag;
  if (show_pin) {
    BLI_rctf_translate(&rect_pin, -PNL_ICON, 0.0f);
  }
  rect_leftmost = rect_pin.xmin;

  /* mouse coordinates in panel space! */

  /* XXX weak code, currently it assumes layout style for location of widgets */

  /* check open/collapsed button */
  if (event == EVT_RETKEY) {
    button = 1;
  }
  else if (event == EVT_AKEY) {
    button = 1;
  }
  else if (ELEM(event, 0, EVT_RETKEY, LEFTMOUSE) && shift) {
    if (block->panel->type->parent == NULL) {
      block->panel->flag ^= PNL_PIN;
      button = 2;
    }
  }
  else if (block->panel->flag & PNL_CLOSEDX) {
    if (my >= block->rect.ymax) {
      button = 1;
    }
  }
  else if (block->panel->control & UI_PNL_CLOSE) {
    /* whole of header can be used to collapse panel (except top-right corner) */
    if (mx <= block->rect.xmax - 8 - PNL_ICON) {
      button = 2;
    }
    // else if (mx <= block->rect.xmin + 10 + 2 * PNL_ICON + 2) {
    //  button = 1;
    //}
  }
  else if (mx < rect_leftmost) {
    button = 1;
  }

  if (button) {
    if (button == 2) { /* close */
      ED_region_tag_redraw(region);
    }
    else {
      /* Collapse and expand panels. */

      if (ctrl) {
        /* For parent panels, collapse all other panels or toggle children. */
        if (block->panel->type != NULL && block->panel->type->parent == NULL) {
          if (block->panel->flag & PNL_CLOSED || BLI_listbase_is_empty(&block->panel->children)) {
            panels_collapse_all(C, area, region, block->panel);

            /* Reset the view - we don't want to display a view without content. */
            UI_view2d_offset(&region->v2d, 0.0f, 1.0f);
          }
          else {
            const int closed_flag = (align == BUT_HORIZONTAL) ? PNL_CLOSEDX : PNL_CLOSEDY;
            /* If a panel has sub-panels and it's open, toggle the expansion
             * of the sub-panels (based on the expansion of the first subpanel). */
            Panel *first_child = block->panel->children.first;
            BLI_assert(first_child != NULL);
            panel_set_flag_recursive(
                block->panel, closed_flag, (first_child->flag & PNL_CLOSED) == 0);
            block->panel->flag |= closed_flag;
          }
        }
      }

      if (block->panel->flag & PNL_CLOSED) {
        block->panel->flag &= ~PNL_CLOSED;
        /* snap back up so full panel aligns with screen edge */
        if (block->panel->snap & PNL_SNAP_BOTTOM) {
          block->panel->ofsy = 0;
        }

        if (event == LEFTMOUSE) {
          ui_panel_drag_collapse_handler_add(C, false);
        }
      }
      else if (align == BUT_HORIZONTAL) {
        block->panel->flag |= PNL_CLOSEDX;

        if (event == LEFTMOUSE) {
          ui_panel_drag_collapse_handler_add(C, true);
        }
      }
      else {
        /* snap down to bottom screen edge */
        block->panel->flag |= PNL_CLOSEDY;
        if (block->panel->snap & PNL_SNAP_BOTTOM) {
          block->panel->ofsy = -block->panel->sizey;
        }

        if (event == LEFTMOUSE) {
          ui_panel_drag_collapse_handler_add(C, true);
        }
      }

      set_panels_list_data_expand_flag(C, region);
    }

    if (align) {
      panel_activate_state(C, block->panel, PANEL_STATE_ANIMATION);
    }
    else {
      /* FIXME: this doesn't update the panel drawing, assert to avoid debugging why this is.
       * We could fix this in the future if it's ever needed. */
      BLI_assert(0);
      ED_region_tag_redraw(region);
    }
  }
  else if (show_drag && BLI_rctf_isect_x(&rect_drag, mx)) {
    /* XXX, for now don't allow dragging in floating windows yet. */
    if (region->alignment == RGN_ALIGN_FLOAT) {
      return;
    }
    panel_activate_state(C, block->panel, PANEL_STATE_DRAG);
  }
  else if (show_pin && BLI_rctf_isect_x(&rect_pin, mx)) {
    block->panel->flag ^= PNL_PIN;
    ED_region_tag_redraw(region);
  }
}

bool UI_panel_category_is_visible(const ARegion *region)
{
  /* more than one */
  return region->panels_category.first &&
         region->panels_category.first != region->panels_category.last;
}

PanelCategoryDyn *UI_panel_category_find(ARegion *region, const char *idname)
{
  return BLI_findstring(&region->panels_category, idname, offsetof(PanelCategoryDyn, idname));
}

PanelCategoryStack *UI_panel_category_active_find(ARegion *region, const char *idname)
{
  return BLI_findstring(
      &region->panels_category_active, idname, offsetof(PanelCategoryStack, idname));
}

static void ui_panel_category_active_set(ARegion *region, const char *idname, bool fallback)
{
  ListBase *lb = &region->panels_category_active;
  PanelCategoryStack *pc_act = UI_panel_category_active_find(region, idname);

  if (pc_act) {
    BLI_remlink(lb, pc_act);
  }
  else {
    pc_act = MEM_callocN(sizeof(PanelCategoryStack), __func__);
    BLI_strncpy(pc_act->idname, idname, sizeof(pc_act->idname));
  }

  if (fallback) {
    /* For fallbacks, add at the end so explicitly chosen categories have priority. */
    BLI_addtail(lb, pc_act);
  }
  else {
    BLI_addhead(lb, pc_act);
  }

  /* validate all active panels, we could do this on load,
   * they are harmless - but we should remove somewhere.
   * (addons could define own and gather cruft over time) */
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
    PanelCategoryDyn *pc_dyn = region->panels_category.first;
    if (pc_dyn) {
      ui_panel_category_active_set(region, pc_dyn->idname, true);
      return pc_dyn->idname;
    }
  }

  return NULL;
}

PanelCategoryDyn *UI_panel_category_find_mouse_over_ex(ARegion *region, const int x, const int y)
{
  LISTBASE_FOREACH (PanelCategoryDyn *, ptd, &region->panels_category) {
    if (BLI_rcti_isect_pt(&ptd->rect, x, y)) {
      return ptd;
    }
  }

  return NULL;
}

PanelCategoryDyn *UI_panel_category_find_mouse_over(ARegion *region, const wmEvent *event)
{
  return UI_panel_category_find_mouse_over_ex(region, event->mval[0], event->mval[1]);
}

void UI_panel_category_add(ARegion *region, const char *name)
{
  PanelCategoryDyn *pc_dyn = MEM_callocN(sizeof(*pc_dyn), __func__);
  BLI_addtail(&region->panels_category, pc_dyn);

  BLI_strncpy(pc_dyn->idname, name, sizeof(pc_dyn->idname));

  /* 'pc_dyn->rect' must be set on draw */
}

void UI_panel_category_clear_all(ARegion *region)
{
  BLI_freelistN(&region->panels_category);
}

static void imm_buf_append(
    float vbuf[][2], uchar cbuf[][3], float x, float y, const uchar col[3], int *index)
{
  ARRAY_SET_ITEMS(vbuf[*index], x, y);
  ARRAY_SET_ITEMS(cbuf[*index], UNPACK3(col));
  (*index)++;
}

/* based on UI_draw_roundbox, check on making a version which allows us to skip some sides */
static void ui_panel_category_draw_tab(bool filled,
                                       float minx,
                                       float miny,
                                       float maxx,
                                       float maxy,
                                       float rad,
                                       const int roundboxtype,
                                       const bool use_highlight,
                                       const bool use_shadow,
                                       const bool use_flip_x,
                                       const uchar highlight_fade[3],
                                       const uchar col[3])
{
  float vec[4][2] = {{0.195, 0.02}, {0.55, 0.169}, {0.831, 0.45}, {0.98, 0.805}};
  int a;

  /* mult */
  for (a = 0; a < 4; a++) {
    mul_v2_fl(vec[a], rad);
  }

  uint vert_len = 0;
  if (use_highlight) {
    vert_len += (roundboxtype & UI_CNR_TOP_RIGHT) ? 6 : 1;
    vert_len += (roundboxtype & UI_CNR_TOP_LEFT) ? 6 : 1;
  }
  if (use_highlight && !use_shadow) {
    vert_len++;
  }
  else {
    vert_len += (roundboxtype & UI_CNR_BOTTOM_RIGHT) ? 6 : 1;
    vert_len += (roundboxtype & UI_CNR_BOTTOM_LEFT) ? 6 : 1;
  }
  /* Maximum size. */
  float vbuf[24][2];
  uchar cbuf[24][3];
  int buf_index = 0;

  /* start with corner right-top */
  if (use_highlight) {
    if (roundboxtype & UI_CNR_TOP_RIGHT) {
      imm_buf_append(vbuf, cbuf, maxx, maxy - rad, col, &buf_index);
      for (a = 0; a < 4; a++) {
        imm_buf_append(vbuf, cbuf, maxx - vec[a][1], maxy - rad + vec[a][0], col, &buf_index);
      }
      imm_buf_append(vbuf, cbuf, maxx - rad, maxy, col, &buf_index);
    }
    else {
      imm_buf_append(vbuf, cbuf, maxx, maxy, col, &buf_index);
    }

    /* corner left-top */
    if (roundboxtype & UI_CNR_TOP_LEFT) {
      imm_buf_append(vbuf, cbuf, minx + rad, maxy, col, &buf_index);
      for (a = 0; a < 4; a++) {
        imm_buf_append(vbuf, cbuf, minx + rad - vec[a][0], maxy - vec[a][1], col, &buf_index);
      }
      imm_buf_append(vbuf, cbuf, minx, maxy - rad, col, &buf_index);
    }
    else {
      imm_buf_append(vbuf, cbuf, minx, maxy, col, &buf_index);
    }
  }

  if (use_highlight && !use_shadow) {
    imm_buf_append(
        vbuf, cbuf, minx, miny + rad, highlight_fade ? col : highlight_fade, &buf_index);
  }
  else {
    /* corner left-bottom */
    if (roundboxtype & UI_CNR_BOTTOM_LEFT) {
      imm_buf_append(vbuf, cbuf, minx, miny + rad, col, &buf_index);
      for (a = 0; a < 4; a++) {
        imm_buf_append(vbuf, cbuf, minx + vec[a][1], miny + rad - vec[a][0], col, &buf_index);
      }
      imm_buf_append(vbuf, cbuf, minx + rad, miny, col, &buf_index);
    }
    else {
      imm_buf_append(vbuf, cbuf, minx, miny, col, &buf_index);
    }

    /* corner right-bottom */
    if (roundboxtype & UI_CNR_BOTTOM_RIGHT) {
      imm_buf_append(vbuf, cbuf, maxx - rad, miny, col, &buf_index);
      for (a = 0; a < 4; a++) {
        imm_buf_append(vbuf, cbuf, maxx - rad + vec[a][0], miny + vec[a][1], col, &buf_index);
      }
      imm_buf_append(vbuf, cbuf, maxx, miny + rad, col, &buf_index);
    }
    else {
      imm_buf_append(vbuf, cbuf, maxx, miny, col, &buf_index);
    }
  }

  if (use_flip_x) {
    float midx = (minx + maxx) / 2.0f;
    for (int i = 0; i < buf_index; i++) {
      vbuf[i][0] = midx - (vbuf[i][0] - midx);
    }
  }

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint color = GPU_vertformat_attr_add(
      format, "color", GPU_COMP_U8, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);

  immBindBuiltinProgram(GPU_SHADER_2D_SMOOTH_COLOR);
  immBegin(filled ? GPU_PRIM_TRI_FAN : GPU_PRIM_LINE_STRIP, vert_len);
  for (int i = 0; i < buf_index; i++) {
    immAttr3ubv(color, cbuf[i]);
    immVertex2fv(pos, vbuf[i]);
  }
  immEnd();
  immUnbindProgram();
}

/**
 * Draw vertical tabs on the left side of the region,
 * one tab per category.
 */
void UI_panel_category_draw_all(ARegion *region, const char *category_id_active)
{
  /* no tab outlines for */
  // #define USE_FLAT_INACTIVE
  const bool is_left = RGN_ALIGN_ENUM_FROM_MASK(region->alignment != RGN_ALIGN_RIGHT);
  View2D *v2d = &region->v2d;
  const uiStyle *style = UI_style_get();
  const uiFontStyle *fstyle = &style->widget;
  const int fontid = fstyle->uifont_id;
  short fstyle_points = fstyle->points;
  const float aspect = ((uiBlock *)region->uiblocks.first)->aspect;
  const float zoom = 1.0f / aspect;
  const int px = max_ii(1, round_fl_to_int(U.pixelsize));
  const int px_x_sign = is_left ? px : -px;
  const int category_tabs_width = round_fl_to_int(UI_PANEL_CATEGORY_MARGIN_WIDTH * zoom);
  const float dpi_fac = UI_DPI_FAC;
  /* padding of tabs around text */
  const int tab_v_pad_text = round_fl_to_int((2 + ((px * 3) * dpi_fac)) * zoom);
  /* padding between tabs */
  const int tab_v_pad = round_fl_to_int((4 + (2 * px * dpi_fac)) * zoom);
  const float tab_curve_radius = ((px * 3) * dpi_fac) * zoom;
  /* We flip the tab drawing, so always use these flags. */
  const int roundboxtype = UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT;
  bool is_alpha;
  bool do_scaletabs = false;
#ifdef USE_FLAT_INACTIVE
  bool is_active_prev = false;
#endif
  float scaletabs = 1.0f;
  /* same for all tabs */
  /* intentionally dont scale by 'px' */
  const int rct_xmin = is_left ? v2d->mask.xmin + 3 : (v2d->mask.xmax - category_tabs_width);
  const int rct_xmax = is_left ? v2d->mask.xmin + category_tabs_width : (v2d->mask.xmax - 3);
  const int text_v_ofs = (rct_xmax - rct_xmin) * 0.3f;

  int y_ofs = tab_v_pad;

  /* Primary theme colors */
  uchar theme_col_back[4];
  uchar theme_col_text[3];
  uchar theme_col_text_hi[3];

  /* Tab colors */
  uchar theme_col_tab_bg[4];
  uchar theme_col_tab_active[3];
  uchar theme_col_tab_inactive[3];

  /* Secondary theme colors */
  uchar theme_col_tab_outline[3];
  uchar theme_col_tab_divider[3]; /* line that divides tabs from the main region */
  uchar theme_col_tab_highlight[3];
  uchar theme_col_tab_highlight_inactive[3];

  UI_GetThemeColor4ubv(TH_BACK, theme_col_back);
  UI_GetThemeColor3ubv(TH_TEXT, theme_col_text);
  UI_GetThemeColor3ubv(TH_TEXT_HI, theme_col_text_hi);

  UI_GetThemeColor4ubv(TH_TAB_BACK, theme_col_tab_bg);
  UI_GetThemeColor3ubv(TH_TAB_ACTIVE, theme_col_tab_active);
  UI_GetThemeColor3ubv(TH_TAB_INACTIVE, theme_col_tab_inactive);
  UI_GetThemeColor3ubv(TH_TAB_OUTLINE, theme_col_tab_outline);

  interp_v3_v3v3_uchar(theme_col_tab_divider, theme_col_back, theme_col_tab_outline, 0.3f);
  interp_v3_v3v3_uchar(theme_col_tab_highlight, theme_col_back, theme_col_text_hi, 0.2f);
  interp_v3_v3v3_uchar(
      theme_col_tab_highlight_inactive, theme_col_tab_inactive, theme_col_text_hi, 0.12f);

  is_alpha = (region->overlap && (theme_col_back[3] != 255));

  if (fstyle->kerning == 1) {
    BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
  }

  BLF_enable(fontid, BLF_ROTATION);
  BLF_rotation(fontid, M_PI_2);
  // UI_fontstyle_set(&style->widget);
  ui_fontscale(&fstyle_points, aspect / (U.pixelsize * 1.1f));
  BLF_size(fontid, fstyle_points, U.dpi);

  /* Check the region type supports categories to avoid an assert
   * for showing 3D view panels in the properties space. */
  if ((1 << region->regiontype) & RGN_TYPE_HAS_CATEGORY_MASK) {
    BLI_assert(UI_panel_category_is_visible(region));
  }

  /* calculate tab rect's and check if we need to scale down */
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

  if (y_ofs > BLI_rcti_size_y(&v2d->mask)) {
    scaletabs = (float)BLI_rcti_size_y(&v2d->mask) / (float)y_ofs;

    LISTBASE_FOREACH (PanelCategoryDyn *, pc_dyn, &region->panels_category) {
      rcti *rct = &pc_dyn->rect;
      rct->ymin = ((rct->ymin - v2d->mask.ymax) * scaletabs) + v2d->mask.ymax;
      rct->ymax = ((rct->ymax - v2d->mask.ymax) * scaletabs) + v2d->mask.ymax;
    }

    do_scaletabs = true;
  }

  /* begin drawing */
  GPU_line_smooth(true);

  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  /* draw the background */
  if (is_alpha) {
    GPU_blend(true);
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
    GPU_blend(false);
  }

  immUnbindProgram();

  const int divider_xmin = is_left ? (v2d->mask.xmin + (category_tabs_width - px)) :
                                     (v2d->mask.xmax - category_tabs_width) + px;
  const int divider_xmax = is_left ? (v2d->mask.xmin + category_tabs_width) :
                                     (v2d->mask.xmax - (category_tabs_width + px)) + px;

  LISTBASE_FOREACH (PanelCategoryDyn *, pc_dyn, &region->panels_category) {
    const rcti *rct = &pc_dyn->rect;
    const char *category_id = pc_dyn->idname;
    const char *category_id_draw = IFACE_(category_id);
    int category_width = BLI_rcti_size_y(rct) - (tab_v_pad_text * 2);
    size_t category_draw_len = BLF_DRAW_STR_DUMMY_MAX;
    // int category_width = BLF_width(fontid, category_id_draw, BLF_DRAW_STR_DUMMY_MAX);

    const bool is_active = STREQ(category_id, category_id_active);

    GPU_blend(true);

#ifdef USE_FLAT_INACTIVE
    if (is_active)
#endif
    {
      const bool use_flip_x = !is_left;
      ui_panel_category_draw_tab(true,
                                 rct->xmin,
                                 rct->ymin,
                                 rct->xmax,
                                 rct->ymax,
                                 tab_curve_radius - px,
                                 roundboxtype,
                                 true,
                                 true,
                                 use_flip_x,
                                 NULL,
                                 is_active ? theme_col_tab_active : theme_col_tab_inactive);

      /* tab outline */
      ui_panel_category_draw_tab(false,
                                 rct->xmin - px_x_sign,
                                 rct->ymin - px,
                                 rct->xmax - px_x_sign,
                                 rct->ymax + px,
                                 tab_curve_radius,
                                 roundboxtype,
                                 true,
                                 true,
                                 use_flip_x,
                                 NULL,
                                 theme_col_tab_outline);

      /* tab highlight (3d look) */
      ui_panel_category_draw_tab(false,
                                 rct->xmin,
                                 rct->ymin,
                                 rct->xmax,
                                 rct->ymax,
                                 tab_curve_radius,
                                 roundboxtype,
                                 true,
                                 false,
                                 use_flip_x,
                                 is_active ? theme_col_back : theme_col_tab_inactive,
                                 is_active ? theme_col_tab_highlight :
                                             theme_col_tab_highlight_inactive);
    }

    /* tab blackline */
    if (!is_active) {
      pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
      immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

      immUniformColor3ubv(theme_col_tab_divider);
      immRecti(pos, divider_xmin, rct->ymin - tab_v_pad, divider_xmax, rct->ymax + tab_v_pad);
      immUnbindProgram();
    }

    if (do_scaletabs) {
      category_draw_len = BLF_width_to_strlen(
          fontid, category_id_draw, category_draw_len, category_width, NULL);
    }

    BLF_position(fontid, rct->xmax - text_v_ofs, rct->ymin + tab_v_pad_text, 0.0f);

    /* tab titles */

    /* draw white shadow to give text more depth */
    BLF_color3ubv(fontid, theme_col_text);

    /* main tab title */
    BLF_draw(fontid, category_id_draw, category_draw_len);

    GPU_blend(false);

    /* tab blackline remaining (last tab) */
    pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
    if (pc_dyn->prev == NULL) {
      immUniformColor3ubv(theme_col_tab_divider);
      immRecti(pos, divider_xmin, rct->ymax + px, divider_xmax, v2d->mask.ymax);
    }
    if (pc_dyn->next == NULL) {
      immUniformColor3ubv(theme_col_tab_divider);
      immRecti(pos, divider_xmin, 0, divider_xmax, rct->ymin);
    }

#ifdef USE_FLAT_INACTIVE
    /* draw line between inactive tabs */
    if (is_active == false && is_active_prev == false && pc_dyn->prev) {
      immUniformColor3ubv(theme_col_tab_divider);
      immRecti(pos,
               v2d->mask.xmin + (category_tabs_width / 5),
               rct->ymax + px,
               (v2d->mask.xmin + category_tabs_width) - (category_tabs_width / 5),
               rct->ymax + (px * 3));
    }

    is_active_prev = is_active;
#endif
    immUnbindProgram();

    /* not essential, but allows events to be handled right up until the region edge [#38171] */
    if (is_left) {
      pc_dyn->rect.xmin = v2d->mask.xmin;
    }
    else {
      pc_dyn->rect.xmax = v2d->mask.xmax;
    }
  }

  GPU_line_smooth(false);

  BLF_disable(fontid, BLF_ROTATION);

  if (fstyle->kerning == 1) {
    BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
  }

#undef USE_FLAT_INACTIVE
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

  /* if mouse is inside non-tab region, ctrl key is required */
  if (is_mousewheel && !event->ctrl && !inside_tabregion) {
    return WM_UI_HANDLER_CONTINUE;
  }

  if (active_but && ui_but_supports_cycling(active_but)) {
    /* skip - exception to make cycling buttons
     * using ctrl+mousewheel work in tabbed regions */
  }
  else {
    const char *category = UI_panel_category_active_get(region, false);
    if (LIKELY(category)) {
      PanelCategoryDyn *pc_dyn = UI_panel_category_find(region, category);
      if (LIKELY(pc_dyn)) {
        if (is_mousewheel) {
          /* we can probably get rid of this and only allow ctrl+tabbing */
          pc_dyn = (event->type == WHEELDOWNMOUSE) ? pc_dyn->next : pc_dyn->prev;
        }
        else {
          const bool backwards = event->shift;
          pc_dyn = backwards ? pc_dyn->prev : pc_dyn->next;
          if (!pc_dyn) {
            /* proper cyclic behavior,
             * back to first/last category (only used for ctrl+tab) */
            pc_dyn = backwards ? region->panels_category.last : region->panels_category.first;
          }
        }

        if (pc_dyn) {
          /* intentionally don't reset scroll in this case,
           * this allows for quick browsing between tabs */
          UI_panel_category_active_set(region, pc_dyn->idname);
          ED_region_tag_redraw(region);
        }
      }
    }
    return WM_UI_HANDLER_BREAK;
  }

  return WM_UI_HANDLER_CONTINUE;
}

/* XXX should become modal keymap */
/* AKey is opening/closing panels, independent of button state now */

int ui_handler_panel_region(bContext *C,
                            const wmEvent *event,
                            ARegion *region,
                            const uiBut *active_but)
{
  Panel *panel;
  int retval, mx, my;
  bool has_category_tabs = UI_panel_category_is_visible(region);

  retval = WM_UI_HANDLER_CONTINUE;

  /* Scrollbars can overlap panels now, they have handling priority. */
  if (UI_view2d_mouse_in_scrollers(region, &region->v2d, event->x, event->y)) {
    return retval;
  }

  /* handle category tabs */
  if (has_category_tabs) {
    if (event->val == KM_PRESS) {
      if (event->type == LEFTMOUSE) {
        PanelCategoryDyn *pc_dyn = UI_panel_category_find_mouse_over(region, event);
        if (pc_dyn) {
          UI_panel_category_active_set(region, pc_dyn->idname);
          ED_region_tag_redraw(region);

          /* reset scroll to the top [#38348] */
          UI_view2d_offset(&region->v2d, -1.0f, 1.0f);

          retval = WM_UI_HANDLER_BREAK;
        }
      }
      else if ((event->type == EVT_TABKEY && event->ctrl) ||
               ELEM(event->type, WHEELUPMOUSE, WHEELDOWNMOUSE)) {
        /* cycle tabs */
        retval = ui_handle_panel_category_cycling(event, region, active_but);
      }
    }
  }

  if (retval == WM_UI_HANDLER_BREAK) {
    return retval;
  }

  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    uiPanelMouseState mouse_state;

    mx = event->x;
    my = event->y;
    ui_window_to_block(region, block, &mx, &my);

    /* checks for mouse position inside */
    panel = block->panel;

    if (!panel) {
      continue;
    }
    /* XXX - accessed freed panels when scripts reload, need to fix. */
    if (panel->type && panel->type->flag & PNL_NO_HEADER) {
      continue;
    }

    mouse_state = ui_panel_mouse_state_get(block, panel, mx, my);

    /* XXX hardcoded key warning */
    if (ELEM(mouse_state, PANEL_MOUSE_INSIDE_CONTENT, PANEL_MOUSE_INSIDE_HEADER) &&
        event->val == KM_PRESS) {
      if (event->type == EVT_AKEY &&
          ((event->ctrl + event->oskey + event->shift + event->alt) == 0)) {

        if (panel->flag & PNL_CLOSEDY) {
          if ((block->rect.ymax <= my) && (block->rect.ymax + PNL_HEADER >= my)) {
            ui_handle_panel_header(C, block, mx, my, event->type, event->ctrl, event->shift);
          }
        }
        else {
          ui_handle_panel_header(C, block, mx, my, event->type, event->ctrl, event->shift);
        }

        retval = WM_UI_HANDLER_BREAK;
        continue;
      }
    }

    /* on active button, do not handle panels */
    if (ui_region_find_active_but(region) != NULL) {
      continue;
    }

    if (ELEM(mouse_state, PANEL_MOUSE_INSIDE_CONTENT, PANEL_MOUSE_INSIDE_HEADER)) {

      if (event->val == KM_PRESS) {

        /* open close on header */
        if (ELEM(event->type, EVT_RETKEY, EVT_PADENTER)) {
          if (mouse_state == PANEL_MOUSE_INSIDE_HEADER) {
            ui_handle_panel_header(C, block, mx, my, EVT_RETKEY, event->ctrl, event->shift);
            retval = WM_UI_HANDLER_BREAK;
            break;
          }
        }
        else if (event->type == LEFTMOUSE) {
          /* all inside clicks should return in break - overlapping/float panels */
          retval = WM_UI_HANDLER_BREAK;

          if (mouse_state == PANEL_MOUSE_INSIDE_HEADER) {
            ui_handle_panel_header(C, block, mx, my, event->type, event->ctrl, event->shift);
            retval = WM_UI_HANDLER_BREAK;
            break;
          }
          else if ((mouse_state == PANEL_MOUSE_INSIDE_SCALE) && !(panel->flag & PNL_CLOSED)) {
            panel_activate_state(C, panel, PANEL_STATE_DRAG_SCALE);
            retval = WM_UI_HANDLER_BREAK;
            break;
          }
        }
        else if (event->type == RIGHTMOUSE) {
          if (mouse_state == PANEL_MOUSE_INSIDE_HEADER) {
            ui_popup_context_menu_for_panel(C, region, block->panel);
            retval = WM_UI_HANDLER_BREAK;
            break;
          }
        }
        else if (event->type == EVT_ESCKEY) {
          /*XXX 2.50*/
#if 0
          if (block->handler) {
            rem_blockhandler(area, block->handler);
            ED_region_tag_redraw(region);
            retval = WM_UI_HANDLER_BREAK;
          }
#endif
        }
        else if (event->type == EVT_PADPLUSKEY || event->type == EVT_PADMINUS) {
#if 0 /* XXX make float panel exception? */
          int zoom = 0;

          /* if panel is closed, only zoom if mouse is over the header */
          if (panel->flag & (PNL_CLOSEDX | PNL_CLOSEDY)) {
            if (inside_header) {
              zoom = 1;
            }
          }
          else {
            zoom = 1;
          }

          if (zoom) {
            ScrArea *area = CTX_wm_area(C);
            SpaceLink *sl = area->spacedata.first;

            if (area->spacetype != SPACE_PROPERTIES) {
              if (!(panel->control & UI_PNL_SCALE)) {
                if (event->type == PADPLUSKEY) {
                  sl->blockscale += 0.1;
                }
                else {
                  sl->blockscale -= 0.1;
                }
                CLAMP(sl->blockscale, 0.6, 1.0);

                ED_region_tag_redraw(region);
                retval = WM_UI_HANDLER_BREAK;
              }
            }
          }
#endif
        }
      }
    }
  }

  return retval;
}

/**************** window level modal panel interaction **************/

/* note, this is modal handler and should not swallow events for animation */
static int ui_handler_panel(bContext *C, const wmEvent *event, void *userdata)
{
  Panel *panel = userdata;
  uiHandlePanelData *data = panel->activedata;

  /* verify if we can stop */
  if (event->type == LEFTMOUSE && event->val == KM_RELEASE) {
    ScrArea *area = CTX_wm_area(C);
    ARegion *region = CTX_wm_region(C);
    int align = panel_aligned(area, region);

    if (align) {
      panel_activate_state(C, panel, PANEL_STATE_ANIMATION);
    }
    else {
      panel_activate_state(C, panel, PANEL_STATE_EXIT);
    }
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

  data = panel->activedata;

  if (data && data->state == PANEL_STATE_ANIMATION) {
    return WM_UI_HANDLER_CONTINUE;
  }
  else {
    return WM_UI_HANDLER_BREAK;
  }
}

static void ui_handler_remove_panel(bContext *C, void *userdata)
{
  Panel *panel = userdata;

  panel_activate_state(C, panel, PANEL_STATE_EXIT);
}

static void panel_activate_state(const bContext *C, Panel *panel, uiHandlePanelState state)
{
  uiHandlePanelData *data = panel->activedata;
  wmWindow *win = CTX_wm_window(C);
  ARegion *region = CTX_wm_region(C);

  if (data && data->state == state) {
    return;
  }

  bool was_drag_drop = (data && data->state == PANEL_STATE_DRAG);

  /* Set selection state for the panel and its sub-panels, which need to know they are selected
   * too so they can be drawn above their parent when it's dragged. */
  if (state == PANEL_STATE_EXIT || state == PANEL_STATE_ANIMATION) {
    if (data && data->state != PANEL_STATE_ANIMATION) {
      /* XXX:
       * - the panel tabbing function call below (test_add_new_tabs()) has been commented out
       *   "It is too easy to do by accident when reordering panels,
       *   is very hard to control and use, and has no real benefit." - BillRey
       * Aligorith, 2009Sep
       */
      // test_add_new_tabs(region);   // also copies locations of tabs in dragged panel
      check_panel_overlap(region, NULL); /* clears */
    }

    panel_set_flag_recursive(panel, PNL_SELECT, false);
  }
  else {
    panel_set_flag_recursive(panel, PNL_SELECT, true);
  }

  if (data && data->animtimer) {
    WM_event_remove_timer(CTX_wm_manager(C), win, data->animtimer);
    data->animtimer = NULL;
  }

  if (state == PANEL_STATE_EXIT) {
    MEM_freeN(data);
    panel->activedata = NULL;

    WM_event_remove_ui_handler(
        &win->modalhandlers, ui_handler_panel, ui_handler_remove_panel, panel, false);
  }
  else {
    if (!data) {
      data = MEM_callocN(sizeof(uiHandlePanelData), "uiHandlePanelData");
      panel->activedata = data;

      WM_event_add_ui_handler(
          C, &win->modalhandlers, ui_handler_panel, ui_handler_remove_panel, panel, 0);
    }

    if (ELEM(state, PANEL_STATE_ANIMATION, PANEL_STATE_DRAG)) {
      data->animtimer = WM_event_add_timer(CTX_wm_manager(C), win, TIMER, ANIMATION_INTERVAL);
    }

    /* Initiate edge panning during drags so we can move beyond the initial region view. */
    if (state == PANEL_STATE_DRAG) {
      wmOperatorType *ot = WM_operatortype_find("VIEW2D_OT_edge_pan", true);
      ui_handle_afterfunc_add_operator(ot, WM_OP_INVOKE_DEFAULT, true);
    }

    data->state = state;
    data->startx = win->eventstate->x;
    data->starty = win->eventstate->y;
    data->startofsx = panel->ofsx;
    data->startofsy = panel->ofsy;
    data->startsizex = panel->sizex;
    data->startsizey = panel->sizey;
    data->start_cur_xmin = region->v2d.cur.xmin;
    data->start_cur_ymin = region->v2d.cur.ymin;
    data->starttime = PIL_check_seconds_timer();

    /* Remember drag drop state even when animating to the aligned position after dragging. */
    data->is_drag_drop = was_drag_drop;
    if (state == PANEL_STATE_DRAG) {
      data->is_drag_drop = true;
    }
  }

  ED_region_tag_redraw(region);
}

PanelType *UI_paneltype_find(int space_id, int region_id, const char *idname)
{
  SpaceType *st = BKE_spacetype_from_id(space_id);
  if (st) {
    ARegionType *art = BKE_regiontype_from_id(st, region_id);
    if (art) {
      return BLI_findstring(&art->paneltypes, idname, offsetof(PanelType, idname));
    }
  }
  return NULL;
}
