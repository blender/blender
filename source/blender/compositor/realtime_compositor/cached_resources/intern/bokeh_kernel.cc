/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <memory>

#include "BLI_hash.hh"
#include "BLI_math_base.h"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_bokeh_kernel.hh"
#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::realtime_compositor {

/* --------------------------------------------------------------------
 * Bokeh Kernel Key.
 */

BokehKernelKey::BokehKernelKey(
    int2 size, int sides, float rotation, float roundness, float catadioptric, float lens_shift)
    : size(size),
      sides(sides),
      rotation(rotation),
      roundness(roundness),
      catadioptric(catadioptric),
      lens_shift(lens_shift)
{
}

uint64_t BokehKernelKey::hash() const
{
  return get_default_hash(
      size, size, get_default_hash(float4(rotation, roundness, catadioptric, lens_shift)));
}

bool operator==(const BokehKernelKey &a, const BokehKernelKey &b)
{
  return a.size == b.size && a.sides == b.sides && a.rotation == b.rotation &&
         a.roundness == b.roundness && a.catadioptric == b.catadioptric &&
         a.lens_shift == b.lens_shift;
}

/* --------------------------------------------------------------------
 * Bokeh Kernel.
 */

/* The exterior angle is the angle between each two consecutive vertices of the regular polygon
 * from its center. */
static float compute_exterior_angle(int sides)
{
  return (M_PI * 2.0f) / sides;
}

static float compute_rotation(float angle, int sides)
{
  /* Offset the rotation such that the second vertex of the regular polygon lies on the positive
   * y axis, which is 90 degrees minus the angle that it makes with the positive x axis assuming
   * the first vertex lies on the positive x axis. */
  const float offset = M_PI_2 - compute_exterior_angle(sides);
  return angle - offset;
}

BokehKernel::BokehKernel(Context &context,
                         int2 size,
                         int sides,
                         float rotation,
                         float roundness,
                         float catadioptric,
                         float lens_shift)
{
  texture_ = GPU_texture_create_2d(
      "Bokeh Kernel",
      size.x,
      size.y,
      1,
      Result::texture_format(ResultType::Color, context.get_precision()),
      GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE,
      nullptr);

  GPUShader *shader = context.get_shader("compositor_bokeh_image");
  GPU_shader_bind(shader);

  GPU_shader_uniform_1f(shader, "exterior_angle", compute_exterior_angle(sides));
  GPU_shader_uniform_1f(shader, "rotation", compute_rotation(rotation, sides));
  GPU_shader_uniform_1f(shader, "roundness", roundness);
  GPU_shader_uniform_1f(shader, "catadioptric", catadioptric);
  GPU_shader_uniform_1f(shader, "lens_shift", lens_shift);

  const int image_unit = GPU_shader_get_sampler_binding(shader, "output_img");
  GPU_texture_image_bind(texture_, image_unit);

  compute_dispatch_threads_at_least(shader, size);

  GPU_texture_image_unbind(texture_);
  GPU_shader_unbind();
}

BokehKernel::~BokehKernel()
{
  GPU_texture_free(texture_);
}

void BokehKernel::bind_as_texture(GPUShader *shader, const char *texture_name) const
{
  const int texture_image_unit = GPU_shader_get_sampler_binding(shader, texture_name);
  GPU_texture_bind(texture_, texture_image_unit);
}

void BokehKernel::unbind_as_texture() const
{
  GPU_texture_unbind(texture_);
}

GPUTexture *BokehKernel::texture() const
{
  return texture_;
}

/* --------------------------------------------------------------------
 * Bokeh Kernel Container.
 */

void BokehKernelContainer::reset()
{
  /* First, delete all resources that are no longer needed. */
  map_.remove_if([](auto item) { return !item.value->needed; });

  /* Second, reset the needed status of the remaining resources to false to ready them to track
   * their needed status for the next evaluation. */
  for (auto &value : map_.values()) {
    value->needed = false;
  }
}

BokehKernel &BokehKernelContainer::get(Context &context,
                                       int2 size,
                                       int sides,
                                       float rotation,
                                       float roundness,
                                       float catadioptric,
                                       float lens_shift)
{
  const BokehKernelKey key(size, sides, rotation, roundness, catadioptric, lens_shift);

  auto &bokeh_kernel = *map_.lookup_or_add_cb(key, [&]() {
    return std::make_unique<BokehKernel>(
        context, size, sides, rotation, roundness, catadioptric, lens_shift);
  });

  bokeh_kernel.needed = true;
  return bokeh_kernel;
}

}  // namespace blender::realtime_compositor
