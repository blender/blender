/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_mesh.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_face_group_boundaries_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Face Group ID", "Face Set")
      .default_value(0)
      .hide_value()
      .supports_field()
      .description(
          "An identifier for the group of each face. All contiguous faces with the "
          "same value are in the same region");
  b.add_output<decl::Bool>("Boundary Edges")
      .field_source_reference_all()
      .description("The edges that lie on the boundaries between the different face groups");
}

class BoundaryFieldInput final : public bke::MeshFieldInput {
 private:
  const Field<int> face_set_;

 public:
  BoundaryFieldInput(const Field<int> face_set)
      : bke::MeshFieldInput(CPPType::get<bool>(), "Face Group Boundaries"), face_set_(face_set)
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    const bke::MeshFieldContext face_context{mesh, ATTR_DOMAIN_FACE};
    FieldEvaluator face_evaluator{face_context, mesh.faces_num};
    face_evaluator.add(face_set_);
    face_evaluator.evaluate();
    const VArray<int> face_set = face_evaluator.get_evaluated<int>(0);

    Array<bool> boundary(mesh.totedge, false);
    Array<bool> edge_visited(mesh.totedge, false);
    Array<int> edge_face_set(mesh.totedge, 0);
    const OffsetIndices faces = mesh.faces();
    const Span<int> corner_edges = mesh.corner_edges();
    for (const int i : faces.index_range()) {
      for (const int edge : corner_edges.slice(faces[i])) {
        if (edge_visited[edge]) {
          if (edge_face_set[edge] != face_set[i]) {
            /* This edge is connected to two faces on different face sets. */
            boundary[edge] = true;
          }
        }
        edge_visited[edge] = true;
        edge_face_set[edge] = face_set[i];
      }
    }
    return mesh.attributes().adapt_domain<bool>(
        VArray<bool>::ForContainer(std::move(boundary)), ATTR_DOMAIN_EDGE, domain);
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override
  {
    face_set_.node().for_each_field_input_recursive(fn);
  }

  std::optional<eAttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return ATTR_DOMAIN_EDGE;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const Field<int> face_set_field = params.extract_input<Field<int>>("Face Set");
  Field<bool> face_set_boundaries{std::make_shared<BoundaryFieldInput>(face_set_field)};
  params.set_output("Boundary Edges", std::move(face_set_boundaries));
}

static void node_register()
{
  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_MESH_FACE_GROUP_BOUNDARIES, "Face Group Boundaries", NODE_CLASS_INPUT);
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::MIDDLE);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_mesh_face_group_boundaries_cc
