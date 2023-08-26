/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_bsdf_hair(vec4 color,
                    float offset,
                    float roughness_u,
                    float roughness_v,
                    vec3 T,
                    float weight,
                    out Closure result)
{
#if 0
  /* NOTE(fclem): This is the way it should be. But we don't have proper implementation of the hair
   * closure yet. For now fall back to a simpler diffuse surface so that we have at least a color
   * feedback. */
  ClosureHair hair_data;
  hair_data.weight = weight;
  hair_data.color = color.rgb;
  hair_data.offset = offset;
  hair_data.roughness = vec2(roughness_u, roughness_v);
  hair_data.T = T;
#else
  ClosureDiffuse hair_data;
  hair_data.weight = weight;
  hair_data.color = color.rgb;
  hair_data.N = g_data.N;
  hair_data.sss_radius = vec3(0.0);
  hair_data.sss_id = 0u;
#endif
  result = closure_eval(hair_data);
}

void node_bsdf_hair_principled(vec4 color,
                               float melanin,
                               float melanin_redness,
                               vec4 tint,
                               vec3 absorption_coefficient,
                               float roughness,
                               float radial_roughness,
                               float coat,
                               float ior,
                               float offset,
                               float aspect_ratio,
                               float R,
                               float TT,
                               float TRT,
                               float random_color,
                               float random_roughness,
                               float random,
                               float weight,
                               out Closure result)
{
  /* Placeholder closure.
   * Some computation will have to happen here just like the Principled BSDF.
   * For now fall back to a simpler diffuse surface so that we have at least a color feedback. */
#if 0
  ClosureHair hair_data;
  hair_data.weight = weight;
  hair_data.color = color.rgb;
  hair_data.offset = offset;
  hair_data.roughness = vec2(0.0);
  hair_data.T = g_data.curve_B;
#else
  ClosureDiffuse hair_data;
  hair_data.weight = weight;
  hair_data.color = color.rgb;
  hair_data.N = g_data.N;
  hair_data.sss_radius = vec3(0.0);
  hair_data.sss_id = 0u;
#endif
  result = closure_eval(hair_data);
}
