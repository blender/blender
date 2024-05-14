/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup RNA
 */

#include "RNA_types.hh"

struct ID;
struct PointerRNA;
struct PropertyRNA;
struct bContext;
namespace blender::bke {
struct bNodeTreeType;
struct bNodeType;
struct bNodeSocketType;
struct RuntimeNodeEnumItems;
}  // namespace blender::bke

/* Types */
#define DEF_ENUM(id) extern const EnumPropertyItem id[];
#include "RNA_enum_items.hh"

extern const EnumPropertyItem *rna_enum_attribute_domain_itemf(ID *id,
                                                               bool include_instances,
                                                               bool *r_free);

/**
 * For ID filters (#FILTER_ID_AC, #FILTER_ID_AR, ...) an int isn't enough. This version allows 64
 * bit integers. So can't use the regular #EnumPropertyItem. Would be nice if RNA supported this
 * itself.
 *
 * Meant to be used with #RNA_def_property_boolean_sdna() which supports 64 bit flags as well.
 */
struct IDFilterEnumPropertyItem {
  const uint64_t flag;
  const char *identifier;
  const int icon;
  const char *name;
  const char *description;
};
extern const IDFilterEnumPropertyItem rna_enum_id_type_filter_items[];

/* API calls */
int rna_node_tree_idname_to_enum(const char *idname);
blender::bke::bNodeTreeType *rna_node_tree_type_from_enum(int value);
const EnumPropertyItem *rna_node_tree_type_itemf(
    void *data, bool (*poll)(void *data, blender::bke::bNodeTreeType *), bool *r_free);

int rna_node_socket_idname_to_enum(const char *idname);
blender::bke::bNodeSocketType *rna_node_socket_type_from_enum(int value);
const EnumPropertyItem *rna_node_socket_type_itemf(
    void *data, bool (*poll)(void *data, blender::bke::bNodeSocketType *), bool *r_free);

const EnumPropertyItem *rna_TransformOrientation_itemf(bContext *C,
                                                       PointerRNA *ptr,
                                                       PropertyRNA *prop,
                                                       bool *r_free);

/**
 * Generic functions, return an enum from library data, index is the position
 * in the linked list can add more for different types as needed.
 */
const EnumPropertyItem *RNA_action_itemf(bContext *C,
                                         PointerRNA *ptr,
                                         PropertyRNA *prop,
                                         bool *r_free);
#if 0
const EnumPropertyItem *RNA_action_local_itemf(bContext *C,
                                               PointerRNA *ptr,
                                               PropertyRNA *prop,
                                               bool *r_free);
#endif
const EnumPropertyItem *RNA_collection_itemf(bContext *C,
                                             PointerRNA *ptr,
                                             PropertyRNA *prop,
                                             bool *r_free);
const EnumPropertyItem *RNA_collection_local_itemf(bContext *C,
                                                   PointerRNA *ptr,
                                                   PropertyRNA *prop,
                                                   bool *r_free);
const EnumPropertyItem *RNA_image_itemf(bContext *C,
                                        PointerRNA *ptr,
                                        PropertyRNA *prop,
                                        bool *r_free);
const EnumPropertyItem *RNA_image_local_itemf(bContext *C,
                                              PointerRNA *ptr,
                                              PropertyRNA *prop,
                                              bool *r_free);
const EnumPropertyItem *RNA_scene_itemf(bContext *C,
                                        PointerRNA *ptr,
                                        PropertyRNA *prop,
                                        bool *r_free);
const EnumPropertyItem *RNA_scene_without_active_itemf(bContext *C,
                                                       PointerRNA *ptr,
                                                       PropertyRNA *prop,
                                                       bool *r_free);
const EnumPropertyItem *RNA_scene_local_itemf(bContext *C,
                                              PointerRNA *ptr,
                                              PropertyRNA *prop,
                                              bool *r_free);
const EnumPropertyItem *RNA_movieclip_itemf(bContext *C,
                                            PointerRNA *ptr,
                                            PropertyRNA *prop,
                                            bool *r_free);
const EnumPropertyItem *RNA_movieclip_local_itemf(bContext *C,
                                                  PointerRNA *ptr,
                                                  PropertyRNA *prop,
                                                  bool *r_free);
const EnumPropertyItem *RNA_mask_itemf(bContext *C,
                                       PointerRNA *ptr,
                                       PropertyRNA *prop,
                                       bool *r_free);
const EnumPropertyItem *RNA_mask_local_itemf(bContext *C,
                                             PointerRNA *ptr,
                                             PropertyRNA *prop,
                                             bool *r_free);

/* Non confirming, utility function. */
const EnumPropertyItem *RNA_enum_node_tree_types_itemf_impl(bContext *C, bool *r_free);

const EnumPropertyItem *RNA_node_enum_definition_itemf(
    const blender::bke::RuntimeNodeEnumItems &enum_items, bool *r_free);
