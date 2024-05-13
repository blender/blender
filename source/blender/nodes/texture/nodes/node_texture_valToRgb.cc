/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup texnodes
 */

#include "BKE_colorband.hh"
#include "IMB_colormanagement.hh"
#include "node_texture_util.hh"
#include "node_util.hh"

/* **************** VALTORGB ******************** */
static blender::bke::bNodeSocketTemplate valtorgb_in[] = {
    {SOCK_FLOAT, N_("Fac"), 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_FACTOR},
    {-1, ""},
};
static blender::bke::bNodeSocketTemplate valtorgb_out[] = {
    {SOCK_RGBA, N_("Color")},
    {-1, ""},
};

static void valtorgb_colorfn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
  if (node->storage) {
    float fac = tex_input_value(in[0], p, thread);

    BKE_colorband_evaluate(static_cast<const ColorBand *>(node->storage), fac, out);
  }
}

static void valtorgb_exec(void *data,
                          int /*thread*/,
                          bNode *node,
                          bNodeExecData *execdata,
                          bNodeStack **in,
                          bNodeStack **out)
{
  tex_output(node, execdata, in, out[0], &valtorgb_colorfn, static_cast<TexCallData *>(data));
}

static void valtorgb_init(bNodeTree * /*ntree*/, bNode *node)
{
  node->storage = BKE_colorband_add(true);
}

void register_node_type_tex_valtorgb()
{
  static blender::bke::bNodeType ntype;

  tex_node_type_base(&ntype, TEX_NODE_VALTORGB, "Color Ramp", NODE_CLASS_CONVERTER);
  blender::bke::node_type_socket_templates(&ntype, valtorgb_in, valtorgb_out);
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::Large);
  ntype.initfunc = valtorgb_init;
  blender::bke::node_type_storage(&ntype, "ColorBand", node_free_standard_storage, node_copy_standard_storage);
  ntype.exec_fn = valtorgb_exec;

  blender::bke::nodeRegisterType(&ntype);
}

/* **************** RGBTOBW ******************** */
static blender::bke::bNodeSocketTemplate rgbtobw_in[] = {
    {SOCK_RGBA, N_("Color"), 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 1.0f},
    {-1, ""},
};
static blender::bke::bNodeSocketTemplate rgbtobw_out[] = {
    {SOCK_FLOAT, N_("Val"), 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
    {-1, ""},
};

static void rgbtobw_valuefn(
    float *out, TexParams *p, bNode * /*node*/, bNodeStack **in, short thread)
{
  float cin[4];
  tex_input_rgba(cin, in[0], p, thread);
  *out = IMB_colormanagement_get_luminance(cin);
}

static void rgbtobw_exec(void *data,
                         int /*thread*/,
                         bNode *node,
                         bNodeExecData *execdata,
                         bNodeStack **in,
                         bNodeStack **out)
{
  tex_output(node, execdata, in, out[0], &rgbtobw_valuefn, static_cast<TexCallData *>(data));
}

void register_node_type_tex_rgbtobw()
{
  static blender::bke::bNodeType ntype;

  tex_node_type_base(&ntype, TEX_NODE_RGBTOBW, "RGB to BW", NODE_CLASS_CONVERTER);
  blender::bke::node_type_socket_templates(&ntype, rgbtobw_in, rgbtobw_out);
  ntype.exec_fn = rgbtobw_exec;

  blender::bke::nodeRegisterType(&ntype);
}
