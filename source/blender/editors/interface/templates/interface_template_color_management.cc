/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BLI_string_ref.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

using blender::StringRefNull;

void uiTemplateColorspaceSettings(uiLayout *layout, PointerRNA *ptr, const StringRefNull propname)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname.c_str());

  if (!prop) {
    printf("%s: property not found: %s.%s\n",
           __func__,
           RNA_struct_identifier(ptr->type),
           propname.c_str());
    return;
  }

  PointerRNA colorspace_settings_ptr = RNA_property_pointer_get(ptr, prop);

  layout->prop(&colorspace_settings_ptr, "name", UI_ITEM_NONE, IFACE_("Color Space"), ICON_NONE);
}

void uiTemplateColormanagedViewSettings(uiLayout *layout,
                                        bContext * /*C*/,
                                        PointerRNA *ptr,
                                        const StringRefNull propname)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname.c_str());

  if (!prop) {
    printf("%s: property not found: %s.%s\n",
           __func__,
           RNA_struct_identifier(ptr->type),
           propname.c_str());
    return;
  }

  PointerRNA view_transform_ptr = RNA_property_pointer_get(ptr, prop);
  ColorManagedViewSettings *view_settings = static_cast<ColorManagedViewSettings *>(
      view_transform_ptr.data);

  uiLayout *col = &layout->column(false);
  col->prop(&view_transform_ptr, "view_transform", UI_ITEM_NONE, IFACE_("View"), ICON_NONE);
  col->prop(&view_transform_ptr, "look", UI_ITEM_NONE, IFACE_("Look"), ICON_NONE);

  col = &layout->column(false);
  col->prop(&view_transform_ptr, "exposure", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(&view_transform_ptr, "gamma", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  col = &layout->column(false);
  col->prop(&view_transform_ptr, "use_curve_mapping", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  if (view_settings->flag & COLORMANAGE_VIEW_USE_CURVES) {
    uiTemplateCurveMapping(
        col, &view_transform_ptr, "curve_mapping", 'c', true, false, false, false, false);
  }

  col = &layout->column(false);
  col->prop(&view_transform_ptr, "use_white_balance", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  if (view_settings->flag & COLORMANAGE_VIEW_USE_WHITE_BALANCE) {
    col->prop(
        &view_transform_ptr, "white_balance_temperature", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(&view_transform_ptr, "white_balance_tint", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
}
