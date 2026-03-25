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

ImageCoordinatesKey::ImageCoordinatesKey(const Domain &domain, const CoordinatesType type)
    : data_size(domain.data_size),
      display_size(domain.display_size),
      data_offset(domain.data_offset),
      type(type)
{
}

uint64_t ImageCoordinatesKey::hash() const
{
  return get_default_hash(this->data_size, this->display_size, this->data_offset, this->type);
}

bool operator==(const ImageCoordinatesKey &a, const ImageCoordinatesKey &b)
{
  return a.data_size == b.data_size && a.display_size == b.display_size &&
         a.data_offset == b.data_offset && a.type == b.type;
}

/* --------------------------------------------------------------------
 * Image Coordinates.
 */

ImageCoordinates::ImageCoordinates(Context &context,
                                   const Domain &domain,
                                   const CoordinatesType type)
    : result(context.create_result(type == CoordinatesType::Pixel ? ResultType::Int2 :
                                                                    ResultType::Float2))
{
  this->result.allocate_texture(domain, false);

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
  gpu::Shader *shader = context.get_shader(get_shader_name(type));
  GPU_shader_bind(shader);

  const Domain domain = this->result.domain();
  GPU_shader_uniform_2iv(shader, "data_offset", domain.data_offset);
  if (type != CoordinatesType::Pixel) {
    GPU_shader_uniform_2iv(shader, "display_size", domain.display_size);
  }

  this->result.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, domain.data_size);

  this->result.unbind_as_image();
  GPU_shader_unbind();
}

void ImageCoordinates::compute_cpu(const CoordinatesType type)
{
  const Domain domain = this->result.domain();
  switch (type) {
    case CoordinatesType::Uniform: {
      const int max_display_size = math::reduce_max(domain.display_size);
      parallel_for(domain.data_size, [&](const int2 texel) {
        const float2 coordinates = float2(domain.data_offset + texel) + 0.5f;
        const float2 centered_coordinates = coordinates - float2(domain.display_size) / 2.0f;
        const float2 normalized_coordinates = (centered_coordinates / max_display_size) * 2.0f;
        this->result.store_pixel(texel, normalized_coordinates);
      });
      break;
    }
    case CoordinatesType::Normalized: {
      parallel_for(domain.data_size, [&](const int2 texel) {
        const float2 coordinates = float2(domain.data_offset + texel) + 0.5f;
        const float2 normalized_coordinates = coordinates / float2(domain.display_size);
        this->result.store_pixel(texel, normalized_coordinates);
      });
      break;
    }
    case CoordinatesType::Pixel: {
      parallel_for(domain.data_size, [&](const int2 texel) {
        this->result.store_pixel(texel, domain.data_offset + texel);
      });
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
                                       const Domain &domain,
                                       const CoordinatesType type)
{
  const ImageCoordinatesKey key(domain, type);

  auto &pixel_coordinates = *map_.lookup_or_add_cb(
      key, [&]() { return std::make_unique<ImageCoordinates>(context, domain, type); });

  pixel_coordinates.needed = true;
  return pixel_coordinates.result;
}

}  // namespace blender::compositor
