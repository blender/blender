/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/workbench_prepass_infos.hh"

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

float3 workbench_hair_random_normal(float3 tangent, float3 binor, float3 nor, float rand)
{
  /* To "simulate" anisotropic shading, randomize hair normal per strand. */
  nor = normalize(mix(nor, -tangent, rand * 0.1f));
  float cos_theta = (rand * 2.0f - 1.0f) * 0.2f;
  float sin_theta = sin_from_cos(cos_theta);
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
  metallic = saturate(metallic + rand);
  roughness = saturate(roughness + rand);
  /* Modulate by color intensity to reduce very high contrast when color is dark. */
  color = saturate(color + rand * (color + 0.05f));
}

#if defined(GPU_NVIDIA) && defined(GPU_OPENGL)
/* WORKAROUND: Fix legacy driver compiler issue (see #148472). */
#  define const
#endif

void main()
{
  const curves::Point ls_pt = curves::point_get(uint(gl_VertexID));
  const curves::Point ws_pt = curves::object_to_world(ls_pt, drw_modelmat());
  const curves::ShapePoint pt = curves::shape_point_get(ws_pt, drw_world_incident_vector(ws_pt.P));
  float3 world_pos = pt.P;

  gl_Position = drw_point_world_to_homogenous(world_pos);

  float hair_rand = integer_noise(ws_pt.curve_id);

  float3 nor = pt.N;
  if (drw_curves.half_cylinder_face_count == 1) {
    /* Very cheap smooth normal using attribute interpolator.
     * Using the correct normals over the cylinder (-1..1) leads to unwanted result as the
     * interpolation is not spherical but linear. So we use a smaller range (-SQRT2..SQRT2) in
     * which the linear interpolation is close enough to the desired result. */
    nor = pt.N + pt.curve_N;
  }
  else if (drw_curves.half_cylinder_face_count == 0) {
    nor = workbench_hair_random_normal(pt.curve_T, pt.curve_B, pt.curve_N, hair_rand);
  }

  view_clipping_distances(world_pos);

  uv_interp = curves::get_customdata_vec2(ws_pt.curve_id, au);

  normal_interp = normalize(drw_normal_world_to_view(nor));

  workbench_material_data_get(int(drw_custom_id()),
                              curves::get_customdata_vec3(ws_pt.curve_id, ac),
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
