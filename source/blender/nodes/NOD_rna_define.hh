/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "RNA_define.h"

#include "WM_types.hh" /* For notifier defines */

void rna_Node_update(Main *bmain, Scene *scene, PointerRNA *ptr);
void rna_Node_socket_update(Main *bmain, Scene *scene, PointerRNA *ptr);

namespace blender::nodes {

struct EnumRNAAccessors {
  EnumPropertyGetFunc getter;
  EnumPropertySetFunc setter;

  EnumRNAAccessors(EnumPropertyGetFunc getter, EnumPropertySetFunc setter)
      : getter(getter), setter(setter)
  {
  }
};

/**
 * Generates accessor methods for a property stored directly in the `bNode`, typically
 * `bNode->custom1` or similar.
 */
#define NOD_inline_enum_accessors(member) \
  EnumRNAAccessors( \
      [](PointerRNA *ptr, PropertyRNA * /*prop*/) -> int { \
        const bNode &node = *static_cast<const bNode *>(ptr->data); \
        return node.member; \
      }, \
      [](PointerRNA *ptr, PropertyRNA * /*prop*/, const int value) { \
        bNode &node = *static_cast<bNode *>(ptr->data); \
        node.member = value; \
      })

/**
 * Generates accessor methods for a property stored in `bNode->storage`. This is expected to be
 * used in a node file that uses #NODE_STORAGE_FUNCS.
 */
#define NOD_storage_enum_accessors(member) \
  EnumRNAAccessors( \
      [](PointerRNA *ptr, PropertyRNA * /*prop*/) -> int { \
        const bNode &node = *static_cast<const bNode *>(ptr->data); \
        return node_storage(node).member; \
      }, \
      [](PointerRNA *ptr, PropertyRNA * /*prop*/, const int value) { \
        bNode &node = *static_cast<bNode *>(ptr->data); \
        node_storage(node).member = value; \
      })

inline PropertyRNA *RNA_def_node_enum(StructRNA *srna,
                                      const char *identifier,
                                      const char *ui_name,
                                      const char *ui_description,
                                      const EnumPropertyItem *static_items,
                                      const EnumRNAAccessors accessors,
                                      std::optional<int> default_value = std::nullopt,
                                      const EnumPropertyItemFunc item_func = nullptr)
{
  PropertyRNA *prop = RNA_def_property(srna, identifier, PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_funcs_runtime(prop, accessors.getter, accessors.setter, item_func);
  RNA_def_property_enum_items(prop, static_items);
  if (default_value.has_value()) {
    RNA_def_property_enum_default(prop, *default_value);
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);
  RNA_def_property_update_runtime(prop, rna_Node_socket_update);
  RNA_def_property_update_notifier(prop, NC_NODE | NA_EDITED);
  return prop;
}

}  // namespace blender::nodes
