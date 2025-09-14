/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <MaterialXFormat/XmlIo.h>

#include "node_parser.h"

#include "BKE_lib_id.hh"

#include "DEG_depsgraph.hh"

#include "DNA_material_types.h"

#include "NOD_shader.h"
#include "NOD_shader_nodes_inline.hh"

#include "material.h"

namespace blender::nodes::materialx {

class DefaultMaterialNodeParser : public NodeParser {
 public:
  using NodeParser::NodeParser;

  NodeItem compute() override
  {
    const Material *material = graph_.material;
    NodeItem surface = create_node(
        "open_pbr_surface",
        NodeItem::Type::SurfaceShader,
        {{"base_weight", val(1.0f)},
         {"base_color", val(MaterialX::Color3(material->r, material->g, material->b))},
         {"base_diffuse_roughness", val(material->roughness)},
         {"specular_weight", val(material->spec)},
         {"base_metalness", val(material->metallic)}});

    NodeItem res = create_node(
        "surfacematerial", NodeItem::Type::Material, {{"surfaceshader", surface}});
    return res;
  }

  NodeItem compute_error()
  {
    NodeItem surface = create_node("open_pbr_surface",
                                   NodeItem::Type::SurfaceShader,
                                   {{"base_color", val(MaterialX::Color3(1.0f, 0.0f, 1.0f))}});
    NodeItem res = create_node(
        "surfacematerial", NodeItem::Type::Material, {{"surfaceshader", surface}});
    return res;
  }
};

MaterialX::DocumentPtr export_to_materialx(Depsgraph *depsgraph,
                                           Material *material,
                                           const ExportParams &export_params)
{
  CLOG_DEBUG(LOG_IO_MATERIALX, "Material: %s", material->id.name);

  MaterialX::DocumentPtr doc = MaterialX::createDocument();
  NodeItem output_item;

  NodeGraph graph(depsgraph, material, export_params, doc);

  bNodeTree *local_tree = bke::node_tree_add_tree(
      nullptr, "Inlined Tree", material->nodetree->idname);
  BLI_SCOPED_DEFER([&]() { BKE_id_free(nullptr, &local_tree->id); });

  InlineShaderNodeTreeParams params;
  inline_shader_node_tree(*material->nodetree, *local_tree, params);

  local_tree->ensure_topology_cache();
  bNode *output_node = ntreeShaderOutputNode(local_tree, SHD_OUTPUT_ALL);
  if (output_node && output_node->typeinfo->materialx_fn) {
    NodeParserData data = {graph, NodeItem::Type::Material, nullptr, graph.empty_node()};
    output_node->typeinfo->materialx_fn(&data, output_node, nullptr);
    output_item = data.result;
  }
  else {
    output_item = DefaultMaterialNodeParser(
                      graph, nullptr, nullptr, NodeItem::Type::Material, nullptr)
                      .compute_error();
  }

  /* This node is expected to have a specific name to link up to USD. */
  graph.set_output_node_name(output_item);

  CLOG_DEBUG(LOG_IO_MATERIALX,
             "Material: %s\n%s",
             material->id.name,
             MaterialX::writeToXmlString(doc).c_str());
  return doc;
}

}  // namespace blender::nodes::materialx
