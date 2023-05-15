/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

/** \file
 * \ingroup texnodes
 */

#include "node_texture_util.hh"
#include <cmath>

static bNodeSocketTemplate inputs[] = {
    {SOCK_RGBA, N_("Color"), 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_VECTOR, N_("Scale"), 1.0f, 1.0f, 1.0f, 0.0f, -10.0f, 10.0f, PROP_XYZ},
    {-1, ""},
};

static bNodeSocketTemplate outputs[] = {
    {SOCK_RGBA, N_("Color")},
    {-1, ""},
};

static void colorfn(float *out, TexParams *p, bNode * /*node*/, bNodeStack **in, short thread)
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
                 int /*thread*/,
                 bNode *node,
                 bNodeExecData *execdata,
                 bNodeStack **in,
                 bNodeStack **out)
{
  tex_output(node, execdata, in, out[0], &colorfn, static_cast<TexCallData *>(data));
}

void register_node_type_tex_scale()
{
  static bNodeType ntype;

  tex_node_type_base(&ntype, TEX_NODE_SCALE, "Scale", NODE_CLASS_DISTORT);
  blender::bke::node_type_socket_templates(&ntype, inputs, outputs);
  ntype.exec_fn = exec;

  nodeRegisterType(&ntype);
}
