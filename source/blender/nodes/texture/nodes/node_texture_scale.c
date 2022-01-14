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
 * \ingroup texnodes
 */

#include "node_texture_util.h"
#include <math.h>

static bNodeSocketTemplate inputs[] = {
    {SOCK_RGBA, N_("Color"), 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_VECTOR, N_("Scale"), 1.0f, 1.0f, 1.0f, 0.0f, -10.0f, 10.0f, PROP_XYZ},
    {-1, ""},
};

static bNodeSocketTemplate outputs[] = {
    {SOCK_RGBA, N_("Color")},
    {-1, ""},
};

static void colorfn(float *out, TexParams *p, bNode *UNUSED(node), bNodeStack **in, short thread)
{
  float scale[3], new_co[3], new_dxt[3], new_dyt[3];
  TexParams np = *p;

  np.co = new_co;
  np.dxt = new_dxt;
  np.dyt = new_dyt;

  tex_input_vec(scale, in[1], p, thread);

  mul_v3_v3v3(new_co, p->co, scale);
  if (p->osatex) {
    mul_v3_v3v3(new_dxt, p->dxt, scale);
    mul_v3_v3v3(new_dyt, p->dyt, scale);
  }

  tex_input_rgba(out, in[0], &np, thread);
}
static void exec(void *data,
                 int UNUSED(thread),
                 bNode *node,
                 bNodeExecData *execdata,
                 bNodeStack **in,
                 bNodeStack **out)
{
  tex_output(node, execdata, in, out[0], &colorfn, data);
}

void register_node_type_tex_scale(void)
{
  static bNodeType ntype;

  tex_node_type_base(&ntype, TEX_NODE_SCALE, "Scale", NODE_CLASS_DISTORT);
  node_type_socket_templates(&ntype, inputs, outputs);
  node_type_exec(&ntype, NULL, NULL, exec);

  nodeRegisterType(&ntype);
}
