/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_mesh.hh"

#include "BLI_atomic_disjoint_set.hh"
#include "BLI_task.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_mesh_island_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>(N_("Island Index"))
      .field_source()
      .description(N_("The index of the each vertex's island. Indices are based on the "
                      "lowest vertex index contained in each island"));
  b.add_output<decl::Int>(N_("Island Count"))
      .field_source()
      .description(N_("The total number of mesh islands"));
}

class IslandFieldInput final : public bke::MeshFieldInput {
 public:
  IslandFieldInput() : bke::MeshFieldInput(CPPType::get<int>(), "Island Index")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 const IndexMask /*mask*/) const final
  {
    const Span<int2> edges = mesh.edges();

    AtomicDisjointSet islands(mesh.totvert);
    threading::parallel_for(edges.index_range(), 1024, [&](const IndexRange range) {
      for (const int2 &edge : edges.slice(range)) {
        islands.join(edge[0], edge[1]);
      }
    });

    Array<int> output(mesh.totvert);
    islands.calc_reduced_ids(output);

    return mesh.attributes().adapt_domain<int>(
        VArray<int>::ForContainer(std::move(output)), ATTR_DOMAIN_POINT, domain);
  }

  uint64_t hash() const override
  {
    /* Some random constant hash. */
    return 635467354;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const IslandFieldInput *>(&other) != nullptr;
  }

  std::optional<eAttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return ATTR_DOMAIN_POINT;
  }
};

class IslandCountFieldInput final : public bke::MeshFieldInput {
 public:
  IslandCountFieldInput() : bke::MeshFieldInput(CPPType::get<int>(), "Island Count")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const Mesh &mesh,
                                 const eAttrDomain domain,
                                 const IndexMask /*mask*/) const final
  {
    const Span<int2> edges = mesh.edges();

    AtomicDisjointSet islands(mesh.totvert);
    threading::parallel_for(edges.index_range(), 1024, [&](const IndexRange range) {
      for (const int2 &edge : edges.slice(range)) {
        islands.join(edge[0], edge[1]);
      }
    });

    const int islands_num = islands.count_sets();
    return VArray<int>::ForSingle(islands_num, mesh.attributes().domain_size(domain));
  }

  uint64_t hash() const override
  {
    /* Some random hash. */
    return 45634572457;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const IslandCountFieldInput *>(&other) != nullptr;
  }

  std::optional<eAttrDomain> preferred_domain(const Mesh & /*mesh*/) const override
  {
    return ATTR_DOMAIN_POINT;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  if (params.output_is_required("Island Index")) {
    Field<int> field{std::make_shared<IslandFieldInput>()};
    params.set_output("Island Index", std::move(field));
  }
  if (params.output_is_required("Island Count")) {
    Field<int> field{std::make_shared<IslandCountFieldInput>()};
    params.set_output("Island Count", std::move(field));
  }
}

}  // namespace blender::nodes::node_geo_input_mesh_island_cc

void register_node_type_geo_input_mesh_island()
{
  namespace file_ns = blender::nodes::node_geo_input_mesh_island_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_INPUT_MESH_ISLAND, "Mesh Island", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
