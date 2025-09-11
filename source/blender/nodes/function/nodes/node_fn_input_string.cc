/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_function_util.hh"

#include "NOD_socket_search_link.hh"

#include "BLT_translation.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BLO_read_write.hh"

#include "BLF_api.hh"

namespace blender::nodes::node_fn_input_string_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_output<decl::String>("String").custom_draw([](CustomSocketDrawParams &params) {
    params.layout.alignment_set(ui::LayoutAlign::Expand);
    PropertyRNA *prop = RNA_struct_find_property(&params.node_ptr, "string");
    params.layout.prop(&params.node_ptr,
                       prop,
                       -1,
                       0,
                       UI_ITEM_R_SPLIT_EMPTY_NAME,
                       "",
                       ICON_NONE,
                       IFACE_("String"));
  });
}

static void node_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const bNode &bnode = builder.node();
  NodeInputString *node_storage = static_cast<NodeInputString *>(bnode.storage);
  std::string string = std::string((node_storage->string) ? node_storage->string : "");
  builder.construct_and_set_matching_fn<mf::CustomMF_Constant<std::string>>(std::move(string));
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->storage = MEM_callocN<NodeInputString>(__func__);
}

static void node_storage_free(bNode *node)
{
  NodeInputString *storage = (NodeInputString *)node->storage;
  if (storage == nullptr) {
    return;
  }
  if (storage->string != nullptr) {
    MEM_freeN(storage->string);
  }
  MEM_freeN(storage);
}

static void node_storage_copy(bNodeTree * /*dst_ntree*/, bNode *dest_node, const bNode *src_node)
{
  NodeInputString *source_storage = (NodeInputString *)src_node->storage;
  NodeInputString *destination_storage = (NodeInputString *)MEM_dupallocN(source_storage);

  if (source_storage->string) {
    destination_storage->string = (char *)MEM_dupallocN(source_storage->string);
  }

  dest_node->storage = destination_storage;
}

static void node_blend_write(const bNodeTree & /*tree*/, const bNode &node, BlendWriter &writer)
{
  const NodeInputString *storage = static_cast<const NodeInputString *>(node.storage);
  BLO_write_string(&writer, storage->string);
}

static void node_blend_read(bNodeTree & /*tree*/, bNode &node, BlendDataReader &reader)
{
  NodeInputString *storage = static_cast<NodeInputString *>(node.storage);
  BLO_read_string(&reader, &storage->string);
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const eNodeSocketDatatype type = eNodeSocketDatatype(params.other_socket().type);
  if (type != SOCK_STRING) {
    return;
  }
  if (params.other_socket().in_out == SOCK_OUT) {
    return;
  }

  params.add_item(IFACE_("String"), [](LinkSearchOpParams &params) {
    bNode &node = params.add_node("FunctionNodeInputString");
    params.update_and_connect_available_socket(node, "String");

    /* Adapt width of the new node to its content. */
    const StringRef string = static_cast<NodeInputString *>(node.storage)->string;
    const uiFontStyle &fstyle = UI_style_get()->widget;
    BLF_size(fstyle.uifont_id, fstyle.points);
    const float width = BLF_width(fstyle.uifont_id, string.data(), string.size()) + 40.0f;
    node.width = std::clamp(width, 140.0f, 1000.0f);
  });
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  fn_node_type_base(&ntype, "FunctionNodeInputString", FN_NODE_INPUT_STRING);
  ntype.ui_name = "String";
  ntype.ui_description = "Provide a string value that can be connected to other nodes in the tree";
  ntype.enum_name_legacy = "INPUT_STRING";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  blender::bke::node_type_storage(ntype, "NodeInputString", node_storage_free, node_storage_copy);
  ntype.build_multi_function = node_build_multi_function;
  ntype.blend_write_storage_content = node_blend_write;
  ntype.blend_data_read_storage_content = node_blend_read;
  ntype.gather_link_search_ops = node_gather_link_searches;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_fn_input_string_cc
