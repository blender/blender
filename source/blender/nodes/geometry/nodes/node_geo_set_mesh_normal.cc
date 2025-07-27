/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_mesh.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_rna_define.hh"

#include "RNA_enum_types.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_mesh_normal_cc {

enum class Mode {
  Sharpness = 0,
  Free = 1,
  CornerFanSpace = 2,
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_default_layout();
  b.add_input<decl::Geometry>("Mesh")
      .supported_type(GeometryComponent::Type::Mesh)
      .description("Mesh to set the custom normals on");
  b.add_output<decl::Geometry>("Mesh").propagate_all().align_with_previous();
  if (const bNode *node = b.node_or_null()) {
    switch (Mode(node->custom1)) {
      case Mode::Sharpness:
        b.add_input<decl::Bool>("Remove Custom").default_value(true);
        b.add_input<decl::Bool>("Edge Sharpness").supports_field();
        b.add_input<decl::Bool>("Face Sharpness").supports_field();
        break;
      case Mode::Free:
      case Mode::CornerFanSpace:
        b.add_input<decl::Vector>("Custom Normal")
            .subtype(PROP_XYZ)
            .implicit_field(NODE_DEFAULT_INPUT_NORMAL_FIELD)
            .hide_value();
        break;
    }
  }
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  const bNode &node = *static_cast<const bNode *>(ptr->data);
  layout->prop(ptr, "mode", UI_ITEM_NONE, "", ICON_NONE);
  if (Mode(node.custom1) == Mode::Free) {
    layout->prop(ptr, "domain", UI_ITEM_NONE, "", ICON_NONE);
  }
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = int16_t(Mode::Sharpness);
  node->custom2 = int16_t(bke::AttrDomain::Point);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const bNode &node = params.node();
  const Mode mode = static_cast<Mode>(node.custom1);
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");

  bool add_sharpness_and_corner_fan_info = false;

  switch (mode) {
    case Mode::Sharpness: {
      const bool remove_custom = params.extract_input<bool>("Remove Custom");
      const fn::Field sharp_edge = params.extract_input<fn::Field<bool>>("Edge Sharpness");
      const fn::Field sharp_face = params.extract_input<fn::Field<bool>>("Face Sharpness");
      geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
        if (Mesh *mesh = geometry_set.get_mesh_for_write()) {
          /* Evaluate both fields before storing the result to avoid one attribute change
           * potentially affecting the other field evaluation. */
          const bke::MeshFieldContext edge_context(*mesh, bke::AttrDomain::Edge);
          const bke::MeshFieldContext face_context(*mesh, bke::AttrDomain::Face);
          fn::FieldEvaluator edge_evaluator(edge_context, mesh->edges_num);
          fn::FieldEvaluator face_evaluator(face_context, mesh->faces_num);
          edge_evaluator.add(sharp_edge);
          face_evaluator.add(sharp_face);
          edge_evaluator.evaluate();
          face_evaluator.evaluate();
          const IndexMask edge_values = edge_evaluator.get_evaluated_as_mask(0);
          const IndexMask face_values = face_evaluator.get_evaluated_as_mask(0);
          bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
          if (edge_values.is_empty()) {
            attributes.remove("sharp_edge");
          }
          else {
            bke::SpanAttributeWriter attr = attributes.lookup_or_add_for_write_only_span<bool>(
                "sharp_edge", bke::AttrDomain::Edge);
            edge_values.to_bools(attr.span);
            attr.finish();
          }
          if (face_values.is_empty()) {
            attributes.remove("sharp_face");
          }
          else {
            bke::SpanAttributeWriter attr = attributes.lookup_or_add_for_write_only_span<bool>(
                "sharp_face", bke::AttrDomain::Face);
            face_values.to_bools(attr.span);
            attr.finish();
          }
          if (remove_custom) {
            attributes.remove("custom_normal");
          }
          else {
            if (const std::optional<bke::AttributeMetaData> meta_data =
                    attributes.lookup_meta_data("custom_normal"))
            {
              if (meta_data->domain == bke::AttrDomain::Corner &&
                  meta_data->data_type == bke::AttrType::Int16_2D)
              {
                add_sharpness_and_corner_fan_info = true;
              }
            }
          }
        }
      });
      break;
    }
    case Mode::Free: {
      const fn::Field custom_normal = params.extract_input<fn::Field<float3>>("Custom Normal");
      geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
        if (Mesh *mesh = geometry_set.get_mesh_for_write()) {
          const bke::AttrDomain domain = bke::AttrDomain(node.custom2);
          bke::try_capture_field_on_geometry(mesh->attributes_for_write(),
                                             bke::MeshFieldContext(*mesh, domain),
                                             "custom_normal",
                                             domain,
                                             fn::make_constant_field(true),
                                             custom_normal);
        }
      });
      break;
    }
    case Mode::CornerFanSpace: {
      const fn::Field custom_normal = params.extract_input<fn::Field<float3>>("Custom Normal");
      geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
        if (Mesh *mesh = geometry_set.get_mesh_for_write()) {
          const bke::MeshFieldContext context(*mesh, bke::AttrDomain::Corner);
          fn::FieldEvaluator evaluator(context, mesh->corners_num);
          Array<float3> corner_normals(mesh->corners_num);
          evaluator.add_with_destination<float3>(custom_normal, corner_normals);
          evaluator.evaluate();
          mesh->attributes_for_write().remove("custom_normal");
          bke::mesh_set_custom_normals(*mesh, corner_normals);
        }
      });
      break;
    }
  }

  if (add_sharpness_and_corner_fan_info) {
    params.error_message_add(NodeWarningType::Info,
                             "Adjusting sharpness with \"Tangent Space\" custom normals "
                             "may lead to unexpected results");
  }

  params.set_output("Mesh", std::move(geometry_set));
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem mode_items[] = {
      {int(Mode::Sharpness),
       "SHARPNESS",
       0,
       "Sharpness",
       "Store the sharpness of each face or edge. Similar to the \"Shade Smooth\" and \"Shade "
       "Flat\" operators."},
      {int(Mode::Free),
       "FREE",
       0,
       "Free",
       "Store custom normals as simple vectors in the local space of the mesh. Values are not "
       "necessarily updated automatically later on as the mesh is deformed."},
      {int(Mode::CornerFanSpace),
       "TANGENT_SPACE",
       0,
       "Tangent Space",
       "Store normals in a deformation dependent custom transformation space. This method is "
       "slower, but can be better when subsequent operations change the mesh without handling "
       "normals specifically."},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "mode",
                    "Mode",
                    "Storage mode for custom normal data",
                    mode_items,
                    NOD_inline_enum_accessors(custom1));
  RNA_def_node_enum(srna,
                    "domain",
                    "Domain",
                    "Attribute domain to store free custom normals",
                    rna_enum_attribute_domain_only_mesh_no_edge_items,
                    NOD_inline_enum_accessors(custom2));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeSetMeshNormal");
  ntype.ui_name = "Set Mesh Normal";
  ntype.ui_description = "Store a normal vector for each mesh element";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.initfunc = node_init;
  ntype.draw_buttons = node_layout;

  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_set_mesh_normal_cc
