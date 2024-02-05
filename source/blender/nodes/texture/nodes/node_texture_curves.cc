/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup texnodes
 */

#include <algorithm>

#include "BKE_colortools.hh"
#include "NOD_texture.h"
#include "node_texture_util.hh"
#include "node_util.hh"

/* **************** CURVE Time  ******************** */

/* custom1 = start-frame, custom2 = end-frame. */
static bNodeSocketTemplate time_outputs[] = {{SOCK_FLOAT, N_("Value")}, {-1, ""}};

static void time_colorfn(
    float *out, TexParams *p, bNode *node, bNodeStack ** /*in*/, short /*thread*/)
{
  /* stack order output: fac */
  float fac = 0.0f;

  if (node->custom1 < node->custom2) {
    fac = (p->cfra - node->custom1) / float(node->custom2 - node->custom1);
  }

  CurveMapping *mapping = static_cast<CurveMapping *>(node->storage);
  BKE_curvemapping_init(mapping);
  fac = BKE_curvemapping_evaluateF(mapping, 0, fac);
  out[0] = std::clamp(fac, 0.0f, 1.0f);
}

static void time_exec(void *data,
                      int /*thread*/,
                      bNode *node,
                      bNodeExecData *execdata,
                      bNodeStack **in,
                      bNodeStack **out)
{
  tex_output(node, execdata, in, out[0], &time_colorfn, static_cast<TexCallData *>(data));
}

static void time_init(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = 1;
  node->custom2 = 250;
  node->storage = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
}

void register_node_type_tex_curve_time()
{
  static bNodeType ntype;

  tex_node_type_base(&ntype, TEX_NODE_CURVE_TIME, "Time", NODE_CLASS_INPUT);
  blender::bke::node_type_socket_templates(&ntype, nullptr, time_outputs);
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::LARGE);
  ntype.initfunc = time_init;
  node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);
  ntype.init_exec_fn = node_initexec_curves;
  ntype.exec_fn = time_exec;

  nodeRegisterType(&ntype);
}

/* **************** CURVE RGB  ******************** */
static bNodeSocketTemplate rgb_inputs[] = {
    {SOCK_RGBA, N_("Color"), 0.0f, 0.0f, 0.0f, 1.0f},
    {-1, ""},
};

static bNodeSocketTemplate rgb_outputs[] = {
    {SOCK_RGBA, N_("Color")},
    {-1, ""},
};

static void rgb_colorfn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
  float cin[4];
  tex_input_rgba(cin, in[0], p, thread);

  BKE_curvemapping_evaluateRGBF(static_cast<CurveMapping *>(node->storage), out, cin);
  out[3] = cin[3];
}

static void rgb_exec(void *data,
                     int /*thread*/,
                     bNode *node,
                     bNodeExecData *execdata,
                     bNodeStack **in,
                     bNodeStack **out)
{
  tex_output(node, execdata, in, out[0], &rgb_colorfn, static_cast<TexCallData *>(data));
}

static void rgb_init(bNodeTree * /*ntree*/, bNode *node)
{
  node->storage = BKE_curvemapping_add(4, 0.0f, 0.0f, 1.0f, 1.0f);
}

void register_node_type_tex_curve_rgb()
{
  static bNodeType ntype;

  tex_node_type_base(&ntype, TEX_NODE_CURVE_RGB, "RGB Curves", NODE_CLASS_OP_COLOR);
  blender::bke::node_type_socket_templates(&ntype, rgb_inputs, rgb_outputs);
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::LARGE);
  ntype.initfunc = rgb_init;
  node_type_storage(&ntype, "CurveMapping", node_free_curves, node_copy_curves);
  ntype.init_exec_fn = node_initexec_curves;
  ntype.exec_fn = rgb_exec;

  nodeRegisterType(&ntype);
}
