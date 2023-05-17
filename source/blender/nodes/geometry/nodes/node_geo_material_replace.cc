/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_material.h"

namespace blender::nodes::node_geo_material_replace_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry").supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Material>("Old");
  b.add_input<decl::Material>("New").translation_context(BLT_I18NCONTEXT_ID_MATERIAL);
  b.add_output<decl::Geometry>("Geometry").propagate_all();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Material *old_material = params.extract_input<Material *>("Old");
  Material *new_material = params.extract_input<Material *>("New");

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (Mesh *mesh = geometry_set.get_mesh_for_write()) {
      for (const int i : IndexRange(mesh->totcol)) {
        if (mesh->mat[i] == old_material) {
          mesh->mat[i] = new_material;
        }
      }
    }
  });

  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_material_replace_cc

void register_node_type_geo_material_replace()
{
  namespace file_ns = blender::nodes::node_geo_material_replace_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_REPLACE_MATERIAL, "Replace Material", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
