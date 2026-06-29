/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <atomic>

#include "BKE_attribute.hh"
#include "BKE_instances.hh"

#include "BLI_index_mask.hh"

#include "GEO_foreach_geometry.hh"
#include "GEO_reorder.hh"

#include "NOD_rna_define.hh"

#include "RNA_enum_types.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_sort_elements_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_default_layout();
  b.add_input<decl::Geometry>("Geometry"_ustr).description("Geometry to sort the elements of");
  b.add_output<decl::Geometry>("Geometry"_ustr).propagate_all_geometry().align_with_previous();
  b.add_input<decl::Bool>("Selection"_ustr)
      .default_value(true)
      .evaluated_geometry_field()
      .hide_value();
  b.add_input<decl::Int>("Group ID"_ustr).evaluated_geometry_field().hide_value();
  b.add_input<decl::Float>("Sort Weight"_ustr).evaluated_geometry_field().hide_value();
}

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "domain", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = int(bke::AttrDomain::Point);
}

static std::optional<Array<int>> sorted_indices(const fn::FieldContext &field_context,
                                                const int domain_size,
                                                const Field<bool> selection_field,
                                                const Field<int> group_id_field,
                                                const Field<float> weight_field)
{
  if (domain_size == 0) {
    return std::nullopt;
  }

  FieldEvaluator evaluator(field_context, domain_size);
  evaluator.set_selection(selection_field);
  evaluator.add(group_id_field);
  evaluator.add(weight_field);
  evaluator.evaluate();
  const IndexMask mask = evaluator.get_evaluated_selection_as_mask();
  const VArray<int> group_id = evaluator.get_evaluated<int>(0);
  const VArray<float> weight = evaluator.get_evaluated<float>(1);

  return geometry::sort_indices_by_weights(domain_size, mask, group_id, weight);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry"_ustr);
  const Field<bool> selection_field = params.extract_input<Field<bool>>("Selection"_ustr);
  const Field<int> group_id_field = params.extract_input<Field<int>>("Group ID"_ustr);
  const Field<float> weight_field = params.extract_input<Field<float>>("Sort Weight"_ustr);
  const bke::AttrDomain domain = bke::AttrDomain(params.node().custom1);

  const NodeAttributeFilter attribute_filter = params.get_attribute_filter("Geometry"_ustr);

  GeometryComponentEditData::remember_deformed_positions_if_necessary(geometry_set);

  std::atomic<bool> has_reorder = false;
  std::atomic<bool> has_unsupported = false;
  if (domain == bke::AttrDomain::Instance) {
    if (const bke::Instances *instances = geometry_set.get_instances()) {
      if (const std::optional<Array<int>> indices = sorted_indices(
              bke::InstancesFieldContext(*instances),
              instances->instances_num(),
              selection_field,
              group_id_field,
              weight_field))
      {
        bke::Instances *result = geometry::reorder_instaces(
            *instances, *indices, attribute_filter);
        geometry_set.replace_instances(result);
        has_reorder = true;
      }
    }
  }
  else {
    geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry_set) {
      for (const auto [type, domains] : geometry::components_supported_reordering().items()) {
        const bke::GeometryComponent *src_component = geometry_set.get_component(type);
        if (src_component == nullptr || src_component->is_empty()) {
          continue;
        }
        if (!domains.contains(domain)) {
          has_unsupported = true;
          continue;
        }
        has_reorder = true;
        const std::optional<Array<int>> indices = sorted_indices(
            bke::GeometryFieldContext(*src_component, domain),
            src_component->attribute_domain_size(domain),
            selection_field,
            group_id_field,
            weight_field);
        if (!indices.has_value()) {
          continue;
        }
        bke::GeometryComponentPtr dst_component = geometry::reordered_component(
            *src_component, *indices, domain, attribute_filter);
        geometry_set.remove(type);
        geometry_set.add(*dst_component.get());
      }
    });
  }

  if (has_unsupported && !has_reorder) {
    params.error_message_add(NodeWarningType::Info,
                             TIP_("Domain and geometry type combination is unsupported"));
  }

  params.set_output("Geometry"_ustr, std::move(geometry_set));
}

template<typename T>
static Vector<EnumPropertyItem> items_value_in(const Span<T> values,
                                               const EnumPropertyItem *src_items)
{
  Vector<EnumPropertyItem> items;
  for (const EnumPropertyItem *item = src_items; item->identifier != nullptr; item++) {
    if (values.contains(T(item->value))) {
      items.append(*item);
    }
  }
  items.append({0, nullptr, 0, nullptr, nullptr});
  return items;
}

static void node_rna(StructRNA *srna)
{
  static const Vector<EnumPropertyItem> supported_items = items_value_in<bke::AttrDomain>(
      {bke::AttrDomain::Point,
       bke::AttrDomain::Edge,
       bke::AttrDomain::Face,
       bke::AttrDomain::Curve,
       bke::AttrDomain::Instance},
      rna_enum_attribute_domain_items);

  RNA_def_node_enum(srna,
                    "domain",
                    "Domain",
                    "",
                    supported_items.data(),
                    NOD_inline_enum_accessors(custom1),
                    int(bke::AttrDomain::Point));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSortElements"_ustr, GEO_NODE_SORT_ELEMENTS);
  ntype.ui_name = "Sort Elements";
  ntype.ui_description = "Rearrange geometry elements, changing their indices";
  ntype.enum_name_legacy = "SORT_ELEMENTS";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_sort_elements_cc
