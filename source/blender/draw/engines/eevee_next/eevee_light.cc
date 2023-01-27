/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation.
 */

/** \file
 * \ingroup eevee
 *
 * The light module manages light data buffers and light culling system.
 */

#include "draw_debug.hh"

#include "eevee_instance.hh"

#include "eevee_light.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name LightData
 * \{ */

static eLightType to_light_type(short blender_light_type, short blender_area_type)
{
  switch (blender_light_type) {
    default:
    case LA_LOCAL:
      return LIGHT_POINT;
    case LA_SUN:
      return LIGHT_SUN;
    case LA_SPOT:
      return LIGHT_SPOT;
    case LA_AREA:
      return ELEM(blender_area_type, LA_AREA_DISK, LA_AREA_ELLIPSE) ? LIGHT_ELLIPSE : LIGHT_RECT;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Light Object
 * \{ */

void Light::sync(/* ShadowModule &shadows , */ const Object *ob, float threshold)
{
  const ::Light *la = (const ::Light *)ob->data;
  float scale[3];

  float max_power = max_fff(la->r, la->g, la->b) * fabsf(la->energy / 100.0f);
  float surface_max_power = max_ff(la->diff_fac, la->spec_fac) * max_power;
  float volume_max_power = la->volume_fac * max_power;

  float influence_radius_surface = attenuation_radius_get(la, threshold, surface_max_power);
  float influence_radius_volume = attenuation_radius_get(la, threshold, volume_max_power);

  this->influence_radius_max = max_ff(influence_radius_surface, influence_radius_volume);
  this->influence_radius_invsqr_surface = 1.0f / square_f(max_ff(influence_radius_surface, 1e-8f));
  this->influence_radius_invsqr_volume = 1.0f / square_f(max_ff(influence_radius_volume, 1e-8f));

  this->color = float3(&la->r) * la->energy;
  normalize_m4_m4_ex(this->object_mat.ptr(), ob->object_to_world, scale);
  /* Make sure we have consistent handedness (in case of negatively scaled Z axis). */
  float3 cross = math::cross(float3(this->_right), float3(this->_up));
  if (math::dot(cross, float3(this->_back)) < 0.0f) {
    negate_v3(this->_up);
  }

  shape_parameters_set(la, scale);

  float shape_power = shape_power_get(la);
  float point_power = point_power_get(la);
  this->diffuse_power = la->diff_fac * shape_power;
  this->transmit_power = la->diff_fac * point_power;
  this->specular_power = la->spec_fac * shape_power;
  this->volume_power = la->volume_fac * point_power;

  eLightType new_type = to_light_type(la->type, la->area_shape);
  if (this->type != new_type) {
    /* shadow_discard_safe(shadows); */
    this->type = new_type;
  }

#if 0
  if (la->mode & LA_SHADOW) {
    if (la->type == LA_SUN) {
      if (this->shadow_id == LIGHT_NO_SHADOW) {
        this->shadow_id = shadows.directionals.alloc();
      }

      ShadowDirectional &shadow = shadows.directionals[this->shadow_id];
      shadow.sync(this->object_mat, la->bias * 0.05f, 1.0f);
    }
    else {
      float cone_aperture = DEG2RAD(360.0);
      if (la->type == LA_SPOT) {
        cone_aperture = min_ff(DEG2RAD(179.9), la->spotsize);
      }
      else if (la->type == LA_AREA) {
        cone_aperture = DEG2RAD(179.9);
      }

      if (this->shadow_id == LIGHT_NO_SHADOW) {
        this->shadow_id = shadows.punctuals.alloc();
      }

      ShadowPunctual &shadow = shadows.punctuals[this->shadow_id];
      shadow.sync(this->type,
                  this->object_mat,
                  cone_aperture,
                  la->clipsta,
                  this->influence_radius_max,
                  la->bias * 0.05f);
    }
  }
  else {
    shadow_discard_safe(shadows);
  }
#endif

  this->initialized = true;
}

#if 0
void Light::shadow_discard_safe(ShadowModule &shadows)
{
  if (shadow_id != LIGHT_NO_SHADOW) {
    if (this->type != LIGHT_SUN) {
      shadows.punctuals.free(shadow_id);
    }
    else {
      shadows.directionals.free(shadow_id);
    }
    shadow_id = LIGHT_NO_SHADOW;
  }
}
#endif

/* Returns attenuation radius inverted & squared for easy bound checking inside the shader. */
float Light::attenuation_radius_get(const ::Light *la, float light_threshold, float light_power)
{
  if (la->type == LA_SUN) {
    return (light_power > 1e-5f) ? 1e16f : 0.0f;
  }

  if (la->mode & LA_CUSTOM_ATTENUATION) {
    return la->att_dist;
  }
  /* Compute the distance (using the inverse square law)
   * at which the light power reaches the light_threshold. */
  /* TODO take area light scale into account. */
  return sqrtf(light_power / light_threshold);
}

void Light::shape_parameters_set(const ::Light *la, const float scale[3])
{
  if (la->type == LA_AREA) {
    float area_size_y = ELEM(la->area_shape, LA_AREA_RECT, LA_AREA_ELLIPSE) ? la->area_sizey :
                                                                              la->area_size;
    _area_size_x = max_ff(0.003f, la->area_size * scale[0] * 0.5f);
    _area_size_y = max_ff(0.003f, area_size_y * scale[1] * 0.5f);
    /* For volume point lighting. */
    radius_squared = max_ff(0.001f, hypotf(_area_size_x, _area_size_y) * 0.5f);
    radius_squared = square_f(radius_squared);
  }
  else {
    if (la->type == LA_SPOT) {
      /* Spot size & blend */
      spot_size_inv[0] = scale[2] / scale[0];
      spot_size_inv[1] = scale[2] / scale[1];
      float spot_size = cosf(la->spotsize * 0.5f);
      float spot_blend = (1.0f - spot_size) * la->spotblend;
      _spot_mul = 1.0f / max_ff(1e-8f, spot_blend);
      _spot_bias = -spot_size * _spot_mul;
      spot_tan = tanf(min_ff(la->spotsize * 0.5f, M_PI_2 - 0.0001f));
    }

    if (la->type == LA_SUN) {
      _area_size_x = tanf(min_ff(la->sun_angle, DEG2RADF(179.9f)) / 2.0f);
    }
    else {
      _area_size_x = la->radius;
    }
    _area_size_y = _area_size_x = max_ff(0.001f, _area_size_x);
    radius_squared = square_f(_area_size_x);
  }
}

float Light::shape_power_get(const ::Light *la)
{
  /* Make illumination power constant */
  switch (la->type) {
    case LA_AREA: {
      float area = _area_size_x * _area_size_y;
      float power = 1.0f / (area * 4.0f * float(M_PI));
      /* FIXME : Empirical, Fit cycles power */
      power *= 0.8f;
      if (ELEM(la->area_shape, LA_AREA_DISK, LA_AREA_ELLIPSE)) {
        /* Scale power to account for the lower area of the ellipse compared to the surrounding
         * rectangle. */
        power *= 4.0f / M_PI;
      }
      return power;
    }
    case LA_SPOT:
    case LA_LOCAL: {
      return 1.0f / (4.0f * square_f(_radius) * float(M_PI * M_PI));
    }
    default:
    case LA_SUN: {
      float power = 1.0f / (square_f(_radius) * float(M_PI));
      /* Make illumination power closer to cycles for bigger radii. Cycles uses a cos^3 term that
       * we cannot reproduce so we account for that by scaling the light power. This function is
       * the result of a rough manual fitting. */
      /* Simplification of: power *= 1 + r²/2 */
      power += 1.0f / (2.0f * M_PI);

      return power;
    }
  }
}

float Light::point_power_get(const ::Light *la)
{
  /* Volume light is evaluated as point lights. Remove the shape power. */
  switch (la->type) {
    case LA_AREA: {
      /* Match cycles. Empirical fit... must correspond to some constant. */
      float power = 0.0792f * M_PI;

      /* This corrects for area light most representative point trick. The fit was found by
       * reducing the average error compared to cycles. */
      float area = _area_size_x * _area_size_y;
      float tmp = M_PI_2 / (M_PI_2 + sqrtf(area));
      /* Lerp between 1.0 and the limit (1 / pi). */
      power *= tmp + (1.0f - tmp) * M_1_PI;

      return power;
    }
    case LA_SPOT:
    case LA_LOCAL: {
      /* Match cycles. Empirical fit... must correspond to some constant. */
      return 0.0792f;
    }
    default:
    case LA_SUN: {
      return 1.0f;
    }
  }
}

void Light::debug_draw()
{
#ifdef DEBUG
  drw_debug_sphere(_position, influence_radius_max, float4(0.8f, 0.3f, 0.0f, 1.0f));
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name LightModule
 * \{ */

void LightModule::begin_sync()
{
  use_scene_lights_ = inst_.use_scene_lights();

  /* In begin_sync so it can be animated. */
  if (assign_if_different(light_threshold_, max_ff(1e-16f, inst_.scene->eevee.light_threshold))) {
    inst_.sampling.reset();
  }

  sun_lights_len_ = 0;
  local_lights_len_ = 0;
}

void LightModule::sync_light(const Object *ob, ObjectHandle &handle)
{
  if (use_scene_lights_ == false) {
    return;
  }
  Light &light = light_map_.lookup_or_add_default(handle.object_key);
  light.used = true;
  if (handle.recalc != 0 || !light.initialized) {
    light.sync(/* inst_.shadows, */ ob, light_threshold_);
  }
  sun_lights_len_ += int(light.type == LIGHT_SUN);
  local_lights_len_ += int(light.type != LIGHT_SUN);
}

void LightModule::end_sync()
{
  // ShadowModule &shadows = inst_.shadows;

  /* NOTE: We resize this buffer before removing deleted lights. */
  int lights_allocated = ceil_to_multiple_u(max_ii(light_map_.size(), 1), LIGHT_CHUNK);
  light_buf_.resize(lights_allocated);

  /* Track light deletion. */
  Vector<ObjectKey, 0> deleted_keys;
  /* Indices inside GPU data array. */
  int sun_lights_idx = 0;
  int local_lights_idx = sun_lights_len_;

  /* Fill GPU data with scene data. */
  for (auto item : light_map_.items()) {
    Light &light = item.value;

    if (!light.used) {
      /* Deleted light. */
      deleted_keys.append(item.key);
      // light.shadow_discard_safe(shadows);
      continue;
    }

    int dst_idx = (light.type == LIGHT_SUN) ? sun_lights_idx++ : local_lights_idx++;
    /* Put all light data into global data SSBO. */
    light_buf_[dst_idx] = light;

#if 0
    if (light.shadow_id != LIGHT_NO_SHADOW) {
      if (light.type == LIGHT_SUN) {
        light_buf_[dst_idx].shadow_data = shadows.directionals[light.shadow_id];
      }
      else {
        light_buf_[dst_idx].shadow_data = shadows.punctuals[light.shadow_id];
      }
    }
#endif
    /* Untag for next sync. */
    light.used = false;
  }
  /* This scene data buffer is then immutable after this point. */
  light_buf_.push_update();

  for (auto &key : deleted_keys) {
    light_map_.remove(key);
  }

  /* Update sampling on deletion or un-hiding (use_scene_lights). */
  if (assign_if_different(light_map_size_, light_map_.size())) {
    inst_.sampling.reset();
  }

  /* If exceeding the limit, just trim off the excess to avoid glitchy rendering. */
  if (sun_lights_len_ + local_lights_len_ > CULLING_MAX_ITEM) {
    sun_lights_len_ = min_ii(sun_lights_len_, CULLING_MAX_ITEM);
    local_lights_len_ = min_ii(local_lights_len_, CULLING_MAX_ITEM - sun_lights_len_);
    inst_.info = "Error: Too many lights in the scene.";
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
    do {
      tile_size *= 2;
      tiles_extent = math::divide_ceil(render_extent, int2(tile_size));
      uint tile_count = tiles_extent.x * tiles_extent.y;
      if (tile_count > max_tile_count_threshold) {
        continue;
      }
      total_word_count_ = tile_count * word_per_tile;

    } while (total_word_count_ > max_word_count_threshold);
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
    inst_.hiz_buffer.bind_resources(&debug_draw_ps_);
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
    inst_.info = "Debug Mode: Light Culling Validation";
    inst_.hiz_buffer.update();
    GPU_framebuffer_bind(view_fb);
    inst_.manager->submit(debug_draw_ps_, view);
  }
}

/** \} */

}  // namespace blender::eevee
