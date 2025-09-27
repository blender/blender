/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_function_ref.hh"

#include "DNA_node_types.h"

#include "RNA_define.hh"

#include "WM_types.hh" /* For notifier defines */

void rna_Node_update(Main *bmain, Scene *scene, PointerRNA *ptr);
void rna_Node_socket_update(Main *bmain, Scene *scene, PointerRNA *ptr);
void rna_Node_update_relations(Main *bmain, Scene *scne, PointerRNA *ptr);
void rna_Node_Viewer_shortcut_node_set(PointerRNA *ptr, PropertyRNA *prop, int value);
const EnumPropertyItem *rna_NodeSocket_structure_type_item_filter(
    const bNodeTree *ntree, const eNodeSocketDatatype socket_type, bool *r_free);

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

struct BooleanRNAAccessors {
  BooleanPropertyGetFunc getter;
  BooleanPropertySetFunc setter;

  BooleanRNAAccessors(BooleanPropertyGetFunc getter, BooleanPropertySetFunc setter)
      : getter(getter), setter(setter)
  {
  }
};

/**
 * Generates accessor methods for a property stored directly in the `bNode`, typically
 * `bNode->custom1` or similar.
 */
#define NOD_inline_boolean_accessors(member, flag) \
  BooleanRNAAccessors( \
      [](PointerRNA *ptr, PropertyRNA * /*prop*/) -> bool { \
        const bNode &node = *static_cast<const bNode *>(ptr->data); \
        return node.member & (flag); \
      }, \
      [](PointerRNA *ptr, PropertyRNA * /*prop*/, const bool value) { \
        bNode &node = *static_cast<bNode *>(ptr->data); \
        SET_FLAG_FROM_TEST(node.member, value, (flag)); \
      })

/**
 * Generates accessor methods for a property stored in `bNode->storage`. This is expected to be
 * used in a node file that uses #NODE_STORAGE_FUNCS.
 */
#define NOD_storage_boolean_accessors(member, flag) \
  BooleanRNAAccessors( \
      [](PointerRNA *ptr, PropertyRNA * /*prop*/) -> bool { \
        const bNode &node = *static_cast<const bNode *>(ptr->data); \
        return node_storage(node).member & (flag); \
      }, \
      [](PointerRNA *ptr, PropertyRNA * /*prop*/, const bool value) { \
        bNode &node = *static_cast<bNode *>(ptr->data); \
        SET_FLAG_FROM_TEST(node_storage(node).member, value, (flag)); \
      })

const EnumPropertyItem *enum_items_filter(const EnumPropertyItem *original_item_array,
                                          FunctionRef<bool(const EnumPropertyItem &item)> fn);

PropertyRNA *RNA_def_node_enum(StructRNA *srna,
                               const char *identifier,
                               const char *ui_name,
                               const char *ui_description,
                               const EnumPropertyItem *static_items,
                               const EnumRNAAccessors accessors,
                               std::optional<int> default_value = std::nullopt,
                               const EnumPropertyItemFunc item_func = nullptr,
                               bool allow_animation = false);

PropertyRNA *RNA_def_node_boolean(StructRNA *srna,
                                  const char *identifier,
                                  const char *ui_name,
                                  const char *ui_description,
                                  const BooleanRNAAccessors accessors,
                                  std::optional<bool> default_value = std::nullopt,
                                  bool allow_animation = false);

}  // namespace blender::nodes
