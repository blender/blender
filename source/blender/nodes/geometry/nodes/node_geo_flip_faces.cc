/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.h"

#include "BKE_attribute_math.hh"

#include "GEO_mesh_flip_faces.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_flip_faces_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Mesh").supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_output<decl::Geometry>("Mesh").propagate_all();
}

static void mesh_flip_faces(Mesh &mesh, const Field<bool> &selection_field)
{
  if (mesh.totpoly == 0) {
    return;
  }
  const bke::MeshFieldContext field_context{mesh, ATTR_DOMAIN_FACE};
  fn::FieldEvaluator evaluator{field_context, mesh.totpoly};
  evaluator.add(selection_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_as_mask(0);

  geometry::flip_faces(mesh, selection);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");

  const Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (Mesh *mesh = geometry_set.get_mesh_for_write()) {
      mesh_flip_faces(*mesh, selection_field);
    }
  });

  params.set_output("Mesh", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_flip_faces_cc

void register_node_type_geo_flip_faces()
{
  namespace file_ns = blender::nodes::node_geo_flip_faces_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_FLIP_FACES, "Flip Faces", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
