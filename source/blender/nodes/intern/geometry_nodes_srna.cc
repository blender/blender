/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "BLI_listbase.hh"
#include "BLI_string.hh"

#include "NOD_geometry_nodes_srna.hh"
#include "NOD_socket.hh"

#include "DNA_modifier_types.h"
#include "DNA_node_types.h"

#include "BKE_idprop.hh"
#include "BKE_node_runtime.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "PRF_profile.hh"

namespace blender::nodes {

static constexpr EnumPropertyItem input_type_item_fallback = {
    int(GeometryNodesInputType::Fallback), "FALLBACK", 0, "Fallback", "Fallback"};
static constexpr EnumPropertyItem input_type_item_value = {
    int(GeometryNodesInputType::Value), "VALUE", 0, "Value", "Pass a single value"};
static constexpr EnumPropertyItem input_type_item_attribute = {
    int(GeometryNodesInputType::Attribute), "ATTRIBUTE", 0, "Attribute", "Pass an attribute"};
static constexpr EnumPropertyItem input_type_item_layer = {
    int(GeometryNodesInputType::Layer), "LAYER", 0, "Layer", "Pass a layer selection"};

const EnumPropertyItem geometry_nodes_input_type_items_fallback[] = {
    input_type_item_fallback,
    {0},
};

const EnumPropertyItem geometry_nodes_input_type_items_value[] = {
    input_type_item_value,
    {0},
};

const EnumPropertyItem geometry_nodes_input_type_items_value_or_attribute[] = {
    input_type_item_value,
    input_type_item_attribute,
    {0},
};

const EnumPropertyItem geometry_nodes_input_type_items_value_or_attribute_or_layer[] = {
    input_type_item_value,
    input_type_item_attribute,
    input_type_item_layer,
    {0},
};

static const ModifierData *find_modifier_data_from_system_property(const PointerRNA *ptr)
{
  for (const AncestorPointerRNA &ancestor : ptr->ancestors) {
    if (RNA_struct_is_a(ancestor.type, RNA_Modifier)) {
      return static_cast<const ModifierData *>(ancestor.data);
    }
  }
  const Object *object = id_cast<const Object *>(ptr->owner_id);
  for (const ModifierData &md : object->modifiers) {
    bool found = false;
    IDP_foreach_property(md.system_properties, 0, [&](IDProperty *id_prop) {
      if (id_prop == ptr->data) {
        found = true;
      }
    });
    if (found) {
      return &md;
    }
  }
  return nullptr;
}

static std::optional<std::string> rna_NodesModifierPropertyInput_path(const PointerRNA *ptr)
{
  StructRNA *srna = ptr->type;
  const char *identifier = RNA_struct_identifier(srna);
  const ModifierData *md = find_modifier_data_from_system_property(ptr);
  std::string name_esc = BLI_str_escape(md->name);
  return fmt::format("modifiers[\"{}\"].properties.inputs.{}", name_esc, identifier);
}

static StructRNA *get_input_socket_struct_rna(const bNodeTree &tree,
                                              const bNodeTreeInterfaceSocket &socket,
                                              GeneratedTreeSrnaData &r_generated)
{
  const bke::bNodeSocketType *stype = socket.socket_typeinfo();
  if (!stype) {
    return nullptr;
  }
  // TODO: Does this actually need to copy the string?
  const StringRefNull srna_identifier = r_generated.scope.allocator().copy_string(
      socket.identifier);

  StructRNA *srna = RNA_def_struct_ptr(
      r_generated.generated_rna, srna_identifier.c_str(), RNA_PropertyGroup);
  RNA_def_struct_path_func_runtime(srna, rna_NodesModifierPropertyInput_path);
  if (stype->make_geometry_nodes_input_srna) {
    stype->make_geometry_nodes_input_srna(tree, *srna, socket, r_generated);
  }

  return srna;
}

static StructRNA *create_inputs_srna(const bNodeTree &tree, GeneratedTreeSrnaData &r_generated)
{
  StructRNA *srna = RNA_def_struct_ptr(
      r_generated.generated_rna, "GeometryNodesInterfaceInputs", RNA_PropertyGroup);

  for (const bNodeTreeInterfaceSocket *socket : tree.interface_inputs()) {
    StructRNA *socket_srna = get_input_socket_struct_rna(tree, *socket, r_generated);
    if (!socket_srna) {
      continue;
    }
    const StringRefNull identifier = r_generated.scope.allocator().copy_string(socket->identifier);
    PropertyRNA *prop = RNA_def_pointer_runtime(
        srna, identifier.c_str(), socket_srna, socket->name, "");
    RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  }

  return srna;
}

static std::optional<std::string> rna_NodesModifierPropertyOutput_path(const PointerRNA *ptr)
{
  StructRNA *srna = ptr->type;
  const char *identifier = RNA_struct_identifier(srna);
  const ModifierData *md = find_modifier_data_from_system_property(ptr);
  std::string name_esc = BLI_str_escape(md->name);
  return fmt::format("modifiers[\"{}\"].properties.outputs.{}", name_esc, identifier);
}

static StructRNA *create_outputs_srna(const bNodeTree &tree, GeneratedTreeSrnaData &r_generated)
{
  StructRNA *srna = RNA_def_struct_ptr(
      r_generated.generated_rna, "GeometryNodesInterfaceOutputs", RNA_PropertyGroup);

  LinearAllocator<> &allocator = r_generated.scope.allocator();

  for (const bNodeTreeInterfaceSocket *output : tree.interface_outputs()) {
    const bke::bNodeSocketType *socket_type = bke::node_socket_type_find(output->socket_type);
    if (!nodes::socket_type_supports_attributes(socket_type->type)) {
      continue;
    }

    const StringRefNull identifier = allocator.copy_string(output->identifier);
    const StringRefNull name = allocator.copy_string(output->name);
    const StringRefNull description = allocator.copy_string(output->description);
    const StringRefNull default_value = allocator.copy_string(output->default_attribute_name);

    StructRNA *output_srna = RNA_def_struct_ptr(
        r_generated.generated_rna, identifier.c_str(), RNA_PropertyGroup);
    RNA_def_struct_path_func_runtime(output_srna, rna_NodesModifierPropertyOutput_path);
    PropertyRNA *prop = RNA_def_string(output_srna,
                                       "attribute_name",
                                       default_value.is_empty() ? nullptr : default_value.c_str(),
                                       0,
                                       name.c_str(),
                                       description.c_str());
    RNA_def_property_flag(prop, PROP_FORCE_GEOMETRY_EVAL);
    RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
    prop = RNA_def_pointer_runtime(srna, identifier.c_str(), output_srna, name.c_str(), "");
    RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  }

  return srna;
}

static StructRNA *create_panels_srna(const bNodeTree &tree, GeneratedTreeSrnaData &r_generated)
{
  StructRNA *srna = RNA_def_struct_ptr(
      r_generated.generated_rna, "GeometryNodesInterfacePanels", RNA_PropertyGroup);

  LinearAllocator<> &allocator = r_generated.scope.allocator();

  tree.ensure_interface_cache();
  for (const bNodeTreeInterfaceItem *item : tree.interface_items()) {
    if (item->item_type != NodeTreeInterfaceItemType::Panel) {
      continue;
    }
    const auto &panel = *reinterpret_cast<const bNodeTreeInterfacePanel *>(item);
    const StringRefNull identifier = allocator.copy_string(
        fmt::format("open_{}", panel.identifier));
    PropertyRNA *prop = RNA_def_boolean(srna,
                                        identifier.c_str(),
                                        !(panel.flag & NODE_INTERFACE_PANEL_DEFAULT_CLOSED),
                                        "Is Open",
                                        "");
    RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  }

  return srna;
}

std::shared_ptr<GeneratedTreeSrnaData> create_geometry_nodes_rna_for_modifier(
    const bNodeTree &tree)
{
  PRF_scope(ProfileCategory::Core);
  auto generated = std::make_unique<GeneratedTreeSrnaData>();
  tree.ensure_interface_cache();

  StructRNA *srna = RNA_def_struct_ptr(
      generated->generated_rna, "GeometryNodesModifierInterface", RNA_NodesModifierProperties);
  generated->properties_struct = srna;

  StructRNA *inputs_srna = create_inputs_srna(tree, *generated);
  StructRNA *outputs_srna = create_outputs_srna(tree, *generated);
  StructRNA *panels_srna = create_panels_srna(tree, *generated);

  PropertyRNA *prop;
  prop = RNA_def_pointer_runtime(
      srna, "inputs", inputs_srna, "Inputs", "Settings for input sockets");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  prop = RNA_def_pointer_runtime(
      srna, "outputs", outputs_srna, "Outputs", "Settings for output sockets");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  prop = RNA_def_pointer_runtime(srna, "panels", panels_srna, "Panels", "Settings for panels");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  return generated;
}

}  // namespace blender::nodes
