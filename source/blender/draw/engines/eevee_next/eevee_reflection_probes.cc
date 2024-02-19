/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_bit_vector.hh"

#include "eevee_instance.hh"
#include "eevee_reflection_probes.hh"

#include <iostream>

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name ProbeLocationFinder
 * \{ */

/**
 * Utility class to find a location in the probes_tx_ that can be used to store a new probe in
 * a specified subdivision level.
 */
class ProbeLocationFinder {
  BitVector<> taken_spots_;
  int probes_per_dimension_;
  int probes_per_layer_;
  int subdivision_level_;

 public:
  ProbeLocationFinder(int num_layers, int subdivision_level)
  {
    subdivision_level_ = subdivision_level;
    probes_per_dimension_ = 1 << subdivision_level_;
    probes_per_layer_ = probes_per_dimension_ * probes_per_dimension_;
    int num_spots = num_layers * probes_per_layer_;
    taken_spots_.resize(num_spots, false);
  }

  void print_debug() const
  {
    std::ostream &os = std::cout;
    int layer = 0;
    int row = 0;
    int column = 0;

    os << "subdivision " << subdivision_level_ << "\n";

    for (bool spot_taken : taken_spots_) {
      if (row == 0 && column == 0) {
        os << "layer " << layer << "\n";
      }

      os << (spot_taken ? '1' : '0');

      column++;
      if (column == probes_per_dimension_) {
        os << "\n";
        column = 0;
        row++;
      }
      if (row == probes_per_dimension_) {
        row = 0;
        layer++;
      }
    }
  }

  /**
   * Mark space to be occupied by the given probe_data.
   *
   * The input probe data can be stored in a different subdivision level and should be converted to
   * the subdivision level what we are looking for.
   */
  void mark_space_used(const ReflectionProbeAtlasCoordinate &coord)
  {
    const int shift_right = max_ii(coord.layer_subdivision - subdivision_level_, 0);
    const int shift_left = max_ii(subdivision_level_ - coord.layer_subdivision, 0);
    const int spots_per_dimension = 1 << shift_left;
    const int probes_per_dimension_in_probe_data = 1 << coord.layer_subdivision;
    const int2 pos_in_probe_data = int2(coord.area_index % probes_per_dimension_in_probe_data,
                                        coord.area_index / probes_per_dimension_in_probe_data);
    const int2 pos_in_location_finder = int2((pos_in_probe_data.x >> shift_right) << shift_left,
                                             (pos_in_probe_data.y >> shift_right) << shift_left);
    const int layer_offset = coord.layer * probes_per_layer_;
    for (const int y : IndexRange(spots_per_dimension)) {
      for (const int x : IndexRange(spots_per_dimension)) {
        const int2 pos = pos_in_location_finder + int2(x, y);
        const int area_index = pos.x + pos.y * probes_per_dimension_;
        taken_spots_[area_index + layer_offset].set();
      }
    }
  }

  /**
   * Get the first free spot.
   *
   * .x contains the layer the first free spot was detected.
   * .y contains the area_index to use.
   *
   * Asserts when no free spot is found. ProbeLocationFinder should always be initialized with an
   * additional layer to make sure that there is always a free spot.
   */
  ReflectionProbeAtlasCoordinate first_free_spot() const
  {
    ReflectionProbeAtlasCoordinate result;
    result.layer_subdivision = subdivision_level_;
    for (int index : taken_spots_.index_range()) {
      if (!taken_spots_[index]) {
        result.layer = index / probes_per_layer_;
        result.area_index = index % probes_per_layer_;
        return result;
      }
    }
    BLI_assert_unreachable();
    return result;
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reflection Probe Module
 * \{ */

eLightProbeResolution ReflectionProbeModule::reflection_probe_resolution() const
{
  switch (instance_.scene->eevee.gi_cubemap_resolution) {
    case 64:
      return LIGHT_PROBE_RESOLUTION_64;
    case 128:
      return LIGHT_PROBE_RESOLUTION_128;
    case 256:
      return LIGHT_PROBE_RESOLUTION_256;
    case 512:
      return LIGHT_PROBE_RESOLUTION_512;
    case 1024:
      return LIGHT_PROBE_RESOLUTION_1024;
    default:
      return LIGHT_PROBE_RESOLUTION_2048;
  }
  return LIGHT_PROBE_RESOLUTION_2048;
}

int ReflectionProbeModule::probe_render_extent() const
{
  return instance_.scene->eevee.gi_cubemap_resolution / 2;
}

void ReflectionProbeModule::init()
{
  if (!is_initialized) {
    is_initialized = true;

    /* Initialize the world probe. */

    ReflectionProbe world_probe = {};
    world_probe.type = ReflectionProbe::Type::WORLD;
    world_probe.is_probe_used = true;
    world_probe.do_render = true;
    world_probe.clipping_distances = float2(1.0f, 10.0f);
    world_probe.world_to_probe_transposed = float3x4::identity();
    world_probe.influence_shape = SHAPE_ELIPSOID;
    world_probe.parallax_shape = SHAPE_ELIPSOID;
    /* Full influence. */
    world_probe.influence_scale = 0.0f;
    world_probe.influence_bias = 1.0f;
    world_probe.parallax_distance = 1e10f;

    probes_.add(world_object_key_, world_probe);

    probes_tx_.ensure_2d_array(GPU_RGBA16F,
                               int2(max_resolution_),
                               1,
                               GPU_TEXTURE_USAGE_SHADER_WRITE | GPU_TEXTURE_USAGE_SHADER_READ,
                               nullptr,
                               REFLECTION_PROBE_MIPMAP_LEVELS);
    GPU_texture_mipmap_mode(probes_tx_, true, true);
    probes_tx_.clear(float4(0.0f));
  }

  {
    const RaytraceEEVEE &options = instance_.scene->eevee.ray_tracing_options;
    float probe_brightness_clamp = (options.sample_clamp > 0.0) ? options.sample_clamp : 1e20;

    PassSimple &pass = remap_ps_;
    pass.init();
    pass.shader_set(instance_.shaders.static_shader_get(REFLECTION_PROBE_REMAP));
    pass.bind_texture("cubemap_tx", &cubemap_tx_);
    pass.bind_texture("atlas_tx", &probes_tx_);
    pass.bind_image("atlas_img", &probes_tx_);
    pass.push_constant("probe_coord_packed", reinterpret_cast<int4 *>(&probe_sampling_coord_));
    pass.push_constant("write_coord_packed", reinterpret_cast<int4 *>(&probe_write_coord_));
    pass.push_constant("world_coord_packed", reinterpret_cast<int4 *>(&world_sampling_coord_));
    pass.push_constant("mip_level", &probe_mip_level_);
    pass.push_constant("probe_brightness_clamp", probe_brightness_clamp);
    pass.dispatch(&dispatch_probe_pack_);
  }

  {
    PassSimple &pass = update_irradiance_ps_;
    pass.init();
    pass.shader_set(instance_.shaders.static_shader_get(REFLECTION_PROBE_UPDATE_IRRADIANCE));
    pass.push_constant("world_coord_packed", reinterpret_cast<int4 *>(&world_sampling_coord_));
    pass.bind_image("irradiance_atlas_img", &instance_.irradiance_cache.irradiance_atlas_tx_);
    pass.bind_texture("reflection_probes_tx", &probes_tx_);
    pass.dispatch(int2(1, 1));
  }

  do_display_draw_ = false;
}

void ReflectionProbeModule::begin_sync()
{
  for (ReflectionProbe &reflection_probe : probes_.values()) {
    if (reflection_probe.type == ReflectionProbe::Type::PROBE) {
      reflection_probe.is_probe_used = false;
    }
  }

  update_probes_this_sample_ = false;
  if (update_probes_next_sample_) {
    update_probes_this_sample_ = true;
  }

  {
    PassSimple &pass = select_ps_;
    pass.init();
    pass.shader_set(instance_.shaders.static_shader_get(REFLECTION_PROBE_SELECT));
    pass.push_constant("reflection_probe_count", &reflection_probe_count_);
    pass.bind_ssbo("reflection_probe_buf", &data_buf_);
    instance_.irradiance_cache.bind_resources(pass);
    instance_.sampling.bind_resources(pass);
    pass.dispatch(&dispatch_probe_select_);
    pass.barrier(GPU_BARRIER_UNIFORM);
  }
}

int ReflectionProbeModule::needed_layers_get() const
{
  int max_layer = 0;
  for (const ReflectionProbe &probe : probes_.values()) {
    max_layer = max_ii(max_layer, probe.atlas_coord.layer);
  }
  return max_layer + 1;
}

static int layer_subdivision_for(const int max_resolution,
                                 const eLightProbeResolution probe_resolution)
{
  int i_probe_resolution = int(probe_resolution);
  return max_ii(int(log2(max_resolution)) - i_probe_resolution, 0);
}

void ReflectionProbeModule::sync_world(::World *world)
{
  ReflectionProbe &probe = probes_.lookup(world_object_key_);

  eLightProbeResolution resolution = static_cast<eLightProbeResolution>(world->probe_resolution);
  int layer_subdivision = layer_subdivision_for(max_resolution_, resolution);
  if (layer_subdivision != probe.atlas_coord.layer_subdivision) {
    probe.atlas_coord = find_empty_atlas_region(layer_subdivision);
    do_world_update_set(true);
  }
  world_sampling_coord_ = probe.atlas_coord.as_sampling_coord(atlas_extent());
}

void ReflectionProbeModule::sync_world_lookdev()
{
  ReflectionProbe &probe = probes_.lookup(world_object_key_);

  const eLightProbeResolution resolution = reflection_probe_resolution();
  int layer_subdivision = layer_subdivision_for(max_resolution_, resolution);
  if (layer_subdivision != probe.atlas_coord.layer_subdivision) {
    probe.atlas_coord = find_empty_atlas_region(layer_subdivision);
  }
  world_sampling_coord_ = probe.atlas_coord.as_sampling_coord(atlas_extent());

  do_world_update_set(true);
}

void ReflectionProbeModule::sync_object(Object *ob, ObjectHandle &ob_handle)
{
  const ::LightProbe &light_probe = *(::LightProbe *)ob->data;
  if (light_probe.type != LIGHTPROBE_TYPE_SPHERE) {
    return;
  }

  ReflectionProbe &probe = probes_.lookup_or_add_cb(ob_handle.object_key.hash(), [&]() {
    ReflectionProbe probe = {};
    probe.do_render = true;
    probe.type = ReflectionProbe::Type::PROBE;
    return probe;
  });

  probe.do_render |= (ob_handle.recalc != 0);
  probe.is_probe_used = true;

  const bool probe_sync_active = instance_.do_reflection_probe_sync();
  if (!probe_sync_active && probe.do_render) {
    update_probes_next_sample_ = true;
  }

  /* Only update data when rerendering the probes to reduce flickering. */
  if (!probe_sync_active) {
    return;
  }

  probe.clipping_distances = float2(light_probe.clipsta, light_probe.clipend);

  int subdivision = layer_subdivision_for(max_resolution_, reflection_probe_resolution());
  if (probe.atlas_coord.layer_subdivision != subdivision) {
    probe.atlas_coord = find_empty_atlas_region(subdivision);
  }

  bool use_custom_parallax = (light_probe.flag & LIGHTPROBE_FLAG_CUSTOM_PARALLAX) != 0;
  float parallax_distance = use_custom_parallax ?
                                max_ff(light_probe.distpar, light_probe.distinf) :
                                light_probe.distinf;
  float influence_distance = light_probe.distinf;
  float influence_falloff = light_probe.falloff;
  probe.influence_shape = (light_probe.attenuation_type == LIGHTPROBE_SHAPE_BOX) ? SHAPE_CUBOID :
                                                                                   SHAPE_ELIPSOID;
  probe.parallax_shape = (light_probe.parallax_type == LIGHTPROBE_SHAPE_BOX) ? SHAPE_CUBOID :
                                                                               SHAPE_ELIPSOID;

  float4x4 object_to_world = math::scale(float4x4(ob->object_to_world),
                                         float3(influence_distance));
  probe.location = object_to_world.location();
  probe.volume = math::abs(math::determinant(object_to_world));
  probe.world_to_probe_transposed = float3x4(math::transpose(math::invert(object_to_world)));
  probe.influence_scale = 1.0 / max_ff(1e-8f, influence_falloff);
  probe.influence_bias = probe.influence_scale;
  probe.parallax_distance = parallax_distance / influence_distance;

  probe.viewport_display = light_probe.flag & LIGHTPROBE_FLAG_SHOW_DATA;
  probe.viewport_display_size = light_probe.data_display_size;
}

ReflectionProbeAtlasCoordinate ReflectionProbeModule::find_empty_atlas_region(
    int subdivision_level) const
{
  ProbeLocationFinder location_finder(needed_layers_get() + 1, subdivision_level);
  for (const ReflectionProbe &probe : probes_.values()) {
    if (probe.atlas_coord.layer != -1) {
      location_finder.mark_space_used(probe.atlas_coord);
    }
  }
  return location_finder.first_free_spot();
}

void ReflectionProbeModule::end_sync()
{
  const bool probes_removed = remove_unused_probes();
  const bool world_updated = do_world_update_get();
  const bool only_world = has_only_world_probe();
  const int number_layers_needed = needed_layers_get();
  const int current_layers = probes_tx_.depth();
  const bool resize_layers = current_layers < number_layers_needed;

  const bool rerender_all_probes = resize_layers || world_updated;
  if (rerender_all_probes) {
    for (ReflectionProbe &probe : probes_.values()) {
      probe.do_render = true;
    }
  }

  const bool do_update = instance_.do_reflection_probe_sync() || (only_world && world_updated);
  if (!do_update) {
    /* World has changed this sample, but probe update isn't initialized this sample. */
    if (world_updated && !only_world) {
      update_probes_next_sample_ = true;
    }
    if (update_probes_next_sample_ && !update_probes_this_sample_) {
      DRW_viewport_request_redraw();
    }

    if (!update_probes_next_sample_ && probes_removed) {
      data_buf_.push_update();
    }
    return;
  }

  if (resize_layers) {
    probes_tx_.ensure_2d_array(GPU_RGBA16F,
                               int2(max_resolution_),
                               number_layers_needed,
                               GPU_TEXTURE_USAGE_SHADER_WRITE | GPU_TEXTURE_USAGE_SHADER_READ,
                               nullptr,
                               9999);
    GPU_texture_mipmap_mode(probes_tx_, true, true);
    probes_tx_.clear(float4(0.0f));
  }

  /* Check reset probe updating as we will rendering probes. */
  if (update_probes_this_sample_ || only_world) {
    update_probes_next_sample_ = false;
  }
  data_buf_.push_update();
}

bool ReflectionProbeModule::remove_unused_probes()
{
  const int64_t removed_count = probes_.remove_if(
      [](const ReflectionProbes::Item &item) { return !item.value.is_probe_used; });
  return removed_count > 0;
}

bool ReflectionProbeModule::do_world_update_get() const
{
  const ReflectionProbe &world_probe = probes_.lookup(world_object_key_);
  return world_probe.do_render;
}

void ReflectionProbeModule::do_world_update_set(bool value)
{
  ReflectionProbe &world_probe = probes_.lookup(world_object_key_);
  world_probe.do_render = value;
  do_world_update_irradiance_set(value);
}

void ReflectionProbeModule::do_world_update_irradiance_set(bool value)
{
  ReflectionProbe &world_probe = probes_.lookup(world_object_key_);
  world_probe.do_world_irradiance_update = value;
}

bool ReflectionProbeModule::has_only_world_probe() const
{
  return probes_.size() == 1;
}

std::optional<ReflectionProbeUpdateInfo> ReflectionProbeModule::update_info_pop(
    const ReflectionProbe::Type probe_type)
{
  const bool do_probe_sync = instance_.do_reflection_probe_sync();
  const bool only_world = has_only_world_probe();
  const int max_shift = int(log2(max_resolution_));
  for (ReflectionProbe &probe : probes_.values()) {
    if (!probe.do_render && !probe.do_world_irradiance_update) {
      continue;
    }
    if (probe.type != probe_type) {
      continue;
    }
    /* Do not update this probe during this sample. */
    if (probe.type == ReflectionProbe::Type::WORLD && !only_world && !do_probe_sync) {
      continue;
    }
    if (probe.type == ReflectionProbe::Type::PROBE && !do_probe_sync) {
      continue;
    }

    ReflectionProbeUpdateInfo info = {};
    info.probe_type = probe.type;
    info.atlas_coord = probe.atlas_coord;
    info.resolution = 1 << (max_shift - probe.atlas_coord.layer_subdivision - 1);
    info.clipping_distances = probe.clipping_distances;
    info.probe_pos = probe.location;
    info.do_render = probe.do_render;
    info.do_world_irradiance_update = probe.do_world_irradiance_update;

    probe.do_render = false;
    probe.do_world_irradiance_update = false;

    if (cubemap_tx_.ensure_cube(GPU_RGBA16F,
                                info.resolution,
                                GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_SHADER_READ))
    {
      GPU_texture_mipmap_mode(cubemap_tx_, false, true);
    }

    return info;
  }

  return std::nullopt;
}

void ReflectionProbeModule::remap_to_octahedral_projection(
    const ReflectionProbeAtlasCoordinate &atlas_coord)
{
  int resolution = max_resolution_ >> atlas_coord.layer_subdivision;
  /* Update shader parameters that change per dispatch. */
  probe_sampling_coord_ = atlas_coord.as_sampling_coord(atlas_extent());
  probe_write_coord_ = atlas_coord.as_write_coord(atlas_extent(), 0);
  probe_mip_level_ = atlas_coord.layer_subdivision;
  dispatch_probe_pack_ = int3(int2(ceil_division(resolution, REFLECTION_PROBE_GROUP_SIZE)), 1);

  instance_.manager->submit(remap_ps_);
}

void ReflectionProbeModule::update_world_irradiance()
{
  instance_.manager->submit(update_irradiance_ps_);
}

void ReflectionProbeModule::update_probes_texture_mipmaps()
{
  GPU_texture_update_mipmap_chain(probes_tx_);
}

void ReflectionProbeModule::set_view(View & /*view*/)
{
  Vector<ReflectionProbe *> probe_active;
  for (auto &probe : probes_.values()) {
    /* Last slot is reserved for the world probe. */
    if (reflection_probe_count_ >= REFLECTION_PROBES_MAX - 1) {
      break;
    }
    probe.prepare_for_upload(atlas_extent());
    /* World is always considered active and added last. */
    if (probe.type == ReflectionProbe::Type::WORLD) {
      continue;
    }
    /* TODO(fclem): Culling. */
    probe_active.append(&probe);
  }

  /* Stable sorting of probes. */
  std::sort(probe_active.begin(),
            probe_active.end(),
            [](const ReflectionProbe *a, const ReflectionProbe *b) {
              if (a->volume != b->volume) {
                /* Smallest first. */
                return a->volume < b->volume;
              }
              /* Volumes are identical. Any arbitrary criteria can be used to sort them.
               * Use position to avoid unstable result caused by depsgraph non deterministic eval
               * order. This could also become a priority parameter. */
              float3 _a = a->location;
              float3 _b = b->location;
              if (_a.x != _b.x) {
                return _a.x < _b.x;
              }
              else if (_a.y != _b.y) {
                return _a.y < _b.y;
              }
              else if (_a.z != _b.z) {
                return _a.z < _b.z;
              }
              else {
                /* Fallback to memory address, since there's no good alternative. */
                return a < b;
              }
            });

  /* Push all sorted data to the UBO. */
  int probe_id = 0;
  for (auto &probe : probe_active) {
    data_buf_[probe_id++] = *probe;
  }
  /* Add world probe at the end. */
  data_buf_[probe_id++] = probes_.lookup(world_object_key_);
  /* Tag the end of the array. */
  if (probe_id < REFLECTION_PROBES_MAX) {
    data_buf_[probe_id].atlas_coord.layer = -1;
  }
  data_buf_.push_update();

  do_display_draw_ = DRW_state_draw_support() && probe_active.size() > 0;
  if (do_display_draw_) {
    int display_index = 0;
    for (int i : probe_active.index_range()) {
      if (probe_active[i]->viewport_display) {
        display_data_buf_.get_or_resize(display_index++) = {
            i, probe_active[i]->viewport_display_size};
      }
    }
    do_display_draw_ = display_index > 0;
    if (do_display_draw_) {
      display_data_buf_.resize(display_index);
      display_data_buf_.push_update();
    }
  }

  /* Add one for world probe. */
  reflection_probe_count_ = probe_active.size() + 1;
  dispatch_probe_select_.x = divide_ceil_u(reflection_probe_count_,
                                           REFLECTION_PROBE_SELECT_GROUP_SIZE);
  instance_.manager->submit(select_ps_);
}

ReflectionProbeAtlasCoordinate ReflectionProbeModule::world_atlas_coord_get() const
{
  return probes_.lookup(world_object_key_).atlas_coord;
}

void ReflectionProbeModule::viewport_draw(View &view, GPUFrameBuffer *view_fb)
{
  if (!do_display_draw_) {
    return;
  }

  viewport_display_ps_.init();
  viewport_display_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                                 DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CULL_BACK);
  viewport_display_ps_.framebuffer_set(&view_fb);
  viewport_display_ps_.shader_set(instance_.shaders.static_shader_get(DISPLAY_PROBE_REFLECTION));
  bind_resources(viewport_display_ps_);
  viewport_display_ps_.bind_ssbo("display_data_buf", display_data_buf_);
  viewport_display_ps_.draw_procedural(GPU_PRIM_TRIS, 1, display_data_buf_.size() * 6);

  instance_.manager->submit(viewport_display_ps_, view);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debugging
 *
 * \{ */

void ReflectionProbeModule::debug_print() const
{
  std::ostream &os = std::cout;
  for (const ReflectionProbe &probe : probes_.values()) {
    switch (probe.type) {
      case ReflectionProbe::Type::WORLD: {
        os << "WORLD";
        os << " do_render: " << probe.do_render;
        os << "\n";
        break;
      }
      case ReflectionProbe::Type::PROBE: {
        os << "PROBE";
        os << " do_render: " << probe.do_render;
        os << " is_used: " << probe.is_probe_used;
        os << "\n";
        break;
      }
    }
    os << " - layer: " << probe.atlas_coord.layer;
    os << " subdivision: " << probe.atlas_coord.layer_subdivision;
    os << " area: " << probe.atlas_coord.area_index;
    os << "\n";
  }
}

/** \} */

}  // namespace blender::eevee
