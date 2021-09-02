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
 */

#pragma once

/** \file
 * \ingroup RNA
 */

#include "RNA_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ID;
struct bNodeSocketType;
struct bNodeTreeType;
struct bNodeType;

/* Types */
#define DEF_ENUM(id) extern const EnumPropertyItem id[];
#include "RNA_enum_items.h"

extern const EnumPropertyItem *rna_enum_attribute_domain_itemf(struct ID *id, bool *r_free);

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
int rna_node_tree_type_to_enum(struct bNodeTreeType *typeinfo);
int rna_node_tree_idname_to_enum(const char *idname);
struct bNodeTreeType *rna_node_tree_type_from_enum(int value);
const EnumPropertyItem *rna_node_tree_type_itemf(void *data,
                                                 bool (*poll)(void *data, struct bNodeTreeType *),
                                                 bool *r_free);

int rna_node_type_to_enum(struct bNodeType *typeinfo);
int rna_node_idname_to_enum(const char *idname);
struct bNodeType *rna_node_type_from_enum(int value);
const EnumPropertyItem *rna_node_type_itemf(void *data,
                                            bool (*poll)(void *data, struct bNodeType *),
                                            bool *r_free);

int rna_node_socket_type_to_enum(struct bNodeSocketType *typeinfo);
int rna_node_socket_idname_to_enum(const char *idname);
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

/* Generic functions, return an enum from library data, index is the position
 * in the linked list can add more for different types as needed */
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

#ifdef __cplusplus
}
#endif
