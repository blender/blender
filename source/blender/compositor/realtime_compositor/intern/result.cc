/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.h"
#include "GPU_state.h"
#include "GPU_texture.h"

#include "COM_domain.hh"
#include "COM_result.hh"
#include "COM_texture_pool.hh"

namespace blender::realtime_compositor {

Result::Result(ResultType type, TexturePool &texture_pool)
    : type_(type), texture_pool_(&texture_pool)
{
}

Result Result::Temporary(ResultType type, TexturePool &texture_pool)
{
  Result result = Result(type, texture_pool);
  result.set_initial_reference_count(1);
  result.reset();
  return result;
}

void Result::allocate_texture(Domain domain)
{
  /* The result is not actually needed, so allocate a dummy single value texture instead. See the
   * method description for more information. */
  if (!should_compute()) {
    allocate_single_value();
    increment_reference_count();
    return;
  }

  is_single_value_ = false;
  switch (type_) {
    case ResultType::Float:
      texture_ = texture_pool_->acquire_float(domain.size);
      break;
    case ResultType::Vector:
      texture_ = texture_pool_->acquire_vector(domain.size);
      break;
    case ResultType::Color:
      texture_ = texture_pool_->acquire_color(domain.size);
      break;
  }
  domain_ = domain;
}

void Result::allocate_single_value()
{
  is_single_value_ = true;
  /* Single values are stored in 1x1 textures as well as the single value members. */
  const int2 texture_size{1, 1};
  switch (type_) {
    case ResultType::Float:
      texture_ = texture_pool_->acquire_float(texture_size);
      break;
    case ResultType::Vector:
      texture_ = texture_pool_->acquire_vector(texture_size);
      break;
    case ResultType::Color:
      texture_ = texture_pool_->acquire_color(texture_size);
      break;
  }
  domain_ = Domain::identity();
}

void Result::allocate_invalid()
{
  allocate_single_value();
  switch (type_) {
    case ResultType::Float:
      set_float_value(0.0f);
      break;
    case ResultType::Vector:
      set_vector_value(float4(0.0f));
      break;
    case ResultType::Color:
      set_color_value(float4(0.0f));
      break;
  }
}

void Result::bind_as_texture(GPUShader *shader, const char *texture_name) const
{
  /* Make sure any prior writes to the texture are reflected before reading from it. */
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH);

  const int texture_image_unit = GPU_shader_get_sampler_binding(shader, texture_name);
  GPU_texture_bind(texture_, texture_image_unit);
}

void Result::bind_as_image(GPUShader *shader, const char *image_name, bool read) const
{
  /* Make sure any prior writes to the texture are reflected before reading from it. */
  if (read) {
    GPU_memory_barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
  }

  const int image_unit = GPU_shader_get_sampler_binding(shader, image_name);
  GPU_texture_image_bind(texture_, image_unit);
}

void Result::unbind_as_texture() const
{
  GPU_texture_unbind(texture_);
}

void Result::unbind_as_image() const
{
  GPU_texture_image_unbind(texture_);
}

void Result::pass_through(Result &target)
{
  /* Increment the reference count of the master by the original reference count of the target. */
  increment_reference_count(target.reference_count());

  /* Make the target an exact copy of this result, but keep the initial reference count, as this is
   * a property of the original result and is needed for correctly resetting the result before the
   * next evaluation. */
  const int initial_reference_count = target.initial_reference_count_;
  target = *this;
  target.initial_reference_count_ = initial_reference_count;

  target.master_ = this;
}

void Result::steal_data(Result &source)
{
  BLI_assert(type_ == source.type_);
  BLI_assert(!is_allocated() && source.is_allocated());
  BLI_assert(master_ == nullptr && source.master_ == nullptr);

  is_single_value_ = source.is_single_value_;
  texture_ = source.texture_;
  texture_pool_ = source.texture_pool_;
  domain_ = source.domain_;

  switch (type_) {
    case ResultType::Float:
      float_value_ = source.float_value_;
      break;
    case ResultType::Vector:
      vector_value_ = source.vector_value_;
      break;
    case ResultType::Color:
      color_value_ = source.color_value_;
      break;
  }

  source.texture_ = nullptr;
  source.texture_pool_ = nullptr;
}

void Result::transform(const float3x3 &transformation)
{
  domain_.transform(transformation);
}

RealizationOptions &Result::get_realization_options()
{
  return domain_.realization_options;
}

float Result::get_float_value() const
{
  return float_value_;
}

float4 Result::get_vector_value() const
{
  return vector_value_;
}

float4 Result::get_color_value() const
{
  return color_value_;
}

float Result::get_float_value_default(float default_value) const
{
  if (is_single_value()) {
    return get_float_value();
  }
  return default_value;
}

float4 Result::get_vector_value_default(const float4 &default_value) const
{
  if (is_single_value()) {
    return get_vector_value();
  }
  return default_value;
}

float4 Result::get_color_value_default(const float4 &default_value) const
{
  if (is_single_value()) {
    return get_color_value();
  }
  return default_value;
}

void Result::set_float_value(float value)
{
  float_value_ = value;
  GPU_texture_update(texture_, GPU_DATA_FLOAT, &float_value_);
}

void Result::set_vector_value(const float4 &value)
{
  vector_value_ = value;
  GPU_texture_update(texture_, GPU_DATA_FLOAT, vector_value_);
}

void Result::set_color_value(const float4 &value)
{
  color_value_ = value;
  GPU_texture_update(texture_, GPU_DATA_FLOAT, color_value_);
}

void Result::set_initial_reference_count(int count)
{
  initial_reference_count_ = count;
}

void Result::reset()
{
  master_ = nullptr;
  reference_count_ = initial_reference_count_;
}

void Result::increment_reference_count(int count)
{
  /* If there is a master result, increment its reference count instead. */
  if (master_) {
    master_->increment_reference_count(count);
    return;
  }

  reference_count_ += count;
}

void Result::release()
{
  /* If there is a master result, release it instead. */
  if (master_) {
    master_->release();
    return;
  }

  /* Decrement the reference count, and if it reaches zero, release the texture back into the
   * texture pool. */
  reference_count_--;
  if (reference_count_ == 0) {
    texture_pool_->release(texture_);
    texture_ = nullptr;
  }
}

bool Result::should_compute()
{
  return initial_reference_count_ != 0;
}

ResultType Result::type() const
{
  return type_;
}

bool Result::is_texture() const
{
  return !is_single_value_;
}

bool Result::is_single_value() const
{
  return is_single_value_;
}

bool Result::is_allocated() const
{
  return texture_ != nullptr;
}

GPUTexture *Result::texture() const
{
  return texture_;
}

int Result::reference_count() const
{
  /* If there is a master result, return its reference count instead. */
  if (master_) {
    return master_->reference_count();
  }
  return reference_count_;
}

const Domain &Result::domain() const
{
  return domain_;
}

}  // namespace blender::realtime_compositor
