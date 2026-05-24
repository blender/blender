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
  b.add_output<decl::Geometry>("Target"_ustr).align_with_previous().propagate_all_geometry();
  b.add_output<decl::String>("Transferred Names"_ustr)
      .structure_type(StructureType::List)
      .description("Attribute names that have been transferred excluding internal attributes");
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
      socket->default_input_type(NODE_DEFAULT_INPUT_INDEX_FIELD);
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
      socket->default_input_type(NODE_DEFAULT_INPUT_INDEX_FIELD);
      socket->structure_type(StructureType::Field);
    }
  }

  b.add_input<decl::Menu>("Pattern Mode"_ustr)
      .static_items(string_pattern_mode_items)
      .default_value(StringPatternMode::Wildcard)
      .optional_label();
  b.add_input<decl::String>("Attribute Names"_ustr)
      .optional_label()
      .structure_type(StructureType::List)
      .description(
          "List of attribute names (not) to transfer. A wildcard (*) at the end is allowed");
  b.add_input<decl::Bool>("Exclude Names"_ustr)
      .default_value(false)
      .description("Transfer all attributes except the ones matching any of the names");
}

static bool should_transfer(const Span<StringPattern> patterns,
                            const StringRef name,
                            const bool exclude_names,
                            MutableSpan<bool> r_found_attribute_using_pattern)
{
  if (ELEM(name, ".corner_vert", ".corner_edge", ".edge_verts")) {
    return false;
  }
  bool match_found = false;
  for (const int pattern_i : patterns.index_range()) {
    const StringPattern &pattern = patterns[pattern_i];
    if (pattern.match(name)) {
      match_found = true;
      r_found_attribute_using_pattern[pattern_i] = true;
    }
  }
  if (exclude_names) {
    return !match_found;
  }
  return match_found;
}

static void transfer_attributes(
    const Span<StringPattern> patterns,
    const bool exclude_names,
    const bke::AttributeAccessor &src_attributes,
    bke::MutableAttributeAccessor &dst_attributes,
    const Map<bke::AttrDomain, Field<int>> &src_id_fields,
    const Map<bke::AttrDomain, Field<int>> &dst_id_fields,
    FunctionRef<fn::FieldContext &(ResourceScope &scope, const bke::AttrDomain domain)>
        create_src_context,
    FunctionRef<fn::FieldContext &(ResourceScope &scope, const bke::AttrDomain domain)>
        create_dst_context,
    VectorSet<std::string> &r_transferred_names,
    MutableSpan<bool> r_found_attribute_using_pattern)
{
  struct AttrItem {
    StringRef name;
    AttrDomain domain;
    bke::AttrType type;
  };
  struct IDs {
    bool transfer_by_index = false;
    Array<int> src_by_dst_index;
    IndexMask gather_mask;
    IndexMask default_mask;
  };
  Map<bke::AttrDomain, IDs> ids_by_domain;
  Vector<AttrItem> items;
  src_attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (should_transfer(patterns, iter.name, exclude_names, r_found_attribute_using_pattern)) {
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
    ids.gather_mask = array_utils::indices_non_negative(
        IndexMask(dst_size), ids.src_by_dst_index, scope.allocator());
    ids.default_mask = ids.gather_mask.complement(IndexMask(dst_size), scope.allocator());
  }

  for (const AttrItem &item : items) {
    const bke::GAttributeReader src_attr = src_attributes.lookup(item.name);
    const CommonVArrayInfo info = src_attr.varray.common_info();
    const IDs &ids = ids_by_domain.lookup(item.domain);
    const CPPType &type = src_attr.varray.type();

    const int src_size = src_attr.varray.size();
    const int dst_size = dst_attributes.domain_size(item.domain);

    const std::optional<bke::AttributeMetaData> old_dst_meta = dst_attributes.lookup_meta_data(
        item.name);
    const bool has_matching_existing_attribute = old_dst_meta.has_value() &&
                                                 old_dst_meta->domain == item.domain &&
                                                 old_dst_meta->data_type == item.type;

    if (!type.is_trivial) {
      /* Non-trivial types are disabled for now because the behavior of the gather function
       * regarding uninitialized data needs to be more well defined. */
      continue;
    }

    /* When the source and destination ids are just the index field transfers can be more
     * efficient. */
    if (ids.transfer_by_index) {
      /* Try to store the destination attribute as single value. */
      if (info.type == CommonVArrayInfo::Type::Single) {
        if (src_size >= dst_size) {
          if (dst_attributes.add_override(
                  item.name, item.domain, item.type, bke::AttributeInitValue({type, info.data})))
          {
            r_transferred_names.add(item.name);
            continue;
          }
        }
      }

      /* Try to share the data with an existing attribute. */
      if (src_attr.sharing_info && info.type == CommonVArrayInfo::Type::Span &&
          src_size == dst_size)
      {
        if (dst_attributes.add_override(
                item.name,
                item.domain,
                item.type,
                bke::AttributeInitShared(info.data, *src_attr.sharing_info)))
        {
          r_transferred_names.add(item.name);
          continue;
        }
      }

      const int copy_num = std::min(src_size, dst_size);
      const IndexRange copy_slice(copy_num);

      /* Values of an existing attribute need to be kept unless they are transferred. */
      if (old_dst_meta.has_value()) {
        if (has_matching_existing_attribute) {
          /* Just copy the new data to the start of the existing attribute, without changing the
           * values at larger indices. */
          bke::GSpanAttributeWriter dst_attr = dst_attributes.lookup_for_write_span(item.name);
          BLI_assert(dst_attr);
          array_utils::copy(src_attr.varray.slice(copy_slice), dst_attr.span.slice(copy_slice));
          dst_attr.finish();
          r_transferred_names.add(item.name);
          continue;
        }
        /* Create a new array for the correct domain and type, copy the transferred values and
         * leave the rest at the old value. */
        const bke::GAttributeReader adapted_old_dst = dst_attributes.lookup(
            item.name, item.domain, item.type);
        if (!adapted_old_dst) {
          continue;
        }
        void *dst_data = MEM_new_array_uninitialized_aligned(
            dst_size, type.size, type.alignment, __func__);
        src_attr.varray.materialize_to_uninitialized(copy_slice, dst_data);
        adapted_old_dst.varray.materialize_to_uninitialized(
            IndexRange(dst_size).drop_front(copy_num), dst_data);
        if (dst_attributes.add_override(
                item.name, item.domain, item.type, bke::AttributeInitMoveArray(dst_data)))
        {
          r_transferred_names.add(item.name);
          continue;
        }
        /* Transfer failed. */
        type.destruct_n(dst_data, dst_size);
        MEM_delete_void(dst_data);
        continue;
      }

      /* Create a new array, copy the first few elements and fill the rest with the defaults. */
      void *dst_data = MEM_new_array_uninitialized_aligned(
          dst_size, type.size, type.alignment, __func__);
      GMutableSpan dst(type, dst_data, dst_size);
      array_utils::copy(src_attr.varray.slice(copy_slice), dst.slice(copy_slice));
      type.fill_construct_indices(
          type.default_value(), dst_data, IndexRange(dst_size).drop_front(copy_num));
      if (dst_attributes.add(
              item.name, item.domain, item.type, bke::AttributeInitMoveArray(dst_data)))
      {
        r_transferred_names.add(item.name);
        continue;
      }
      /* Transfer failed. */
      type.destruct_n(dst_data, dst_size);
      MEM_delete_void(dst_data);
      continue;
    }

    /* If all indices are transferred, the old attribute can be ignored. */
    if (ids.gather_mask.size() == dst_size) {
      /* The dst attribute can be a single value when the source is a single value since each
       * element is copied from the source. */
      if (info.type == CommonVArrayInfo::Type::Single) {
        if (dst_attributes.add_override(
                item.name, item.domain, item.type, bke::AttributeInitValue({type, info.data})))
        {
          r_transferred_names.add(item.name);
          continue;
        }
      }
      /* Try writing into an existing attribute buffer. */
      if (has_matching_existing_attribute) {
        bke::GSpanAttributeWriter dst_attr = dst_attributes.lookup_or_add_for_write_only_span(
            item.name, item.domain, item.type);
        BLI_assert(dst_attr);
        bke::attribute_math::gather(*src_attr, ids.src_by_dst_index, dst_attr.span);
        dst_attr.finish();
        r_transferred_names.add(item.name);
        continue;
      }
      /* Create a new array for the attribute. */
      void *dst_data = MEM_new_array_uninitialized_aligned(
          dst_size, type.size, type.alignment, __func__);
      bke::attribute_math::gather(*src_attr, ids.src_by_dst_index, {type, dst_data, dst_size});
      if (dst_attributes.add_override(
              item.name, item.domain, item.type, bke::AttributeInitMoveArray(dst_data)))
      {
        r_transferred_names.add(item.name);
        continue;
      }
      /* Transfer failed. */
      type.destruct_n(dst_data, dst_size);
      MEM_delete_void(dst_data);
      continue;
    }

    /* A subset of the indices are transferred, first try to write them into an existing array. */
    if (has_matching_existing_attribute) {
      bke::GSpanAttributeWriter dst_attr = dst_attributes.lookup_or_add_for_write_span(
          item.name, item.domain, item.type);
      BLI_assert(dst_attr);
      bke::attribute_math::gather(*src_attr, ids.src_by_dst_index, ids.gather_mask, dst_attr.span);
      dst_attr.finish();
      r_transferred_names.add(item.name);
      continue;
    }
    if (!old_dst_meta) {
      void *dst_data = MEM_new_array_uninitialized_aligned(
          dst_size, type.size, type.alignment, __func__);
      bke::attribute_math::gather(
          *src_attr, ids.src_by_dst_index, ids.gather_mask, {type, dst_data, dst_size});
      type.fill_construct_indices(type.default_value(), dst_data, ids.default_mask);
      if (dst_attributes.add(
              item.name, item.domain, item.type, bke::AttributeInitMoveArray(dst_data)))
      {
        r_transferred_names.add(item.name);
        continue;
      }
      /* Transfer failed. */
      type.destruct_n(dst_data, dst_size);
      MEM_delete_void(dst_data);
      continue;
    }
    const bke::GAttributeReader adapted_old_dst = dst_attributes.lookup(
        item.name, item.domain, item.type);
    if (!adapted_old_dst) {
      continue;
    }
    void *dst_data = MEM_new_array_uninitialized_aligned(
        dst_size, type.size, type.alignment, __func__);
    bke::attribute_math::gather(
        src_attr.varray, ids.src_by_dst_index, ids.gather_mask, {type, dst_data, dst_size});
    adapted_old_dst.varray.materialize_to_uninitialized(ids.default_mask, dst_data);
    if (dst_attributes.add_override(
            item.name, item.domain, item.type, bke::AttributeInitMoveArray(dst_data)))
    {
      r_transferred_names.add(item.name);
      continue;
    }
    /* Transfer failed. */
    type.destruct_n(dst_data, dst_size);
    MEM_delete_void(dst_data);
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet dst_geo = params.extract_input<GeometrySet>("Target"_ustr);
  GeometrySet src_geo = params.extract_input<GeometrySet>("Source"_ustr);
  const StringPatternMode pattern_mode = params.extract_input<StringPatternMode>(
      "Pattern Mode"_ustr);
  const GListPtr attribute_patterns_list = params.extract_input<GListPtr>("Attribute Names"_ustr);
  const bool exclude_names = params.extract_input<bool>("Exclude Names"_ustr);

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
        if (std::optional<StringPattern> pattern_fn = StringPattern::from_str(
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
    params.set_output("Transferred Names"_ustr, GList::from_container(Array<std::string>()));
    return;
  }

  VectorSet<std::string> transferred_names;
  Array<bool> found_attribute_using_pattern(patterns.size(), false);
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
    transfer_attributes(
        patterns,
        exclude_names,
        src_attributes,
        dst_attributes,
        src_id_fields,
        dst_id_fields,
        [&](ResourceScope &scope, const bke::AttrDomain domain) -> fn::FieldContext & {
          return scope.construct<bke::GeometryFieldContext>(src_component, domain);
        },
        [&](ResourceScope &scope, const bke::AttrDomain domain) -> fn::FieldContext & {
          return scope.construct<bke::GeometryFieldContext>(dst_component, domain);
        },
        transferred_names,
        found_attribute_using_pattern);
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
      transfer_attributes(
          patterns,
          exclude_names,
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
          },
          transferred_names,
          found_attribute_using_pattern);
    }
  }

  transferred_names.remove_if([&](const StringRef name) { return name.startswith("."); });

  if (!exclude_names) {
    for (const int pattern_i : patterns.index_range()) {
      const StringRef full_pattern = patterns[pattern_i].full_pattern();
      if (full_pattern.is_empty()) {
        continue;
      }
      if (found_attribute_using_pattern[pattern_i]) {
        continue;
      }
      params.error_message_add(NodeWarningType::Info,
                               fmt::format("{} \"{}\"",
                                           TIP_("No attribute found matching"),
                                           patterns[pattern_i].full_pattern()));
    }
  }

  params.set_output("Target"_ustr, std::move(dst_geo));
  params.set_output("Transferred Names"_ustr,
                    GList::from_container(transferred_names.extract_vector()));
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
  ntype.default_width = bke::NodeWidth::_160;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_transfer_attributes_cc
