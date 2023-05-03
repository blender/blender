/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "scene/constant_fold.h"
#include "scene/shader_graph.h"

#include "util/foreach.h"
#include "util/log.h"

CCL_NAMESPACE_BEGIN

ConstantFolder::ConstantFolder(ShaderGraph *graph,
                               ShaderNode *node,
                               ShaderOutput *output,
                               Scene *scene)
    : graph(graph), node(node), output(output), scene(scene)
{
}

bool ConstantFolder::all_inputs_constant() const
{
  foreach (ShaderInput *input, node->inputs) {
    if (input->link) {
      return false;
    }
  }

  return true;
}

void ConstantFolder::make_constant(float value) const
{
  VLOG_DEBUG << "Folding " << node->name << "::" << output->name() << " to constant (" << value
             << ").";

  foreach (ShaderInput *sock, output->links) {
    sock->set(value);
    sock->constant_folded_in = true;
  }

  graph->disconnect(output);
}

void ConstantFolder::make_constant(float3 value) const
{
  VLOG_DEBUG << "Folding " << node->name << "::" << output->name() << " to constant " << value
             << ".";

  foreach (ShaderInput *sock, output->links) {
    sock->set(value);
    sock->constant_folded_in = true;
  }

  graph->disconnect(output);
}

void ConstantFolder::make_constant_clamp(float value, bool clamp) const
{
  make_constant(clamp ? saturatef(value) : value);
}

void ConstantFolder::make_constant_clamp(float3 value, bool clamp) const
{
  if (clamp) {
    value.x = saturatef(value.x);
    value.y = saturatef(value.y);
    value.z = saturatef(value.z);
  }

  make_constant(value);
}

void ConstantFolder::make_zero() const
{
  if (output->type() == SocketType::FLOAT) {
    make_constant(0.0f);
  }
  else if (SocketType::is_float3(output->type())) {
    make_constant(zero_float3());
  }
  else {
    assert(0);
  }
}

void ConstantFolder::make_one() const
{
  if (output->type() == SocketType::FLOAT) {
    make_constant(1.0f);
  }
  else if (SocketType::is_float3(output->type())) {
    make_constant(one_float3());
  }
  else {
    assert(0);
  }
}

void ConstantFolder::bypass(ShaderOutput *new_output) const
{
  assert(new_output);

  VLOG_DEBUG << "Folding " << node->name << "::" << output->name() << " to socket "
             << new_output->parent->name << "::" << new_output->name() << ".";

  /* Remove all outgoing links from socket and connect them to new_output instead.
   * The graph->relink method affects node inputs, so it's not safe to use in constant
   * folding if the node has multiple outputs and will thus be folded multiple times. */
  vector<ShaderInput *> outputs = output->links;

  graph->disconnect(output);

  foreach (ShaderInput *sock, outputs) {
    graph->connect(new_output, sock);
  }
}

void ConstantFolder::discard() const
{
  assert(output->type() == SocketType::CLOSURE);

  VLOG_DEBUG << "Discarding closure " << node->name << ".";

  graph->disconnect(output);
}

void ConstantFolder::bypass_or_discard(ShaderInput *input) const
{
  assert(input->type() == SocketType::CLOSURE);

  if (input->link) {
    bypass(input->link);
  }
  else {
    discard();
  }
}

bool ConstantFolder::try_bypass_or_make_constant(ShaderInput *input, bool clamp) const
{
  if (input->type() != output->type()) {
    return false;
  }
  else if (!input->link) {
    if (input->type() == SocketType::FLOAT) {
      make_constant_clamp(node->get_float(input->socket_type), clamp);
      return true;
    }
    else if (SocketType::is_float3(input->type())) {
      make_constant_clamp(node->get_float3(input->socket_type), clamp);
      return true;
    }
  }
  else if (!clamp) {
    bypass(input->link);
    return true;
  }
  else {
    /* disconnect other inputs if we can't fully bypass due to clamp */
    foreach (ShaderInput *other, node->inputs) {
      if (other != input && other->link) {
        graph->disconnect(other);
      }
    }
  }

  return false;
}

bool ConstantFolder::is_zero(ShaderInput *input) const
{
  if (!input->link) {
    if (input->type() == SocketType::FLOAT) {
      return node->get_float(input->socket_type) == 0.0f;
    }
    else if (SocketType::is_float3(input->type())) {
      return node->get_float3(input->socket_type) == zero_float3();
    }
  }

  return false;
}

bool ConstantFolder::is_one(ShaderInput *input) const
{
  if (!input->link) {
    if (input->type() == SocketType::FLOAT) {
      return node->get_float(input->socket_type) == 1.0f;
    }
    else if (SocketType::is_float3(input->type())) {
      return node->get_float3(input->socket_type) == one_float3();
    }
  }

  return false;
}

/* Specific nodes */

void ConstantFolder::fold_mix(NodeMix type, bool clamp) const
{
  ShaderInput *fac_in = node->input("Fac");
  ShaderInput *color1_in = node->input("Color1");
  ShaderInput *color2_in = node->input("Color2");

  float fac = saturatef(node->get_float(fac_in->socket_type));
  bool fac_is_zero = !fac_in->link && fac == 0.0f;
  bool fac_is_one = !fac_in->link && fac == 1.0f;

  /* remove no-op node when factor is 0.0 */
  if (fac_is_zero) {
    /* note that some of the modes will clamp out of bounds values even without use_clamp */
    if (!(type == NODE_MIX_LIGHT || type == NODE_MIX_DODGE || type == NODE_MIX_BURN)) {
      if (try_bypass_or_make_constant(color1_in, clamp)) {
        return;
      }
    }
  }

  switch (type) {
    case NODE_MIX_BLEND:
      /* remove useless mix colors nodes */
      if (color1_in->link && color2_in->link) {
        if (color1_in->link == color2_in->link) {
          try_bypass_or_make_constant(color1_in, clamp);
          break;
        }
      }
      else if (!color1_in->link && !color2_in->link) {
        float3 color1 = node->get_float3(color1_in->socket_type);
        float3 color2 = node->get_float3(color2_in->socket_type);
        if (color1 == color2) {
          try_bypass_or_make_constant(color1_in, clamp);
          break;
        }
      }
      /* remove no-op mix color node when factor is 1.0 */
      if (fac_is_one) {
        try_bypass_or_make_constant(color2_in, clamp);
        break;
      }
      break;
    case NODE_MIX_ADD:
      /* 0 + X (fac 1) == X */
      if (is_zero(color1_in) && fac_is_one) {
        try_bypass_or_make_constant(color2_in, clamp);
      }
      /* X + 0 (fac ?) == X */
      else if (is_zero(color2_in)) {
        try_bypass_or_make_constant(color1_in, clamp);
      }
      break;
    case NODE_MIX_SUB:
      /* X - 0 (fac ?) == X */
      if (is_zero(color2_in)) {
        try_bypass_or_make_constant(color1_in, clamp);
      }
      /* X - X (fac 1) == 0 */
      else if (color1_in->link && color1_in->link == color2_in->link && fac_is_one) {
        make_zero();
      }
      break;
    case NODE_MIX_MUL:
      /* X * 1 (fac ?) == X, 1 * X (fac 1) == X */
      if (is_one(color1_in) && fac_is_one) {
        try_bypass_or_make_constant(color2_in, clamp);
      }
      else if (is_one(color2_in)) {
        try_bypass_or_make_constant(color1_in, clamp);
      }
      /* 0 * ? (fac ?) == 0, ? * 0 (fac 1) == 0 */
      else if (is_zero(color1_in)) {
        make_zero();
      }
      else if (is_zero(color2_in) && fac_is_one) {
        make_zero();
      }
      break;
    case NODE_MIX_DIV:
      /* X / 1 (fac ?) == X */
      if (is_one(color2_in)) {
        try_bypass_or_make_constant(color1_in, clamp);
      }
      /* 0 / ? (fac ?) == 0 */
      else if (is_zero(color1_in)) {
        make_zero();
      }
      break;
    default:
      break;
  }
}

void ConstantFolder::fold_mix_color(NodeMix type, bool clamp_factor, bool clamp) const
{
  ShaderInput *fac_in = node->input("Factor");
  ShaderInput *color1_in = node->input("A");
  ShaderInput *color2_in = node->input("B");

  float fac = clamp_factor ? saturatef(node->get_float(fac_in->socket_type)) :
                             node->get_float(fac_in->socket_type);
  bool fac_is_zero = !fac_in->link && fac == 0.0f;
  bool fac_is_one = !fac_in->link && fac == 1.0f;

  /* remove no-op node when factor is 0.0 */
  if (fac_is_zero) {
    /* note that some of the modes will clamp out of bounds values even without use_clamp */
    if (!(type == NODE_MIX_LIGHT || type == NODE_MIX_DODGE || type == NODE_MIX_BURN)) {
      if (try_bypass_or_make_constant(color1_in, clamp)) {
        return;
      }
    }
  }

  switch (type) {
    case NODE_MIX_BLEND:
      /* remove useless mix colors nodes */
      if (color1_in->link && color2_in->link) {
        if (color1_in->link == color2_in->link) {
          try_bypass_or_make_constant(color1_in, clamp);
          break;
        }
      }
      else if (!color1_in->link && !color2_in->link) {
        float3 color1 = node->get_float3(color1_in->socket_type);
        float3 color2 = node->get_float3(color2_in->socket_type);
        if (color1 == color2) {
          try_bypass_or_make_constant(color1_in, clamp);
          break;
        }
      }
      /* remove no-op mix color node when factor is 1.0 */
      if (fac_is_one) {
        try_bypass_or_make_constant(color2_in, clamp);
        break;
      }
      break;
    case NODE_MIX_ADD:
      /* 0 + X (fac 1) == X */
      if (is_zero(color1_in) && fac_is_one) {
        try_bypass_or_make_constant(color2_in, clamp);
      }
      /* X + 0 (fac ?) == X */
      else if (is_zero(color2_in)) {
        try_bypass_or_make_constant(color1_in, clamp);
      }
      break;
    case NODE_MIX_SUB:
      /* X - 0 (fac ?) == X */
      if (is_zero(color2_in)) {
        try_bypass_or_make_constant(color1_in, clamp);
      }
      /* X - X (fac 1) == 0 */
      else if (color1_in->link && color1_in->link == color2_in->link && fac_is_one) {
        make_zero();
      }
      break;
    case NODE_MIX_MUL:
      /* X * 1 (fac ?) == X, 1 * X (fac 1) == X */
      if (is_one(color1_in) && fac_is_one) {
        try_bypass_or_make_constant(color2_in, clamp);
      }
      else if (is_one(color2_in)) {
        try_bypass_or_make_constant(color1_in, clamp);
      }
      /* 0 * ? (fac ?) == 0, ? * 0 (fac 1) == 0 */
      else if (is_zero(color1_in)) {
        make_zero();
      }
      else if (is_zero(color2_in) && fac_is_one) {
        make_zero();
      }
      break;
    case NODE_MIX_DIV:
      /* X / 1 (fac ?) == X */
      if (is_one(color2_in)) {
        try_bypass_or_make_constant(color1_in, clamp);
      }
      /* 0 / ? (fac ?) == 0 */
      else if (is_zero(color1_in)) {
        make_zero();
      }
      break;
    default:
      break;
  }
}

void ConstantFolder::fold_mix_float(bool clamp_factor, bool clamp) const
{
  ShaderInput *fac_in = node->input("Factor");
  ShaderInput *float1_in = node->input("A");
  ShaderInput *float2_in = node->input("B");

  float fac = clamp_factor ? saturatef(node->get_float(fac_in->socket_type)) :
                             node->get_float(fac_in->socket_type);
  bool fac_is_zero = !fac_in->link && fac == 0.0f;
  bool fac_is_one = !fac_in->link && fac == 1.0f;

  /* remove no-op node when factor is 0.0 */
  if (fac_is_zero) {
    if (try_bypass_or_make_constant(float1_in, clamp)) {
      return;
    }
  }

  /* remove useless mix floats nodes */
  if (float1_in->link && float2_in->link) {
    if (float1_in->link == float2_in->link) {
      try_bypass_or_make_constant(float1_in, clamp);
      return;
    }
  }
  else if (!float1_in->link && !float2_in->link) {
    float value1 = node->get_float(float1_in->socket_type);
    float value2 = node->get_float(float2_in->socket_type);
    if (value1 == value2) {
      try_bypass_or_make_constant(float1_in, clamp);
      return;
    }
  }
  /* remove no-op mix float node when factor is 1.0 */
  if (fac_is_one) {
    try_bypass_or_make_constant(float2_in, clamp);
    return;
  }
}

void ConstantFolder::fold_math(NodeMathType type) const
{
  ShaderInput *value1_in = node->input("Value1");
  ShaderInput *value2_in = node->input("Value2");

  switch (type) {
    case NODE_MATH_ADD:
      /* X + 0 == 0 + X == X */
      if (is_zero(value1_in)) {
        try_bypass_or_make_constant(value2_in);
      }
      else if (is_zero(value2_in)) {
        try_bypass_or_make_constant(value1_in);
      }
      break;
    case NODE_MATH_SUBTRACT:
      /* X - 0 == X */
      if (is_zero(value2_in)) {
        try_bypass_or_make_constant(value1_in);
      }
      break;
    case NODE_MATH_MULTIPLY:
      /* X * 1 == 1 * X == X */
      if (is_one(value1_in)) {
        try_bypass_or_make_constant(value2_in);
      }
      else if (is_one(value2_in)) {
        try_bypass_or_make_constant(value1_in);
      }
      /* X * 0 == 0 * X == 0 */
      else if (is_zero(value1_in) || is_zero(value2_in)) {
        make_zero();
      }
      break;
    case NODE_MATH_DIVIDE:
      /* X / 1 == X */
      if (is_one(value2_in)) {
        try_bypass_or_make_constant(value1_in);
      }
      /* 0 / X == 0 */
      else if (is_zero(value1_in)) {
        make_zero();
      }
      break;
    case NODE_MATH_POWER:
      /* 1 ^ X == X ^ 0 == 1 */
      if (is_one(value1_in) || is_zero(value2_in)) {
        make_one();
      }
      /* X ^ 1 == X */
      else if (is_one(value2_in)) {
        try_bypass_or_make_constant(value1_in);
      }
    default:
      break;
  }
}

void ConstantFolder::fold_vector_math(NodeVectorMathType type) const
{
  ShaderInput *vector1_in = node->input("Vector1");
  ShaderInput *vector2_in = node->input("Vector2");
  ShaderInput *scale_in = node->input("Scale");

  switch (type) {
    case NODE_VECTOR_MATH_ADD:
      /* X + 0 == 0 + X == X */
      if (is_zero(vector1_in)) {
        try_bypass_or_make_constant(vector2_in);
      }
      else if (is_zero(vector2_in)) {
        try_bypass_or_make_constant(vector1_in);
      }
      break;
    case NODE_VECTOR_MATH_SUBTRACT:
      /* X - 0 == X */
      if (is_zero(vector2_in)) {
        try_bypass_or_make_constant(vector1_in);
      }
      break;
    case NODE_VECTOR_MATH_MULTIPLY:
      /* X * 0 == 0 * X == 0 */
      if (is_zero(vector1_in) || is_zero(vector2_in)) {
        make_zero();
      } /* X * 1 == 1 * X == X */
      else if (is_one(vector1_in)) {
        try_bypass_or_make_constant(vector2_in);
      }
      else if (is_one(vector2_in)) {
        try_bypass_or_make_constant(vector1_in);
      }
      break;
    case NODE_VECTOR_MATH_DIVIDE:
      /* X / 0 == 0 / X == 0 */
      if (is_zero(vector1_in) || is_zero(vector2_in)) {
        make_zero();
      } /* X / 1 == X */
      else if (is_one(vector2_in)) {
        try_bypass_or_make_constant(vector1_in);
      }
      break;
    case NODE_VECTOR_MATH_DOT_PRODUCT:
    case NODE_VECTOR_MATH_CROSS_PRODUCT:
      /* X * 0 == 0 * X == 0 */
      if (is_zero(vector1_in) || is_zero(vector2_in)) {
        make_zero();
      }
      break;
    case NODE_VECTOR_MATH_LENGTH:
    case NODE_VECTOR_MATH_ABSOLUTE:
      if (is_zero(vector1_in)) {
        make_zero();
      }
      break;
    case NODE_VECTOR_MATH_SCALE:
      /* X * 0 == 0 * X == 0 */
      if (is_zero(vector1_in) || is_zero(scale_in)) {
        make_zero();
      } /* X * 1 == X */
      else if (is_one(scale_in)) {
        try_bypass_or_make_constant(vector1_in);
      }
      break;
    default:
      break;
  }
}

void ConstantFolder::fold_mapping(NodeMappingType type) const
{
  ShaderInput *vector_in = node->input("Vector");
  ShaderInput *location_in = node->input("Location");
  ShaderInput *rotation_in = node->input("Rotation");
  ShaderInput *scale_in = node->input("Scale");

  if (is_zero(scale_in)) {
    make_zero();
  }
  else if (
      /* Can't constant fold since we always need to normalize the output. */
      (type != NODE_MAPPING_TYPE_NORMAL) &&
      /* Check all use values are zero, note location is not used by vector and normal types. */
      (is_zero(location_in) || type == NODE_MAPPING_TYPE_VECTOR ||
       type == NODE_MAPPING_TYPE_NORMAL) &&
      is_zero(rotation_in) && is_one(scale_in))
  {
    try_bypass_or_make_constant(vector_in);
  }
}

CCL_NAMESPACE_END
