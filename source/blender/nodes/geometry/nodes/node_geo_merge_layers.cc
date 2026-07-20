/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "GEO_foreach_geometry.hh"
#include "GEO_merge_layers.hh"

#include "BKE_grease_pencil.hh"

#include "NOD_rna_define.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_geo_merge_layers_cc {

NODE_STORAGE_FUNCS(NodeGeometryMergeLayers);

enum class MergeLayerMode {
  ByName = 0,
  ByID = 1,
};

static const EnumPropertyItem mode_items[] = {
    {int(MergeLayerMode::ByName),
     "MERGE_BY_NAME",
     0,
     "By Name",
     "Combine all layers which have the same name"},
    {int(MergeLayerMode::ByID),
     "MERGE_BY_ID",
     0,
     "By Group ID",
     "Provide a custom group ID for each layer and all layers with the same ID will be merged "
     "into one"},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Geometry>("Grease Pencil"_ustr)
      .supported_type(GeometryComponent::Type::GreasePencil)
      .description("Grease Pencil data to merge layers of");
  b.add_output<decl::Geometry>("Grease Pencil"_ustr)
      .propagate_all_geometry()
      .align_with_previous();
  b.add_input<decl::Bool>("Selection"_ustr)
      .default_value(true)
      .hide_value()
      .evaluated_geometry_field();
  b.add_input<decl::Menu>("Mode"_ustr).static_items(mode_items).optional_label();
  b.add_input<decl::Int>("Group ID"_ustr)
      .hide_value()
      .evaluated_geometry_field()
      .usage_by_single_menu(int(MergeLayerMode::ByID));
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  /* Still used for forward compatibility. */
  node->storage = MEM_new<NodeGeometryMergeLayers>(__func__);
}

static GreasePencil *merge_by_name(const GreasePencil &src_grease_pencil,
                                   const GeoNodeExecParams &params,
                                   const AttributeFilter &attribute_filter)
{
  using namespace bke::greasepencil;
  const Field<bool> selection_field = params.get_input<Field<bool>>("Selection"_ustr);

  bke::GreasePencilFieldContext field_context{src_grease_pencil};
  FieldEvaluator field_evaluator{field_context, src_grease_pencil.layers().size()};
  field_evaluator.add(selection_field);
  field_evaluator.evaluate();
  return geometry::merge_layers_by_name(
      src_grease_pencil, field_evaluator.get_evaluated<bool>(0), attribute_filter);
}

static GroupedSpan<int> get_layers_map_by_id(const GreasePencil &src_grease_pencil,
                                             const GeoNodeExecParams &params,
                                             Array<int> &r_offsets,
                                             Array<int> &r_indices)
{
  using namespace bke::greasepencil;

  const int old_layers_num = src_grease_pencil.layers().size();

  const Field<bool> selection_field = params.get_input<Field<bool>>("Selection"_ustr);
  const Field<int> group_id_field = params.get_input<Field<int>>("Group ID"_ustr);

  bke::GreasePencilFieldContext field_context{src_grease_pencil};
  FieldEvaluator field_evaluator{field_context, old_layers_num};
  field_evaluator.add(selection_field);
  field_evaluator.add(group_id_field);
  field_evaluator.evaluate();
  const VArray<bool> selection = field_evaluator.get_evaluated<bool>(0);
  const VArray<int> group_ids = field_evaluator.get_evaluated<int>(1);

  Array<int> layer_to_group(old_layers_num);
  Map<int, int> id_to_group_index;
  int groups_num = 0;
  for (const int i : IndexRange(old_layers_num)) {
    if (selection[i]) {
      layer_to_group[i] = id_to_group_index.lookup_or_add_cb(group_ids[i],
                                                             [&]() { return groups_num++; });
    }
    else {
      layer_to_group[i] = groups_num++;
    }
  }
  return offset_indices::build_groups_from_indices(
      layer_to_group, groups_num, r_offsets, r_indices);
}

static void merge_layers(GeometrySet &geometry,
                         const GeoNodeExecParams &params,
                         const AttributeFilter &attribute_filter)
{
  using namespace bke::greasepencil;

  const GreasePencil *src_grease_pencil = geometry.get_grease_pencil();
  if (!src_grease_pencil) {
    return;
  }
  const int old_layers_num = src_grease_pencil->layers().size();

  GreasePencil *new_grease_pencil;
  switch (MergeLayerMode(params.get_input<MergeLayerMode>("Mode"_ustr))) {
    case MergeLayerMode::ByName: {
      new_grease_pencil = merge_by_name(*src_grease_pencil, params, attribute_filter);
      break;
    }
    case MergeLayerMode::ByID: {
      Array<int> offsets;
      Array<int> all_indices;
      const GroupedSpan<int> layers_map = get_layers_map_by_id(
          *src_grease_pencil, params, offsets, all_indices);
      const int new_layers_num = layers_map.size();
      if (old_layers_num == new_layers_num) {
        return;
      }
      new_grease_pencil = geometry::merge_layers(*src_grease_pencil, layers_map, attribute_filter);
      break;
    }
  }

  geometry.replace_grease_pencil(new_grease_pencil);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet main_geometry = params.extract_input<GeometrySet>("Grease Pencil"_ustr);
  const NodeAttributeFilter attribute_filter = params.get_attribute_filter("Grease Pencil"_ustr);

  geometry::foreach_real_geometry(main_geometry, [&](GeometrySet &geometry) {
    merge_layers(geometry, params, attribute_filter);
  });

  params.set_output("Grease Pencil"_ustr, std::move(main_geometry));
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "mode",
                    "Mode",
                    "Determines how to choose which layers are merged",
                    mode_items,
                    NOD_storage_enum_accessors(mode),
                    int(MergeLayerMode::ByName),
                    nullptr);
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeMergeLayers"_ustr, GEO_NODE_MERGE_LAYERS);
  ntype.ui_name = "Merge Layers";
  ntype.ui_description = "Join groups of Grease Pencil layers into one";
  ntype.enum_name_legacy = "MERGE_LAYERS";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_type_storage(
      ntype, "NodeGeometryMergeLayers", node_free_standard_storage, node_copy_standard_storage);
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_merge_layers_cc
