/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(eevee_volume_lib.glsl)

/* Needed includes for shader nodes. */
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_attribute_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_attributes_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_nodetree_lib.glsl)

/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

/* Store volumetric properties into the froxel textures. */

GlobalData init_globals(vec3 wP)
{
  GlobalData surf;
  surf.P = wP;
  surf.N = vec3(0.0);
  surf.Ng = vec3(0.0);
  surf.is_strand = false;
  surf.hair_time = 0.0;
  surf.hair_thickness = 0.0;
  surf.hair_strand_id = 0;
  surf.barycentric_coords = vec2(0.0);
  surf.barycentric_dists = vec3(0.0);
  surf.ray_type = RAY_TYPE_CAMERA;
  surf.ray_depth = 0.0;
  surf.ray_length = distance(surf.P, cameraPos);
  return surf;
}

#ifndef GPU_METAL
Closure nodetree_volume();
void attrib_load();
#endif

void main()
{
  ivec3 froxel = ivec3(gl_GlobalInvocationID);
#ifdef MAT_GEOM_VOLUME_OBJECT
  froxel += grid_coords_min;
#endif

  if (any(greaterThanEqual(froxel, volumes_info_buf.tex_size))) {
    return;
  }

  vec3 jitter = sampling_rng_3D_get(SAMPLING_VOLUME_U);
  vec3 ndc_cell = volume_to_ndc((vec3(froxel) + jitter) * volumes_info_buf.inv_tex_size);

  vec3 vP = get_view_space_from_depth(ndc_cell.xy, ndc_cell.z);
  vec3 wP = point_view_to_world(vP);
#ifdef MAT_GEOM_VOLUME_OBJECT
  g_lP = point_world_to_object(wP);
  g_orco = OrcoTexCoFactors[0].xyz + g_lP * OrcoTexCoFactors[1].xyz;

  if (any(lessThan(g_orco, vec3(0.0))) || any(greaterThan(g_orco, vec3(1.0)))) {
    return;
  }
#else /* WORLD_SHADER */
  g_orco = wP;
#endif

  g_data = init_globals(wP);
  attrib_load();
  nodetree_volume();

  vec3 scattering = g_volume_scatter_data.scattering;
  vec3 absorption = g_volume_absorption_data.absorption;
  vec3 emission = g_emission;
  float anisotropy = g_volume_scatter_data.anisotropy;
  vec2 phase = vec2(anisotropy, 1.0);

#ifdef MAT_GEOM_VOLUME_OBJECT
  scattering *= drw_volume.density_scale;
  absorption *= drw_volume.density_scale;
  emission *= drw_volume.density_scale;
#endif

  vec3 extinction = scattering + absorption;

  /* Do not add phase weight if there's no scattering. */
  if (all(equal(scattering, vec3(0.0)))) {
    phase = vec2(0.0);
  }

#ifdef MAT_GEOM_VOLUME_OBJECT
  /* Additive Blending.
   * No race condition since each invocation only handles its own froxel. */
  scattering += imageLoad(out_scattering_img, froxel).rgb;
  extinction += imageLoad(out_extinction_img, froxel).rgb;
  emission += imageLoad(out_emissive_img, froxel).rgb;
  phase += imageLoad(out_phase_img, froxel).rg;
#endif

  imageStore(out_scattering_img, froxel, vec4(scattering, 1.0));
  imageStore(out_extinction_img, froxel, vec4(extinction, 1.0));
  imageStore(out_emissive_img, froxel, vec4(emission, 1.0));
  imageStore(out_phase_img, froxel, vec4(phase, vec2(1.0)));
}
