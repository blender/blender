/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "DNA_mesh_types.h"

#include "BKE_grease_pencil.hh"

namespace blender::nodes::node_geo_material_replace_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry")
      .supported_type({GeometryComponent::Type::Mesh, GeometryComponent::Type::GreasePencil});
  b.add_input<decl::Material>("Old");
  b.add_input<decl::Material>("New").translation_context(BLT_I18NCONTEXT_ID_MATERIAL);
  b.add_output<decl::Geometry>("Geometry").propagate_all();
}

static void replace_materials(MutableSpan<Material *> materials,
                              Material *src_material,
                              Material *dst_material)
{
  for (const int i : materials.index_range()) {
    if (materials[i] == src_material) {
      materials[i] = dst_material;
    }
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Material *old_material = params.extract_input<Material *>("Old");
  Material *new_material = params.extract_input<Material *>("New");

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (Mesh *mesh = geometry_set.get_mesh_for_write()) {
      replace_materials({mesh->mat, mesh->totcol}, old_material, new_material);
    }
    if (GreasePencil *grease_pencil = geometry_set.get_grease_pencil_for_write()) {
      replace_materials({grease_pencil->material_array, grease_pencil->material_array_num},
                        old_material,
                        new_material);
    }
  });

  params.set_output("Geometry", std::move(geometry_set));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_REPLACE_MATERIAL, "Replace Material", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_material_replace_cc
