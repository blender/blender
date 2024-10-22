/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "MEM_guardedalloc.h"

#include "BLI_assert.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"

#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_result.hh"

namespace blender::realtime_compositor {

Result::Result(Context &context) : context_(&context) {}

Result::Result(Context &context, ResultType type, ResultPrecision precision)
    : context_(&context), type_(type), precision_(precision)
{
}

Result::Result(Context &context, eGPUTextureFormat format)
    : context_(&context), type_(Result::type(format)), precision_(Result::precision(format))
{
}

eGPUTextureFormat Result::gpu_texture_format(ResultType type, ResultPrecision precision)
{
  switch (precision) {
    case ResultPrecision::Half:
      switch (type) {
        case ResultType::Float:
          return GPU_R16F;
        case ResultType::Vector:
        case ResultType::Color:
          return GPU_RGBA16F;
        case ResultType::Float2:
          return GPU_RG16F;
        case ResultType::Float3:
          return GPU_RGB16F;
        case ResultType::Int2:
          return GPU_RG16I;
      }
      break;
    case ResultPrecision::Full:
      switch (type) {
        case ResultType::Float:
          return GPU_R32F;
        case ResultType::Vector:
        case ResultType::Color:
          return GPU_RGBA32F;
        case ResultType::Float2:
          return GPU_RG32F;
        case ResultType::Float3:
          return GPU_RGB32F;
        case ResultType::Int2:
          return GPU_RG32I;
      }
      break;
  }

  BLI_assert_unreachable();
  return GPU_RGBA32F;
}

eGPUTextureFormat Result::gpu_texture_format(eGPUTextureFormat format, ResultPrecision precision)
{
  switch (precision) {
    case ResultPrecision::Half:
      switch (format) {
        /* Already half precision, return the input format. */
        case GPU_R16F:
        case GPU_RG16F:
        case GPU_RGB16F:
        case GPU_RGBA16F:
        case GPU_RG16I:
          return format;

        case GPU_R32F:
          return GPU_R16F;
        case GPU_RG32F:
          return GPU_RG16F;
        case GPU_RGB32F:
          return GPU_RGB16F;
        case GPU_RGBA32F:
          return GPU_RGBA16F;
        case GPU_RG32I:
          return GPU_RG16I;
        default:
          break;
      }
      break;
    case ResultPrecision::Full:
      switch (format) {
        /* Already full precision, return the input format. */
        case GPU_R32F:
        case GPU_RG32F:
        case GPU_RGB32F:
        case GPU_RGBA32F:
        case GPU_RG32I:
          return format;

        case GPU_R16F:
          return GPU_R32F;
        case GPU_RG16F:
          return GPU_RG32F;
        case GPU_RGB16F:
          return GPU_RGB32F;
        case GPU_RGBA16F:
          return GPU_RGBA32F;
        case GPU_RG16I:
          return GPU_RG32I;
        default:
          break;
      }
      break;
  }

  BLI_assert_unreachable();
  return format;
}

ResultPrecision Result::precision(eGPUTextureFormat format)
{
  switch (format) {
    case GPU_R16F:
    case GPU_RG16F:
    case GPU_RGB16F:
    case GPU_RGBA16F:
    case GPU_RG16I:
      return ResultPrecision::Half;
    case GPU_R32F:
    case GPU_RG32F:
    case GPU_RGB32F:
    case GPU_RGBA32F:
    case GPU_RG32I:
      return ResultPrecision::Full;
    default:
      break;
  }

  BLI_assert_unreachable();
  return ResultPrecision::Full;
}

ResultType Result::type(eGPUTextureFormat format)
{
  switch (format) {
    case GPU_R16F:
    case GPU_R32F:
      return ResultType::Float;
    case GPU_RG16F:
    case GPU_RG32F:
      return ResultType::Float2;
    case GPU_RGB16F:
    case GPU_RGB32F:
      return ResultType::Float3;
    case GPU_RGBA16F:
    case GPU_RGBA32F:
      return ResultType::Color;
    case GPU_RG16I:
    case GPU_RG32I:
      return ResultType::Int2;
    default:
      break;
  }

  BLI_assert_unreachable();
  return ResultType::Color;
}

ResultType Result::float_type(const int channels_count)
{
  switch (channels_count) {
    case 1:
      return ResultType::Float;
    case 2:
      return ResultType::Float2;
    case 3:
      return ResultType::Float3;
    case 4:
      return ResultType::Color;
    default:
      break;
  }

  BLI_assert_unreachable();
  return ResultType::Color;
}

Result::operator GPUTexture *() const
{
  BLI_assert(storage_type_ == ResultStorageType::GPU);
  return gpu_texture_;
}

eGPUTextureFormat Result::get_gpu_texture_format() const
{
  return Result::gpu_texture_format(type_, precision_);
}

void Result::allocate_texture(Domain domain, bool from_pool)
{
  /* The result is not actually needed, so allocate a dummy single value texture instead. See the
   * method description for more information. */
  if (!should_compute()) {
    allocate_single_value();
    increment_reference_count();
    return;
  }

  is_single_value_ = false;
  this->allocate_data(domain.size, from_pool);
  domain_ = domain;
}

void Result::allocate_single_value()
{
  /* Single values are stored in 1x1 textures as well as the single value members. Further, they
   * are always allocated from the pool. */
  is_single_value_ = true;
  this->allocate_data(int2(1), true);
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
    case ResultType::Float2:
      set_float2_value(float2(0.0f));
      break;
    case ResultType::Float3:
      set_float3_value(float3(0.0f));
      break;
    case ResultType::Int2:
      set_int2_value(int2(0));
      break;
  }
}

void Result::bind_as_texture(GPUShader *shader, const char *texture_name) const
{
  BLI_assert(storage_type_ == ResultStorageType::GPU);

  /* Make sure any prior writes to the texture are reflected before reading from it. */
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH);

  const int texture_image_unit = GPU_shader_get_sampler_binding(shader, texture_name);
  GPU_texture_bind(gpu_texture_, texture_image_unit);
}

void Result::bind_as_image(GPUShader *shader, const char *image_name, bool read) const
{
  BLI_assert(storage_type_ == ResultStorageType::GPU);

  /* Make sure any prior writes to the texture are reflected before reading from it. */
  if (read) {
    GPU_memory_barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
  }

  const int image_unit = GPU_shader_get_sampler_binding(shader, image_name);
  GPU_texture_image_bind(gpu_texture_, image_unit);
}

void Result::unbind_as_texture() const
{
  BLI_assert(storage_type_ == ResultStorageType::GPU);
  GPU_texture_unbind(gpu_texture_);
}

void Result::unbind_as_image() const
{
  BLI_assert(storage_type_ == ResultStorageType::GPU);
  GPU_texture_image_unbind(gpu_texture_);
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
  BLI_assert(precision_ == source.precision_);
  BLI_assert(!this->is_allocated() && source.is_allocated());
  BLI_assert(master_ == nullptr && source.master_ == nullptr);

  /* Overwrite everything except reference counts. */
  const int reference_count = reference_count_;
  const int initial_reference_count = initial_reference_count_;
  *this = source;
  reference_count_ = reference_count;
  initial_reference_count_ = initial_reference_count;

  source.reset();
}

void Result::wrap_external(GPUTexture *texture)
{
  BLI_assert(GPU_texture_format(texture) == this->get_gpu_texture_format());
  BLI_assert(!this->is_allocated());
  BLI_assert(!master_);

  gpu_texture_ = texture;
  storage_type_ = ResultStorageType::GPU;
  is_external_ = true;
  is_single_value_ = false;
  domain_ = Domain(int2(GPU_texture_width(texture), GPU_texture_height(texture)));
}

void Result::wrap_external(float *texture, int2 size)
{
  BLI_assert(!this->is_allocated());
  BLI_assert(!master_);

  float_texture_ = texture;
  storage_type_ = ResultStorageType::FloatCPU;
  is_external_ = true;
  domain_ = Domain(size);
}

void Result::wrap_external(int *texture, int2 size)
{
  BLI_assert(!this->is_allocated());
  BLI_assert(!master_);

  integer_texture_ = texture;
  storage_type_ = ResultStorageType::IntegerCPU;
  is_external_ = true;
  domain_ = Domain(size);
}

void Result::wrap_external(const Result &result)
{
  BLI_assert(type_ == result.type());
  BLI_assert(precision_ == result.precision());
  BLI_assert(!this->is_allocated());
  BLI_assert(!master_);

  /* Steal the data of the given result and mark it as wrapping external data, but create a
   * temporary copy of the result first, since steal_data will reset it. */
  Result result_copy = result;
  this->steal_data(result_copy);
  is_external_ = true;
}

void Result::set_transformation(const float3x3 &transformation)
{
  domain_.transformation = transformation;
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
  BLI_assert(type_ == ResultType::Float);
  BLI_assert(is_single_value_);
  return float_value_;
}

float4 Result::get_vector_value() const
{
  BLI_assert(type_ == ResultType::Vector);
  BLI_assert(is_single_value_);
  return vector_value_;
}

float4 Result::get_color_value() const
{
  BLI_assert(type_ == ResultType::Color);
  BLI_assert(is_single_value_);
  return color_value_;
}

float2 Result::get_float2_value() const
{
  BLI_assert(type_ == ResultType::Float2);
  BLI_assert(is_single_value_);
  return float2_value_;
}

float3 Result::get_float3_value() const
{
  BLI_assert(type_ == ResultType::Float3);
  BLI_assert(is_single_value_);
  return float3_value_;
}

int2 Result::get_int2_value() const
{
  BLI_assert(type_ == ResultType::Int2);
  BLI_assert(is_single_value_);
  return int2_value_;
}

float Result::get_float_value_default(float default_value) const
{
  BLI_assert(type_ == ResultType::Float);
  if (is_single_value()) {
    return get_float_value();
  }
  return default_value;
}

float4 Result::get_vector_value_default(const float4 &default_value) const
{
  BLI_assert(type_ == ResultType::Vector);
  if (is_single_value()) {
    return get_vector_value();
  }
  return default_value;
}

float4 Result::get_color_value_default(const float4 &default_value) const
{
  BLI_assert(type_ == ResultType::Color);
  if (is_single_value()) {
    return get_color_value();
  }
  return default_value;
}

float2 Result::get_float2_value_default(const float2 &default_value) const
{
  BLI_assert(type_ == ResultType::Float2);
  if (is_single_value()) {
    return get_float2_value();
  }
  return default_value;
}

float3 Result::get_float3_value_default(const float3 &default_value) const
{
  BLI_assert(type_ == ResultType::Float3);
  if (is_single_value()) {
    return get_float3_value();
  }
  return default_value;
}

int2 Result::get_int2_value_default(const int2 &default_value) const
{
  BLI_assert(type_ == ResultType::Int2);
  if (is_single_value()) {
    return get_int2_value();
  }
  return default_value;
}

void Result::set_float_value(float value)
{
  BLI_assert(type_ == ResultType::Float);
  BLI_assert(is_single_value_);
  BLI_assert(this->is_allocated());

  float_value_ = value;
  switch (storage_type_) {
    case ResultStorageType::GPU:
      GPU_texture_update(gpu_texture_, GPU_DATA_FLOAT, &value);
      break;
    case ResultStorageType::FloatCPU:
      *float_texture_ = value;
      break;
    case ResultStorageType::IntegerCPU:
      BLI_assert_unreachable();
      break;
  }
}

void Result::set_vector_value(const float4 &value)
{
  BLI_assert(type_ == ResultType::Vector);
  BLI_assert(is_single_value_);
  BLI_assert(this->is_allocated());

  vector_value_ = value;
  switch (storage_type_) {
    case ResultStorageType::GPU:
      GPU_texture_update(gpu_texture_, GPU_DATA_FLOAT, value);
      break;
    case ResultStorageType::FloatCPU:
      copy_v4_v4(float_texture_, value);
      break;
    case ResultStorageType::IntegerCPU:
      BLI_assert_unreachable();
      break;
  }
}

void Result::set_color_value(const float4 &value)
{
  BLI_assert(type_ == ResultType::Color);
  BLI_assert(is_single_value_);
  BLI_assert(this->is_allocated());

  color_value_ = value;
  switch (storage_type_) {
    case ResultStorageType::GPU:
      GPU_texture_update(gpu_texture_, GPU_DATA_FLOAT, value);
      break;
    case ResultStorageType::FloatCPU:
      copy_v4_v4(float_texture_, value);
      break;
    case ResultStorageType::IntegerCPU:
      BLI_assert_unreachable();
      break;
  }
}

void Result::set_float2_value(const float2 &value)
{
  BLI_assert(type_ == ResultType::Float2);
  BLI_assert(is_single_value_);
  BLI_assert(this->is_allocated());

  float2_value_ = value;
  switch (storage_type_) {
    case ResultStorageType::GPU:
      GPU_texture_update(gpu_texture_, GPU_DATA_FLOAT, value);
      break;
    case ResultStorageType::FloatCPU:
      copy_v2_v2(float_texture_, value);
      break;
    case ResultStorageType::IntegerCPU:
      BLI_assert_unreachable();
      break;
  }
}

void Result::set_float3_value(const float3 &value)
{
  BLI_assert(type_ == ResultType::Float3);
  BLI_assert(is_single_value_);
  BLI_assert(this->is_allocated());

  float3_value_ = value;
  switch (storage_type_) {
    case ResultStorageType::GPU:
      GPU_texture_update(gpu_texture_, GPU_DATA_FLOAT, value);
      break;
    case ResultStorageType::FloatCPU:
      copy_v3_v3(float_texture_, value);
      break;
    case ResultStorageType::IntegerCPU:
      BLI_assert_unreachable();
      break;
  }
}

void Result::set_int2_value(const int2 &value)
{
  BLI_assert(type_ == ResultType::Int2);
  BLI_assert(is_single_value_);
  BLI_assert(this->is_allocated());

  int2_value_ = value;
  switch (storage_type_) {
    case ResultStorageType::GPU:
      GPU_texture_update(gpu_texture_, GPU_DATA_INT, value);
      break;
    case ResultStorageType::FloatCPU:
      BLI_assert_unreachable();
      break;
    case ResultStorageType::IntegerCPU:
      copy_v2_v2_int(integer_texture_, value);
      break;
  }
}

void Result::set_initial_reference_count(int count)
{
  initial_reference_count_ = count;
}

void Result::reset()
{
  const int initial_reference_count = initial_reference_count_;
  *this = Result(*context_, type_, precision_);
  initial_reference_count_ = initial_reference_count;
  reference_count_ = initial_reference_count;
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
  if (reference_count_ != 0) {
    return;
  }

  this->free();
}

void Result::free()
{
  /* If there is a master result, free it instead. */
  if (master_) {
    master_->free();
    return;
  }

  if (is_external_) {
    return;
  }

  if (!this->is_allocated()) {
    return;
  }

  switch (storage_type_) {
    case ResultStorageType::GPU:
      if (is_from_pool_) {
        context_->texture_pool().release(gpu_texture_);
      }
      else {
        GPU_texture_free(gpu_texture_);
      }
      gpu_texture_ = nullptr;
      break;
    case ResultStorageType::FloatCPU:
      MEM_freeN(float_texture_);
      float_texture_ = nullptr;
      break;
    case ResultStorageType::IntegerCPU:
      MEM_freeN(integer_texture_);
      integer_texture_ = nullptr;
      break;
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

ResultPrecision Result::precision() const
{
  return precision_;
}

void Result::set_type(ResultType type)
{
  /* Changing the type can only be done if it wasn't allocated yet. */
  BLI_assert(!this->is_allocated());
  type_ = type;
}

void Result::set_precision(ResultPrecision precision)
{
  /* Changing the precision can only be done if it wasn't allocated yet. */
  BLI_assert(!this->is_allocated());
  precision_ = precision;
}

bool Result::is_single_value() const
{
  return is_single_value_;
}

bool Result::is_allocated() const
{
  switch (storage_type_) {
    case ResultStorageType::GPU:
      return gpu_texture_ != nullptr;
    case ResultStorageType::FloatCPU:
      return float_texture_ != nullptr;
    case ResultStorageType::IntegerCPU:
      return integer_texture_ != nullptr;
  }

  return false;
}

int Result::reference_count() const
{
  /* If there is a master result, return its reference count instead. */
  if (master_) {
    return master_->reference_count();
  }
  return reference_count_;
}

void Result::allocate_data(int2 size, bool from_pool)
{
  if (context_->use_gpu()) {
    is_from_pool_ = from_pool;
    if (from_pool) {
      gpu_texture_ = context_->texture_pool().acquire(size, this->get_gpu_texture_format());
    }
    else {
      gpu_texture_ = GPU_texture_create_2d(__func__,
                                           size.x,
                                           size.y,
                                           1,
                                           this->get_gpu_texture_format(),
                                           GPU_TEXTURE_USAGE_GENERAL,
                                           nullptr);
    }
  }
  else {
    switch (type_) {
      case ResultType::Float:
      case ResultType::Vector:
      case ResultType::Color:
      case ResultType::Float2:
      case ResultType::Float3:
        float_texture_ = static_cast<float *>(MEM_malloc_arrayN(
            int64_t(size.x) * int64_t(size.y), this->channels_count() * sizeof(float), __func__));
        storage_type_ = ResultStorageType::FloatCPU;
        break;
      case ResultType::Int2:
        integer_texture_ = static_cast<int *>(MEM_malloc_arrayN(
            int64_t(size.x) * int64_t(size.y), this->channels_count() * sizeof(int), __func__));
        storage_type_ = ResultStorageType::IntegerCPU;
        break;
    }
  }
}

}  // namespace blender::realtime_compositor
