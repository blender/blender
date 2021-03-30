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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edinterface
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BLI_alloca.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_search.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_lib_id.h"
#include "BKE_report.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "interface_intern.h"

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
  uiBut *but = NULL;

  switch (RNA_property_type(prop)) {
    case PROP_BOOLEAN: {
      if (RNA_property_array_check(prop) && index == -1) {
        return NULL;
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
                                 -1,
                                 -1,
                                 NULL);
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
                                     -1,
                                     -1,
                                     NULL);
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
                             -1,
                             -1,
                             NULL);
      }
      break;
    }
    case PROP_INT:
    case PROP_FLOAT: {
      if (RNA_property_array_check(prop) && index == -1) {
        if (ELEM(RNA_property_subtype(prop), PROP_COLOR, PROP_COLOR_GAMMA)) {
          but = uiDefButR_prop(block,
                               UI_BTYPE_COLOR,
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
                               0,
                               0,
                               NULL);
        }
        else {
          return NULL;
        }
      }
      else if (RNA_property_subtype(prop) == PROP_PERCENTAGE ||
               RNA_property_subtype(prop) == PROP_FACTOR) {
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
                             -1,
                             -1,
                             NULL);
      }
      else {
        but = uiDefButR_prop(
            block, UI_BTYPE_NUM, 0, name, x, y, width, height, ptr, prop, index, 0, 0, 0, 0, NULL);
      }

      if (RNA_property_flag(prop) & PROP_TEXTEDIT_UPDATE) {
        UI_but_flag_enable(but, UI_BUT_TEXTEDIT_UPDATE);
      }
      break;
    }
    case PROP_ENUM:
      if (icon && name && name[0] == '\0') {
        but = uiDefIconButR_prop(block,
                                 UI_BTYPE_MENU,
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
                                 -1,
                                 -1,
                                 NULL);
      }
      else if (icon) {
        but = uiDefIconTextButR_prop(block,
                                     UI_BTYPE_MENU,
                                     0,
                                     icon,
                                     NULL,
                                     x,
                                     y,
                                     width,
                                     height,
                                     ptr,
                                     prop,
                                     index,
                                     0,
                                     0,
                                     -1,
                                     -1,
                                     NULL);
      }
      else {
        but = uiDefButR_prop(block,
                             UI_BTYPE_MENU,
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
                             -1,
                             -1,
                             NULL);
      }
      break;
    case PROP_STRING:
      if (icon && name && name[0] == '\0') {
        but = uiDefIconButR_prop(block,
                                 UI_BTYPE_TEXT,
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
                                 -1,
                                 -1,
                                 NULL);
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
                                     -1,
                                     -1,
                                     NULL);
      }
      else {
        but = uiDefButR_prop(block,
                             UI_BTYPE_TEXT,
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
                             -1,
                             -1,
                             NULL);
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
                                   -1,
                                   -1,
                                   NULL);
      break;
    }
    case PROP_COLLECTION: {
      char text[256];
      BLI_snprintf(
          text, sizeof(text), IFACE_("%d items"), RNA_property_collection_length(ptr, prop));
      but = uiDefBut(block, UI_BTYPE_LABEL, 0, text, x, y, width, height, NULL, 0, 0, 0, 0, NULL);
      UI_but_flag_enable(but, UI_BUT_DISABLED);
      break;
    }
    default:
      but = NULL;
      break;
  }

  return but;
}

/**
 * \a check_prop callback filters functions to avoid drawing certain properties,
 * in cases where PROP_HIDDEN flag can't be used for a property.
 *
 * \param prop_activate_init: Property to activate on initial popup (#UI_BUT_ACTIVATE_ON_INIT).
 */
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
        name = NULL; /* no smart label alignment, show default name with button */
        break;
    }

    /* Only buttons that can be edited as text. */
    const bool use_activate_init = ((prop == prop_activate_init) &&
                                    (ELEM(type, PROP_STRING, PROP_INT, PROP_FLOAT)));

    if (use_activate_init) {
      uiLayoutSetActivateInit(col, true);
    }

    uiItemFullR(col, ptr, prop, -1, 0, compact ? UI_ITEM_R_COMPACT : 0, name, ICON_NONE);
    return_info &= ~UI_PROP_BUTS_NONE_ADDED;

    if (use_activate_init) {
      uiLayoutSetActivateInit(col, false);
    }
  }
  RNA_STRUCT_END;

  return return_info;
}

/* *** RNA collection search menu *** */

typedef struct CollItemSearch {
  struct CollItemSearch *next, *prev;
  void *data;
  char *name;
  int index;
  int iconid;
  bool is_id;
  int name_prefix_offset;
  uint has_sep_char : 1;
} CollItemSearch;

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
    cis->iconid = UI_icon_from_library(cis->data);
    /* No need to re-allocate, string should be shorter than before (lib status prefix is
     * removed). */
    BKE_id_full_name_ui_prefix_get(name_buf, cis->data, false, UI_SEP_CHAR, &name_prefix_offset);
    BLI_assert(strlen(name_buf) <= MEM_allocN_len(cis->name));
    strcpy(cis->name, name_buf);
  }

  return UI_search_item_add(items,
                            cis->name,
                            cis->data,
                            cis->iconid,
                            cis->has_sep_char ? UI_BUT_HAS_SEP_CHAR : 0,
                            name_prefix_offset);
}

void ui_rna_collection_search_update_fn(const struct bContext *C,
                                        void *arg,
                                        const char *str,
                                        uiSearchItems *items,
                                        const bool is_first)
{
  uiRNACollectionSearch *data = arg;
  const int flag = RNA_property_flag(data->target_prop);
  ListBase *items_list = MEM_callocN(sizeof(ListBase), "items_list");
  const bool is_ptr_target = (RNA_property_type(data->target_prop) == PROP_POINTER);
  /* For non-pointer properties, UI code acts entirely based on the item's name. So the name has to
   * match the RNA name exactly. So only for pointer properties, the name can be modified to add
   * further UI hints. */
  const bool requires_exact_data_name = !is_ptr_target;
  const bool skip_filter = is_first;
  char name_buf[UI_MAX_DRAW_STR];
  char *name;
  bool has_id_icon = false;

  StringSearch *search = skip_filter ? NULL : BLI_string_search_new();

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
      iconid = ui_id_icon_get(C, itemptr.data, false);
      if (!ELEM(iconid, 0, ICON_BLANK1)) {
        has_id_icon = true;
      }

      if (requires_exact_data_name) {
        name = RNA_struct_name_get_alloc(&itemptr, name_buf, sizeof(name_buf), NULL);
      }
      else {
        const ID *id = itemptr.data;
        BKE_id_full_name_ui_prefix_get(
            name_buf, itemptr.data, true, UI_SEP_CHAR, &name_prefix_offset);
        BLI_STATIC_ASSERT(sizeof(name_buf) >= MAX_ID_FULL_NAME_UI,
                          "Name string buffer should be big enough to hold full UI ID name");
        name = name_buf;
        has_sep_char = (id->lib != NULL);
      }
    }
    else {
      name = RNA_struct_name_get_alloc(&itemptr, name_buf, sizeof(name_buf), NULL);
    }

    if (name) {
      CollItemSearch *cis = MEM_callocN(sizeof(CollItemSearch), "CollectionItemSearch");
      cis->data = itemptr.data;
      cis->name = BLI_strdup(name);
      cis->index = item_index;
      cis->iconid = iconid;
      cis->is_id = is_id;
      cis->name_prefix_offset = name_prefix_offset;
      cis->has_sep_char = has_sep_char;
      if (!skip_filter) {
        BLI_string_search_add(search, name, cis);
      }
      BLI_addtail(items_list, cis);
      if (name != name_buf) {
        MEM_freeN(name);
      }
    }

    item_index++;
  }
  RNA_PROP_END;

  if (skip_filter) {
    LISTBASE_FOREACH (CollItemSearch *, cis, items_list) {
      if (!add_collection_search_item(cis, requires_exact_data_name, has_id_icon, items)) {
        break;
      }
    }
  }
  else {
    CollItemSearch **filtered_items;
    int filtered_amount = BLI_string_search_query(search, str, (void ***)&filtered_items);

    for (int i = 0; i < filtered_amount; i++) {
      CollItemSearch *cis = filtered_items[i];
      if (!add_collection_search_item(cis, requires_exact_data_name, has_id_icon, items)) {
        break;
      }
    }

    MEM_freeN(filtered_items);
    BLI_string_search_free(search);
  }

  LISTBASE_FOREACH (CollItemSearch *, cis, items_list) {
    MEM_freeN(cis->name);
  }
  BLI_freelistN(items_list);
  MEM_freeN(items_list);
}

/***************************** ID Utilities *******************************/
int UI_icon_from_id(const ID *id)
{
  if (id == NULL) {
    return ICON_NONE;
  }

  /* exception for objects */
  if (GS(id->name) == ID_OB) {
    Object *ob = (Object *)id;

    if (ob->type == OB_EMPTY) {
      return ICON_EMPTY_DATA;
    }
    return UI_icon_from_id(ob->data);
  }

  /* otherwise get it through RNA, creating the pointer
   * will set the right type, also with subclassing */
  PointerRNA ptr;
  RNA_id_pointer_create((ID *)id, &ptr);

  return (ptr.type) ? RNA_struct_ui_icon(ptr.type) : ICON_NONE;
}

/* see: report_type_str */
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

/**
 * Returns the best "UI" precision for given floating value,
 * so that e.g. 10.000001 rather gets drawn as '10'...
 */
int UI_calc_float_precision(int prec, double value)
{
  static const double pow10_neg[UI_PRECISION_FLOAT_MAX + 1] = {
      1e0, 1e-1, 1e-2, 1e-3, 1e-4, 1e-5, 1e-6};
  static const double max_pow = 10000000.0; /* pow(10, UI_PRECISION_FLOAT_MAX) */

  BLI_assert(prec <= UI_PRECISION_FLOAT_MAX);
  BLI_assert(fabs(pow10_neg[prec] - pow(10, -prec)) < 1e-16);

  /* Check on the number of decimal places need to display the number,
   * this is so 0.00001 is not displayed as 0.00,
   * _but_, this is only for small values si 10.0001 will not get the same treatment.
   */
  value = fabs(value);
  if ((value < pow10_neg[prec]) && (value > (1.0 / max_pow))) {
    int value_i = (int)((value * max_pow) + 0.5);
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

bool UI_but_online_manual_id(const uiBut *but, char *r_str, size_t maxlength)
{
  if (but->rnapoin.owner_id && but->rnapoin.data && but->rnaprop) {
    BLI_snprintf(r_str,
                 maxlength,
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

bool UI_but_online_manual_id_from_active(const struct bContext *C, char *r_str, size_t maxlength)
{
  uiBut *but = UI_context_active_but_get(C);

  if (but) {
    return UI_but_online_manual_id(but, r_str, maxlength);
  }

  *r_str = '\0';
  return false;
}

/* -------------------------------------------------------------------- */
/** \name Button Store
 *
 * Modal Button Store API.
 *
 * Store for modal operators & handlers to register button pointers
 * which are maintained while drawing or NULL when removed.
 *
 * This is needed since button pointers are continuously freed and re-allocated.
 *
 * \{ */

struct uiButStore {
  struct uiButStore *next, *prev;
  uiBlock *block;
  ListBase items;
};

struct uiButStoreElem {
  struct uiButStoreElem *next, *prev;
  uiBut **but_p;
};

/**
 * Create a new button store, the caller must manage and run #UI_butstore_free
 */
uiButStore *UI_butstore_create(uiBlock *block)
{
  uiButStore *bs_handle = MEM_callocN(sizeof(uiButStore), __func__);

  bs_handle->block = block;
  BLI_addtail(&block->butstore, bs_handle);

  return bs_handle;
}

void UI_butstore_free(uiBlock *block, uiButStore *bs_handle)
{
  /* Workaround for button store being moved into new block,
   * which then can't use the previous buttons state
   * ('ui_but_update_from_old_block' fails to find a match),
   * keeping the active button in the old block holding a reference
   * to the button-state in the new block: see T49034.
   *
   * Ideally we would manage moving the 'uiButStore', keeping a correct state.
   * All things considered this is the most straightforward fix - Campbell.
   */
  if (block != bs_handle->block && bs_handle->block != NULL) {
    block = bs_handle->block;
  }

  BLI_freelistN(&bs_handle->items);
  BLI_assert(BLI_findindex(&block->butstore, bs_handle) != -1);
  BLI_remlink(&block->butstore, bs_handle);

  MEM_freeN(bs_handle);
}

bool UI_butstore_is_valid(uiButStore *bs)
{
  return (bs->block != NULL);
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
  uiButStoreElem *bs_elem = MEM_callocN(sizeof(uiButStoreElem), __func__);
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

/**
 * Update the pointer for a registered button.
 */
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

/**
 * NULL all pointers, don't free since the owner needs to be able to inspect.
 */
void UI_butstore_clear(uiBlock *block)
{
  LISTBASE_FOREACH (uiButStore *, bs_handle, &block->butstore) {
    bs_handle->block = NULL;
    LISTBASE_FOREACH (uiButStoreElem *, bs_elem, &bs_handle->items) {
      *bs_elem->but_p = NULL;
    }
  }
}

/**
 * Map freed buttons from the old block and update pointers.
 */
void UI_butstore_update(uiBlock *block)
{
  /* move this list to the new block */
  if (block->oldblock) {
    if (block->oldblock->butstore.first) {
      BLI_movelisttolist(&block->butstore, &block->oldblock->butstore);
    }
  }

  if (LIKELY(block->butstore.first == NULL)) {
    return;
  }

  /* warning, loop-in-loop, in practice we only store <10 buttons at a time,
   * so this isn't going to be a problem, if that changes old-new mapping can be cached first */
  LISTBASE_FOREACH (uiButStore *, bs_handle, &block->butstore) {
    BLI_assert(ELEM(bs_handle->block, NULL, block) ||
               (block->oldblock && block->oldblock == bs_handle->block));

    if (bs_handle->block == block->oldblock) {
      bs_handle->block = block;

      LISTBASE_FOREACH (uiButStoreElem *, bs_elem, &bs_handle->items) {
        if (*bs_elem->but_p) {
          uiBut *but_new = ui_but_find_new(block, *bs_elem->but_p);

          /* can be NULL if the buttons removed,
           * note: we could allow passing in a callback when buttons are removed
           * so the caller can cleanup */
          *bs_elem->but_p = but_new;
        }
      }
    }
  }
}

/** \} */
