/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "NOD_sh_zones.hh"
#include "NOD_socket.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"

#include "BLO_read_write.hh"

#include "BLI_string_utils.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "BKE_screen.hh"

#include "WM_api.hh"

#include "UI_interface.hh"

#include "node_shader_util.hh"
#include "node_util.hh"

namespace blender::nodes::node_shader_foreach_light_cc {

/** Shared between zone input and output node. */
static void node_layout_ex(uiLayout *layout, bContext *C, PointerRNA *current_node_ptr)
{
  bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(current_node_ptr->owner_id);
  bNode *current_node = static_cast<bNode *>(current_node_ptr->data);

  const bke::bNodeTreeZones *zones = ntree.zones();
  if (!zones) {
    return;
  }
  const bke::bNodeTreeZone *zone = zones->get_zone_by_node(current_node->identifier);
  if (!zone) {
    return;
  }
  if (!zone->output_node) {
    return;
  }
  bNode &output_node = const_cast<bNode &>(*zone->output_node);
  PointerRNA output_node_ptr = RNA_pointer_create_discrete(
      current_node_ptr->owner_id, &RNA_Node, &output_node);

  if (uiLayout *panel = uiLayoutPanel(
          C, layout, "foreach_light_items", false, TIP_("For Each Light Items")))
  {
    socket_items::ui::draw_items_list_with_operators<ShForeachLightItemsAccessor>(
        C, panel, ntree, output_node);
    socket_items::ui::draw_active_item_props<ShForeachLightItemsAccessor>(
        ntree, output_node, [&](PointerRNA *item_ptr) {
          uiLayoutSetPropSep(panel, true);
          uiLayoutSetPropDecorate(panel, false);
          uiItemR(panel, item_ptr, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
        });
  }
}

namespace foreach_light_input_node {

NODE_STORAGE_FUNCS(NodeShaderForeachLightInput);

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Vector>("Normal").hide_value();
  b.add_output<decl::Color>("Color");
  b.add_output<decl::Vector>("Direction");
  b.add_output<decl::Float>("Distance");
  b.add_output<decl::Float>("Attenuation");
  b.add_output<decl::Float>("Shadow Mask");

  const bNode *node = b.node_or_null();
  const bNodeTree *tree = b.tree_or_null();
  if (node && tree) {
    const NodeShaderForeachLightInput &storage = node_storage(*node);
    const bNode *output_node = tree->node_by_id(storage.output_node_id);
    if (output_node) {
      const auto &output_storage = *static_cast<const NodeShaderForeachLightOutput *>(
          output_node->storage);
      for (const int i : IndexRange(output_storage.items_num)) {
        const NodeShaderForeachLightItem &item = output_storage.items[i];
        const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
        const StringRef name = item.name ? item.name : "";
        const std::string identifier = ShForeachLightItemsAccessor::socket_identifier_for_item(
            item);
        if (socket_type == SOCK_RGBA) {
          /* Make the color items black by default. */
          b.add_input<decl::Color>(name, identifier)
              .default_value(ColorGeometry4f(0.0f, 0.0f, 0.0f, 1.0f))
              .socket_name_ptr(&tree->id, ShForeachLightItemsAccessor::item_srna, &item, "name");
        }
        else {
          b.add_input(socket_type, name, identifier)
              .socket_name_ptr(&tree->id, ShForeachLightItemsAccessor::item_srna, &item, "name");
        }
        b.add_output(socket_type, name, identifier).align_with_previous();
      }
    }
  }
  b.add_input<decl::Extend>("", "__extend__");
  b.add_output<decl::Extend>("", "__extend__").align_with_previous();
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeShaderForeachLightInput *data = MEM_cnew<NodeShaderForeachLightInput>(__func__);
  /* Needs to be initialized for the node to work. */
  data->output_node_id = 0;
  node->storage = data;
}

static void node_label(const bNodeTree * /*ntree*/,
                       const bNode * /*node*/,
                       char *label,
                       const int label_maxncpy)
{
  BLI_strncpy_utf8(label, IFACE_("For Each Light"), label_maxncpy);
}

static bool node_insert_link(bNodeTree *ntree, bNode *node, bNodeLink *link)
{
  bNode *output_node = ntree->node_by_id(node_storage(*node).output_node_id);
  if (!output_node) {
    return true;
  }
  return socket_items::try_add_item_via_any_extend_socket<ShForeachLightItemsAccessor>(
      *ntree, *node, *output_node, *link);
}

static int node_shader_fn(GPUMaterial *mat,
                          bNode *node,
                          bNodeExecData * /*execdata*/,
                          GPUNodeStack *in,
                          GPUNodeStack *out)
{
  if (!in[0].link) {
    GPU_link(mat, "world_normals_get", &in[0].link);
  }

  int zone_id = ((NodeShaderForeachLightInput *)node->storage)->output_node_id;
  return GPU_stack_link_zone(mat, node, "FOREACH_LIGHT_BEGIN", in, out, zone_id, false, 1, 5);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  sh_node_type_base(&ntype, "ShaderNodeForeachLightInput", SH_NODE_FOREACH_LIGHT_INPUT);
  ntype.enum_name_legacy = "FOREACH_LIGHT_INPUT";
  ntype.ui_name = "For Each Light Output";
  ntype.nclass = NODE_CLASS_INTERFACE;
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.labelfunc = node_label;
  ntype.gather_link_search_ops = nullptr;
  ntype.insert_link = node_insert_link;
  ntype.no_muting = true;
  ntype.draw_buttons_ex = node_layout_ex;
  blender::bke::node_type_storage(&ntype,
                                  "NodeShaderForeachLightInput",
                                  node_free_standard_storage,
                                  node_copy_standard_storage);
  ntype.gpu_fn = node_shader_fn;
  blender::bke::node_register_type(&ntype);
}
// NOD_REGISTER_NODE(node_register)

}  // namespace foreach_light_input_node

namespace foreach_light_output_node {

NODE_STORAGE_FUNCS(NodeShaderForeachLightOutput);

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  const bNodeTree *tree = b.tree_or_null();
  const bNode *node = b.node_or_null();
  if (node) {
    const NodeShaderForeachLightOutput &storage = node_storage(*node);
    for (const int i : IndexRange(storage.items_num)) {
      const NodeShaderForeachLightItem &item = storage.items[i];
      const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
      const StringRef name = item.name ? item.name : "";
      const std::string identifier = ShForeachLightItemsAccessor::socket_identifier_for_item(item);
      b.add_input(socket_type, name, identifier)
          .socket_name_ptr(&tree->id, ShForeachLightItemsAccessor::item_srna, &item, "name")
          .hide_value();
      b.add_output(socket_type, name, identifier).align_with_previous();
    }
  }
  b.add_input<decl::Extend>("", "__extend__");
  b.add_output<decl::Extend>("", "__extend__").align_with_previous();
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeShaderForeachLightOutput *data = MEM_cnew<NodeShaderForeachLightOutput>(__func__);

  data->next_identifier = 0;

  data->items = MEM_cnew_array<NodeShaderForeachLightItem>(1, __func__);
  data->items[0].name = BLI_strdup(DATA_("Zone IO"));
  data->items[0].socket_type = SOCK_RGBA;
  data->items[0].identifier = data->next_identifier++;
  data->items_num = 1;

  node->storage = data;
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<ShForeachLightItemsAccessor>(*node);
  MEM_freeN(node->storage);
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeShaderForeachLightOutput &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_cnew<NodeShaderForeachLightOutput>(__func__, src_storage);
  dst_node->storage = dst_storage;

  socket_items::copy_array<ShForeachLightItemsAccessor>(*src_node, *dst_node);
}

static bool node_insert_link(bNodeTree *ntree, bNode *node, bNodeLink *link)
{
  return socket_items::try_add_item_via_any_extend_socket<ShForeachLightItemsAccessor>(
      *ntree, *node, *node, *link);
}

static void node_operators()
{
  socket_items::ops::make_common_operators<ShForeachLightItemsAccessor>();
}

static int node_shader_fn(GPUMaterial *mat,
                          bNode *node,
                          bNodeExecData * /*execdata*/,
                          GPUNodeStack *in,
                          GPUNodeStack *out)
{
  int zone_id = node->identifier;
  return GPU_stack_link_zone(mat, node, "FOREACH_LIGHT_END", in, out, zone_id, true, 0, 0);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  sh_node_type_base(&ntype, "ShaderNodeForeachLightOutput", SH_NODE_FOREACH_LIGHT_OUTPUT);
  ntype.enum_name_legacy = "FOREACH_LIGHT_OUTPUT";
  ntype.ui_name = "For Each Light Output";
  ntype.nclass = NODE_CLASS_INTERFACE;
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.labelfunc = foreach_light_input_node::node_label;
  ntype.insert_link = node_insert_link;
  ntype.no_muting = true;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.register_operators = node_operators;
  blender::bke::node_type_storage(
      &ntype, "NodeShaderForeachLightOutput", node_free_storage, node_copy_storage);
  ntype.gpu_fn = node_shader_fn;
  blender::bke::node_register_type(&ntype);
}
// NOD_REGISTER_NODE(node_register)

}  // namespace foreach_light_output_node

}  // namespace blender::nodes::node_shader_foreach_light_cc

namespace blender::nodes {

StructRNA *ShForeachLightItemsAccessor::item_srna = &RNA_ShaderForeachLightItem;
int ShForeachLightItemsAccessor::node_type = SH_NODE_FOREACH_LIGHT_OUTPUT;
int ShForeachLightItemsAccessor::item_dna_type = SDNA_TYPE_FROM_STRUCT(NodeShaderForeachLightItem);

void ShForeachLightItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  BLO_write_string(writer, item.name);
}

void ShForeachLightItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

}  // namespace blender::nodes

blender::Span<NodeShaderForeachLightItem> NodeShaderForeachLightOutput::items_span() const
{
  return blender::Span<NodeShaderForeachLightItem>(items, items_num);
}

blender::MutableSpan<NodeShaderForeachLightItem> NodeShaderForeachLightOutput::items_span()
{
  return blender::MutableSpan<NodeShaderForeachLightItem>(items, items_num);
}

void register_node_type_sh_foreach_light()
{
  blender::nodes::node_shader_foreach_light_cc::foreach_light_input_node::node_register();
  blender::nodes::node_shader_foreach_light_cc::foreach_light_output_node::node_register();
}
