/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup texnodes
 */

#include "node_texture_util.hh"
#include <cmath>

static bNodeSocketTemplate inputs[] = {
    {SOCK_RGBA, N_("Color"), 0.0f, 0.0f, 0.0f, 1.0f},
    {-1, ""},
};
static bNodeSocketTemplate outputs[] = {
    {SOCK_FLOAT, N_("Red")},
    {SOCK_FLOAT, N_("Green")},
    {SOCK_FLOAT, N_("Blue")},
    {SOCK_FLOAT, N_("Alpha")},
    {-1, ""},
};

static void valuefn_r(float *out, TexParams *p, bNode * /*node*/, bNodeStack **in, short thread)
{
  tex_input_rgba(out, in[0], p, thread);
  *out = out[0];
}

static void valuefn_g(float *out, TexParams *p, bNode * /*node*/, bNodeStack **in, short thread)
{
  tex_input_rgba(out, in[0], p, thread);
  *out = out[1];
}

static void valuefn_b(float *out, TexParams *p, bNode * /*node*/, bNodeStack **in, short thread)
{
  tex_input_rgba(out, in[0], p, thread);
  *out = out[2];
}

static void valuefn_a(float *out, TexParams *p, bNode * /*node*/, bNodeStack **in, short thread)
{
  tex_input_rgba(out, in[0], p, thread);
  *out = out[3];
}

static void exec(void *data,
                 int /*thread*/,
                 bNode *node,
                 bNodeExecData *execdata,
                 bNodeStack **in,
                 bNodeStack **out)
{
  TexCallData *tex_call_data = static_cast<TexCallData *>(data);
  tex_output(node, execdata, in, out[0], &valuefn_r, tex_call_data);
  tex_output(node, execdata, in, out[1], &valuefn_g, tex_call_data);
  tex_output(node, execdata, in, out[2], &valuefn_b, tex_call_data);
  tex_output(node, execdata, in, out[3], &valuefn_a, tex_call_data);
}

void register_node_type_tex_decompose()
{
  static bNodeType ntype;

  tex_node_type_base(&ntype, TEX_NODE_DECOMPOSE_LEGACY, "Separate RGBA", NODE_CLASS_OP_COLOR);
  blender::bke::node_type_socket_templates(&ntype, inputs, outputs);
  ntype.exec_fn = exec;

  nodeRegisterType(&ntype);
}
