/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "MEM_guardedalloc.h"

#include "BLI_assert.h"
#include "BLI_cpp_type.hh"
#include "BLI_generic_pointer.hh"
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

Result::Result(Context &context, blender::gpu::TextureFormat format)
    : context_(&context), type_(Result::type(format)), precision_(Result::precision(format))
{
}

bool Result::is_single_value_only_type(ResultType type)
{
  switch (type) {
    case ResultType::Float:
    case ResultType::Color:
    case ResultType::Float4:
    case ResultType::Float3:
    case ResultType::Float2:
    case ResultType::Int:
    case ResultType::Int2:
    case ResultType::Bool:
    case ResultType::Menu:
      return false;
    case ResultType::String:
      return true;
  }

  BLI_assert_unreachable();
  return true;
}

blender::gpu::TextureFormat Result::gpu_texture_format(ResultType type, ResultPrecision precision)
{
  switch (precision) {
    case ResultPrecision::Half:
      switch (type) {
        case ResultType::Float:
          return blender::gpu::TextureFormat::SFLOAT_16;
        case ResultType::Color:
        case ResultType::Float4:
          return blender::gpu::TextureFormat::SFLOAT_16_16_16_16;
        case ResultType::Float3:
          /* RGB textures are not fully supported by hardware, so we store Float3 results in RGBA
           * textures. */
          return blender::gpu::TextureFormat::SFLOAT_16_16_16_16;
        case ResultType::Float2:
          return blender::gpu::TextureFormat::SFLOAT_16_16;
        case ResultType::Int:
          return blender::gpu::TextureFormat::SINT_16;
        case ResultType::Int2:
          return blender::gpu::TextureFormat::SINT_16_16;
        case ResultType::Bool:
          /* No bool texture formats, so we store in an 8-bit integer. Precision doesn't matter. */
          return blender::gpu::TextureFormat::SINT_8;
        case ResultType::Menu:
          /* Menu values are technically stored in 32-bit integers, but 8 is sufficient in
           * practice. */
          return blender::gpu::TextureFormat::SINT_8;
        case ResultType::String:
          /* Single only types do not support GPU code path. */
          BLI_assert(Result::is_single_value_only_type(type));
          BLI_assert_unreachable();
          break;
      }
      break;
    case ResultPrecision::Full:
      switch (type) {
        case ResultType::Float:
          return blender::gpu::TextureFormat::SFLOAT_32;
        case ResultType::Color:
        case ResultType::Float4:
          return blender::gpu::TextureFormat::SFLOAT_32_32_32_32;
        case ResultType::Float3:
          /* RGB textures are not fully supported by hardware, so we store Float3 results in RGBA
           * textures. */
          return blender::gpu::TextureFormat::SFLOAT_32_32_32_32;
        case ResultType::Float2:
          return blender::gpu::TextureFormat::SFLOAT_32_32;
        case ResultType::Int:
          return blender::gpu::TextureFormat::SINT_32;
        case ResultType::Int2:
          return blender::gpu::TextureFormat::SINT_32_32;
        case ResultType::Bool:
          /* No bool texture formats, so we store in an 8-bit integer. Precision doesn't matter. */
          return blender::gpu::TextureFormat::SINT_8;
        case ResultType::Menu:
          /* Menu values are technically stored in 32-bit integers, but 8 is sufficient in
           * practice. */
          return blender::gpu::TextureFormat::SINT_8;
        case ResultType::String:
          /* Single only types do not support GPU storage. */
          BLI_assert(Result::is_single_value_only_type(type));
          BLI_assert_unreachable();
          break;
      }
      break;
  }

  BLI_assert_unreachable();
  return blender::gpu::TextureFormat::SFLOAT_32_32_32_32;
}

eGPUDataFormat Result::gpu_data_format(ResultType type)
{
  switch (type) {
    case ResultType::Float:
    case ResultType::Color:
    case ResultType::Float4:
    case ResultType::Float3:
    case ResultType::Float2:
      return GPU_DATA_FLOAT;
    case ResultType::Int:
    case ResultType::Int2:
    case ResultType::Bool:
    case ResultType::Menu:
      return GPU_DATA_INT;
    case ResultType::String:
      /* Single only types do not support GPU storage. */
      BLI_assert(Result::is_single_value_only_type(type));
      BLI_assert_unreachable();
      break;
  }

  BLI_assert_unreachable();
  return GPU_DATA_FLOAT;
}

blender::gpu::TextureFormat Result::gpu_texture_format(blender::gpu::TextureFormat format,
                                                       ResultPrecision precision)
{
  switch (precision) {
    case ResultPrecision::Half:
      switch (format) {
        /* Already half precision, return the input format. */
        case blender::gpu::TextureFormat::SFLOAT_16:
        case blender::gpu::TextureFormat::SFLOAT_16_16:
        case blender::gpu::TextureFormat::SFLOAT_16_16_16:
        case blender::gpu::TextureFormat::SFLOAT_16_16_16_16:
        case blender::gpu::TextureFormat::SINT_16:
        case blender::gpu::TextureFormat::SINT_16_16:
          return format;

        /* Used to store booleans where precision doesn't matter. */
        case blender::gpu::TextureFormat::SINT_8:
          return format;

        case blender::gpu::TextureFormat::SFLOAT_32:
          return blender::gpu::TextureFormat::SFLOAT_16;
        case blender::gpu::TextureFormat::SFLOAT_32_32:
          return blender::gpu::TextureFormat::SFLOAT_16_16;
        case blender::gpu::TextureFormat::SFLOAT_32_32_32:
          return blender::gpu::TextureFormat::SFLOAT_16_16_16;
        case blender::gpu::TextureFormat::SFLOAT_32_32_32_32:
          return blender::gpu::TextureFormat::SFLOAT_16_16_16_16;
        case blender::gpu::TextureFormat::SINT_32:
          return blender::gpu::TextureFormat::SINT_16;
        case blender::gpu::TextureFormat::SINT_32_32:
          return blender::gpu::TextureFormat::SINT_16_16;
        default:
          break;
      }
      break;
    case ResultPrecision::Full:
      switch (format) {
        /* Already full precision, return the input format. */
        case blender::gpu::TextureFormat::SFLOAT_32:
        case blender::gpu::TextureFormat::SFLOAT_32_32:
        case blender::gpu::TextureFormat::SFLOAT_32_32_32:
        case blender::gpu::TextureFormat::SFLOAT_32_32_32_32:
        case blender::gpu::TextureFormat::SINT_32:
        case blender::gpu::TextureFormat::SINT_32_32:
          return format;

        /* Used to store booleans where precision doesn't matter. */
        case blender::gpu::TextureFormat::SINT_8:
          return format;

        case blender::gpu::TextureFormat::SFLOAT_16:
          return blender::gpu::TextureFormat::SFLOAT_32;
        case blender::gpu::TextureFormat::SFLOAT_16_16:
          return blender::gpu::TextureFormat::SFLOAT_32_32;
        case blender::gpu::TextureFormat::SFLOAT_16_16_16:
          return blender::gpu::TextureFormat::SFLOAT_32_32_32;
        case blender::gpu::TextureFormat::SFLOAT_16_16_16_16:
          return blender::gpu::TextureFormat::SFLOAT_32_32_32_32;
        case blender::gpu::TextureFormat::SINT_16:
          return blender::gpu::TextureFormat::SINT_32;
        case blender::gpu::TextureFormat::SINT_16_16:
          return blender::gpu::TextureFormat::SINT_32_32;
        default:
          break;
      }
      break;
  }

  BLI_assert_unreachable();
  return format;
}

ResultPrecision Result::precision(blender::gpu::TextureFormat format)
{
  switch (format) {
    case blender::gpu::TextureFormat::SFLOAT_16:
    case blender::gpu::TextureFormat::SFLOAT_16_16:
    case blender::gpu::TextureFormat::SFLOAT_16_16_16:
    case blender::gpu::TextureFormat::SFLOAT_16_16_16_16:
    case blender::gpu::TextureFormat::SINT_16:
    case blender::gpu::TextureFormat::SINT_16_16:
      return ResultPrecision::Half;
    case blender::gpu::TextureFormat::SFLOAT_32:
    case blender::gpu::TextureFormat::SFLOAT_32_32:
    case blender::gpu::TextureFormat::SFLOAT_32_32_32:
    case blender::gpu::TextureFormat::SFLOAT_32_32_32_32:
    case blender::gpu::TextureFormat::SINT_32:
    case blender::gpu::TextureFormat::SINT_32_32:
      return ResultPrecision::Full;
    /* Used to store booleans where precision doesn't matter. */
    case blender::gpu::TextureFormat::SINT_8:
      return ResultPrecision::Full;
    default:
      break;
  }

  BLI_assert_unreachable();
  return ResultPrecision::Full;
}

ResultType Result::type(blender::gpu::TextureFormat format)
{
  switch (format) {
    case blender::gpu::TextureFormat::SFLOAT_16:
    case blender::gpu::TextureFormat::SFLOAT_32:
      return ResultType::Float;
    case blender::gpu::TextureFormat::SFLOAT_16_16:
    case blender::gpu::TextureFormat::SFLOAT_32_32:
      return ResultType::Float2;
    case blender::gpu::TextureFormat::SFLOAT_16_16_16:
    case blender::gpu::TextureFormat::SFLOAT_32_32_32:
      return ResultType::Float3;
    case blender::gpu::TextureFormat::SFLOAT_16_16_16_16:
    case blender::gpu::TextureFormat::SFLOAT_32_32_32_32:
      return ResultType::Color;
    case blender::gpu::TextureFormat::SINT_16:
    case blender::gpu::TextureFormat::SINT_32:
      return ResultType::Int;
    case blender::gpu::TextureFormat::SINT_16_16:
    case blender::gpu::TextureFormat::SINT_32_32:
      return ResultType::Int2;
    case blender::gpu::TextureFormat::SINT_8:
      return ResultType::Bool;
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
    case ResultType::Float2:
      return CPPType::get<float2>();
    case ResultType::Float3:
      return CPPType::get<float3>();
    case ResultType::Float4:
      return CPPType::get<float4>();
    case ResultType::Color:
      return CPPType::get<Color>();
    case ResultType::Int:
      return CPPType::get<int32_t>();
    case ResultType::Int2:
      return CPPType::get<int2>();
    case ResultType::Bool:
      return CPPType::get<bool>();
    case ResultType::Menu:
      return CPPType::get<nodes::MenuValue>();
    case ResultType::String:
      return CPPType::get<std::string>();
  }

  BLI_assert_unreachable();
  return CPPType::get<float>();
}

const char *Result::type_name(const ResultType type)
{
  switch (type) {
    case ResultType::Float:
      return "float";
    case ResultType::Float2:
      return "float2";
    case ResultType::Float3:
      return "float3";
    case ResultType::Float4:
      return "float4";
    case ResultType::Color:
      return "color";
    case ResultType::Int:
      return "int";
    case ResultType::Int2:
      return "int2";
    case ResultType::Bool:
      return "bool";
    case ResultType::Menu:
      return "menu";
    case ResultType::String:
      return "string";
  }

  BLI_assert_unreachable();
  return "";
}

Result::operator blender::gpu::Texture *() const
{
  return this->gpu_texture();
}

const CPPType &Result::get_cpp_type() const
{
  return Result::cpp_type(this->type());
}

blender::gpu::TextureFormat Result::get_gpu_texture_format() const
{
  return Result::gpu_texture_format(type_, precision_);
}

eGPUDataFormat Result::get_gpu_data_format() const
{
  return Result::gpu_data_format(type_);
}

void Result::allocate_texture(const Domain domain,
                              const bool from_pool,
                              const std::optional<ResultStorageType> storage_type)
{
  /* Make sure we are not allocating a result that should not be computed. */
  BLI_assert(this->should_compute());
  BLI_assert(!Result::is_single_value_only_type(this->type()));

  is_single_value_ = false;
  this->allocate_data(domain.size, from_pool, storage_type);
  domain_ = domain;
}

void Result::allocate_single_value()
{
  /* Make sure we are not allocating a result that should not be computed. */
  BLI_assert(this->should_compute());

  is_single_value_ = true;

  /* Single values are stored in 1x1 image as well as the single value members. Further, they are
   * always allocated from the pool. Finally, single value only types do not support GPU code
   * paths, so we always allocate on CPU. */
  if (Result::is_single_value_only_type(this->type())) {
    this->allocate_data(int2(1), true, ResultStorageType::CPU);
  }
  else {
    this->allocate_data(int2(1), true);
  }

  domain_ = Domain::identity();

  /* It is important that we initialize single values because the variant member that stores single
   * values need to have its type initialized. */
  switch (type_) {
    case ResultType::Float:
      this->set_single_value(0.0f);
      break;
    case ResultType::Float2:
      this->set_single_value(float2(0.0f));
      break;
    case ResultType::Float3:
      this->set_single_value(float3(0.0f));
      break;
    case ResultType::Float4:
      this->set_single_value(float4(0.0f));
      break;
    case ResultType::Color:
      this->set_single_value(Color(0.0f));
      break;
    case ResultType::Int:
      this->set_single_value(0);
      break;
    case ResultType::Int2:
      this->set_single_value(int2(0));
      break;
    case ResultType::Bool:
      this->set_single_value(false);
      break;
    case ResultType::Menu:
      this->set_single_value(nodes::MenuValue(0));
      break;
    case ResultType::String:
      this->set_single_value(std::string(""));
      break;
  }
}

void Result::allocate_invalid()
{
  this->allocate_single_value();
}

Result Result::upload_to_gpu(const bool from_pool) const
{
  BLI_assert(storage_type_ == ResultStorageType::CPU);
  BLI_assert(this->is_allocated());

  Result result = Result(*context_, this->type(), this->precision());
  result.allocate_texture(this->domain().size, from_pool, ResultStorageType::GPU);

  GPU_texture_update(result, this->get_gpu_data_format(), this->cpu_data().data());
  return result;
}

Result Result::download_to_cpu() const
{
  BLI_assert(storage_type_ == ResultStorageType::GPU);
  BLI_assert(this->is_allocated());

  Result result = Result(*context_, this->type(), this->precision());
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
  void *data = GPU_texture_read(*this, this->get_gpu_data_format(), 0);
  result.steal_data(data, this->domain().size);

  return result;
}

void Result::bind_as_texture(gpu::Shader *shader, const char *texture_name) const
{
  BLI_assert(storage_type_ == ResultStorageType::GPU);

  /* Make sure any prior writes to the texture are reflected before reading from it. */
  GPU_memory_barrier(GPU_BARRIER_TEXTURE_FETCH);

  const int texture_image_unit = GPU_shader_get_sampler_binding(shader, texture_name);
  GPU_texture_bind(this->gpu_texture(), texture_image_unit);
}

void Result::bind_as_image(gpu::Shader *shader, const char *image_name, bool read) const
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

void Result::share_data(const Result &source)
{
  BLI_assert(type_ == source.type_);
  BLI_assert(!this->is_allocated() && source.is_allocated());

  /* Overwrite everything except reference count. */
  const int reference_count = reference_count_;
  *this = source;
  reference_count_ = reference_count;

  /* External data is intrinsically shared, and data_reference_count_ is nullptr in this case since
   * it is not needed. */
  if (!is_external_) {
    (*data_reference_count_)++;
  }
}

void Result::steal_data(Result &source)
{
  BLI_assert(type_ == source.type_);
  BLI_assert(precision_ == source.precision_);
  BLI_assert(!this->is_allocated() && source.is_allocated());

  /* Overwrite everything except reference counts. */
  const int reference_count = reference_count_;
  *this = source;
  reference_count_ = reference_count;

  source = Result(*context_, type_, precision_);
}

void Result::steal_data(void *data, int2 size)
{
  BLI_assert(!this->is_allocated());

  const int64_t array_size = int64_t(size.x) * int64_t(size.y);
  cpu_data_ = GMutableSpan(this->get_cpp_type(), data, array_size);
  storage_type_ = ResultStorageType::CPU;
  domain_ = Domain(size);
  data_reference_count_ = new int(1);
}

/* Returns true if the given GPU texture is compatible with the type and precision of the given
 * result. */
[[maybe_unused]] static bool is_compatible_texture(const blender::gpu::Texture *texture,
                                                   const Result &result)
{
  /* Float3 types are an exception, see the documentation on the get_gpu_texture_format method for
   * more information. */
  if (result.type() == ResultType::Float3) {
    if (GPU_texture_format(texture) ==
        Result::gpu_texture_format(blender::gpu::TextureFormat::SFLOAT_32_32_32,
                                   result.precision()))
    {
      return true;
    }
  }

  return GPU_texture_format(texture) == result.get_gpu_texture_format();
}

void Result::wrap_external(blender::gpu::Texture *texture)
{
  BLI_assert(is_compatible_texture(texture, *this));
  BLI_assert(!this->is_allocated());

  gpu_texture_ = texture;
  storage_type_ = ResultStorageType::GPU;
  is_external_ = true;
  is_single_value_ = false;
  domain_ = Domain(int2(GPU_texture_width(texture), GPU_texture_height(texture)));
}

void Result::wrap_external(void *data, int2 size)
{
  BLI_assert(!this->is_allocated());

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

const RealizationOptions &Result::get_realization_options() const
{
  return domain_.realization_options;
}

void Result::set_reference_count(int count)
{
  reference_count_ = count;
}

void Result::increment_reference_count(int count)
{
  reference_count_ += count;
}

void Result::decrement_reference_count(int count)
{
  reference_count_ -= count;
}

void Result::release()
{
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
  if (is_external_) {
    return;
  }

  if (!this->is_allocated()) {
    return;
  }

  /* Data is still shared with some other result, so decrement data reference count and reset data
   * members without actually freeing the data itself. */
  BLI_assert(*data_reference_count_ >= 1);
  if (*data_reference_count_ != 1) {
    (*data_reference_count_)--;

    switch (storage_type_) {
      case ResultStorageType::GPU:
        gpu_texture_ = nullptr;
        break;
      case ResultStorageType::CPU:
        cpu_data_ = GMutableSpan();
        break;
    }

    data_reference_count_ = nullptr;
    derived_resources_ = nullptr;

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

  delete data_reference_count_;
  data_reference_count_ = nullptr;

  delete derived_resources_;
  derived_resources_ = nullptr;
}

bool Result::should_compute()
{
  return reference_count_ != 0;
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
  return reference_count_;
}

int64_t Result::size_in_bytes() const
{
  const int64_t pixel_size = this->get_cpp_type().size;
  if (this->is_single_value()) {
    return pixel_size;
  }
  const int2 image_size = this->domain().size;
  return pixel_size * image_size.x * image_size.y;
}

GPointer Result::single_value() const
{
  return std::visit([](const auto &value) { return GPointer(&value); }, single_value_);
}

GMutablePointer Result::single_value()
{
  return std::visit([](auto &value) { return GMutablePointer(&value); }, single_value_);
}

void Result::update_single_value_data()
{
  BLI_assert(this->is_single_value());
  BLI_assert(this->is_allocated());

  switch (storage_type_) {
    case ResultStorageType::GPU:
      switch (type_) {
        case ResultType::Float:
        case ResultType::Float2:
        case ResultType::Float4:
        case ResultType::Color:
        case ResultType::Int:
        case ResultType::Int2:
        case ResultType::Bool:
        case ResultType::Menu:
          GPU_texture_update(
              this->gpu_texture(), this->get_gpu_data_format(), this->single_value().get());
          break;
        case ResultType::Float3: {
          /* Float3 results are stored in 4-component textures due to hardware limitations. So
           * pad the value with a zero before updating. */
          const float4 vector_value = float4(this->get_single_value<float3>(), 0.0f);
          GPU_texture_update(this->gpu_texture(), GPU_DATA_FLOAT, vector_value);
          break;
        }
        case ResultType::String:
          /* Single only types do not support GPU storage. */
          BLI_assert(Result::is_single_value_only_type(this->type()));
          BLI_assert_unreachable();
          break;
      }
      break;
    case ResultStorageType::CPU:
      this->get_cpp_type().copy_assign(this->single_value().get(), this->cpu_data().data());
      break;
  }
}

void Result::allocate_data(const int2 size,
                           const bool from_pool,
                           const std::optional<ResultStorageType> storage_type)
{
  BLI_assert(!this->is_allocated());

  const bool use_gpu = storage_type.has_value() ? storage_type.value() == ResultStorageType::GPU :
                                                  context_->use_gpu();
  if (use_gpu) {
    storage_type_ = ResultStorageType::GPU;
    is_from_pool_ = from_pool;

    const blender::gpu::TextureFormat format = this->get_gpu_texture_format();
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
    const int64_t item_size = cpp_type.size;
    const int64_t alignment = cpp_type.alignment;
    const int64_t array_size = int64_t(size.x) * int64_t(size.y);
    const int64_t memory_size = array_size * item_size;

    void *data = MEM_mallocN_aligned(memory_size, alignment, AT);
    cpp_type.default_construct_n(data, array_size);

    cpu_data_ = GMutableSpan(cpp_type, data, array_size);
  }

  data_reference_count_ = new int(1);
}

}  // namespace blender::compositor
