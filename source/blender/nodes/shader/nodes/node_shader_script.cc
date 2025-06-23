/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_script_cc {

static void node_shader_buts_script(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *row;

  row = &layout->row(false);
  row->prop(ptr, "mode", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);

  row = &layout->row(true);

  if (RNA_enum_get(ptr, "mode") == NODE_SCRIPT_INTERNAL) {
    row->prop(ptr, "script", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  }
  else {
    row->prop(ptr, "filepath", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  }

  row->op("node.shader_script_update", "", ICON_FILE_REFRESH);
}

static void node_shader_buts_script_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  layout->separator();

  node_shader_buts_script(layout, C, ptr);

#if 0 /* not implemented yet */
  if (RNA_enum_get(ptr, "mode") == NODE_SCRIPT_EXTERNAL) {
    layout->prop(ptr, "use_auto_update", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  }
#endif
}

static void init(bNodeTree * /*ntree*/, bNode *node)
{
  NodeShaderScript *nss = MEM_callocN<NodeShaderScript>("shader script node");
  node->storage = nss;
}

static void node_free_script(bNode *node)
{
  NodeShaderScript *nss = static_cast<NodeShaderScript *>(node->storage);

  if (nss) {
    if (nss->bytecode) {
      MEM_freeN(nss->bytecode);
    }

    MEM_freeN(nss);
  }
}

static void node_copy_script(bNodeTree * /*dst_ntree*/, bNode *dest_node, const bNode *src_node)
{
  NodeShaderScript *src_nss = static_cast<NodeShaderScript *>(src_node->storage);
  NodeShaderScript *dest_nss = static_cast<NodeShaderScript *>(MEM_dupallocN(src_nss));

  if (src_nss->bytecode) {
    dest_nss->bytecode = static_cast<char *>(MEM_dupallocN(src_nss->bytecode));
  }

  dest_node->storage = dest_nss;
}

}  // namespace blender::nodes::node_shader_script_cc

void register_node_type_sh_script()
{
  namespace file_ns = blender::nodes::node_shader_script_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeScript", SH_NODE_SCRIPT);
  ntype.ui_name = "Script";
  ntype.ui_description =
      "Generate an OSL shader from a file or text data-block.\nNote: OSL shaders are not "
      "supported on all GPU backends";
  ntype.enum_name_legacy = "SCRIPT";
  ntype.nclass = NODE_CLASS_SCRIPT;
  ntype.draw_buttons = file_ns::node_shader_buts_script;
  ntype.draw_buttons_ex = file_ns::node_shader_buts_script_ex;
  ntype.initfunc = file_ns::init;
  blender::bke::node_type_storage(
      ntype, "NodeShaderScript", file_ns::node_free_script, file_ns::node_copy_script);

  blender::bke::node_register_type(ntype);
}
