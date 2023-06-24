/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup texnodes
 */

#include "NOD_texture.h"
#include "node_texture_util.hh"
#include <cmath>

static bNodeSocketTemplate inputs[] = {
    {SOCK_RGBA, N_("Color1"), 1.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, N_("Color2"), 1.0f, 1.0f, 1.0f, 1.0f},
    {SOCK_FLOAT, N_("Size"), 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 100.0f, PROP_UNSIGNED},
    {-1, ""},
};
static bNodeSocketTemplate outputs[] = {
    {SOCK_RGBA, N_("Color")},
    {-1, ""},
};

static void colorfn(float *out, TexParams *p, bNode * /*node*/, bNodeStack **in, short thread)
{
  float x = p->co[0];
  float y = p->co[1];
  float z = p->co[2];
  float sz = tex_input_value(in[2], p, thread);

  /* 0.00001  because of unit sized stuff */
  int xi = int(fabs(floor(0.00001f + x / sz)));
  int yi = int(fabs(floor(0.00001f + y / sz)));
  int zi = int(fabs(floor(0.00001f + z / sz)));

  if ((xi % 2 == yi % 2) == (zi % 2)) {
    tex_input_rgba(out, in[0], p, thread);
  }
  else {
    tex_input_rgba(out, in[1], p, thread);
  }
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

void register_node_type_tex_checker()
{
  static bNodeType ntype;

  tex_node_type_base(&ntype, TEX_NODE_CHECKER, "Checker", NODE_CLASS_PATTERN);
  blender::bke::node_type_socket_templates(&ntype, inputs, outputs);
  ntype.exec_fn = exec;
  ntype.flag |= NODE_PREVIEW;

  nodeRegisterType(&ntype);
}
