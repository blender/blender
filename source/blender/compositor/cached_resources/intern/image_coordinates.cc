/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <memory>

#include "BLI_assert.h"
#include "BLI_hash.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"

#include "COM_context.hh"
#include "COM_image_coordinates.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

/* --------------------------------------------------------------------
 * Image Coordinates Key.
 */

ImageCoordinatesKey::ImageCoordinatesKey(const int2 &size, const CoordinatesType type)
    : size(size), type(type)
{
}

uint64_t ImageCoordinatesKey::hash() const
{
  return get_default_hash(this->size, this->type);
}

bool operator==(const ImageCoordinatesKey &a, const ImageCoordinatesKey &b)
{
  return a.size == b.size && a.type == b.type;
}

/* --------------------------------------------------------------------
 * Image Coordinates.
 */

ImageCoordinates::ImageCoordinates(Context &context, const int2 &size, const CoordinatesType type)
    : result(context.create_result(ResultType::Float2))
{
  this->result.allocate_texture(Domain(size), false);

  if (context.use_gpu()) {
    this->compute_gpu(context, type);
  }
  else {
    this->compute_cpu(type);
  }
}

ImageCoordinates::~ImageCoordinates()
{
  this->result.release();
}

static const char *get_shader_name(const CoordinatesType type)
{
  switch (type) {
    case CoordinatesType::Uniform:
      return "compositor_image_coordinates_uniform";
    case CoordinatesType::Normalized:
      return "compositor_image_coordinates_normalized";
    case CoordinatesType::Pixel:
      return "compositor_image_coordinates_pixel";
  }

  BLI_assert_unreachable();
  return "";
}

void ImageCoordinates::compute_gpu(Context &context, const CoordinatesType type)
{
  GPUShader *shader = context.get_shader(get_shader_name(type));
  GPU_shader_bind(shader);

  this->result.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, this->result.domain().size);

  this->result.unbind_as_image();
  GPU_shader_unbind();
}

void ImageCoordinates::compute_cpu(const CoordinatesType type)
{
  switch (type) {
    case CoordinatesType::Uniform: {
      const int2 size = this->result.domain().size;
      const int max_size = math::max(size.x, size.y);
      parallel_for(size, [&](const int2 texel) {
        float2 centered_coordinates = (float2(texel) + 0.5f) - float2(size) / 2.0f;
        float2 normalized_coordinates = (centered_coordinates / max_size) * 2.0f;
        this->result.store_pixel(texel, normalized_coordinates);
      });
      break;
    }
    case CoordinatesType::Normalized: {
      const int2 size = this->result.domain().size;
      parallel_for(size, [&](const int2 texel) {
        float2 normalized_coordinates = (float2(texel) + 0.5f) / float2(size);
        this->result.store_pixel(texel, normalized_coordinates);
      });
      break;
    }
    case CoordinatesType::Pixel: {
      parallel_for(this->result.domain().size,
                   [&](const int2 texel) { this->result.store_pixel(texel, float2(texel)); });
      break;
    }
  }
}

/* --------------------------------------------------------------------
 * Image Coordinates Container.
 */

void ImageCoordinatesContainer::reset()
{
  /* First, delete all resources that are no longer needed. */
  map_.remove_if([](auto item) { return !item.value->needed; });

  /* Second, reset the needed status of the remaining resources to false to ready them to track
   * their needed status for the next evaluation. */
  for (auto &value : map_.values()) {
    value->needed = false;
  }
}

Result &ImageCoordinatesContainer::get(Context &context,
                                       const int2 &size,
                                       const CoordinatesType type)
{
  const ImageCoordinatesKey key(size, type);

  auto &pixel_coordinates = *map_.lookup_or_add_cb(
      key, [&]() { return std::make_unique<ImageCoordinates>(context, size, type); });

  pixel_coordinates.needed = true;
  return pixel_coordinates.result;
}

}  // namespace blender::compositor
