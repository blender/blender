/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <memory>

#include "BLI_hash.hh"
#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_texture_coordinates.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

/* --------------------------------------------------------------------
 * Texture Coordinates Key.
 */

TextureCoordinatesKey::TextureCoordinatesKey(const int2 &size) : size(size) {}

uint64_t TextureCoordinatesKey::hash() const
{
  return get_default_hash(size);
}

bool operator==(const TextureCoordinatesKey &a, const TextureCoordinatesKey &b)
{
  return a.size == b.size;
}

/* --------------------------------------------------------------------
 * Texture Coordinates.
 */

TextureCoordinates::TextureCoordinates(Context &context, const int2 &size)
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

TextureCoordinates::~TextureCoordinates()
{
  this->result.release();
}

void TextureCoordinates::compute_gpu(Context &context)
{
  GPUShader *shader = context.get_shader("compositor_texture_coordinates");
  GPU_shader_bind(shader);

  this->result.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, this->result.domain().size);

  this->result.unbind_as_image();
  GPU_shader_unbind();
}

void TextureCoordinates::compute_cpu()
{
  const int2 size = this->result.domain().size;
  parallel_for(size, [&](const int2 texel) {
    float2 centered_coordinates = (float2(texel) + 0.5f) - float2(size) / 2.0f;

    int max_size = math::max(size.x, size.y);
    float2 normalized_coordinates = (centered_coordinates / max_size) * 2.0f;

    this->result.store_pixel(texel, float3(normalized_coordinates, 0.0f));
  });
}

/* --------------------------------------------------------------------
 * Texture Coordinates Container.
 */

void TextureCoordinatesContainer::reset()
{
  /* First, delete all resources that are no longer needed. */
  map_.remove_if([](auto item) { return !item.value->needed; });

  /* Second, reset the needed status of the remaining resources to false to ready them to track
   * their needed status for the next evaluation. */
  for (auto &value : map_.values()) {
    value->needed = false;
  }
}

Result &TextureCoordinatesContainer::get(Context &context, const int2 &size)
{
  const TextureCoordinatesKey key(size);

  auto &texture_coordinates = *map_.lookup_or_add_cb(
      key, [&]() { return std::make_unique<TextureCoordinates>(context, size); });

  texture_coordinates.needed = true;
  return texture_coordinates.result;
}

}  // namespace blender::compositor
