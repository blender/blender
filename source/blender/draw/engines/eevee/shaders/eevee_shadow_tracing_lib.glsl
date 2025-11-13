/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Evaluate shadowing using shadow map ray-tracing.
 */

#include "infos/eevee_shadow_pipeline_infos.hh"
#include "infos/eevee_uniform_infos.hh"

SHADER_LIBRARY_CREATE_INFO(eevee_global_ubo)
SHADER_LIBRARY_CREATE_INFO(eevee_shadow_data)

#include "draw_math_geom_lib.glsl"
#include "draw_view_lib.glsl"
#include "eevee_light_lib.glsl"
#include "eevee_sampling_lib.glsl"
#include "eevee_shadow_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_fast_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"
#include "gpu_shader_ray_utils_lib.glsl"

/* ---------------------------------------------------------------------- */
/** \name Shadow Map Tracing loop
 * \{ */

#define SHADOW_TRACING_INVALID_HISTORY FLT_MAX

struct ShadowMapTracingState {
  /* Occluder ray coordinate at previous valid depth sample. */
  float2 occluder_history;
  /* Time slope between previous valid sample (N-1) and the one before that (N-2). */
  float occluder_slope;
  /* Multiplier and bias to the ray step quickly compute ray time. */
  float ray_step_mul;
  float ray_step_bias;
  /* State of the trace. */
  float ray_time;
  bool hit;
};

ShadowMapTracingState shadow_map_trace_init(int sample_count, float step_offset)
{
  ShadowMapTracingState state;
  state.occluder_history = float2(SHADOW_TRACING_INVALID_HISTORY);
  state.occluder_slope = SHADOW_TRACING_INVALID_HISTORY;
  /* We trace the ray in reverse. From 1.0 (light) to 0.0f (shading point). */
  state.ray_step_mul = -1.0f / float(sample_count);
  state.ray_step_bias = 1.0f + step_offset * state.ray_step_mul;
  state.hit = false;
  return state;
}

struct ShadowTracingSample {
  /**
   * Occluder position in ray space.
   * `x` component is just the normalized distance from the ray start to the ray end.
   * `y` component is signed distance to the ray, positive if on the light side of the ray.
   */
  float2 occluder;
  bool skip_sample;
};

/**
 * We trace from a point on the light towards the shading point.
 *
 * This reverse tracing allows to approximate the geometry behind occluders while minimizing
 * light-leaks.
 */
void shadow_map_trace_hit_check(inout ShadowMapTracingState state,
                                ShadowTracingSample samp,
                                bool is_last_sample)
{
  bool is_behind_occluder = samp.occluder.y > 1e-6f;

  if (samp.skip_sample) {
    /* Skip empty tiles since they do not contain actual depth information.
     * Not doing so would change the z gradient history. */
  }
  else if (state.occluder_history.x == SHADOW_TRACING_INVALID_HISTORY) {
    /* First sample, regular depth compare since we do not have history values. */
    state.hit = is_behind_occluder || (is_last_sample && (samp.occluder.x > state.ray_time));
    state.occluder_history = samp.occluder;
  }
  else if (is_behind_occluder && (state.occluder_slope != SHADOW_TRACING_INVALID_HISTORY)) {
    /* Extrapolate last known valid occluder and check if it crossed the ray.
     * Note that we only want to check if the extrapolated occluder is above the ray at a certain
     * time value, we don't actually care about the correct value. So we replace the complex
     * problem of trying to get the extrapolation in shadow map space into the extrapolation at
     * ray_time in ray space. This is equivalent as both functions have the same roots. */
    float delta_time = state.ray_time - state.occluder_history.x;
    float extrapolated_occluder_y = abs(state.occluder_history.y) +
                                    state.occluder_slope * delta_time;
    /* NOTE: We use the absolute of the function to account for all occluders configurations.
     * The test just checks if it doesn't extrapolate in the other Y region. */
    state.hit = extrapolated_occluder_y < 0.0f;
  }
  else {
    /* Compute current occluder slope and record history for when the ray goes behind a surface. */
    float2 delta = samp.occluder - state.occluder_history;
    /* Clamping the slope to a minimum avoid light leaking. */
    /* TODO(@fclem): Expose as parameter? */
    const float min_slope = tan(M_PI * 0.25f);
    state.occluder_slope = max(min_slope, abs(delta.y / delta.x));
    state.occluder_history = samp.occluder;
    /* Intersection test. Intersect if above the ray time. */
    state.hit = is_behind_occluder || (is_last_sample && (samp.occluder.x > state.ray_time));
  }
}

/**
 * This need to be instantiated for each `ShadowRay*` type.
 * This way we can implement `shadow_map_trace_sample` for each type without too much code
 * duplication.
 * Most of the code is wrapped into functions to avoid to debug issues inside macro code.
 */
template<typename ShadowRayType>
bool shadow_map_trace(ShadowRayType ray, int sample_count, float step_offset)
{
  ShadowMapTracingState state = shadow_map_trace_init(sample_count, step_offset);
  for (int i = 0; (i <= sample_count) && (i <= SHADOW_MAX_STEP) && (state.hit == false); i++)
  { /* Saturate to always cover the shading point position when i == sample_count. */
    state.ray_time = square(saturate(float(i) * state.ray_step_mul + state.ray_step_bias));

    ShadowTracingSample samp = shadow_map_trace_sample(state, ray);

    shadow_map_trace_hit_check(state, samp, i == sample_count);
  }
  return state.hit;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Directional Shadow Map Tracing
 * \{ */

struct ShadowRayDirectional {
  /* Ray in light rotated space. But not translated. */
  float3 origin;
  float3 direction;
  /* Convert form local light position to ray oriented position where X axis is the ray. */
  float3 local_ray_up;
  LightData light;
};

/* `lP` is supposed to be in light rotated space. But not translated. */
ShadowRayDirectional shadow_ray_generate_directional(
    LightData light, float2 random_2d, float3 lP, float3 lNg, float texel_radius)
{
  float clip_near = orderedIntBitsToFloat(light.clip_near);
  /* Assumed to be non-null. */
  float dist_to_near_plane = -lP.z - clip_near;
  /* Trace in a radius that is covered by low resolution page inflation. */
  float max_tracing_distance = texel_radius * float(SHADOW_PAGE_RES << SHADOW_TILEMAP_LOD);
  /* TODO(fclem): Remove atan here. We only need the cosine of the angle. */
  float max_tracing_angle = atan_fast(max_tracing_distance / dist_to_near_plane);
  float shadow_angle = min(light_sun_data_get(light).shadow_angle, max_tracing_angle);

  /* Light shape is 1 unit away from the shading point. */
  float3 direction = sample_uniform_cone(random_2d, cos(shadow_angle));

  /* It only make sense to trace where there can be occluder. Clamp by distance to near plane. */
  direction *= max(texel_radius, dist_to_near_plane / direction.z);

  ShadowRayDirectional ray;
  ray.origin = lP;
  ray.direction = direction;
  ray.light = light;
  /* TODO(fclem): We can simplify this using the ray direction construction. */
  ray.local_ray_up = safe_normalize(
      cross(cross(float3(0.0f, 0.0f, -1.0f), ray.direction), ray.direction));
  return ray;
}

ShadowTracingSample shadow_map_trace_sample(ShadowMapTracingState state,
                                            inout ShadowRayDirectional ray)
{
  /* Ray position is ray local position with origin at light origin. */
  float3 ray_pos = ray.origin + ray.direction * state.ray_time;

  ShadowCoordinates coord = shadow_directional_coordinates(ray.light, ray_pos);

  float depth = shadow_read_depth(shadow_atlas_tx, shadow_tilemaps_tx, coord);
  /* Distance from near plane. */
  float clip_near = orderedIntBitsToFloat(ray.light.clip_near);
  float3 occluder_pos = float3(ray_pos.xy, -depth - clip_near);
  /* Transform to ray local space. */
  float3 ray_local_occluder = occluder_pos - ray.origin;

  ShadowTracingSample samp;
  samp.occluder.x = dot(ray_local_occluder, ray.direction) / length_squared(ray.direction);
  samp.occluder.y = dot(ray_local_occluder, ray.local_ray_up);
  samp.skip_sample = (depth == -1.0f);
  return samp;
}

template bool shadow_map_trace<ShadowRayDirectional>(ShadowRayDirectional, int, float);

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Punctual Shadow Map Tracing
 * \{ */

struct ShadowRayPunctual {
  /* Light space shadow ray origin and direction. */
  float3 origin;
  float3 direction;
  /* Convert form local light position to ray oriented position where X axis is the ray. */
  float3 local_ray_up;
  /* Tile-map to sample. */
  int light_tilemap_index;
  LightData light;
};

/* Return ray in UV clip space [0..1]. */
ShadowRayPunctual shadow_ray_generate_punctual(LightData light,
                                               float2 random_2d,
                                               float3 lP,
                                               float3 lNg)
{
  if (light.type == LIGHT_RECT) {
    random_2d = random_2d * 2.0f - 1.0f;
  }
  else {
    random_2d = sample_disk(random_2d);
  }

  float clip_near = intBitsToFloat(light.clip_near);
  float shape_radius = light_spot_data_get(light).shadow_radius;
  /* Clamp to a minimum value to avoid `local_ray_up` being degenerate. Could be revisited as the
   * issue might reappear at different zoom level. */
  shape_radius = max(0.00002f, shape_radius);

  float3 direction;
  if (is_area_light(light.type)) {
    random_2d *= light_area_data_get(light).size * light_area_data_get(light).shadow_scale;

    float3 point_on_light_shape = float3(random_2d, 0.0f);

    direction = point_on_light_shape - lP;
  }
  else {
    float dist;
    float3 lL = normalize_and_get_length(lP, dist);
    /* Disk rotated towards light vector. */
    float3 right, up;
    make_orthonormal_basis(lL, right, up);

    if (is_sphere_light(light.type)) {
      shape_radius = light_sphere_disk_radius(shape_radius, dist);
    }
    random_2d *= shape_radius;

    float3 point_on_light_shape = right * random_2d.x + up * random_2d.y;

    direction = point_on_light_shape - lP;
  }
  float3 shadow_position = light_local_data_get(light).shadow_position;
  /* Clip the ray to not cross the near plane.
   * Avoid traces that starts on tiles that have not been queried, creating noise. */
  float clip_distance = length(lP - shadow_position) - clip_near;
  /* Still clamp to a minimal size to avoid issue with zero length vectors. */
  direction *= saturate(1e-6f + clip_distance * inversesqrt(length_squared(direction)));

  /* Compute the ray again. */
  ShadowRayPunctual ray;
  /* Transform to shadow local space. */
  ray.origin = lP - shadow_position;
  ray.direction = direction + shadow_position;
  ray.light_tilemap_index = light.tilemap_index;
  ray.local_ray_up = safe_normalize(cross(cross(ray.origin, ray.direction), ray.direction));
  ray.light = light;
  return ray;
}

ShadowTracingSample shadow_map_trace_sample(ShadowMapTracingState state,
                                            inout ShadowRayPunctual ray)
{
  float3 receiver_pos = ray.origin + ray.direction * state.ray_time;
  int face_id = shadow_punctual_face_index_get(receiver_pos);
  float3 face_pos = shadow_punctual_local_position_to_face_local(face_id, receiver_pos);
  ShadowCoordinates coord = shadow_punctual_coordinates(ray.light, face_pos, face_id);

  float radial_occluder_depth = shadow_read_depth(shadow_atlas_tx, shadow_tilemaps_tx, coord);
  float3 occluder_pos = receiver_pos * (radial_occluder_depth / length(receiver_pos));

  /* Transform to ray local space. */
  float3 ray_local_occluder = occluder_pos - ray.origin;

  ShadowTracingSample samp;
  samp.occluder.x = dot(ray_local_occluder, ray.direction) / length_squared(ray.direction);
  samp.occluder.y = dot(ray_local_occluder, ray.local_ray_up);
  samp.skip_sample = (radial_occluder_depth == -1.0f);
  return samp;
}

template bool shadow_map_trace<ShadowRayPunctual>(ShadowRayPunctual, int, float);

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Shadow Evaluation
 * \{ */

/* Compute the world space offset of the shading position required for
 * stochastic percentage closer filtering of shadow-maps. */
float3 shadow_pcf_offset(float3 L, float3 Ng, float2 random)
{
  /* Angle between Light and normal. */
  float cos_theta = abs(dot(L, Ng));
  float sin_theta = sin_from_cos(cos_theta);
  /* Slope of the receiver plane with respect to light direction. Equal to `tan(theta)`.
   * Stop at 45° angle to avoid large bias and peter panning artifacts. */
  float cone_height = saturate(sin_theta * safe_rcp(cos_theta));
  /* We choose a random disk distribution because it is rotationally invariant.
   * This saves us the trouble of getting the correct orientation for punctual. */
  float distance_to_center = sqrt(random.x);
  float2 disk_sample = sample_circle(random.y) * distance_to_center;
  /* Set the samples on a cone up to 45 degree. */
  float3 cone_sample = float3(disk_sample, distance_to_center * cone_height);
  /* Setup the cone around the light vector. */
  float3 pcf_offset = from_up_axis(L) * cone_sample;
  /* Offset the cone in normal direction to avoid self shadowing when angle is greater than 45°. */
  pcf_offset += Ng * saturate(sin_theta - cos_theta);
  return pcf_offset;
}

/**
 * Returns the world space radius of a shadow map texel at a given position.
 * This is a smooth (not discretized to the LOD transitions) conservative (always above actual
 * density) estimate value.
 */
float shadow_texel_radius_at_position(LightData light, const bool is_directional, float3 P)
{
  /* For direction, footprint of the sampled clipmap (or cascade) at the given position.
   * For punctual, footprint of the tilemap at given position scaled by the LOD level.
   * Each of these a smooth upper bound estimation (will transition smoothly to the next level). */
  float scale = 1.0f;
  if (is_directional) {
    float3 lP = transform_direction_transposed(light.object_to_world, P);
    lP -= light_position_get(light);
    LightSunData sun = light_sun_data_get(light);
    if (light.type == LIGHT_SUN) {
      /* Simplification of `coverage_get(shadow_directional_level_fractional)`.
       * Do not apply the narrowing since we want the size of the tilemap (not its application
       * radius). */
      scale = length(lP) * 2.0f;
      scale = max(scale * exp2(light.lod_bias), exp2(light.lod_min));
      scale = clamp(scale, exp2(float(sun.clipmap_lod_min)), exp2(float(sun.clipmap_lod_max)));
    }
    else {
      /* Uniform distribution everywhere. No distance scaling.
       * shadow_directional_level_fractional returns the cascade level, but all levels have the
       * same density as the level 0. So the effective density only depends on the `lod_bias`. */
      scale = exp2(float(light_sun_data_get(light).clipmap_lod_min));
    }
  }
  else {
    float3 lP = light_world_to_local_point(light, P);
    lP -= light_local_data_get(light).shadow_position;
    /* Simplification of `exp2(shadow_punctual_level_fractional)`. */
    scale = shadow_punctual_pixel_ratio(light,
                                        lP,
                                        drw_view_is_perspective(),
                                        drw_view_z_distance(P),
                                        uniform_buf.shadow.film_pixel_radius);
    /* This gives the size of pixels at Z = 1. */
    scale = 1.0f / scale;
    scale = min(scale, float(1 << SHADOW_TILEMAP_LOD));
    /* Now scale by distance to the light. */
    scale *= reduce_max(abs(lP));
  }
  /* Pixel bounding radius inside a tilemap of unit scale.
   * Take only half of it because we want the radius and not the diameter. */
  constexpr float texel_radius = M_SQRT2 / SHADOW_MAP_MAX_RES;
  return texel_radius * scale;
}

/**
 * Compute the amount of offset to add to the shading point in the normal direction to avoid self
 * shadowing caused by aliasing artifacts. This is on top of the slope bias computed in the shadow
 * render shader to avoid aliasing issues of other polygons. The slope bias only fixes the self
 * shadowing from the current polygon, which is not enough in cases with adjacent polygons with
 * very different slopes.
 */
float shadow_normal_offset(float3 Ng, float3 L, float texel_radius)
{
  /* Attenuate depending on light angle. */
  float cos_theta = abs(dot(Ng, L));
  float slope_offset = sin_from_cos(cos_theta);

  /* Ng might have been quantized. Compensate the error by scaling the offset. */
  const float max_angular_quantization_error = 0.534f; /* Radians. */
  const float max_error_cos_inv = 1.0f / cos(max_angular_quantization_error);
  /* The scaling is only to fix the self shadowing we need another bias for shadowing of adjacent
   * polygons. */
  const float max_error_adjacent_polygon = 0.195f; /* Eye-balled. */
  float biased_offset = slope_offset * max_error_cos_inv + max_error_adjacent_polygon;

  return biased_offset * texel_radius;
}

float shadow_terminator_offset(float3 N,
                               float3 L,
                               float shadow_terminator_normal_offset,
                               float shadow_terminator_geometry_offset)
{
  const float offset_cutoff = shadow_terminator_geometry_offset;

  if (shadow_terminator_geometry_offset == 0.0) {
    return 0.0;
  }

  float cos_theta = dot(N, L);
  const float offset_amount = saturate(1.0f - cos_theta / offset_cutoff);
  return offset_amount * shadow_terminator_normal_offset;
}

/**
 * Evaluate shadowing by casting rays toward the light direction.
 * Returns light visibility.
 */
float shadow_eval(LightData light,
                  const bool is_directional,
                  const bool is_transmission,
                  bool is_translucent_with_thickness,
                  float thickness, /* Only used if is_transmission is true. */
                  float3 P,
                  float3 Ng,
                  float3 N,
                  float terminator_normal_offset,
                  float terminator_geometry_offset,
                  int ray_count,
                  int ray_step_count)
{
  /* Case of surfel light eval. */
  float3 random_shadow_3d = float3(0.5f);
  float2 random_pcf_2d = float2(0.0f);
#if defined(EEVEE_SAMPLING_DATA) && !defined(GLSL_CPP_STUBS)
  if (true) {
    auto &util_tx = sampler_get(eevee_utility_texture, utility_tx);
    float3 blue_noise_3d = utility_tx_fetch(util_tx, UTIL_TEXEL, UTIL_BLUE_NOISE_LAYER).rgb;
    random_shadow_3d = fract(blue_noise_3d + sampling_rng_3D_get(SAMPLING_SHADOW_U));
    random_pcf_2d = fract(blue_noise_3d.xy + sampling_rng_2D_get(SAMPLING_SHADOW_X));
  }
#endif

  float distance_to_shadow;
  /* Direction towards the shadow center (punctual) or direction (direction).
   * Not the same as the light vector if the shadow is jittered. */
  float3 L;
  if (is_directional) {
    L = light_z_axis(light);
  }
  else {
    L = light_position_get(light) + light_local_data_get(light).shadow_position - P;
    L = normalize_and_get_length(L, distance_to_shadow);
  }

  bool is_facing_light = (dot(Ng, L) > 0.0f);
  /* Still bias the transmission surfaces towards the light if they are facing away. */
  float3 N_bias = (is_transmission && !is_facing_light) ? reflect(Ng, L) : Ng;

  /* Shadow map texel radius at the receiver position. */
  float texel_radius = shadow_texel_radius_at_position(light, is_directional, P);

  if (is_transmission && !is_facing_light) {
    /* Ideally, we should bias using the chosen ray direction. In practice, this conflict with our
     * shadow tile usage tagging system as the sampling position becomes heavily shifted from the
     * tagging position. This is the same thing happening with missing tiles with large radii. */
    P += abs(is_directional ? thickness : min(thickness, distance_to_shadow - 0.01f)) * L;
  }
  /* Avoid self intersection with respect to numerical precision. */
  P = offset_ray(P, N_bias);
  /* Stochastic Percentage Closer Filtering. */
  P += (light.filter_radius * texel_radius) * shadow_pcf_offset(L, Ng, random_pcf_2d);
  /* Add normal bias to avoid aliasing artifacts. */
  P += N_bias * shadow_normal_offset(Ng, L, texel_radius);

  /* Bias more to avoid terminator artifacts. */
  P += N * shadow_terminator_offset(N, L, terminator_normal_offset, terminator_geometry_offset);

  float3 lP = is_directional ? light_world_to_local_direction(light, P) :
                               light_world_to_local_point(light, P);
  float3 lNg = light_world_to_local_direction(light, Ng);
  /* Invert horizon clipping. */
  lNg = (is_transmission) ? -lNg : lNg;
  /* Don't do a any horizon clipping in this case as the closure is lit from both sides. */
  lNg = (is_transmission && is_translucent_with_thickness) ? float3(0.0f) : lNg;

  float surface_hit = 0.0f;
  for (int ray_index = 0; ray_index < ray_count && ray_index < SHADOW_MAX_RAY; ray_index++) {
    float2 random_ray_2d = fract(hammersley_2d(ray_index, ray_count) + random_shadow_3d.xy);

    bool has_hit;
    if (is_directional) {
      ShadowRayDirectional clip_ray = shadow_ray_generate_directional(
          light, random_ray_2d, lP, lNg, texel_radius);
      has_hit = shadow_map_trace(clip_ray, ray_step_count, random_shadow_3d.z);
    }
    else {
      ShadowRayPunctual clip_ray = shadow_ray_generate_punctual(light, random_ray_2d, lP, lNg);
      has_hit = shadow_map_trace(clip_ray, ray_step_count, random_shadow_3d.z);
    }

    surface_hit += float(has_hit);
  }
  /* Average samples. */
  return saturate(1.0f - surface_hit / float(ray_count));
}

/** \} */
