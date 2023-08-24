/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_attribute_color(vec4 attr, out vec4 out_attr)
{
  out_attr = attr_load_color_post(attr);
}

void node_attribute_temperature(vec4 attr, out vec4 out_attr)
{
  float temperature = attr_load_temperature_post(attr.x);
  out_attr.x = temperature;
  out_attr.y = temperature;
  out_attr.z = temperature;
  out_attr.w = 1.0;
}

void node_attribute_density(vec4 attr, out float out_attr)
{
  out_attr = attr.x;
}

void node_attribute_flame(vec4 attr, out float out_attr)
{
  out_attr = attr.x;
}

void node_attribute_uniform(vec4 attr, const float attr_hash, out vec4 out_attr)
{
  /* Temporary solution to support both old UBO attribs and new SSBO loading.
   * Old UBO load is already done through `attr` and will just be passed through. */
  out_attr = attr_load_uniform(attr, floatBitsToUint(attr_hash));
}

vec4 attr_load_layer(const uint attr_hash)
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

  return vec4(0.0);
}

void node_attribute(
    vec4 attr, out vec4 outcol, out vec3 outvec, out float outf, out float outalpha)
{
  outcol = vec4(attr.xyz, 1.0);
  outvec = attr.xyz;
  outf = avg(attr.xyz);
  outalpha = attr.w;
}
