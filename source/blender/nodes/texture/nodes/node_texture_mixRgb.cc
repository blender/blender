/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

/** \file
 * \ingroup texnodes
 */

#include "NOD_texture.h"
#include "node_texture_util.hh"

/* **************** MIX RGB ******************** */
static bNodeSocketTemplate inputs[] = {
    {SOCK_FLOAT, N_("Factor"), 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_NONE},
    {SOCK_RGBA, N_("Color1"), 0.5f, 0.5f, 0.5f, 1.0f},
    {SOCK_RGBA, N_("Color2"), 0.5f, 0.5f, 0.5f, 1.0f},
    {-1, ""},
};
static bNodeSocketTemplate outputs[] = {
    {SOCK_RGBA, N_("Color")},
    {-1, ""},
};

static void colorfn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
  float fac = tex_input_value(in[0], p, thread);
  float col1[4], col2[4];

  tex_input_rgba(col1, in[1], p, thread);
  tex_input_rgba(col2, in[2], p, thread);

  /* use alpha */
  if (node->custom2 & 1) {
    fac *= col2[3];
  }

  CLAMP(fac, 0.0f, 1.0f);

  copy_v4_v4(out, col1);
  ramp_blend(node->custom1, out, fac, col2);
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

void register_node_type_tex_mix_rgb(void)
{
  static bNodeType ntype;

  tex_node_type_base(&ntype, TEX_NODE_MIX_RGB, "Mix", NODE_CLASS_OP_COLOR);
  node_type_socket_templates(&ntype, inputs, outputs);
  ntype.labelfunc = node_blend_label;
  ntype.exec_fn = exec;

  nodeRegisterType(&ntype);
}
