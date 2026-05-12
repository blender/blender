/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "BLI_assert.h"
#include "BLI_cpp_type.hh"
#include "BLI_generic_array.hh"
#include "BLI_generic_pointer.hh"
#include "BLI_generic_span.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_quaternion_types.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"
#include "GPU_texture_pool.hh"

#include "COM_context.hh"
#include "COM_derived_resources.hh"
#include "COM_domain.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

Result::Result(Context &context) : context_(&context) {}

Result::Result(Context &context, ResultType type, ResultPrecision precision)
    : context_(&context), type_(type), precision_(precision)
{
}

Result::Result(Context &context, gpu::TextureFormat format)
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
    case ResultType::Int3:
    case ResultType::Int4:
    case ResultType::Bool:
    case ResultType::Float4x4:
    case ResultType::Menu:
    case ResultType::Quaternion:
      return false;
    case ResultType::String:
    case ResultType::Object:
    case ResultType::Image:
    case ResultType::Font:
    case ResultType::Scene:
    case ResultType::Text:
    case ResultType::Mask:
      return true;
  }

  BLI_assert_unreachable();
  return true;
}

gpu::TextureFormat Result::gpu_texture_format(ResultType type, ResultPrecision precision)
{
  switch (precision) {
    case ResultPrecision::Half:
      switch (type) {
        case ResultType::Float:
          return gpu::TextureFormat::SFLOAT_16;
        case ResultType::Color:
        case ResultType::Float4:
          return gpu::TextureFormat::SFLOAT_16_16_16_16;
        case ResultType::Float3:
          /* RGB textures are not fully supported by hardware, so we store Float3 results in RGBA
           * textures. */
          return gpu::TextureFormat::SFLOAT_16_16_16_16;
        case ResultType::Float2:
          return gpu::TextureFormat::SFLOAT_16_16;
        case ResultType::Int:
          return gpu::TextureFormat::SINT_16;
        case ResultType::Int2:
          return gpu::TextureFormat::SINT_16_16;
        case ResultType::Int3:
          /* RGB textures are not fully supported by hardware, so we store Int3 results in RGBA
           * textures. */
          return gpu::TextureFormat::SINT_16_16_16_16;
        case ResultType::Int4:
          return gpu::TextureFormat::SINT_16_16_16_16;
        case ResultType::Bool:
          /* No bool texture formats, so we store in an 8-bit integer. Precision doesn't matter. */
          return gpu::TextureFormat::SINT_8;
        case ResultType::Float4x4:
          /* Stored as an array of 4 RGBA texture. */
          return gpu::TextureFormat::SFLOAT_16_16_16_16;
        case ResultType::Menu:
          /* Menu values are technically stored in 32-bit integers, but 8 is sufficient in
           * practice. */
          return gpu::TextureFormat::SINT_8;
        case ResultType::Quaternion:
          return gpu::TextureFormat::SFLOAT_16_16_16_16;
        case ResultType::String:
        case ResultType::Object:
        case ResultType::Image:
        case ResultType::Font:
        case ResultType::Scene:
        case ResultType::Text:
        case ResultType::Mask:
          /* Single only types do not support GPU code path. */
          BLI_assert(Result::is_single_value_only_type(type));
          BLI_assert_unreachable();
          break;
      }
      break;
    case ResultPrecision::Full:
      switch (type) {
        case ResultType::Float:
          return gpu::TextureFormat::SFLOAT_32;
        case ResultType::Color:
        case ResultType::Float4:
          return gpu::TextureFormat::SFLOAT_32_32_32_32;
        case ResultType::Float3:
          /* RGB textures are not fully supported by hardware, so we store Float3 results in RGBA
           * textures. */
          return gpu::TextureFormat::SFLOAT_32_32_32_32;
        case ResultType::Float2:
          return gpu::TextureFormat::SFLOAT_32_32;
        case ResultType::Int:
          return gpu::TextureFormat::SINT_32;
        case ResultType::Int2:
          return gpu::TextureFormat::SINT_32_32;
        case ResultType::Int3:
          /* RGB textures are not fully supported by hardware, so we store Int3 results in RGBA
           * textures. */
          return gpu::TextureFormat::SINT_32_32_32_32;
        case ResultType::Int4:
          return gpu::TextureFormat::SINT_32_32_32_32;
        case ResultType::Bool:
          /* No bool texture formats, so we store in an 8-bit integer. Precision doesn't matter. */
          return gpu::TextureFormat::SINT_8;
        case ResultType::Float4x4:
          /* Stored as an array of 4 RGBA texture. */
          return gpu::TextureFormat::SFLOAT_32_32_32_32;
        case ResultType::Menu:
          /* Menu values are technically stored in 32-bit integers, but 8 is sufficient in
           * practice. */
          return gpu::TextureFormat::SINT_8;
        case ResultType::Quaternion:
          return gpu::TextureFormat::SFLOAT_32_32_32_32;
        case ResultType::String:
        case ResultType::Object:
        case ResultType::Image:
        case ResultType::Font:
        case ResultType::Scene:
        case ResultType::Text:
        case ResultType::Mask:
          /* Single only types do not support GPU storage. */
          BLI_assert(Result::is_single_value_only_type(type));
          BLI_assert_unreachable();
          break;
      }
      break;
  }

  BLI_assert_unreachable();
  return gpu::TextureFormat::SFLOAT_32_32_32_32;
}

eGPUDataFormat Result::gpu_data_format(ResultType type)
{
  switch (type) {
    case ResultType::Float:
    case ResultType::Color:
    case ResultType::Quaternion:
    case ResultType::Float4:
    case ResultType::Float3:
    case ResultType::Float2:
    case ResultType::Float4x4:
      return GPU_DATA_FLOAT;
    case ResultType::Int:
    case ResultType::Int2:
    case ResultType::Int3:
    case ResultType::Int4:
    case ResultType::Bool:
    case ResultType::Menu:
      return GPU_DATA_INT;
    case ResultType::String:
    case ResultType::Object:
    case ResultType::Image:
    case ResultType::Font:
    case ResultType::Scene:
    case ResultType::Text:
    case ResultType::Mask:
      /* Single only types do not support GPU storage. */
      BLI_assert(Result::is_single_value_only_type(type));
      BLI_assert_unreachable();
      break;
  }

  BLI_assert_unreachable();
  return GPU_DATA_FLOAT;
}

gpu::TextureFormat Result::gpu_texture_format(gpu::TextureFormat format, ResultPrecision precision)
{
  switch (precision) {
    case ResultPrecision::Half:
      switch (format) {
        /* Already half precision, return the input format. */
        case gpu::TextureFormat::SFLOAT_16:
        case gpu::TextureFormat::SFLOAT_16_16:
        case gpu::TextureFormat::SFLOAT_16_16_16:
        case gpu::TextureFormat::SFLOAT_16_16_16_16:
        case gpu::TextureFormat::SINT_16:
        case gpu::TextureFormat::SINT_16_16:
        case gpu::TextureFormat::SINT_16_16_16_16:
          return format;

        /* Used to store booleans where precision doesn't matter. */
        case gpu::TextureFormat::SINT_8:
          return format;

        case gpu::TextureFormat::SFLOAT_32:
          return gpu::TextureFormat::SFLOAT_16;
        case gpu::TextureFormat::SFLOAT_32_32:
          return gpu::TextureFormat::SFLOAT_16_16;
        case gpu::TextureFormat::SFLOAT_32_32_32:
          return gpu::TextureFormat::SFLOAT_16_16_16;
        case gpu::TextureFormat::SFLOAT_32_32_32_32:
          return gpu::TextureFormat::SFLOAT_16_16_16_16;
        case gpu::TextureFormat::SINT_32:
          return gpu::TextureFormat::SINT_16;
        case gpu::TextureFormat::SINT_32_32:
          return gpu::TextureFormat::SINT_16_16;
        case gpu::TextureFormat::SINT_32_32_32_32:
          return gpu::TextureFormat::SINT_16_16_16_16;
        default:
          break;
      }
      break;
    case ResultPrecision::Full:
      switch (format) {
        /* Already full precision, return the input format. */
        case gpu::TextureFormat::SFLOAT_32:
        case gpu::TextureFormat::SFLOAT_32_32:
        case gpu::TextureFormat::SFLOAT_32_32_32:
        case gpu::TextureFormat::SFLOAT_32_32_32_32:
        case gpu::TextureFormat::SINT_32:
        case gpu::TextureFormat::SINT_32_32:
        case gpu::TextureFormat::SINT_32_32_32_32:
          return format;

        /* Used to store booleans where precision doesn't matter. */
        case gpu::TextureFormat::SINT_8:
          return format;

        case gpu::TextureFormat::SFLOAT_16:
          return gpu::TextureFormat::SFLOAT_32;
        case gpu::TextureFormat::SFLOAT_16_16:
          return gpu::TextureFormat::SFLOAT_32_32;
        case gpu::TextureFormat::SFLOAT_16_16_16:
          return gpu::TextureFormat::SFLOAT_32_32_32;
        case gpu::TextureFormat::SFLOAT_16_16_16_16:
          return gpu::TextureFormat::SFLOAT_32_32_32_32;
        case gpu::TextureFormat::SINT_16:
          return gpu::TextureFormat::SINT_32;
        case gpu::TextureFormat::SINT_16_16:
          return gpu::TextureFormat::SINT_32_32;
        case gpu::TextureFormat::SINT_16_16_16_16:
          return gpu::TextureFormat::SINT_32_32_32_32;
        default:
          break;
      }
      break;
  }

  BLI_assert_unreachable();
  return format;
}

ResultPrecision Result::precision(gpu::TextureFormat format)
{
  switch (format) {
    case gpu::TextureFormat::SFLOAT_16:
    case gpu::TextureFormat::SFLOAT_16_16:
    case gpu::TextureFormat::SFLOAT_16_16_16:
    case gpu::TextureFormat::SFLOAT_16_16_16_16:
    case gpu::TextureFormat::SINT_16:
    case gpu::TextureFormat::SINT_16_16:
    case gpu::TextureFormat::SINT_16_16_16_16:
      return ResultPrecision::Half;
    case gpu::TextureFormat::SFLOAT_32:
    case gpu::TextureFormat::SFLOAT_32_32:
    case gpu::TextureFormat::SFLOAT_32_32_32:
    case gpu::TextureFormat::SFLOAT_32_32_32_32:
    case gpu::TextureFormat::SINT_32:
    case gpu::TextureFormat::SINT_32_32:
    case gpu::TextureFormat::SINT_32_32_32_32:
      return ResultPrecision::Full;
    /* Used to store booleans where precision doesn't matter. */
    case gpu::TextureFormat::SINT_8:
      return ResultPrecision::Full;
    default:
      break;
  }

  BLI_assert_unreachable();
  return ResultPrecision::Full;
}

ResultType Result::type(gpu::TextureFormat format)
{
  switch (format) {
    case gpu::TextureFormat::SFLOAT_16:
    case gpu::TextureFormat::SFLOAT_32:
      return ResultType::Float;
    case gpu::TextureFormat::SFLOAT_16_16:
    case gpu::TextureFormat::SFLOAT_32_32:
      return ResultType::Float2;
    case gpu::TextureFormat::SFLOAT_16_16_16:
    case gpu::TextureFormat::SFLOAT_32_32_32:
      return ResultType::Float3;
    case gpu::TextureFormat::SFLOAT_16_16_16_16:
    case gpu::TextureFormat::SFLOAT_32_32_32_32:
      return ResultType::Color;
    case gpu::TextureFormat::SINT_16:
    case gpu::TextureFormat::SINT_32:
      return ResultType::Int;
    case gpu::TextureFormat::SINT_16_16:
    case gpu::TextureFormat::SINT_32_32:
      return ResultType::Int2;
    case gpu::TextureFormat::SINT_16_16_16_16:
    case gpu::TextureFormat::SINT_32_32_32_32:
      return ResultType::Int4;
    case gpu::TextureFormat::SINT_8:
      return ResultType::Bool;
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
    case ResultType::Int3:
      return CPPType::get<int3>();
    case ResultType::Int4:
      return CPPType::get<int4>();
    case ResultType::Bool:
      return CPPType::get<bool>();
    case ResultType::Float4x4:
      return CPPType::get<float4x4>();
    case ResultType::Menu:
      return CPPType::get<nodes::MenuValue>();
    case ResultType::Quaternion:
      return CPPType::get<math::Quaternion>();
    case ResultType::String:
      return CPPType::get<std::string>();
    case ResultType::Object:
      return CPPType::get<Object *>();
    case ResultType::Image:
      return CPPType::get<Image *>();
    case ResultType::Font:
      return CPPType::get<VFont *>();
    case ResultType::Scene:
      return CPPType::get<Scene *>();
    case ResultType::Text:
      return CPPType::get<Text *>();
    case ResultType::Mask:
      return CPPType::get<Mask *>();
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
    case ResultType::Int3:
      return "int3";
    case ResultType::Int4:
      return "int4";
    case ResultType::Bool:
      return "bool";
    case ResultType::Float4x4:
      return "float4x4";
    case ResultType::Menu:
      return "menu";
    case ResultType::Quaternion:
      return "quaternion";
    case ResultType::String:
      return "string";
    case ResultType::Object:
      return "object";
    case ResultType::Image:
      return "image";
    case ResultType::Font:
      return "font";
    case ResultType::Scene:
      return "scene";
    case ResultType::Text:
      return "text";
    case ResultType::Mask:
      return "mask";
  }

  BLI_assert_unreachable();
  return "";
}

Result::operator gpu::Texture *() const
{
  return this->gpu_texture();
}

const CPPType &Result::get_cpp_type() const
{
  return Result::cpp_type(this->type());
}

gpu::TextureFormat Result::get_gpu_texture_format() const
{
  return Result::gpu_texture_format(type_, precision_);
}

eGPUDataFormat Result::get_gpu_data_format() const
{
  return Result::gpu_data_format(type_);
}

static Domain sanitize_domain_data_size(const Domain domain,
                                        const Context &context,
                                        const std::optional<ResultStorageType> storage_type)
{
  Domain sanitized_domain = domain;
  const bool use_gpu = storage_type.has_value() ? storage_type.value() == ResultStorageType::GPU :
                                                  context.use_gpu();
  const int max_size = use_gpu ? 8192 : 32768;
  sanitized_domain.data_size = math::clamp(domain.data_size, int2(1), int2(max_size));
  return sanitized_domain;
}

void Result::allocate_texture(const Domain domain,
                              const bool from_pool,
                              const std::optional<ResultStorageType> storage_type)
{
  /* Make sure we are not allocating a result that should not be computed. */
  BLI_assert(this->should_compute());
  BLI_assert(!Result::is_single_value_only_type(this->type()));

  is_single_value_ = false;
  domain_ = sanitize_domain_data_size(domain, *context_, storage_type);
  this->allocate_data(domain_.data_size, from_pool, storage_type);
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
    case ResultType::Int3:
      this->set_single_value(int3(0));
      break;
    case ResultType::Int4:
      this->set_single_value(int4(0));
      break;
    case ResultType::Bool:
      this->set_single_value(false);
      break;
    case ResultType::Float4x4:
      this->set_single_value(float4x4::zero());
      break;
    case ResultType::Menu:
      this->set_single_value(nodes::MenuValue(0));
      break;
    case ResultType::Quaternion:
      this->set_single_value(math::Quaternion(0.0f, 0.0f, 0.0f, 0.0f));
      break;
    case ResultType::String:
      this->set_single_value(std::string(""));
      break;
    case ResultType::Object:
      this->set_single_value(static_cast<Object *>(nullptr));
      break;
    case ResultType::Image:
      this->set_single_value(static_cast<Image *>(nullptr));
      break;
    case ResultType::Font:
      this->set_single_value(static_cast<VFont *>(nullptr));
      break;
    case ResultType::Scene:
      this->set_single_value(static_cast<Scene *>(nullptr));
      break;
    case ResultType::Text:
      this->set_single_value(static_cast<Text *>(nullptr));
      break;
    case ResultType::Mask:
      this->set_single_value(static_cast<Mask *>(nullptr));
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
  BLI_assert(!this->is_single_value());

  Result result = Result(*context_, this->type(), this->precision());
  result.allocate_texture(this->domain(), from_pool, ResultStorageType::GPU);

  switch (this->type()) {
    case ResultType::Float:
    case ResultType::Color:
    case ResultType::Quaternion:
    case ResultType::Float4:
    case ResultType::Float2:
    case ResultType::Int:
    case ResultType::Int2:
    case ResultType::Int4:
    case ResultType::Bool:
    case ResultType::Menu:
      GPU_texture_update(result, this->get_gpu_data_format(), this->cpu_data().data());
      break;
    case ResultType::Int3: {
      /* Int3 is stored as an Int4 on GPU due to hardware limitations, so copy to an Int4 result
       * before uploading. */
      Result temporary_result = Result(*context_, ResultType::Int4, this->precision());
      temporary_result.allocate_texture(this->domain(), false, ResultStorageType::CPU);
      parallel_for(this->domain().data_size, [&](const int2 texel) {
        temporary_result.store_pixel(texel, int4(this->load_pixel<int3>(texel), 0));
      });
      GPU_texture_update(result, this->get_gpu_data_format(), temporary_result.cpu_data().data());
      temporary_result.release();
      break;
    }
    case ResultType::Float3: {
      /* Float3 is stored as a Float4 on GPU due to hardware limitations, so copy to a Float4
       * result before uploading. */
      Result temporary_result = Result(*context_, ResultType::Float4, this->precision());
      temporary_result.allocate_texture(this->domain(), false, ResultStorageType::CPU);
      parallel_for(this->domain().data_size, [&](const int2 texel) {
        temporary_result.store_pixel(texel, float4(this->load_pixel<float3>(texel), 0.0f));
      });
      GPU_texture_update(result, this->get_gpu_data_format(), temporary_result.cpu_data().data());
      temporary_result.release();
      break;
    }
    case ResultType::Float4x4: {
      const int2 size = this->domain().data_size;
      Result temporary_result = Result(*context_, ResultType::Float4, this->precision());
      /* Float4x4 is stored in a 4-layer array texture, with each layer storing a column, so copy
       * to a result with 4 times the height, each slice storing a column. */
      temporary_result.allocate_texture(size * int2(1, 4), false, ResultStorageType::CPU);
      for (int i = 0; i < 4; i++) {
        parallel_for(this->domain().data_size, [&](const int2 texel) {
          temporary_result.store_pixel(texel + int2(0, size.y * i),
                                       this->load_pixel<float4x4>(texel)[i]);
        });
      }
      GPU_texture_update(result, this->get_gpu_data_format(), temporary_result.cpu_data().data());
      temporary_result.release();
      break;
    }
    case ResultType::String:
    case ResultType::Object:
    case ResultType::Image:
    case ResultType::Font:
    case ResultType::Scene:
    case ResultType::Text:
    case ResultType::Mask:
      /* Single only types do not support GPU. */
      break;
  }
  return result;
}

Result Result::download_to_cpu() const
{
  BLI_assert(storage_type_ == ResultStorageType::GPU);
  BLI_assert(this->is_allocated());

  Result result = Result(*context_, this->type(), this->precision());
  result.allocate_texture(this->domain(), false, ResultStorageType::CPU);

  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);

  switch (this->type()) {
    case ResultType::Float:
    case ResultType::Color:
    case ResultType::Quaternion:
    case ResultType::Float4:
    case ResultType::Float2:
    case ResultType::Int:
    case ResultType::Int2:
    case ResultType::Int4:
    case ResultType::Bool:
    case ResultType::Menu:
      GPU_texture_read(*this, this->get_gpu_data_format(), 0, result.cpu_data_for_write().data());
      break;
    case ResultType::Int3: {
      if (this->channels_count() == 3) {
        GPU_texture_read(
            *this, this->get_gpu_data_format(), 0, result.cpu_data_for_write().data());
        break;
      }
      /* Int3 is stored as an Int4 on GPU due to hardware limitations, so read to an Int4 result
       * before copying to result. */
      Result temporary_result = Result(*context_, ResultType::Int4, this->precision());
      temporary_result.allocate_texture(this->domain(), false, ResultStorageType::CPU);
      GPU_texture_read(
          *this, this->get_gpu_data_format(), 0, temporary_result.cpu_data_for_write().data());
      parallel_for(this->domain().data_size, [&](const int2 texel) {
        result.store_pixel(texel, temporary_result.load_pixel<int4>(texel).xyz());
      });
      temporary_result.release();
      break;
    }
    case ResultType::Float3: {
      if (this->channels_count() == 3) {
        GPU_texture_read(
            *this, this->get_gpu_data_format(), 0, result.cpu_data_for_write().data());
        break;
      }
      /* Float3 is stored as a Float4 on GPU due to hardware limitations, so read to a Float4
       * result before copying to result. */
      Result temporary_result = Result(*context_, ResultType::Float4, this->precision());
      temporary_result.allocate_texture(this->domain(), false, ResultStorageType::CPU);
      GPU_texture_read(
          *this, this->get_gpu_data_format(), 0, temporary_result.cpu_data_for_write().data());
      parallel_for(this->domain().data_size, [&](const int2 texel) {
        result.store_pixel(texel, temporary_result.load_pixel<float4>(texel).xyz());
      });
      temporary_result.release();
      break;
    }
    case ResultType::Float4x4: {
      /* Float4x4 is stored in a 4-layer array texture, with each layer storing a column, so read
       * to a result with 4 times the height, each slice storing a column, before finally copying
       * and constructing the float4x4 in the result. */
      const int2 size = this->domain().data_size;
      Result temporary_result = Result(*context_, ResultType::Float4, this->precision());
      temporary_result.allocate_texture(size * int2(1, 4), false, ResultStorageType::CPU);
      GPU_texture_read(
          *this, this->get_gpu_data_format(), 0, temporary_result.cpu_data_for_write().data());
      parallel_for(this->domain().data_size, [&](const int2 texel) {
        result.store_pixel(
            texel,
            float4x4(temporary_result.load_pixel<float4>(texel + int2(0, size.y * 0)),
                     temporary_result.load_pixel<float4>(texel + int2(0, size.y * 1)),
                     temporary_result.load_pixel<float4>(texel + int2(0, size.y * 2)),
                     temporary_result.load_pixel<float4>(texel + int2(0, size.y * 3))));
      });
      temporary_result.release();
      break;
    }
    case ResultType::String:
    case ResultType::Object:
    case ResultType::Image:
    case ResultType::Font:
    case ResultType::Scene:
    case ResultType::Text:
    case ResultType::Mask:
      /* Single only types do not support GPU. */
      break;
  }

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

  if (sharing_info_) {
    sharing_info_->add_user();
  }

  /* Derived resources can't be shared, so reset them. */
  derived_resources_ = nullptr;
}

/* Returns true if the given GPU texture is compatible with the type and precision of the given
 * result. */
[[maybe_unused]] static bool is_compatible_texture(const gpu::Texture *texture,
                                                   const Result &result)
{
  /* Float3 and Int3 types are an exception, see the documentation on the get_gpu_texture_format
   * method for more information. */
  if (result.type() == ResultType::Float3) {
    if (GPU_texture_format(texture) ==
        Result::gpu_texture_format(gpu::TextureFormat::SFLOAT_32_32_32, result.precision()))
    {
      return true;
    }
  }
  else if (result.type() == ResultType::Int3) {
    if (GPU_texture_format(texture) ==
        Result::gpu_texture_format(gpu::TextureFormat::SINT_32_32_32, result.precision()))
    {
      return true;
    }
  }

  return GPU_texture_format(texture) == result.get_gpu_texture_format();
}

void Result::share_data(gpu::Texture *texture, ImplicitSharingInfo *sharing_info)
{
  BLI_assert(is_compatible_texture(texture, *this));
  BLI_assert(!this->is_allocated());

  gpu_texture_ = texture;
  storage_type_ = ResultStorageType::GPU;
  is_single_value_ = false;
  domain_ = Domain(int2(GPU_texture_width(texture), GPU_texture_height(texture)));
  sharing_info_ = sharing_info;
  if (sharing_info) {
    sharing_info_->add_user();
  }
}

void Result::share_data(const void *data, const int2 size, ImplicitSharingInfo *sharing_info)
{
  BLI_assert(!this->is_allocated());

  const int64_t array_size = int64_t(size.x) * int64_t(size.y);
  cpu_data_ = GSpan(this->get_cpp_type(), data, array_size);
  storage_type_ = ResultStorageType::CPU;
  domain_ = Domain(size);
  sharing_info_ = sharing_info;
  if (sharing_info) {
    sharing_info_->add_user();
  }
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
  if (!this->is_allocated()) {
    return;
  }

  delete derived_resources_;
  derived_resources_ = nullptr;

  if (sharing_info_) {
    sharing_info_->remove_user_and_delete_if_last();
  }
  sharing_info_ = nullptr;
  switch (storage_type_) {
    case ResultStorageType::GPU:
      gpu_texture_ = nullptr;
      break;
    case ResultStorageType::CPU:
      cpu_data_ = GSpan();
      break;
  }
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

int64_t Result::channels_count() const
{
  if (storage_type_ == ResultStorageType::GPU) {
    return GPU_texture_component_len(GPU_texture_format(this->gpu_texture()));
  }

  switch (type_) {
    case ResultType::Float:
    case ResultType::Int:
    case ResultType::Bool:
    case ResultType::Menu:
      return 1;
    case ResultType::Float2:
    case ResultType::Int2:
      return 2;
    case ResultType::Float3:
    case ResultType::Int3:
      return 3;
    case ResultType::Color:
    case ResultType::Float4:
    case ResultType::Int4:
    case ResultType::Quaternion:
      return 4;
    case ResultType::Float4x4:
      return 16;
    case ResultType::String:
    case ResultType::Object:
    case ResultType::Image:
    case ResultType::Font:
    case ResultType::Scene:
    case ResultType::Text:
    case ResultType::Mask:
      /* Single only types do not have channels. */
      BLI_assert(Result::is_single_value_only_type(type_));
      BLI_assert_unreachable();
      break;
  }

  BLI_assert_unreachable();
  return 4;
}

int64_t Result::size_in_bytes() const
{
  const int64_t pixel_size = this->get_cpp_type().size;
  if (this->is_single_value()) {
    return pixel_size;
  }
  const int2 image_size = this->domain().data_size;
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
        case ResultType::Int4:
        case ResultType::Bool:
        case ResultType::Menu:
        case ResultType::Quaternion:
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
        case ResultType::Int3: {
          /* Int3 results are stored in 4-component textures due to hardware limitations. So
           * pad the value with a zero before updating. */
          const int4 vector_value = int4(this->get_single_value<int3>(), 0);
          GPU_texture_update(this->gpu_texture(), GPU_DATA_INT, vector_value);
          break;
        }
        case ResultType::Float4x4: {
          /* Float4x4 stores each column in one texture layer in a 4-layer texture. */
          GPU_texture_update_sub(
              this->gpu_texture(), GPU_DATA_FLOAT, this->single_value().get(), 0, 0, 0, 1, 1, 4);
          break;
        }
        case ResultType::String:
        case ResultType::Object:
        case ResultType::Image:
        case ResultType::Font:
        case ResultType::Scene:
        case ResultType::Text:
        case ResultType::Mask:
          /* Single only types do not support GPU storage. */
          BLI_assert(Result::is_single_value_only_type(this->type()));
          BLI_assert_unreachable();
          break;
      }
      break;
    case ResultStorageType::CPU:
      this->get_cpp_type().copy_assign(this->single_value().get(),
                                       this->cpu_data_for_write().data());
      break;
  }
}

/* A RAII structure that makes it easier to manage the different ways of allocating and freeing
 * GPU textures. */
class GPUData {
 public:
  /* The allocated texture. */
  gpu::Texture *texture;

 private:
  /* If true, the GPU texture was allocated from the texture pool of the context and should be
   * released back into the pool instead of being freed. */
  const bool is_from_pool_;

 public:
  GPUData(const int2 size,
          const ResultType type,
          const ResultPrecision precision,
          const bool is_from_pool)
      : is_from_pool_(type == ResultType::Float4x4 ? false : is_from_pool)
  {
    const gpu::TextureFormat format = Result::gpu_texture_format(type, precision);
    const eGPUTextureUsage usage = GPU_TEXTURE_USAGE_GENERAL;
    if (type == ResultType::Float4x4) {
      this->texture = GPU_texture_create_2d_array(
          __func__, size.x, size.y, 4, 1, format, usage, nullptr);
    }
    else if (is_from_pool) {
      this->texture = gpu::TexturePool::get().acquire_texture(size, format, usage);
    }
    else {
      this->texture = GPU_texture_create_2d(__func__, size.x, size.y, 1, format, usage, nullptr);
    }
  }

  ~GPUData()
  {
    if (is_from_pool_) {
      gpu::TexturePool::get().release_texture(this->texture);
    }
    else {
      GPU_texture_free(this->texture);
    }
  }
};

void Result::allocate_data(const int2 size,
                           const bool from_pool,
                           const std::optional<ResultStorageType> storage_type)
{
  BLI_assert(!this->is_allocated());

  const bool use_gpu = storage_type.has_value() ? storage_type.value() == ResultStorageType::GPU :
                                                  context_->use_gpu();
  if (use_gpu) {
    storage_type_ = ResultStorageType::GPU;
    auto *new_texture = new ImplicitSharedValue<GPUData>(
        size, this->type(), this->precision(), from_pool);
    sharing_info_ = new_texture;
    gpu_texture_ = new_texture->data.texture;
  }
  else {
    storage_type_ = ResultStorageType::CPU;
    const int64_t array_size = int64_t(size.x) * int64_t(size.y);
    auto *new_array = new ImplicitSharedValue<GArray<>>(this->get_cpp_type(), array_size);
    sharing_info_ = new_array;
    cpu_data_ = new_array->data.as_span();
  }
}

}  // namespace blender::compositor
