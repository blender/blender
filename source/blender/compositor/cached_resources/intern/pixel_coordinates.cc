/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <memory>

#include "BLI_hash.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"

#include "COM_context.hh"
#include "COM_pixel_coordinates.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

/* --------------------------------------------------------------------
 * Pixel Coordinates Key.
 */

PixelCoordinatesKey::PixelCoordinatesKey(const int2 &size) : size(size) {}

uint64_t PixelCoordinatesKey::hash() const
{
  return get_default_hash(size);
}

bool operator==(const PixelCoordinatesKey &a, const PixelCoordinatesKey &b)
{
  return a.size == b.size;
}

/* --------------------------------------------------------------------
 * Pixel Coordinates.
 */

PixelCoordinates::PixelCoordinates(Context &context, const int2 &size)
    : result(context.create_result(ResultType::Float3))
{
  this->result.allocate_texture(Domain(size), false);

  if (context.use_gpu()) {
    this->compute_gpu(context);
  }
  else {
    this->compute_cpu();
  }
}

PixelCoordinates::~PixelCoordinates()
{
  this->result.release();
}

void PixelCoordinates::compute_gpu(Context &context)
{
  GPUShader *shader = context.get_shader("compositor_pixel_coordinates");
  GPU_shader_bind(shader);

  this->result.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, this->result.domain().size);

  this->result.unbind_as_image();
  GPU_shader_unbind();
}

void PixelCoordinates::compute_cpu()
{
  parallel_for(this->result.domain().size, [&](const int2 texel) {
    this->result.store_pixel(texel, float3(float2(texel) + float2(0.5f), 0.0f));
  });
}

/* --------------------------------------------------------------------
 * Pixel Coordinates Container.
 */

void PixelCoordinatesContainer::reset()
{
  /* First, delete all resources that are no longer needed. */
  map_.remove_if([](auto item) { return !item.value->needed; });

  /* Second, reset the needed status of the remaining resources to false to ready them to track
   * their needed status for the next evaluation. */
  for (auto &value : map_.values()) {
    value->needed = false;
  }
}

Result &PixelCoordinatesContainer::get(Context &context, const int2 &size)
{
  const PixelCoordinatesKey key(size);

  auto &pixel_coordinates = *map_.lookup_or_add_cb(
      key, [&]() { return std::make_unique<PixelCoordinates>(context, size); });

  pixel_coordinates.needed = true;
  return pixel_coordinates.result;
}

}  // namespace blender::compositor
