/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/workbench_prepass_info.hh"

VERTEX_SHADER_CREATE_INFO(workbench_prepass)
VERTEX_SHADER_CREATE_INFO(workbench_lighting_flat)
VERTEX_SHADER_CREATE_INFO(workbench_transparent_accum)
VERTEX_SHADER_CREATE_INFO(workbench_curves)

#include "draw_curves_lib.glsl"
#include "draw_model_lib.glsl"
#include "draw_view_clipping_lib.glsl"
#include "draw_view_lib.glsl"
#include "workbench_common_lib.glsl"
#include "workbench_image_lib.glsl"
#include "workbench_material_lib.glsl"

/* From http://libnoise.sourceforge.net/noisegen/index.html */
float integer_noise(int n)
{
  /* Integer bit-shifts cause precision issues due to overflow
   * in a number of workbench tests. Use uint instead. */
  uint nn = (uint(n) >> 13u) ^ uint(n);
  nn = (nn * (nn * nn * 60493u + 19990303u) + 1376312589u) & 0x7fffffffu;
  return (float(nn) / 1073741824.0f);
}

float3 workbench_hair_random_normal(float3 tangent, float3 binor, float rand)
{
  /* To "simulate" anisotropic shading, randomize hair normal per strand. */
  float3 nor = cross(tangent, binor);
  nor = normalize(mix(nor, -tangent, rand * 0.1f));
  float cos_theta = (rand * 2.0f - 1.0f) * 0.2f;
  float sin_theta = sqrt(max(0.0f, 1.0f - cos_theta * cos_theta));
  nor = nor * sin_theta + binor * cos_theta;
  return nor;
}

void workbench_hair_random_material(float rand,
                                    inout float3 color,
                                    inout float roughness,
                                    inout float metallic)
{
  /* Center noise around 0. */
  rand -= 0.5f;
  rand *= 0.1f;
  /* Add some variation to the hairs to avoid uniform look. */
  metallic = clamp(metallic + rand, 0.0f, 1.0f);
  roughness = clamp(roughness + rand, 0.0f, 1.0f);
  /* Modulate by color intensity to reduce very high contrast when color is dark. */
  color = clamp(color + rand * (color + 0.05f), 0.0f, 1.0f);
}

void main()
{
  bool is_persp = (drw_view().winmat[3][3] == 0.0f);
  float time = 0.0f, thick_time = 0.0f, thickness = 0.0f;
  float3 world_pos, tangent, binor;
  hair_get_pos_tan_binor_time(is_persp,
                              drw_modelinv(),
                              drw_view().viewinv[3].xyz,
                              drw_view().viewinv[2].xyz,
                              world_pos,
                              tangent,
                              binor,
                              time,
                              thickness,
                              thick_time);

  gl_Position = drw_point_world_to_homogenous(world_pos);

  float hair_rand = integer_noise(hair_get_strand_id());
  float3 nor = workbench_hair_random_normal(tangent, binor, hair_rand);

  view_clipping_distances(world_pos);

  uv_interp = hair_get_customdata_vec2(au);

  normal_interp = normalize(drw_normal_world_to_view(nor));

  workbench_material_data_get(int(drw_custom_id()),
                              hair_get_customdata_vec3(ac),
                              color_interp,
                              alpha_interp,
                              _roughness,
                              metallic);

  /* Hairs have lots of layer and can rapidly become the most prominent surface.
   * So we lower their alpha artificially. */
  alpha_interp *= 0.3f;

  workbench_hair_random_material(hair_rand, color_interp, _roughness, metallic);

  object_id = int(uint(drw_resource_id()) & 0xFFFFu) + 1;

  if (emitter_object_id != 0) {
    object_id = int(uint(emitter_object_id) & 0xFFFFu) + 1;
  }
}
