/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

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

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_default_layout();
  b.add_input<decl::Geometry>("Grease Pencil")
      .supported_type(GeometryComponent::Type::GreasePencil)
      .description("Grease Pencil data to merge layers of");
  b.add_output<decl::Geometry>("Grease Pencil").propagate_all().align_with_previous();
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  auto &group_id = b.add_input<decl::Int>("Group ID")
                       .hide_value()
                       .field_on_all()
                       .make_available([](bNode &node) {
                         node_storage(node).mode = int8_t(MergeLayerMode::ByID);
                       });

  const bNode *node = b.node_or_null();
  if (node) {
    const NodeGeometryMergeLayers &storage = node_storage(*node);
    const MergeLayerMode mode = MergeLayerMode(storage.mode);
    group_id.available(mode == MergeLayerMode::ByID);
  }
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  auto *data = MEM_callocN<NodeGeometryMergeLayers>(__func__);
  data->mode = int8_t(MergeLayerMode::ByName);
  node->storage = data;
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "mode", UI_ITEM_NONE, "", ICON_NONE);
}

static Vector<Vector<int>> get_layers_map_by_name(const GreasePencil &src_grease_pencil,
                                                  const GeoNodeExecParams &params)
{
  using namespace bke::greasepencil;

  const int old_layers_num = src_grease_pencil.layers().size();

  const Field<bool> selection_field = params.get_input<Field<bool>>("Selection");

  bke::GreasePencilFieldContext field_context{src_grease_pencil};
  FieldEvaluator field_evaluator{field_context, old_layers_num};
  field_evaluator.add(selection_field);
  field_evaluator.evaluate();
  const VArray<bool> selection = field_evaluator.get_evaluated<bool>(0);

  Vector<Vector<int>> layers_map;
  Map<StringRef, int> new_layer_index_by_name;

  for (const int layer_i : IndexRange(old_layers_num)) {
    const bool is_selected = selection[layer_i];
    if (!is_selected) {
      layers_map.append({layer_i});
      continue;
    }

    const Layer &layer = src_grease_pencil.layer(layer_i);
    const int new_layer_index = new_layer_index_by_name.lookup_or_add_cb(
        layer.name(), [&]() { return layers_map.append_and_get_index_as(); });
    layers_map[new_layer_index].append(layer_i);
  }
  return layers_map;
}

static Vector<Vector<int>> get_layers_map_by_id(const GreasePencil &src_grease_pencil,
                                                const GeoNodeExecParams &params)
{
  using namespace bke::greasepencil;

  const int old_layers_num = src_grease_pencil.layers().size();

  const Field<bool> selection_field = params.get_input<Field<bool>>("Selection");
  const Field<int> group_id_field = params.get_input<Field<int>>("Group ID");

  bke::GreasePencilFieldContext field_context{src_grease_pencil};
  FieldEvaluator field_evaluator{field_context, old_layers_num};
  field_evaluator.add(selection_field);
  field_evaluator.add(group_id_field);
  field_evaluator.evaluate();
  const VArray<bool> selection = field_evaluator.get_evaluated<bool>(0);
  const VArray<int> group_ids = field_evaluator.get_evaluated<int>(1);

  Vector<Vector<int>> layers_map;
  Map<int, int> new_layer_index_by_id;

  for (const int layer_i : IndexRange(old_layers_num)) {
    const bool is_selected = selection[layer_i];
    if (!is_selected) {
      layers_map.append({layer_i});
      continue;
    }
    const int group_id = group_ids[layer_i];
    const int new_layer_index = new_layer_index_by_id.lookup_or_add_cb(
        group_id, [&]() { return layers_map.append_and_get_index_as(); });
    layers_map[new_layer_index].append(layer_i);
  }
  return layers_map;
}

static void merge_layers(GeometrySet &geometry,
                         const NodeGeometryMergeLayers &storage,
                         const GeoNodeExecParams &params,
                         const AttributeFilter &attribute_filter)
{
  using namespace bke::greasepencil;

  const GreasePencil *src_grease_pencil = geometry.get_grease_pencil();
  if (!src_grease_pencil) {
    return;
  }
  const int old_layers_num = src_grease_pencil->layers().size();

  Vector<Vector<int>> layers_map;
  switch (MergeLayerMode(storage.mode)) {
    case MergeLayerMode::ByName: {
      layers_map = get_layers_map_by_name(*src_grease_pencil, params);
      break;
    }
    case MergeLayerMode::ByID: {
      layers_map = get_layers_map_by_id(*src_grease_pencil, params);
      break;
    }
  }

  const int new_layers_num = layers_map.size();
  if (old_layers_num == new_layers_num) {
    return;
  }

  GreasePencil *new_grease_pencil = geometry::merge_layers(
      *src_grease_pencil, layers_map, attribute_filter);
  geometry.replace_grease_pencil(new_grease_pencil);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet main_geometry = params.extract_input<GeometrySet>("Grease Pencil");
  const bNode &node = params.node();
  const NodeGeometryMergeLayers &storage = node_storage(node);

  const NodeAttributeFilter attribute_filter = params.get_attribute_filter("Grease Pencil");

  main_geometry.modify_geometry_sets(
      [&](GeometrySet &geometry) { merge_layers(geometry, storage, params, attribute_filter); });

  params.set_output("Grease Pencil", std::move(main_geometry));
}

static void node_rna(StructRNA *srna)
{
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
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeMergeLayers", GEO_NODE_MERGE_LAYERS);
  ntype.ui_name = "Merge Layers";
  ntype.ui_description = "Join groups of Grease Pencil layers into one";
  ntype.enum_name_legacy = "MERGE_LAYERS";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.draw_buttons = node_layout;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_type_storage(
      ntype, "NodeGeometryMergeLayers", node_free_standard_storage, node_copy_standard_storage);
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_merge_layers_cc
