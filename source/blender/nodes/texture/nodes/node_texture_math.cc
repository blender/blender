/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup texnodes
 */

#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "node_texture_util.hh"
#include "node_util.hh"

/* **************** SCALAR MATH ******************** */
static blender::bke::bNodeSocketTemplate inputs[] = {
    {SOCK_FLOAT, N_("Value"), 0.5f, 0.5f, 0.5f, 1.0f, -100.0f, 100.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Value"), 0.5f, 0.5f, 0.5f, 1.0f, -100.0f, 100.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Value"), 0.0f, 0.5f, 0.5f, 1.0f, -100.0f, 100.0f, PROP_NONE},
    {-1, ""},
};

static blender::bke::bNodeSocketTemplate outputs[] = {
    {SOCK_FLOAT, N_("Value")},
    {-1, ""},
};

static void valuefn(float *out, TexParams *p, bNode *node, bNodeStack **in, short thread)
{
  float in0 = tex_input_value(in[0], p, thread);
  float in1 = tex_input_value(in[1], p, thread);

  switch (node->custom1) {

    case NODE_MATH_ADD:
      *out = in0 + in1;
      break;
    case NODE_MATH_SUBTRACT:
      *out = in0 - in1;
      break;
    case NODE_MATH_MULTIPLY:
      *out = in0 * in1;
      break;
    case NODE_MATH_DIVIDE: {
      if (in1 == 0) {
        /* We don't want to divide by zero. */
        *out = 0.0;
      }
      else {
        *out = in0 / in1;
      }
      break;
    }
    case NODE_MATH_SINE: {
      *out = sinf(in0);
      break;
    }
    case NODE_MATH_COSINE: {
      *out = cosf(in0);
      break;
    }
    case NODE_MATH_TANGENT: {
      *out = tanf(in0);
      break;
    }
    case NODE_MATH_SINH: {
      *out = sinhf(in0);
      break;
    }
    case NODE_MATH_COSH: {
      *out = coshf(in0);
      break;
    }
    case NODE_MATH_TANH: {
      *out = tanhf(in0);
      break;
    }
    case NODE_MATH_ARCSINE: {
      /* Can't do the impossible... */
      if (in0 <= 1 && in0 >= -1) {
        *out = asinf(in0);
      }
      else {
        *out = 0.0;
      }
      break;
    }
    case NODE_MATH_ARCCOSINE: {
      /* Can't do the impossible... */
      if (in0 <= 1 && in0 >= -1) {
        *out = acosf(in0);
      }
      else {
        *out = 0.0;
      }
      break;
    }
    case NODE_MATH_ARCTANGENT: {
      *out = atan(in0);
      break;
    }
    case NODE_MATH_POWER: {
      /* Only raise negative numbers by full integers */
      if (in0 >= 0) {
        out[0] = pow(in0, in1);
      }
      else {
        float y_mod_1 = fmod(in1, 1);
        if (y_mod_1 > 0.999f || y_mod_1 < 0.001f) {
          *out = pow(in0, floor(in1 + 0.5f));
        }
        else {
          *out = 0.0;
        }
      }
      break;
    }
    case NODE_MATH_LOGARITHM: {
      /* Don't want any imaginary numbers... */
      if (in0 > 0 && in1 > 0) {
        *out = log(in0) / log(in1);
      }
      else {
        *out = 0.0;
      }
      break;
    }
    case NODE_MATH_MINIMUM: {
      if (in0 < in1) {
        *out = in0;
      }
      else {
        *out = in1;
      }
      break;
    }
    case NODE_MATH_MAXIMUM: {
      if (in0 > in1) {
        *out = in0;
      }
      else {
        *out = in1;
      }
      break;
    }
    case NODE_MATH_ROUND: {
      *out = (in0 < 0) ? int(in0 - 0.5f) : int(in0 + 0.5f);
      break;
    }

    case NODE_MATH_LESS_THAN: {
      if (in0 < in1) {
        *out = 1.0f;
      }
      else {
        *out = 0.0f;
      }
      break;
    }

    case NODE_MATH_GREATER_THAN: {
      if (in0 > in1) {
        *out = 1.0f;
      }
      else {
        *out = 0.0f;
      }
      break;
    }

    case NODE_MATH_MODULO: {
      if (in1 == 0.0f) {
        *out = 0.0f;
      }
      else {
        *out = fmod(in0, in1);
      }
      break;
    }

    case NODE_MATH_FLOORED_MODULO: {
      if (in1 == 0.0f) {
        *out = 0.0f;
      }
      else {
        *out = in0 - floorf(in0 / in1) * in1;
      }
      break;
    }

    case NODE_MATH_ABSOLUTE: {
      *out = fabsf(in0);
      break;
    }

    case NODE_MATH_RADIANS: {
      *out = DEG2RADF(in0);
      break;
    }

    case NODE_MATH_DEGREES: {
      *out = RAD2DEGF(in0);
      break;
    }

    case NODE_MATH_ARCTAN2: {
      *out = atan2(in0, in1);
      break;
    }

    case NODE_MATH_SIGN: {
      *out = compatible_signf(in0);
      break;
    }

    case NODE_MATH_EXPONENT: {
      *out = expf(in0);
      break;
    }

    case NODE_MATH_FLOOR: {
      *out = floorf(in0);
      break;
    }

    case NODE_MATH_CEIL: {
      *out = ceilf(in0);
      break;
    }

    case NODE_MATH_FRACTION: {
      *out = in0 - floorf(in0);
      break;
    }

    case NODE_MATH_SQRT: {
      if (in0 > 0.0f) {
        *out = sqrtf(in0);
      }
      else {
        *out = 0.0f;
      }
      break;
    }

    case NODE_MATH_INV_SQRT: {
      if (in0 > 0.0f) {
        *out = 1.0f / sqrtf(in0);
      }
      else {
        *out = 0.0f;
      }
      break;
    }

    case NODE_MATH_TRUNC: {
      if (in0 > 0.0f) {
        *out = floorf(in0);
      }
      else {
        *out = ceilf(in0);
      }
      break;
    }

    case NODE_MATH_SNAP: {
      if (in1 == 0) {
        *out = 0.0;
      }
      else {
        *out = floorf(in0 / in1) * in1;
      }
      break;
    }

    case NODE_MATH_WRAP: {
      float in2 = tex_input_value(in[2], p, thread);
      *out = wrapf(in0, in1, in2);
      break;
    }

    case NODE_MATH_PINGPONG: {
      *out = pingpongf(in0, in1);
      break;
    }

    case NODE_MATH_COMPARE: {
      float in2 = tex_input_value(in[2], p, thread);
      *out = (fabsf(in0 - in1) <= std::max(in2, 1e-5f)) ? 1.0f : 0.0f;
      break;
    }

    case NODE_MATH_MULTIPLY_ADD: {
      float in2 = tex_input_value(in[2], p, thread);
      *out = in0 * in1 + in2;
      break;
    }

    case NODE_MATH_SMOOTH_MIN: {
      float in2 = tex_input_value(in[2], p, thread);
      *out = smoothminf(in0, in1, in2);
      break;
    }

    case NODE_MATH_SMOOTH_MAX: {
      float in2 = tex_input_value(in[2], p, thread);
      *out = -smoothminf(-in0, -in1, in2);
      break;
    }

    default: {
      BLI_assert(0);
      break;
    }
  }

  if (node->custom2 & SHD_MATH_CLAMP) {
    CLAMP(*out, 0.0f, 1.0f);
  }
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  node_math_update(ntree, node);

  bNodeSocket *sock1 = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 0));
  bNodeSocket *sock2 = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 1));
  bNodeSocket *sock3 = static_cast<bNodeSocket *>(BLI_findlink(&node->inputs, 2));

  node_sock_label_clear(sock1);
  node_sock_label_clear(sock2);
  node_sock_label_clear(sock3);

  switch (node->custom1) {
    case NODE_MATH_WRAP:
      node_sock_label(sock2, "Max");
      node_sock_label(sock3, "Min");
      break;
    case NODE_MATH_MULTIPLY_ADD:
      node_sock_label(sock2, "Multiplier");
      node_sock_label(sock3, "Addend");
      break;
    case NODE_MATH_LESS_THAN:
    case NODE_MATH_GREATER_THAN:
      node_sock_label(sock2, "Threshold");
      break;
    case NODE_MATH_PINGPONG:
      node_sock_label(sock2, "Scale");
      break;
    case NODE_MATH_SNAP:
      node_sock_label(sock2, "Increment");
      break;
    case NODE_MATH_POWER:
      node_sock_label(sock1, "Base");
      node_sock_label(sock2, "Exponent");
      break;
    case NODE_MATH_LOGARITHM:
      node_sock_label(sock2, "Base");
      break;
    case NODE_MATH_DEGREES:
      node_sock_label(sock1, "Radians");
      break;
    case NODE_MATH_RADIANS:
      node_sock_label(sock1, "Degrees");
      break;
    case NODE_MATH_COMPARE:
      node_sock_label(sock3, "Epsilon");
      break;
    case NODE_MATH_SMOOTH_MAX:
    case NODE_MATH_SMOOTH_MIN:
      node_sock_label(sock3, "Distance");
      break;
  }
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

void register_node_type_tex_math()
{
  static blender::bke::bNodeType ntype;

  tex_node_type_base(&ntype, "TextureNodeMath", TEX_NODE_MATH);
  ntype.ui_name = "Math";
  ntype.enum_name_legacy = "MATH";
  ntype.nclass = NODE_CLASS_CONVERTER;
  blender::bke::node_type_socket_templates(&ntype, inputs, outputs);
  ntype.labelfunc = node_math_label;
  ntype.exec_fn = exec;
  ntype.updatefunc = node_update;

  blender::bke::node_register_type(ntype);
}
