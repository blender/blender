/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "NOD_compositor_nodes_srna.hh"
#include "NOD_socket.hh"

#include "DNA_node_types.h"
#include "DNA_sequence_types.h"

#include "BKE_idprop.hh"
#include "BKE_node_runtime.hh"

#include "SEQ_iterator.hh"
#include "SEQ_sequencer.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

namespace blender::nodes {

static constexpr EnumPropertyItem input_type_item_fallback = {
    int(CompositorNodesInputType::Fallback), "FALLBACK", 0, "Fallback", "Fallback"};
static constexpr EnumPropertyItem input_type_item_value = {
    int(CompositorNodesInputType::Value), "VALUE", 0, "Value", "Pass a single value"};

const EnumPropertyItem compositor_nodes_input_type_items_fallback[] = {
    input_type_item_fallback,
    {0},
};

const EnumPropertyItem compositor_nodes_input_type_items_value[] = {
    input_type_item_value,
    {0},
};

static std::pair<const Strip *, const StripModifierData *>
find_strip_and_modifier_data_from_system_property(const PointerRNA *ptr)
{
  if (const auto modifier = RNA_struct_search_closest_ancestor_by_type(ptr, RNA_StripModifier)) {
    if (const auto strip = RNA_struct_search_closest_ancestor_by_type(ptr, RNA_Strip)) {
      return {static_cast<const Strip *>(strip->data),
              static_cast<const StripModifierData *>(modifier->data)};
    }
  }
  const Scene *sequencer_scene = id_cast<const Scene *>(ptr->owner_id);
  const Editing *ed = seq::editing_get(sequencer_scene);
  BLI_assert(ed);
  for (Strip *strip : seq::query_all_strips_recursive(&ed->seqbase)) {
    for (StripModifierData &md : strip->modifiers) {
      bool found = false;
      IDP_foreach_property(md.system_properties, 0, [&](IDProperty *id_prop) {
        if (id_prop == ptr->data) {
          found = true;
        }
      });
      if (found) {
        return {strip, &md};
      }
    }
  }
  return {};
}

static std::optional<std::string> rna_CompositorNodesModifierProperty_path(
    const PointerRNA *ptr, const StringRef properties_path)
{
  StructRNA *srna = ptr->type;
  const char *identifier = RNA_struct_identifier(srna);
  const auto [strip, smd] = find_strip_and_modifier_data_from_system_property(ptr);
  BLI_assert(strip && smd);
  std::string strip_name_esc = BLI_str_escape(strip->name + 2);
  std::string modifier_name_esc = BLI_str_escape(smd->name);
  return fmt::format("sequence_editor.strips_all[\"{}\"].modifiers[\"{}\"].properties.{}.{}",
                     strip_name_esc,
                     modifier_name_esc,
                     properties_path,
                     identifier);
}

static std::optional<std::string> rna_CompositorNodesModifierPropertyInput_path(
    const PointerRNA *ptr)
{
  return rna_CompositorNodesModifierProperty_path(ptr, "inputs");
}

static StructRNA *get_input_socket_struct_rna(const bNodeTree &tree,
                                              const bNodeTreeInterfaceSocket &socket,
                                              GeneratedTreeSrnaData &r_generated)
{
  const bke::bNodeSocketType *stype = socket.socket_typeinfo();
  if (!stype) {
    return nullptr;
  }
  const StringRefNull srna_identifier = r_generated.scope.allocator().copy_string(
      socket.identifier);

  StructRNA *srna = RNA_def_struct_ptr(
      r_generated.generated_rna, srna_identifier.c_str(), RNA_PropertyGroup);
  RNA_def_struct_path_func_runtime(srna, rna_CompositorNodesModifierPropertyInput_path);
  if (stype->make_compositor_nodes_input_srna) {
    stype->make_compositor_nodes_input_srna(tree, *srna, socket, r_generated);
  }

  return srna;
}

static StructRNA *create_inputs_srna(const bNodeTree &tree, GeneratedTreeSrnaData &r_generated)
{
  StructRNA *srna = RNA_def_struct_ptr(
      r_generated.generated_rna, "CompositorNodesInterfaceInputs", RNA_PropertyGroup);

  for (const bNodeTreeInterfaceSocket *socket : tree.interface_inputs()) {
    StructRNA *socket_srna = get_input_socket_struct_rna(tree, *socket, r_generated);
    if (!socket_srna) {
      continue;
    }
    const StringRefNull identifier = r_generated.scope.allocator().copy_string(socket->identifier);
    RNA_def_pointer_runtime(srna, identifier.c_str(), socket_srna, socket->name, "");
  }

  return srna;
}

static StructRNA *create_panels_srna(const bNodeTree &tree, GeneratedTreeSrnaData &r_generated)
{
  StructRNA *srna = RNA_def_struct_ptr(
      r_generated.generated_rna, "CompositorNodesInterfacePanels", RNA_PropertyGroup);

  LinearAllocator<> &allocator = r_generated.scope.allocator();

  tree.ensure_interface_cache();
  for (const bNodeTreeInterfaceItem *item : tree.interface_items()) {
    if (item->item_type != NodeTreeInterfaceItemType::Panel) {
      continue;
    }
    const auto &panel = *reinterpret_cast<const bNodeTreeInterfacePanel *>(item);
    const StringRefNull identifier = allocator.copy_string(
        fmt::format("open_{}", panel.identifier));
    RNA_def_boolean(srna,
                    identifier.c_str(),
                    !(panel.flag & NODE_INTERFACE_PANEL_DEFAULT_CLOSED),
                    "Is Open",
                    "");
  }

  return srna;
}

std::shared_ptr<GeneratedTreeSrnaData> create_compositor_nodes_rna_for_strip_modifier(
    const bNodeTree &tree)
{
  auto generated = std::make_unique<GeneratedTreeSrnaData>();
  tree.ensure_interface_cache();

  StructRNA *srna = RNA_def_struct_ptr(generated->generated_rna,
                                       "CompositorNodesModifierInterface",
                                       RNA_SequencerCompositorModifierProperties);
  generated->properties_struct = srna;

  StructRNA *inputs_srna = create_inputs_srna(tree, *generated);
  /* Note: We don't generate any srna for the outputs because they are unused by the compositor. */
  StructRNA *panels_srna = create_panels_srna(tree, *generated);

  PropertyRNA *prop;
  prop = RNA_def_pointer_runtime(
      srna, "inputs", inputs_srna, "Inputs", "Settings for input sockets");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  prop = RNA_def_pointer_runtime(srna, "panels", panels_srna, "Panels", "Settings for panels");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  return generated;
}

}  // namespace blender::nodes
