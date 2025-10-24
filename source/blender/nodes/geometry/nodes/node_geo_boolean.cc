/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_geometry_set_instances.hh"
#include "BKE_instances.hh"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "NOD_rna_define.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "GEO_join_geometries.hh"
#include "GEO_mesh_boolean.hh"
#include "GEO_randomize.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_boolean_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();

  auto &first_geometry = b.add_input<decl::Geometry>("Mesh 1")
                             .only_realized_data()
                             .supported_type(GeometryComponent::Type::Mesh)
                             .description("Base mesh to subtract geometry from");

  if (node != nullptr) {
    switch (geometry::boolean::Operation(node->custom1)) {
      case geometry::boolean::Operation::Intersect:
      case geometry::boolean::Operation::Union:
        b.add_input<decl::Geometry>("Mesh", "Mesh 2")
            .supported_type(GeometryComponent::Type::Mesh)
            .multi_input()
            .description("Meshes to union or intersect");
        break;
      case geometry::boolean::Operation::Difference:
        b.add_input<decl::Geometry>("Mesh 2")
            .supported_type(GeometryComponent::Type::Mesh)
            .multi_input()
            .description("Mesh that is subtracted from the first mesh");
        break;
    }
  }

  auto make_mesh_arr = [](bNode &node) {
    node.custom2 = int16_t(geometry::boolean::Solver::MeshArr);
  };
  auto &self_intersect =
      b.add_input<decl::Bool>("Self Intersection").make_available(make_mesh_arr);
  auto &hole_tolerant = b.add_input<decl::Bool>("Hole Tolerant").make_available(make_mesh_arr);
  b.add_output<decl::Geometry>("Mesh").propagate_all();
  auto &output_edges =
      b.add_output<decl::Bool>("Intersecting Edges").field_on_all().make_available(make_mesh_arr);

  if (node != nullptr) {
    const auto operation = geometry::boolean::Operation(node->custom1);
    const auto solver = geometry::boolean::Solver(node->custom2);

    output_edges.available(
        ELEM(solver, geometry::boolean::Solver::MeshArr, geometry::boolean::Solver::Manifold));
    self_intersect.available(solver == geometry::boolean::Solver::MeshArr);
    hole_tolerant.available(solver == geometry::boolean::Solver::MeshArr);

    switch (operation) {
      case geometry::boolean::Operation::Intersect:
      case geometry::boolean::Operation::Union:
        first_geometry.available(false);
        break;
      case geometry::boolean::Operation::Difference:
        break;
    }
  }
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "operation", UI_ITEM_NONE, "", ICON_NONE);
  layout->prop(ptr, "solver", UI_ITEM_NONE, "", ICON_NONE);
}

struct AttributeOutputs {
  std::optional<std::string> intersecting_edges_id;
};

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = int16_t(geometry::boolean::Operation::Difference);
  node->custom2 = int16_t(geometry::boolean::Solver::Float);
}

static Array<short> calc_mesh_material_map(const Mesh &mesh, VectorSet<Material *> &all_materials)
{
  Array<short> map(mesh.totcol);
  for (const int i : IndexRange(mesh.totcol)) {
    Material *material = mesh.mat[i];
    map[i] = material ? all_materials.index_of_or_add(material) : -1;
  }
  return map;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  geometry::boolean::Operation operation = geometry::boolean::Operation(params.node().custom1);
  geometry::boolean::Solver solver = geometry::boolean::Solver(params.node().custom2);
  bool use_self = false;
  bool hole_tolerant = false;
  if (solver == geometry::boolean::Solver::MeshArr) {
    use_self = params.extract_input<bool>("Self Intersection");
    hole_tolerant = params.extract_input<bool>("Hole Tolerant");
  }

  Vector<const Mesh *> meshes;
  Vector<float4x4> transforms;
  VectorSet<Material *> materials;
  Vector<Array<short>> material_remaps;

  GeometrySet set_a;
  if (operation == geometry::boolean::Operation::Difference) {
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

  GeoNodesMultiInput<GeometrySet> geometry_sets =
      params.extract_input<GeoNodesMultiInput<GeometrySet>>("Mesh 2");

  for (const GeometrySet &geometry : geometry_sets.values) {
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

  if (ELEM(solver, geometry::boolean::Solver::MeshArr, geometry::boolean::Solver::Manifold)) {
    /* These solvers remap materials using realize_instances. */
    material_remaps.resize(0);
  }

  AttributeOutputs attribute_outputs;
  if (ELEM(solver, geometry::boolean::Solver::MeshArr, geometry::boolean::Solver::Manifold)) {
    attribute_outputs.intersecting_edges_id = params.get_output_anonymous_attribute_id_if_needed(
        "Intersecting Edges");
  }

  Vector<int> intersecting_edges;
  geometry::boolean::BooleanOpParameters op_params;
  op_params.boolean_mode = operation;
  op_params.no_self_intersections = !use_self;
  op_params.watertight = !hole_tolerant;
  op_params.no_nested_components = true; /* TODO: make this configurable. */
  geometry::boolean::BooleanError error = geometry::boolean::BooleanError::NoError;
  Mesh *result = geometry::boolean::mesh_boolean(
      meshes,
      transforms,
      material_remaps,
      op_params,
      solver,
      attribute_outputs.intersecting_edges_id ? &intersecting_edges : nullptr,
      &error);
  if (error == geometry::boolean::BooleanError::NonManifold) {
    params.error_message_add(NodeWarningType::Error, TIP_("An input was not manifold"));
  }
  else if (error == geometry::boolean::BooleanError::ResultTooBig) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("Boolean result is too big for solver to handle"));
  }
  else if (error == geometry::boolean::BooleanError::SolverNotAvailable) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("Boolean solver not available (compiled without it)"));
  }
  else if (error == geometry::boolean::BooleanError::UnknownError) {
    params.error_message_add(NodeWarningType::Error, TIP_("Unknown Boolean error"));
  }
  if (!result) {
    params.set_default_remaining_outputs();
    return;
  }

  MEM_SAFE_FREE(result->mat);
  result->mat = MEM_malloc_arrayN<Material *>(size_t(materials.size()), __func__);
  result->totcol = materials.size();
  MutableSpan(result->mat, result->totcol).copy_from(materials);

  /* Store intersecting edges in attribute. */
  if (attribute_outputs.intersecting_edges_id) {
    MutableAttributeAccessor attributes = result->attributes_for_write();
    SpanAttributeWriter<bool> selection = attributes.lookup_or_add_for_write_only_span<bool>(
        *attribute_outputs.intersecting_edges_id, AttrDomain::Edge);

    selection.span.fill(false);
    for (const int i : intersecting_edges) {
      selection.span[i] = true;
    }
    selection.finish();
  }
  geometry::debug_randomize_mesh_order(result);

  Vector<GeometrySet> all_geometries;
  all_geometries.append(set_a);
  all_geometries.extend(geometry_sets.values);

  const std::array types_to_join = {GeometryComponent::Type::Edit};
  GeometrySet result_geometry = geometry::join_geometries(
      all_geometries, {}, std::make_optional(types_to_join));
  result_geometry.replace_mesh(result);
  result_geometry.name = set_a.name;

  params.set_output("Mesh", std::move(result_geometry));
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem rna_node_geometry_boolean_method_items[] = {
      {int(geometry::boolean::Operation::Intersect),
       "INTERSECT",
       0,
       "Intersect",
       "Keep the part of the mesh that is common between all operands"},
      {int(geometry::boolean::Operation::Union),
       "UNION",
       0,
       "Union",
       "Combine meshes in an additive way"},
      {int(geometry::boolean::Operation::Difference),
       "DIFFERENCE",
       0,
       "Difference",
       "Combine meshes in a subtractive way"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  static const EnumPropertyItem rna_geometry_boolean_solver_items[] = {
      {int(geometry::boolean::Solver::MeshArr),
       "EXACT",
       0,
       "Exact",
       "Slower solver with the best results for coplanar faces"},
      {int(geometry::boolean::Solver::Float),
       "FLOAT",
       0,
       "Float",
       "Simple solver with good performance, without support for overlapping geometry"},
      {int(geometry::boolean::Solver::Manifold),
       "MANIFOLD",
       0,
       "Manifold",
       "Fastest solver that works only on manifold meshes but gives better results"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "operation",
                    "Operation",
                    "",
                    rna_node_geometry_boolean_method_items,
                    NOD_inline_enum_accessors(custom1),
                    int(geometry::boolean::Operation::Intersect));

  RNA_def_node_enum(srna,
                    "solver",
                    "Solver",
                    "",
                    rna_geometry_boolean_solver_items,
                    NOD_inline_enum_accessors(custom2),
                    int(geometry::boolean::Solver::Float));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeMeshBoolean", GEO_NODE_MESH_BOOLEAN);
  ntype.ui_name = "Mesh Boolean";
  ntype.ui_description = "Cut, subtract, or join multiple mesh inputs";
  ntype.enum_name_legacy = "MESH_BOOLEAN";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.initfunc = node_init;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_boolean_cc
