/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef OBINFO_LIB
[[node]]
void node_normal_map(float4 tangent, float strength, float3 texnormal, float3 &outnormal)
{
  if (all(equal(tangent, float4(0.0f, 0.0f, 0.0f, 1.0f)))) {
    outnormal = g_data.Ni;
    return;
  }
  tangent *= (FrontFacing ? 1.0f : -1.0f);
  float3 B = tangent.w * cross(g_data.Ni, tangent.xyz);
  B *= (drw_object_infos().flag & OBJECT_NEGATIVE_SCALE) != 0 ? -1.0f : 1.0f;

  /* Apply strength here instead of in node_normal_map_mix for tangent space. */
  texnormal.xy *= strength;
  texnormal.z = mix(1.0f, texnormal.z, saturate(strength));

  outnormal = texnormal.x * tangent.xyz + texnormal.y * B + texnormal.z * g_data.Ni;
  outnormal = normalize(outnormal);
}
#endif

[[node]]
void color_to_normal_new_shading(float3 color, float3 &normal)
{
  normal = float3(2.0f) * color - float3(1.0f);
}

[[node]]
void color_to_blender_normal_new_shading(float3 color, float3 &normal)
{
  normal = float3(2.0f, -2.0f, -2.0f) * color - float3(1.0f);
}

[[node]]
void color_invert_green_channel(float3 color, out float3 result)
{
  result = float3(color.x, -color.y, color.z);
}

[[node]]
void node_normal_map_mix(float strength, float3 newnormal, float3 &outnormal)
{
  outnormal = normalize(mix(g_data.N, newnormal, max(0.0f, strength)));
}
