/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_mesh.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_mesh_edge_vertices_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>(N_("Edge Index"))
      .implicit_field(implicit_field_inputs::index)
      .description(N_("The edge to retrieve data from. Defaults to the edge from the context"));
  b.add_output<decl::Int>(N_("Vertex Index 1"))
      .dependent_field()
      .description(N_("The index of the first vertex in the edge"));
  b.add_output<decl::Int>(N_("Vertex Index 2"))
      .dependent_field()
      .description(N_("The index of the second vertex in the edge"));
  b.add_output<decl::Vector>(N_("Position 1"))
      .dependent_field()
      .description(N_("The position of the first vertex in the edge"));
  b.add_output<decl::Vector>(N_("Position 2"))
      .dependent_field()
      .description(N_("The position of the second vertex in the edge"));
}

enum class VertNumber { V1, V2 };

static int edge_get_v1(const MEdge &edge)
{
  return edge.v1;
}

static int edge_get_v2(const MEdge &edge)
{
  return edge.v2;
}

class EdgeVertsInput final : public bke::MeshFieldInput {
 private:
  VertNumber vertex_;

 public:
  EdgeVertsInput(VertNumber vertex)
      : bke::MeshFieldInput(CPPType::get<int>(), "Edge Vertices Field"), vertex_(vertex)
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 const IndexMask /*mask*/) const final
  {
    if (domain != ATTR_DOMAIN_EDGE) {
      return {};
    }
    switch (vertex_) {
      case VertNumber::V1:
        return VArray<int>::ForDerivedSpan<MEdge, edge_get_v1>(mesh.edges());
      case VertNumber::V2:
        return VArray<int>::ForDerivedSpan<MEdge, edge_get_v2>(mesh.edges());
    }
    BLI_assert_unreachable();
    return {};
  }

  uint64_t hash() const override
  {
    return vertex_ == VertNumber::V1 ? 23847562893465 : 92384598734567;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    if (const EdgeVertsInput *other_field = dynamic_cast<const EdgeVertsInput *>(&other)) {
      return vertex_ == other_field->vertex_;
    }
    return false;
  }

  std::optional<eAttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return ATTR_DOMAIN_EDGE;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const Field<int> edge_index = params.extract_input<Field<int>>("Edge Index");
  Field<int> vertex_field_1(std::make_shared<FieldAtIndexInput>(
      edge_index, Field<int>(std::make_shared<EdgeVertsInput>(VertNumber::V1)), ATTR_DOMAIN_EDGE));
  Field<int> vertex_field_2(std::make_shared<FieldAtIndexInput>(
      edge_index, Field<int>(std::make_shared<EdgeVertsInput>(VertNumber::V2)), ATTR_DOMAIN_EDGE));

  params.set_output("Vertex Index 1", vertex_field_1);
  params.set_output("Vertex Index 2", vertex_field_2);
  if (params.output_is_required("Position 1")) {
    params.set_output("Position 1",
                      Field<float3>(std::make_shared<FieldAtIndexInput>(
                          vertex_field_1,
                          Field<float3>(AttributeFieldInput::Create<float3>("position")),
                          ATTR_DOMAIN_POINT)));
  }
  if (params.output_is_required("Position 2")) {
    params.set_output("Position 2",
                      Field<float3>(std::make_shared<FieldAtIndexInput>(
                          vertex_field_2,
                          Field<float3>(AttributeFieldInput::Create<float3>("position")),
                          ATTR_DOMAIN_POINT)));
  }
}

}  // namespace blender::nodes::node_geo_input_mesh_edge_vertices_cc

void register_node_type_geo_input_mesh_edge_vertices()
{
  namespace file_ns = blender::nodes::node_geo_input_mesh_edge_vertices_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_INPUT_MESH_EDGE_VERTICES, "Edge Vertices", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
