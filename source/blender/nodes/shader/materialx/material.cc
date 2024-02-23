/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <MaterialXFormat/XmlIo.h>

#include "node_parser.h"

#include "DEG_depsgraph.hh"

#include "DNA_material_types.h"

#include "NOD_shader.h"

#include "material.h"

namespace blender::nodes::materialx {

class DefaultMaterialNodeParser : public NodeParser {
 public:
  using NodeParser::NodeParser;

  NodeItem compute() override
  {
    NodeItem surface = create_node(
        "standard_surface",
        NodeItem::Type::SurfaceShader,
        {{"base", val(1.0f)},
         {"base_color", val(MaterialX::Color3(material_->r, material_->g, material_->b))},
         {"diffuse_roughness", val(material_->roughness)},
         {"specular", val(material_->spec)},
         {"metalness", val(material_->metallic)}});

    NodeItem res = create_node(
        "surfacematerial", NodeItem::Type::Material, {{"surfaceshader", surface}});
    res.node->setName("Material_Default");
    return res;
  }

  NodeItem compute_error()
  {
    NodeItem surface = create_node("standard_surface",
                                   NodeItem::Type::SurfaceShader,
                                   {{"base_color", val(MaterialX::Color3(1.0f, 0.0f, 1.0f))}});
    NodeItem res = create_node(
        "surfacematerial", NodeItem::Type::Material, {{"surfaceshader", surface}});
    res.node->setName("Material_Error");
    return res;
  }
};

MaterialX::DocumentPtr export_to_materialx(Depsgraph *depsgraph,
                                           Material *material,
                                           ExportImageFunction export_image_fn)
{
  CLOG_INFO(LOG_MATERIALX_SHADER, 0, "Material: %s", material->id.name);

  MaterialX::DocumentPtr doc = MaterialX::createDocument();
  if (material->use_nodes) {
    material->nodetree->ensure_topology_cache();
    bNode *output_node = ntreeShaderOutputNode(material->nodetree, SHD_OUTPUT_ALL);
    if (output_node && output_node->typeinfo->materialx_fn) {
      NodeParserData data = {doc.get(),
                             depsgraph,
                             material,
                             NodeItem::Type::Material,
                             nullptr,
                             NodeItem(doc.get()),
                             export_image_fn};
      output_node->typeinfo->materialx_fn(&data, output_node, nullptr);
    }
    else {
      DefaultMaterialNodeParser(doc.get(),
                                depsgraph,
                                material,
                                nullptr,
                                nullptr,
                                NodeItem::Type::Material,
                                nullptr,
                                export_image_fn)
          .compute_error();
    }
  }
  else {
    DefaultMaterialNodeParser(doc.get(),
                              depsgraph,
                              material,
                              nullptr,
                              nullptr,
                              NodeItem::Type::Material,
                              nullptr,
                              export_image_fn)
        .compute();
  }

  CLOG_INFO(LOG_MATERIALX_SHADER,
            1,
            "Material: %s\n%s",
            material->id.name,
            MaterialX::writeToXmlString(doc).c_str());
  return doc;
}

}  // namespace blender::nodes::materialx
