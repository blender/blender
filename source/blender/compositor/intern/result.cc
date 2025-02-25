/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <variant>

#include "MEM_guardedalloc.h"

#include "BLI_assert.h"
#include "BLI_cpp_type.hh"
#include "BLI_generic_span.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_utildefines.h"

#include "GPU_shader.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"
#include "GPU_texture_pool.hh"

#include "COM_context.hh"
#include "COM_derived_resources.hh"
#include "COM_domain.hh"
#include "COM_result.hh"

namespace blender::compositor {

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
        case ResultType::Color:
        case ResultType::Float4:
          return GPU_RGBA16F;
        case ResultType::Float3:
          /* RGB textures are not fully supported by hardware, so we store Float3 results in RGBA
           * textures. */
          return GPU_RGBA16F;
        case ResultType::Float2:
          return GPU_RG16F;
        case ResultType::Int:
          return GPU_R16I;
        case ResultType::Int2:
          return GPU_RG16I;
      }
      break;
    case ResultPrecision::Full:
      switch (type) {
        case ResultType::Float:
          return GPU_R32F;
        case ResultType::Color:
        case ResultType::Float4:
          return GPU_RGBA32F;
        case ResultType::Float3:
          /* RGB textures are not fully supported by hardware, so we store Float3 results in RGBA
           * textures. */
          return GPU_RGBA32F;
        case ResultType::Float2:
          return GPU_RG32F;
        case ResultType::Int:
          return GPU_R32I;
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
        case GPU_R16I:
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
        case GPU_R32I:
          return GPU_R16I;
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
        case GPU_R32I:
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
        case GPU_R16I:
          return GPU_R32I;
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
    case GPU_R16I:
    case GPU_RG16I:
      return ResultPrecision::Half;
    case GPU_R32F:
    case GPU_RG32F:
    case GPU_RGB32F:
    case GPU_RGBA32F:
    case GPU_R32I:
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
    case GPU_R16I:
    case GPU_R32I:
      return ResultType::Int;
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

const CPPType &Result::cpp_type(const ResultType type)
{
  switch (type) {
    case ResultType::Float:
      return CPPType::get<float>();
    case ResultType::Int:
      return CPPType::get<int32_t>();
    case ResultType::Color:
      return CPPType::get<float4>();
    case ResultType::Float4:
      return CPPType::get<float4>();
    case ResultType::Float2:
      return CPPType::get<float2>();
    case ResultType::Float3:
      return CPPType::get<float3>();
    case ResultType::Int2:
      return CPPType::get<int2>();
  }

  BLI_assert_unreachable();
  return CPPType::get<float>();
}

Result::operator GPUTexture *() const
{
  return this->gpu_texture();
}

const CPPType &Result::get_cpp_type() const
{
  return Result::cpp_type(this->type());
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
  /* Single values are stored in 1x1 image as well as the single value members. Further, they
   * are always allocated from the pool. */
  is_single_value_ = true;
  this->allocate_data(int2(1), true);
  domain_ = Domain::identity();

  /* It is important that we initialize single values because the variant member that stores single
   * values need to have its type initialized. */
  switch (type_) {
    case ResultType::Float:
      this->set_single_value(0.0f);
      break;
    case ResultType::Color:
      this->set_single_value(float4(0.0f));
      break;
    case ResultType::Float4:
      this->set_single_value(float4(0.0f));
      break;
    case ResultType::Float2:
      this->set_single_value(float2(0.0f));
      break;
    case ResultType::Float3:
      this->set_single_value(float3(0.0f));
      break;
    case ResultType::Int:
      this->set_single_value(0);
      break;
    case ResultType::Int2:
      this->set_single_value(int2(0));
      break;
  }
}

void Result::allocate_invalid()
{
  this->allocate_single_value();
}

void Result::bind_as_texture(GPUShader *shader, const char *texture_name) const
{
  BLI_assert(storage_type_ == ResultStorageType::GPU);

  /* Make sure any prior writes to the texture are reflected before reading from it. */
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH);

  const int texture_image_unit = GPU_shader_get_sampler_binding(shader, texture_name);
  GPU_texture_bind(this->gpu_texture(), texture_image_unit);
}

void Result::bind_as_image(GPUShader *shader, const char *image_name, bool read) const
{
  BLI_assert(storage_type_ == ResultStorageType::GPU);

  /* Make sure any prior writes to the texture are reflected before reading from it. */
  if (read) {
    GPU_memory_barrier(GPU_BARRIER_SHADER_IMAGE_ACCESS);
  }

  const int image_unit = GPU_shader_get_sampler_binding(shader, image_name);
  GPU_texture_image_bind(this->gpu_texture(), image_unit);
}

void Result::unbind_as_texture() const
{
  BLI_assert(storage_type_ == ResultStorageType::GPU);
  GPU_texture_unbind(this->gpu_texture());
}

void Result::unbind_as_image() const
{
  BLI_assert(storage_type_ == ResultStorageType::GPU);
  GPU_texture_image_unbind(this->gpu_texture());
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

/* Returns true if the given GPU texture is compatible with the type and precision of the given
 * result. */
[[maybe_unused]] static bool is_compatible_texture(const GPUTexture *texture, const Result &result)
{
  /* Float3 types are an exception, see the documentation on the get_gpu_texture_format method for
   * more information. */
  if (result.type() == ResultType::Float3) {
    if (GPU_texture_format(texture) == Result::gpu_texture_format(GPU_RGB32F, result.precision()))
    {
      return true;
    }
  }

  return GPU_texture_format(texture) == result.get_gpu_texture_format();
}

void Result::wrap_external(GPUTexture *texture)
{
  BLI_assert(is_compatible_texture(texture, *this));
  BLI_assert(!this->is_allocated());
  BLI_assert(!master_);

  gpu_texture_ = texture;
  storage_type_ = ResultStorageType::GPU;
  is_external_ = true;
  is_single_value_ = false;
  domain_ = Domain(int2(GPU_texture_width(texture), GPU_texture_height(texture)));
}

void Result::wrap_external(void *data, int2 size)
{
  BLI_assert(!this->is_allocated());
  BLI_assert(!master_);

  const int64_t array_size = int64_t(size.x) * int64_t(size.y);
  cpu_data_ = GMutableSpan(this->get_cpp_type(), data, array_size);
  storage_type_ = ResultStorageType::CPU;
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

void Result::decrement_reference_count(int count)
{
  /* If there is a master result, decrement its reference count instead. */
  if (master_) {
    master_->decrement_reference_count(count);
    return;
  }

  reference_count_ -= count;
}

void Result::release()
{
  /* If there is a master result, release it instead. */
  if (master_) {
    master_->release();
    return;
  }

  /* Decrement the reference count, and if it is not yet zero, return and do not free. */
  reference_count_--;
  BLI_assert(reference_count_ >= 0);
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
        gpu::TexturePool::get().release_texture(this->gpu_texture());
      }
      else {
        GPU_texture_free(this->gpu_texture());
      }
      gpu_texture_ = nullptr;
      break;
    case ResultStorageType::CPU:
      MEM_freeN(this->cpu_data().data());
      cpu_data_ = GMutableSpan();
      break;
  }

  delete derived_resources_;
  derived_resources_ = nullptr;
}

bool Result::should_compute()
{
  return initial_reference_count_ != 0;
}

DerivedResources &Result::derived_resources()
{
  if (!derived_resources_) {
    derived_resources_ = new DerivedResources();
  }
  return *derived_resources_;
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
      return this->gpu_texture();
    case ResultStorageType::CPU:
      return this->cpu_data().data();
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
    storage_type_ = ResultStorageType::GPU;
    is_from_pool_ = from_pool;

    const eGPUTextureFormat format = this->get_gpu_texture_format();
    const eGPUTextureUsage usage = GPU_TEXTURE_USAGE_GENERAL;
    if (from_pool) {
      gpu_texture_ = gpu::TexturePool::get().acquire_texture(size.x, size.y, format, usage);
    }
    else {
      gpu_texture_ = GPU_texture_create_2d(__func__, size.x, size.y, 1, format, usage, nullptr);
    }
  }
  else {
    storage_type_ = ResultStorageType::CPU;

    const CPPType &cpp_type = this->get_cpp_type();
    const int64_t item_size = cpp_type.size();
    const int64_t alignment = cpp_type.alignment();
    const int64_t array_size = int64_t(size.x) * int64_t(size.y);
    const int64_t memory_size = array_size * item_size;

    void *data = MEM_mallocN_aligned(memory_size, alignment, AT);
    cpp_type.default_construct_n(data, array_size);

    cpu_data_ = GMutableSpan(cpp_type, data, array_size);
  }
}

}  // namespace blender::compositor
