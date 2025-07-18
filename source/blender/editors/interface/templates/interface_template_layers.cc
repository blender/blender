/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BKE_context.hh"

#include "BLI_array.hh"
#include "BLI_string_ref.hh"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"
#include "interface_intern.hh"

using blender::StringRefNull;

static void handle_layer_buttons(bContext *C, void *arg1, void *arg2)
{
  uiBut *but = static_cast<uiBut *>(arg1);
  const int cur = POINTER_AS_INT(arg2);
  wmWindow *win = CTX_wm_window(C);
  const bool shift = win->eventstate->modifier & KM_SHIFT;

  if (!shift) {
    const int tot = RNA_property_array_length(&but->rnapoin, but->rnaprop);

    BLI_assert(cur < tot);
    blender::Array<bool, RNA_STACK_ARRAY> value_array(tot);
    value_array.fill(false);
    value_array[cur] = true;

    /* Normally clicking only selects one layer */
    RNA_property_boolean_set_array(&but->rnapoin, but->rnaprop, value_array.data());
  }

  /* view3d layer change should update depsgraph (invisible object changed maybe) */
  /* see `view3d_header.cc` */
}

void uiTemplateLayers(uiLayout *layout,
                      PointerRNA *ptr,
                      const StringRefNull propname,
                      PointerRNA *used_ptr,
                      const char *used_propname,
                      int active_layer)
{
  const int cols_per_group = 5;

  PropertyRNA *prop = RNA_struct_find_property(ptr, propname.c_str());
  if (!prop) {
    RNA_warning(
        "layers property not found: %s.%s", RNA_struct_identifier(ptr->type), propname.c_str());
    return;
  }

  /* the number of layers determines the way we group them
   * - we want 2 rows only (for now)
   * - The number of columns (cols) is the total number of buttons per row the 'remainder'
   *   is added to this, as it will be ok to have first row slightly wider if need be.
   * - For now, only split into groups if group will have at least 5 items.
   */
  const int layers = RNA_property_array_length(ptr, prop);
  const int cols = (layers / 2) + (layers % 2);
  const int groups = ((cols / 2) < cols_per_group) ? (1) : (cols / cols_per_group);

  PropertyRNA *used_prop = nullptr;
  if (used_ptr && used_propname) {
    used_prop = RNA_struct_find_property(used_ptr, used_propname);
    if (!used_prop) {
      RNA_warning("used layers property not found: %s.%s",
                  RNA_struct_identifier(ptr->type),
                  used_propname);
      return;
    }

    if (RNA_property_array_length(used_ptr, used_prop) < layers) {
      used_prop = nullptr;
    }
  }

  /* layers are laid out going across rows, with the columns being divided into groups */

  for (int group = 0; group < groups; group++) {
    uiLayout *uCol = &layout->column(true);

    for (int row = 0; row < 2; row++) {
      uiLayout *uRow = &uCol->row(true);
      uiBlock *block = uRow->block();
      int layer = groups * cols_per_group * row + cols_per_group * group;

      /* add layers as toggle buts */
      for (int col = 0; (col < cols_per_group) && (layer < layers); col++, layer++) {
        int icon = 0;
        const int butlay = 1 << layer;

        if (active_layer & butlay) {
          icon = ICON_LAYER_ACTIVE;
        }
        else if (used_prop && RNA_property_boolean_get_index(used_ptr, used_prop, layer)) {
          icon = ICON_LAYER_USED;
        }

        uiBut *but = uiDefAutoButR(
            block, ptr, prop, layer, "", icon, 0, 0, UI_UNIT_X / 2, UI_UNIT_Y / 2);
        UI_but_func_set(but, handle_layer_buttons, but, POINTER_FROM_INT(layer));
        but->type = ButType::Toggle;
      }
    }
  }
}
