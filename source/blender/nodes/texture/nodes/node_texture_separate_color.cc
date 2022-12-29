/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup texnodes
 */

#include "BLI_listbase.h"
#include "NOD_texture.h"
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

static void apply_color_space(float *out, NodeCombSepColorMode type)
{
  switch (type) {
    case NODE_COMBSEP_COLOR_RGB: {
      /* Pass */
      break;
    }
    case NODE_COMBSEP_COLOR_HSV: {
      rgb_to_hsv_v(out, out);
      break;
    }
    case NODE_COMBSEP_COLOR_HSL: {
      rgb_to_hsl_v(out, out);
      break;
    }
    default: {
      BLI_assert_unreachable();
      break;
    }
  }
}

static void valuefn_r(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
  tex_input_rgba(out, in[0], p, thread);
  apply_color_space(out, (NodeCombSepColorMode)node->custom1);
  *out = out[0];
}

static void valuefn_g(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
  tex_input_rgba(out, in[0], p, thread);
  apply_color_space(out, (NodeCombSepColorMode)node->custom1);
  *out = out[1];
}

static void valuefn_b(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
  tex_input_rgba(out, in[0], p, thread);
  apply_color_space(out, (NodeCombSepColorMode)node->custom1);
  *out = out[2];
}

static void valuefn_a(float *out, TexParams *p, bNode * /*node*/, bNodeStack **in, short thread)
{
  tex_input_rgba(out, in[0], p, thread);
  *out = out[3];
}

static void update(bNodeTree * /*ntree*/, bNode *node)
{
  node_combsep_color_label(&node->outputs, (NodeCombSepColorMode)node->custom1);
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

void register_node_type_tex_separate_color()
{
  static bNodeType ntype;

  tex_node_type_base(&ntype, TEX_NODE_SEPARATE_COLOR, "Separate Color", NODE_CLASS_OP_COLOR);
  node_type_socket_templates(&ntype, inputs, outputs);
  ntype.exec_fn = exec;
  ntype.updatefunc = update;

  nodeRegisterType(&ntype);
}
