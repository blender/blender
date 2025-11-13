/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

/* Store volumetric properties into the froxel textures. */

#include "infos/eevee_geom_infos.hh"
#include "infos/eevee_surf_volume_infos.hh"

#ifdef GLSL_CPP_STUBS
#  define MAT_VOLUME
#endif

FRAGMENT_SHADER_CREATE_INFO(eevee_geom_mesh)
FRAGMENT_SHADER_CREATE_INFO(eevee_surf_volume)

#include "eevee_volume_lib.glsl"

/* Needed includes for shader nodes. */
#include "eevee_attributes_volume_lib.glsl"
#include "eevee_nodetree_frag_lib.glsl"
#include "eevee_occupancy_lib.glsl"
#include "eevee_sampling_lib.glsl"

GlobalData init_globals(float3 wP)
{
  GlobalData surf;
  surf.P = wP;
  surf.N = float3(0.0f);
  surf.Ng = float3(0.0f);
  surf.is_strand = false;
  surf.hair_diameter = 0.0f;
  surf.hair_strand_id = 0;
  surf.barycentric_coords = float2(0.0f);
  surf.barycentric_dists = float3(0.0f);
  surf.ray_type = RAY_TYPE_CAMERA;
  surf.ray_depth = 0.0f;
  surf.ray_length = distance(surf.P, drw_view_position());
  return surf;
}

struct VolumeProperties {
  float3 scattering;
  float3 absorption;
  float3 emission;
  float anisotropy;
};

VolumeProperties eval_froxel(int3 froxel, float jitter)
{
  float3 uvw = (float3(froxel) + float3(0.5f, 0.5f, 0.5f - jitter)) *
               uniform_buf.volumes.inv_tex_size;

  float3 vP = volume_jitter_to_view(uvw);
  float3 wP = drw_point_view_to_world(vP);
#if !defined(MAT_GEOM_CURVES) && !defined(MAT_GEOM_POINTCLOUD)
#  ifdef GRID_ATTRIBUTES
  g_lP = drw_point_world_to_object(wP);
#  else
  g_wP = wP;
#  endif
  /* TODO(fclem): This is very dangerous as it requires a reset for each time `attrib_load` is
   * called. Instead, the right attribute index should be passed to attr_load_* functions. */
  g_attr_id = 0;
#endif

  g_data = init_globals(wP);
  attrib_load(VolumePoint(0));
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

void write_froxel(int3 froxel, VolumeProperties prop)
{
  float2 phase = float2(prop.anisotropy, 1.0f);

  /* Do not add phase weight if there's no scattering. */
  if (all(equal(prop.scattering, float3(0.0f)))) {
    phase = float2(0.0f);
  }

  float3 extinction = prop.scattering + prop.absorption;

#ifndef MAT_GEOM_WORLD
  /* Additive Blending. No race condition since we have a barrier between each conflicting
   * invocations. */
  prop.scattering += imageLoadFast(out_scattering_img, froxel).rgb;
  prop.emission += imageLoadFast(out_emissive_img, froxel).rgb;
  extinction += imageLoadFast(out_extinction_img, froxel).rgb;
  phase.x += imageLoadFast(out_phase_img, froxel).r;
  phase.y += imageLoadFast(out_phase_weight_img, froxel).r;
#endif

  imageStoreFast(out_scattering_img, froxel, prop.scattering.xyzz);
  imageStoreFast(out_extinction_img, froxel, extinction.xyzz);
  imageStoreFast(out_emissive_img, froxel, prop.emission.xyzz);
  imageStoreFast(out_phase_img, froxel, phase.xxxx);
  imageStoreFast(out_phase_weight_img, froxel, phase.yyyy);
}

void main()
{
  int3 froxel = int3(int2(gl_FragCoord.xy), 0);
  float offset = sampling_rng_1D_get(SAMPLING_VOLUME_W);
  float jitter = volume_froxel_jitter(froxel.xy, offset);

#ifdef VOLUME_HOMOGENOUS
  /* Homogenous volumes only evaluate properties at volume entrance and write the same values for
   * each froxel. */
  VolumeProperties prop = eval_froxel(froxel, jitter);
#endif

#ifndef MAT_GEOM_WORLD
  occupancy::Bits occupancy;
  for (int j = 0; j < 8; j++) {
    occupancy.bits[j] = imageLoad(occupancy_img, int3(froxel.xy, j)).r;
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
