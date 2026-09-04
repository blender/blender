/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup nodes
 */

#include <fmt/format.h>

#include "BLI_listbase.hh"
#include "BLI_string.hh"

#include "NOD_compositor_nodes_srna.hh"
#include "NOD_scene_compositor_effect_inputs_srna.hh"
#include "NOD_socket.hh"

#include "DNA_node_types.h"

#include "BKE_compositor.hh"
#include "BKE_node_runtime.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

namespace blender::nodes {

static std::optional<std::string> rna_SceneCompositorEffectInputProperty_path(
    const PointerRNA *property_ptr)
{
  const char *identifier = RNA_struct_identifier(property_ptr->type);
  const SceneCompositorEffect *effect = bke::compositor::get_effect_from_property(*property_ptr);
  return fmt::format(
      "compositor_effects[\"{}\"].properties.inputs.{}", BLI_str_escape(effect->name), identifier);
}

static StructRNA *get_input_socket_struct_rna(const bNodeTree &node_group,
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
  RNA_def_struct_path_func_runtime(srna, rna_SceneCompositorEffectInputProperty_path);
  if (stype->make_scene_compositor_effect_input_srna) {
    stype->make_scene_compositor_effect_input_srna(node_group, *srna, socket, r_generated);
  }

  return srna;
}

static StructRNA *create_inputs_srna(const bNodeTree &node_group,
                                     GeneratedTreeSrnaData &r_generated)
{
  StructRNA *srna = RNA_def_struct_ptr(
      r_generated.generated_rna, "SceneCompositorEffectInterfaceInputs", RNA_PropertyGroup);

  node_group.ensure_interface_cache();
  for (const bNodeTreeInterfaceSocket *socket : node_group.interface_inputs()) {
    StructRNA *socket_srna = get_input_socket_struct_rna(node_group, *socket, r_generated);
    if (!socket_srna) {
      continue;
    }
    const StringRefNull identifier = r_generated.scope.allocator().copy_string(socket->identifier);
    PropertyRNA *property = RNA_def_pointer_runtime(
        srna, identifier.c_str(), socket_srna, socket->name().c_str(), "");
    RNA_def_property_override_flag(property, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  }

  return srna;
}

static StructRNA *create_panels_srna(const bNodeTree &node_group,
                                     GeneratedTreeSrnaData &r_generated)
{
  StructRNA *srna = RNA_def_struct_ptr(
      r_generated.generated_rna, "SceneCompositorEffectInterfacePanels", RNA_PropertyGroup);

  LinearAllocator<> &allocator = r_generated.scope.allocator();

  node_group.ensure_interface_cache();
  for (const bNodeTreeInterfaceItem *item : node_group.interface_items()) {
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

std::shared_ptr<GeneratedTreeSrnaData> create_scene_compositor_effect_inputs_srna(
    const bNodeTree &node_group)
{
  std::unique_ptr<GeneratedTreeSrnaData> generated = std::make_unique<GeneratedTreeSrnaData>();

  StructRNA *srna = RNA_def_struct_ptr(generated->generated_rna,
                                       "SceneCompositorEffectInterface",
                                       RNA_SceneCompositorEffectProperties);
  generated->properties_struct = srna;

  StructRNA *inputs_srna = create_inputs_srna(node_group, *generated);
  StructRNA *panels_srna = create_panels_srna(node_group, *generated);

  PropertyRNA *prop;
  prop = RNA_def_pointer_runtime(
      srna, "inputs", inputs_srna, "Inputs", "Settings for input sockets");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  prop = RNA_def_pointer_runtime(srna, "panels", panels_srna, "Panels", "Settings for panels");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  return generated;
}

}  // namespace blender::nodes
