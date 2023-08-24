/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The following functions are called to store the given value in the output identified by the
 * given ID. The ID is an unsigned integer that is encoded in a float, so floatBitsToUint is called
 * to get the actual identifier. The functions have an output value as their last argument that is
 * used to establish an output link that is then used to track the nodes that contribute to the
 * output of the compositor node tree.
 *
 * The store_[float|vector|color] functions are dynamically generated in
 * ShaderOperation::generate_code_for_outputs. */

void node_compositor_store_output_float(const float id, float value, out float out_value)
{
  store_float(floatBitsToUint(id), value);
  out_value = value;
}

void node_compositor_store_output_vector(const float id, vec3 vector, out vec3 out_vector)
{
  store_vector(floatBitsToUint(id), vector);
  out_vector = vector;
}

void node_compositor_store_output_color(const float id, vec4 color, out vec4 out_color)
{
  store_color(floatBitsToUint(id), color);
  out_color = color;
}
