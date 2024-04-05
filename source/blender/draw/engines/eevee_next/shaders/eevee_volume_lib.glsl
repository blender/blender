/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * The resources expected to be defined are:
 * - uniform_buf.volumes
 */

#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_spherical_harmonics_lib.glsl)

/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

/* Per froxel jitter to break slices and flickering.
 * Wrapped so that changing it is easier. */
float volume_froxel_jitter(ivec2 froxel, float offset)
{
  return interlieved_gradient_noise(vec2(froxel), 0.0, offset);
}

/* Volume froxel texture normalized linear Z to view space Z.
 * Not dependant on projection matrix (as long as drw_view_is_perspective is consistent). */
float volume_z_to_view_z(float z)
{
  float near = uniform_buf.volumes.depth_near;
  float far = uniform_buf.volumes.depth_far;
  float distribution = uniform_buf.volumes.depth_distribution;
  if (drw_view_is_perspective()) {
    /* Exponential distribution. */
    return (exp2(z / distribution) - near) / far;
  }
  else {
    /* Linear distribution. */
    return near + (far - near) * z;
  }
}

/* View space Z to volume froxel texture normalized linear Z.
 * Not dependant on projection matrix (as long as drw_view_is_perspective is consistent). */
float view_z_to_volume_z(float depth)
{
  float near = uniform_buf.volumes.depth_near;
  float far = uniform_buf.volumes.depth_far;
  float distribution = uniform_buf.volumes.depth_distribution;
  if (drw_view_is_perspective()) {
    /* Exponential distribution. */
    return distribution * log2(depth * far + near);
  }
  else {
    /* Linear distribution. */
    return (depth - near) / (far - near);
  }
}

/* Jittered volume texture normalized coordinates to view space position. */
vec3 volume_jitter_to_view(vec3 coord)
{
  /* Since we use an infinite projection matrix for rendering inside the jittered volumes,
   * we need to use a different matrix to reconstruct positions as the infinite matrix is not
   * always invertible. */
  mat4x4 winmat = uniform_buf.volumes.winmat_finite;
  mat4x4 wininv = uniform_buf.volumes.wininv_finite;
  /* Input coordinates are in jittered volume texture space. */
  float view_z = volume_z_to_view_z(coord.z);
  /* We need to recover the NDC position for correct perspective divide. */
  float ndc_z = drw_perspective_divide(winmat * vec4(0.0, 0.0, view_z, 1.0)).z;
  vec2 ndc_xy = drw_screen_to_ndc(coord.xy);
  /* NDC to view. */
  return drw_perspective_divide(wininv * vec4(ndc_xy, ndc_z, 1.0)).xyz;
}

/* View space position to jittered volume texture normalized coordinates. */
vec3 volume_view_to_jitter(vec3 vP)
{
  /* Since we use an infinite projection matrix for rendering inside the jittered volumes,
   * we need to use a different matrix to reconstruct positions as the infinite matrix is not
   * always invertible. */
  mat4x4 winmat = uniform_buf.volumes.winmat_finite;
  /* View to ndc. */
  vec3 ndc_P = drw_perspective_divide(winmat * vec4(vP, 1.0));
  /* Here, screen is the same as volume texture UVW space. */
  return vec3(drw_ndc_to_screen(ndc_P.xy), view_z_to_volume_z(vP.z));
}

/* Volume texture normalized coordinates (UVW) to render screen (UV).
 * Expect active view to be the main view. */
vec3 volume_resolve_to_screen(vec3 coord)
{
  coord.z = volume_z_to_view_z(coord.z);
  coord.z = drw_depth_view_to_screen(coord.z);
  coord.xy /= uniform_buf.volumes.coord_scale;
  return coord;
}
/* Render screen (UV) to volume texture normalized coordinates (UVW).
 * Expect active view to be the main view. */
vec3 volume_screen_to_resolve(vec3 coord)
{
  coord.xy *= uniform_buf.volumes.coord_scale;
  coord.z = drw_depth_screen_to_view(coord.z);
  coord.z = view_z_to_volume_z(coord.z);
  return coord;
}

/* Returns the uvw (normalized coordinate) of a froxel in the previous frame.
 * If no history exists, it will return out of bounds sampling coordinates. */
vec3 volume_history_position_get(ivec3 froxel)
{
  /* We can't reproject by a simple matrix multiplication. We first need to remap to the view Z,
   * then transform, then remap back to Volume range. */
  vec3 uvw = (vec3(froxel) + 0.5) * uniform_buf.volumes.inv_tex_size;
  uvw.z = volume_z_to_view_z(uvw.z);

  vec3 uvw_history = transform_point(uniform_buf.volumes.history_matrix, uvw);
  /* TODO(fclem): For now assume same distribution settings. */
  uvw_history.z = view_z_to_volume_z(uvw_history.z);
  return uvw_history;
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
    float volume_radius_squared = light_local_data_get(light).radius_squared;
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
    vec3 w_pos = P + L * t;

    vec3 v_pos = drw_point_world_to_view(w_pos);
    vec3 volume_co = volume_view_to_jitter(v_pos);
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
  vec3 coord = volume_screen_to_resolve(ndc_P);

  /* Volumes objects have the same aliasing problems has shadow maps.
   * To fix this we need a quantization bias (the size of a step in Z) and a slope bias
   * (multiplied by the size of a froxel in 2D). */
  coord.z -= uniform_buf.volumes.inv_tex_size.z;
  /* TODO(fclem): Slope bias. */

  VolumeResolveSample volume;
  volume.scattering = texture(scattering_tx, coord).rgb;
  volume.transmittance = texture(transmittance_tx, coord).rgb;
  return volume;
}
