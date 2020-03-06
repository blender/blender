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

#include "../node_shader_util.h"

#include "DNA_customdata_types.h"

/* **************** OUTPUT ******************** */

static bNodeSocketTemplate sh_node_tex_coord_out[] = {
    {SOCK_VECTOR, N_("Generated"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_VECTOR, N_("Normal"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_VECTOR, N_("UV"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_VECTOR, N_("Object"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_VECTOR, N_("Camera"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_VECTOR, N_("Window"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_VECTOR, N_("Reflection"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {-1, ""},
};

static int node_shader_gpu_tex_coord(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData *UNUSED(execdata),
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  Object *ob = (Object *)node->id;

  GPUNodeLink *inv_obmat = (ob != NULL) ? GPU_uniform(&ob->imat[0][0]) :
                                          GPU_builtin(GPU_INVERSE_OBJECT_MATRIX);

  /* Opti: don't request orco if not needed. */
  GPUNodeLink *orco = (!out[0].hasoutput) ? GPU_constant((float[4]){0.0f, 0.0f, 0.0f, 0.0f}) :
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

  /* for each output. */
  for (int i = 0; sh_node_tex_coord_out[i].type != -1; i++) {
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
               NULL);
    }
  }

  return 1;
}

/* node type definition */
void register_node_type_sh_tex_coord(void)
{
  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_TEX_COORD, "Texture Coordinate", NODE_CLASS_INPUT, 0);
  node_type_socket_templates(&ntype, NULL, sh_node_tex_coord_out);
  node_type_init(&ntype, NULL);
  node_type_storage(&ntype, "", NULL, NULL);
  node_type_gpu(&ntype, node_shader_gpu_tex_coord);

  nodeRegisterType(&ntype);
}
