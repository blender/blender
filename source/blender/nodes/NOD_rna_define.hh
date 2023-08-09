/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "RNA_define.h"

/**
 * Generates accessor methods for a property stored in `bNode->storage`. This is expected to be
 * used in a node file that uses #NODE_STORAGE_FUNCS.
 */
#define RNA_def_property_enum_node_storage(prop, member) \
  RNA_def_property_enum_funcs_runtime( \
      prop, \
      [](PointerRNA *ptr, PropertyRNA * /*prop*/) -> int { \
        const bNode &node = *static_cast<const bNode *>(ptr->data); \
        return node_storage(node).member; \
      }, \
      [](PointerRNA *ptr, PropertyRNA * /*prop*/, const int value) { \
        bNode &node = *static_cast<bNode *>(ptr->data); \
        node_storage(node).member = value; \
      }, \
      nullptr)

/**
 * Generates accessor methods for a property stored directly in the `bNode`, typically
 * `bNode->custom1` or similar.
 */
#define RNA_def_property_enum_node(prop, member) \
  RNA_def_property_enum_funcs_runtime( \
      prop, \
      [](PointerRNA *ptr, PropertyRNA * /*prop*/) -> int { \
        const bNode &node = *static_cast<const bNode *>(ptr->data); \
        return node.member; \
      }, \
      [](PointerRNA *ptr, PropertyRNA * /*prop*/, const int value) { \
        bNode &node = *static_cast<bNode *>(ptr->data); \
        node.member = value; \
      }, \
      nullptr)

void rna_Node_update(Main *bmain, Scene *scene, PointerRNA *ptr);
void rna_Node_socket_update(Main *bmain, Scene *scene, PointerRNA *ptr);
