/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * The resources expected to be defined are:
 * - uniform_buf.volumes
 */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_spherical_harmonics_lib.glsl)

/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

/* Volume slice to view space depth. */
float volume_z_to_view_z(float z)
{
  float near = uniform_buf.volumes.depth_near;
  float far = uniform_buf.volumes.depth_far;
  float distribution = uniform_buf.volumes.depth_distribution;
  bool is_persp = ProjectionMatrix[3][3] == 0.0;
  /* Implemented in eevee_shader_shared.cc */
  return volume_z_to_view_z(near, far, distribution, is_persp, z);
}

float view_z_to_volume_z(float depth)
{
  float near = uniform_buf.volumes.depth_near;
  float far = uniform_buf.volumes.depth_far;
  float distribution = uniform_buf.volumes.depth_distribution;
  bool is_persp = ProjectionMatrix[3][3] == 0.0;
  /* Implemented in eevee_shader_shared.cc */
  return view_z_to_volume_z(near, far, distribution, is_persp, depth);
}

/* Volume texture normalized coordinates to screen UVs (special range [0, 1]). */
vec3 volume_to_screen(vec3 coord)
{
  coord.z = volume_z_to_view_z(coord.z);
  coord.z = drw_depth_view_to_screen(coord.z);
  coord.xy /= uniform_buf.volumes.coord_scale;
  return coord;
}

vec3 screen_to_volume(vec3 coord)
{
  float near = uniform_buf.volumes.depth_near;
  float far = uniform_buf.volumes.depth_far;
  float distribution = uniform_buf.volumes.depth_distribution;
  vec2 coord_scale = uniform_buf.volumes.coord_scale;
  /* Implemented in eevee_shader_shared.cc */
  return screen_to_volume(ProjectionMatrix, near, far, distribution, coord_scale, coord);
}

float volume_phase_function_isotropic()
{
  return 1.0 / (4.0 * M_PI);
}

float volume_phase_function(vec3 V, vec3 L, float g)
{
  /* Henyey-Greenstein. */
  float cos_theta = dot(V, L);
  g = clamp(g, -1.0 + 1e-3, 1.0 - 1e-3);
  float sqr_g = g * g;
  return (1 - sqr_g) / max(1e-8, 4.0 * M_PI * pow(1 + sqr_g - 2 * g * cos_theta, 3.0 / 2.0));
}

SphericalHarmonicL1 volume_phase_function_as_sh_L1(vec3 V, float g)
{
  /* Compute rotated zonal harmonic.
   * From Bartlomiej Wronsky
   * "Volumetric Fog: Unified compute shader based solution to atmospheric scattering" page 55
   * SIGGRAPH 2014
   * https://bartwronski.files.wordpress.com/2014/08/bwronski_volumetric_fog_siggraph2014.pdf
   */
  SphericalHarmonicL1 sh;
  sh.L0.M0 = spherical_harmonics_L0_M0(V) * vec4(1.0);
  sh.L1.Mn1 = spherical_harmonics_L1_Mn1(V) * vec4(g);
  sh.L1.M0 = spherical_harmonics_L1_M0(V) * vec4(g);
  sh.L1.Mp1 = spherical_harmonics_L1_Mp1(V) * vec4(g);
  return sh;
}

vec3 volume_light(LightData light, const bool is_directional, LightVector lv)
{
  float power = 1.0;
  if (!is_directional) {
    float volume_radius_squared = light.radius_squared;
    float light_clamp = uniform_buf.volumes.light_clamp;
    if (light_clamp != 0.0) {
      /* 0.0 light clamp means it's disabled. */
      float max_power = reduce_max(light.color) * light.power[LIGHT_VOLUME];
      if (max_power > 0.0) {
        /* The limit of the power attenuation function when the distance to the light goes to 0 is
         * `2 / r^2` where r is the light radius. We need to find the right radius that emits at
         * most the volume light upper bound. Inverting the function we get: */
        float min_radius_squared = 1.0 / (0.5 * light_clamp / max_power);
        /* Square it here to avoid a multiplication inside the shader. */
        volume_radius_squared = max(volume_radius_squared, min_radius_squared);
      }
    }

    /**
     * Using "Point Light Attenuation Without Singularity" from Cem Yuksel
     * http://www.cemyuksel.com/research/pointlightattenuation/pointlightattenuation.pdf
     * http://www.cemyuksel.com/research/pointlightattenuation/
     */
    float d = lv.dist;
    float d_sqr = square(d);
    float r_sqr = volume_radius_squared;

    /* Using reformulation that has better numerical precision. */
    power = 2.0 / (d_sqr + r_sqr + d * sqrt(d_sqr + r_sqr));

    if (light.type == LIGHT_RECT || light.type == LIGHT_ELLIPSE) {
      /* Modulate by light plane orientation / solid angle. */
      power *= saturate(dot(light._back, lv.L));
    }
  }
  return light.color * light.power[LIGHT_VOLUME] * power;
}

#define VOLUMETRIC_SHADOW_MAX_STEP 128.0

vec3 volume_shadow(
    LightData ld, const bool is_directional, vec3 P, LightVector lv, sampler3D extinction_tx)
{
#if defined(VOLUME_SHADOW)
  if (uniform_buf.volumes.shadow_steps == 0) {
    return vec3(1.0);
  }

  /* Heterogeneous volume shadows. */
  float dd = lv.dist / uniform_buf.volumes.shadow_steps;
  vec3 L = lv.L * lv.dist / uniform_buf.volumes.shadow_steps;

  if (is_directional) {
    /* For sun light we scan the whole frustum. So we need to get the correct endpoints. */
    vec3 ndcP = drw_point_world_to_ndc(P);
    vec3 ndcL = drw_point_world_to_ndc(P + lv.L * lv.dist) - ndcP;

    vec3 ndc_frustum_isect = ndcP + ndcL * line_unit_box_intersect_dist_safe(ndcP, ndcL);

    L = drw_point_ndc_to_world(ndc_frustum_isect) - P;
    L /= uniform_buf.volumes.shadow_steps;
    dd = length(L);
  }

  /* TODO use shadow maps instead. */
  vec3 shadow = vec3(1.0);
  for (float t = 1.0; t < VOLUMETRIC_SHADOW_MAX_STEP && t <= uniform_buf.volumes.shadow_steps;
       t += 1.0)
  {
    vec3 pos = P + L * t;

    vec3 ndc = drw_point_world_to_ndc(pos);
    vec3 volume_co = screen_to_volume(drw_ndc_to_screen(ndc));
    /* Let the texture be clamped to edge. This reduce visual glitches. */
    vec3 s_extinction = texture(extinction_tx, volume_co).rgb;

    shadow *= exp(-s_extinction * dd);
  }
  return shadow;
#else
  return vec3(1.0);
#endif /* VOLUME_SHADOW */
}

struct VolumeResolveSample {
  vec3 transmittance;
  vec3 scattering;
};

VolumeResolveSample volume_resolve(vec3 ndc_P, sampler3D transmittance_tx, sampler3D scattering_tx)
{
  vec3 coord = screen_to_volume(ndc_P);

  VolumeResolveSample volume;
  volume.scattering = texture(scattering_tx, coord).rgb;
  volume.transmittance = texture(transmittance_tx, coord).rgb;
  return volume;
}
