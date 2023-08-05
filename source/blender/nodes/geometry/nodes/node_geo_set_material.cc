/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "DNA_curves_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_volume_types.h"

#include "BKE_material.h"
#include "BKE_mesh.hh"

namespace blender::nodes::node_geo_set_material_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry")
      .supported_type({GeometryComponent::Type::Mesh,
                       GeometryComponent::Type::Volume,
                       GeometryComponent::Type::PointCloud,
                       GeometryComponent::Type::Curve});
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Material>("Material").hide_label();
  b.add_output<decl::Geometry>("Geometry").propagate_all();
}

static void assign_material_to_faces(Mesh &mesh, const IndexMask &selection, Material *material)
{
  if (selection.size() != mesh.faces_num) {
    /* If the entire mesh isn't selected, and there is no material slot yet, add an empty
     * slot so that the faces that aren't selected can still refer to the default material. */
    BKE_id_material_eval_ensure_default_slot(&mesh.id);
  }

  int new_material_index = -1;
  for (const int i : IndexRange(mesh.totcol)) {
    Material *other_material = mesh.mat[i];
    if (other_material == material) {
      new_material_index = i;
      break;
    }
  }
  if (new_material_index == -1) {
    /* Append a new material index. */
    new_material_index = mesh.totcol;
    BKE_id_material_eval_assign(&mesh.id, new_material_index + 1, material);
  }

  MutableAttributeAccessor attributes = mesh.attributes_for_write();
  SpanAttributeWriter<int> material_indices = attributes.lookup_or_add_for_write_span<int>(
      "material_index", ATTR_DOMAIN_FACE);
  index_mask::masked_fill(material_indices.span, new_material_index, selection);
  material_indices.finish();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Material *material = params.extract_input<Material *>("Material");
  const Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  /* Only add the warnings once, even if there are many unique instances. */
  bool no_faces_warning = false;
  bool point_selection_warning = false;
  bool volume_selection_warning = false;
  bool curves_selection_warning = false;

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (Mesh *mesh = geometry_set.get_mesh_for_write()) {
      if (mesh->faces_num == 0) {
        if (mesh->totvert > 0) {
          no_faces_warning = true;
        }
      }
      else {
        const bke::MeshFieldContext field_context{*mesh, ATTR_DOMAIN_FACE};
        fn::FieldEvaluator selection_evaluator{field_context, mesh->faces_num};
        selection_evaluator.add(selection_field);
        selection_evaluator.evaluate();
        const IndexMask selection = selection_evaluator.get_evaluated_as_mask(0);

        assign_material_to_faces(*mesh, selection, material);
      }
    }
    if (Volume *volume = geometry_set.get_volume_for_write()) {
      BKE_id_material_eval_assign(&volume->id, 1, material);
      if (selection_field.node().depends_on_input()) {
        volume_selection_warning = true;
      }
    }
    if (PointCloud *pointcloud = geometry_set.get_pointcloud_for_write()) {
      BKE_id_material_eval_assign(&pointcloud->id, 1, material);
      if (selection_field.node().depends_on_input()) {
        point_selection_warning = true;
      }
    }
    if (Curves *curves = geometry_set.get_curves_for_write()) {
      BKE_id_material_eval_assign(&curves->id, 1, material);
      if (selection_field.node().depends_on_input()) {
        curves_selection_warning = true;
      }
    }
  });

  if (no_faces_warning) {
    params.error_message_add(NodeWarningType::Info,
                             TIP_("Mesh has no faces for material assignment"));
  }
  if (volume_selection_warning) {
    params.error_message_add(
        NodeWarningType::Info,
        TIP_("Volumes only support a single material; selection input can not be a field"));
  }
  if (point_selection_warning) {
    params.error_message_add(
        NodeWarningType::Info,
        TIP_("Point clouds only support a single material; selection input can not be a field"));
  }
  if (curves_selection_warning) {
    params.error_message_add(
        NodeWarningType::Info,
        TIP_("Curves only support a single material; selection input can not be a field"));
  }

  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_set_material_cc

void register_node_type_geo_set_material()
{
  namespace file_ns = blender::nodes::node_geo_set_material_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SET_MATERIAL, "Set Material", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
