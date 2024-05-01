/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Evaluate shadowing using shadow map ray-tracing.
 */

#pragma BLENDER_REQUIRE(gpu_shader_math_base_lib.glsl)
#pragma BLENDER_REQUIRE(gpu_shader_math_matrix_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_light_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_shadow_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(eevee_bxdf_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(draw_view_lib.glsl)
#pragma BLENDER_REQUIRE(draw_math_geom_lib.glsl)

float shadow_read_depth_at_tilemap_uv(int tilemap_index, vec2 tilemap_uv)
{
  /* Prevent out of bound access. Assumes the input is already non negative. */
  tilemap_uv = min(tilemap_uv, vec2(0.99999));

  ivec2 texel_coord = ivec2(tilemap_uv * float(SHADOW_MAP_MAX_RES));
  /* Using bitwise ops is way faster than integer ops. */
  const int page_shift = SHADOW_PAGE_LOD;
  const int page_mask = ~(0xFFFFFFFF << SHADOW_PAGE_LOD);

  ivec2 tile_coord = texel_coord >> page_shift;
  ShadowSamplingTile tile = shadow_tile_load(shadow_tilemaps_tx, tile_coord, tilemap_index);

  if (!tile.is_valid) {
    return -1.0;
  }
  /* Shift LOD0 pixels so that they get wrapped at the right position for the given LOD. */
  /* TODO convert everything to uint to avoid signed int operations. */
  texel_coord += ivec2(tile.lod_offset << SHADOW_PAGE_LOD);
  /* Scale to LOD pixels (merge LOD0 pixels together) then mask to get pixel in page. */
  ivec2 texel_page = (texel_coord >> int(tile.lod)) & page_mask;
  ivec3 texel = ivec3((ivec2(tile.page.xy) << page_shift) | texel_page, tile.page.z);

  return uintBitsToFloat(texelFetch(shadow_atlas_tx, texel, 0).r);
}

/* ---------------------------------------------------------------------- */
/** \name Shadow Map Tracing loop
 * \{ */

#define SHADOW_TRACING_INVALID_HISTORY -999.0

struct ShadowMapTracingState {
  /* Receiver Z value at previous valid depth sample. */
  float receiver_depth_history;
  /* Occluder Z value at previous valid depth sample. */
  float occluder_depth_history;
  /* Ray time at previous valid depth sample. */
  float ray_time_history;
  /* Z slope (delta/time) between previous valid sample (N-1) and the one before that (N-2). */
  float occluder_depth_slope;
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
  state.receiver_depth_history = -1.0;
  state.occluder_depth_history = SHADOW_TRACING_INVALID_HISTORY;
  state.ray_time_history = -1.0;
  state.occluder_depth_slope = 0.0;
  /* We trace the ray in reverse. From 1.0 (light) to 0.0 (shading point). */
  state.ray_step_mul = -1.0 / float(sample_count);
  state.ray_step_bias = 1.0 + step_offset * state.ray_step_mul;
  state.hit = false;
  return state;
}

struct ShadowTracingSample {
  float receiver_depth;
  float occluder_depth;
  bool skip_sample;
};

/**
 * This need to be instantiated for each `ShadowRay*` type.
 * This way we can implement `shadow_map_trace_sample` for each type without too much code
 * duplication.
 * Most of the code is wrapped into functions to avoid to debug issues inside macro code.
 */
#define SHADOW_MAP_TRACE_FN(ShadowRayType) \
  ShadowMapTraceResult shadow_map_trace(ShadowRayType ray, int sample_count, float step_offset) \
  { \
    ShadowMapTracingState state = shadow_map_trace_init(sample_count, step_offset); \
    for (int i = 0; (i <= sample_count) && (i <= SHADOW_MAX_STEP) && (state.hit == false); i++) { \
      /* Saturate to always cover the shading point position when i == sample_count. */ \
      state.ray_time = square(saturate(float(i) * state.ray_step_mul + state.ray_step_bias)); \
\
      ShadowTracingSample samp = shadow_map_trace_sample(state, ray); \
\
      shadow_map_trace_hit_check(state, samp); \
    } \
    return shadow_map_trace_finish(state); \
  }

/**
 * We trace from a point on the light towards the shading point.
 *
 * This reverse tracing allows to approximate the geometry behind occluders while minimizing
 * light-leaks.
 */
void shadow_map_trace_hit_check(inout ShadowMapTracingState state, ShadowTracingSample samp)
{
  /* Skip empty tiles since they do not contain actual depth information.
   * Not doing so would change the z gradient history. */
  if (samp.skip_sample) {
    return;
  }
  /* For the first sample, regular depth compare since we do not have history values. */
  if (state.occluder_depth_history == SHADOW_TRACING_INVALID_HISTORY) {
    if (samp.occluder_depth < samp.receiver_depth) {
      state.hit = true;
      return;
    }
    state.occluder_depth_history = samp.occluder_depth;
    state.receiver_depth_history = samp.receiver_depth;
    state.ray_time_history = state.ray_time;
    return;
  }
  /* Delta between previous valid sample. */
  float ray_depth_delta = samp.receiver_depth - state.receiver_depth_history;
  /* Delta between previous valid sample not occluding the ray. */
  float time_delta = state.ray_time - state.ray_time_history;
  /* Arbitrary increase the threshold to avoid missing occluders because of precision issues.
   * Increasing the threshold inflates the occluders. */
  float compare_threshold = abs(ray_depth_delta) * 1.05;
  /* Find out if the ray step is behind an occluder.
   * To be consider behind (and ignore the occluder), the occluder must not be cross the ray.
   * Use the full delta ray depth as threshold to make sure to not miss any occluder. */
  bool is_behind = samp.occluder_depth < (samp.receiver_depth - compare_threshold);

  if (is_behind) {
    /* Use last known valid occluder Z value and extrapolate to the sample position. */
    samp.occluder_depth = state.occluder_depth_history + state.occluder_depth_slope * time_delta;
    /* Intersection test will be against the extrapolated last known occluder. */
  }
  else {
    /* Compute current occluder slope and record history for when the ray goes behind a surface. */
    state.occluder_depth_slope = (samp.occluder_depth - state.occluder_depth_history) / time_delta;
    state.occluder_depth_slope = clamp(state.occluder_depth_slope, -100.0, 100.0);
    state.occluder_depth_history = samp.occluder_depth;
    state.ray_time_history = state.ray_time;
    /* Intersection test will be against the current sample's occluder. */
  }

  if (samp.occluder_depth < samp.receiver_depth) {
    state.occluder_depth_history = samp.occluder_depth;
    state.hit = true;
    return;
  }
  /* No intersection. */
  state.receiver_depth_history = samp.receiver_depth;
}

struct ShadowMapTraceResult {
  bool has_hit;
  float occluder_depth;
};

ShadowMapTraceResult shadow_map_trace_finish(ShadowMapTracingState state)
{
  ShadowMapTraceResult result;
  if (state.hit) {
    result.occluder_depth = state.occluder_depth_history;
    result.has_hit = true;
  }
  else {
    result.occluder_depth = 0.0;
    result.has_hit = false;
  }
  return result;
}

/** \} */

/* If the ray direction `L`  is below the horizon defined by N (normalized) at the shading point,
 * push it just above the horizon so that this ray will never be below it and produce
 * over-shadowing (since light evaluation already clips the light shape). */
vec3 shadow_ray_above_horizon_ensure(vec3 L, vec3 N)
{
  float distance_to_plan = dot(L, -N);
  if (distance_to_plan > 0.0) {
    L += N * (0.01 + distance_to_plan);
  }
  return L;
}

/* ---------------------------------------------------------------------- */
/** \name Directional Shadow Map Tracing
 * \{ */

struct ShadowRayDirectional {
  /* Ray in local translated coordinate, with depth in [0..1] range in W component. */
  vec4 origin;
  vec4 direction;
  LightData light;
};

ShadowRayDirectional shadow_ray_generate_directional(LightData light,
                                                     vec2 random_2d,
                                                     vec3 lP,
                                                     vec3 lNg)
{
  float clip_near = orderedIntBitsToFloat(light.clip_near);
  float clip_far = orderedIntBitsToFloat(light.clip_far);
  /* Assumed to be non-null. */
  float z_range = clip_far - clip_near;
  float dist_to_near_plane = -lP.z - clip_near;

  /* `lP` is supposed to be in light rotated space. But not translated. */
  vec4 origin = vec4(lP, dist_to_near_plane / z_range);

  vec3 disk_direction = sample_uniform_cone(sample_cylinder(random_2d),
                                            light_sun_data_get(light).shadow_angle);

  disk_direction = shadow_ray_above_horizon_ensure(disk_direction, lNg);

  /* Light shape is 1 unit away from the shading point. */
  vec4 direction = vec4(disk_direction, -1.0 / z_range);

  /* It only make sense to trace where there can be occluder. Clamp by distance to near plane. */
  direction *= min(light_sun_data_get(light).shadow_trace_distance,
                   dist_to_near_plane / disk_direction.z);

  ShadowRayDirectional ray;
  ray.origin = origin;
  ray.direction = direction;
  ray.light = light;
  return ray;
}

ShadowTracingSample shadow_map_trace_sample(ShadowMapTracingState state,
                                            inout ShadowRayDirectional ray)
{
  /* Ray position is ray local position with origin at light origin. */
  vec4 ray_pos = ray.origin + ray.direction * state.ray_time;

  int level = shadow_directional_level(ray.light, ray_pos.xyz - light_position_get(ray.light));
  /* This difference needs to be less than 32 for the later shift to be valid.
   * This is ensured by ShadowDirectional::clipmap_level_range(). */
  int level_relative = level - light_sun_data_get(ray.light).clipmap_lod_min;

  int lod_relative = (ray.light.type == LIGHT_SUN_ORTHO) ?
                         light_sun_data_get(ray.light).clipmap_lod_min :
                         level;

  vec2 clipmap_origin = light_sun_data_get(ray.light).clipmap_origin;
  vec2 clipmap_pos = ray_pos.xy - clipmap_origin;
  vec2 tilemap_uv = clipmap_pos * exp2(-float(lod_relative)) + 0.5;

  /* Compute offset in tile. */
  ivec2 clipmap_offset = shadow_decompress_grid_offset(
      ray.light.type,
      light_sun_data_get(ray.light).clipmap_base_offset_neg,
      light_sun_data_get(ray.light).clipmap_base_offset_pos,
      level_relative);
  /* Translate tilemap UVs to its origin. */
  tilemap_uv -= vec2(clipmap_offset) / float(SHADOW_TILEMAP_RES);
  /* Clamp to avoid out of tilemap access. */
  tilemap_uv = saturate(tilemap_uv);

  ShadowTracingSample samp;
  samp.receiver_depth = ray_pos.w;
  samp.occluder_depth = shadow_read_depth_at_tilemap_uv(ray.light.tilemap_index + level_relative,
                                                        tilemap_uv);
  samp.skip_sample = (samp.occluder_depth == -1.0);
  return samp;
}

SHADOW_MAP_TRACE_FN(ShadowRayDirectional)

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Punctual Shadow Map Tracing
 * \{ */

struct ShadowRayPunctual {
  /* Ray in tile-map normalized coordinates [0..1]. */
  vec3 origin;
  vec3 direction;
  /* Tile-map to sample. */
  int tilemap_index;
};

/* Return ray in UV clip space [0..1]. */
ShadowRayPunctual shadow_ray_generate_punctual(LightData light, vec2 random_2d, vec3 lP, vec3 lNg)
{
  if (light.type == LIGHT_RECT) {
    random_2d = random_2d * 2.0 - 1.0;
  }
  else {
    random_2d = sample_disk(random_2d);
  }

  float clip_far = intBitsToFloat(light.clip_far);
  float clip_near = intBitsToFloat(light.clip_near);
  float clip_side = light_local_data_get(light).clip_side;

  /* TODO(fclem): 3D shift for jittered soft shadows. */
  vec3 projection_origin = vec3(0.0, 0.0, -light_local_data_get(light).shadow_projection_shift);
  vec3 direction;
  if (is_area_light(light.type)) {
    random_2d *= light_area_data_get(light).size;

    vec3 point_on_light_shape = vec3(random_2d, 0.0);
    /* Progressively blend the shape back to the projection origin. */
    point_on_light_shape = mix(
        -projection_origin, point_on_light_shape, light_local_data_get(light).shadow_scale);

    direction = point_on_light_shape - lP;
    direction = shadow_ray_above_horizon_ensure(direction, lNg);

    /* Clip the ray to not cross the near plane.
     * Scale it so that it encompass the whole cube (with a safety margin). */
    float clip_distance = clip_near + 0.001;
    float ray_length = max(abs(direction.x), max(abs(direction.y), abs(direction.z)));
    direction *= saturate((ray_length - clip_distance) / ray_length);
  }
  else {
    float dist;
    vec3 L = normalize_and_get_length(lP, dist);
    /* Disk rotated towards light vector. */
    vec3 right, up;
    make_orthonormal_basis(L, right, up);

    float shape_radius = light_spot_data_get(light).radius;
    if (is_sphere_light(light.type)) {
      /* FIXME(weizhen): this is not well-defined when `dist < light.spot.radius`. */
      shape_radius = light_sphere_disk_radius(shape_radius, dist);
    }
    random_2d *= shape_radius;

    random_2d *= light_local_data_get(light).shadow_scale;
    vec3 point_on_light_shape = right * random_2d.x + up * random_2d.y;

    direction = point_on_light_shape - lP;
    direction = shadow_ray_above_horizon_ensure(direction, lNg);

    /* Clip the ray to not cross the light shape. */
    float clip_distance = light_spot_data_get(light).radius;
    direction *= saturate((dist - clip_distance) / dist);
  }

  /* Apply shadow origin shift. */
  vec3 local_ray_start = lP + projection_origin;
  vec3 local_ray_end = local_ray_start + direction;

  /* Use an offset in the ray direction to jitter which face is traced.
   * This helps hiding some harsh discontinuity. */
  int face_id = shadow_punctual_face_index_get(local_ray_start + direction * 0.5);
  /* Local Light Space > Face Local (View) Space. */
  vec3 view_ray_start = shadow_punctual_local_position_to_face_local(face_id, local_ray_start);
  vec3 view_ray_end = shadow_punctual_local_position_to_face_local(face_id, local_ray_end);

  /* Face Local (View) Space > Clip Space. */
  /* TODO: Could be simplified since frustum is completely symmetrical. */
  mat4 winmat = projection_perspective(
      -clip_side, clip_side, -clip_side, clip_side, clip_near, clip_far);
  vec3 clip_ray_start = project_point(winmat, view_ray_start);
  vec3 clip_ray_end = project_point(winmat, view_ray_end);
  /* Clip Space > UV Space. */
  vec3 uv_ray_start = clip_ray_start * 0.5 + 0.5;
  vec3 uv_ray_end = clip_ray_end * 0.5 + 0.5;
  /* Compute the ray again. */
  ShadowRayPunctual ray;
  ray.origin = uv_ray_start;
  ray.direction = uv_ray_end - uv_ray_start;
  ray.tilemap_index = light.tilemap_index + face_id;
  return ray;
}

ShadowTracingSample shadow_map_trace_sample(ShadowMapTracingState state,
                                            inout ShadowRayPunctual ray)
{
  vec3 ray_pos = ray.origin + ray.direction * state.ray_time;
  vec2 tilemap_uv = saturate(ray_pos.xy);

  ShadowTracingSample samp;
  samp.receiver_depth = ray_pos.z;
  samp.occluder_depth = shadow_read_depth_at_tilemap_uv(ray.tilemap_index, tilemap_uv);
  samp.skip_sample = (samp.occluder_depth == -1.0);
  return samp;
}

SHADOW_MAP_TRACE_FN(ShadowRayPunctual)

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Shadow Evaluation
 * \{ */

/* Compute the world space offset of the shading position required for
 * stochastic percentage closer filtering of shadow-maps. */
vec3 shadow_pcf_offset(vec3 L, vec3 Ng, vec2 random)
{
  /* We choose a random disk distribution because it is rotationally invariant.
   * This sames us the trouble of getting the correct orientation for punctual. */
  vec2 disk_sample = sample_disk(random);
  /* Compute the offset as a disk around the normal. */
  mat3x3 tangent_frame = from_up_axis(Ng);
  vec3 pcf_offset = tangent_frame[0] * disk_sample.x + tangent_frame[1] * disk_sample.y;

  if (dot(pcf_offset, L) < 0.0) {
    /* Reflect the offset to avoid overshadowing caused by moving the sampling point below another
     * polygon behind the shading point. */
    pcf_offset = reflect(pcf_offset, L);
  }
  return pcf_offset;
}

/**
 * Returns the world space radius of a shadow map texel at a given position.
 * This is a smooth (not discretized to the LOD transitions) conservative (always above actual
 * density) estimate value.
 */
float shadow_texel_radius_at_position(LightData light, const bool is_directional, vec3 P)
{
  vec3 lP = light_world_to_local_point(light, P);

  float scale = 1.0;
  if (is_directional) {
    LightSunData sun = light_sun_data_get(light);
    if (light.type == LIGHT_SUN) {
      /* Simplification of `coverage_get(shadow_directional_level_fractional)`. */
      const float narrowing = float(SHADOW_TILEMAP_RES) / (float(SHADOW_TILEMAP_RES) - 1.0001);
      scale = length(lP) * narrowing;
      scale *= exp2(light.lod_bias);
      scale = clamp(scale, float(1 << sun.clipmap_lod_min), float(1 << sun.clipmap_lod_max));
    }
    else {
      /* Uniform distribution everywhere. No distance scaling. */
      scale = 1.0 / float(1 << sun.clipmap_lod_min);
    }
  }
  else {
    /* Simplification of `coverage_get(shadow_punctual_level_fractional)`. */
    scale = shadow_punctual_pixel_ratio(light,
                                        lP,
                                        drw_view_is_perspective(),
                                        drw_view_z_distance(P),
                                        uniform_buf.shadow.film_pixel_radius);
    /* This gives the size of pixels at Z = 1. */
    scale = 1.0 / scale;
    scale *= exp2(-1.0 + light.lod_bias);
    scale = clamp(scale, float(1 << 0), float(1 << SHADOW_TILEMAP_LOD));
    scale *= shadow_punctual_frustum_padding_get(light);
    /* Now scale by distance to the light. */
    scale *= reduce_max(abs(lP));
  }
  /* Footprint of a tilemap at unit distance from the camera. */
  const float texel_footprint = 2.0 * M_SQRT2 / SHADOW_MAP_MAX_RES;
  return texel_footprint * scale;
}

/**
 * Compute the amount of offset to add to the shading point in the normal direction to avoid self
 * shadowing caused by aliasing artifacts. This is on top of the slope bias computed in the shadow
 * render shader to avoid aliasing issues of other polygons. The slope bias only fixes the self
 * shadowing from the current polygon, which is not enough in cases with adjacent polygons with
 * very different slopes.
 */
float shadow_normal_offset(vec3 Ng, vec3 L)
{
  /* Attenuate depending on light angle. */
  /* TODO: Should we take the light shape into consideration? */
  float cos_theta = abs(dot(Ng, L));
  float sin_theta = sqrt(saturate(1.0 - square(cos_theta)));
  /* Note that we still bias by one pixel anyway to fight quantization artifacts.
   * This helps with self intersection of slopped surfaces and gives softer soft shadow (?! why).
   * FIXME: This is likely to hide some issue, and we need a user facing bias parameter anyway. */
  return sin_theta + 3.0;
}

/**
 * Evaluate shadowing by casting rays toward the light direction.
 */
ShadowEvalResult shadow_eval(LightData light,
                             const bool is_directional,
                             const bool is_transmission,
                             bool is_translucent_with_thickness,
                             float thickness, /* Only used if is_transmission is true. */
                             vec3 P,
                             vec3 Ng,
                             vec3 L,
                             int ray_count,
                             int ray_step_count)
{
#if defined(EEVEE_SAMPLING_DATA) && defined(EEVEE_UTILITY_TX)
#  ifdef GPU_FRAGMENT_SHADER
  vec2 pixel = floor(gl_FragCoord.xy);
#  elif defined(GPU_COMPUTE_SHADER)
  vec2 pixel = vec2(gl_GlobalInvocationID.xy);
#  endif
  vec3 blue_noise_3d = utility_tx_fetch(utility_tx, pixel, UTIL_BLUE_NOISE_LAYER).rgb;
  vec3 random_shadow_3d = blue_noise_3d + sampling_rng_3D_get(SAMPLING_SHADOW_U);
  vec2 random_pcf_2d = fract(blue_noise_3d.xy + sampling_rng_2D_get(SAMPLING_SHADOW_X));
#else
  /* Case of surfel light eval. */
  vec3 random_shadow_3d = vec3(0.5);
  vec2 random_pcf_2d = vec2(0.0);
#endif

  bool is_facing_light = (dot(Ng, L) > 0.0);
  /* Still bias the transmission surfaces towards the light if they are facing away. */
  vec3 N_bias = (is_transmission && !is_facing_light) ? reflect(Ng, L) : Ng;

  /* Shadow map texel radius at the receiver position. */
  float texel_radius = shadow_texel_radius_at_position(light, is_directional, P);
  /* Stochastic Percentage Closer Filtering. */
  if (is_transmission && !is_facing_light) {
    /* Ideally, we should bias using the chosen ray direction. In practice, this conflict with our
     * shadow tile usage tagging system as the sampling position becomes heavily shifted from the
     * tagging position. This is the same thing happening with missing tiles with large radii. */
    P += abs(thickness) * L;
  }
  /* Avoid self intersection with respect to numerical precision. */
  P = offset_ray(P, N_bias);
  /* Stochastic Percentage Closer Filtering. */
  P += (light.pcf_radius * texel_radius) * shadow_pcf_offset(L, Ng, random_pcf_2d);
  /* Add normal bias to avoid aliasing artifacts. */
  P += N_bias * (texel_radius * shadow_normal_offset(Ng, L));

  vec3 lP = is_directional ? light_world_to_local(light, P) :
                             light_world_to_local(light, P - light_position_get(light));
  vec3 lNg = light_world_to_local(light, Ng);
  /* Invert horizon clipping. */
  lNg = (is_transmission) ? -lNg : lNg;
  /* Don't do a any horizon clipping in this case as the closure is lit from both sides. */
  lNg = (is_transmission && is_translucent_with_thickness) ? vec3(0.0) : lNg;

  float surface_hit = 0.0;
  for (int ray_index = 0; ray_index < ray_count && ray_index < SHADOW_MAX_RAY; ray_index++) {
    vec2 random_ray_2d = fract(hammersley_2d(ray_index, ray_count) + random_shadow_3d.xy);

    ShadowMapTraceResult trace;
    if (is_directional) {
      ShadowRayDirectional clip_ray = shadow_ray_generate_directional(
          light, random_ray_2d, lP, lNg);
      trace = shadow_map_trace(clip_ray, ray_step_count, random_shadow_3d.z);
    }
    else {
      ShadowRayPunctual clip_ray = shadow_ray_generate_punctual(light, random_ray_2d, lP, lNg);
      trace = shadow_map_trace(clip_ray, ray_step_count, random_shadow_3d.z);
    }

    surface_hit += float(trace.has_hit);
  }
  /* Average samples. */
  ShadowEvalResult result;
  result.light_visibilty = saturate(1.0 - surface_hit / float(ray_count));
  result.occluder_distance = 0.0; /* Unused. Could reintroduced if needed. */
  return result;
}

/** \} */
