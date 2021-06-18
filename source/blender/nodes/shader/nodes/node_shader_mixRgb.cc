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

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.h"

/* **************** MIX RGB ******************** */
static bNodeSocketTemplate sh_node_mix_rgb_in[] = {
    {SOCK_FLOAT, N_("Fac"), 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
    {SOCK_RGBA, N_("Color1"), 0.5f, 0.5f, 0.5f, 1.0f},
    {SOCK_RGBA, N_("Color2"), 0.5f, 0.5f, 0.5f, 1.0f},
    {-1, ""},
};
static bNodeSocketTemplate sh_node_mix_rgb_out[] = {
    {SOCK_RGBA, N_("Color")},
    {-1, ""},
};

static void node_shader_exec_mix_rgb(void *UNUSED(data),
                                     int UNUSED(thread),
                                     bNode *node,
                                     bNodeExecData *UNUSED(execdata),
                                     bNodeStack **in,
                                     bNodeStack **out)
{
  /* stack order in: fac, col1, col2 */
  /* stack order out: col */
  float col[3];
  float fac;
  float vec[3];

  nodestack_get_vec(&fac, SOCK_FLOAT, in[0]);
  CLAMP(fac, 0.0f, 1.0f);

  nodestack_get_vec(col, SOCK_VECTOR, in[1]);
  nodestack_get_vec(vec, SOCK_VECTOR, in[2]);

  ramp_blend(node->custom1, col, fac, vec);
  if (node->custom2 & SHD_MIXRGB_CLAMP) {
    CLAMP3(col, 0.0f, 1.0f);
  }
  copy_v3_v3(out[0]->vec, col);
}

static const char *gpu_shader_get_name(int mode)
{
  switch (mode) {
    case MA_RAMP_BLEND:
      return "mix_blend";
    case MA_RAMP_ADD:
      return "mix_add";
    case MA_RAMP_MULT:
      return "mix_mult";
    case MA_RAMP_SUB:
      return "mix_sub";
    case MA_RAMP_SCREEN:
      return "mix_screen";
    case MA_RAMP_DIV:
      return "mix_div";
    case MA_RAMP_DIFF:
      return "mix_diff";
    case MA_RAMP_DARK:
      return "mix_dark";
    case MA_RAMP_LIGHT:
      return "mix_light";
    case MA_RAMP_OVERLAY:
      return "mix_overlay";
    case MA_RAMP_DODGE:
      return "mix_dodge";
    case MA_RAMP_BURN:
      return "mix_burn";
    case MA_RAMP_HUE:
      return "mix_hue";
    case MA_RAMP_SAT:
      return "mix_sat";
    case MA_RAMP_VAL:
      return "mix_val";
    case MA_RAMP_COLOR:
      return "mix_color";
    case MA_RAMP_SOFT:
      return "mix_soft";
    case MA_RAMP_LINEAR:
      return "mix_linear";
  }

  return nullptr;
}

static int gpu_shader_mix_rgb(GPUMaterial *mat,
                              bNode *node,
                              bNodeExecData *UNUSED(execdata),
                              GPUNodeStack *in,
                              GPUNodeStack *out)
{
  const char *name = gpu_shader_get_name(node->custom1);

  if (name != nullptr) {
    int ret = GPU_stack_link(mat, node, name, in, out);
    if (ret && node->custom2 & SHD_MIXRGB_CLAMP) {
      const float min[3] = {0.0f, 0.0f, 0.0f};
      const float max[3] = {1.0f, 1.0f, 1.0f};
      GPU_link(
          mat, "clamp_color", out[0].link, GPU_constant(min), GPU_constant(max), &out[0].link);
    }
    return ret;
  }

  return 0;
}

void register_node_type_sh_mix_rgb(void)
{
  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_MIX_RGB, "Mix", NODE_CLASS_OP_COLOR, 0);
  node_type_socket_templates(&ntype, sh_node_mix_rgb_in, sh_node_mix_rgb_out);
  node_type_label(&ntype, node_blend_label);
  node_type_exec(&ntype, nullptr, nullptr, node_shader_exec_mix_rgb);
  node_type_gpu(&ntype, gpu_shader_mix_rgb);

  nodeRegisterType(&ntype);
}
