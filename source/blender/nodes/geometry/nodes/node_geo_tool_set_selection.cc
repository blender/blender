/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_mesh.hh"
#include "BKE_type_conversions.hh"

#include "NOD_rna_define.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_enum_types.hh"

#include "FN_multi_function_builder.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_tool_set_selection_cc {

/** \warning Values are stored in files. */
enum class SelectionType {
  Boolean = 0,
  Float = 1,
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_default_layout();
  b.add_input<decl::Geometry>("Geometry").description("Geometry to update the selection of");
  b.add_output<decl::Geometry>("Geometry").align_with_previous();
  if (const bNode *node = b.node_or_null()) {
    switch (SelectionType(node->custom2)) {
      case SelectionType::Boolean:
        b.add_input<decl::Bool>("Selection").default_value(true).field_on_all();
        break;
      case SelectionType::Float:
        b.add_input<decl::Float>("Selection").default_value(1.0f).field_on_all();
        break;
    }
  }
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "domain", UI_ITEM_NONE, "", ICON_NONE);
  layout->prop(ptr, "selection_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = int16_t(AttrDomain::Point);
  node->custom2 = int16_t(SelectionType::Boolean);
}

static GField clamp_selection(const GField &selection)
{
  if (selection.cpp_type().is<bool>()) {
    return selection;
  }
  static auto clamp = mf::build::SI1_SO<float, float>(
      "Clamp", [](const float value) { return std::clamp(value, 0.0f, 1.0f); });
  return Field<float>(FieldOperation::from(clamp, {selection}));
}

static GField invert_selection(const GField &selection)
{
  if (selection.cpp_type().is<bool>()) {
    static auto invert = mf::build::SI1_SO<bool, bool>("Invert Selection",
                                                       [](const bool value) { return !value; });
    return GField(FieldOperation::from(invert, {selection}));
  }

  static auto invert = mf::build::SI1_SO<float, float>(
      "Invert Selection", [](const float value) { return 1.0f - value; });
  return GField(FieldOperation::from(invert, {selection}));
}

static void node_geo_exec(GeoNodeExecParams params)
{
  if (!check_tool_context_and_error(params)) {
    return;
  }
  GeometrySet geometry = params.extract_input<GeometrySet>("Geometry");
  const eObjectMode mode = params.user_data()->call_data->operator_data->mode;
  if (ELEM(mode, OB_MODE_OBJECT, OB_MODE_PAINT_GREASE_PENCIL)) {
    params.error_message_add(NodeWarningType::Error,
                             "Selection control is not supported in this mode");
    params.set_output("Geometry", std::move(geometry));
    return;
  }

  const GField selection = params.extract_input<GField>("Selection");
  const AttrDomain domain = AttrDomain(params.node().custom1);
  const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();
  geometry.modify_geometry_sets([&](GeometrySet &geometry) {
    if (Mesh *mesh = geometry.get_mesh_for_write()) {
      switch (mode) {
        case OB_MODE_EDIT: {
          const Field<bool> field = conversions.try_convert(selection, CPPType::get<bool>());
          switch (domain) {
            case AttrDomain::Point:
              /* Remove attributes in case they are on the wrong domain, which can happen after
               * conversion to and from other geometry types. */
              mesh->attributes_for_write().remove(".select_edge");
              mesh->attributes_for_write().remove(".select_poly");
              bke::try_capture_field_on_geometry(geometry.get_component_for_write<MeshComponent>(),
                                                 ".select_vert",
                                                 AttrDomain::Point,
                                                 field);
              bke::mesh_select_vert_flush(*mesh);
              break;
            case AttrDomain::Edge:
              bke::try_capture_field_on_geometry(geometry.get_component_for_write<MeshComponent>(),
                                                 ".select_edge",
                                                 AttrDomain::Edge,
                                                 field);
              bke::mesh_select_edge_flush(*mesh);
              break;
            case AttrDomain::Face:
              /* Remove attributes in case they are on the wrong domain, which can happen after
               * conversion to and from other geometry types. */
              mesh->attributes_for_write().remove(".select_vert");
              mesh->attributes_for_write().remove(".select_edge");
              bke::try_capture_field_on_geometry(geometry.get_component_for_write<MeshComponent>(),
                                                 ".select_poly",
                                                 AttrDomain::Face,
                                                 field);
              bke::mesh_select_face_flush(*mesh);
              break;
            default: {
              break;
            }
          }
          break;
        }
        case OB_MODE_SCULPT: {
          GField on_domain = GField(
              std::make_shared<bke::EvaluateOnDomainInput>(selection, domain));
          GField clamped_and_inverted = invert_selection(clamp_selection(std::move(on_domain)));
          const Field<float> field = conversions.try_convert(std::move(clamped_and_inverted),
                                                             CPPType::get<float>());
          bke::try_capture_field_on_geometry(geometry.get_component_for_write<MeshComponent>(),
                                             ".sculpt_mask",
                                             AttrDomain::Point,
                                             field);
          break;
        }
        default: {
          break;
        }
      }
    }
    if (geometry.has_curves()) {
      const GField field = clamp_selection(selection);
      if (ELEM(domain, AttrDomain::Point, AttrDomain::Curve)) {
        bke::try_capture_field_on_geometry(
            geometry.get_component_for_write<CurveComponent>(), ".selection", domain, field);
      }
    }
    if (geometry.has_pointcloud()) {
      const GField field = clamp_selection(selection);
      if (domain == AttrDomain::Point) {
        bke::try_capture_field_on_geometry(
            geometry.get_component_for_write<PointCloudComponent>(), ".selection", domain, field);
      }
    }
    if (geometry.has_grease_pencil()) {
      /* Grease Pencil only supports boolean selection. */
      const Field<bool> field = conversions.try_convert(selection, CPPType::get<bool>());
      if (ELEM(domain, AttrDomain::Point, AttrDomain::Curve)) {
        bke::try_capture_field_on_geometry(
            geometry.get_component_for_write<GreasePencilComponent>(),
            ".selection",
            domain,
            field);
      }
    }
  });
  params.set_output("Geometry", std::move(geometry));
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "domain",
                    "Domain",
                    "",
                    rna_enum_attribute_domain_point_edge_face_curve_items,
                    NOD_inline_enum_accessors(custom1),
                    int(AttrDomain::Point));
  static EnumPropertyItem mode_items[] = {
      {int(SelectionType::Boolean),
       "BOOLEAN",
       0,
       "Boolean",
       "Store true or false selection values in edit mode"},
      {int(SelectionType::Float),
       "FLOAT",
       0,
       "Float",
       "Store floating point selection values. For mesh geometry, stored inverted as the sculpt "
       "mode mask"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  RNA_def_node_enum(srna,
                    "selection_type",
                    "Selection Type",
                    "",
                    mode_items,
                    NOD_inline_enum_accessors(custom2),
                    int(SelectionType::Boolean));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeToolSetSelection", GEO_NODE_TOOL_SET_SELECTION);
  ntype.ui_name = "Set Selection";
  ntype.ui_description = "Set selection of the edited geometry, for tool execution";
  ntype.enum_name_legacy = "TOOL_SELECTION_SET";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.gather_link_search_ops = search_link_ops_for_tool_node;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_tool_set_selection_cc
