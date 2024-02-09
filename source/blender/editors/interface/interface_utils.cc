/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "ED_screen.hh"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_lib_id.hh"
#include "BKE_report.h"
#include "BKE_screen.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_resources.hh"
#include "UI_string_search.hh"
#include "UI_view2d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "interface_intern.hh"

/*************************** RNA Utilities ******************************/

uiBut *uiDefAutoButR(uiBlock *block,
                     PointerRNA *ptr,
                     PropertyRNA *prop,
                     int index,
                     const char *name,
                     int icon,
                     int x,
                     int y,
                     int width,
                     int height)
{
  uiBut *but = nullptr;

  switch (RNA_property_type(prop)) {
    case PROP_BOOLEAN: {
      if (RNA_property_array_check(prop) && index == -1) {
        return nullptr;
      }

      if (icon && name && name[0] == '\0') {
        but = uiDefIconButR_prop(block,
                                 UI_BTYPE_ICON_TOGGLE,
                                 0,
                                 icon,
                                 x,
                                 y,
                                 width,
                                 height,
                                 ptr,
                                 prop,
                                 index,
                                 0,
                                 0,
                                 nullptr);
      }
      else if (icon) {
        but = uiDefIconTextButR_prop(block,
                                     UI_BTYPE_ICON_TOGGLE,
                                     0,
                                     icon,
                                     name,
                                     x,
                                     y,
                                     width,
                                     height,
                                     ptr,
                                     prop,
                                     index,
                                     0,
                                     0,
                                     nullptr);
      }
      else {
        but = uiDefButR_prop(block,
                             UI_BTYPE_CHECKBOX,
                             0,
                             name,
                             x,
                             y,
                             width,
                             height,
                             ptr,
                             prop,
                             index,
                             0,
                             0,
                             nullptr);
      }
      break;
    }
    case PROP_INT:
    case PROP_FLOAT: {
      if (RNA_property_array_check(prop) && index == -1) {
        if (ELEM(RNA_property_subtype(prop), PROP_COLOR, PROP_COLOR_GAMMA)) {
          but = uiDefButR_prop(
              block, UI_BTYPE_COLOR, 0, name, x, y, width, height, ptr, prop, -1, 0, 0, nullptr);
        }
        else {
          return nullptr;
        }
      }
      else if (RNA_property_subtype(prop) == PROP_PERCENTAGE ||
               RNA_property_subtype(prop) == PROP_FACTOR)
      {
        but = uiDefButR_prop(block,
                             UI_BTYPE_NUM_SLIDER,
                             0,
                             name,
                             x,
                             y,
                             width,
                             height,
                             ptr,
                             prop,
                             index,
                             0,
                             0,
                             nullptr);
      }
      else {
        but = uiDefButR_prop(
            block, UI_BTYPE_NUM, 0, name, x, y, width, height, ptr, prop, index, 0, 0, nullptr);
      }

      if (RNA_property_flag(prop) & PROP_TEXTEDIT_UPDATE) {
        UI_but_flag_enable(but, UI_BUT_TEXTEDIT_UPDATE);
      }
      break;
    }
    case PROP_ENUM:
      if (icon && name && name[0] == '\0') {
        but = uiDefIconButR_prop(
            block, UI_BTYPE_MENU, 0, icon, x, y, width, height, ptr, prop, index, 0, 0, nullptr);
      }
      else if (icon) {
        but = uiDefIconTextButR_prop(block,
                                     UI_BTYPE_MENU,
                                     0,
                                     icon,
                                     nullptr,
                                     x,
                                     y,
                                     width,
                                     height,
                                     ptr,
                                     prop,
                                     index,
                                     0,
                                     0,
                                     nullptr);
      }
      else {
        but = uiDefButR_prop(
            block, UI_BTYPE_MENU, 0, name, x, y, width, height, ptr, prop, index, 0, 0, nullptr);
      }
      break;
    case PROP_STRING:
      if (icon && name && name[0] == '\0') {
        but = uiDefIconButR_prop(
            block, UI_BTYPE_TEXT, 0, icon, x, y, width, height, ptr, prop, index, 0, 0, nullptr);
      }
      else if (icon) {
        but = uiDefIconTextButR_prop(block,
                                     UI_BTYPE_TEXT,
                                     0,
                                     icon,
                                     name,
                                     x,
                                     y,
                                     width,
                                     height,
                                     ptr,
                                     prop,
                                     index,
                                     0,
                                     0,
                                     nullptr);
      }
      else {
        but = uiDefButR_prop(
            block, UI_BTYPE_TEXT, 0, name, x, y, width, height, ptr, prop, index, 0, 0, nullptr);
      }

      if (RNA_property_flag(prop) & PROP_TEXTEDIT_UPDATE) {
        /* TEXTEDIT_UPDATE is usually used for search buttons. For these we also want
         * the 'x' icon to clear search string, so setting VALUE_CLEAR flag, too. */
        UI_but_flag_enable(but, UI_BUT_TEXTEDIT_UPDATE | UI_BUT_VALUE_CLEAR);
      }
      break;
    case PROP_POINTER: {
      if (icon == 0) {
        const PointerRNA pptr = RNA_property_pointer_get(ptr, prop);
        icon = RNA_struct_ui_icon(pptr.type ? pptr.type : RNA_property_pointer_type(ptr, prop));
      }
      if (icon == ICON_DOT) {
        icon = 0;
      }

      but = uiDefIconTextButR_prop(block,
                                   UI_BTYPE_SEARCH_MENU,
                                   0,
                                   icon,
                                   name,
                                   x,
                                   y,
                                   width,
                                   height,
                                   ptr,
                                   prop,
                                   index,
                                   0,
                                   0,
                                   nullptr);
      ui_but_add_search(but, ptr, prop, nullptr, nullptr, false);
      break;
    }
    case PROP_COLLECTION: {
      char text[256];
      SNPRINTF(text, IFACE_("%d items"), RNA_property_collection_length(ptr, prop));
      but = uiDefBut(
          block, UI_BTYPE_LABEL, 0, text, x, y, width, height, nullptr, 0, 0, 0, 0, nullptr);
      UI_but_flag_enable(but, UI_BUT_DISABLED);
      break;
    }
    default:
      but = nullptr;
      break;
  }

  return but;
}

void uiDefAutoButsArrayR(uiBlock *block,
                         PointerRNA *ptr,
                         PropertyRNA *prop,
                         const int icon,
                         const int x,
                         const int y,
                         const int tot_width,
                         const int height)
{
  const int len = RNA_property_array_length(ptr, prop);
  if (len == 0) {
    return;
  }

  const int item_width = tot_width / len;

  UI_block_align_begin(block);
  for (int i = 0; i < len; i++) {
    uiDefAutoButR(block, ptr, prop, i, "", icon, x + i * item_width, y, item_width, height);
  }
  UI_block_align_end(block);
}

eAutoPropButsReturn uiDefAutoButsRNA(uiLayout *layout,
                                     PointerRNA *ptr,
                                     bool (*check_prop)(PointerRNA *ptr,
                                                        PropertyRNA *prop,
                                                        void *user_data),
                                     void *user_data,
                                     PropertyRNA *prop_activate_init,
                                     const eButLabelAlign label_align,
                                     const bool compact)
{
  eAutoPropButsReturn return_info = UI_PROP_BUTS_NONE_ADDED;
  uiLayout *col;
  const char *name;

  RNA_STRUCT_BEGIN (ptr, prop) {
    const int flag = RNA_property_flag(prop);

    if (flag & PROP_HIDDEN) {
      continue;
    }
    if (check_prop && check_prop(ptr, prop, user_data) == 0) {
      return_info |= UI_PROP_BUTS_ANY_FAILED_CHECK;
      continue;
    }

    const PropertyType type = RNA_property_type(prop);
    switch (label_align) {
      case UI_BUT_LABEL_ALIGN_COLUMN:
      case UI_BUT_LABEL_ALIGN_SPLIT_COLUMN: {
        const bool is_boolean = (type == PROP_BOOLEAN && !RNA_property_array_check(prop));

        name = RNA_property_ui_name(prop);

        if (label_align == UI_BUT_LABEL_ALIGN_COLUMN) {
          col = uiLayoutColumn(layout, true);

          if (!is_boolean) {
            uiItemL(col, name, ICON_NONE);
          }
        }
        else {
          BLI_assert(label_align == UI_BUT_LABEL_ALIGN_SPLIT_COLUMN);
          col = uiLayoutColumn(layout, true);
          /* Let uiItemFullR() create the split layout. */
          uiLayoutSetPropSep(col, true);
        }

        break;
      }
      case UI_BUT_LABEL_ALIGN_NONE:
      default:
        col = layout;
        name = nullptr; /* no smart label alignment, show default name with button */
        break;
    }

    /* Only buttons that can be edited as text. */
    const bool use_activate_init = ((prop == prop_activate_init) &&
                                    ELEM(type, PROP_STRING, PROP_INT, PROP_FLOAT));

    if (use_activate_init) {
      uiLayoutSetActivateInit(col, true);
    }

    uiItemFullR(
        col, ptr, prop, -1, 0, compact ? UI_ITEM_R_COMPACT : UI_ITEM_NONE, name, ICON_NONE);
    return_info &= ~UI_PROP_BUTS_NONE_ADDED;

    if (use_activate_init) {
      uiLayoutSetActivateInit(col, false);
    }
  }
  RNA_STRUCT_END;

  return return_info;
}

void UI_but_func_identity_compare_set(uiBut *but, uiButIdentityCompareFunc cmp_fn)
{
  but->identity_cmp_func = cmp_fn;
}

/* *** RNA collection search menu *** */

struct CollItemSearch {
  CollItemSearch *next, *prev;
  void *data;
  char *name;
  int index;
  int iconid;
  bool is_id;
  int name_prefix_offset;
  uint has_sep_char : 1;
};

static bool add_collection_search_item(CollItemSearch *cis,
                                       const bool requires_exact_data_name,
                                       const bool has_id_icon,
                                       uiSearchItems *items)
{
  char name_buf[UI_MAX_DRAW_STR];

  /* If no item has an own icon to display, libraries can use the library icons rather than the
   * name prefix for showing the library status. */
  int name_prefix_offset = cis->name_prefix_offset;
  if (!has_id_icon && cis->is_id && !requires_exact_data_name) {
    cis->iconid = UI_icon_from_library(static_cast<const ID *>(cis->data));
    /* No need to re-allocate, string should be shorter than before (lib status prefix is
     * removed). */
    BKE_id_full_name_ui_prefix_get(
        name_buf, static_cast<const ID *>(cis->data), false, UI_SEP_CHAR, &name_prefix_offset);
    const int name_buf_len = strlen(name_buf);
    BLI_assert(name_buf_len <= strlen(cis->name));
    memcpy(cis->name, name_buf, name_buf_len + 1);
  }

  return UI_search_item_add(items,
                            cis->name,
                            cis->data,
                            cis->iconid,
                            cis->has_sep_char ? int(UI_BUT_HAS_SEP_CHAR) : 0,
                            name_prefix_offset);
}

void ui_rna_collection_search_update_fn(
    const bContext *C, void *arg, const char *str, uiSearchItems *items, const bool is_first)
{
  uiRNACollectionSearch *data = static_cast<uiRNACollectionSearch *>(arg);
  const int flag = RNA_property_flag(data->target_prop);
  ListBase *items_list = MEM_cnew<ListBase>("items_list");
  const bool is_ptr_target = (RNA_property_type(data->target_prop) == PROP_POINTER);
  /* For non-pointer properties, UI code acts entirely based on the item's name. So the name has to
   * match the RNA name exactly. So only for pointer properties, the name can be modified to add
   * further UI hints. */
  const bool requires_exact_data_name = !is_ptr_target;
  const bool skip_filter = is_first;
  char name_buf[UI_MAX_DRAW_STR];
  char *name;
  bool has_id_icon = false;

  blender::ui::string_search::StringSearch<CollItemSearch> search;

  if (data->search_prop != nullptr) {
    /* build a temporary list of relevant items first */
    int item_index = 0;
    RNA_PROP_BEGIN (&data->search_ptr, itemptr, data->search_prop) {

      if (flag & PROP_ID_SELF_CHECK) {
        if (itemptr.data == data->target_ptr.owner_id) {
          continue;
        }
      }

      /* use filter */
      if (is_ptr_target) {
        if (RNA_property_pointer_poll(&data->target_ptr, data->target_prop, &itemptr) == 0) {
          continue;
        }
      }

      int name_prefix_offset = 0;
      int iconid = ICON_NONE;
      bool has_sep_char = false;
      const bool is_id = itemptr.type && RNA_struct_is_ID(itemptr.type);

      if (is_id) {
        iconid = ui_id_icon_get(C, static_cast<ID *>(itemptr.data), false);
        if (!ELEM(iconid, 0, ICON_BLANK1)) {
          has_id_icon = true;
        }

        if (requires_exact_data_name) {
          name = RNA_struct_name_get_alloc(&itemptr, name_buf, sizeof(name_buf), nullptr);
        }
        else {
          const ID *id = static_cast<ID *>(itemptr.data);
          BKE_id_full_name_ui_prefix_get(name_buf, id, true, UI_SEP_CHAR, &name_prefix_offset);
          BLI_STATIC_ASSERT(sizeof(name_buf) >= MAX_ID_FULL_NAME_UI,
                            "Name string buffer should be big enough to hold full UI ID name");
          name = name_buf;
          has_sep_char = ID_IS_LINKED(id);
        }
      }
      else {
        name = RNA_struct_name_get_alloc(&itemptr, name_buf, sizeof(name_buf), nullptr);
      }

      if (name) {
        CollItemSearch *cis = MEM_cnew<CollItemSearch>(__func__);
        cis->data = itemptr.data;
        cis->name = BLI_strdup(name);
        cis->index = item_index;
        cis->iconid = iconid;
        cis->is_id = is_id;
        cis->name_prefix_offset = name_prefix_offset;
        cis->has_sep_char = has_sep_char;
        if (!skip_filter) {
          search.add(name, cis);
        }
        BLI_addtail(items_list, cis);
        if (name != name_buf) {
          MEM_freeN(name);
        }
      }

      item_index++;
    }
    RNA_PROP_END;
  }
  else {
    BLI_assert(RNA_property_type(data->target_prop) == PROP_STRING);
    const eStringPropertySearchFlag search_flag = RNA_property_string_search_flag(
        data->target_prop);
    BLI_assert(search_flag & PROP_STRING_SEARCH_SUPPORTED);

    struct SearchVisitUserData {
      blender::string_search::StringSearch<CollItemSearch> *search;
      bool skip_filter;
      int item_index;
      ListBase *items_list;
      const char *func_id;
    } user_data = {nullptr};

    user_data.search = &search;
    user_data.skip_filter = skip_filter;
    user_data.items_list = items_list;
    user_data.func_id = __func__;

    RNA_property_string_search(
        C,
        &data->target_ptr,
        data->target_prop,
        str,
        [](void *user_data, const StringPropertySearchVisitParams *visit_params) {
          const bool show_extra_info = (G.debug_value == 102);

          SearchVisitUserData *search_data = (SearchVisitUserData *)user_data;
          CollItemSearch *cis = MEM_cnew<CollItemSearch>(search_data->func_id);
          cis->data = nullptr;
          if (visit_params->info && show_extra_info) {
            cis->name = BLI_sprintfN(
                "%s" UI_SEP_CHAR_S "%s", visit_params->text, visit_params->info);
          }
          else {
            cis->name = BLI_strdup(visit_params->text);
          }
          cis->index = search_data->item_index;
          cis->iconid = ICON_NONE;
          cis->is_id = false;
          cis->name_prefix_offset = 0;
          cis->has_sep_char = visit_params->info != nullptr;
          if (!search_data->skip_filter) {
            search_data->search->add(visit_params->text, cis);
          }
          BLI_addtail(search_data->items_list, cis);
          search_data->item_index++;
        },
        (void *)&user_data);

    if (search_flag & PROP_STRING_SEARCH_SORT) {
      BLI_listbase_sort(items_list, [](const void *a_, const void *b_) -> int {
        const CollItemSearch *cis_a = (const CollItemSearch *)a_;
        const CollItemSearch *cis_b = (const CollItemSearch *)b_;
        return BLI_strcasecmp_natural(cis_a->name, cis_b->name);
      });
      int i = 0;
      LISTBASE_FOREACH (CollItemSearch *, cis, items_list) {
        cis->index = i;
        i++;
      }
    }
  }

  if (skip_filter) {
    LISTBASE_FOREACH (CollItemSearch *, cis, items_list) {
      if (!add_collection_search_item(cis, requires_exact_data_name, has_id_icon, items)) {
        break;
      }
    }
  }
  else {
    const blender::Vector<CollItemSearch *> filtered_items = search.query(str);
    for (CollItemSearch *cis : filtered_items) {
      if (!add_collection_search_item(cis, requires_exact_data_name, has_id_icon, items)) {
        break;
      }
    }
  }

  LISTBASE_FOREACH (CollItemSearch *, cis, items_list) {
    MEM_freeN(cis->name);
  }
  BLI_freelistN(items_list);
  MEM_freeN(items_list);
}

int UI_icon_from_id(const ID *id)
{
  if (id == nullptr) {
    return ICON_NONE;
  }

  /* exception for objects */
  if (GS(id->name) == ID_OB) {
    Object *ob = (Object *)id;

    if (ob->type == OB_EMPTY) {
      return ICON_EMPTY_DATA;
    }
    return UI_icon_from_id(static_cast<const ID *>(ob->data));
  }

  /* otherwise get it through RNA, creating the pointer
   * will set the right type, also with subclassing */
  PointerRNA ptr = RNA_id_pointer_create((ID *)id);

  return (ptr.type) ? RNA_struct_ui_icon(ptr.type) : ICON_NONE;
}

int UI_icon_from_report_type(int type)
{
  if (type & RPT_ERROR_ALL) {
    return ICON_CANCEL;
  }
  if (type & RPT_WARNING_ALL) {
    return ICON_ERROR;
  }
  if (type & RPT_INFO_ALL) {
    return ICON_INFO;
  }
  if (type & RPT_DEBUG_ALL) {
    return ICON_SYSTEM;
  }
  if (type & RPT_PROPERTY) {
    return ICON_OPTIONS;
  }
  if (type & RPT_OPERATOR) {
    return ICON_CHECKMARK;
  }
  return ICON_INFO;
}

int UI_icon_colorid_from_report_type(int type)
{
  if (type & RPT_ERROR_ALL) {
    return TH_INFO_ERROR;
  }
  if (type & RPT_WARNING_ALL) {
    return TH_INFO_WARNING;
  }
  if (type & RPT_INFO_ALL) {
    return TH_INFO_INFO;
  }
  if (type & RPT_DEBUG_ALL) {
    return TH_INFO_DEBUG;
  }
  if (type & RPT_PROPERTY) {
    return TH_INFO_PROPERTY;
  }
  if (type & RPT_OPERATOR) {
    return TH_INFO_OPERATOR;
  }
  return TH_INFO_WARNING;
}

int UI_text_colorid_from_report_type(int type)
{
  if (type & RPT_ERROR_ALL) {
    return TH_INFO_ERROR_TEXT;
  }
  if (type & RPT_WARNING_ALL) {
    return TH_INFO_WARNING_TEXT;
  }
  if (type & RPT_INFO_ALL) {
    return TH_INFO_INFO_TEXT;
  }
  if (type & RPT_DEBUG_ALL) {
    return TH_INFO_DEBUG_TEXT;
  }
  if (type & RPT_PROPERTY) {
    return TH_INFO_PROPERTY_TEXT;
  }
  if (type & RPT_OPERATOR) {
    return TH_INFO_OPERATOR_TEXT;
  }
  return TH_INFO_WARNING_TEXT;
}

/********************************** Misc **************************************/

int UI_calc_float_precision(int prec, double value)
{
  static const double pow10_neg[UI_PRECISION_FLOAT_MAX + 1] = {
      1e0, 1e-1, 1e-2, 1e-3, 1e-4, 1e-5, 1e-6};
  static const double max_pow = 10000000.0; /* pow(10, UI_PRECISION_FLOAT_MAX) */

  BLI_assert(prec <= UI_PRECISION_FLOAT_MAX);
  BLI_assert(fabs(pow10_neg[prec] - pow(10, -prec)) < 1e-16);

  /* Check on the number of decimal places need to display the number,
   * this is so 0.00001 is not displayed as 0.00,
   * _but_, this is only for small values as 10.0001 will not get the same treatment.
   */
  value = fabs(value);
  if ((value < pow10_neg[prec]) && (value > (1.0 / max_pow))) {
    int value_i = int(lround(value * max_pow));
    if (value_i != 0) {
      const int prec_span = 3; /* show: 0.01001, 5 would allow 0.0100001 for eg. */
      int test_prec;
      int prec_min = -1;
      int dec_flag = 0;
      int i = UI_PRECISION_FLOAT_MAX;
      while (i && value_i) {
        if (value_i % 10) {
          dec_flag |= 1 << i;
          prec_min = i;
        }
        value_i /= 10;
        i--;
      }

      /* even though its a small value, if the second last digit is not 0, use it */
      test_prec = prec_min;

      dec_flag = (dec_flag >> (prec_min + 1)) & ((1 << prec_span) - 1);

      while (dec_flag) {
        test_prec++;
        dec_flag = dec_flag >> 1;
      }

      if (test_prec > prec) {
        prec = test_prec;
      }
    }
  }

  CLAMP(prec, 0, UI_PRECISION_FLOAT_MAX);

  return prec;
}

bool UI_but_online_manual_id(const uiBut *but, char *r_str, size_t str_maxncpy)
{
  if (but->rnapoin.owner_id && but->rnapoin.data && but->rnaprop) {
    BLI_snprintf(r_str,
                 str_maxncpy,
                 "%s.%s",
                 RNA_struct_identifier(but->rnapoin.type),
                 RNA_property_identifier(but->rnaprop));
    return true;
  }
  if (but->optype) {
    WM_operator_py_idname(r_str, but->optype->idname);
    return true;
  }

  *r_str = '\0';
  return false;
}

bool UI_but_online_manual_id_from_active(const bContext *C, char *r_str, size_t str_maxncpy)
{
  uiBut *but = UI_context_active_but_get(C);

  if (but) {
    return UI_but_online_manual_id(but, r_str, str_maxncpy);
  }

  *r_str = '\0';
  return false;
}

/* -------------------------------------------------------------------- */

static rctf ui_but_rect_to_view(const uiBut *but, const ARegion *region, const View2D *v2d)
{
  rctf region_rect;
  ui_block_to_region_rctf(region, but->block, &region_rect, &but->rect);

  rctf view_rect;
  UI_view2d_region_to_view_rctf(v2d, &region_rect, &view_rect);

  return view_rect;
}

/**
 * To get a margin (typically wanted), add the margin to \a rect directly.
 *
 * Based on #file_ensure_inside_viewbounds(), could probably share code.
 *
 * \return true if anything changed.
 */
static bool ui_view2d_cur_ensure_rect_in_view(View2D *v2d, const rctf *rect)
{
  const float rect_width = BLI_rctf_size_x(rect);
  const float rect_height = BLI_rctf_size_y(rect);

  rctf *cur = &v2d->cur;
  const float cur_width = BLI_rctf_size_x(cur);
  const float cur_height = BLI_rctf_size_y(cur);

  bool changed = false;

  /* Snap to bottom edge. Also use if rect is higher than view bounds (could be a parameter). */
  if ((cur->ymin > rect->ymin) || (rect_height > cur_height)) {
    cur->ymin = rect->ymin;
    cur->ymax = cur->ymin + cur_height;
    changed = true;
  }
  /* Snap to upper edge. */
  else if (cur->ymax < rect->ymax) {
    cur->ymax = rect->ymax;
    cur->ymin = cur->ymax - cur_height;
    changed = true;
  }
  /* Snap to left edge. Also use if rect is wider than view bounds. */
  else if ((cur->xmin > rect->xmin) || (rect_width > cur_width)) {
    cur->xmin = rect->xmin;
    cur->xmax = cur->xmin + cur_width;
    changed = true;
  }
  /* Snap to right edge. */
  else if (cur->xmax < rect->xmax) {
    cur->xmax = rect->xmax;
    cur->xmin = cur->xmax - cur_width;
    changed = true;
  }
  else {
    BLI_assert(BLI_rctf_inside_rctf(cur, rect));
  }

  return changed;
}

void UI_but_ensure_in_view(const bContext *C, ARegion *region, const uiBut *but)
{
  View2D *v2d = &region->v2d;
  /* Uninitialized view or region that doesn't use View2D. */
  if ((v2d->flag & V2D_IS_INIT) == 0) {
    return;
  }

  rctf rect = ui_but_rect_to_view(but, region, v2d);

  const int margin = UI_UNIT_X * 0.5f;
  BLI_rctf_pad(&rect, margin, margin);

  const bool changed = ui_view2d_cur_ensure_rect_in_view(v2d, &rect);
  if (changed) {
    UI_view2d_curRect_changed(C, v2d);
    ED_region_tag_redraw_no_rebuild(region);
  }
}

/* -------------------------------------------------------------------- */
/** \name Button Store
 *
 * Modal Button Store API.
 *
 * Store for modal operators & handlers to register button pointers
 * which are maintained while drawing or nullptr when removed.
 *
 * This is needed since button pointers are continuously freed and re-allocated.
 *
 * \{ */

struct uiButStore {
  uiButStore *next, *prev;
  uiBlock *block;
  ListBase items;
};

struct uiButStoreElem {
  uiButStoreElem *next, *prev;
  uiBut **but_p;
};

uiButStore *UI_butstore_create(uiBlock *block)
{
  uiButStore *bs_handle = MEM_cnew<uiButStore>(__func__);

  bs_handle->block = block;
  BLI_addtail(&block->butstore, bs_handle);

  return bs_handle;
}

void UI_butstore_free(uiBlock *block, uiButStore *bs_handle)
{
  /* NOTE(@ideasman42): Workaround for button store being moved into new block,
   * which then can't use the previous buttons state
   * (#ui_but_update_from_old_block fails to find a match),
   * keeping the active button in the old block holding a reference
   * to the button-state in the new block: see #49034.
   *
   * Ideally we would manage moving the 'uiButStore', keeping a correct state.
   * All things considered this is the most straightforward fix. */
  if (block != bs_handle->block && bs_handle->block != nullptr) {
    block = bs_handle->block;
  }

  BLI_freelistN(&bs_handle->items);
  BLI_assert(BLI_findindex(&block->butstore, bs_handle) != -1);
  BLI_remlink(&block->butstore, bs_handle);

  MEM_freeN(bs_handle);
}

bool UI_butstore_is_valid(uiButStore *bs)
{
  return (bs->block != nullptr);
}

bool UI_butstore_is_registered(uiBlock *block, uiBut *but)
{
  LISTBASE_FOREACH (uiButStore *, bs_handle, &block->butstore) {
    LISTBASE_FOREACH (uiButStoreElem *, bs_elem, &bs_handle->items) {
      if (*bs_elem->but_p == but) {
        return true;
      }
    }
  }

  return false;
}

void UI_butstore_register(uiButStore *bs_handle, uiBut **but_p)
{
  uiButStoreElem *bs_elem = MEM_cnew<uiButStoreElem>(__func__);
  BLI_assert(*but_p);
  bs_elem->but_p = but_p;

  BLI_addtail(&bs_handle->items, bs_elem);
}

void UI_butstore_unregister(uiButStore *bs_handle, uiBut **but_p)
{
  LISTBASE_FOREACH_MUTABLE (uiButStoreElem *, bs_elem, &bs_handle->items) {
    if (bs_elem->but_p == but_p) {
      BLI_remlink(&bs_handle->items, bs_elem);
      MEM_freeN(bs_elem);
    }
  }

  BLI_assert(0);
}

bool UI_butstore_register_update(uiBlock *block, uiBut *but_dst, const uiBut *but_src)
{
  bool found = false;

  LISTBASE_FOREACH (uiButStore *, bs_handle, &block->butstore) {
    LISTBASE_FOREACH (uiButStoreElem *, bs_elem, &bs_handle->items) {
      if (*bs_elem->but_p == but_src) {
        *bs_elem->but_p = but_dst;
        found = true;
      }
    }
  }

  return found;
}

void UI_butstore_clear(uiBlock *block)
{
  LISTBASE_FOREACH (uiButStore *, bs_handle, &block->butstore) {
    bs_handle->block = nullptr;
    LISTBASE_FOREACH (uiButStoreElem *, bs_elem, &bs_handle->items) {
      *bs_elem->but_p = nullptr;
    }
  }
}

void UI_butstore_update(uiBlock *block)
{
  /* move this list to the new block */
  if (block->oldblock) {
    if (block->oldblock->butstore.first) {
      BLI_movelisttolist(&block->butstore, &block->oldblock->butstore);
    }
  }

  if (LIKELY(block->butstore.first == nullptr)) {
    return;
  }

  /* warning, loop-in-loop, in practice we only store <10 buttons at a time,
   * so this isn't going to be a problem, if that changes old-new mapping can be cached first */
  LISTBASE_FOREACH (uiButStore *, bs_handle, &block->butstore) {
    BLI_assert(ELEM(bs_handle->block, nullptr, block) ||
               (block->oldblock && block->oldblock == bs_handle->block));

    if (bs_handle->block == block->oldblock) {
      bs_handle->block = block;

      LISTBASE_FOREACH (uiButStoreElem *, bs_elem, &bs_handle->items) {
        if (*bs_elem->but_p) {
          uiBut *but_new = ui_but_find_new(block, *bs_elem->but_p);

          /* can be nullptr if the buttons removed,
           * NOTE: we could allow passing in a callback when buttons are removed
           * so the caller can cleanup */
          *bs_elem->but_p = but_new;
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Key Event from UI
 * \{ */

/**
 * Follow the logic from #wm_keymap_item_find_in_keymap.
 */
static bool ui_key_event_property_match(const char *opname,
                                        IDProperty *properties,
                                        const bool is_strict,
                                        wmOperatorType *ui_optype,
                                        PointerRNA *ui_opptr)
{
  if (!STREQ(ui_optype->idname, opname)) {
    return false;
  }

  bool match = false;
  if (properties) {
    if (ui_opptr &&
        IDP_EqualsProperties_ex(properties, static_cast<IDProperty *>(ui_opptr->data), is_strict))
    {
      match = true;
    }
  }
  else {
    match = true;
  }
  return match;
}

std::optional<std::string> UI_key_event_operator_string(const bContext *C,
                                                        const char *opname,
                                                        IDProperty *properties,
                                                        const bool is_strict)
{
  /* NOTE: currently only actions on UI Lists are supported (for the asset manager).
   * Other kinds of events can be supported as needed. */

  ARegion *region = CTX_wm_region(C);
  if (region == nullptr) {
    return std::nullopt;
  }

  /* Early exit regions which don't have UI-Lists. */
  if ((region->type->keymapflag & ED_KEYMAP_UI) == 0) {
    return std::nullopt;
  }

  uiBut *but = UI_region_active_but_get(region);
  if (but == nullptr) {
    return std::nullopt;
  }

  if (but->type != UI_BTYPE_PREVIEW_TILE) {
    return std::nullopt;
  }

  short event_val = KM_NOTHING;
  short event_type = KM_NOTHING;

  uiBut *listbox = nullptr;
  LISTBASE_FOREACH_BACKWARD (uiBut *, but_iter, &but->block->buttons) {
    if ((but_iter->type == UI_BTYPE_LISTBOX) && ui_but_contains_rect(but_iter, &but->rect)) {
      listbox = but_iter;
      break;
    }
  }

  if (listbox && listbox->custom_data) {
    uiList *list = static_cast<uiList *>(listbox->custom_data);
    uiListDyn *dyn_data = list->dyn_data;
    if ((dyn_data->custom_activate_optype != nullptr) &&
        ui_key_event_property_match(opname,
                                    properties,
                                    is_strict,
                                    dyn_data->custom_activate_optype,
                                    dyn_data->custom_activate_opptr))
    {
      event_val = KM_CLICK;
      event_type = LEFTMOUSE;
    }
    else if ((dyn_data->custom_activate_optype != nullptr) &&
             ui_key_event_property_match(opname,
                                         properties,
                                         is_strict,
                                         dyn_data->custom_drag_optype,
                                         dyn_data->custom_drag_opptr))
    {
      event_val = KM_CLICK_DRAG;
      event_type = LEFTMOUSE;
    }
  }

  if ((event_val != KM_NOTHING) && (event_type != KM_NOTHING)) {
    return WM_keymap_item_raw_to_string(
        false, false, false, false, 0, event_val, event_type, false);
  }

  return std::nullopt;
}

/** \} */
