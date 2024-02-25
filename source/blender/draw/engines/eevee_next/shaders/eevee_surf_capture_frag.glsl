/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Surface Capture: Output surface parameters to diverse storage.
 *
 * This is a separate shader to allow custom closure behavior and avoid putting more complexity
 * into other surface shaders.
 */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_vector_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)
#pragma BLENDER_REQUIRE(common_hair_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_surf_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_nodetree_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)

vec4 closure_to_rgba(Closure cl)
{
  return vec4(0.0);
}

void main()
{
  init_globals();

  /* TODO(fclem): Remove random sampling for capture and accumulate color. */
  float closure_rand = 0.5;

  nodetree_surface(closure_rand);

  vec3 albedo = vec3(0.0);

  for (int i = 0; i < CLOSURE_BIN_COUNT; i++) {
    ClosureUndetermined cl = g_closure_get_resolved(i, 1.0);
    if (cl.weight <= 1e-5) {
      continue;
    }
    if (cl.type != CLOSURE_BSDF_TRANSLUCENT_ID &&
        cl.type != CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID)
    {
      albedo += cl.color;
    }
  }

  /* ----- Surfel output ----- */

  if (capture_info_buf.do_surfel_count) {
    /* Generate a surfel only once. This check allow cases where no axis is dominant. */
    vec3 vNg = drw_normal_world_to_view(g_data.Ng);
    bool is_surface_view_aligned = dominant_axis(vNg) == 2;
    if (is_surface_view_aligned) {
      uint surfel_id = atomicAdd(capture_info_buf.surfel_len, 1u);
      if (capture_info_buf.do_surfel_output) {
        surfel_buf[surfel_id].position = g_data.P;
        surfel_buf[surfel_id].normal = gl_FrontFacing ? g_data.Ng : -g_data.Ng;
        surfel_buf[surfel_id].albedo_front = albedo;
        surfel_buf[surfel_id].radiance_direct.front.rgb = g_emission;
        surfel_buf[surfel_id].radiance_direct.front.a = 0.0;
        /* TODO(fclem): 2nd surface evaluation. */
        surfel_buf[surfel_id].albedo_back = is_double_sided ? albedo : vec3(0);
        surfel_buf[surfel_id].radiance_direct.back.rgb = is_double_sided ? g_emission : vec3(0);
        surfel_buf[surfel_id].radiance_direct.back.a = 0.0;
        surfel_buf[surfel_id].double_sided = is_double_sided;

        if (!capture_info_buf.capture_emission) {
          surfel_buf[surfel_id].radiance_direct.front.rgb = vec3(0.0);
          surfel_buf[surfel_id].radiance_direct.back.rgb = vec3(0.0);
        }
      }
    }
  }
}
