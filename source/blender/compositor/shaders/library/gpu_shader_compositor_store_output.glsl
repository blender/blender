/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The following functions are called to store the given value in the output identified by the
 * given ID. The ID is an unsigned integer that is encoded in a float, so floatBitsToUint is called
 * to get the actual identifier. The functions have an output value as their last argument that is
 * used to establish an output link that is then used to track the nodes that contribute to the
 * output of the compositor node tree.
 *
 * The store_[type] functions are dynamically generated in
 * ShaderOperation::generate_code_for_outputs. */

#include "gpu_shader_compositor_store.glsl"

void node_compositor_store_output_float(const float id, float value, out float out_value)
{
  store_float(floatBitsToUint(id), value);
  out_value = value;
}

void node_compositor_store_output_float2(const float id, float2 value, out float2 out_value)
{
  store_float2(floatBitsToUint(id), value);
  out_value = value;
}

void node_compositor_store_output_float3(const float id, float3 value, out float3 out_value)
{
  store_float3(floatBitsToUint(id), value);
  out_value = value;
}

void node_compositor_store_output_float4(const float id, float4 value, out float4 out_value)
{
  store_float4(floatBitsToUint(id), value);
  out_value = value;
}

void node_compositor_store_output_color(const float id, float4 value, out float4 out_value)
{
  store_color(floatBitsToUint(id), value);
  out_value = value;
}

/* GPUMaterial doesn't support int, so it is passed as a float. */
void node_compositor_store_output_int(const float id, float value, out float out_value)
{
  store_int(floatBitsToUint(id), value);
  out_value = value;
}

/* GPUMaterial doesn't support int2, so it is passed as a float2. */
void node_compositor_store_output_int2(const float id, float2 value, out float2 out_value)
{
  store_int2(floatBitsToUint(id), value);
  out_value = value;
}

/* GPUMaterial doesn't support bool, so it is passed as a float. */
void node_compositor_store_output_bool(const float id, float value, out float out_value)
{
  store_bool(floatBitsToUint(id), value);
  out_value = value;
}

/* GPUMaterial doesn't support int, so it is passed as a float. */
void node_compositor_store_output_menu(const float id, float value, out float out_value)
{
  store_menu(floatBitsToUint(id), value);
  out_value = value;
}
