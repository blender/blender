/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fmt/format.h>

#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "ED_screen.hh"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_lib_id.hh"
#include "BKE_screen.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "ANIM_action.hh"

#include "UI_interface_icons.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"
#include "UI_string_search.hh"
#include "UI_view2d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "interface_intern.hh"

using blender::StringRef;
using blender::StringRefNull;

/*************************** RNA Utilities ******************************/

uiBut *uiDefAutoButR(uiBlock *block,
                     PointerRNA *ptr,
                     PropertyRNA *prop,
                     int index,
                     const std::optional<StringRef> name,
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

      if (icon && name && name->is_empty()) {
        but = uiDefIconButR_prop(block,
                                 ButType::IconToggle,
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
                                 std::nullopt);
      }
      else if (icon) {
        but = uiDefIconTextButR_prop(block,
                                     ButType::IconToggle,
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
                                     std::nullopt);
      }
      else {
        but = uiDefButR_prop(block,
                             ButType::Checkbox,
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
                             std::nullopt);
      }
      break;
    }
    case PROP_INT:
    case PROP_FLOAT: {
      if (RNA_property_array_check(prop) && index == -1) {
        if (ELEM(RNA_property_subtype(prop), PROP_COLOR, PROP_COLOR_GAMMA)) {
          but = uiDefButR_prop(block,
                               ButType::Color,
                               0,
                               name,
                               x,
                               y,
                               width,
                               height,
                               ptr,
                               prop,
                               -1,
                               0,
                               0,
                               std::nullopt);
        }
        else {
          return nullptr;
        }
      }
      else if (RNA_property_subtype(prop) == PROP_PERCENTAGE ||
               RNA_property_subtype(prop) == PROP_FACTOR)
      {
        but = uiDefButR_prop(block,
                             ButType::NumSlider,
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
                             std::nullopt);
      }
      else {
        but = uiDefButR_prop(block,
                             ButType::Num,
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
                             std::nullopt);
      }

      if (RNA_property_flag(prop) & PROP_TEXTEDIT_UPDATE) {
        UI_but_flag_enable(but, UI_BUT_TEXTEDIT_UPDATE);
      }
      break;
    }
    case PROP_ENUM:
      if (icon && name && name->is_empty()) {
        but = uiDefIconButR_prop(block,
                                 ButType::Menu,
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
                                 std::nullopt);
      }
      else if (icon) {
        but = uiDefIconTextButR_prop(block,
                                     ButType::Menu,
                                     0,
                                     icon,
                                     std::nullopt,
                                     x,
                                     y,
                                     width,
                                     height,
                                     ptr,
                                     prop,
                                     index,
                                     0,
                                     0,
                                     std::nullopt);
      }
      else {
        but = uiDefButR_prop(block,
                             ButType::Menu,
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
                             std::nullopt);
      }
      break;
    case PROP_STRING:
      if (icon && name && name->is_empty()) {
        but = uiDefIconButR_prop(block,
                                 ButType::Text,
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
                                 std::nullopt);
      }
      else if (icon) {
        but = uiDefIconTextButR_prop(block,
                                     ButType::Text,
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
                                     std::nullopt);
      }
      else {
        but = uiDefButR_prop(block,
                             ButType::Text,
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
                             std::nullopt);
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
                                   ButType::SearchMenu,
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
                                   std::nullopt);
      ui_but_add_search(but, ptr, prop, nullptr, nullptr, nullptr, false);
      break;
    }
    case PROP_COLLECTION: {
      char text[256];
      SNPRINTF_UTF8(text, IFACE_("%d items"), RNA_property_collection_length(ptr, prop));
      but = uiDefBut(
          block, ButType::Label, 0, text, x, y, width, height, nullptr, 0, 0, std::nullopt);
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
  std::optional<StringRefNull> name;

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
          col = &layout->column(true);

          if (!is_boolean) {
            col->label(*name, ICON_NONE);
          }
        }
        else {
          BLI_assert(label_align == UI_BUT_LABEL_ALIGN_SPLIT_COLUMN);
          col = &layout->column(true);
          /* Let uiLayout::prop() create the split layout. */
          col->use_property_split_set(true);
        }

        break;
      }
      case UI_BUT_LABEL_ALIGN_NONE:
      default:
        col = layout;
        name = std::nullopt; /* no smart label alignment, show default name with button */
        break;
    }

    /* Only buttons that can be edited as text. */
    const bool use_activate_init = ((prop == prop_activate_init) &&
                                    ELEM(type, PROP_STRING, PROP_INT, PROP_FLOAT));

    if (use_activate_init) {
      col->activate_init_set(true);
    }

    col->prop(ptr, prop, -1, 0, compact ? UI_ITEM_R_COMPACT : UI_ITEM_NONE, name, ICON_NONE);
    return_info &= ~UI_PROP_BUTS_NONE_ADDED;

    if (use_activate_init) {
      col->activate_init_set(false);
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
  void *data;
  std::string name;
  int index;
  int iconid;
  int name_prefix_offset;
  bool is_id;
  bool has_sep_char;
};

static bool add_collection_search_item(CollItemSearch &cis,
                                       const bool requires_exact_data_name,
                                       const bool has_id_icon,
                                       uiSearchItems *items)
{

  /* If no item has its own icon to display, libraries can use the library icons rather than the
   * name prefix for showing the library status. */
  int name_prefix_offset = cis.name_prefix_offset;
  if (!has_id_icon && cis.is_id && !requires_exact_data_name) {
    cis.iconid = UI_icon_from_library(static_cast<const ID *>(cis.data));
    char name_buf[UI_MAX_DRAW_STR];
    BKE_id_full_name_ui_prefix_get(
        name_buf, static_cast<const ID *>(cis.data), false, UI_SEP_CHAR, &name_prefix_offset);
    cis.name = name_buf;
  }

  return UI_search_item_add(items,
                            cis.name,
                            cis.data,
                            cis.iconid,
                            cis.has_sep_char ? int(UI_BUT_HAS_SEP_CHAR) : 0,
                            name_prefix_offset);
}

void ui_rna_collection_search_update_fn(
    const bContext *C, void *arg, const char *str, uiSearchItems *items, const bool is_first)
{
  using namespace blender;
  uiRNACollectionSearch *data = static_cast<uiRNACollectionSearch *>(arg);
  const int flag = RNA_property_flag(data->target_prop);
  const bool is_ptr_target = (RNA_property_type(data->target_prop) == PROP_POINTER);
  /* For non-pointer properties, UI code acts entirely based on the item's name. So the name has to
   * match the RNA name exactly. So only for pointer properties, the name can be modified to add
   * further UI hints. */
  const bool requires_exact_data_name = !is_ptr_target;
  const bool skip_filter = is_first;
  char name_buf[UI_MAX_DRAW_STR];
  bool has_id_icon = false;

  /* The string search API requires pointer stability. */
  Vector<std::unique_ptr<CollItemSearch>> items_list;

  if (data->search_prop != nullptr) {
    /* build a temporary list of relevant items first */
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

      char *name;
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
      else if (data->item_search_prop) {
        name = RNA_property_string_get_alloc(
            &itemptr, data->item_search_prop, name_buf, sizeof(name_buf), nullptr);
      }
      else if (itemptr.type == &RNA_ActionSlot) {
        /* FIXME: This special case is fairly annoying.
         *
         * `item_search_prop` now allows to specify another string property than the default RNA
         * struct name one as source, but icons are still an issue. RNA access API for icons likely
         * needs some love, to allow callbacks, data-based icons retrieval, in addition to the
         * purely static options currently available (see #RNA_struct_ui_icon and
         * #RNA_property_ui_icon).
         */
        PropertyRNA *prop = RNA_struct_find_property(&itemptr, "name_display");
        name = RNA_property_string_get_alloc(&itemptr, prop, name_buf, sizeof(name_buf), nullptr);
        /* Also show an icon for the data-block type that each slot is intended for. */
        animrig::Slot &slot = reinterpret_cast<ActionSlot *>(itemptr.data)->wrap();
        iconid = UI_icon_from_idcode(slot.idtype);
        /* So indentation is kept when no icon is present. */
        if (iconid == ICON_NONE) {
          iconid = ICON_BLANK1;
        }
      }
      else {
        name = RNA_struct_name_get_alloc(&itemptr, name_buf, sizeof(name_buf), nullptr);
      }

      if (name) {
        auto cis = std::make_unique<CollItemSearch>();
        cis->data = itemptr.data;
        cis->name = name;
        cis->index = items_list.size();
        cis->iconid = iconid;
        cis->is_id = is_id;
        cis->name_prefix_offset = name_prefix_offset;
        cis->has_sep_char = has_sep_char;
        items_list.append(std::move(cis));
        if (name != name_buf) {
          MEM_freeN(name);
        }
      }
    }
    RNA_PROP_END;
  }
  else {
    BLI_assert(RNA_property_type(data->target_prop) == PROP_STRING);
    const eStringPropertySearchFlag search_flag = RNA_property_string_search_flag(
        data->target_prop);
    BLI_assert(search_flag & PROP_STRING_SEARCH_SUPPORTED);

    const bool show_extra_info = (G.debug_value == 102);

    RNA_property_string_search(C,
                               &data->target_ptr,
                               data->target_prop,
                               str,
                               [&](StringPropertySearchVisitParams visit_params) {
                                 auto cis = std::make_unique<CollItemSearch>();

                                 cis->data = nullptr;
                                 if (visit_params.info && show_extra_info) {
                                   cis->name = fmt::format("{}" UI_SEP_CHAR_S "{}",
                                                           visit_params.text,
                                                           *visit_params.info);
                                 }
                                 else {
                                   cis->name = std::move(visit_params.text);
                                 }

                                 cis->index = items_list.size();
                                 cis->iconid = visit_params.icon_id.value_or(ICON_NONE);
                                 cis->is_id = false;
                                 cis->name_prefix_offset = 0;
                                 cis->has_sep_char = visit_params.info.has_value();
                                 items_list.append(std::move(cis));
                               });

    if (search_flag & PROP_STRING_SEARCH_SORT) {
      std::sort(
          items_list.begin(),
          items_list.end(),
          [](const std::unique_ptr<CollItemSearch> &a, const std::unique_ptr<CollItemSearch> &b) {
            return BLI_strcasecmp_natural(a->name.c_str(), b->name.c_str()) < 0;
          });
      for (const int i : items_list.index_range()) {
        items_list[i]->index = i;
      }
    }
  }

  if (skip_filter) {
    for (std::unique_ptr<CollItemSearch> &cis : items_list) {
      if (!add_collection_search_item(*cis, requires_exact_data_name, has_id_icon, items)) {
        break;
      }
    }
  }
  else {
    ui::string_search::StringSearch<CollItemSearch> search;
    for (std::unique_ptr<CollItemSearch> &cis : items_list) {
      search.add(cis->name, cis.get());
    }

    const Vector<CollItemSearch *> filtered_items = search.query(str);
    for (CollItemSearch *cis : filtered_items) {
      if (!add_collection_search_item(*cis, requires_exact_data_name, has_id_icon, items)) {
        break;
      }
    }
  }
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
    return TH_ERROR;
  }
  if (type & RPT_WARNING_ALL) {
    return TH_WARNING;
  }
  if (type & RPT_INFO_ALL) {
    return TH_INFO;
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
  return TH_WARNING;
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

      prec = std::max(test_prec, prec);
    }
  }

  CLAMP(prec, 0, UI_PRECISION_FLOAT_MAX);

  return prec;
}

std::optional<std::string> UI_but_online_manual_id(const uiBut *but)
{
  if (but->rnapoin.data && but->rnaprop) {
    return fmt::format(
        "{}.{}", RNA_struct_identifier(but->rnapoin.type), RNA_property_identifier(but->rnaprop));
  }
  if (but->optype) {
    char idname[OP_MAX_TYPENAME];
    const size_t idname_len = WM_operator_py_idname(idname, but->optype->idname);
    return std::string(idname, idname_len);
  }

  return std::nullopt;
}

std::optional<std::string> UI_but_online_manual_id_from_active(const bContext *C)
{
  if (uiBut *but = UI_context_active_but_get(C)) {
    return UI_but_online_manual_id(but);
  }
  return std::nullopt;
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
  uiButStore *bs_handle = MEM_callocN<uiButStore>(__func__);

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

bool UI_butstore_is_valid(uiButStore *bs_handle)
{
  return (bs_handle->block != nullptr);
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
  uiButStoreElem *bs_elem = MEM_callocN<uiButStoreElem>(__func__);
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
