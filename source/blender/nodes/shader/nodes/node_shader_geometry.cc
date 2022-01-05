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

namespace blender::nodes::node_shader_geometry_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>(N_("Position"));
  b.add_output<decl::Vector>(N_("Normal"));
  b.add_output<decl::Vector>(N_("Tangent"));
  b.add_output<decl::Vector>(N_("True Normal"));
  b.add_output<decl::Vector>(N_("Incoming"));
  b.add_output<decl::Vector>(N_("Parametric"));
  b.add_output<decl::Float>(N_("Backfacing"));
  b.add_output<decl::Float>(N_("Pointiness"));
  b.add_output<decl::Float>(N_("Random Per Island"));
}

static int node_shader_gpu_geometry(GPUMaterial *mat,
                                    bNode *node,
                                    bNodeExecData *UNUSED(execdata),
                                    GPUNodeStack *in,
                                    GPUNodeStack *out)
{
  /* HACK: Don't request GPU_BARYCENTRIC_TEXCO if not used because it will
   * trigger the use of geometry shader (and the performance penalty it implies). */
  const float val[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  GPUNodeLink *bary_link = (!out[5].hasoutput) ? GPU_constant(val) :
                                                 GPU_builtin(GPU_BARYCENTRIC_TEXCO);
  if (out[5].hasoutput) {
    GPU_material_flag_set(mat, GPU_MATFLAG_BARYCENTRIC);
  }
  /* Opti: don't request orco if not needed. */
  GPUNodeLink *orco_link = (!out[2].hasoutput) ? GPU_constant(val) :
                                                 GPU_attribute(mat, CD_ORCO, "");

  const bool success = GPU_stack_link(mat,
                                      node,
                                      "node_geometry",
                                      in,
                                      out,
                                      GPU_builtin(GPU_VIEW_POSITION),
                                      GPU_builtin(GPU_WORLD_NORMAL),
                                      orco_link,
                                      GPU_builtin(GPU_OBJECT_MATRIX),
                                      GPU_builtin(GPU_INVERSE_VIEW_MATRIX),
                                      bary_link);

  int i;
  LISTBASE_FOREACH_INDEX (bNodeSocket *, sock, &node->outputs, i) {
    node_shader_gpu_bump_tex_coord(mat, node, &out[i].link);
    /* Normalize some vectors after dFdx/dFdy offsets.
     * This is the case for interpolated, non linear functions.
     * The resulting vector can still be a bit wrong but not as much.
     * (see T70644) */
    if (node->branch_tag != 0 && ELEM(i, 1, 2, 4)) {
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

  return success;
}

}  // namespace blender::nodes::node_shader_geometry_cc

/* node type definition */
void register_node_type_sh_geometry()
{
  namespace file_ns = blender::nodes::node_shader_geometry_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_NEW_GEOMETRY, "Geometry", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  node_type_gpu(&ntype, file_ns::node_shader_gpu_geometry);

  nodeRegisterType(&ntype);
}
