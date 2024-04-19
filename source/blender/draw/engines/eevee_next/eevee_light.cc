/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * The light module manages light data buffers and light culling system.
 */

#include "draw_debug.hh"

#include "eevee_instance.hh"

#include "eevee_light.hh"

#include "BLI_math_rotation.h"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name LightData
 * \{ */

static eLightType to_light_type(short blender_light_type,
                                short blender_area_type,
                                bool use_soft_falloff)
{
  switch (blender_light_type) {
    default:
    case LA_LOCAL:
      return use_soft_falloff ? LIGHT_OMNI_DISK : LIGHT_OMNI_SPHERE;
    case LA_SUN:
      return LIGHT_SUN;
    case LA_SPOT:
      return use_soft_falloff ? LIGHT_SPOT_DISK : LIGHT_SPOT_SPHERE;
    case LA_AREA:
      return ELEM(blender_area_type, LA_AREA_DISK, LA_AREA_ELLIPSE) ? LIGHT_ELLIPSE : LIGHT_RECT;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Light Object
 * \{ */

void Light::sync(ShadowModule &shadows, const Object *ob, float threshold)
{
  using namespace blender::math;

  const ::Light *la = (const ::Light *)ob->data;

  eLightType new_type = to_light_type(la->type, la->area_shape, la->mode & LA_USE_SOFT_FALLOFF);
  if (assign_if_different(this->type, new_type)) {
    shadow_discard_safe(shadows);
  }

  this->color = float3(&la->r) * la->energy;

  float3 scale;
  this->object_mat = ob->object_to_world();
  this->object_mat.view<3, 3>() = normalize_and_get_size(this->object_mat.view<3, 3>(), scale);

  /* Make sure we have consistent handedness (in case of negatively scaled Z axis). */
  float3 back = cross(float3(this->_right), float3(this->_up));
  if (dot(back, float3(this->_back)) < 0.0f) {
    negate_v3(this->_up);
  }

  shape_parameters_set(la, scale, threshold);

  const bool diffuse_visibility = (ob->visibility_flag & OB_HIDE_DIFFUSE) == 0;
  const bool glossy_visibility = (ob->visibility_flag & OB_HIDE_GLOSSY) == 0;
  const bool transmission_visibility = (ob->visibility_flag & OB_HIDE_TRANSMISSION) == 0;
  const bool volume_visibility = (ob->visibility_flag & OB_HIDE_VOLUME_SCATTER) == 0;

  float shape_power = shape_radiance_get();
  float point_power = point_radiance_get();
  this->power[LIGHT_DIFFUSE] = la->diff_fac * shape_power * diffuse_visibility;
  this->power[LIGHT_SPECULAR] = la->spec_fac * shape_power * glossy_visibility;
  this->power[LIGHT_TRANSMISSION] = la->transmission_fac * shape_power * transmission_visibility;
  this->power[LIGHT_VOLUME] = la->volume_fac * point_power * volume_visibility;

  this->pcf_radius = la->shadow_filter_radius;

  /* TODO(fclem): Cleanup: Move that block to `ShadowPunctual::end_sync()` and
   * `ShadowDirectional::end_sync()`. */
  float resolution_scale = clamp(la->shadow_resolution_scale, 0.0f, 2.0f);
  this->lod_bias = (resolution_scale < 1.0) ? -log2(resolution_scale) : -(resolution_scale - 1.0f);
  this->lod_bias += shadows.get_global_lod_bias();

  if (la->mode & LA_SHADOW) {
    shadow_ensure(shadows);
    if (is_sun_light(this->type)) {
      this->directional->sync(this->object_mat,
                              1.0f,
                              la->sun_angle * la->shadow_softness_factor,
                              la->shadow_trace_distance);
    }
    else {
      /* Reuse shape radius as near clip plane. */
      /* This assumes `shape_parameters_set` has already set `radius_squared`. */
      float radius = math::sqrt(this->local.radius_squared);
      float shadow_radius = la->shadow_softness_factor * radius;
      if (ELEM(la->type, LA_LOCAL, LA_SPOT)) {
        /* `shape_parameters_set` can increase the radius of point and spot lights to ensure a
         * minimum radius/energy ratio.
         * But we don't want to take that into account for computing the shadow-map projection,
         * since non-zero radius introduces padding (required for soft-shadows tracing), reducing
         * the effective resolution of shadow-maps.
         * So we use the original light radius instead. */
        shadow_radius = la->shadow_softness_factor * la->radius;
      }
      this->punctual->sync(this->type,
                           this->object_mat,
                           la->spotsize,
                           radius,
                           this->local.influence_radius_max,
                           la->shadow_softness_factor,
                           shadow_radius);
    }
  }
  else {
    shadow_discard_safe(shadows);
  }

  this->initialized = true;
}

void Light::shadow_discard_safe(ShadowModule &shadows)
{
  if (this->directional != nullptr) {
    shadows.directional_pool.destruct(*directional);
    this->directional = nullptr;
  }
  if (this->punctual != nullptr) {
    shadows.punctual_pool.destruct(*punctual);
    this->punctual = nullptr;
  }
}

void Light::shadow_ensure(ShadowModule &shadows)
{
  if (is_sun_light(this->type) && this->directional == nullptr) {
    this->directional = &shadows.directional_pool.construct(shadows);
  }
  else if (this->punctual == nullptr) {
    this->punctual = &shadows.punctual_pool.construct(shadows);
  }
}

float Light::attenuation_radius_get(const ::Light *la, float light_threshold, float light_power)
{
  if (la->mode & LA_CUSTOM_ATTENUATION) {
    return la->att_dist;
  }
  /* Compute the distance (using the inverse square law)
   * at which the light power reaches the light_threshold. */
  /* TODO take area light scale into account. */
  return sqrtf(light_power / light_threshold);
}

void Light::shape_parameters_set(const ::Light *la, const float3 &scale, float threshold)
{
  using namespace blender::math;

  if (is_sun_light(this->type)) {
    this->sun.radius = tanf(min_ff(la->sun_angle, DEG2RADF(179.9f)) / 2.0f);
    /* Clamp to minimum value before float imprecision artifacts appear. */
    this->sun.radius = max(0.001f, this->sun.radius);
  }
  else if (is_area_light(this->type)) {
    const bool is_irregular = ELEM(la->area_shape, LA_AREA_RECT, LA_AREA_ELLIPSE);
    this->area.size = float2(la->area_size, is_irregular ? la->area_sizey : la->area_size);
    /* Scale and clamp to minimum value before float imprecision artifacts appear. */
    this->area.size = max(float2(0.003f), this->area.size * scale.xy() / 2.0f);

    /* For volume point lighting. */
    this->local.radius_squared = square(max(0.001f, length(this->area.size) / 2.0f));
  }
  else if (is_point_light(this->type)) {
    /* Spot size & blend */
    if (is_spot_light(this->type)) {
      const float spot_size = cosf(la->spotsize * 0.5f);
      const float spot_blend = (1.0f - spot_size) * la->spotblend;
      this->spot.spot_size_inv = scale.z / max(scale.xy(), float2(1e-8f));
      this->spot.spot_mul = 1.0f / max(1e-8f, spot_blend);
      this->spot.spot_bias = -spot_size * this->spot.spot_mul;
      this->spot.spot_tan = tanf(min(la->spotsize * 0.5f, float(M_PI_2 - 0.0001f)));
    }
    else {
      /* Point light could access it. Make sure to avoid Undefined Behavior.
       * In practice it is only ever used. */
      this->spot.spot_size_inv = float2(1.0f);
      this->spot.spot_mul = 0.0f;
      this->spot.spot_bias = 1.0f;
      this->spot.spot_tan = 0.0f;
    }

    this->spot.radius = la->radius;
    /* Ensure a minimum radius/energy ratio to avoid harsh cut-offs. (See 114284) */
    this->spot.radius = max(this->spot.radius, la->energy * 2e-05f);
    /* Clamp to minimum value before float imprecision artifacts appear. */
    this->spot.radius = max(0.001f, this->spot.radius);

    /* For volume point lighting. */
    this->local.radius_squared = square(this->spot.radius);
  }

  if (is_local_light(this->type)) {
    const float max_power = reduce_max(float3(&la->r)) * fabsf(la->energy / 100.0f);
    const float surface_max_power = max(la->diff_fac, la->spec_fac) * max_power;
    const float volume_max_power = la->volume_fac * max_power;

    float influence_radius_surface = attenuation_radius_get(la, threshold, surface_max_power);
    float influence_radius_volume = attenuation_radius_get(la, threshold, volume_max_power);

    this->local.influence_radius_max = max(influence_radius_surface, influence_radius_volume);
    this->local.influence_radius_invsqr_surface = safe_rcp(square(influence_radius_surface));
    this->local.influence_radius_invsqr_volume = safe_rcp(square(influence_radius_volume));
  }
}

float Light::shape_radiance_get()
{
  using namespace blender::math;

  /* Make illumination power constant. */
  switch (this->type) {
    case LIGHT_RECT:
    case LIGHT_ELLIPSE: {
      /* Rectangle area. */
      float area = this->area.size.x * this->area.size.y * 4.0f;
      /* Scale for the lower area of the ellipse compared to the surrounding rectangle. */
      if (this->type == LIGHT_ELLIPSE) {
        area *= M_PI / 4.0f;
      }
      /* Convert radiant flux to radiance. */
      return float(M_1_PI) / area;
    }
    case LIGHT_OMNI_SPHERE:
    case LIGHT_OMNI_DISK:
    case LIGHT_SPOT_SPHERE:
    case LIGHT_SPOT_DISK: {
      /* Sphere area. */
      float area = float(4.0f * M_PI) * this->spot.radius_squared;
      /* Convert radiant flux to radiance. */
      return 1.0f / (area * float(M_PI));
    }
    case LIGHT_SUN_ORTHO:
    case LIGHT_SUN: {
      float inv_sin_sq = 1.0f + 1.0f / square(this->sun.radius);
      /* Convert irradiance to radiance. */
      return float(M_1_PI) * inv_sin_sq;
    }
  }
  BLI_assert_unreachable();
  return 0.0f;
}

float Light::point_radiance_get()
{
  /* Volume light is evaluated as point lights. */
  switch (this->type) {
    case LIGHT_RECT:
    case LIGHT_ELLIPSE: {
      /* This corrects for area light most representative point trick.
       * The fit was found by reducing the average error compared to cycles. */
      float area = this->area.size.x * this->area.size.y * 4.0f;
      float tmp = M_PI_2 / (M_PI_2 + sqrtf(area));
      /* Lerp between 1.0 and the limit (1 / pi). */
      float mrp_scaling = tmp + (1.0f - tmp) * M_1_PI;
      return float(M_1_PI) * mrp_scaling;
    }
    case LIGHT_OMNI_SPHERE:
    case LIGHT_OMNI_DISK:
    case LIGHT_SPOT_SPHERE:
    case LIGHT_SPOT_DISK: {
      /* Convert radiant flux to intensity. */
      /* Inverse of sphere solid angle. */
      return float(1.0 / (4.0 * M_PI));
    }
    case LIGHT_SUN_ORTHO:
    case LIGHT_SUN: {
      return 1.0f;
    }
  }
  BLI_assert_unreachable();
  return 0.0f;
}

void Light::debug_draw()
{
#ifndef NDEBUG
  drw_debug_sphere(float3(_position), local.influence_radius_max, float4(0.8f, 0.3f, 0.0f, 1.0f));
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name LightModule
 * \{ */

LightModule::~LightModule()
{
  /* WATCH: Destructor order. Expect shadow module to be destructed later. */
  for (Light &light : light_map_.values()) {
    light.shadow_discard_safe(inst_.shadows);
  }
};

void LightModule::begin_sync()
{
  use_scene_lights_ = inst_.use_scene_lights();
  /* Disable sunlight if world has a volume shader as we consider the light cannot go through an
   * infinite opaque medium. */
  use_sun_lights_ = (inst_.world.has_volume_absorption() == false);

  /* In begin_sync so it can be animated. */
  if (assign_if_different(light_threshold_, max_ff(1e-16f, inst_.scene->eevee.light_threshold))) {
    /* All local lights need to be re-sync. */
    for (Light &light : light_map_.values()) {
      if (!ELEM(light.type, LIGHT_SUN, LIGHT_SUN_ORTHO)) {
        light.initialized = false;
      }
    }
  }

  sun_lights_len_ = 0;
  local_lights_len_ = 0;
}

void LightModule::sync_light(const Object *ob, ObjectHandle &handle)
{
  if (use_scene_lights_ == false) {
    return;
  }

  if (use_sun_lights_ == false) {
    if (static_cast<const ::Light *>(ob->data)->type == LA_SUN) {
      return;
    }
  }

  Light &light = light_map_.lookup_or_add_default(handle.object_key);
  light.used = true;
  if (handle.recalc != 0 || !light.initialized) {
    light.initialized = true;
    light.sync(inst_.shadows, ob, light_threshold_);
  }
  sun_lights_len_ += int(is_sun_light(light.type));
  local_lights_len_ += int(!is_sun_light(light.type));
}

void LightModule::end_sync()
{
  /* NOTE: We resize this buffer before removing deleted lights. */
  int lights_allocated = ceil_to_multiple_u(max_ii(light_map_.size(), 1), LIGHT_CHUNK);
  light_buf_.resize(lights_allocated);

  /* Track light deletion. */
  /* Indices inside GPU data array. */
  int sun_lights_idx = 0;
  int local_lights_idx = sun_lights_len_;

  /* Fill GPU data with scene data. */
  auto it_end = light_map_.items().end();
  for (auto it = light_map_.items().begin(); it != it_end; ++it) {
    Light &light = (*it).value;

    if (!light.used) {
      light_map_.remove(it);
      continue;
    }

    int dst_idx = is_sun_light(light.type) ? sun_lights_idx++ : local_lights_idx++;
    /* Put all light data into global data SSBO. */
    light_buf_[dst_idx] = light;

    /* Untag for next sync. */
    light.used = false;
  }
  /* This scene data buffer is then immutable after this point. */
  light_buf_.push_update();

  /* If exceeding the limit, just trim off the excess to avoid glitchy rendering. */
  if (sun_lights_len_ + local_lights_len_ > CULLING_MAX_ITEM) {
    sun_lights_len_ = min_ii(sun_lights_len_, CULLING_MAX_ITEM);
    local_lights_len_ = min_ii(local_lights_len_, CULLING_MAX_ITEM - sun_lights_len_);
    inst_.info += "Error: Too many lights in the scene.\n";
  }
  lights_len_ = sun_lights_len_ + local_lights_len_;

  /* Resize to the actual number of lights after pruning. */
  lights_allocated = ceil_to_multiple_u(max_ii(lights_len_, 1), LIGHT_CHUNK);
  culling_key_buf_.resize(lights_allocated);
  culling_zdist_buf_.resize(lights_allocated);
  culling_light_buf_.resize(lights_allocated);

  {
    /* Compute tile size and total word count. */
    uint word_per_tile = divide_ceil_u(max_ii(lights_len_, 1), 32);
    int2 render_extent = inst_.film.render_extent_get();
    int2 tiles_extent;
    /* Default to 32 as this is likely to be the maximum
     * tile size used by hardware or compute shading. */
    uint tile_size = 16;
    bool tile_size_valid = false;
    do {
      tile_size *= 2;
      tiles_extent = math::divide_ceil(render_extent, int2(tile_size));
      uint tile_count = tiles_extent.x * tiles_extent.y;
      if (tile_count > max_tile_count_threshold) {
        continue;
      }
      total_word_count_ = tile_count * word_per_tile;
      tile_size_valid = true;

    } while (total_word_count_ > max_word_count_threshold || !tile_size_valid);
    /* Keep aligned with storage buffer requirements. */
    total_word_count_ = ceil_to_multiple_u(total_word_count_, 32);

    culling_data_buf_.tile_word_len = word_per_tile;
    culling_data_buf_.tile_size = tile_size;
    culling_data_buf_.tile_x_len = tiles_extent.x;
    culling_data_buf_.tile_y_len = tiles_extent.y;
    culling_data_buf_.items_count = lights_len_;
    culling_data_buf_.local_lights_len = local_lights_len_;
    culling_data_buf_.sun_lights_len = sun_lights_len_;
  }
  culling_tile_buf_.resize(total_word_count_);

  culling_pass_sync();
  debug_pass_sync();
}

void LightModule::culling_pass_sync()
{
  uint safe_lights_len = max_ii(lights_len_, 1);
  uint culling_select_dispatch_size = divide_ceil_u(safe_lights_len, CULLING_SELECT_GROUP_SIZE);
  uint culling_sort_dispatch_size = divide_ceil_u(safe_lights_len, CULLING_SORT_GROUP_SIZE);
  uint culling_tile_dispatch_size = divide_ceil_u(total_word_count_, CULLING_TILE_GROUP_SIZE);

  /* NOTE: We reference the buffers that may be resized or updated later. */

  culling_ps_.init();
  {
    auto &sub = culling_ps_.sub("Select");
    sub.shader_set(inst_.shaders.static_shader_get(LIGHT_CULLING_SELECT));
    sub.bind_ssbo("light_cull_buf", &culling_data_buf_);
    sub.bind_ssbo("in_light_buf", light_buf_);
    sub.bind_ssbo("out_light_buf", culling_light_buf_);
    sub.bind_ssbo("out_zdist_buf", culling_zdist_buf_);
    sub.bind_ssbo("out_key_buf", culling_key_buf_);
    sub.dispatch(int3(culling_select_dispatch_size, 1, 1));
    sub.barrier(GPU_BARRIER_SHADER_STORAGE);
  }
  {
    auto &sub = culling_ps_.sub("Sort");
    sub.shader_set(inst_.shaders.static_shader_get(LIGHT_CULLING_SORT));
    sub.bind_ssbo("light_cull_buf", &culling_data_buf_);
    sub.bind_ssbo("in_light_buf", light_buf_);
    sub.bind_ssbo("out_light_buf", culling_light_buf_);
    sub.bind_ssbo("in_zdist_buf", culling_zdist_buf_);
    sub.bind_ssbo("in_key_buf", culling_key_buf_);
    sub.dispatch(int3(culling_sort_dispatch_size, 1, 1));
    sub.barrier(GPU_BARRIER_SHADER_STORAGE);
  }
  {
    auto &sub = culling_ps_.sub("Zbin");
    sub.shader_set(inst_.shaders.static_shader_get(LIGHT_CULLING_ZBIN));
    sub.bind_ssbo("light_cull_buf", &culling_data_buf_);
    sub.bind_ssbo("light_buf", culling_light_buf_);
    sub.bind_ssbo("out_zbin_buf", culling_zbin_buf_);
    sub.dispatch(int3(1, 1, 1));
    sub.barrier(GPU_BARRIER_SHADER_STORAGE);
  }
  {
    auto &sub = culling_ps_.sub("Tiles");
    sub.shader_set(inst_.shaders.static_shader_get(LIGHT_CULLING_TILE));
    sub.bind_ssbo("light_cull_buf", &culling_data_buf_);
    sub.bind_ssbo("light_buf", culling_light_buf_);
    sub.bind_ssbo("out_light_tile_buf", culling_tile_buf_);
    sub.dispatch(int3(culling_tile_dispatch_size, 1, 1));
    sub.barrier(GPU_BARRIER_SHADER_STORAGE);
  }
}

void LightModule::debug_pass_sync()
{
  if (inst_.debug_mode == eDebugMode::DEBUG_LIGHT_CULLING) {
    debug_draw_ps_.init();
    debug_draw_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM);
    debug_draw_ps_.shader_set(inst_.shaders.static_shader_get(LIGHT_CULLING_DEBUG));
    debug_draw_ps_.bind_resources(inst_.uniform_data);
    debug_draw_ps_.bind_resources(inst_.hiz_buffer.front);
    debug_draw_ps_.bind_ssbo("light_buf", &culling_light_buf_);
    debug_draw_ps_.bind_ssbo("light_cull_buf", &culling_data_buf_);
    debug_draw_ps_.bind_ssbo("light_zbin_buf", &culling_zbin_buf_);
    debug_draw_ps_.bind_ssbo("light_tile_buf", &culling_tile_buf_);
    debug_draw_ps_.bind_texture("depth_tx", &inst_.render_buffers.depth_tx);
    debug_draw_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
}

void LightModule::set_view(View &view, const int2 extent)
{
  float far_z = view.far_clip();
  float near_z = view.near_clip();

  culling_data_buf_.zbin_scale = -CULLING_ZBIN_COUNT / fabsf(far_z - near_z);
  culling_data_buf_.zbin_bias = -near_z * culling_data_buf_.zbin_scale;
  culling_data_buf_.tile_to_uv_fac = (culling_data_buf_.tile_size / float2(extent));
  culling_data_buf_.visible_count = 0;
  culling_data_buf_.push_update();

  inst_.manager->submit(culling_ps_, view);
}

void LightModule::debug_draw(View &view, GPUFrameBuffer *view_fb)
{
  if (inst_.debug_mode == eDebugMode::DEBUG_LIGHT_CULLING) {
    inst_.info += "Debug Mode: Light Culling Validation\n";
    inst_.hiz_buffer.update();
    GPU_framebuffer_bind(view_fb);
    inst_.manager->submit(debug_draw_ps_, view);
  }
}

/** \} */

}  // namespace blender::eevee
