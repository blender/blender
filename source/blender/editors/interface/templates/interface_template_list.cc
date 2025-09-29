/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "BLI_fnmatch.h"
#include "BLI_function_ref.hh"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "ED_screen.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_layout.hh"
#include "UI_view2d.hh"

#include "WM_api.hh"

#include "interface_intern.hh"

using namespace blender;

/**
 * The validated data that was passed to #uiTemplateList (typically through Python).
 * Populated through #ui_template_list_data_retrieve().
 */
struct TemplateListInputData {
  PointerRNA dataptr;
  PropertyRNA *prop;
  PointerRNA active_dataptr;
  PropertyRNA *activeprop;
  const char *item_dyntip_propname;

  /* Index as stored in the input property. I.e. the index before sorting. */
  int active_item_idx;
};

/**
 * Internal wrapper for a single item in the list (well, actually stored as a vector).
 */
struct _uilist_item {
  PointerRNA item;
  int org_idx;
  int flt_flag;
};

/**
 * Container for the item vector and additional info.
 */
struct TemplateListItems {
  blender::Vector<_uilist_item> item_vec = {};
  /* Index of the active item following visual order. I.e. unlike
   * TemplateListInputData.active_item_idx, this is the index after sorting. */
  int active_item_idx = 0;
};

struct TemplateListLayoutDrawData {
  uiListDrawItemFunc draw_item;
  uiListDrawFilterFunc draw_filter;

  int rows;
  int maxrows;
  int columns;
};

struct TemplateListVisualInfo {
  int visual_items; /* Visual number of items (i.e. number of items we have room to display). */
  int start_idx;    /* Index of first item to display. */
  int end_idx;      /* Index of last item to display + 1. */
};

static void uilist_draw_item_default(uiList *ui_list,
                                     const bContext * /*C*/,
                                     uiLayout *layout,
                                     PointerRNA * /*dataptr*/,
                                     PointerRNA *itemptr,
                                     int icon,
                                     PointerRNA * /*active_dataptr*/,
                                     const char * /*active_propname*/,
                                     int /*index*/,
                                     int /*flt_flag*/)
{
  PropertyRNA *nameprop = RNA_struct_name_property(itemptr->type);

  /* Simplest one! */
  switch (ui_list->layout_type) {
    case UILST_LAYOUT_DEFAULT:
    case UILST_LAYOUT_COMPACT:
    default:
      if (nameprop) {
        layout->prop(itemptr, nameprop, RNA_NO_INDEX, 0, UI_ITEM_R_NO_BG, "", icon);
      }
      else {
        layout->label("", icon);
      }
      break;
  }
}

static void uilist_draw_filter_default(uiList *ui_list, const bContext * /*C*/, uiLayout *layout)
{
  PointerRNA listptr = RNA_pointer_create_discrete(nullptr, &RNA_UIList, ui_list);

  uiLayout *row = &layout->row(false);

  uiLayout *subrow = &row->row(true);
  subrow->prop(&listptr, "filter_name", UI_ITEM_NONE, "", ICON_NONE);
  subrow->prop(&listptr,
               "use_filter_invert",
               UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY,
               "",
               ICON_ARROW_LEFTRIGHT);

  if ((ui_list->filter_sort_flag & UILST_FLT_SORT_LOCK) == 0) {
    subrow = &row->row(true);
    subrow->prop(
        &listptr, "use_filter_sort_alpha", UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
    subrow->prop(&listptr,
                 "use_filter_sort_reverse",
                 UI_ITEM_R_TOGGLE | UI_ITEM_R_ICON_ONLY,
                 "",
                 (ui_list->filter_sort_flag & UILST_FLT_SORT_REVERSE) ? ICON_SORT_DESC :
                                                                        ICON_SORT_ASC);
  }
}

uiListNameFilter::uiListNameFilter(uiList &list)
{
  const char *filter_raw = list.filter_byname;

  if (filter_raw[0]) {
    const size_t slen = strlen(filter_raw);

    /* Implicitly add heading/trailing wildcards if needed. */
    if (slen + 3 <= sizeof(storage_.filter_buff)) {
      filter_ = storage_.filter_buff;
    }
    else {
      filter_ = storage_.filter_dyn = MEM_malloc_arrayN<char>((slen + 3), "filter_dyn");
    }
    BLI_strncpy_ensure_pad(filter_, filter_raw, '*', slen + 3);
  }
}

uiListNameFilter::~uiListNameFilter()
{
  MEM_SAFE_FREE(storage_.filter_dyn);
}

eUIListFilterResult uiListNameFilter::operator()(const PointerRNA & /*itemptr*/,
                                                 StringRefNull name,
                                                 int /*index*/)
{
  if (!filter_) {
    return UI_LIST_ITEM_FILTER_MATCHES;
  }

  /* Use `fnmatch` for shell-style globing.
   * - Case-insensitive.
   * - Don't handle escape characters as "special" characters are not expected in names.
   *   Unlike shell input - `\` should be treated like any other character.
   */
  const int fn_flag = FNM_CASEFOLD | FNM_NOESCAPE;
  if (fnmatch(filter_, name.c_str(), fn_flag) == 0) {
    return UI_LIST_ITEM_FILTER_MATCHES;
  }
  return UI_LIST_ITEM_FILTER_MISMATCHES;
}

struct StringCmp {
  char name[MAX_IDPROP_NAME];
  int org_idx;
};

static int cmpstringp(const void *p1, const void *p2)
{
  /* Case-insensitive comparison. */
  return BLI_strcasecmp(static_cast<const StringCmp *>(p1)->name,
                        static_cast<const StringCmp *>(p2)->name);
}

void UI_list_filter_and_sort_items(uiList *ui_list,
                                   const bContext * /*C*/,
                                   uiListItemFilterFn item_filter_fn,
                                   PointerRNA *dataptr,
                                   const char *propname,
                                   uiListItemGetNameFn get_name_fn)
{
  uiListDyn *dyn_data = ui_list->dyn_data;
  PropertyRNA *prop = RNA_struct_find_property(dataptr, propname);

  const bool filter_exclude = (ui_list->filter_flag & UILST_FLT_EXCLUDE) != 0;
  const bool order_by_name = (ui_list->filter_sort_flag & UILST_FLT_SORT_ALPHA) &&
                             !(ui_list->filter_sort_flag & UILST_FLT_SORT_LOCK);
  const int len = RNA_property_collection_length(dataptr, prop);

  dyn_data->items_shown = dyn_data->items_len = len;

  if (len && (order_by_name || item_filter_fn)) {
    StringCmp *names = nullptr;
    int order_idx = 0, i = 0;

    if (order_by_name) {
      names = MEM_calloc_arrayN<StringCmp>(len, "StringCmp");
    }

    if (item_filter_fn) {
      dyn_data->items_filter_flags = MEM_calloc_arrayN<int>(len, "items_filter_flags");
      dyn_data->items_shown = 0;
    }

    RNA_PROP_BEGIN (dataptr, itemptr, prop) {
      bool do_order = false;

      char *namebuf;
      if (get_name_fn) {
        namebuf = BLI_strdup(get_name_fn(itemptr, i).c_str());
      }
      else {
        namebuf = RNA_struct_name_get_alloc(&itemptr, nullptr, 0, nullptr);
      }

      const char *name = namebuf ? namebuf : "";

      if (item_filter_fn) {
        const eUIListFilterResult filter_result = item_filter_fn(itemptr, name, i);

        if (filter_result == UI_LIST_ITEM_NEVER_SHOW) {
          dyn_data->items_filter_flags[i] = UILST_FLT_ITEM_NEVER_SHOW;
        }
        else if (filter_result == UI_LIST_ITEM_FILTER_MATCHES) {
          if (!filter_exclude) {
            dyn_data->items_filter_flags[i] = UILST_FLT_ITEM;
            dyn_data->items_shown++;
            do_order = order_by_name;
          }
          // printf("%s: '%s' matches '%s'\n", __func__, name, filter);
        }
        else if (filter_exclude) {
          dyn_data->items_filter_flags[i] = UILST_FLT_ITEM;
          dyn_data->items_shown++;
          do_order = order_by_name;
        }
      }
      else {
        do_order = order_by_name;
      }

      if (do_order) {
        names[order_idx].org_idx = order_idx;
        STRNCPY(names[order_idx++].name, name);
      }

      /* free name */
      if (namebuf) {
        MEM_freeN(namebuf);
      }
      i++;
    }
    RNA_PROP_END;

    if (order_by_name) {
      int new_idx;
      /* NOTE: order_idx equals either to ui_list->items_len if no filtering done,
       *       or to ui_list->items_shown if filter is enabled,
       *       or to (ui_list->items_len - ui_list->items_shown) if filtered items are excluded.
       *       This way, we only sort items we actually intend to draw!
       */
      qsort(names, order_idx, sizeof(StringCmp), cmpstringp);

      dyn_data->items_filter_neworder = MEM_malloc_arrayN<int>(order_idx, "items_filter_neworder");
      for (new_idx = 0; new_idx < order_idx; new_idx++) {
        dyn_data->items_filter_neworder[names[new_idx].org_idx] = new_idx;
      }
    }

    if (names) {
      MEM_freeN(names);
    }
  }
}

bool UI_list_item_index_is_filtered_visible(const uiList *ui_list, const int item_idx)
{
  const uiListDyn *dyn_data = ui_list->dyn_data;

  if (!dyn_data->items_filter_flags) {
    /* If there are no filter flags to check, always consider all items visible. */
    return true;
  }

  if (dyn_data->items_filter_flags[item_idx] & UILST_FLT_ITEM_NEVER_SHOW) {
    return false;
  }

  return (dyn_data->items_filter_flags[item_idx] & UILST_FLT_ITEM);
}

/**
 * Default UI List filtering: Filter by name.
 */
static void uilist_filter_items_default(uiList *ui_list,
                                        const bContext *C,
                                        PointerRNA *dataptr,
                                        const char *propname)
{
  if (ui_list->filter_byname[0]) {
    uiListNameFilter name_filter(*ui_list);
    UI_list_filter_and_sort_items(ui_list, C, name_filter, dataptr, propname);
  }
  /* Optimization: Skip filtering entirely when there is no filter string set. */
  else {
    UI_list_filter_and_sort_items(ui_list, C, nullptr, dataptr, propname);
  }
}

static void uilist_free_dyn_data(uiList *ui_list)
{
  uiListDyn *dyn_data = ui_list->dyn_data;
  if (!dyn_data) {
    return;
  }

  MEM_SAFE_FREE(dyn_data->items_filter_flags);
  MEM_SAFE_FREE(dyn_data->items_filter_neworder);
  MEM_SAFE_FREE(dyn_data->customdata);
}

/**
 * Validate input parameters and initialize \a r_data from that. Plus find the list-type and return
 * it in \a r_list_type.
 *
 * \return false if the input data isn't valid. Will also raise an RNA warning in that case.
 */
static bool ui_template_list_data_retrieve(const StringRef listtype_name,
                                           const char *list_id,
                                           PointerRNA *dataptr,
                                           const StringRefNull propname,
                                           PointerRNA *active_dataptr,
                                           const StringRefNull active_propname,
                                           const char *item_dyntip_propname,
                                           TemplateListInputData *r_input_data,
                                           uiListType **r_list_type)
{
  *r_input_data = {};

  /* Forbid default UI_UL_DEFAULT_CLASS_NAME list class without a custom list_id! */
  if ((UI_UL_DEFAULT_CLASS_NAME == listtype_name) && !(list_id && list_id[0])) {
    RNA_warning("template_list using default '%s' UIList class must provide a custom list_id",
                UI_UL_DEFAULT_CLASS_NAME);
    return false;
  }

  if (!active_dataptr->data) {
    RNA_warning("No active data");
    return false;
  }

  r_input_data->dataptr = *dataptr;
  if (dataptr->data) {
    r_input_data->prop = RNA_struct_find_property(dataptr, propname.c_str());
    if (!r_input_data->prop) {
      RNA_warning(
          "Property not found: %s.%s", RNA_struct_identifier(dataptr->type), propname.c_str());
      return false;
    }
  }

  r_input_data->active_dataptr = *active_dataptr;
  r_input_data->activeprop = RNA_struct_find_property(active_dataptr, active_propname.c_str());
  if (!r_input_data->activeprop) {
    RNA_warning("Property not found: %s.%s",
                RNA_struct_identifier(active_dataptr->type),
                active_propname.c_str());
    return false;
  }

  if (r_input_data->prop) {
    const PropertyType type = RNA_property_type(r_input_data->prop);
    if (type != PROP_COLLECTION) {
      RNA_warning("Expected a collection data property");
      return false;
    }
  }

  const PropertyType activetype = RNA_property_type(r_input_data->activeprop);
  if (activetype != PROP_INT) {
    RNA_warning("Expected an integer active data property");
    return false;
  }

  /* Find the uiList type. */
  if (!(*r_list_type = WM_uilisttype_find(listtype_name, false))) {
    RNA_warning("List type %s not found", std::string(listtype_name).c_str());
    return false;
  }

  r_input_data->active_item_idx = RNA_property_int_get(&r_input_data->active_dataptr,
                                                       r_input_data->activeprop);
  r_input_data->item_dyntip_propname = item_dyntip_propname;

  return true;
}

static void ui_template_list_collect_items(PointerRNA *list_ptr,
                                           PropertyRNA *list_prop,
                                           const uiList *ui_list,
                                           int activei,
                                           TemplateListItems *r_items)
{
  const uiListDyn *dyn_data = ui_list->dyn_data;
  const bool order_reverse = (ui_list->filter_sort_flag & UILST_FLT_SORT_REVERSE) != 0;
  int i = 0;
  int reorder_i = 0;
  bool activei_mapping_pending = true;

  RNA_PROP_BEGIN (list_ptr, itemptr, list_prop) {
    if (UI_list_item_index_is_filtered_visible(ui_list, i)) {
      int new_order_idx;
      if (dyn_data->items_filter_neworder) {
        new_order_idx = dyn_data->items_filter_neworder[reorder_i++];
        new_order_idx = order_reverse ? dyn_data->items_shown - new_order_idx - 1 : new_order_idx;
      }
      else {
        new_order_idx = order_reverse ? dyn_data->items_shown - ++reorder_i : reorder_i++;
      }
      // printf("%s: ii: %d\n", __func__, ii);
      r_items->item_vec[new_order_idx].item = itemptr;
      r_items->item_vec[new_order_idx].org_idx = i;
      r_items->item_vec[new_order_idx].flt_flag = dyn_data->items_filter_flags ?
                                                      dyn_data->items_filter_flags[i] :
                                                      0;

      if (activei_mapping_pending && activei == i) {
        activei = new_order_idx;
        /* So that we do not map again activei! */
        activei_mapping_pending = false;
      }
#if 0 /* For now, do not alter active element, even if it will be hidden... */
      else if (activei < i) {
        /* We do not want an active but invisible item!
         * Only exception is when all items are filtered out...
         */
        if (prev_order_idx >= 0) {
          activei = prev_order_idx;
          RNA_property_int_set(active_dataptr, activeprop, prev_i);
        }
        else {
          activei = new_order_idx;
          RNA_property_int_set(active_dataptr, activeprop, i);
        }
      }
      prev_i = i;
      prev_ii = new_order_idx;
#endif
    }
    i++;
  }
  RNA_PROP_END;

  /* If mapping is still pending, no active item was found. Mark as invalid (-1) */
  r_items->active_item_idx = activei_mapping_pending ? -1 : activei;
}

/**
 * Create the UI-list representation of the list items, sorted and filtered if needed.
 */
static void ui_template_list_collect_display_items(const bContext *C,
                                                   uiList *ui_list,
                                                   TemplateListInputData *input_data,
                                                   const uiListFilterItemsFunc filter_items_fn,
                                                   TemplateListItems *r_items)
{
  uiListDyn *dyn_data = ui_list->dyn_data;

  /* Filter list items! (not for compact layout, though) */
  if (input_data->dataptr.data && input_data->prop) {
    int items_shown;
#if 0
    int prev_ii = -1, prev_i;
#endif

    if (ui_list->layout_type == UILST_LAYOUT_COMPACT) {
      dyn_data->items_len = dyn_data->items_shown = RNA_property_collection_length(
          &input_data->dataptr, input_data->prop);
    }
    else {
      // printf("%s: filtering...\n", __func__);
      filter_items_fn(ui_list, C, &input_data->dataptr, RNA_property_identifier(input_data->prop));
      // printf("%s: filtering done.\n", __func__);
    }

    items_shown = dyn_data->items_shown;
    if (items_shown >= 0) {
      r_items->item_vec.resize(items_shown);
      // printf("%s: items shown: %d.\n", __func__, items_shown);

      ui_template_list_collect_items(
          &input_data->dataptr, input_data->prop, ui_list, input_data->active_item_idx, r_items);
    }
  }
}

static void uilist_prepare(uiList *ui_list,
                           const TemplateListItems *items,
                           const TemplateListLayoutDrawData *layout_data,
                           TemplateListVisualInfo *r_visual_info)
{
  uiListDyn *dyn_data = ui_list->dyn_data;
  const bool use_auto_size = (ui_list->list_grip <
                              (layout_data->rows - UI_LIST_AUTO_SIZE_THRESHOLD));

  int actual_rows = layout_data->rows;
  int actual_maxrows = layout_data->maxrows;
  int columns = layout_data->columns;

  /* default rows */
  if (actual_rows <= 0) {
    actual_rows = 5;
  }
  dyn_data->visual_height_min = actual_rows;
  if (actual_maxrows < actual_rows) {
    actual_maxrows = max_ii(actual_rows, 5);
  }
  if (columns <= 0) {
    columns = 9;
  }

  int activei_row;
  if (columns > 1) {
    dyn_data->height = int(ceil(double(items->item_vec.size()) / double(columns)));
    activei_row = int(floor(double(items->active_item_idx) / double(columns)));
  }
  else {
    dyn_data->height = items->item_vec.size();
    activei_row = items->active_item_idx;
  }

  dyn_data->columns = columns;

  if (!use_auto_size) {
    /* No auto-size, yet we clamp at min size! */
    actual_rows = max_ii(ui_list->list_grip, actual_rows);
  }
  else if ((actual_rows != actual_maxrows) && (dyn_data->height > actual_rows)) {
    /* Expand size if needed and possible. */
    actual_rows = min_ii(dyn_data->height, actual_maxrows);
  }

  /* If list length changes or list is tagged to check this,
   * and active is out of view, scroll to it. */
  if ((ui_list->list_last_len != items->item_vec.size()) ||
      (ui_list->flag & UILST_SCROLL_TO_ACTIVE_ITEM))
  {
    if (activei_row < ui_list->list_scroll) {
      ui_list->list_scroll = activei_row;
    }
    else if (activei_row >= ui_list->list_scroll + actual_rows) {
      ui_list->list_scroll = activei_row - actual_rows + 1;
    }
    ui_list->flag &= ~UILST_SCROLL_TO_ACTIVE_ITEM;
  }

  const int max_scroll = max_ii(0, dyn_data->height - actual_rows);
  CLAMP(ui_list->list_scroll, 0, max_scroll);
  ui_list->list_last_len = items->item_vec.size();
  dyn_data->visual_height = actual_rows;
  r_visual_info->visual_items = actual_rows * columns;
  r_visual_info->start_idx = ui_list->list_scroll * columns;
  r_visual_info->end_idx = min_ii(r_visual_info->start_idx + actual_rows * columns,
                                  items->item_vec.size());
}

static void uilist_resize_update(bContext *C, uiList *ui_list)
{
  uiListDyn *dyn_data = ui_list->dyn_data;

  /* This way we get diff in number of additional items to show (positive) or hide (negative). */
  const int diff = round_fl_to_int(float(dyn_data->resize - dyn_data->resize_prev) /
                                   float(UI_UNIT_Y));

  if (diff != 0) {
    ui_list->list_grip += diff;
    dyn_data->resize_prev += diff * UI_UNIT_Y;
    ui_list->flag |= UILST_SCROLL_TO_ACTIVE_ITEM;
  }

  /* In case uilist is in popup, we need special refreshing */
  ED_region_tag_refresh_ui(CTX_wm_region_popup(C));
}

static void *uilist_item_use_dynamic_tooltip(PointerRNA *itemptr, const char *propname)
{
  if (propname && propname[0] && itemptr && itemptr->data) {
    PropertyRNA *prop = RNA_struct_find_property(itemptr, propname);

    if (prop && (RNA_property_type(prop) == PROP_STRING)) {
      return RNA_property_string_get_alloc(itemptr, prop, nullptr, 0, nullptr);
    }
  }
  return nullptr;
}

static std::string uilist_item_tooltip_func(bContext * /*C*/, void *argN, const StringRef tip)
{
  char *dyn_tooltip = static_cast<char *>(argN);
  std::string tooltip_string = dyn_tooltip;
  if (!tip.is_empty()) {
    tooltip_string += '\n';
    tooltip_string += tip;
  }
  return tooltip_string;
}

/**
 * \note that \a layout_type may be null.
 */
static uiList *ui_list_ensure(const bContext *C,
                              uiListType *ui_list_type,
                              const char *list_id,
                              int layout_type,
                              bool sort_reverse,
                              bool sort_lock)
{
  /* Allows to work in popups. */
  ARegion *region = CTX_wm_region_popup(C);
  if (region == nullptr) {
    region = CTX_wm_region(C);
  }

  /* Find or add the uiList to the current Region. */

  char full_list_id[UI_MAX_NAME_STR];
  WM_uilisttype_to_full_list_id(ui_list_type, list_id, full_list_id);

  uiList *ui_list = static_cast<uiList *>(
      BLI_findstring(&region->ui_lists, full_list_id, offsetof(uiList, list_id)));

  if (!ui_list) {
    ui_list = MEM_callocN<uiList>("uiList");
    STRNCPY_UTF8(ui_list->list_id, full_list_id);
    BLI_addtail(&region->ui_lists, ui_list);
    ui_list->list_grip = -UI_LIST_AUTO_SIZE_THRESHOLD; /* Force auto size by default. */
    if (sort_reverse) {
      ui_list->filter_sort_flag |= UILST_FLT_SORT_REVERSE;
    }
    if (sort_lock) {
      ui_list->filter_sort_flag |= UILST_FLT_SORT_LOCK;
    }
  }

  if (!ui_list->dyn_data) {
    ui_list->dyn_data = MEM_callocN<uiListDyn>("uiList.dyn_data");
  }
  uiListDyn *dyn_data = ui_list->dyn_data;
  /* Note that this isn't a `uiListType` callback, it's stored in the runtime list data. Otherwise
   * the runtime data could leak when the type is unregistered (e.g. on "Reload Scripts"). */
  dyn_data->free_runtime_data_fn = uilist_free_dyn_data;

  /* Because we can't actually pass type across save&load... */
  ui_list->type = ui_list_type;
  ui_list->layout_type = layout_type;

  /* Reset filtering data. */
  MEM_SAFE_FREE(dyn_data->items_filter_flags);
  MEM_SAFE_FREE(dyn_data->items_filter_neworder);
  dyn_data->items_len = dyn_data->items_shown = -1;

  return ui_list;
}

static void ui_template_list_layout_draw(const bContext *C,
                                         uiList *ui_list,
                                         uiLayout *layout,
                                         TemplateListInputData *input_data,
                                         TemplateListItems *items,
                                         const TemplateListLayoutDrawData *layout_data,
                                         const enum uiTemplateListFlags flags)
{
  uiListDyn *dyn_data = ui_list->dyn_data;
  const char *active_propname = RNA_property_identifier(input_data->activeprop);

  uiLayout *glob = nullptr, *box, *row, *col, *sub, *overlap;
  char numstr[32];
  int rnaicon = ICON_NONE, icon = ICON_NONE;
  uiBut *but;

  uiBlock *block = layout->block();

  /* get icon */
  if (input_data->dataptr.data && input_data->prop) {
    StructRNA *ptype = RNA_property_pointer_type(&input_data->dataptr, input_data->prop);
    rnaicon = RNA_struct_ui_icon(ptype);
  }

  TemplateListVisualInfo visual_info;
  switch (ui_list->layout_type) {
    case UILST_LAYOUT_DEFAULT: {
      /* layout */
      box = &layout->list_box(ui_list, &input_data->active_dataptr, input_data->activeprop);
      glob = &box->column(true);
      row = &glob->row(false);
      col = &row->column(true);

      TemplateListLayoutDrawData adjusted_layout_data = *layout_data;
      adjusted_layout_data.columns = 1;
      /* init numbers */
      uilist_prepare(ui_list, items, &adjusted_layout_data, &visual_info);

      int i = 0;
      if (input_data->dataptr.data && input_data->prop) {

        const bool editable = (RNA_property_flag(input_data->prop) & PROP_EDITABLE);

        /* create list items */
        for (i = visual_info.start_idx; i < visual_info.end_idx; i++) {
          PointerRNA *itemptr = &items->item_vec[i].item;
          void *dyntip_data;
          const int org_i = items->item_vec[i].org_idx;
          const int flt_flag = items->item_vec[i].flt_flag;
          uiBlock *subblock = col->block();

          overlap = &col->overlap();

          UI_block_flag_enable(subblock, UI_BLOCK_LIST_ITEM);

          /* list item behind label & other buttons */
          overlap->row(false);

          but = uiDefButR_prop(subblock,
                               ButType::ListRow,
                               0,
                               "",
                               0,
                               0,
                               UI_UNIT_X * 10,
                               UI_UNIT_Y,
                               &input_data->active_dataptr,
                               input_data->activeprop,
                               0,
                               0,
                               org_i,
                               editable ? TIP_("Select List Item "
                                               "(Double click to rename)") :
                                          TIP_("Select List Item"));

          if ((dyntip_data = uilist_item_use_dynamic_tooltip(itemptr,
                                                             input_data->item_dyntip_propname)))
          {
            UI_but_func_tooltip_set(but, uilist_item_tooltip_func, dyntip_data, MEM_freeN);
          }

          uiLayout *item_row = &overlap->row(true);

          uiLayoutListItemAddPadding(item_row);

          sub = &item_row->row(false);
          icon = UI_icon_from_rnaptr(C, itemptr, rnaicon, false);
          if (icon == ICON_DOT) {
            icon = ICON_NONE;
          }
          layout_data->draw_item(ui_list,
                                 C,
                                 sub,
                                 &input_data->dataptr,
                                 itemptr,
                                 icon,
                                 &input_data->active_dataptr,
                                 active_propname,
                                 org_i,
                                 flt_flag);

          /* Items should be able to set context pointers for the layout. But the list-row button
           * swallows events, so it needs the context storage too for handlers to see it. */
          but->context = sub->context_store();

          /* If we are "drawing" active item, set all labels as active. */
          if (i == items->active_item_idx) {
            ui_layout_list_set_labels_active(sub);
          }

          uiLayoutListItemAddPadding(item_row);
          UI_block_flag_disable(subblock, UI_BLOCK_LIST_ITEM);
        }
      }

      /* add dummy buttons to fill space */
      for (; i < visual_info.start_idx + visual_info.visual_items; i++) {
        col->label("", ICON_NONE);
      }

      /* Add scroll-bar. */
      if (items->item_vec.size() > visual_info.visual_items) {
        row->column(false);
        but = uiDefButI(block,
                        ButType::Scroll,
                        0,
                        "",
                        0,
                        0,
                        V2D_SCROLL_WIDTH,
                        UI_UNIT_Y * dyn_data->visual_height,
                        &ui_list->list_scroll,
                        0,
                        dyn_data->height - dyn_data->visual_height,
                        "");
        uiButScrollBar *but_scroll = reinterpret_cast<uiButScrollBar *>(but);
        but_scroll->visual_height = dyn_data->visual_height;
      }
      break;
    }
    case UILST_LAYOUT_COMPACT:
      row = &layout->row(true);

      if ((input_data->dataptr.data && input_data->prop) && (dyn_data->items_shown > 0) &&
          (items->active_item_idx >= 0) && (items->active_item_idx < dyn_data->items_shown))
      {
        PointerRNA *itemptr = &items->item_vec[items->active_item_idx].item;
        const int org_i = items->item_vec[items->active_item_idx].org_idx;

        icon = UI_icon_from_rnaptr(C, itemptr, rnaicon, false);
        if (icon == ICON_DOT) {
          icon = ICON_NONE;
        }
        layout_data->draw_item(ui_list,
                               C,
                               row,
                               &input_data->dataptr,
                               itemptr,
                               icon,
                               &input_data->active_dataptr,
                               active_propname,
                               org_i,
                               0);
      }
      /* if list is empty, add in dummy button */
      else {
        row->label("", ICON_NONE);
      }

      /* next/prev button */
      SNPRINTF_UTF8(numstr, "%d :", dyn_data->items_shown);
      but = uiDefIconTextButR_prop(block,
                                   ButType::Num,
                                   0,
                                   ICON_NONE,
                                   numstr,
                                   0,
                                   0,
                                   UI_UNIT_X * 5,
                                   UI_UNIT_Y,
                                   &input_data->active_dataptr,
                                   input_data->activeprop,
                                   0,
                                   0,
                                   0,
                                   "");
      if (dyn_data->items_shown == 0) {
        UI_but_flag_enable(but, UI_BUT_DISABLED);
      }
      break;
    case UILST_LAYOUT_BIG_PREVIEW_GRID:
      box = &layout->list_box(ui_list, &input_data->active_dataptr, input_data->activeprop);
      /* For grip button. */
      glob = &box->column(true);
      /* For scroll-bar. */
      row = &glob->row(false);

      const bool show_names = (flags & UI_TEMPLATE_LIST_NO_NAMES) == 0;

      const int size_x = UI_preview_tile_size_x();
      const int size_y = show_names ? UI_preview_tile_size_y() : UI_preview_tile_size_y_no_label();

      const int cols_per_row = std::max(int((box->width() - V2D_SCROLL_WIDTH) / size_x), 1);
      uiLayout *grid = &row->grid_flow(true, cols_per_row, true, true, true);

      TemplateListLayoutDrawData adjusted_layout_data = *layout_data;
      adjusted_layout_data.columns = cols_per_row;
      uilist_prepare(ui_list, items, &adjusted_layout_data, &visual_info);

      if (input_data->dataptr.data && input_data->prop) {
        /* create list items */
        for (int i = visual_info.start_idx; i < visual_info.end_idx; i++) {
          PointerRNA *itemptr = &items->item_vec[i].item;
          const int org_i = items->item_vec[i].org_idx;
          const int flt_flag = items->item_vec[i].flt_flag;

          overlap = &grid->overlap();
          col = &overlap->column(false);

          uiBlock *subblock = col->block();
          UI_block_flag_enable(subblock, UI_BLOCK_LIST_ITEM);

          but = uiDefButR_prop(subblock,
                               ButType::ListRow,
                               0,
                               "",
                               0,
                               0,
                               size_x,
                               size_y,
                               &input_data->active_dataptr,
                               input_data->activeprop,
                               0,
                               0,
                               org_i,
                               std::nullopt);
          UI_but_drawflag_enable(but, UI_BUT_NO_TOOLTIP);

          col = &overlap->column(false);

          icon = UI_icon_from_rnaptr(C, itemptr, rnaicon, false);
          layout_data->draw_item(ui_list,
                                 C,
                                 col,
                                 &input_data->dataptr,
                                 itemptr,
                                 icon,
                                 &input_data->active_dataptr,
                                 active_propname,
                                 org_i,
                                 flt_flag);

          /* Items should be able to set context pointers for the layout. But the list-row button
           * swallows events, so it needs the context storage too for handlers to see it. */
          but->context = col->context_store();

          /* If we are "drawing" active item, set all labels as active. */
          if (i == items->active_item_idx) {
            ui_layout_list_set_labels_active(col);
          }

          UI_block_flag_disable(subblock, UI_BLOCK_LIST_ITEM);
        }
      }

      if (items->item_vec.size() > visual_info.visual_items) {
        /* col = */ row->column(false);
        but = uiDefButI(block,
                        ButType::Scroll,
                        0,
                        "",
                        0,
                        0,
                        V2D_SCROLL_WIDTH,
                        size_y * dyn_data->visual_height,
                        &ui_list->list_scroll,
                        0,
                        dyn_data->height - dyn_data->visual_height,
                        "");
        uiButScrollBar *but_scroll = reinterpret_cast<uiButScrollBar *>(but);
        but_scroll->visual_height = dyn_data->visual_height;
      }
      break;
  }

  const bool add_filters_but = (flags & UI_TEMPLATE_LIST_NO_FILTER_OPTIONS) == 0;
  if (glob && add_filters_but) {
    const bool add_grip_but = (flags & UI_TEMPLATE_LIST_NO_GRIP) == 0;

    /* About #ButType::Grip drag-resize:
     * We can't directly use results from a grip button, since we have a
     * rather complex behavior here (sizing by discrete steps and, overall, auto-size feature).
     * Since we *never* know whether we are grip-resizing or not
     * (because there is no callback for when a button enters/leaves its "edit mode"),
     * we use the fact that grip-controlled value (dyn_data->resize) is completely handled
     * by the grip during the grab resize, so settings its value here has no effect at all.
     *
     * It is only meaningful when we are not resizing,
     * in which case this gives us the correct "init drag" value.
     * Note we cannot affect `dyn_data->resize_prev here`,
     * since this value is not controlled by the grip!
     */
    dyn_data->resize = dyn_data->resize_prev +
                       (dyn_data->visual_height - ui_list->list_grip) * UI_UNIT_Y;

    row = &glob->row(true);
    uiBlock *subblock = row->block();
    UI_block_emboss_set(subblock, blender::ui::EmbossType::None);

    if (ui_list->filter_flag & UILST_FLT_SHOW) {
      but = uiDefIconButBitI(subblock,
                             ButType::Toggle,
                             UILST_FLT_SHOW,
                             0,
                             ICON_DISCLOSURE_TRI_DOWN,
                             0,
                             0,
                             UI_UNIT_X,
                             UI_UNIT_Y * 0.5f,
                             &(ui_list->filter_flag),
                             0,
                             0,
                             TIP_("Hide filtering options"));
      UI_but_flag_disable(but, UI_BUT_UNDO); /* skip undo on screen buttons */

      if (add_grip_but) {
        but = uiDefIconButI(subblock,
                            ButType::Grip,
                            0,
                            ICON_GRIP,
                            0,
                            0,
                            UI_UNIT_X * 10.0f,
                            UI_UNIT_Y * 0.5f,
                            &dyn_data->resize,
                            0.0,
                            0.0,
                            "");
        UI_but_func_set(but, [ui_list](bContext &C) { uilist_resize_update(&C, ui_list); });
      }

      UI_block_emboss_set(subblock, blender::ui::EmbossType::Emboss);

      col = &glob->column(false);
      subblock = col->block();
      uiDefBut(subblock,
               ButType::Sepr,
               0,
               "",
               0,
               0,
               UI_UNIT_X,
               UI_UNIT_Y * 0.05f,
               nullptr,
               0.0,
               0.0,
               "");

      layout_data->draw_filter(ui_list, C, col);
    }
    else {
      but = uiDefIconButBitI(subblock,
                             ButType::Toggle,
                             UILST_FLT_SHOW,
                             0,
                             ICON_DISCLOSURE_TRI_RIGHT,
                             0,
                             0,
                             UI_UNIT_X,
                             UI_UNIT_Y * 0.5f,
                             &(ui_list->filter_flag),
                             0,
                             0,
                             TIP_("Show filtering options"));
      UI_but_flag_disable(but, UI_BUT_UNDO); /* skip undo on screen buttons */

      if (add_grip_but) {
        but = uiDefIconButI(subblock,
                            ButType::Grip,
                            0,
                            ICON_GRIP,
                            0,
                            0,
                            UI_UNIT_X * 10.0f,
                            UI_UNIT_Y * 0.5f,
                            &dyn_data->resize,
                            0.0,
                            0.0,
                            "");
        UI_but_func_set(but, [ui_list](bContext &C) { uilist_resize_update(&C, ui_list); });
      }

      UI_block_emboss_set(subblock, blender::ui::EmbossType::Emboss);
    }
  }
}

uiList *uiTemplateList_ex(uiLayout *layout,
                          const bContext *C,
                          const char *listtype_name,
                          const char *list_id,
                          PointerRNA *dataptr,
                          const StringRefNull propname,
                          PointerRNA *active_dataptr,
                          const StringRefNull active_propname,
                          const char *item_dyntip_propname,
                          int rows,
                          int maxrows,
                          int layout_type,
                          int columns,
                          enum uiTemplateListFlags flags,
                          void *customdata)
{
  TemplateListInputData input_data = {};
  uiListType *ui_list_type;
  if (!ui_template_list_data_retrieve(listtype_name,
                                      list_id,
                                      dataptr,
                                      propname,
                                      active_dataptr,
                                      active_propname,
                                      item_dyntip_propname,
                                      &input_data,
                                      &ui_list_type))
  {
    return nullptr;
  }

  uiListDrawItemFunc draw_item = ui_list_type->draw_item ? ui_list_type->draw_item :
                                                           uilist_draw_item_default;
  uiListDrawFilterFunc draw_filter = ui_list_type->draw_filter ? ui_list_type->draw_filter :
                                                                 uilist_draw_filter_default;
  uiListFilterItemsFunc filter_items = ui_list_type->filter_items ? ui_list_type->filter_items :
                                                                    uilist_filter_items_default;

  uiList *ui_list = ui_list_ensure(C,
                                   ui_list_type,
                                   list_id,
                                   layout_type,
                                   flags & UI_TEMPLATE_LIST_SORT_REVERSE,
                                   flags & UI_TEMPLATE_LIST_SORT_LOCK);
  uiListDyn *dyn_data = ui_list->dyn_data;

  MEM_SAFE_FREE(dyn_data->customdata);
  dyn_data->customdata = customdata;

  /* When active item changed since last draw, scroll to it. */
  if (input_data.active_item_idx != ui_list->list_last_activei) {
    ui_list->flag |= UILST_SCROLL_TO_ACTIVE_ITEM;
    ui_list->list_last_activei = input_data.active_item_idx;
  }

  TemplateListItems items;
  ui_template_list_collect_display_items(C, ui_list, &input_data, filter_items, &items);

  TemplateListLayoutDrawData layout_data;
  layout_data.draw_item = draw_item;
  layout_data.draw_filter = draw_filter;
  layout_data.rows = rows;
  layout_data.maxrows = maxrows;
  layout_data.columns = columns;

  ui_template_list_layout_draw(C, ui_list, layout, &input_data, &items, &layout_data, flags);

  return ui_list;
}

void uiTemplateList(uiLayout *layout,
                    const bContext *C,
                    const char *listtype_name,
                    const char *list_id,
                    PointerRNA *dataptr,
                    blender::StringRefNull propname,
                    PointerRNA *active_dataptr,
                    const char *active_propname,
                    const char *item_dyntip_propname,
                    int rows,
                    int maxrows,
                    int layout_type,
                    int columns,
                    enum uiTemplateListFlags flags)
{
  uiTemplateList_ex(layout,
                    C,
                    listtype_name,
                    list_id,
                    dataptr,
                    propname,
                    active_dataptr,
                    active_propname,
                    item_dyntip_propname,
                    rows,
                    maxrows,
                    layout_type,
                    columns,
                    flags,
                    nullptr);
}

/* -------------------------------------------------------------------- */

/** \name List-types Registration
 * \{ */

void ED_uilisttypes_ui()
{
  WM_uilisttype_add(UI_UL_cache_file_layers());
}

/** \} */
