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

#include "DNA_defaults.h"
#include "DNA_light_types.h"
#include "DNA_sdna_type_ids.hh"

#include "BKE_light.h"

namespace blender::eevee {

/* Convert by putting the least significant bits in the first component. */
static uint2 uint64_to_uint2(uint64_t data)
{
  return {uint(data), uint(data >> uint64_t(32))};
}

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

void Light::sync(ShadowModule &shadows,
                 float4x4 object_to_world,
                 char visibility_flag,
                 const ::Light *la,
                 const LightLinking *light_linking /* = nullptr */,
                 float threshold)
{
  using namespace blender::math;

  eLightType new_type = to_light_type(la->type, la->area_shape, la->mode & LA_USE_SOFT_FALLOFF);
  if (assign_if_different(this->type, new_type)) {
    shadow_discard_safe(shadows);
  }

  this->color = BKE_light_power(*la) * BKE_light_color(*la);
  if (la->mode & LA_UNNORMALIZED) {
    this->color *= BKE_light_area(*la, object_to_world);
  }

  float3 scale;
  object_to_world.view<3, 3>() = normalize_and_get_size(object_to_world.view<3, 3>(), scale);

  /* Make sure we have consistent handedness (in case of negatively scaled Z axis). */
  float3 back = cross(float3(object_to_world.x_axis()), float3(object_to_world.y_axis()));
  if (dot(back, float3(object_to_world.z_axis())) < 0.0f) {
    negate_v3(object_to_world.y_axis());
  }

  this->object_to_world = object_to_world;

  shape_parameters_set(
      la, scale, object_to_world.z_axis(), threshold, shadows.get_data().use_jitter);

  const bool diffuse_visibility = (visibility_flag & OB_HIDE_DIFFUSE) == 0;
  const bool glossy_visibility = (visibility_flag & OB_HIDE_GLOSSY) == 0;
  const bool transmission_visibility = (visibility_flag & OB_HIDE_TRANSMISSION) == 0;
  const bool volume_visibility = (visibility_flag & OB_HIDE_VOLUME_SCATTER) == 0;

  float shape_power = shape_radiance_get();
  float point_power = point_radiance_get();
  this->power[LIGHT_DIFFUSE] = la->diff_fac * shape_power * diffuse_visibility;
  this->power[LIGHT_SPECULAR] = la->spec_fac * shape_power * glossy_visibility;
  this->power[LIGHT_TRANSMISSION] = la->transmission_fac * shape_power * transmission_visibility;
  this->power[LIGHT_VOLUME] = la->volume_fac * point_power * volume_visibility;

  this->lod_bias = shadows.global_lod_bias();
  this->lod_min = shadow_lod_min_get(la);
  this->filter_radius = la->shadow_filter_radius;
  this->shadow_jitter = (la->mode & LA_SHADOW_JITTER) != 0;

  if (la->mode & LA_SHADOW) {
    shadow_ensure(shadows);
  }
  else {
    shadow_discard_safe(shadows);
  }

  if (light_linking) {
    this->light_set_membership = uint64_to_uint2(light_linking->runtime.light_set_membership);
    this->shadow_set_membership = uint64_to_uint2(light_linking->runtime.shadow_set_membership);
  }
  else {
    /* Set all bits if light linking is not used. */
    this->light_set_membership = uint64_to_uint2(~uint64_t(0));
    this->shadow_set_membership = uint64_to_uint2(~uint64_t(0));
  }

  this->initialized = true;
}

float Light::shadow_lod_min_get(const ::Light *la)
{
  /* Property is in mm. Convert to unit. */
  float max_res_unit = la->shadow_maximum_resolution;
  if (is_sun_light(this->type)) {
    return log2f(max_res_unit * SHADOW_MAP_MAX_RES) - 1.0f;
  }
  /* Store absolute mode as negative. */
  return (la->mode & LA_SHAD_RES_ABSOLUTE) ? -max_res_unit : max_res_unit;
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

void Light::shape_parameters_set(const ::Light *la,
                                 const float3 &scale,
                                 const float3 &z_axis,
                                 const float threshold,
                                 const bool use_jitter)
{
  using namespace blender::math;

  /* Compute influence radius first. Can be amended by shape later. */
  if (is_local_light(this->type)) {
    const float max_power = reduce_max(BKE_light_color(*la)) *
                            fabsf(BKE_light_power(*la) / 100.0f);
    const float surface_max_power = max(la->diff_fac, la->spec_fac) * max_power;
    const float volume_max_power = la->volume_fac * max_power;

    float influence_radius_surface = attenuation_radius_get(la, threshold, surface_max_power);
    float influence_radius_volume = attenuation_radius_get(la, threshold, volume_max_power);

    this->local.influence_radius_max = max(influence_radius_surface, influence_radius_volume);
    this->local.influence_radius_invsqr_surface = safe_rcp(square(influence_radius_surface));
    this->local.influence_radius_invsqr_volume = safe_rcp(square(influence_radius_volume));
    /* TODO(fclem): This is just duplicating a member for local lights. */
    this->clip_far = float_as_int(this->local.influence_radius_max);
    this->clip_near = float_as_int(this->local.influence_radius_max / 4000.0f);
  }

  float trace_scaling_fac = (use_jitter && (la->mode & LA_SHADOW_JITTER)) ?
                                la->shadow_jitter_overblur / 100.0f :
                                1.0f;

  if (is_sun_light(this->type)) {
    float sun_half_angle = min_ff(la->sun_angle, DEG2RADF(179.9f)) / 2.0f;
    /* Use non-clamped radius for soft shadows. Avoid having a minimum blur. */
    this->sun.shadow_angle = sun_half_angle * trace_scaling_fac;
    /* Clamp to a minimum to distinguish between point lights and area light shadow. */
    this->sun.shadow_angle = (sun_half_angle > 0.0f) ? max_ff(1e-8f, sun.shadow_angle) : 0.0f;
    /* Clamp to minimum value before float imprecision artifacts appear. */
    this->sun.shape_radius = clamp(tanf(sun_half_angle), 0.001f, 20.0f);
    /* Stable shading direction. */
    this->sun.direction = z_axis;
  }
  else if (is_area_light(this->type)) {
    const bool is_irregular = ELEM(la->area_shape, LA_AREA_RECT, LA_AREA_ELLIPSE);
    this->area.size = float2(la->area_size, is_irregular ? la->area_sizey : la->area_size);
    /* Scale and clamp to minimum value before float imprecision artifacts appear. */
    this->area.size *= scale.xy() / 2.0f;
    this->area.shadow_scale = trace_scaling_fac;
    this->local.shadow_radius = length(this->area.size) * trace_scaling_fac;
    /* Set to default position. */
    this->local.shadow_position = float3(0.0f);
    /* Do not render lights that have no area. */
    if (this->area.size.x * this->area.size.y < 0.00001f) {
      /* Forces light to be culled. */
      this->local.influence_radius_max = 0.0f;
    }
    /* Clamp to minimum value before float imprecision artifacts appear. */
    this->area.size = max(float2(0.003f), this->area.size);
    /* For volume point lighting. */
    this->local.shape_radius = max(0.001f, length(this->area.size) / 2.0f);
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
    /* Use unclamped radius for soft shadows. Avoid having a minimum blur. */
    this->local.shadow_radius = max(0.0f, la->radius) * trace_scaling_fac;
    /* Clamp to a minimum to distinguish between point lights and area light shadow. */
    this->local.shadow_radius = (la->radius > 0.0f) ? max_ff(1e-8f, local.shadow_radius) : 0.0f;
    /* Set to default position. */
    this->local.shadow_position = float3(0.0f);
    this->local.shape_radius = la->radius;
    /* Clamp to minimum value before float imprecision artifacts appear. */
    this->local.shape_radius = max(0.001f, this->local.shape_radius);
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
      float area = float(4.0f * M_PI) * square(this->local.shape_radius);
      /* Convert radiant flux to radiance. */
      return 1.0f / (area * float(M_PI));
    }
    case LIGHT_SUN_ORTHO:
    case LIGHT_SUN: {
      float inv_sin_sq = 1.0f + 1.0f / square(this->sun.shape_radius);
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
  drw_debug_sphere(transform_location(this->object_to_world),
                   local.influence_radius_max,
                   float4(0.8f, 0.3f, 0.0f, 1.0f));
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
  if (assign_if_different(use_scene_lights_, inst_.use_scene_lights())) {
    if (inst_.is_viewport()) {
      /* Catch lookdev viewport properties updates. */
      inst_.sampling.reset();
    }
  }

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

  if (use_sun_lights_ && inst_.world.sun_threshold() > 0.0f) {
    /* Create a placeholder light to be fed by the GPU after sunlight extraction.
     * Sunlight is disabled if power is zero. */
    ::Light la = blender::dna::shallow_copy(
        *(const ::Light *)DNA_default_table[dna::sdna_struct_id_get<::Light>()]);
    la.type = LA_SUN;
    /* Set on the GPU. */
    la.r = la.g = la.b = -1.0f; /* Tag as world sun light. */
    la.energy = 1.0f;
    la.sun_angle = inst_.world.sun_angle();
    la.shadow_filter_radius = inst_.world.sun_shadow_filter_radius();
    la.shadow_jitter_overblur = inst_.world.sun_shadow_jitter_overblur();
    la.shadow_maximum_resolution = inst_.world.sun_shadow_max_resolution();
    SET_FLAG_FROM_TEST(la.mode, inst_.world.use_sun_shadow(), LA_SHADOW);
    SET_FLAG_FROM_TEST(la.mode, inst_.world.use_sun_shadow_jitter(), LA_SHADOW_JITTER);

    Light &light = light_map_.lookup_or_add_default(world_sunlight_key);
    light.used = true;
    light.sync(inst_.shadows, float4x4::identity(), 0, &la, nullptr, light_threshold_);

    sun_lights_len_ += 1;
  }
}

void LightModule::sync_light(const Object *ob, ObjectHandle &handle)
{
  const ::Light &la = DRW_object_get_data_for_drawing<const ::Light>(*ob);
  if (use_scene_lights_ == false) {
    return;
  }

  if (use_sun_lights_ == false) {
    if (la.type == LA_SUN) {
      return;
    }
  }

  Light &light = light_map_.lookup_or_add_default(handle.object_key);
  light.used = true;
  if (handle.recalc != 0 || !light.initialized) {
    light.initialized = true;
    light.sync(inst_.shadows,
               ob->object_to_world(),
               ob->visibility_flag,
               &la,
               ob->light_linking,
               light_threshold_);
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
    /* Do not discard casters in baking mode. See WORKAROUND in `surfels_create`. */
    if (!light.used && !inst_.is_baking()) {
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
    inst_.info_append_i18n("Error: Too many lights in the scene.");
  }
  lights_len_ = sun_lights_len_ + local_lights_len_;

  /* Resize to the actual number of lights after pruning. */
  lights_allocated = ceil_to_multiple_u(max_ii(lights_len_, 1), LIGHT_CHUNK);
  culling_key_buf_.resize(lights_allocated);
  culling_zdist_buf_.resize(lights_allocated);
  culling_light_buf_.resize(lights_allocated);

  {

    int2 render_extent = inst_.film.render_extent_get();
    int2 probe_extent = int2(inst_.sphere_probes.probe_render_extent());
    int2 max_extent = math::max(render_extent, probe_extent);
    /* Compute tile size and total word count. */
    uint word_per_tile = divide_ceil_u(max_ii(lights_len_, 1), 32);
    int2 tiles_extent;
    /* Default to 32 as this is likely to be the maximum
     * tile size used by hardware or compute shading. */
    uint tile_size = 16;
    bool tile_size_valid = false;
    do {
      tile_size *= 2;
      tiles_extent = math::divide_ceil(max_extent, int2(tile_size));
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
  update_pass_sync();
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
    sub.bind_ubo("sunlight_buf", &inst_.world.sunlight);
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

void LightModule::update_pass_sync()
{
  /* TODO(fclem): This dispatch for all light before culling. This could be made better by
   * only running on lights that survive culling using an indirect dispatch. */
  uint safe_lights_len = max_ii(lights_len_, 1);
  uint shadow_setup_dispatch_size = divide_ceil_u(safe_lights_len, CULLING_SELECT_GROUP_SIZE);

  auto &pass = update_ps_;
  pass.init();
  pass.shader_set(inst_.shaders.static_shader_get(LIGHT_SHADOW_SETUP));
  pass.bind_ssbo("light_buf", &culling_light_buf_);
  pass.bind_ssbo("light_cull_buf", &culling_data_buf_);
  pass.bind_ssbo("tilemaps_buf", &inst_.shadows.tilemap_pool.tilemaps_data);
  pass.bind_ssbo("tilemaps_clip_buf", &inst_.shadows.tilemap_pool.tilemaps_clip);
  pass.bind_resources(inst_.uniform_data);
  pass.bind_resources(inst_.sampling);
  pass.dispatch(int3(shadow_setup_dispatch_size, 1, 1));
  pass.barrier(GPU_BARRIER_SHADER_STORAGE);
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
  culling_data_buf_.view_is_flipped = view.is_inverted();
  culling_data_buf_.push_update();

  inst_.manager->submit(culling_ps_, view);
  inst_.manager->submit(update_ps_, view);
}

void LightModule::debug_draw(View &view, gpu::FrameBuffer *view_fb)
{
  if (inst_.debug_mode == eDebugMode::DEBUG_LIGHT_CULLING) {
    inst_.info_append("Debug Mode: Light Culling Validation");
    inst_.hiz_buffer.update();
    GPU_framebuffer_bind(view_fb);
    inst_.manager->submit(debug_draw_ps_, view);
  }
}

/** \} */

}  // namespace blender::eevee
