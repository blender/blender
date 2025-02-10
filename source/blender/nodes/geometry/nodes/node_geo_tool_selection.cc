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

static const void *true_value(const eCustomDataType data_type)
{
  switch (data_type) {
    case CD_PROP_BOOL: {
      static const bool value = true;
      return &value;
    }
    case CD_PROP_FLOAT: {
      static const float value = 1.0f;
      return &value;
    }
    default: {
      BLI_assert_unreachable();
      return nullptr;
    }
  }
}

static const void *false_value(const eCustomDataType data_type)
{
  switch (data_type) {
    case CD_PROP_BOOL: {
      static const bool value = false;
      return &value;
    }
    case CD_PROP_FLOAT: {
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
  EditSelectionFieldInput(eCustomDataType data_type)
      : bke::GeometryFieldInput(*bke::custom_data_type_to_cpp_type(data_type), "Edit Selection")
  {
    category_ = Category::NamedAttribute;
  }

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask & /*mask*/) const override
  {
    const AttrDomain domain = context.domain();
    const eCustomDataType data_type = bke::cpp_type_to_custom_data_type(*type_);
    const AttributeAccessor attributes = *context.attributes();
    switch (context.type()) {
      case GeometryComponent::Type::Curve:
      case GeometryComponent::Type::PointCloud:
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
  SculptSelectionFieldInput(eCustomDataType data_type)
      : bke::GeometryFieldInput(*bke::custom_data_type_to_cpp_type(data_type), "Sculpt Selection")
  {
    category_ = Category::NamedAttribute;
  }

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask &mask) const final
  {
    const AttrDomain domain = context.domain();
    const eCustomDataType data_type = bke::cpp_type_to_custom_data_type(*type_);
    const AttributeAccessor attributes = *context.attributes();
    switch (context.type()) {
      case GeometryComponent::Type::Curve:
      case GeometryComponent::Type::PointCloud:
        return *attributes.lookup_or_default(
            ".selection", domain, data_type, true_value(data_type));
      case GeometryComponent::Type::Mesh: {
        const VArraySpan<float> attribute = *attributes.lookup<float>(".sculpt_mask", domain);
        if (attribute.is_empty()) {
          return GVArray::ForSingle(*type_, mask.min_array_size(), true_value(data_type));
        }
        switch (data_type) {
          case CD_PROP_BOOL: {
            Array<bool> selection(mask.min_array_size());
            mask.foreach_index_optimized<int>(
                GrainSize(4096), [&](const int i) { selection[i] = attribute[i] < 1.0f; });
            return VArray<bool>::ForContainer(std::move(selection));
          }
          case CD_PROP_FLOAT: {
            Array<float> selection(mask.min_array_size());
            mask.foreach_index_optimized<int>(
                GrainSize(4096), [&](const int i) { selection[i] = 1.0f - attribute[i]; });
            return VArray<float>::ForContainer(std::move(selection));
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

static GField get_selection_field(const eObjectMode object_mode, const eCustomDataType data_type)
{
  switch (object_mode) {
    case OB_MODE_OBJECT:
      return fn::make_constant_field<bool>(true);
    case OB_MODE_EDIT:
      return GField(std::make_shared<EditSelectionFieldInput>(data_type));
    case OB_MODE_SCULPT:
    case OB_MODE_SCULPT_CURVES:
      return GField(std::make_shared<SculptSelectionFieldInput>(data_type));
    default:
      return fn::make_constant_field<bool>(false);
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  if (!check_tool_context_and_error(params)) {
    return;
  }
  const eObjectMode mode = params.user_data()->call_data->operator_data->mode;
  params.set_output("Selection", get_selection_field(mode, CD_PROP_BOOL));
  params.set_output("Float", get_selection_field(mode, CD_PROP_FLOAT));
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
  blender::bke::node_register_type(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_tool_selection_cc
