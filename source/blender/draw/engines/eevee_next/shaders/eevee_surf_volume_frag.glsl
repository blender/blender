/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

/* Store volumetric properties into the froxel textures. */

#pragma BLENDER_REQUIRE(eevee_volume_lib.glsl)

/* Needed includes for shader nodes. */
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_attributes_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_nodetree_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_occupancy_lib.glsl)

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
  surf.ray_length = distance(surf.P, drw_view_position());
  return surf;
}

struct VolumeProperties {
  vec3 scattering;
  vec3 absorption;
  vec3 emission;
  float anisotropy;
};

VolumeProperties eval_froxel(ivec3 froxel, float jitter)
{
  vec3 uvw = (vec3(froxel) + vec3(0.5, 0.5, 0.5 - jitter)) * uniform_buf.volumes.inv_tex_size;

  vec3 vP = volume_jitter_to_view(uvw);
  vec3 wP = point_view_to_world(vP);
#if !defined(MAT_GEOM_CURVES) && !defined(MAT_GEOM_POINT_CLOUD)
#  ifdef GRID_ATTRIBUTES
  g_lP = point_world_to_object(wP);
#  else
  g_wP = wP;
#  endif
  /* TODO(fclem): This is very dangerous as it requires a reset for each time `attrib_load` is
   * called. Instead, the right attribute index should be passed to attr_load_* functions. */
  g_attr_id = 0;
#endif

  g_data = init_globals(wP);
  attrib_load();
  nodetree_volume();

#if defined(MAT_GEOM_VOLUME)
  g_volume_scattering *= drw_volume.density_scale;
  g_volume_absorption *= drw_volume.density_scale;
  g_emission *= drw_volume.density_scale;
#endif

  VolumeProperties prop;
  prop.scattering = g_volume_scattering;
  prop.absorption = g_volume_absorption;
  prop.emission = g_emission;
  prop.anisotropy = g_volume_anisotropy;
  return prop;
}

void write_froxel(ivec3 froxel, VolumeProperties prop)
{
  vec2 phase = vec2(prop.anisotropy, 1.0);

  /* Do not add phase weight if there's no scattering. */
  if (all(equal(prop.scattering, vec3(0.0)))) {
    phase = vec2(0.0);
  }

  vec3 extinction = prop.scattering + prop.absorption;

#ifndef MAT_GEOM_WORLD
  /* Additive Blending. No race condition since we have a barrier between each conflicting
   * invocations. */
  prop.scattering += imageLoad(out_scattering_img, froxel).rgb;
  prop.emission += imageLoad(out_emissive_img, froxel).rgb;
  extinction += imageLoad(out_extinction_img, froxel).rgb;
  phase += imageLoad(out_phase_img, froxel).rg;
#endif

  imageStore(out_scattering_img, froxel, prop.scattering.xyzz);
  imageStore(out_extinction_img, froxel, extinction.xyzz);
  imageStore(out_emissive_img, froxel, prop.emission.xyzz);
  imageStore(out_phase_img, froxel, phase.xyyy);
}

void main()
{
  ivec3 froxel = ivec3(ivec2(gl_FragCoord.xy), 0);
  float offset = sampling_rng_1D_get(SAMPLING_VOLUME_W);
  float jitter = volume_froxel_jitter(froxel.xy, offset);

#ifdef VOLUME_HOMOGENOUS
  /* Homogenous volumes only evaluate properties at volume entrance and write the same values for
   * each froxel. */
  VolumeProperties prop = eval_froxel(froxel, jitter);
#endif

#ifndef MAT_GEOM_WORLD
  OccupancyBits occupancy;
  for (int j = 0; j < 8; j++) {
    occupancy.bits[j] = imageLoad(occupancy_img, ivec3(froxel.xy, j)).r;
  }
#endif

  /* Check all occupancy bits. */
  for (int j = 0; j < 8; j++) {
    for (int i = 0; i < 32; i++) {
      froxel.z = j * 32 + i;

      if (froxel.z >= imageSize(out_scattering_img).z) {
        break;
      }

#ifndef MAT_GEOM_WORLD
      if (((occupancy.bits[j] >> i) & 1u) == 0) {
        continue;
      }
#endif

#ifndef VOLUME_HOMOGENOUS
      /* Heterogeneous volumes evaluate properties at every froxel position. */
      VolumeProperties prop = eval_froxel(froxel, jitter);
#endif
      write_froxel(froxel, prop);
    }
  }
}
