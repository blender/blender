/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_reflection_probes.hh"
#include "eevee_instance.hh"

/* Generate dummy light probe texture.
 *
 * Baking of Light probes aren't implemented yet. For testing purposes this can be enabled to
 * generate a dummy texture.
 */
#define GENERATE_DUMMY_LIGHT_PROBE_TEXTURE false

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
    world_probe_data.pos = float3(0.0f);
    data_buf_[0] = world_probe_data;

    ReflectionProbe world_probe;
    world_probe.type = ReflectionProbe::Type::World;
    world_probe.do_update_data = true;
    world_probe.do_render = true;
    world_probe.index = 0;
    probes_.add(world_object_key_, world_probe);

    probes_tx_.ensure_2d_array(GPU_RGBA16F,
                               int2(max_resolution_),
                               init_num_probes_,
                               GPU_TEXTURE_USAGE_SHADER_WRITE,
                               nullptr,
                               REFLECTION_PROBE_MIPMAP_LEVELS);
    GPU_texture_mipmap_mode(probes_tx_, true, true);

    /* Cube-map is half of the resolution of the octahedral map. */
    cubemap_tx_.ensure_cube(
        GPU_RGBA16F, max_resolution_ / 2, GPU_TEXTURE_USAGE_ATTACHMENT, nullptr, 9999);
    GPU_texture_mipmap_mode(cubemap_tx_, true, true);
  }

  {
    PassSimple &pass = remap_ps_;
    pass.init();
    pass.shader_set(instance_.shaders.static_shader_get(REFLECTION_PROBE_REMAP));
    pass.bind_texture("cubemap_tx", cubemap_tx_);
    pass.bind_image("octahedral_img", probes_tx_);
    pass.bind_ssbo(REFLECTION_PROBE_BUF_SLOT, data_buf_);
    pass.dispatch(&dispatch_probe_pack_);
  }
}
void ReflectionProbeModule::begin_sync()
{
  for (ReflectionProbe &reflection_probe : probes_.values()) {
    if (reflection_probe.type == ReflectionProbe::Type::Probe) {
      reflection_probe.is_probe_used = false;
    }
  }
}

int ReflectionProbeModule::needed_layers_get() const
{
  const int max_probe_data_index = reflection_probe_data_index_max();
  int max_layer = 0;
  for (const ReflectionProbeData &data :
       Span<ReflectionProbeData>(data_buf_.data(), max_probe_data_index + 1))
  {
    max_layer = max_ii(max_layer, data.layer);
  }
  return max_layer + 1;
}

void ReflectionProbeModule::sync(const ReflectionProbe &probe)
{
  switch (probe.type) {
    case ReflectionProbe::Type::World: {
      break;
    }
    case ReflectionProbe::Type::Probe: {
      if (probe.do_render) {
        upload_dummy_texture(probe);
      }
      break;
    }
    case ReflectionProbe::Type::Unused: {
      break;
    }
  }
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
  probe_data.layer_subdivision = layer_subdivision_for(
      max_resolution_, static_cast<eLightProbeResolution>(world->probe_resolution));
}

void ReflectionProbeModule::sync_object(Object *ob, ObjectHandle &ob_handle)
{
#if GENERATE_DUMMY_LIGHT_PROBE_TEXTURE
  const ::LightProbe *light_probe = (::LightProbe *)ob->data;
  if (light_probe->type != LIGHTPROBE_TYPE_CUBE) {
    return;
  }
  const bool is_dirty = ob_handle.recalc != 0;
  int subdivision = layer_subdivision_for(
      max_resolution_, static_cast<eLightProbeResolution>(light_probe->resolution));
  ReflectionProbe &probe = find_or_insert(ob_handle, subdivision);
  probe.do_update_data |= is_dirty;
  probe.is_probe_used = true;

  ReflectionProbeData &probe_data = data_buf_[probe.index];
  probe_data.pos = float3(float4x4(ob->object_to_world) * float4(0.0, 0.0, 0.0, 1.0));
  probe_data.layer_subdivision = subdivision;
#else
  UNUSED_VARS(ob, ob_handle);
#endif
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

  /**
   * Mark space to be occupied by the given probe_data.
   *
   * The input probe data can be stored in a different subdivision level and should be converted to
   * the subdivision level what we are looking for.
   */
  void mark_space_used(const ReflectionProbeData &probe_data)
  {
    /* Number of spots that the probe data will occupied in a single dimension. */
    int clamped_subdivision_shift = max_ii(probe_data.layer_subdivision - subdivision_level_, 0);
    int spots_per_dimension = 1 << max_ii(subdivision_level_ - probe_data.layer_subdivision, 0);
    int probes_per_dimension_in_probe_data = 1 << probe_data.layer_subdivision;
    int2 pos_in_probe_data = int2(probe_data.area_index % probes_per_dimension_in_probe_data,
                                  probe_data.area_index / probes_per_dimension_in_probe_data);
    int2 pos_in_location_finder = int2(pos_in_probe_data.x >> clamped_subdivision_shift,
                                       pos_in_probe_data.y >> clamped_subdivision_shift);
    int layer_offset = probe_data.layer * probes_per_layer_;
    for (int y : IndexRange(spots_per_dimension)) {
      for (int x : IndexRange(spots_per_dimension)) {
        int2 pos = pos_in_location_finder + int2(x, y);
        int area_index = pos.x + pos.y * probes_per_dimension_;
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

  int number_layers_needed = needed_layers_get();
  int current_layers = probes_tx_.depth();
  bool resize_layers = current_layers < number_layers_needed;
  if (resize_layers) {
    /* TODO: Create new texture and copy previous texture so we don't need to rerender all the
     * probes.*/
    probes_tx_.ensure_2d_array(GPU_RGBA16F,
                               int2(max_resolution_),
                               number_layers_needed,
                               GPU_TEXTURE_USAGE_SHADER_WRITE,
                               nullptr,
                               REFLECTION_PROBE_MIPMAP_LEVELS);
    GPU_texture_mipmap_mode(probes_tx_, true, true);
  }

  recalc_lod_factors();
  data_buf_.push_update();

  /* Regenerate mipmaps when a probe texture is updated. It can be postponed when the world probe
   * is also updated. In this case it would happen as part of the WorldProbePipeline. */
  bool regenerate_mipmaps = false;
  bool regenerate_mipmaps_postponed = false;

  for (ReflectionProbe &probe : probes_.values()) {
    if (resize_layers) {
      probe.do_update_data = true;
      probe.do_render = true;
    }

    if (!probe.needs_update()) {
      continue;
    }
    sync(probe);

    switch (probe.type) {
      case ReflectionProbe::Type::World:
        regenerate_mipmaps_postponed = true;
        break;

      case ReflectionProbe::Type::Probe:
        regenerate_mipmaps = probe.do_render;
        break;

      case ReflectionProbe::Type::Unused:
        BLI_assert_unreachable();
        break;
    }
    probe.do_update_data = false;
    probe.do_render = false;
  }

  if (regenerate_mipmaps) {
    GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
    if (!regenerate_mipmaps_postponed) {
      GPU_texture_update_mipmap_chain(probes_tx_);
    }
  }
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

void ReflectionProbeModule::upload_dummy_texture(const ReflectionProbe &probe)
{
  const ReflectionProbeData &probe_data = data_buf_[probe.index];
  const int resolution = max_resolution_ >> probe_data.layer_subdivision;
  float4 *data = static_cast<float4 *>(
      MEM_mallocN(sizeof(float4) * resolution * resolution, __func__));

  /* Generate dummy checker pattern. */
  int index = 0;
  const int BLOCK_SIZE = max_ii(1024 >> probe_data.layer_subdivision, 1);
  for (int y : IndexRange(resolution)) {
    for (int x : IndexRange(resolution)) {
      int tx = (x / BLOCK_SIZE) & 1;
      int ty = (y / BLOCK_SIZE) & 1;
      bool solid = (tx + ty) & 1;
      if (solid) {
        data[index] = float4((probe.index & 1) == 0 ? 0.0f : 1.0f,
                             (probe.index & 2) == 0 ? 0.0f : 1.0f,
                             (probe.index & 4) == 0 ? 0.0f : 1.0f,
                             1.0f);
      }
      else {
        data[index] = float4(0.0f);
      }

      index++;
    }
  }

  /* Upload the checker pattern. */
  int probes_per_dimension = 1 << probe_data.layer_subdivision;
  int2 probe_area_pos(probe_data.area_index % probes_per_dimension,
                      probe_data.area_index / probes_per_dimension);
  int2 pos = probe_area_pos * int2(max_resolution_ / probes_per_dimension);
  GPU_texture_update_sub(
      probes_tx_, GPU_DATA_FLOAT, data, UNPACK2(pos), probe_data.layer, resolution, resolution, 1);

  MEM_freeN(data);
}

/** \} */

void ReflectionProbeModule::remap_to_octahedral_projection()
{
  const ReflectionProbe &world_probe = probes_.lookup(world_object_key_);
  const ReflectionProbeData &probe_data = data_buf_[world_probe.index];
  dispatch_probe_pack_ = int3(int2(ceil_division(max_resolution_ >> probe_data.layer_subdivision,
                                                 REFLECTION_PROBE_GROUP_SIZE)),
                              1);
  instance_.manager->submit(remap_ps_);
  /* TODO: Performance - Should only update the area that has changed. */
  GPU_texture_update_mipmap_chain(probes_tx_);
}

}  // namespace blender::eevee
