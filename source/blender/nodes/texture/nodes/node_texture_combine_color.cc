/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup texnodes
 */

#include "BLI_listbase.h"
#include "NOD_texture.h"
#include "node_texture_util.hh"

static bNodeSocketTemplate inputs[] = {
    {SOCK_FLOAT, N_("Red"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
    {SOCK_FLOAT, N_("Green"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
    {SOCK_FLOAT, N_("Blue"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
    {SOCK_FLOAT, N_("Alpha"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
    {-1, ""},
};
static bNodeSocketTemplate outputs[] = {
    {SOCK_RGBA, N_("Color")},
    {-1, ""},
};

static void colorfn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
  int i;
  for (i = 0; i < 4; i++) {
    out[i] = tex_input_value(in[i], p, thread);
  }
  /* Apply color space if required. */
  switch (node->custom1) {
    case NODE_COMBSEP_COLOR_RGB: {
      /* Pass */
      break;
    }
    case NODE_COMBSEP_COLOR_HSV: {
      hsv_to_rgb_v(out, out);
      break;
    }
    case NODE_COMBSEP_COLOR_HSL: {
      hsl_to_rgb_v(out, out);
      break;
    }
    default: {
      BLI_assert_unreachable();
      break;
    }
  }
}

static void update(bNodeTree * /*ntree*/, bNode *node)
{
  node_combsep_color_label(&node->inputs, (NodeCombSepColorMode)node->custom1);
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

void register_node_type_tex_combine_color()
{
  static bNodeType ntype;

  tex_node_type_base(&ntype, TEX_NODE_COMBINE_COLOR, "Combine Color", NODE_CLASS_OP_COLOR);
  blender::bke::node_type_socket_templates(&ntype, inputs, outputs);
  ntype.exec_fn = exec;
  ntype.updatefunc = update;

  nodeRegisterType(&ntype);
}
