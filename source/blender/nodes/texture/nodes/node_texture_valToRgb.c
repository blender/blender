/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup texnodes
 */

#include "IMB_colormanagement.h"
#include "NOD_texture.h"
#include "node_texture_util.h"

/* **************** VALTORGB ******************** */
static bNodeSocketTemplate valtorgb_in[] = {
    {SOCK_FLOAT, N_("Fac"), 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
    {-1, ""},
};
static bNodeSocketTemplate valtorgb_out[] = {
    {SOCK_RGBA, N_("Color")},
    {-1, ""},
};

static void valtorgb_colorfn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
  if (node->storage) {
    float fac = tex_input_value(in[0], p, thread);

    BKE_colorband_evaluate(node->storage, fac, out);
  }
}

static void valtorgb_exec(void *data,
                          int UNUSED(thread),
                          bNode *node,
                          bNodeExecData *execdata,
                          bNodeStack **in,
                          bNodeStack **out)
{
  tex_output(node, execdata, in, out[0], &valtorgb_colorfn, data);
}

static void valtorgb_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->storage = BKE_colorband_add(true);
}

void register_node_type_tex_valtorgb(void)
{
  static bNodeType ntype;

  tex_node_type_base(&ntype, TEX_NODE_VALTORGB, "ColorRamp", NODE_CLASS_CONVERTER);
  node_type_socket_templates(&ntype, valtorgb_in, valtorgb_out);
  node_type_size_preset(&ntype, NODE_SIZE_LARGE);
  node_type_init(&ntype, valtorgb_init);
  node_type_storage(&ntype, "ColorBand", node_free_standard_storage, node_copy_standard_storage);
  node_type_exec(&ntype, NULL, NULL, valtorgb_exec);

  nodeRegisterType(&ntype);
}

/* **************** RGBTOBW ******************** */
static bNodeSocketTemplate rgbtobw_in[] = {
    {SOCK_RGBA, N_("Color"), 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 1.0f},
    {-1, ""},
};
static bNodeSocketTemplate rgbtobw_out[] = {
    {SOCK_FLOAT, N_("Val"), 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
    {-1, ""},
};

static void rgbtobw_valuefn(
    float *out, TexParams *p, bNode *UNUSED(node), bNodeStack **in, short thread)
{
  float cin[4];
  tex_input_rgba(cin, in[0], p, thread);
  *out = IMB_colormanagement_get_luminance(cin);
}

static void rgbtobw_exec(void *data,
                         int UNUSED(thread),
                         bNode *node,
                         bNodeExecData *execdata,
                         bNodeStack **in,
                         bNodeStack **out)
{
  tex_output(node, execdata, in, out[0], &rgbtobw_valuefn, data);
}

void register_node_type_tex_rgbtobw(void)
{
  static bNodeType ntype;

  tex_node_type_base(&ntype, TEX_NODE_RGBTOBW, "RGB to BW", NODE_CLASS_CONVERTER);
  node_type_socket_templates(&ntype, rgbtobw_in, rgbtobw_out);
  node_type_exec(&ntype, NULL, NULL, rgbtobw_exec);

  nodeRegisterType(&ntype);
}
