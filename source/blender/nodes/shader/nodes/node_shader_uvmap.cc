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
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

#include "node_shader_util.hh"

#include "BKE_context.h"

#include "DNA_customdata_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_shader_uvmap_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>(N_("UV"));
}

static void node_shader_buts_uvmap(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "from_instancer", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, 0);

  if (!RNA_boolean_get(ptr, "from_instancer")) {
    PointerRNA obptr = CTX_data_pointer_get(C, "active_object");

    if (obptr.data && RNA_enum_get(&obptr, "type") == OB_MESH) {
      PointerRNA dataptr = RNA_pointer_get(&obptr, "data");
      uiItemPointerR(layout, ptr, "uv_map", &dataptr, "uv_layers", "", ICON_NONE);
    }
  }
}

static void node_shader_init_uvmap(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeShaderUVMap *attr = MEM_cnew<NodeShaderUVMap>("NodeShaderUVMap");
  node->storage = attr;
}

static int node_shader_gpu_uvmap(GPUMaterial *mat,
                                 bNode *node,
                                 bNodeExecData *UNUSED(execdata),
                                 GPUNodeStack *in,
                                 GPUNodeStack *out)
{
  NodeShaderUVMap *attr = static_cast<NodeShaderUVMap *>(node->storage);
  GPUNodeLink *mtface = GPU_attribute(mat, CD_MTFACE, attr->uv_map);

  GPU_stack_link(mat, node, "node_uvmap", in, out, mtface);

  node_shader_gpu_bump_tex_coord(mat, node, &out[0].link);

  return 1;
}

}  // namespace blender::nodes::node_shader_uvmap_cc

/* node type definition */
void register_node_type_sh_uvmap()
{
  namespace file_ns = blender::nodes::node_shader_uvmap_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_UVMAP, "UV Map", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_uvmap;
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  node_type_init(&ntype, file_ns::node_shader_init_uvmap);
  node_type_storage(
      &ntype, "NodeShaderUVMap", node_free_standard_storage, node_copy_standard_storage);
  node_type_gpu(&ntype, file_ns::node_shader_gpu_uvmap);

  nodeRegisterType(&ntype);
}
