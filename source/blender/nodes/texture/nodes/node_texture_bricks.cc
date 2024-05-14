/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup texnodes
 */

#include "BKE_material.h"
#include "BLI_math_vector.h"
#include "DNA_material_types.h"
#include "node_texture_util.hh"

#include <cmath>

static blender::bke::bNodeSocketTemplate inputs[] = {
    {SOCK_RGBA, N_("Bricks 1"), 0.596f, 0.282f, 0.0f, 1.0f},
    {SOCK_RGBA, N_("Bricks 2"), 0.632f, 0.504f, 0.05f, 1.0f},
    {SOCK_RGBA, N_("Mortar"), 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_FLOAT, N_("Thickness"), 0.02f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, PROP_UNSIGNED},
    {SOCK_FLOAT, N_("Bias"), 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Brick Width"), 0.5f, 0.0f, 0.0f, 0.0f, 0.001f, 99.0f, PROP_UNSIGNED},
    {SOCK_FLOAT, N_("Row Height"), 0.25f, 0.0f, 0.0f, 0.0f, 0.001f, 99.0f, PROP_UNSIGNED},
    {-1, ""},
};
static blender::bke::bNodeSocketTemplate outputs[] = {
    {SOCK_RGBA, N_("Color")},
    {-1, ""},
};

static void init(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom3 = 0.5; /* offset */
  node->custom4 = 1.0; /* squash */
}

static float noise(int n) /* fast integer noise */
{
  int nn;
  n = (n >> 13) ^ n;
  nn = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
  return 0.5f * (float(nn) / 1073741824.0f);
}

static void colorfn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
  const float *co = p->co;

  float x = co[0];
  float y = co[1];

  int bricknum, rownum;
  float offset = 0;
  float ins_x, ins_y;
  float tint;

  float bricks1[4];
  float bricks2[4];
  float mortar[4];

  float mortar_thickness = tex_input_value(in[3], p, thread);
  float bias = tex_input_value(in[4], p, thread);
  float brick_width = tex_input_value(in[5], p, thread);
  float row_height = tex_input_value(in[6], p, thread);

  tex_input_rgba(bricks1, in[0], p, thread);
  tex_input_rgba(bricks2, in[1], p, thread);
  tex_input_rgba(mortar, in[2], p, thread);

  rownum = int(floor(y / row_height));

  if (node->custom1 && node->custom2) {
    brick_width *= (int(rownum) % node->custom2) ? 1.0f : node->custom4;        /* squash */
    offset = (int(rownum) % node->custom1) ? 0 : (brick_width * node->custom3); /* offset */
  }

  bricknum = int(floor((x + offset) / brick_width));

  ins_x = (x + offset) - brick_width * bricknum;
  ins_y = y - row_height * rownum;

  tint = noise((rownum << 16) + (bricknum & 0xFFFF)) + bias;
  CLAMP(tint, 0.0f, 1.0f);

  if (ins_x < mortar_thickness || ins_y < mortar_thickness ||
      ins_x > (brick_width - mortar_thickness) || ins_y > (row_height - mortar_thickness))
  {
    copy_v4_v4(out, mortar);
  }
  else {
    copy_v4_v4(out, bricks1);
    ramp_blend(MA_RAMP_BLEND, out, tint, bricks2);
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

void register_node_type_tex_bricks()
{
  static blender::bke::bNodeType ntype;

  tex_node_type_base(&ntype, TEX_NODE_BRICKS, "Bricks", NODE_CLASS_PATTERN);
  blender::bke::node_type_socket_templates(&ntype, inputs, outputs);
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::Middle);
  ntype.initfunc = init;
  ntype.exec_fn = exec;
  ntype.flag |= NODE_PREVIEW;

  blender::bke::nodeRegisterType(&ntype);
}
