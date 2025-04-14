/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_common_math.glsl"

void node_attribute_color(float4 attr, out float4 out_attr)
{
  out_attr = attr_load_color_post(attr);
}

void node_attribute_temperature(float4 attr, out float4 out_attr)
{
  float temperature = attr_load_temperature_post(attr.x);
  out_attr.x = temperature;
  out_attr.y = temperature;
  out_attr.z = temperature;
  out_attr.w = 1.0f;
}

void node_attribute_density(float4 attr, out float out_attr)
{
  out_attr = attr.x;
}

void node_attribute_flame(float4 attr, out float out_attr)
{
  out_attr = attr.x;
}

void node_attribute_uniform(float4 attr, const float attr_hash, out float4 out_attr)
{
  /* Temporary solution to support both old UBO attributes and new SSBO loading.
   * Old UBO load is already done through `attr` and will just be passed through. */
  out_attr = attr_load_uniform(attr, floatBitsToUint(attr_hash));
}

float4 attr_load_layer(const uint attr_hash)
{
#ifdef VLATTR_LIB
  /* The first record of the buffer stores the length. */
  uint left = 0, right = drw_layer_attrs[0].buffer_length;

  while (left < right) {
    uint mid = (left + right) / 2;
    uint hash = drw_layer_attrs[mid].hash_code;

    if (hash < attr_hash) {
      left = mid + 1;
    }
    else if (hash > attr_hash) {
      right = mid;
    }
    else {
      return drw_layer_attrs[mid].data;
    }
  }
#endif

  return float4(0.0f);
}

void node_attribute(
    float4 attr, out float4 outcol, out float3 outvec, out float outf, out float outalpha)
{
  outcol = float4(attr.xyz, 1.0f);
  outvec = attr.xyz;
  outf = math_average(attr.xyz);
  outalpha = attr.w;
}
