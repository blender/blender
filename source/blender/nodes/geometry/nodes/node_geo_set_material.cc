/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "DNA_curves_types.h"
#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_volume_types.h"

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_material.hh"

#include "GEO_foreach_geometry.hh"

namespace blender::nodes::node_geo_set_material_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Geometry>("Geometry")
      .supported_type({GeometryComponent::Type::Mesh,
                       GeometryComponent::Type::Volume,
                       GeometryComponent::Type::PointCloud,
                       GeometryComponent::Type::Curve,
                       GeometryComponent::Type::GreasePencil});
  b.add_output<decl::Geometry>("Geometry")
      .propagate_all()
      .align_with_previous()
      .description("Geometry to assign a material to");
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Material>("Material").optional_label();
}

static void assign_material_to_id_geometry(ID *id,
                                           const fn::FieldContext &field_context,
                                           const Field<bool> &selection_field,
                                           MutableAttributeAccessor &attributes,
                                           const AttrDomain domain,
                                           Material *material)
{
  const int domain_size = attributes.domain_size(domain);
  fn::FieldEvaluator selection_evaluator{field_context, domain_size};
  selection_evaluator.set_selection(selection_field);
  selection_evaluator.evaluate();
  const IndexMask selection = selection_evaluator.get_evaluated_selection_as_mask();

  if (selection.size() != attributes.domain_size(domain)) {
    /* If the entire geometry isn't selected, and there is no material slot yet, add an empty
     * slot so that the faces that aren't selected can still refer to the default material. */
    BKE_id_material_eval_ensure_default_slot(id);
  }

  int new_index = -1;
  const int orig_materials_num = *BKE_id_material_len_p(id);
  if (Material **materials = *BKE_id_material_array_p(id)) {
    new_index = Span(materials, orig_materials_num).first_index_try(material);
  }

  if (new_index == -1) {
    /* Append a new material index. */
    new_index = orig_materials_num;
    BKE_id_material_eval_assign(id, new_index + 1, material);
  }

  SpanAttributeWriter<int> indices = attributes.lookup_or_add_for_write_span<int>("material_index",
                                                                                  domain);
  index_mask::masked_fill(indices.span, new_index, selection);
  indices.finish();
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

  geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry_set) {
    if (Mesh *mesh = geometry_set.get_mesh_for_write()) {
      if (mesh->faces_num == 0) {
        if (mesh->verts_num > 0) {
          no_faces_warning = true;
        }
      }
      else {
        const bke::MeshFieldContext field_context{*mesh, AttrDomain::Face};
        MutableAttributeAccessor attributes = mesh->attributes_for_write();
        assign_material_to_id_geometry(
            &mesh->id, field_context, selection_field, attributes, AttrDomain::Face, material);
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
    if (GreasePencil *grease_pencil = geometry_set.get_grease_pencil_for_write()) {
      using namespace blender::bke::greasepencil;
      for (const int layer_index : grease_pencil->layers().index_range()) {
        Drawing *drawing = grease_pencil->get_eval_drawing(grease_pencil->layer(layer_index));
        if (drawing == nullptr) {
          continue;
        }
        bke::CurvesGeometry &curves = drawing->strokes_for_write();
        if (curves.is_empty()) {
          continue;
        }

        const bke::GreasePencilLayerFieldContext field_context{
            *grease_pencil, AttrDomain::Curve, layer_index};
        MutableAttributeAccessor attributes = curves.attributes_for_write();
        assign_material_to_id_geometry(&grease_pencil->id,
                                       field_context,
                                       selection_field,
                                       attributes,
                                       AttrDomain::Curve,
                                       material);
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
        TIP_("Volumes only support a single material; selection input cannot be a field"));
  }
  if (point_selection_warning) {
    params.error_message_add(
        NodeWarningType::Info,
        TIP_("Point clouds only support a single material; selection input cannot be a field"));
  }
  if (curves_selection_warning) {
    params.error_message_add(
        NodeWarningType::Info,
        TIP_("Curves only support a single material; selection input cannot be a field"));
  }

  params.set_output("Geometry", std::move(geometry_set));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSetMaterial", GEO_NODE_SET_MATERIAL);
  ntype.ui_name = "Set Material";
  ntype.ui_description = "Assign a material to geometry elements";
  ntype.enum_name_legacy = "SET_MATERIAL";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_set_material_cc
