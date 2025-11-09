/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "BLI_assert.h"
#include "BLI_color_types.hh"
#include "BLI_compiler_compat.h"
#include "BLI_cpp_type.hh"
#include "BLI_generic_pointer.hh"
#include "BLI_generic_span.hh"
#include "BLI_math_interp.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_memory_utils.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "NOD_menu_value.hh"

#include "COM_domain.hh"
#include "COM_meta_data.hh"

namespace blender::compositor {

class Context;
class DerivedResources;

/* Make sure to update the format related static methods in the Result class. */
enum class ResultType : uint8_t {
  Float,
  Float2,
  Float3,
  Float4,
  Color,
  Int,
  Int2,
  Bool,
  Menu,

  /* Single value only types. See Result::is_single_value_only_type. */
  String,
};

/* The precision of the data. CPU data is always stored using full precision at the moment. */
enum class ResultPrecision : uint8_t {
  Full,
  Half,
};

/* The type of storage used to hold the result data. */
enum class ResultStorageType : uint8_t {
  /* Stored as a blender::gpu::Texture on the GPU. */
  GPU,
  /* Stored as a buffer on the CPU and wrapped in a GMutableSpan. */
  CPU,
};

using Color = ColorSceneLinear4f<eAlpha::Premultiplied>;

/* ------------------------------------------------------------------------------------------------
 * Result
 *
 * A result represents the computed value of an output of an operation. A result can either
 * represent an image or a single value. A result is typed, and can be of types like color, vector,
 * or float. Single value results are stored in 1x1 textures to make them easily accessible in
 * shaders. But the same value is also stored in the value union member of the result for any
 * host-side processing. The GPU texture of the result can either be allocated from the texture
 * pool of the context referenced by the result or it can be allocated directly from the GPU
 * module, see the allocation method for more information.
 *
 * Results are reference counted and their data are released once their reference count reaches
 * zero. After constructing a result, the set_reference_count method is called to declare the
 * number of operations that needs this result. Once each operation that needs the result no longer
 * needs it, the release method is called and the reference count is decremented, until it reaches
 * zero, where the result's data is then released.
 *
 * A result not only represents an image, but also the area it occupies in the virtual compositing
 * space. This area is called the Domain of the result, see the discussion in COM_domain.hh for
 * more information.
 *
 * Allocated data of results can be shared by multiple results, this is achieved by tracking an
 * extra reference count for data data_reference_count_, which is heap allocated along with the
 * data, and shared by all results that share the same data. This reference count is incremented
 * every time the data is shared by a call to the share_data method, and decremented during
 * freeing, where the data is only freed if the reference count is 1, that is, no longer shared.
 *
 * A result can wrap external data that is not allocated nor managed by the result. This is set up
 * by a call to the wrap_external method. In that case, when the reference count eventually reach
 * zero, the data will not be freed.
 *
 * A result may store resources that are computed and cached in case they are needed by multiple
 * operations. Those are called Derived Resources and can be accessed using the derived_resources
 * method. */
class Result {
 private:
  /* The context that the result was created within, this should be initialized during
   * construction. */
  Context *context_ = nullptr;
  /* The base type of the result's image or single value. */
  ResultType type_ = ResultType::Float;
  /* The precision of the result's data. Only relevant for GPU textures. CPU buffers and single
   * values are always stored using full precision. */
  ResultPrecision precision_ = ResultPrecision::Half;
  /* If true, the result is a single value, otherwise, the result is an image. */
  bool is_single_value_ = false;
  /* The type of storage used to hold the data. Used to correctly interpret the data union. */
  ResultStorageType storage_type_ = ResultStorageType::GPU;
  /* Stores the result's pixel data, either stored in a GPU texture or a buffer that is wrapped in
   * a GMutableSpan on CPU. This will represent a 1x1 image if the result is a single value, the
   * value of which will be identical to that of the value member. See class description for more
   * information. */
  union {
    blender::gpu::Texture *gpu_texture_ = nullptr;
    GMutableSpan cpu_data_;
  };
  /* The number of users that currently needs this result. Operations initializes this by calling
   * the set_reference_count method before evaluation. Once each operation that needs the result no
   * longer needs it, the release method is called and the reference count is decremented, until it
   * reaches zero, where the result's data is then released. */
  int reference_count_ = 1;
  /* Allocated result data can be shared by multiple results by calling the share_data method. This
   * member stores the number of results that share the data. This is heap allocated and have the
   * same lifetime as allocated data, that's because this reference count is shared by all results
   * that share the same data. Unlike the result's reference count, the data is freed if the count
   * becomes 1, that is, data is no longer shared with some other result. This is nullptr if the
   * data is external. */
  int *data_reference_count_ = nullptr;
  /* If the result is a single value, this member stores the value of the result, the value of
   * which will be identical to that stored in the data_ member. The active variant member depends
   * on the type of the result. This member is uninitialized and should not be used if the result
   * is not a single value. */
  std::variant<float,
               float2,
               float3,
               float4,
               Color,
               int32_t,
               int2,
               bool,
               nodes::MenuValue,
               std::string>
      single_value_ = 0.0f;
  /* The domain of the result. This only matters if the result was not a single value. See the
   * discussion in COM_domain.hh for more information. */
  Domain domain_ = Domain::identity();
  /* If true, then the result wraps external data that is not allocated nor managed by the result.
   * This is set up by a call to the wrap_external method. In that case, when the reference count
   * eventually reach zero, the data will not be freed. */
  bool is_external_ = false;
  /* If true, the GPU texture that holds the data was allocated from the texture pool of the
   * context and should be released back into the pool instead of being freed. For CPU storage,
   * this is irrelevant. */
  bool is_from_pool_ = false;
  /* Stores resources that are derived from this result. Lazily allocated if needed. See the class
   * description for more information. */
  DerivedResources *derived_resources_ = nullptr;

 public:
  /* Stores extra information about the result such as image meta data that can eventually be
   * written to file. */
  MetaData meta_data;

  /* Construct a result within the given context. */
  Result(Context &context);

  /* Construct a result of the given type and precision within the given context. */
  Result(Context &context, ResultType type, ResultPrecision precision);

  /* Construct a result of an appropriate type and precision based on the given GPU texture format
   * within the given context. */
  Result(Context &context, blender::gpu::TextureFormat format);

  /* Returns true if the given type can only be used with single value results. Consequently, it is
   * always allocated on the CPU and GPU code paths needn't support the type. */
  static bool is_single_value_only_type(ResultType type);

  /* Returns the appropriate GPU texture format based on the given result type and precision. A
   * special case is given to ResultType::Float3, because 3-component textures can't be used as
   * write targets in shaders, so we need to allocate 4-component textures for them, and ignore the
   * fourth channel during processing. */
  static blender::gpu::TextureFormat gpu_texture_format(ResultType type,
                                                        ResultPrecision precision);

  /* Returns the GPU data format that corresponds to the give result type. */
  static eGPUDataFormat gpu_data_format(const ResultType type);

  /* Returns the GPU texture format that corresponds to the give one, but whose precision is the
   * given precision. */
  static blender::gpu::TextureFormat gpu_texture_format(blender::gpu::TextureFormat format,
                                                        ResultPrecision precision);

  /* Returns the precision of the given GPU texture format. */
  static ResultPrecision precision(blender::gpu::TextureFormat format);

  /* Returns the type of the given GPU texture format. */
  static ResultType type(blender::gpu::TextureFormat format);

  /* Returns the float type of the result given the channels count. */
  static ResultType float_type(const int channels_count);

  /* Returns the CPP type corresponding to the given result type. */
  static const CPPType &cpp_type(const ResultType type);

  /* Returns a string representation of the given result type. */
  static const char *type_name(const ResultType type);

  /* Implicit conversion to the internal GPU texture. */
  operator blender::gpu::Texture *() const;

  /* Returns the CPP type of the result. */
  const CPPType &get_cpp_type() const;

  /* Returns the appropriate texture format based on the result's type and precision. This is
   * identical to the gpu_texture_format static method. This will match the format of the allocated
   * texture, with one exception. Results of type ResultType::Float3 that wrap external textures
   * might hold a 3-component texture as opposed to a 4-component one, which would have been
   * created by uploading data from CPU. */
  blender::gpu::TextureFormat get_gpu_texture_format() const;

  /* Identical to gpu_data_format but assumes the result's type. */
  eGPUDataFormat get_gpu_data_format() const;

  /* Declare the result to be a texture result, allocate a texture of an appropriate type with
   * the size of the given domain, and set the domain of the result to the given domain.
   *
   * See the allocate_data method for more information on the from_pool and storage_type
   * parameters. */
  void allocate_texture(const Domain domain,
                        const bool from_pool = true,
                        const std::optional<ResultStorageType> storage_type = std::nullopt);

  /* Declare the result to be a single value result, allocate a texture of an appropriate type with
   * size 1x1 from the texture pool, and set the domain to be an identity domain. The value is zero
   * initialized. See class description for more information. */
  void allocate_single_value();

  /* Allocate a single value result whose value is zero. This is called for results whose value
   * can't be computed and are considered invalid. */
  void allocate_invalid();

  /* Creates and allocates a new result that matches the type and precision of this result and
   * uploads the CPU data that exist in this result. The result is assumed to be allocated on the
   * CPU. See the allocate_data method for more information on the from_pool parameters. */
  Result upload_to_gpu(const bool from_pool) const;

  /* Creates and allocates a new result that matches the type and precision of this result and
   * downloads the GPU data that exist in this result. The result is assumed to be allocated on the
   * GPU. */
  Result download_to_cpu() const;

  /* Bind the GPU texture of the result to the texture image unit with the given name in the
   * currently bound given shader. This also inserts a memory barrier for texture fetches to ensure
   * any prior writes to the texture are reflected before reading from it. */
  void bind_as_texture(gpu::Shader *shader, const char *texture_name) const;

  /* Bind the GPU texture of the result to the image unit with the given name in the currently
   * bound given shader. If read is true, a memory barrier will be inserted for image reads to
   * ensure any prior writes to the images are reflected before reading from it. */
  void bind_as_image(gpu::Shader *shader, const char *image_name, bool read = false) const;

  /* Unbind the GPU texture which was previously bound using bind_as_texture. */
  void unbind_as_texture() const;

  /* Unbind the GPU texture which was previously bound using bind_as_image. */
  void unbind_as_image() const;

  /* Share the data of the given source result. For a source that wraps external results, this just
   * shallow copies the data since it can be transparency shared. Otherwise, the data is also
   * shallow copied and the data_reference_count_ is incremented to denote sharing. The source data
   * is expect to be allocated and have the same type and precision as this result. */
  void share_data(const Result &source);

  /* Steal the allocated data from the given source result and assign it to this result, then
   * remove any references to the data from the source result. It is assumed that:
   *
   *   - Both results are of the same type.
   *   - This result is not allocated but the source result is allocated.
   *
   * This is most useful in multi-step compositor operations where some steps can be optional, in
   * that case, intermediate results can be temporary results that can eventually be stolen by the
   * actual output of the operation. See the uses of the method for a practical example of use. */
  void steal_data(Result &source);

  /* Similar to the Result variant of steal_data, but steals from a raw data buffer. The buffer is
   * assumed to be allocated using Blender's guarded allocator. */
  void steal_data(void *data, int2 size);

  /* Set up the result to wrap an external GPU texture that is not allocated nor managed by the
   * result. The is_external_ member will be set to true, the domain will be set to have the same
   * size as the texture, and the texture will be set to the given texture. See the is_external_
   * member for more information. The given texture should have the same format as the result and
   * is assumed to have a lifetime that covers the evaluation of the compositor. */
  void wrap_external(blender::gpu::Texture *texture);

  /* Identical to GPU variant of wrap_external but wraps a CPU buffer instead. */
  void wrap_external(void *data, int2 size);

  /* Identical to GPU variant of wrap_external but wraps whatever the given result has instead. */
  void wrap_external(const Result &result);

  /* Sets the transformation of the domain of the result to the given transformation. */
  void set_transformation(const float3x3 &transformation);

  /* Transform the result by the given transformation. This effectively pre-multiply the given
   * transformation by the current transformation of the domain of the result. */
  void transform(const float3x3 &transformation);

  /* Get a reference to the realization options of this result. See the RealizationOptions struct
   * for more information. */
  RealizationOptions &get_realization_options();
  const RealizationOptions &get_realization_options() const;

  /* Set the value of reference_count_, see that member for more details. This should be called
   * after constructing the result to declare the number of operations that needs it. */
  void set_reference_count(int count);

  /* Increment the reference count of the result by the given count. */
  void increment_reference_count(int count = 1);

  /* Decrement the reference count of the result by the given count. */
  void decrement_reference_count(int count = 1);

  /* Decrement the reference count of the result and free its data if it reaches zero. */
  void release();

  /* Frees the result data. If the result is not allocated, wraps external data, or shares data
   * with some other result, then this does nothing. */
  void free();

  /* Returns true if this result should be computed and false otherwise. The result should be
   * computed if its reference count is not zero, that is, its result is used by at least one
   * operation. */
  bool should_compute();

  /* Returns a reference to the derived resources of the result, which is allocated if it was not
   * allocated already. */
  DerivedResources &derived_resources();

  /* Returns the type of the result. */
  ResultType type() const;

  /* Returns the precision of the result. */
  ResultPrecision precision() const;

  /* Sets the type of the result. */
  void set_type(ResultType type);

  /* Sets the precision of the result. */
  void set_precision(ResultPrecision precision);

  /* Returns true if the result is a single value and false of it is an image. */
  bool is_single_value() const;

  /* Returns true if the result is allocated. */
  bool is_allocated() const;

  /* Returns the reference count of the result. */
  int reference_count() const;

  /* Returns a reference to the domain of the result. See the Domain class. */
  const Domain &domain() const;

  /* Computes the number of channels of the result based on its type. */
  int64_t channels_count() const;

  /* Computes the size of the result's data in bytes. */
  int64_t size_in_bytes() const;

  blender::gpu::Texture *gpu_texture() const;

  GSpan cpu_data() const;
  GMutableSpan cpu_data();

  /* It is important to call update_single_value_data after adjusting the single value. See that
   * method for more information. */
  GPointer single_value() const;
  GMutablePointer single_value();

  /* Gets the single value stored in the result. Assumes the result stores a value of the given
   * template type. */
  template<typename T> const T &get_single_value() const;

  /* Gets the single value stored in the result, if the result is not a single value, the given
   * default value is returned. Assumes the result stores a value of the same type as the template
   * type. */
  template<typename T> T get_single_value_default(const T &default_value) const;

  /* Sets the single value of the result to the given value, which also involves setting the single
   * pixel in the image to that value. See the class description for more information. Assumes
   * the result stores a value of the given template type. */
  template<typename T> void set_single_value(const T &value);

  /* Updates the single pixel in the image to the current single value in the result. This is
   * called implicitly in the set_single_value method, but calling this explicitly is useful when
   * the single value was adjusted through its data pointer returned by the single_value method.
   * See the class description for more information. */
  void update_single_value_data();

  /* Loads the pixel at the given texel coordinates. Assumes the result stores a value of the given
   * template type. If the CouldBeSingleValue template argument is true and the result is a single
   * value result, then that single value is returned for all texel coordinates. */
  template<typename T, bool CouldBeSingleValue = false> T load_pixel(const int2 &texel) const;

  /* Identical to load_pixel but with extended boundary condition. */
  template<typename T, bool CouldBeSingleValue = false>
  T load_pixel_extended(const int2 &texel) const;

  /* Identical to load_pixel but with a fallback value for out of bound access. */
  template<typename T, bool CouldBeSingleValue = false>
  T load_pixel_fallback(const int2 &texel, const T &fallback) const;

  /* Identical to load_pixel but with zero boundary condition. */
  template<typename T, bool CouldBeSingleValue = false> T load_pixel_zero(const int2 &texel) const;

  /* Similar to load_pixel, but can load a result whose type is not known at compile time. If the
   * number of channels in the result are less than 4, then the rest of the returned float4 will
   * have its vales initialized as follows: float4(0, 0, 0, 1). This is similar to how the
   * texelFetch function in GLSL works. */
  float4 load_pixel_generic_type(const int2 &texel) const;

  /* Stores the given pixel value in the pixel at the given texel coordinates. Assumes the result
   * stores a value of the given template type. */
  template<typename T> void store_pixel(const int2 &texel, const T &pixel_value);

  /* Similar to store_pixel, but can write to a result whose types is not known at compile time.
   * While a float4 is given, only the number of channels of the result will be written, while the
   * rest of the float4 will be ignored. This is similar to how the imageStore function in GLSL
   * works. */
  void store_pixel_generic_type(const int2 &texel, const float4 &pixel_value);

  /* Samples the result at the given normalized coordinates with the given interpolation and
   * boundary conditions. The interpolation is ignored for non float types that do not support
   * interpolation. Assumes the result stores a value of the given template type. If the
   * CouldBeSingleValue template argument is true and the result is a single value result, then
   * that single value is returned for all coordinates. */
  template<typename T, bool CouldBeSingleValue = false>
  T sample(const float2 &coordinates,
           const Interpolation &interpolation,
           const ExtensionMode &extend_mode_x,
           const ExtensionMode &extend_mode_y) const;

  /* Equivalent to the GLSL texture() function with nearest interpolation and zero boundary
   * condition. The coordinates are thus expected to have half-pixels offsets. A float4 is always
   * returned regardless of the number of channels of the buffer, the remaining channels will be
   * initialized with the template float4(0, 0, 0, 1). */
  float4 sample_nearest_zero(const float2 &coordinates) const;

  /* Identical to sample_nearest_zero but with bilinear interpolation. */
  float4 sample_bilinear_zero(const float2 &coordinates) const;

  /* Identical to sample_nearest_zero but with extended boundary condition. */
  float4 sample_nearest_extended(const float2 &coordinates) const;

  /* Identical to sample_nearest_extended but with bilinear interpolation. */
  float4 sample_bilinear_extended(const float2 &coordinates) const;

  /* Identical to sample_nearest_extended but with cubic interpolation. */
  float4 sample_cubic_extended(const float2 &coordinates) const;

  float4 sample_nearest_wrap(const float2 &coordinates, bool wrap_x, bool wrap_y) const;
  float4 sample_bilinear_wrap(const float2 &coordinates, bool wrap_x, bool wrap_y) const;
  float4 sample_cubic_wrap(const float2 &coordinates, bool wrap_x, bool wrap_y) const;

  /* Equivalent to the GLSL textureGrad() function with EWA filtering and extended boundary
   * condition. Note that extended boundaries only cover areas touched by the ellipses whose
   * center is inside the image, other areas will be zero. The coordinates are thus expected to
   * have half-pixels offsets. Only supports ResultType::Color. */
  float4 sample_ewa_extended(const float2 &coordinates,
                             const float2 &x_gradient,
                             const float2 &y_gradient) const;

  /* Identical to sample_ewa_extended but with zero boundary condition. */
  float4 sample_ewa_zero(const float2 &coordinates,
                         const float2 &x_gradient,
                         const float2 &y_gradient) const;

 private:
  /* Allocates the image data for the given size.
   *
   * The data is allocated on the CPU or GPU depending on the given storage_type. A nullopt may be
   * passed to storage_type, in which case, the data will be allocated on the device of the
   * result's context as specified by context.use_gpu().
   *
   * If from_pool is true, GPU textures will be allocated from the texture pool of the context,
   * otherwise, a new texture will be allocated. Pooling should not be used for persistent results
   * that might span more than one evaluation, like cached resources. While pooling should be used
   * for most other cases where the result will be allocated then later released in the same
   * evaluation. */
  void allocate_data(const int2 size,
                     const bool from_pool = true,
                     const std::optional<ResultStorageType> storage_type = std::nullopt);

  /* Same as get_pixel_index but can be used when the type of the result is not known at compile
   * time. */
  int64_t get_pixel_index(const int2 &texel) const;
};

/* -------------------------------------------------------------------- */
/* Inline Methods.
 */

BLI_INLINE_METHOD const Domain &Result::domain() const
{
  return domain_;
}

BLI_INLINE_METHOD int64_t Result::channels_count() const
{
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
      return 3;
    case ResultType::Color:
    case ResultType::Float4:
      return 4;
    case ResultType::String:
      /* Single only types do not have channels. */
      BLI_assert(Result::is_single_value_only_type(type_));
      BLI_assert_unreachable();
      break;
  }

  BLI_assert_unreachable();
  return 4;
}

BLI_INLINE_METHOD blender::gpu::Texture *Result::gpu_texture() const
{
  BLI_assert(storage_type_ == ResultStorageType::GPU);
  return gpu_texture_;
}

BLI_INLINE_METHOD GSpan Result::cpu_data() const
{
  BLI_assert(storage_type_ == ResultStorageType::CPU);
  return cpu_data_;
}

BLI_INLINE_METHOD GMutableSpan Result::cpu_data()
{
  BLI_assert(storage_type_ == ResultStorageType::CPU);
  return cpu_data_;
}

template<typename T> BLI_INLINE_METHOD const T &Result::get_single_value() const
{
  BLI_assert(this->is_single_value());

  return std::get<T>(single_value_);
}

template<typename T>
BLI_INLINE_METHOD T Result::get_single_value_default(const T &default_value) const
{
  if (this->is_single_value()) {
    return this->get_single_value<T>();
  }
  return default_value;
}

template<typename T> BLI_INLINE_METHOD void Result::set_single_value(const T &value)
{
  BLI_assert(this->is_allocated());
  BLI_assert(this->is_single_value());

  single_value_ = value;
  this->update_single_value_data();
}

template<typename T, bool CouldBeSingleValue>
BLI_INLINE_METHOD T Result::load_pixel(const int2 &texel) const
{
  if constexpr (CouldBeSingleValue) {
    if (is_single_value_) {
      return this->get_single_value<T>();
    }
  }
  else {
    BLI_assert(!this->is_single_value());
  }

  return this->cpu_data().typed<T>()[this->get_pixel_index(texel)];
}

template<typename T, bool CouldBeSingleValue>
BLI_INLINE_METHOD T Result::load_pixel_extended(const int2 &texel) const
{
  if constexpr (CouldBeSingleValue) {
    if (is_single_value_) {
      return this->get_single_value<T>();
    }
  }
  else {
    BLI_assert(!this->is_single_value());
  }

  const int2 clamped_texel = math::clamp(texel, int2(0), domain_.size - int2(1));
  return this->cpu_data().typed<T>()[this->get_pixel_index(clamped_texel)];
}

template<typename T, bool CouldBeSingleValue>
BLI_INLINE_METHOD T Result::load_pixel_fallback(const int2 &texel, const T &fallback) const
{
  if constexpr (CouldBeSingleValue) {
    if (is_single_value_) {
      return this->get_single_value<T>();
    }
  }
  else {
    BLI_assert(!this->is_single_value());
  }

  if (texel.x < 0 || texel.y < 0 || texel.x >= domain_.size.x || texel.y >= domain_.size.y) {
    return fallback;
  }

  return this->cpu_data().typed<T>()[this->get_pixel_index(texel)];
}

template<typename T, bool CouldBeSingleValue>
BLI_INLINE_METHOD T Result::load_pixel_zero(const int2 &texel) const
{
  return this->load_pixel_fallback<T, CouldBeSingleValue>(texel, T(0));
}

BLI_INLINE_METHOD float4 Result::load_pixel_generic_type(const int2 &texel) const
{
  float4 pixel_value = float4(0.0f, 0.0f, 0.0f, 1.0f);
  if (is_single_value_) {
    this->get_cpp_type().copy_assign(this->cpu_data().data(), pixel_value);
  }
  else {
    this->get_cpp_type().copy_assign(this->cpu_data()[this->get_pixel_index(texel)], pixel_value);
  }
  return pixel_value;
}

template<typename T>
BLI_INLINE_METHOD void Result::store_pixel(const int2 &texel, const T &pixel_value)
{
  this->cpu_data().typed<T>()[this->get_pixel_index(texel)] = pixel_value;
}

BLI_INLINE_METHOD void Result::store_pixel_generic_type(const int2 &texel,
                                                        const float4 &pixel_value)
{
  this->get_cpp_type().copy_assign(pixel_value, this->cpu_data()[this->get_pixel_index(texel)]);
}

BLI_INLINE int wrap_coordinates(float coordinates, int size, const ExtensionMode extension_mode)
{
  switch (extension_mode) {
    case ExtensionMode::Extend:
      return math::clamp(int(coordinates), 0, size - 1);
    case ExtensionMode::Repeat:
      return int(math::floored_mod(coordinates, float(size)));
    case ExtensionMode::Clip:
      if (coordinates < 0.0f || coordinates >= size) {
        return -1;
      }
      return int(coordinates);
  }

  BLI_assert_unreachable();
  return 0;
}

template<typename T, bool CouldBeSingleValue>
BLI_INLINE_METHOD T Result::sample(const float2 &coordinates,
                                   const Interpolation &interpolation,
                                   const ExtensionMode &mode_x,
                                   const ExtensionMode &mode_y) const
{
  if constexpr (CouldBeSingleValue) {
    if (is_single_value_) {
      return this->get_single_value<T>();
    }
  }

  const int2 size = domain_.size;
  const float2 texel_coordinates = coordinates * float2(size);

  if constexpr (is_same_any_v<T, float, float2, float3, float4, Color>) {
    const math::InterpWrapMode extension_mode_x = map_extension_mode_to_wrap_mode(mode_x);
    const math::InterpWrapMode extension_mode_y = map_extension_mode_to_wrap_mode(mode_y);

    T pixel_value = T(0);
    const float *buffer = static_cast<const float *>(this->cpu_data().data());
    float *output = nullptr;
    if constexpr (std::is_same_v<T, float>) {
      output = &pixel_value;
    }
    else {
      output = pixel_value;
    }

    switch (interpolation) {
      case Interpolation::Nearest:
        math::interpolate_nearest_wrapmode_fl(buffer,
                                              output,
                                              size.x,
                                              size.y,
                                              this->channels_count(),
                                              texel_coordinates.x,
                                              texel_coordinates.y,
                                              extension_mode_x,
                                              extension_mode_y);
        break;
      case Interpolation::Bilinear:
        math::interpolate_bilinear_wrapmode_fl(buffer,
                                               output,
                                               size.x,
                                               size.y,
                                               this->channels_count(),
                                               texel_coordinates.x - 0.5f,
                                               texel_coordinates.y - 0.5f,
                                               extension_mode_x,
                                               extension_mode_y);
        break;
      case Interpolation::Bicubic:
      case Interpolation::Anisotropic:
        math::interpolate_cubic_bspline_wrapmode_fl(buffer,
                                                    output,
                                                    size.x,
                                                    size.y,
                                                    this->channels_count(),
                                                    texel_coordinates.x - 0.5f,
                                                    texel_coordinates.y - 0.5f,
                                                    extension_mode_x,
                                                    extension_mode_y);
        break;
    }

    return pixel_value;
  }
  else {
    /* Non float types do not support interpolations and are always sampled in nearest. */
    const int x = wrap_coordinates(texel_coordinates.x, size.x, mode_x);
    const int y = wrap_coordinates(texel_coordinates.y, size.y, mode_y);
    if (x < 0 || y < 0) {
      return T(0);
    }
    return this->load_pixel<T>(int2(x, y));
  }
}

BLI_INLINE_METHOD float4 Result::sample_nearest_zero(const float2 &coordinates) const
{
  float4 pixel_value = float4(0.0f, 0.0f, 0.0f, 1.0f);
  if (is_single_value_) {
    this->get_cpp_type().copy_assign(this->cpu_data().data(), pixel_value);
    return pixel_value;
  }

  const int2 size = domain_.size;
  const float2 texel_coordinates = coordinates * float2(size);

  const float *buffer = static_cast<const float *>(this->cpu_data().data());
  math::interpolate_nearest_border_fl(buffer,
                                      pixel_value,
                                      size.x,
                                      size.y,
                                      this->channels_count(),
                                      texel_coordinates.x,
                                      texel_coordinates.y);
  return pixel_value;
}

BLI_INLINE_METHOD float4 Result::sample_nearest_wrap(const float2 &coordinates,
                                                     bool wrap_x,
                                                     bool wrap_y) const
{
  float4 pixel_value = float4(0.0f, 0.0f, 0.0f, 1.0f);
  if (is_single_value_) {
    this->get_cpp_type().copy_assign(this->cpu_data().data(), pixel_value);
    return pixel_value;
  }

  const int2 size = domain_.size;
  const float2 texel_coordinates = coordinates * float2(size);

  const float *buffer = static_cast<const float *>(this->cpu_data().data());
  math::interpolate_nearest_wrapmode_fl(
      buffer,
      pixel_value,
      size.x,
      size.y,
      this->channels_count(),
      texel_coordinates.x,
      texel_coordinates.y,
      wrap_x ? math::InterpWrapMode::Repeat : math::InterpWrapMode::Border,
      wrap_y ? math::InterpWrapMode::Repeat : math::InterpWrapMode::Border);
  return pixel_value;
}

BLI_INLINE_METHOD float4 Result::sample_bilinear_wrap(const float2 &coordinates,
                                                      bool wrap_x,
                                                      bool wrap_y) const
{
  float4 pixel_value = float4(0.0f, 0.0f, 0.0f, 1.0f);
  if (is_single_value_) {
    this->get_cpp_type().copy_assign(this->cpu_data().data(), pixel_value);
    return pixel_value;
  }

  const int2 size = domain_.size;
  const float2 texel_coordinates = coordinates * float2(size) - 0.5f;

  const float *buffer = static_cast<const float *>(this->cpu_data().data());
  math::interpolate_bilinear_wrapmode_fl(
      buffer,
      pixel_value,
      size.x,
      size.y,
      this->channels_count(),
      texel_coordinates.x,
      texel_coordinates.y,
      wrap_x ? math::InterpWrapMode::Repeat : math::InterpWrapMode::Border,
      wrap_y ? math::InterpWrapMode::Repeat : math::InterpWrapMode::Border);
  return pixel_value;
}

BLI_INLINE_METHOD float4 Result::sample_cubic_wrap(const float2 &coordinates,
                                                   bool wrap_x,
                                                   bool wrap_y) const
{
  float4 pixel_value = float4(0.0f, 0.0f, 0.0f, 1.0f);
  if (is_single_value_) {
    this->get_cpp_type().copy_assign(this->cpu_data().data(), pixel_value);
    return pixel_value;
  }

  const int2 size = domain_.size;
  const float2 texel_coordinates = coordinates * float2(size) - 0.5f;

  const float *buffer = static_cast<const float *>(this->cpu_data().data());
  math::interpolate_cubic_bspline_wrapmode_fl(
      buffer,
      pixel_value,
      size.x,
      size.y,
      this->channels_count(),
      texel_coordinates.x,
      texel_coordinates.y,
      wrap_x ? math::InterpWrapMode::Repeat : math::InterpWrapMode::Border,
      wrap_y ? math::InterpWrapMode::Repeat : math::InterpWrapMode::Border);
  return pixel_value;
}

BLI_INLINE_METHOD float4 Result::sample_bilinear_zero(const float2 &coordinates) const
{
  float4 pixel_value = float4(0.0f, 0.0f, 0.0f, 1.0f);
  if (is_single_value_) {
    this->get_cpp_type().copy_assign(this->cpu_data().data(), pixel_value);
    return pixel_value;
  }

  const int2 size = domain_.size;
  const float2 texel_coordinates = (coordinates * float2(size)) - 0.5f;

  const float *buffer = static_cast<const float *>(this->cpu_data().data());
  math::interpolate_bilinear_border_fl(buffer,
                                       pixel_value,
                                       size.x,
                                       size.y,
                                       this->channels_count(),
                                       texel_coordinates.x,
                                       texel_coordinates.y);
  return pixel_value;
}

BLI_INLINE_METHOD float4 Result::sample_nearest_extended(const float2 &coordinates) const
{
  float4 pixel_value = float4(0.0f, 0.0f, 0.0f, 1.0f);
  if (is_single_value_) {
    this->get_cpp_type().copy_assign(this->cpu_data().data(), pixel_value);
    return pixel_value;
  }

  const int2 size = domain_.size;
  const float2 texel_coordinates = coordinates * float2(size);

  const float *buffer = static_cast<const float *>(this->cpu_data().data());
  math::interpolate_nearest_fl(buffer,
                               pixel_value,
                               size.x,
                               size.y,
                               this->channels_count(),
                               texel_coordinates.x,
                               texel_coordinates.y);
  return pixel_value;
}

BLI_INLINE_METHOD float4 Result::sample_bilinear_extended(const float2 &coordinates) const
{
  float4 pixel_value = float4(0.0f, 0.0f, 0.0f, 1.0f);
  if (is_single_value_) {
    this->get_cpp_type().copy_assign(this->cpu_data().data(), pixel_value);
    return pixel_value;
  }

  const int2 size = domain_.size;
  const float2 texel_coordinates = (coordinates * float2(size)) - 0.5f;

  const float *buffer = static_cast<const float *>(this->cpu_data().data());
  math::interpolate_bilinear_fl(buffer,
                                pixel_value,
                                size.x,
                                size.y,
                                this->channels_count(),
                                texel_coordinates.x,
                                texel_coordinates.y);
  return pixel_value;
}

BLI_INLINE_METHOD float4 Result::sample_cubic_extended(const float2 &coordinates) const
{
  float4 pixel_value = float4(0.0f, 0.0f, 0.0f, 1.0f);
  if (is_single_value_) {
    this->get_cpp_type().copy_assign(this->cpu_data().data(), pixel_value);
    return pixel_value;
  }

  const int2 size = domain_.size;
  const float2 texel_coordinates = (coordinates * float2(size)) - 0.5f;

  const float *buffer = static_cast<const float *>(this->cpu_data().data());
  math::interpolate_cubic_bspline_fl(buffer,
                                     pixel_value,
                                     size.x,
                                     size.y,
                                     this->channels_count(),
                                     texel_coordinates.x,
                                     texel_coordinates.y);
  return pixel_value;
}

/**
 * Given a Result as the userdata argument, sample it at the given coordinates using extended
 * boundary condition and write the result to the result argument.
 */
static void sample_ewa_extended_read_callback(void *userdata, int x, int y, float result[4])
{
  const Result *input = static_cast<const Result *>(userdata);
  const Color sampled_result = input->load_pixel_extended<Color>(int2(x, y));
  copy_v4_v4(result, sampled_result);
}

BLI_INLINE_METHOD float4 Result::sample_ewa_extended(const float2 &coordinates,
                                                     const float2 &x_gradient,
                                                     const float2 &y_gradient) const
{
  BLI_assert(type_ == ResultType::Color);

  float4 pixel_value = float4(0.0f, 0.0f, 0.0f, 1.0f);
  if (is_single_value_) {
    this->get_cpp_type().copy_assign(this->cpu_data().data(), pixel_value);
    return pixel_value;
  }

  const int2 size = domain_.size;
  BLI_ewa_filter(size.x,
                 size.y,
                 false,
                 true,
                 coordinates,
                 x_gradient,
                 y_gradient,
                 sample_ewa_extended_read_callback,
                 const_cast<Result *>(this),
                 pixel_value);
  return pixel_value;
}

/**
 * Given a Result as the userdata argument, sample it at the given coordinates using zero boundary
 * condition and write the result to the result argument.
 */
static void sample_ewa_zero_read_callback(void *userdata, int x, int y, float result[4])
{
  const Result *input = static_cast<const Result *>(userdata);
  const Color sampled_result = input->load_pixel_zero<Color>(int2(x, y));
  copy_v4_v4(result, sampled_result);
}

BLI_INLINE_METHOD float4 Result::sample_ewa_zero(const float2 &coordinates,
                                                 const float2 &x_gradient,
                                                 const float2 &y_gradient) const
{
  BLI_assert(type_ == ResultType::Color);

  float4 pixel_value = float4(0.0f, 0.0f, 0.0f, 1.0f);
  if (is_single_value_) {
    this->get_cpp_type().copy_assign(this->cpu_data().data(), pixel_value);
    return pixel_value;
  }

  const int2 size = domain_.size;
  BLI_ewa_filter(size.x,
                 size.y,
                 false,
                 true,
                 coordinates,
                 x_gradient,
                 y_gradient,
                 sample_ewa_zero_read_callback,
                 const_cast<Result *>(this),
                 pixel_value);
  return pixel_value;
}

BLI_INLINE_METHOD int64_t Result::get_pixel_index(const int2 &texel) const
{
  BLI_assert(!is_single_value_);
  BLI_assert(this->is_allocated());
  BLI_assert(texel.x >= 0 && texel.y >= 0 && texel.x < domain_.size.x && texel.y < domain_.size.y);
  return int64_t(texel.y) * domain_.size.x + texel.x;
}

}  // namespace blender::compositor
