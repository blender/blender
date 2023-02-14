/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"

#include "BLI_task.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_topology_edges_of_corner_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>(N_("Corner Index"))
      .implicit_field(implicit_field_inputs::index)
      .description(
          N_("The corner to retrieve data from. Defaults to the corner from the context"));
  b.add_output<decl::Int>(N_("Next Edge Index"))
      .field_source_reference_all()
      .description(
          N_("The edge after the corner in the face, in the direction of increasing indices"));
  b.add_output<decl::Int>(N_("Previous Edge Index"))
      .field_source_reference_all()
      .description(
          N_("The edge before the corner in the face, in the direction of decreasing indices"));
}

static int get_loop_edge(const MLoop &loop)
{
  return loop.e;
}

class CornerNextEdgeFieldInput final : public bke::MeshFieldInput {
 public:
  CornerNextEdgeFieldInput() : bke::MeshFieldInput(CPPType::get<int>(), "Corner Next Edge")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 const IndexMask /*mask*/) const final
  {
    if (domain != ATTR_DOMAIN_CORNER) {
      return {};
    }
    return VArray<int>::ForDerivedSpan<MLoop, get_loop_edge>(mesh.loops());
  }

  uint64_t hash() const final
  {
    return 1892753404495;
  }

  bool is_equal_to(const fn::FieldNode &other) const final
  {
    if (dynamic_cast<const CornerNextEdgeFieldInput *>(&other)) {
      return true;
    }
    return false;
  }

  std::optional<eAttrDomain> preferred_domain(const Mesh & /*mesh*/) const final
  {
    return ATTR_DOMAIN_CORNER;
  }
};

class CornerPreviousEdgeFieldInput final : public bke::MeshFieldInput {
 public:
  CornerPreviousEdgeFieldInput() : bke::MeshFieldInput(CPPType::get<int>(), "Corner Previous Edge")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 const IndexMask /*mask*/) const final
  {
    if (domain != ATTR_DOMAIN_CORNER) {
      return {};
    }
    const Span<MPoly> polys = mesh.polys();
    const Span<MLoop> loops = mesh.loops();
    Array<int> loop_to_poly_map = bke::mesh_topology::build_loop_to_poly_map(polys, mesh.totloop);
    return VArray<int>::ForFunc(
        mesh.totloop,
        [polys, loops, loop_to_poly_map = std::move(loop_to_poly_map)](const int corner_i) {
          const int poly_i = loop_to_poly_map[corner_i];
          const MPoly &poly = polys[poly_i];
          const int corner_i_prev = bke::mesh_topology::poly_loop_prev(poly, corner_i);
          return loops[corner_i_prev].e;
        });
  }

  uint64_t hash() const final
  {
    return 987298345762465;
  }

  bool is_equal_to(const fn::FieldNode &other) const final
  {
    if (dynamic_cast<const CornerPreviousEdgeFieldInput *>(&other)) {
      return true;
    }
    return false;
  }

  std::optional<eAttrDomain> preferred_domain(const Mesh & /*mesh*/) const final
  {
    return ATTR_DOMAIN_CORNER;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const Field<int> corner_index = params.extract_input<Field<int>>("Corner Index");
  if (params.output_is_required("Next Edge Index")) {
    params.set_output("Next Edge Index",
                      Field<int>(std::make_shared<FieldAtIndexInput>(
                          corner_index,
                          Field<int>(std::make_shared<CornerNextEdgeFieldInput>()),
                          ATTR_DOMAIN_CORNER)));
  }
  if (params.output_is_required("Previous Edge Index")) {
    params.set_output("Previous Edge Index",
                      Field<int>(std::make_shared<FieldAtIndexInput>(
                          corner_index,
                          Field<int>(std::make_shared<CornerPreviousEdgeFieldInput>()),
                          ATTR_DOMAIN_CORNER)));
  }
}

}  // namespace blender::nodes::node_geo_mesh_topology_edges_of_corner_cc

void register_node_type_geo_mesh_topology_edges_of_corner()
{
  namespace file_ns = blender::nodes::node_geo_mesh_topology_edges_of_corner_cc;

  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_MESH_TOPOLOGY_EDGES_OF_CORNER, "Edges of Corner", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
