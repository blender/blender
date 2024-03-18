/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup texnodes
 */

#include "BLI_math_vector.h"
#include "NOD_texture.h"
#include "node_texture_util.hh"
#include <cmath>

static bNodeSocketTemplate inputs[] = {
    {SOCK_VECTOR, N_("Coordinate 1"), 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, PROP_NONE},
    {SOCK_VECTOR, N_("Coordinate 2"), 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, PROP_NONE},
    {-1, ""},
};

static bNodeSocketTemplate outputs[] = {
    {SOCK_FLOAT, N_("Value")},
    {-1, ""},
};

static void valuefn(float *out, TexParams *p, bNode * /*node*/, bNodeStack **in, short thread)
{
  float co1[3], co2[3];

  tex_input_vec(co1, in[0], p, thread);
  tex_input_vec(co2, in[1], p, thread);

  *out = len_v3v3(co2, co1);
}

static void exec(void *data,
                 int /*thread*/,
                 bNode *node,
                 bNodeExecData *execdata,
                 bNodeStack **in,
                 bNodeStack **out)
{
  tex_output(node, execdata, in, out[0], &valuefn, static_cast<TexCallData *>(data));
}

void register_node_type_tex_distance()
{
  static bNodeType ntype;

  tex_node_type_base(&ntype, TEX_NODE_DISTANCE, "Distance", NODE_CLASS_CONVERTER);
  blender::bke::node_type_socket_templates(&ntype, inputs, outputs);
  ntype.exec_fn = exec;

  nodeRegisterType(&ntype);
}
