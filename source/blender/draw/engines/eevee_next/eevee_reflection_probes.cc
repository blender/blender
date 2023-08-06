/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_bit_vector.hh"

#include "eevee_instance.hh"
#include "eevee_reflection_probes.hh"

namespace blender::eevee {

void ReflectionProbeModule::init()
{
  if (probes_.is_empty()) {
    ReflectionProbeData init_probe_data = {};
    init_probe_data.layer = -1;
    for (int i : IndexRange(REFLECTION_PROBES_MAX)) {
      data_buf_[i] = init_probe_data;
    }

    /* Initialize the world probe. */
    ReflectionProbeData world_probe_data{};
    world_probe_data.layer = 0;
    world_probe_data.layer_subdivision = 0;
    world_probe_data.area_index = 0;
    world_probe_data.pos = float4(0.0f);
    data_buf_[0] = world_probe_data;

    ReflectionProbe world_probe;
    world_probe.type = ReflectionProbe::Type::World;
    world_probe.do_update_data = true;
    world_probe.do_render = true;
    world_probe.index = 0;
    world_probe.clipping_distances = float2(1.0f, 10.0f);
    probes_.add(world_object_key_, world_probe);

    probes_tx_.ensure_2d_array(GPU_RGBA16F,
                               int2(max_resolution_),
                               init_num_probes_,
                               GPU_TEXTURE_USAGE_SHADER_WRITE | GPU_TEXTURE_USAGE_SHADER_READ,
                               nullptr,
                               9999);
    GPU_texture_mipmap_mode(probes_tx_, true, true);
    probes_tx_.clear(float4(0.0f));

    recalc_lod_factors();
    data_buf_.push_update();
  }

  {
    PassSimple &pass = remap_ps_;
    pass.init();
    pass.shader_set(instance_.shaders.static_shader_get(REFLECTION_PROBE_REMAP));
    pass.bind_texture("cubemap_tx", &cubemap_tx_);
    pass.bind_image("octahedral_img", &probes_tx_);
    pass.bind_ubo(REFLECTION_PROBE_BUF_SLOT, data_buf_);
    pass.push_constant("reflection_probe_index", &reflection_probe_index_);
    pass.dispatch(&dispatch_probe_pack_);
  }

  {
    PassSimple &pass = update_irradiance_ps_;
    pass.init();
    pass.shader_set(instance_.shaders.static_shader_get(REFLECTION_PROBE_UPDATE_IRRADIANCE));
    pass.push_constant("reflection_probe_index", &reflection_probe_index_);
    pass.bind_image("irradiance_atlas_img", &instance_.irradiance_cache.irradiance_atlas_tx_);
    bind_resources(&pass);
    pass.dispatch(int2(1, 1));
  }
}
void ReflectionProbeModule::begin_sync()
{
  for (ReflectionProbe &reflection_probe : probes_.values()) {
    if (reflection_probe.type == ReflectionProbe::Type::Probe) {
      reflection_probe.is_probe_used = false;
    }
  }

  update_probes_this_sample_ = false;
  if (update_probes_next_sample_) {
    update_probes_this_sample_ = true;
    instance_.sampling.reset();
  }
}

int ReflectionProbeModule::needed_layers_get() const
{
  int max_layer = 0;
  for (const ReflectionProbe &probe : probes_.values()) {
    const ReflectionProbeData &probe_data = data_buf_[probe.index];
    max_layer = max_ii(max_layer, probe_data.layer);
  }
  return max_layer + 1;
}

static int layer_subdivision_for(const int max_resolution,
                                 const eLightProbeResolution probe_resolution)
{
  int i_probe_resolution = int(probe_resolution);
  return max_ii(int(log2(max_resolution)) - i_probe_resolution, 0);
}

void ReflectionProbeModule::sync_world(::World *world, WorldHandle & /*ob_handle*/)
{
  const ReflectionProbe &probe = probes_.lookup(world_object_key_);
  ReflectionProbeData &probe_data = data_buf_[probe.index];
  int requested_layer_subdivision = layer_subdivision_for(
      max_resolution_, static_cast<eLightProbeResolution>(world->probe_resolution));
  if (requested_layer_subdivision != probe_data.layer_subdivision) {
    ReflectionProbeData new_probe_data = find_empty_reflection_probe_data(
        requested_layer_subdivision);
    probe_data.layer = new_probe_data.layer;
    probe_data.layer_subdivision = new_probe_data.layer_subdivision;
    probe_data.area_index = new_probe_data.area_index;
    do_world_update_set(true);
  }
}

void ReflectionProbeModule::sync_object(Object *ob, ObjectHandle &ob_handle)
{
  const ::LightProbe *light_probe = (::LightProbe *)ob->data;
  if (light_probe->type != LIGHTPROBE_TYPE_CUBE) {
    return;
  }
  const bool is_dirty = ob_handle.recalc != 0;
  int subdivision = layer_subdivision_for(
      max_resolution_, static_cast<eLightProbeResolution>(light_probe->resolution));
  ReflectionProbe &probe = find_or_insert(ob_handle, subdivision);
  probe.do_render |= is_dirty;
  probe.is_probe_used = true;

  /* Only update data when rerendering the probes to reduce flickering. */
  if (!instance_.do_probe_sync()) {
    update_probes_next_sample_ = true;
    return;
  }

  probe.do_update_data |= is_dirty;
  probe.clipping_distances = float2(light_probe->clipsta, light_probe->clipend);

  ReflectionProbeData &probe_data = data_buf_[probe.index];
  if (probe_data.layer_subdivision != subdivision) {
    ReflectionProbeData new_probe_data = find_empty_reflection_probe_data(subdivision);
    probe_data.layer = new_probe_data.layer;
    probe_data.layer_subdivision = new_probe_data.layer_subdivision;
    probe_data.area_index = new_probe_data.area_index;
  }

  probe_data.pos = float4x4(ob->object_to_world) * float4(0.0, 0.0, 0.0, 1.0);
}

ReflectionProbe &ReflectionProbeModule::find_or_insert(ObjectHandle &ob_handle,
                                                       int subdivision_level)
{
  ReflectionProbe &reflection_probe = probes_.lookup_or_add_cb(
      ob_handle.object_key.hash_value, [this, subdivision_level]() {
        ReflectionProbe probe;
        ReflectionProbeData probe_data = find_empty_reflection_probe_data(subdivision_level);

        probe.do_update_data = true;
        probe.do_render = true;
        probe.type = ReflectionProbe::Type::Probe;
        probe.index = reflection_probe_data_index_max() + 1;

        data_buf_[probe.index] = probe_data;
        return probe;
      });

  return reflection_probe;
}

int ReflectionProbeModule::reflection_probe_data_index_max() const
{
  int result = -1;
  for (const ReflectionProbe &probe : probes_.values()) {
    if (probe.type != ReflectionProbe::Type::Unused) {
      result = max_ii(result, probe.index);
    }
  }
  return result;
}

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
  void mark_space_used(const ReflectionProbeData &probe_data)
  {
    const int shift_right = max_ii(probe_data.layer_subdivision - subdivision_level_, 0);
    const int shift_left = max_ii(subdivision_level_ - probe_data.layer_subdivision, 0);
    const int spots_per_dimension = 1 << shift_left;
    const int probes_per_dimension_in_probe_data = 1 << probe_data.layer_subdivision;
    const int2 pos_in_probe_data = int2(probe_data.area_index % probes_per_dimension_in_probe_data,
                                        probe_data.area_index /
                                            probes_per_dimension_in_probe_data);
    const int2 pos_in_location_finder = int2((pos_in_probe_data.x >> shift_right) << shift_left,
                                             (pos_in_probe_data.y >> shift_right) << shift_left);
    const int layer_offset = probe_data.layer * probes_per_layer_;
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
  ReflectionProbeData first_free_spot() const
  {
    ReflectionProbeData result = {};
    result.layer_subdivision = subdivision_level_;
    for (int index : taken_spots_.index_range()) {
      if (!taken_spots_[index]) {
        int layer = index / probes_per_layer_;
        int area_index = index % probes_per_layer_;
        result.layer = layer;
        result.area_index = area_index;
        return result;
      }
    }

    BLI_assert_unreachable();
    return result;
  }
};

ReflectionProbeData ReflectionProbeModule::find_empty_reflection_probe_data(
    int subdivision_level) const
{
  ProbeLocationFinder location_finder(needed_layers_get() + 1, subdivision_level);
  for (const ReflectionProbeData &data :
       Span<ReflectionProbeData>(data_buf_.data(), reflection_probe_data_index_max() + 1))
  {
    location_finder.mark_space_used(data);
  }
  return location_finder.first_free_spot();
}

void ReflectionProbeModule::end_sync()
{
  remove_unused_probes();

  const bool do_update = instance_.do_probe_sync() ||
                         (has_only_world_probe() && do_world_update_get());
  if (!do_update) {
    return;
  }

  int number_layers_needed = needed_layers_get();
  int current_layers = probes_tx_.depth();
  bool resize_layers = current_layers < number_layers_needed;

  if (resize_layers) {
    probes_tx_.ensure_2d_array(GPU_RGBA16F,
                               int2(max_resolution_),
                               number_layers_needed,
                               GPU_TEXTURE_USAGE_SHADER_WRITE | GPU_TEXTURE_USAGE_SHADER_READ,
                               nullptr,
                               9999);
    GPU_texture_mipmap_mode(probes_tx_, true, true);
    probes_tx_.clear(float4(0.0f));

    for (ReflectionProbe &probe : probes_.values()) {
      probe.do_update_data = true;
      probe.do_render = true;
    }
  }

  recalc_lod_factors();
  data_buf_.push_update();
}

void ReflectionProbeModule::remove_unused_probes()
{
  bool found = false;
  do {
    found = false;
    uint64_t key_to_remove = 0;
    for (const Map<uint64_t, ReflectionProbe>::Item &item : probes_.items()) {
      const ReflectionProbe &probe = item.value;
      if (probe.type == ReflectionProbe::Type::Probe && !probe.is_probe_used) {
        key_to_remove = item.key;
        found = true;
        break;
      }
    }
    if (found) {
      probes_.remove(key_to_remove);
    }
  } while (found);
}

void ReflectionProbeModule::remove_reflection_probe_data(int reflection_probe_data_index)
{
  int max_index = reflection_probe_data_index_max();
  BLI_assert_msg(reflection_probe_data_index <= max_index,
                 "Trying to remove reflection probes when it isn't part of the reflection probe "
                 "data. This can also happens when the state is set to "
                 "ReflectionProbe::Type::UNUSED, before removing the data.");
  for (int index = reflection_probe_data_index; index < max_index; index++) {
    data_buf_[index] = data_buf_[index + 1];
  }
  for (ReflectionProbe &probe : probes_.values()) {
    if (probe.index == reflection_probe_data_index) {
      probe.index = -1;
    }
    if (probe.index > reflection_probe_data_index) {
      probe.index--;
    }
  }
  data_buf_[max_index].layer = -1;
  BLI_assert(reflection_probe_data_index_max() == max_index - 1);
}

void ReflectionProbeModule::recalc_lod_factors()
{
  for (ReflectionProbeData &probe_data : data_buf_) {
    if (probe_data.layer == -1) {
      return;
    }

    const float bias = 0.0;
    const float lod_factor =
        bias +
        0.5 * log(float(square_i(probes_tx_.width() >> probe_data.layer_subdivision))) / log(2.0);
    probe_data.lod_factor = lod_factor;
  }
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
  world_probe.do_world_irradiance_update = value;
  instance_.irradiance_cache.do_update_world_ = true;
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

/* -------------------------------------------------------------------- */
/** \name Debugging
 *
 * \{ */

void ReflectionProbeModule::debug_print() const
{
  std::ostream &os = std::cout;
  for (const ReflectionProbe &probe : probes_.values()) {
    os << probe;

    if (probe.index != -1) {
      os << data_buf_[probe.index];
    }
  }
}

std::ostream &operator<<(std::ostream &os, const ReflectionProbeData &probe_data)
{
  os << " - layer: " << probe_data.layer;
  os << " subdivision: " << probe_data.layer_subdivision;
  os << " area: " << probe_data.area_index;
  os << "\n";

  return os;
}

std::ostream &operator<<(std::ostream &os, const ReflectionProbe &probe)
{
  switch (probe.type) {
    case ReflectionProbe::Type::Unused: {
      os << "UNUSED\n";

      break;
    }
    case ReflectionProbe::Type::World: {
      os << "WORLD";
      os << " is_dirty: " << probe.do_update_data;
      os << " index: " << probe.index;
      os << "\n";
      break;
    }
    case ReflectionProbe::Type::Probe: {
      os << "PROBE";
      os << " is_dirty: " << probe.do_update_data;
      os << " is_used: " << probe.is_probe_used;
      os << " index: " << probe.index;
      os << "\n";
      break;
    }
  }
  return os;
}

/** \} */

std::optional<ReflectionProbeUpdateInfo> ReflectionProbeModule::update_info_pop(
    const ReflectionProbe::Type probe_type)
{
  const bool do_probe_sync = instance_.do_probe_sync();
  const int max_shift = int(log2(max_resolution_));
  for (const Map<uint64_t, ReflectionProbe>::Item &item : probes_.items()) {
    if (!item.value.do_render && !item.value.do_world_irradiance_update) {
      continue;
    }
    if (probe_type == ReflectionProbe::Type::World && item.value.type != probe_type) {
      return std::nullopt;
    }
    if (probe_type == ReflectionProbe::Type::Probe && item.value.type != probe_type) {
      continue;
    }
    /* Do not update this probe during this sample. */
    if (item.value.type == ReflectionProbe::Type::Probe && !do_probe_sync) {
      continue;
    }

    ReflectionProbeData &probe_data = data_buf_[item.value.index];
    ReflectionProbeUpdateInfo info = {};
    info.probe_type = item.value.type;
    info.object_key = item.key;
    info.resolution = 1 << (max_shift - probe_data.layer_subdivision - 1);
    info.clipping_distances = item.value.clipping_distances;
    info.probe_pos = float3(probe_data.pos);
    info.do_render = item.value.do_render;
    info.do_world_irradiance_update = item.value.do_world_irradiance_update;

    ReflectionProbe &probe = probes_.lookup(item.key);
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

  /* Check reset probe updating as we completed rendering all Probes. */
  if (probe_type == ReflectionProbe::Type::Probe && update_probes_this_sample_ &&
      update_probes_next_sample_)
  {
    update_probes_next_sample_ = false;
  }

  return std::nullopt;
}

void ReflectionProbeModule::remap_to_octahedral_projection(uint64_t object_key)
{
  const ReflectionProbe &probe = probes_.lookup(object_key);
  const ReflectionProbeData &probe_data = data_buf_[probe.index];

  /* Update shader parameters that change per dispatch. */
  reflection_probe_index_ = probe.index;
  dispatch_probe_pack_ = int3(int2(ceil_division(max_resolution_ >> probe_data.layer_subdivision,
                                                 REFLECTION_PROBE_GROUP_SIZE)),
                              1);

  instance_.manager->submit(remap_ps_);
}

void ReflectionProbeModule::update_world_irradiance()
{
  const ReflectionProbe &probe = probes_.lookup(world_object_key_);

  /* Update shader parameters that change per dispatch. */
  reflection_probe_index_ = probe.index;

  instance_.manager->submit(update_irradiance_ps_);
}

void ReflectionProbeModule::update_probes_texture_mipmaps()
{
  GPU_texture_update_mipmap_chain(probes_tx_);
}

}  // namespace blender::eevee
