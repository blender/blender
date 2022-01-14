/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "node_function_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_fn_input_string_cc {

static void fn_node_input_string_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_output<decl::String>(N_("String"));
}

static void fn_node_input_string_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "string", 0, "", ICON_NONE);
}

static void fn_node_input_string_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  bNode &bnode = builder.node();
  NodeInputString *node_storage = static_cast<NodeInputString *>(bnode.storage);
  std::string string = std::string((node_storage->string) ? node_storage->string : "");
  builder.construct_and_set_matching_fn<fn::CustomMF_Constant<std::string>>(std::move(string));
}

static void fn_node_input_string_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->storage = MEM_callocN(sizeof(NodeInputString), __func__);
}

static void fn_node_input_string_free(bNode *node)
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

static void fn_node_string_copy(bNodeTree *UNUSED(dest_ntree),
                                bNode *dest_node,
                                const bNode *src_node)
{
  NodeInputString *source_storage = (NodeInputString *)src_node->storage;
  NodeInputString *destination_storage = (NodeInputString *)MEM_dupallocN(source_storage);

  if (source_storage->string) {
    destination_storage->string = (char *)MEM_dupallocN(source_storage->string);
  }

  dest_node->storage = destination_storage;
}

}  // namespace blender::nodes::node_fn_input_string_cc

void register_node_type_fn_input_string()
{
  namespace file_ns = blender::nodes::node_fn_input_string_cc;

  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_INPUT_STRING, "String", NODE_CLASS_INPUT);
  ntype.declare = file_ns::fn_node_input_string_declare;
  node_type_init(&ntype, file_ns::fn_node_input_string_init);
  node_type_storage(
      &ntype, "NodeInputString", file_ns::fn_node_input_string_free, file_ns::fn_node_string_copy);
  ntype.build_multi_function = file_ns::fn_node_input_string_build_multi_function;
  ntype.draw_buttons = file_ns::fn_node_input_string_layout;
  nodeRegisterType(&ntype);
}
