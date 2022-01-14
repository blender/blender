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

#include "DNA_customdata_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_shader_tex_coord_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>(N_("Generated"));
  b.add_output<decl::Vector>(N_("Normal"));
  b.add_output<decl::Vector>(N_("UV"));
  b.add_output<decl::Vector>(N_("Object"));
  b.add_output<decl::Vector>(N_("Camera"));
  b.add_output<decl::Vector>(N_("Window"));
  b.add_output<decl::Vector>(N_("Reflection"));
}

static void node_shader_buts_tex_coord(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "object", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, 0);
  uiItemR(layout, ptr, "from_instancer", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, 0);
}

static int node_shader_gpu_tex_coord(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData *UNUSED(execdata),
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  Object *ob = (Object *)node->id;

  GPUNodeLink *inv_obmat = (ob != nullptr) ? GPU_uniform(&ob->imat[0][0]) :
                                             GPU_builtin(GPU_INVERSE_OBJECT_MATRIX);

  /* Opti: don't request orco if not needed. */
  const float default_coords[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  GPUNodeLink *orco = (!out[0].hasoutput) ? GPU_constant(default_coords) :
                                            GPU_attribute(mat, CD_ORCO, "");
  GPUNodeLink *mtface = GPU_attribute(mat, CD_MTFACE, "");
  GPUNodeLink *viewpos = GPU_builtin(GPU_VIEW_POSITION);
  GPUNodeLink *worldnor = GPU_builtin(GPU_WORLD_NORMAL);
  GPUNodeLink *texcofacs = GPU_builtin(GPU_CAMERA_TEXCO_FACTORS);

  if (out[0].hasoutput) {
    GPU_link(mat, "generated_from_orco", orco, &orco);
  }

  GPU_stack_link(
      mat, node, "node_tex_coord", in, out, viewpos, worldnor, inv_obmat, texcofacs, orco, mtface);

  int i;
  LISTBASE_FOREACH_INDEX (bNodeSocket *, sock, &node->outputs, i) {
    node_shader_gpu_bump_tex_coord(mat, node, &out[i].link);
    /* Normalize some vectors after dFdx/dFdy offsets.
     * This is the case for interpolated, non linear functions.
     * The resulting vector can still be a bit wrong but not as much.
     * (see T70644) */
    if (node->branch_tag != 0 && ELEM(i, 1, 6)) {
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
  node_type_gpu(&ntype, file_ns::node_shader_gpu_tex_coord);

  nodeRegisterType(&ntype);
}
