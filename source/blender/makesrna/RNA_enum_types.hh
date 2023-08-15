/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup RNA
 */

#include "RNA_types.hh"

struct ID;
struct bNodeSocketType;
struct bNodeTreeType;
struct bNodeType;

/* Types */
#define DEF_ENUM(id) extern const EnumPropertyItem id[];
#include "RNA_enum_items.hh"

extern const EnumPropertyItem *rna_enum_attribute_domain_itemf(struct ID *id,
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
extern const struct IDFilterEnumPropertyItem rna_enum_id_type_filter_items[];

/* API calls */
int rna_node_tree_idname_to_enum(const char *idname);
struct bNodeTreeType *rna_node_tree_type_from_enum(int value);
const EnumPropertyItem *rna_node_tree_type_itemf(void *data,
                                                 bool (*poll)(void *data, struct bNodeTreeType *),
                                                 bool *r_free);

struct bNodeSocketType *rna_node_socket_type_from_enum(int value);
const EnumPropertyItem *rna_node_socket_type_itemf(
    void *data, bool (*poll)(void *data, struct bNodeSocketType *), bool *r_free);

struct PointerRNA;
struct PropertyRNA;
struct bContext;

const EnumPropertyItem *rna_TransformOrientation_itemf(struct bContext *C,
                                                       struct PointerRNA *ptr,
                                                       struct PropertyRNA *prop,
                                                       bool *r_free);

/**
 * Generic functions, return an enum from library data, index is the position
 * in the linked list can add more for different types as needed.
 */
const EnumPropertyItem *RNA_action_itemf(struct bContext *C,
                                         struct PointerRNA *ptr,
                                         struct PropertyRNA *prop,
                                         bool *r_free);
#if 0
EnumPropertyItem *RNA_action_local_itemf(struct bContext *C,
                                         struct PointerRNA *ptr,
                                         struct PropertyRNA *prop,
                                         bool *r_free);
#endif
const EnumPropertyItem *RNA_collection_itemf(struct bContext *C,
                                             struct PointerRNA *ptr,
                                             struct PropertyRNA *prop,
                                             bool *r_free);
const EnumPropertyItem *RNA_collection_local_itemf(struct bContext *C,
                                                   struct PointerRNA *ptr,
                                                   struct PropertyRNA *prop,
                                                   bool *r_free);
const EnumPropertyItem *RNA_image_itemf(struct bContext *C,
                                        struct PointerRNA *ptr,
                                        struct PropertyRNA *prop,
                                        bool *r_free);
const EnumPropertyItem *RNA_image_local_itemf(struct bContext *C,
                                              struct PointerRNA *ptr,
                                              struct PropertyRNA *prop,
                                              bool *r_free);
const EnumPropertyItem *RNA_scene_itemf(struct bContext *C,
                                        struct PointerRNA *ptr,
                                        struct PropertyRNA *prop,
                                        bool *r_free);
const EnumPropertyItem *RNA_scene_without_active_itemf(struct bContext *C,
                                                       struct PointerRNA *ptr,
                                                       struct PropertyRNA *prop,
                                                       bool *r_free);
const EnumPropertyItem *RNA_scene_local_itemf(struct bContext *C,
                                              struct PointerRNA *ptr,
                                              struct PropertyRNA *prop,
                                              bool *r_free);
const EnumPropertyItem *RNA_movieclip_itemf(struct bContext *C,
                                            struct PointerRNA *ptr,
                                            struct PropertyRNA *prop,
                                            bool *r_free);
const EnumPropertyItem *RNA_movieclip_local_itemf(struct bContext *C,
                                                  struct PointerRNA *ptr,
                                                  struct PropertyRNA *prop,
                                                  bool *r_free);
const EnumPropertyItem *RNA_mask_itemf(struct bContext *C,
                                       struct PointerRNA *ptr,
                                       struct PropertyRNA *prop,
                                       bool *r_free);
const EnumPropertyItem *RNA_mask_local_itemf(struct bContext *C,
                                             struct PointerRNA *ptr,
                                             struct PropertyRNA *prop,
                                             bool *r_free);

/* Non confirming, utility function. */
const EnumPropertyItem *RNA_enum_node_tree_types_itemf_impl(struct bContext *C, bool *r_free);
