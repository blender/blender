/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "BLI_math_vector.h"

#include "RNA_prototypes.hh"

#include "NOD_node_extra_info.hh"
#include "NOD_shader_raycast.hh"
#include "NOD_socket_items_blend.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"

#include "BLO_read_write.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender {

namespace nodes::node_shader_raycast_cc {

NODE_STORAGE_FUNCS(NodeShaderRaycast)

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  const bNodeTree *tree = b.tree_or_null();

  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_default_layout();

  b.add_output<decl::Float>("Is Hit"_ustr);
  b.add_output<decl::Float>("Self Hit"_ustr);
  b.add_output<decl::Float>("Hit Distance"_ustr);
  b.add_output<decl::Vector>("Hit Position"_ustr);
  b.add_output<decl::Vector>("Hit Normal"_ustr);

  b.add_input<decl::Vector>("Position"_ustr).hide_value();
  b.add_input<decl::Vector>("Direction"_ustr).hide_value();
  b.add_input<decl::Float>("Length"_ustr).default_value(1.0);

  auto &panel = b.add_panel("Attributes"_ustr).default_closed(true);
  if (node != nullptr) {
    const NodeShaderRaycast &storage = node_storage(*node);
    for (const NodeRaycastSampleAttributeItem &item :
         Span(storage.sample_attribute_items, storage.sample_attribute_items_num))
    {
      const UString name(item.name);
      const eCustomDataType data_type = eCustomDataType(item.data_type);
      const UString input_identifier(
          RaycastSampleAttributeItemsAccessor::input_socket_identifier_for_item(item));
      const UString output_identifier(
          RaycastSampleAttributeItemsAccessor::output_socket_identifier_for_item(item));
      panel.add_input<decl::String>(name, input_identifier)
          .socket_name_ptr(
              &tree->id, *RaycastSampleAttributeItemsAccessor::item_srna, &item, "name")
          .optional_label();
      panel.add_output(data_type, name, output_identifier).align_with_previous();

      /* In shaders color sockets are RGB. To access color alpha, add a separate output socket,
       * similar to how color access is implemented in the Attribute shader node. */
      if (item.data_type == CD_PROP_COLOR) {
        const UString alpha_name(std::string(name.c_str()) + " Alpha");
        const UString alpha_output_identifier(
            RaycastSampleAttributeItemsAccessor::output_socket_identifier_for_item_alpha(item));
        panel.add_output(CD_PROP_FLOAT, alpha_name, alpha_output_identifier);
      }
    }
  }
  panel.add_input<decl::Extend>(""_ustr, "__extend__"_ustr);
  panel.add_output<decl::Extend>(""_ustr, "__extend__"_ustr).align_with_previous();
}

static void node_shader_init(bNodeTree * /*ntree*/, bNode *node)
{
  NodeShaderRaycast *data = MEM_new<NodeShaderRaycast>(__func__);
  node->storage = data;

  node->custom1 = 0; /* Only Local */
}

static bool node_insert_link(bke::NodeInsertLinkParams &params)
{
  return socket_items::try_add_item_via_any_extend_socket<RaycastSampleAttributeItemsAccessor>(
      params.ntree, params.node, params.node, params.link);
}

static void node_shader_buts(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "only_local", ui::ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

static void node_layout_ex(ui::Layout &layout, bContext *C, PointerRNA *ptr)
{
  bNodeTree &tree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode &node = *static_cast<bNode *>(ptr->data);

  layout.prop(ptr, "only_local", ui::ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);

  if (ui::Layout *panel = layout.panel(
          C, "sample_attribute_items", false, IFACE_("Sample Attributes")))
  {
    socket_items::ui::draw_items_list_with_operators<RaycastSampleAttributeItemsAccessor>(
        C, panel, tree, node);
    socket_items::ui::draw_active_item_props<RaycastSampleAttributeItemsAccessor>(
        tree, node, [&](PointerRNA *item_ptr) {
          panel->use_property_split_set(true);
          panel->use_property_decorate_set(false);
          panel->prop(item_ptr, "data_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
        });
  }
}

static void node_extra_info(NodeExtraInfoParams &parameters)
{
  const Scene *scene = CTX_data_scene(&parameters.C);
  if (StringRef(scene->r.engine) != RE_engine_id_BLENDER_EEVEE) {
    return;
  }

  const NodeShaderRaycast &storage = node_storage(parameters.node);
  if (storage.sample_attribute_items_num == 0) {
    return;
  }

  NodeExtraInfoRow row;
  row.text = RPT_("Attributes Not Supported");
  row.tooltip = TIP_("Accessing attributes is not supported by EEVEE renderer");
  row.icon = ICON_ERROR;
  parameters.rows.append(std::move(row));
}

static void node_operators()
{
  socket_items::ops::make_common_operators<RaycastSampleAttributeItemsAccessor>();
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<RaycastSampleAttributeItemsAccessor>(*node);
  MEM_delete(reinterpret_cast<NodeShaderRaycast *>(node->storage));
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeShaderRaycast &src_storage = node_storage(*src_node);
  NodeShaderRaycast *dst_storage = MEM_new<NodeShaderRaycast>(__func__,
                                                              dna::shallow_copy(src_storage));
  dst_node->storage = dst_storage;

  socket_items::copy_array<RaycastSampleAttributeItemsAccessor>(*src_node, *dst_node);
}

static void node_blend_write(const bNodeTree & /*tree*/, const bNode &node, BlendWriter &writer)
{
  socket_items::blend_write<RaycastSampleAttributeItemsAccessor>(&writer, node);
}

static void node_blend_read(bNodeTree & /*tree*/, bNode &node, BlendDataReader &reader)
{
  socket_items::blend_read_data<RaycastSampleAttributeItemsAccessor>(&reader, node);
}

static int node_shader_gpu(GPUMaterial *mat,
                           bNode *node,
                           bNodeExecData * /*execdata*/,
                           GPUNodeStack *in,
                           GPUNodeStack *out)
{
  GPU_material_flag_set(mat, GPU_MATFLAG_RAYCAST);

  if (!in[0].link) {
    GPU_link(mat, "world_position_get", &in[0].link);
  }

  if (!in[1].link) {
    GPU_link(mat, "world_normals_get", &in[1].link);
  }

  /* GPU raycast node does not support attribute sampling. Relink all attribute outputs to a zero
   * value. */
  const NodeShaderRaycast &storage = node_storage(*node);
  for (const NodeRaycastSampleAttributeItem &item :
       Span(storage.sample_attribute_items, storage.sample_attribute_items_num))
  {
    static const float ZERO[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    const UString output_identifier(
        RaycastSampleAttributeItemsAccessor::output_socket_identifier_for_item(item));
    GPUNodeStack &output = GPU_node_get_output(*node, out, output_identifier.ref());
    GPU_link(mat, "set_value", GPU_constant(ZERO), &output.link);

    if (item.data_type == CD_PROP_COLOR) {
      const UString alpha_output_identifier(
          RaycastSampleAttributeItemsAccessor::output_socket_identifier_for_item_alpha(item));
      GPUNodeStack &alpha_output = GPU_node_get_output(*node, out, alpha_output_identifier.ref());
      GPU_link(mat, "set_value", GPU_constant(ZERO), &alpha_output.link);
    }
  }

  const bool only_local = node->custom1;
  const char *shader_name = only_local ? "node_raycast_only_local" : "node_raycast";

  /* Explicitly link all sockets that are expected by the shader.
   * Do not rely on the stack linking as it will attempt to link dynamically added attribute
   * sockets. */
  return GPU_link(mat,
                  shader_name,
                  GPU_node_get_input_link(*node, in, "Position"),
                  GPU_node_get_input_link(*node, in, "Direction"),
                  GPU_node_get_input_link(*node, in, "Length"),
                  &GPU_node_get_output(*node, out, "Is Hit").link,
                  &GPU_node_get_output(*node, out, "Self Hit").link,
                  &GPU_node_get_output(*node, out, "Hit Distance").link,
                  &GPU_node_get_output(*node, out, "Hit Position").link,
                  &GPU_node_get_output(*node, out, "Hit Normal").link);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  return get_output_default(socket_out_->identifier, NodeItem::Type::Any);
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace nodes::node_shader_raycast_cc

/* node type definition */
void register_node_type_sh_raycast()
{
  namespace file_ns = blender::nodes::node_shader_raycast_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeRaycast"_ustr, SH_NODE_RAYCAST);
  ntype.ui_name = "Raycast";
  ntype.ui_description = "Cast rays and retrieve information from the hit point";
  ntype.enum_name_legacy = "MATERIAL_RAYCAST";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.initfunc = file_ns::node_shader_init;
  ntype.add_ui_poll = object_shader_nodes_poll;
  ntype.insert_link = file_ns::node_insert_link;
  ntype.draw_buttons = file_ns::node_shader_buts;
  ntype.draw_buttons_ex = file_ns::node_layout_ex;
  ntype.register_operators = file_ns::node_operators;
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu;
  ntype.materialx_fn = file_ns::node_shader_materialx;
  ntype.get_extra_info = file_ns::node_extra_info;
  ntype.blend_write_storage_content = file_ns::node_blend_write;
  ntype.blend_data_read_storage_content = file_ns::node_blend_read;

  bke::node_type_storage(
      ntype, "NodeShaderRaycast", file_ns::node_free_storage, file_ns::node_copy_storage);

  blender::bke::node_register_type(ntype);
}

namespace nodes {

StructRNA **RaycastSampleAttributeItemsAccessor::item_srna = &RNA_NodeRaycastSampleAttributeItem;

void RaycastSampleAttributeItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  writer->write_string(item.name);
}

void RaycastSampleAttributeItemsAccessor::blend_read_data_item(BlendDataReader *reader,
                                                               ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

}  // namespace nodes

}  // namespace blender
