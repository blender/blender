/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "BKE_geometry_set_instances.hh"
#include "BKE_mesh_boolean_convert.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_boolean_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Mesh 1"))
      .only_realized_data()
      .supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Geometry>(N_("Mesh 2")).multi_input().supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Bool>(N_("Self Intersection"));
  b.add_input<decl::Bool>(N_("Hole Tolerant"));
  b.add_output<decl::Geometry>(N_("Mesh")).propagate_all();
  b.add_output<decl::Bool>(N_("Intersecting Edges")).field_on_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "operation", 0, "", ICON_NONE);
}

struct AttributeOutputs {
  AnonymousAttributeIDPtr intersecting_edges_id;
};

static void node_update(bNodeTree *ntree, bNode *node)
{
  GeometryNodeBooleanOperation operation = (GeometryNodeBooleanOperation)node->custom1;

  bNodeSocket *geometry_1_socket = static_cast<bNodeSocket *>(node->inputs.first);
  bNodeSocket *geometry_2_socket = geometry_1_socket->next;

  switch (operation) {
    case GEO_NODE_BOOLEAN_INTERSECT:
    case GEO_NODE_BOOLEAN_UNION:
      bke::nodeSetSocketAvailability(ntree, geometry_1_socket, false);
      bke::nodeSetSocketAvailability(ntree, geometry_2_socket, true);
      node_sock_label(geometry_2_socket, N_("Mesh"));
      break;
    case GEO_NODE_BOOLEAN_DIFFERENCE:
      bke::nodeSetSocketAvailability(ntree, geometry_1_socket, true);
      bke::nodeSetSocketAvailability(ntree, geometry_2_socket, true);
      node_sock_label(geometry_2_socket, N_("Mesh 2"));
      break;
  }
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = GEO_NODE_BOOLEAN_DIFFERENCE;
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_GMP
  GeometryNodeBooleanOperation operation = (GeometryNodeBooleanOperation)params.node().custom1;
  const bool use_self = params.get_input<bool>("Self Intersection");
  const bool hole_tolerant = params.get_input<bool>("Hole Tolerant");

  Vector<const Mesh *> meshes;
  Vector<const float4x4 *> transforms;

  VectorSet<Material *> materials;
  Vector<Array<short>> material_remaps;

  GeometrySet set_a;
  if (operation == GEO_NODE_BOOLEAN_DIFFERENCE) {
    set_a = params.extract_input<GeometrySet>("Mesh 1");
    /* Note that it technically wouldn't be necessary to realize the instances for the first
     * geometry input, but the boolean code expects the first shape for the difference operation
     * to be a single mesh. */
    const Mesh *mesh_in_a = set_a.get_mesh_for_read();
    if (mesh_in_a != nullptr) {
      meshes.append(mesh_in_a);
      transforms.append(nullptr);
      if (mesh_in_a->totcol == 0) {
        /* Necessary for faces using the default material when there are no material slots. */
        materials.add(nullptr);
      }
      else {
        materials.add_multiple({mesh_in_a->mat, mesh_in_a->totcol});
      }
      material_remaps.append({});
    }
  }

  /* The instance transform matrices are owned by the instance group, so we have to
   * keep all of them around for use during the boolean operation. */
  Vector<bke::GeometryInstanceGroup> set_groups;
  Vector<GeometrySet> geometry_sets = params.extract_input<Vector<GeometrySet>>("Mesh 2");
  for (const GeometrySet &geometry_set : geometry_sets) {
    bke::geometry_set_gather_instances(geometry_set, set_groups);
  }

  for (const bke::GeometryInstanceGroup &set_group : set_groups) {
    const Mesh *mesh = set_group.geometry_set.get_mesh_for_read();
    if (mesh != nullptr) {
      Array<short> map(mesh->totcol);
      for (const int i : IndexRange(mesh->totcol)) {
        Material *material = mesh->mat[i];
        map[i] = material ? materials.index_of_or_add(material) : -1;
      }
      material_remaps.append(std::move(map));
    }
  }

  for (const bke::GeometryInstanceGroup &set_group : set_groups) {
    const Mesh *mesh_in = set_group.geometry_set.get_mesh_for_read();
    if (mesh_in != nullptr) {
      meshes.append_n_times(mesh_in, set_group.transforms.size());
      for (const int i : set_group.transforms.index_range()) {
        transforms.append(set_group.transforms.begin() + i);
      }
    }
  }

  AttributeOutputs attribute_outputs;
  attribute_outputs.intersecting_edges_id = params.get_output_anonymous_attribute_id_if_needed(
      "Intersecting Edges");

  Vector<int> intersecting_edges;
  Mesh *result = blender::meshintersect::direct_mesh_boolean(
      meshes,
      transforms,
      float4x4::identity(),
      material_remaps,
      use_self,
      hole_tolerant,
      operation,
      attribute_outputs.intersecting_edges_id ? &intersecting_edges : nullptr);
  if (!result) {
    params.set_default_remaining_outputs();
    return;
  }

  MEM_SAFE_FREE(result->mat);
  result->mat = static_cast<Material **>(
      MEM_malloc_arrayN(materials.size(), sizeof(Material *), __func__));
  result->totcol = materials.size();
  MutableSpan(result->mat, result->totcol).copy_from(materials);

  /* Store intersecting edges in attribute. */
  if (attribute_outputs.intersecting_edges_id) {
    MutableAttributeAccessor attributes = result->attributes_for_write();
    SpanAttributeWriter<bool> selection = attributes.lookup_or_add_for_write_only_span<bool>(
        attribute_outputs.intersecting_edges_id.get(), ATTR_DOMAIN_EDGE);

    selection.span.fill(false);
    for (const int i : intersecting_edges) {
      selection.span[i] = true;
    }
    selection.finish();
  }

  params.set_output("Mesh", GeometrySet::create_with_mesh(result));
#else
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without GMP"));
  params.set_default_remaining_outputs();
#endif
}

}  // namespace blender::nodes::node_geo_boolean_cc

void register_node_type_geo_boolean()
{
  namespace file_ns = blender::nodes::node_geo_boolean_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_BOOLEAN, "Mesh Boolean", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.updatefunc = file_ns::node_update;
  ntype.initfunc = file_ns::node_init;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
