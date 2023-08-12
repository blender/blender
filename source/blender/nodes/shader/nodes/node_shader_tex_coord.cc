/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "DNA_customdata_types.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_tex_coord_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>("Generated");
  b.add_output<decl::Vector>("Normal");
  b.add_output<decl::Vector>("UV");
  b.add_output<decl::Vector>("Object");
  b.add_output<decl::Vector>("Camera");
  b.add_output<decl::Vector>("Window");
  b.add_output<decl::Vector>("Reflection");
}

static void node_shader_buts_tex_coord(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "object", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "from_instancer", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

static int node_shader_gpu_tex_coord(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  Object *ob = (Object *)node->id;

  /* Use special matrix to let the shader branch to using the render object's matrix. */
  float dummy_matrix[4][4];
  dummy_matrix[3][3] = 0.0f;
  GPUNodeLink *inv_obmat = (ob != nullptr) ? GPU_uniform(&ob->world_to_object[0][0]) :
                                             GPU_uniform(&dummy_matrix[0][0]);

  /* Optimization: don't request orco if not needed. */
  float4 zero(0.0f);
  GPUNodeLink *orco = out[0].hasoutput ? GPU_attribute(mat, CD_ORCO, "") : GPU_constant(zero);
  GPUNodeLink *mtface = GPU_attribute(mat, CD_AUTO_FROM_NAME, "");

  GPU_stack_link(mat, node, "node_tex_coord", in, out, inv_obmat, orco, mtface);

  int i;
  LISTBASE_FOREACH_INDEX (bNodeSocket *, sock, &node->outputs, i) {
    node_shader_gpu_bump_tex_coord(mat, node, &out[i].link);
    /* Normalize some vectors after dFdx/dFdy offsets.
     * This is the case for interpolated, non linear functions.
     * The resulting vector can still be a bit wrong but not as much.
     * (see #70644) */
    if (ELEM(i, 1, 6)) {
      GPU_link(mat,
               "vector_math_normalize",
               out[i].link,
               out[i].link,
               out[i].link,
               out[i].link,
               &out[i].link,
               nullptr);
    }
  }

  return 1;
}

}  // namespace blender::nodes::node_shader_tex_coord_cc

/* node type definition */
void register_node_type_sh_tex_coord()
{
  namespace file_ns = blender::nodes::node_shader_tex_coord_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_TEX_COORD, "Texture Coordinate", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_tex_coord;
  ntype.gpu_fn = file_ns::node_shader_gpu_tex_coord;

  nodeRegisterType(&ntype);
}
