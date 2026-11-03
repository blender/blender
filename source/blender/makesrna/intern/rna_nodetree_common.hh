/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#pragma once

#include <optional>
#include <string>

#include "RNA_types.hh"

#ifdef RNA_RUNTIME
#  include "BLI_function_ref.hh"

#  include "BKE_main_invariants.hh"
#  include "BKE_node.hh"
#  include "BKE_node_tree_update.hh"
#  include "BKE_report.hh"

#  include "DNA_array_utils.hh"
#  include "DNA_node_types.h"

#  include "ED_node.hh"

#  include "NOD_rna_define.hh"
#  include "NOD_socket_items.hh"

#  include "RNA_access.hh"
#  include "RNA_enum_types.hh"

#  include "WM_api.hh"
#  include "WM_types.hh"
#endif

namespace blender {

struct BlenderRNA;
struct StructRNA;

extern const EnumPropertyItem prop_shader_output_target_items[];

#ifdef RNA_RUNTIME

struct IDProperty;
struct Main;
struct ReportList;
struct Scene;

StructRNA *rna_Node_refine(PointerRNA *ptr);
std::optional<std::string> rna_Node_path(const PointerRNA *ptr);
bool rna_Node_unregister(Main *bmain, StructRNA *type);
IDProperty **rna_Node_idprops(PointerRNA *ptr);
IDProperty **rna_Node_system_idprops(PointerRNA *ptr);
StructRNA *rna_ShaderNode_register(Main *bmain,
                                   ReportList *reports,
                                   void *data,
                                   const char *identifier,
                                   StructValidateFunc validate,
                                   StructCallbackFunc call,
                                   StructFreeFunc free);
StructRNA *rna_CompositorNode_register(Main *bmain,
                                       ReportList *reports,
                                       void *data,
                                       const char *identifier,
                                       StructValidateFunc validate,
                                       StructCallbackFunc call,
                                       StructFreeFunc free);
StructRNA *rna_TextureNode_register(Main *bmain,
                                    ReportList *reports,
                                    void *data,
                                    const char *identifier,
                                    StructValidateFunc validate,
                                    StructCallbackFunc call,
                                    StructFreeFunc free);
StructRNA *rna_GeometryNode_register(Main *bmain,
                                     ReportList *reports,
                                     void *data,
                                     const char *identifier,
                                     StructValidateFunc validate,
                                     StructCallbackFunc call,
                                     StructFreeFunc free);
StructRNA *rna_FunctionNode_register(Main *bmain,
                                     ReportList *reports,
                                     void *data,
                                     const char *identifier,
                                     StructValidateFunc validate,
                                     StructCallbackFunc call,
                                     StructFreeFunc free);

void rna_Node_update(Main *bmain, Scene *scene, PointerRNA *ptr);
void rna_Node_update_relations(Main *bmain, Scene *scene, PointerRNA *ptr);
void rna_Node_socket_update(Main *bmain, Scene *scene, PointerRNA *ptr);
void rna_ShaderNode_socket_update(Main *bmain, Scene *scene, PointerRNA *ptr);

bool rna_Node_is_registered_node_type(StructRNA *type);
PointerRNA rna_NodeInternal_input_template(StructRNA *srna, int index);
PointerRNA rna_NodeInternal_output_template(StructRNA *srna, int index);
PointerRNA rna_Node_paired_output_get(PointerRNA *ptr);
bool rna_Node_pair_with_output(
    ID *id, bNode *node, bContext *C, ReportList *reports, bNode *output_node);
void rna_Node_Viewer_shortcut_node_set(PointerRNA *ptr, int value);

const EnumPropertyItem *itemf_function_check(
    const EnumPropertyItem *original_item_array,
    FunctionRef<bool(const EnumPropertyItem *item)> value_supported);

template<typename Accessor>
void rna_Node_ItemArray_remove(ID *id,
                               bNode *node,
                               Main *bmain,
                               ReportList *reports,
                               typename Accessor::ItemT *item_to_remove)
{
  nodes::socket_items::SocketItemsRef ref = Accessor::get_items_from_node(*node);
  if (item_to_remove < *ref.items || item_to_remove >= *ref.items + *ref.items_num) {
    if constexpr (Accessor::has_name) {
      char **name_ptr = Accessor::get_name(*item_to_remove);
      if (name_ptr && *name_ptr) {
        BKE_reportf(reports, RPT_ERROR, "Unable to locate item '%s' in node", *name_ptr);
        return;
      }
    }
    else {
      BKE_report(reports, RPT_ERROR, "Unable to locate item in node");
    }
    return;
  }
  const int remove_index = item_to_remove - *ref.items;
  dna::array::remove_index(
      ref.items, ref.items_num, ref.active_index, remove_index, Accessor::destruct_item);

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  BKE_ntree_update_tag_node_property(ntree, node);
  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

template<typename Accessor> void rna_Node_ItemArray_clear(ID *id, bNode *node, Main *bmain)
{
  nodes::socket_items::SocketItemsRef ref = Accessor::get_items_from_node(*node);
  dna::array::clear(ref.items, ref.items_num, ref.active_index, Accessor::destruct_item);

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  BKE_ntree_update_tag_node_property(ntree, node);
  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

template<typename Accessor>
void rna_Node_ItemArray_move(
    ID *id, bNode *node, Main *bmain, const int from_index, const int to_index)
{
  nodes::socket_items::SocketItemsRef ref = Accessor::get_items_from_node(*node);
  const int items_num = *ref.items_num;
  if (from_index < 0 || to_index < 0 || from_index >= items_num || to_index >= items_num) {
    return;
  }
  dna::array::move_index(*ref.items, items_num, from_index, to_index);

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  BKE_ntree_update_tag_node_property(ntree, node);
  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);
}

template<typename Accessor> PointerRNA rna_Node_ItemArray_active_get(PointerRNA *ptr)
{
  bNode *node = ptr->data_as<bNode>();
  nodes::socket_items::SocketItemsRef ref = Accessor::get_items_from_node(*node);
  typename Accessor::ItemT *active_item = nullptr;
  const int active_index = *ref.active_index;
  const int items_num = *ref.items_num;
  if (active_index >= 0 && active_index < items_num) {
    active_item = &(*ref.items)[active_index];
  }
  return RNA_pointer_create_discrete(ptr->owner_id, *Accessor::item_srna, active_item);
}
template<typename Accessor>
void rna_Node_ItemArray_active_set(PointerRNA *ptr, PointerRNA value, ReportList * /*reports*/)
{
  using ItemT = typename Accessor::ItemT;
  bNode *node = ptr->data_as<bNode>();
  ItemT *item = static_cast<ItemT *>(value.data);

  nodes::socket_items::SocketItemsRef ref = Accessor::get_items_from_node(*node);
  if (item >= *ref.items && item < *ref.items + *ref.items_num) {
    *ref.active_index = item - *ref.items;
  }
}

template<typename Accessor>
void rna_Node_ItemArray_item_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  using ItemT = typename Accessor::ItemT;
  bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  ItemT &item = *static_cast<ItemT *>(ptr->data);
  bNode *node = nodes::socket_items::find_node_by_item<Accessor>(ntree, item);
  BLI_assert(node != nullptr);

  BKE_ntree_update_tag_node_property(&ntree, node);
  BKE_main_ensure_invariants(*bmain, ntree.id);
}

template<typename Accessor>
const EnumPropertyItem *rna_Node_ItemArray_socket_type_itemf(bContext * /*C*/,
                                                             PointerRNA *ptr,
                                                             PropertyRNA * /*prop*/,
                                                             bool *r_free)
{
  *r_free = true;
  bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  return itemf_function_check(
      rna_enum_node_socket_data_type_items, [&](const EnumPropertyItem *item) {
        return Accessor::supports_socket_type(eNodeSocketDatatype(item->value), ntree.type);
      });
}

template<typename Accessor>
void rna_Node_ItemArray_item_name_set(PointerRNA *ptr, const char *value)
{
  using ItemT = typename Accessor::ItemT;
  bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  ItemT &item = *static_cast<ItemT *>(ptr->data);
  bNode *node = nodes::socket_items::find_node_by_item<Accessor>(ntree, item);
  BLI_assert(node != nullptr);
  nodes::socket_items::set_item_name_and_make_unique<Accessor>(*node, item, value);
}

template<typename Accessors> void rna_Node_ItemArray_item_color_get(PointerRNA *ptr, float *values)
{
  using ItemT = typename Accessors::ItemT;
  ItemT &item = *static_cast<ItemT *>(ptr->data);
  const StringRefNull socket_type_idname = *bke::node_static_socket_type(
      Accessors::get_socket_type(item), 0);
  ED_node_type_draw_color(socket_type_idname.c_str(), values);
}

template<typename Accessor>
typename Accessor::ItemT *rna_Node_ItemArray_new_with_socket_and_name(
    ID *id, bNode *node, Main *bmain, ReportList *reports, int socket_type, const char *name)
{
  using ItemT = typename Accessor::ItemT;

  bNodeTree *ntree = reinterpret_cast<bNodeTree *>(id);
  if (!Accessor::supports_socket_type(eNodeSocketDatatype(socket_type), ntree->type)) {
    BKE_report(reports, RPT_ERROR, "Unable to create item with this socket type");
    return nullptr;
  }
  ItemT *new_item = nodes::socket_items::add_item_with_socket_type_and_name<Accessor>(
      *ntree, *node, eNodeSocketDatatype(socket_type), name);

  BKE_ntree_update_tag_node_property(ntree, node);
  BKE_main_ensure_invariants(*bmain, ntree->id);
  WM_main_add_notifier(NC_NODE | NA_EDITED, ntree);

  return new_item;
}

template<typename Accessor>
const EnumPropertyItem *rna_Node_ItemArray_structure_type_itemf(bContext * /*C*/,
                                                                PointerRNA *ptr,
                                                                PropertyRNA * /*prop*/,
                                                                bool *r_free)
{
  using ItemT = typename Accessor::ItemT;

  const bNodeTree *ntree = reinterpret_cast<const bNodeTree *>(ptr->owner_id);
  const ItemT &item = *static_cast<const ItemT *>(ptr->data);
  const eNodeSocketDatatype socket_type = Accessor::get_socket_type(item);
  return rna_NodeSocket_structure_type_item_filter(ntree, socket_type, r_free);
}

void rna_NodeGroup_update(Main *bmain, Scene *scene, PointerRNA *ptr);
void rna_NodeGroup_node_tree_set(PointerRNA *ptr, PointerRNA value, ReportList *reports);
bool rna_NodeGroup_node_tree_poll(PointerRNA *ptr, PointerRNA value);

#else

StructRNA *define_specific_node(BlenderRNA *brna, const char *struct_name, const char *base_name);

void def_group(BlenderRNA *brna, StructRNA *srna);
void def_math(BlenderRNA *brna, StructRNA *srna);
void def_mix_rgb(BlenderRNA *brna, StructRNA *srna);
void def_rgb_curve(BlenderRNA *brna, StructRNA *srna);
void def_time(BlenderRNA *brna, StructRNA *srna);
void def_colorramp(BlenderRNA *brna, StructRNA *srna);
void def_common_zone_input(BlenderRNA *, StructRNA *srna);
extern const EnumPropertyItem prop_image_extension[];
void rna_def_node_item_array_common_functions(StructRNA *srna,
                                              const char *item_name,
                                              const char *accessor_name);
void rna_def_node_item_array_new_with_socket_and_name(StructRNA *srna,
                                                      const char *item_name,
                                                      const char *accessor_name);
void rna_def_node_item_array_socket_item_common(StructRNA *srna,
                                                const char *accessor,
                                                const bool add_socket_type,
                                                const bool add_vector_socket_dimensions = false);

#endif

}  // namespace blender
