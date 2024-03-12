/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_rna_define.hh"

namespace blender::nodes {

const EnumPropertyItem *enum_items_filter(const EnumPropertyItem *original_item_array,
                                          FunctionRef<bool(const EnumPropertyItem &item)> fn)
{
  EnumPropertyItem *item_array = nullptr;
  int items_len = 0;

  for (const EnumPropertyItem *item = original_item_array; item->identifier != nullptr; item++) {
    if (fn(*item)) {
      RNA_enum_item_add(&item_array, &items_len, item);
    }
  }

  RNA_enum_item_end(&item_array, &items_len);
  return item_array;
}

PropertyRNA *RNA_def_node_enum(StructRNA *srna,
                               const char *identifier,
                               const char *ui_name,
                               const char *ui_description,
                               const EnumPropertyItem *static_items,
                               const EnumRNAAccessors accessors,
                               std::optional<int> default_value,
                               const EnumPropertyItemFunc item_func,
                               const bool allow_animation)
{
  PropertyRNA *prop = RNA_def_property(srna, identifier, PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_funcs_runtime(prop, accessors.getter, accessors.setter, item_func);
  RNA_def_property_enum_items(prop, static_items);
  if (default_value.has_value()) {
    RNA_def_property_enum_default(prop, *default_value);
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);
  if (allow_animation) {
    RNA_def_property_update_runtime(prop, rna_Node_update);
  }
  else {
    RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
    RNA_def_property_update_runtime(prop, rna_Node_socket_update);
  }
  RNA_def_property_update_notifier(prop, NC_NODE | NA_EDITED);
  return prop;
}

}  // namespace blender::nodes
