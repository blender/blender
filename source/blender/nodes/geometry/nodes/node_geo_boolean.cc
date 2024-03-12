/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_geometry_set_instances.hh"
#include "BKE_instances.hh"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "NOD_rna_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GEO_mesh_boolean_convert.hh"
#include "GEO_randomize.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_boolean_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Mesh 1").only_realized_data().supported_type(
      GeometryComponent::Type::Mesh);
  b.add_input<decl::Geometry>("Mesh 2")
      .supported_type(GeometryComponent::Type::Mesh)
      .multi_input();
  b.add_input<decl::Bool>("Self Intersection");
  b.add_input<decl::Bool>("Hole Tolerant");
  b.add_output<decl::Geometry>("Mesh").propagate_all();
  b.add_output<decl::Bool>("Intersecting Edges").field_on_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "operation", UI_ITEM_NONE, "", ICON_NONE);
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
      node_sock_label(geometry_2_socket, "Mesh");
      break;
    case GEO_NODE_BOOLEAN_DIFFERENCE:
      bke::nodeSetSocketAvailability(ntree, geometry_1_socket, true);
      node_sock_label(geometry_2_socket, "Mesh 2");
      break;
  }
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = GEO_NODE_BOOLEAN_DIFFERENCE;
}

#ifdef WITH_GMP
static Array<short> calc_mesh_material_map(const Mesh &mesh, VectorSet<Material *> &all_materials)
{
  Array<short> map(mesh.totcol);
  for (const int i : IndexRange(mesh.totcol)) {
    Material *material = mesh.mat[i];
    map[i] = material ? all_materials.index_of_or_add(material) : -1;
  }
  return map;
}
#endif /* WITH_GMP */

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_GMP
  GeometryNodeBooleanOperation operation = (GeometryNodeBooleanOperation)params.node().custom1;
  const bool use_self = params.get_input<bool>("Self Intersection");
  const bool hole_tolerant = params.get_input<bool>("Hole Tolerant");

  Vector<const Mesh *> meshes;
  Vector<float4x4> transforms;
  VectorSet<Material *> materials;
  Vector<Array<short>> material_remaps;

  GeometrySet set_a;
  if (operation == GEO_NODE_BOOLEAN_DIFFERENCE) {
    set_a = params.extract_input<GeometrySet>("Mesh 1");
    /* Note that it technically wouldn't be necessary to realize the instances for the first
     * geometry input, but the boolean code expects the first shape for the difference operation
     * to be a single mesh. */
    if (const Mesh *mesh_in_a = set_a.get_mesh()) {
      meshes.append(mesh_in_a);
      transforms.append(float4x4::identity());
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

  Vector<GeometrySet> geometry_sets = params.extract_input<Vector<GeometrySet>>("Mesh 2");

  for (const GeometrySet &geometry : geometry_sets) {
    if (const Mesh *mesh = geometry.get_mesh()) {
      meshes.append(mesh);
      transforms.append(float4x4::identity());
      material_remaps.append(calc_mesh_material_map(*mesh, materials));
    }
    if (const bke::Instances *instances = geometry.get_instances()) {
      const Span<bke::InstanceReference> references = instances->references();
      const Span<int> handles = instances->reference_handles();
      const Span<float4x4> instance_transforms = instances->transforms();
      for (const int i : handles.index_range()) {
        const bke::InstanceReference &reference = references[handles[i]];
        switch (reference.type()) {
          case bke::InstanceReference::Type::Object: {
            const GeometrySet object_geometry = bke::object_get_evaluated_geometry_set(
                reference.object());
            if (const Mesh *mesh = object_geometry.get_mesh()) {
              meshes.append(mesh);
              transforms.append(instance_transforms[i]);
              material_remaps.append(calc_mesh_material_map(*mesh, materials));
            }
            break;
          }
          case bke::InstanceReference::Type::GeometrySet: {
            if (const Mesh *mesh = reference.geometry_set().get_mesh()) {
              meshes.append(mesh);
              transforms.append(instance_transforms[i]);
              material_remaps.append(calc_mesh_material_map(*mesh, materials));
            }
            break;
          }
          case bke::InstanceReference::Type::None:
          case bke::InstanceReference::Type::Collection:
            break;
        }
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
        attribute_outputs.intersecting_edges_id.get(), AttrDomain::Edge);

    selection.span.fill(false);
    for (const int i : intersecting_edges) {
      selection.span[i] = true;
    }
    selection.finish();
  }

  geometry::debug_randomize_mesh_order(result);

  params.set_output("Mesh", GeometrySet::from_mesh(result));
#else
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without GMP"));
  params.set_default_remaining_outputs();
#endif
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem rna_node_geometry_boolean_method_items[] = {
      {GEO_NODE_BOOLEAN_INTERSECT,
       "INTERSECT",
       0,
       "Intersect",
       "Keep the part of the mesh that is common between all operands"},
      {GEO_NODE_BOOLEAN_UNION, "UNION", 0, "Union", "Combine meshes in an additive way"},
      {GEO_NODE_BOOLEAN_DIFFERENCE,
       "DIFFERENCE",
       0,
       "Difference",
       "Combine meshes in a subtractive way"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "operation",
                    "Operation",
                    "",
                    rna_node_geometry_boolean_method_items,
                    NOD_inline_enum_accessors(custom1),
                    GEO_NODE_BOOLEAN_INTERSECT);
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_BOOLEAN, "Mesh Boolean", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.updatefunc = node_update;
  ntype.initfunc = node_init;
  ntype.geometry_node_execute = node_geo_exec;
  nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_boolean_cc
