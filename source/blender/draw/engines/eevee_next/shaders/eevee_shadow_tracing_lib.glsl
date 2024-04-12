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

  int level = shadow_directional_level(ray.light, ray_pos.xyz - ray.light._position);
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
vec3 shadow_pcf_offset(LightData light, const bool is_directional, vec3 P, vec3 Ng, vec2 random)
{
  if (light.pcf_radius <= 0.001) {
    /* Early return. */
    return vec3(0.0);
  }

  vec3 L = light_vector_get(light, is_directional, P).L;
  if (dot(L, Ng) < 0.001) {
    /* Don't apply PCF to almost perpendicular,
     * since we can't project the offset to the surface. */
    return vec3(0.0);
  }

  ShadowSampleParams params;
  if (is_directional) {
    params = shadow_directional_sample_params_get(shadow_tilemaps_tx, light, P);
  }
  else {
    params = shadow_punctual_sample_params_get(light, P);
  }
  ShadowSamplingTile tile = shadow_tile_data_get(shadow_tilemaps_tx, params);
  if (!tile.is_valid) {
    return vec3(0.0);
  }

  /* Compute the shadow-map tangent-bitangent matrix. */

  float uv_offset = 1.0 / float(SHADOW_MAP_MAX_RES);
  vec3 TP, BP;
  if (is_directional) {
    TP = shadow_directional_reconstruct_position(
        params, light, params.uv + vec3(uv_offset, 0.0, 0.0));
    BP = shadow_directional_reconstruct_position(
        params, light, params.uv + vec3(0.0, uv_offset, 0.0));
  }
  else {
    mat4 wininv = shadow_punctual_projection_perspective_inverse(light);
    TP = shadow_punctual_reconstruct_position(
        params, wininv, light, params.uv + vec3(uv_offset, 0.0, 0.0));
    BP = shadow_punctual_reconstruct_position(
        params, wininv, light, params.uv + vec3(0.0, uv_offset, 0.0));
  }

  /* TODO: Use a mat2x3 (Currently not supported by the Metal backend). */
  mat3 TBN = mat3(TP - P, BP - P, Ng);

  /* Compute the actual offset. */

  vec2 pcf_offset = random * 2.0 - 1.0;
  pcf_offset *= light.pcf_radius;

  /* Scale the offset based on shadow LOD. */
  if (is_directional) {
    vec3 lP = light_world_to_local(light, P);
    float level = shadow_directional_level_fractional(light, lP - light._position);
    float pcf_scale = mix(0.5, 1.0, fract(level));
    pcf_offset *= pcf_scale;
  }
  else {
    bool is_perspective = drw_view_is_perspective();
    float dist_to_cam = distance(P, drw_view_position());
    float footprint_ratio = shadow_punctual_footprint_ratio(
        light, P, is_perspective, dist_to_cam, uniform_buf.shadow.tilemap_projection_ratio);
    float lod = -log2(footprint_ratio) + light.lod_bias;
    lod = clamp(lod, 0.0, float(SHADOW_TILEMAP_LOD));
    float pcf_scale = exp2(lod);
    pcf_offset *= pcf_scale;
  }

  vec3 ws_offset = TBN * vec3(pcf_offset, 0.0);
  vec3 offset_P = P + ws_offset;

  /* Project the offset position into the surface */

#ifdef GPU_NVIDIA
  /* Workaround for a bug in the Nvidia shader compiler.
   * If we don't compute L here again, it breaks shadows on reflection probes. */
  L = light_vector_get(light, is_directional, P).L;
#endif

  if (abs(dot(Ng, L)) > 0.999) {
    return ws_offset;
  }

  offset_P = line_plane_intersect(offset_P, L, P, Ng);
  ws_offset = offset_P - P;

  if (dot(ws_offset, L) < 0.0) {
    /* Project the offset position into the perpendicular plane, since it's closer to the light
     * (avoids overshadowing at geometry angles). */
    vec3 perpendicular_plane_normal = cross(Ng, normalize(cross(Ng, L)));
    offset_P = line_plane_intersect(offset_P, L, P, perpendicular_plane_normal);
    ws_offset = offset_P - P;
  }

  return ws_offset;
}

/**
 * Evaluate shadowing by casting rays toward the light direction.
 */
ShadowEvalResult shadow_eval(LightData light,
                             const bool is_directional,
                             const bool is_transmission,
                             vec3 P,
                             vec3 Ng,
                             int ray_count,
                             int ray_step_count)
{
#ifdef EEVEE_SAMPLING_DATA
#  ifdef GPU_FRAGMENT_SHADER
  vec2 pixel = floor(gl_FragCoord.xy);
#  elif defined(GPU_COMPUTE_SHADER)
  vec2 pixel = vec2(gl_GlobalInvocationID.xy);
#  endif
  vec3 blue_noise_3d = utility_tx_fetch(utility_tx, pixel, UTIL_BLUE_NOISE_LAYER).rgb;
  vec3 random_shadow_3d = blue_noise_3d + sampling_rng_3D_get(SAMPLING_SHADOW_U);
  vec2 random_pcf_2d = fract(blue_noise_3d.xy + sampling_rng_2D_get(SAMPLING_SHADOW_X));
  float normal_offset = uniform_buf.shadow.normal_bias;
#else
  /* Case of surfel light eval. */
  vec3 random_shadow_3d = vec3(0.5);
  vec2 random_pcf_2d = vec2(0.0);
  /* TODO(fclem): Parameter on irradiance volumes? */
  float normal_offset = 0.02;
#endif

  /* We want to bias inside the object for transmission to go through the object itself. */
  normal_offset = is_transmission ? -normal_offset : normal_offset;

  P += shadow_pcf_offset(light, is_directional, P, Ng, random_pcf_2d);

  /* Avoid self intersection. */
  P = offset_ray(P, Ng);
  /* The above offset isn't enough in most situation. Still add a bigger bias. */
  /* TODO(fclem): Scale based on depth. */
  P += Ng * normal_offset;

  vec3 lP = is_directional ? light_world_to_local(light, P) :
                             light_world_to_local(light, P - light._position);
  vec3 lNg = light_world_to_local(light, Ng);

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
  result.light_visibilty = saturate(1.0 - surface_hit * float(ray_count));
  result.occluder_distance = 0.0; /* Unused. Could reintroduced if needed. */
  return result;
}

/** \} */
