/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"

#include "DNA_curves_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"

#include "NOD_geometry_nodes_list.hh"
#include "NOD_string_pattern.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_transfer_attributes_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Geometry>("Target"_ustr);
  b.add_output<decl::Geometry>("Target"_ustr).align_with_previous().propagate_all();
  b.add_output<decl::Bool>("Success"_ustr);
  {
    auto &p = b.add_panel("Target IDs"_ustr).default_closed(true);
    Vector<BaseSocketDeclarationBuilder *> sockets;
    sockets.append(&p.add_input<decl::Int>("Target Point ID"_ustr));
    sockets.append(&p.add_input<decl::Int>("Target Edge ID"_ustr));
    sockets.append(&p.add_input<decl::Int>("Target Face ID"_ustr));
    sockets.append(&p.add_input<decl::Int>("Target Corner ID"_ustr));
    sockets.append(&p.add_input<decl::Int>("Target Curve ID"_ustr));
    sockets.append(&p.add_input<decl::Int>("Target Instance ID"_ustr));

    for (BaseSocketDeclarationBuilder *socket : sockets) {
      socket->implicit_field(NODE_DEFAULT_INPUT_INDEX_FIELD);
      socket->structure_type(StructureType::Field);
    }
  }
  b.add_input<decl::Geometry>("Source"_ustr);
  {
    auto &p = b.add_panel("Source IDs"_ustr).default_closed(true);
    Vector<BaseSocketDeclarationBuilder *> sockets;
    sockets.append(&p.add_input<decl::Int>("Source Point ID"_ustr));
    sockets.append(&p.add_input<decl::Int>("Source Edge ID"_ustr));
    sockets.append(&p.add_input<decl::Int>("Source Face ID"_ustr));
    sockets.append(&p.add_input<decl::Int>("Source Corner ID"_ustr));
    sockets.append(&p.add_input<decl::Int>("Source Curve ID"_ustr));
    sockets.append(&p.add_input<decl::Int>("Source Instance ID"_ustr));

    for (BaseSocketDeclarationBuilder *socket : sockets) {
      socket->implicit_field(NODE_DEFAULT_INPUT_INDEX_FIELD);
      socket->structure_type(StructureType::Field);
    }
  }

  b.add_input<decl::Menu>("Pattern Mode"_ustr)
      .static_items(string_pattern_mode_items)
      .optional_label();
  b.add_input<decl::String>("Names"_ustr)
      .optional_label()
      .structure_type(StructureType::List)
      .description(
          "List of attribute names (not) to transfer. A wildcard (*) at the end is allowed");
  b.add_input<decl::Bool>("Ignore Names"_ustr).default_value(false);
}

static bool name_matches_any_pattern(const Span<StringPattern> patterns, const StringRef name)
{
  for (const StringPattern &pattern : patterns) {
    if (pattern.match(name)) {
      return true;
    }
  }
  return false;
}

static bool should_transfer(const Span<StringPattern> patterns,
                            const StringRef name,
                            const bool ignore_names)
{
  if (ELEM(name, ".corner_vert", ".corner_edge", ".edge_verts")) {
    return false;
  }
  const bool matches = name_matches_any_pattern(patterns, name);
  if (ignore_names) {
    return !matches;
  }
  return matches;
}

static bool transfer_attributes(
    const Span<StringPattern> patterns,
    const bool ignore_names,
    const bke::AttributeAccessor &src_attributes,
    bke::MutableAttributeAccessor &dst_attributes,
    const Map<bke::AttrDomain, Field<int>> &src_id_fields,
    const Map<bke::AttrDomain, Field<int>> &dst_id_fields,
    FunctionRef<fn::FieldContext &(ResourceScope &scope, const bke::AttrDomain domain)>
        create_src_context,
    FunctionRef<fn::FieldContext &(ResourceScope &scope, const bke::AttrDomain domain)>
        create_dst_context)
{
  struct AttrItem {
    StringRef name;
    AttrDomain domain;
    bke::AttrType type;
  };
  struct IDs {
    bool transfer_by_index = false;
    Array<int> src_by_dst_index;
    IndexMask dst_mask;
  };
  Map<bke::AttrDomain, IDs> ids_by_domain;
  Vector<AttrItem> items;
  src_attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (should_transfer(patterns, iter.name, ignore_names)) {
      items.append({iter.name, iter.domain, iter.data_type});
      ids_by_domain.lookup_or_add_default(iter.domain);
    }
  });

  ResourceScope scope;
  for (const auto &[domain, ids] : ids_by_domain.items()) {
    const Field<int> &src_id_field = src_id_fields.lookup(domain);
    const Field<int> &dst_id_field = dst_id_fields.lookup(domain);
    if (src_id_field.get_input_if<fn::IndexFieldInput>() &&
        dst_id_field.get_input_if<fn::IndexFieldInput>())
    {
      ids.transfer_by_index = true;
      continue;
    }

    const int src_size = src_attributes.domain_size(domain);
    const int dst_size = dst_attributes.domain_size(domain);

    fn::FieldContext &src_field_context = create_src_context(scope, domain);
    fn::FieldEvaluator src_evaluator(src_field_context, src_size);
    src_evaluator.add(src_id_field);
    src_evaluator.evaluate();
    const VArraySpan<int> src_ids = src_evaluator.get_evaluated<int>(0);

    fn::FieldContext &dst_field_context = create_dst_context(scope, domain);
    fn::FieldEvaluator dst_evaluator(dst_field_context, dst_size);
    dst_evaluator.add(dst_id_field);
    dst_evaluator.evaluate();
    const VArraySpan<int> dst_ids = dst_evaluator.get_evaluated<int>(0);

    Map<int, int> src_index_by_id;
    for (const int i : IndexRange(src_size)) {
      const int id = src_ids[i];
      src_index_by_id.add(id, i);
    }
    ids.src_by_dst_index.reinitialize(dst_size);
    threading::parallel_for(IndexRange(dst_size), 2048, [&](const IndexRange range) {
      for (const int dst_i : range) {
        const int dst_id = dst_ids[dst_i];
        const int src_i = src_index_by_id.lookup_default(dst_id, -1);
        ids.src_by_dst_index[dst_i] = src_i;
      }
    });
    ids.dst_mask = array_utils::indices_non_negative(
        IndexMask(dst_size), ids.src_by_dst_index, scope.allocator());
  }

  bool any_transferred = false;
  for (const AttrItem &item : items) {
    const bke::GAttributeReader src_attr = src_attributes.lookup(item.name);
    const CommonVArrayInfo info = src_attr.varray.common_info();
    const IDs &ids = ids_by_domain.lookup(item.domain);
    if (info.type == CommonVArrayInfo::Type::Single) {
      if (ids.dst_mask.size() == dst_attributes.domain_size(item.domain)) {
        if (dst_attributes.add(item.name,
                               item.domain,
                               item.type,
                               bke::AttributeInitValue(GPointer{
                                   bke::attribute_type_to_cpp_type(item.type), info.data})))
        {
          any_transferred = true;
          continue;
        }
      }
    }
    if (info.type == CommonVArrayInfo::Type::Span) {
      if (ids.transfer_by_index) {
        if (ids.dst_mask.size() == dst_attributes.domain_size(item.domain)) {
          if (src_attr.sharing_info) {
            if (dst_attributes.add(item.name,
                                   item.domain,
                                   item.type,
                                   bke::AttributeInitShared(info.data, *src_attr.sharing_info)))
            {
              any_transferred = true;
              continue;
            }
          }
        }
      }
    }

    bke::GSpanAttributeWriter dst_attr;
    if (ids.dst_mask.size() == dst_attributes.domain_size(item.domain)) {
      dst_attr = dst_attributes.lookup_or_add_for_write_span(item.name, item.domain, item.type);
    }
    else {
      dst_attr = dst_attributes.lookup_or_add_for_write_only_span(
          item.name, item.domain, item.type);
    }
    if (!dst_attr) {
      continue;
    }
    const int src_size = src_attr.varray.size();
    const int dst_size = dst_attr.span.size();

    if (ids.transfer_by_index) {
      const int copy_num = std::min(src_size, dst_size);
      const IndexRange slice(copy_num);
      array_utils::copy(src_attr.varray.slice(slice), dst_attr.span.slice(slice));
    }
    else {
      bke::attribute_math::gather(*src_attr, ids.src_by_dst_index, ids.dst_mask, dst_attr.span);
    }
    dst_attr.finish();
    any_transferred = true;
  }

  return any_transferred;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet dst_geo = params.extract_input<GeometrySet>("Target"_ustr);
  GeometrySet src_geo = params.extract_input<GeometrySet>("Source"_ustr);
  const StringPatternMode pattern_mode = params.extract_input<StringPatternMode>(
      "Pattern Mode"_ustr);
  const GListPtr attribute_patterns_list = params.extract_input<GListPtr>("Names"_ustr);
  const bool ignore_names = params.extract_input<bool>("Ignore Names"_ustr);

  Map<bke::AttrDomain, Field<int>> dst_id_fields = {
      {AttrDomain::Point, params.extract_input<Field<int>>("Target Point ID"_ustr)},
      {AttrDomain::Edge, params.extract_input<Field<int>>("Target Edge ID"_ustr)},
      {AttrDomain::Face, params.extract_input<Field<int>>("Target Face ID"_ustr)},
      {AttrDomain::Corner, params.extract_input<Field<int>>("Target Corner ID"_ustr)},
      {AttrDomain::Curve, params.extract_input<Field<int>>("Target Curve ID"_ustr)},
      {AttrDomain::Instance, params.extract_input<Field<int>>("Target Instance ID"_ustr)},
  };

  Map<bke::AttrDomain, Field<int>> src_id_fields{
      {AttrDomain::Point, params.extract_input<Field<int>>("Source Point ID"_ustr)},
      {AttrDomain::Edge, params.extract_input<Field<int>>("Source Edge ID"_ustr)},
      {AttrDomain::Face, params.extract_input<Field<int>>("Source Face ID"_ustr)},
      {AttrDomain::Corner, params.extract_input<Field<int>>("Source Corner ID"_ustr)},
      {AttrDomain::Curve, params.extract_input<Field<int>>("Source Curve ID"_ustr)},
      {AttrDomain::Instance, params.extract_input<Field<int>>("Source Instance ID"_ustr)},
  };

  Vector<StringPattern> patterns;
  bool has_error = false;
  if (attribute_patterns_list) {
    if (attribute_patterns_list->cpp_type().is<std::string>()) {
      attribute_patterns_list.typed<std::string>()->foreach([&](const std::string &pattern) {
        std::string error;
        if (std::optional<StringPattern> pattern_fn = StringPattern::from_string(
                pattern_mode, pattern, error))
        {
          patterns.append(std::move(*pattern_fn));
        }
        else {
          params.error_message_add(NodeWarningType::Error, error);
          has_error = true;
        }
      });
    }
  }

  if (patterns.is_empty()) {
    params.set_output("Target"_ustr, std::move(dst_geo));
    params.set_output("Success"_ustr, !has_error);
    return;
  }

  bool success = false;
  for (const bke::GeometryComponent::Type type : {bke::GeometryComponent::Type::Mesh,
                                                  bke::GeometryComponent::Type::PointCloud,
                                                  bke::GeometryComponent::Type::Curve,
                                                  bke::GeometryComponent::Type::Instance})
  {
    if (!dst_geo.has(type)) {
      continue;
    }
    if (!src_geo.has(type)) {
      continue;
    }
    const GeometryComponent &src_component = *src_geo.get_component(type);
    GeometryComponent &dst_component = dst_geo.get_component_for_write(type);
    const bke::AttributeAccessor src_attributes = *src_component.attributes();
    bke::MutableAttributeAccessor dst_attributes = *dst_component.attributes_for_write();
    success = success |
              transfer_attributes(
                  patterns,
                  ignore_names,
                  src_attributes,
                  dst_attributes,
                  src_id_fields,
                  dst_id_fields,
                  [&](ResourceScope &scope, const bke::AttrDomain domain) -> fn::FieldContext & {
                    return scope.construct<bke::GeometryFieldContext>(src_component, domain);
                  },
                  [&](ResourceScope &scope, const bke::AttrDomain domain) -> fn::FieldContext & {
                    return scope.construct<bke::GeometryFieldContext>(dst_component, domain);
                  });
  }

  if (src_geo.has_grease_pencil() && dst_geo.has_grease_pencil()) {
    using namespace bke::greasepencil;
    const GreasePencil &src_grease_pencil = *src_geo.get_grease_pencil();
    GreasePencil &dst_grease_pencil = *dst_geo.get_grease_pencil_for_write();
    const int src_layer_num = src_grease_pencil.layers().size();
    const int dst_layer_num = dst_grease_pencil.layers().size();
    /* Could also support custom mapping of src to dst layers. */
    const int common_layer_num = std::min(src_layer_num, dst_layer_num);
    for (const int layer_i : IndexRange(common_layer_num)) {
      const Layer &src_layer = src_grease_pencil.layer(layer_i);
      const Drawing *src_drawing = src_grease_pencil.get_eval_drawing(src_layer);
      if (!src_drawing) {
        continue;
      }
      Layer &dst_layer = dst_grease_pencil.layer(layer_i);
      Drawing *dst_drawing = dst_grease_pencil.get_eval_drawing(dst_layer);
      if (!dst_drawing) {
        continue;
      }
      const bke::CurvesGeometry &src_curves = src_drawing->strokes();
      bke::CurvesGeometry &dst_curves = dst_drawing->strokes_for_write();
      const bke::AttributeAccessor src_attributes = src_curves.attributes();
      bke::MutableAttributeAccessor dst_attributes = dst_curves.attributes_for_write();
      success = success |
                transfer_attributes(
                    patterns,
                    ignore_names,
                    src_attributes,
                    dst_attributes,
                    src_id_fields,
                    dst_id_fields,
                    [&](ResourceScope &scope, const bke::AttrDomain domain) -> fn::FieldContext & {
                      return scope.construct<bke::GreasePencilLayerFieldContext>(
                          src_grease_pencil, domain, layer_i);
                    },
                    [&](ResourceScope &scope, const bke::AttrDomain domain) -> fn::FieldContext & {
                      return scope.construct<bke::GreasePencilLayerFieldContext>(
                          dst_grease_pencil, domain, layer_i);
                    });
    }
  }

  params.set_output("Target"_ustr, std::move(dst_geo));
  params.set_output("Success"_ustr, success);
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeTransferAttributes"_ustr);
  ntype.ui_name = "Transfer Attributes";
  ntype.ui_description = "Copy attributes from one geometry to another";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_transfer_attributes_cc
