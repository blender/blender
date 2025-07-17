/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_geometry_fields.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_tool_selection_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Bool>("Boolean", "Selection")
      .field_source()
      .description("The selection of each element as a true or false value");
  b.add_output<decl::Float>("Float").field_source().description(
      "The selection of each element as a floating point value");
}

static const void *true_value(const bke::AttrType data_type)
{
  switch (data_type) {
    case bke::AttrType::Bool: {
      static const bool value = true;
      return &value;
    }
    case bke::AttrType::Float: {
      static const float value = 1.0f;
      return &value;
    }
    default: {
      BLI_assert_unreachable();
      return nullptr;
    }
  }
}

static const void *false_value(const bke::AttrType data_type)
{
  switch (data_type) {
    case bke::AttrType::Bool: {
      static const bool value = false;
      return &value;
    }
    case bke::AttrType::Float: {
      static const float value = 0.0f;
      return &value;
    }
    default: {
      BLI_assert_unreachable();
      return nullptr;
    }
  }
}

static StringRef mesh_selection_name(const AttrDomain domain)
{
  switch (domain) {
    case AttrDomain::Point:
      return ".select_vert";
    case AttrDomain::Edge:
      return ".select_edge";
    case AttrDomain::Face:
    case AttrDomain::Corner:
      return ".select_poly";
    default:
      BLI_assert_unreachable();
      return "";
  }
}

class EditSelectionFieldInput final : public bke::GeometryFieldInput {
 public:
  EditSelectionFieldInput(bke::AttrType data_type)
      : bke::GeometryFieldInput(bke::attribute_type_to_cpp_type(data_type), "Edit Selection")
  {
    category_ = Category::NamedAttribute;
  }

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask & /*mask*/) const override
  {
    const AttrDomain domain = context.domain();
    const bke::AttrType data_type = bke::cpp_type_to_attribute_type(*type_);
    const AttributeAccessor attributes = *context.attributes();
    switch (context.type()) {
      case GeometryComponent::Type::Curve:
      case GeometryComponent::Type::PointCloud:
      case GeometryComponent::Type::GreasePencil:
        return *attributes.lookup_or_default(
            ".selection", domain, data_type, true_value(data_type));
      case GeometryComponent::Type::Mesh:
        return *attributes.lookup_or_default(
            mesh_selection_name(domain), domain, data_type, false_value(data_type));
      default:
        return {};
    }
  }
};

class SculptSelectionFieldInput final : public bke::GeometryFieldInput {
 public:
  SculptSelectionFieldInput(bke::AttrType data_type)
      : bke::GeometryFieldInput(bke::attribute_type_to_cpp_type(data_type), "Sculpt Selection")
  {
    category_ = Category::NamedAttribute;
  }

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask &mask) const final
  {
    const AttrDomain domain = context.domain();
    const bke::AttrType data_type = bke::cpp_type_to_attribute_type(*type_);
    const AttributeAccessor attributes = *context.attributes();
    switch (context.type()) {
      case GeometryComponent::Type::Curve:
      case GeometryComponent::Type::PointCloud:
      case GeometryComponent::Type::GreasePencil:
        return *attributes.lookup_or_default(
            ".selection", domain, data_type, true_value(data_type));
      case GeometryComponent::Type::Mesh: {
        const VArraySpan<float> attribute = *attributes.lookup<float>(".sculpt_mask", domain);
        if (attribute.is_empty()) {
          return GVArray::from_single(*type_, mask.min_array_size(), true_value(data_type));
        }
        switch (data_type) {
          case bke::AttrType::Bool: {
            Array<bool> selection(mask.min_array_size());
            mask.foreach_index_optimized<int>(
                GrainSize(4096), [&](const int i) { selection[i] = attribute[i] < 1.0f; });
            return VArray<bool>::from_container(std::move(selection));
          }
          case bke::AttrType::Float: {
            Array<float> selection(mask.min_array_size());
            mask.foreach_index_optimized<int>(
                GrainSize(4096), [&](const int i) { selection[i] = 1.0f - attribute[i]; });
            return VArray<float>::from_container(std::move(selection));
          }
          default: {
            BLI_assert_unreachable();
            return {};
          }
        }
      }
      default:
        return {};
    }
  }
};

static GField get_selection_field(const eObjectMode object_mode, const bke::AttrType data_type)
{
  switch (object_mode) {
    case OB_MODE_OBJECT:
      return fn::make_constant_field(bke::attribute_type_to_cpp_type(data_type),
                                     true_value(data_type));
    case OB_MODE_EDIT:
      return GField(std::make_shared<EditSelectionFieldInput>(data_type));
    case OB_MODE_SCULPT:
    case OB_MODE_SCULPT_CURVES:
    case OB_MODE_SCULPT_GREASE_PENCIL:
      return GField(std::make_shared<SculptSelectionFieldInput>(data_type));
    case OB_MODE_PAINT_GREASE_PENCIL:
      return fn::make_constant_field(bke::attribute_type_to_cpp_type(data_type),
                                     true_value(data_type));
    default:
      return fn::make_constant_field(bke::attribute_type_to_cpp_type(data_type),
                                     false_value(data_type));
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  if (!check_tool_context_and_error(params)) {
    return;
  }
  const eObjectMode mode = params.user_data()->call_data->operator_data->mode;
  params.set_output("Selection", get_selection_field(mode, bke::AttrType::Bool));
  params.set_output("Float", get_selection_field(mode, bke::AttrType::Float));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeToolSelection", GEO_NODE_TOOL_SELECTION);
  ntype.ui_name = "Selection";
  ntype.ui_description = "User selection of the edited geometry, for tool execution";
  ntype.enum_name_legacy = "TOOL_SELECTION";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.gather_link_search_ops = search_link_ops_for_tool_node;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_tool_selection_cc
