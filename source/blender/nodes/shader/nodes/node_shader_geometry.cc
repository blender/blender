/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

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
                                    bNodeExecData * /*execdata*/,
                                    GPUNodeStack *in,
                                    GPUNodeStack *out)
{
  /* HACK: Don't request GPU_MATFLAG_BARYCENTRIC if not used because it will
   * trigger the use of geometry shader (and the performance penalty it implies). */
  if (out[5].hasoutput) {
    GPU_material_flag_set(mat, GPU_MATFLAG_BARYCENTRIC);
  }
  /* Optimization: don't request orco if not needed. */
  const float val[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  GPUNodeLink *orco_link = out[2].hasoutput ? GPU_attribute(mat, CD_ORCO, "") : GPU_constant(val);

  const bool success = GPU_stack_link(mat, node, "node_geometry", in, out, orco_link);

  int i;
  LISTBASE_FOREACH_INDEX (bNodeSocket *, sock, &node->outputs, i) {
    node_shader_gpu_bump_tex_coord(mat, node, &out[i].link);
    /* Normalize some vectors after dFdx/dFdy offsets.
     * This is the case for interpolated, non linear functions.
     * The resulting vector can still be a bit wrong but not as much.
     * (see #70644) */
    if (ELEM(i, 1, 2, 4)) {
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
  ntype.gpu_fn = file_ns::node_shader_gpu_geometry;

  nodeRegisterType(&ntype);
}
