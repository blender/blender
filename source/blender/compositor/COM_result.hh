/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <type_traits>
#include <utility>
#include <variant>

#include "BLI_assert.h"
#include "BLI_cpp_type.hh"
#include "BLI_generic_span.hh"
#include "BLI_math_interp.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_memory_utils.hh"
#include "BLI_utildefines.h"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_domain.hh"
#include "COM_meta_data.hh"

namespace blender::compositor {

class Context;
class DerivedResources;

/* Make sure to update the format related static methods in the Result class. */
enum class ResultType : uint8_t {
  /* The following types are user facing and can be used as inputs and outputs of operations. They
   * either represent the base type of the result's image or a single value result. */
  Float,
  Int,
  Color,
  Float3,
  Float4,

  /* The following types are for internal use only, not user facing, and can't be used as inputs
   * and outputs of operations. It follows that they needn't be handled in implicit operations like
   * type conversion, shader, or single value reduction operations. */
  Float2,
  Int2,
};

/* The precision of the data. CPU data is always stored using full precision at the moment. */
enum class ResultPrecision : uint8_t {
  Full,
  Half,
};

/* The type of storage used to hold the result data. */
enum class ResultStorageType : uint8_t {
  /* Stored as a GPUTexture on the GPU. */
  GPU,
  /* Stored as a buffer on the CPU and wrapped in a GMutableSpan. */
  CPU,
};

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
 * zero. After constructing a result, the set_initial_reference_count method is called to declare
 * the number of operations that needs this result. Once each operation that needs the result no
 * longer needs it, the release method is called and the reference count is decremented, until it
 * reaches zero, where the result's data is then released. Since results are eventually
 * decremented to zero by the end of every evaluation, the reference count is restored before every
 * evaluation to its initial reference count by calling the reset method, which is why a separate
 * member initial_reference_count_ is stored to keep track of the initial value.
 *
 * A result not only represents an image, but also the area it occupies in the virtual compositing
 * space. This area is called the Domain of the result, see the discussion in COM_domain.hh for
 * more information.
 *
 * A result can be a proxy result that merely wraps another master result, in which case, it shares
 * its values and delegates all reference counting to it. While a proxy result shares the value of
 * the master result, it can have a different domain. Consequently, transformation operations are
 * implemented using proxy results, where their results are proxy results of their inputs but with
 * their domains transformed based on their options. Moreover, proxy results can also be used as
 * the results of identity operations, that is, operations that do nothing to their inputs in
 * certain configurations. In which case, the proxy result is left as is with no extra
 * transformation on its domain whatsoever. Proxy results can be created by calling the
 * pass_through method, see that method for more details.
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
    GPUTexture *gpu_texture_ = nullptr;
    GMutableSpan cpu_data_;
  };
  /* The number of operations that currently needs this result. At the time when the result is
   * computed, this member will have a value that matches initial_reference_count_. Once each
   * operation that needs the result no longer needs it, the release method is called and the
   * reference count is decremented, until it reaches zero, where the result's data is then
   * released. If this result have a master result, then this reference count is irrelevant and
   * shadowed by the reference count of the master result. */
  int reference_count_ = 1;
  /* The number of operations that reference and use this result at the time when it was initially
   * computed. Since reference_count_ is decremented and always becomes zero at the end of the
   * evaluation, this member is used to reset the reference count of the results for later
   * evaluations by calling the reset method. This member is also used to determine if this result
   * should be computed by calling the should_compute method. */
  int initial_reference_count_ = 1;
  /* If the result is a single value, this member stores the value of the result, the value of
   * which will be identical to that stored in the data_ member. The active variant member depends
   * on the type of the result. This member is uninitialized and should not be used if the result
   * is not a single value. */
  std::variant<float, float2, float3, float4, int, int2> single_value_ = 0.0f;
  /* The domain of the result. This only matters if the result was not a single value. See the
   * discussion in COM_domain.hh for more information. */
  Domain domain_ = Domain::identity();
  /* If not nullptr, then this result wraps and shares the value of another master result. In this
   * case, calls to methods like increment_reference_count and release should operate on the master
   * result as opposed to this result. This member is typically set upon calling the pass_through
   * method, which sets this result to be the master of a target result. See that method for more
   * information. */
  Result *master_ = nullptr;
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
  Result(Context &context, eGPUTextureFormat format);

  /* Returns the appropriate GPU texture format based on the given result type and precision. A
   * special case is given to ResultType::Float3, because 3-component textures can't be used as
   * write targets in shaders, so we need to allocate 4-component textures for them, and ignore the
   * fourth channel during processing. */
  static eGPUTextureFormat gpu_texture_format(ResultType type, ResultPrecision precision);

  /* Returns the GPU texture format that corresponds to the give one, but whose precision is the
   * given precision. */
  static eGPUTextureFormat gpu_texture_format(eGPUTextureFormat format, ResultPrecision precision);

  /* Returns the precision of the given GPU texture format. */
  static ResultPrecision precision(eGPUTextureFormat format);

  /* Returns the type of the given GPU texture format. */
  static ResultType type(eGPUTextureFormat format);

  /* Returns the float type of the result given the channels count. */
  static ResultType float_type(const int channels_count);

  /* Returns the CPP type corresponding to the given result type. */
  static const CPPType &cpp_type(const ResultType type);

  /* Implicit conversion to the internal GPU texture. */
  operator GPUTexture *() const;

  /* Returns the CPP type of the result. */
  const CPPType &get_cpp_type() const;

  /* Returns the appropriate texture format based on the result's type and precision. This is
   * identical to the gpu_texture_format static method. This will match the format of the allocated
   * texture, with one exception. Results of type ResultType::Float3 that wrap external textures
   * might hold a 3-component texture as opposed to a 4-component one, which would have been
   * created by uploading data from CPU. */
  eGPUTextureFormat get_gpu_texture_format() const;

  /* Declare the result to be a texture result, allocate a texture of an appropriate type with
   * the size of the given domain, and set the domain of the result to the given domain.
   *
   * If from_pool is true, the texture will be allocated from the texture pool of the context,
   * otherwise, a new texture will be allocated. Pooling should not be used for persistent
   * results that might span more than one evaluation, like cached resources. While pooling should
   * be used for most other cases where the result will be allocated then later released in the
   * same evaluation.
   *
   * If the context of the result uses GPU, then GPU allocation will be done, otherwise, CPU
   * allocation will be done.
   *
   * If the result should not be computed, that is, should_compute() returns false, yet this method
   * is called, that means the result is only being allocated because the shader that computes it
   * also computes another result that is actually needed, and shaders needs to have a texture
   * bound to all their images units for a correct invocation, even if some of those textures are
   * not needed and will eventually be discarded. In that case, since allocating the full texture
   * is not needed, allocate_single_value() is called instead and the reference count is set to 1.
   * This essentially allocates a dummy 1x1 texture, which works because out of bound shader writes
   * to images are safe. Since this result is not referenced by any other operation, it should be
   * manually released after the operation is evaluated, which is implemented by calling the
   * Operation::release_unneeded_results() method. */
  void allocate_texture(Domain domain, bool from_pool = true);

  /* Declare the result to be a single value result, allocate a texture of an appropriate type with
   * size 1x1 from the texture pool, and set the domain to be an identity domain. The value is zero
   * initialized. See class description for more information. */
  void allocate_single_value();

  /* Allocate a single value result whose value is zero. This is called for results whose value
   * can't be computed and are considered invalid. */
  void allocate_invalid();

  /* Bind the GPU texture of the result to the texture image unit with the given name in the
   * currently bound given shader. This also inserts a memory barrier for texture fetches to ensure
   * any prior writes to the texture are reflected before reading from it. */
  void bind_as_texture(GPUShader *shader, const char *texture_name) const;

  /* Bind the GPU texture of the result to the image unit with the given name in the currently
   * bound given shader. If read is true, a memory barrier will be inserted for image reads to
   * ensure any prior writes to the images are reflected before reading from it. */
  void bind_as_image(GPUShader *shader, const char *image_name, bool read = false) const;

  /* Unbind the GPU texture which was previously bound using bind_as_texture. */
  void unbind_as_texture() const;

  /* Unbind the GPU texture which was previously bound using bind_as_image. */
  void unbind_as_image() const;

  /* Pass this result through to a target result, in which case, the target result becomes a proxy
   * result with this result as its master result. This is done by making the target result a copy
   * of this result, essentially having identical values between the two and consequently sharing
   * the underlying texture. An exception is the initial reference count, whose value is retained
   * and not copied, because it is a property of the original result and is needed for correctly
   * resetting the result before the next evaluation. Additionally, this result is set to be the
   * master of the target result, by setting the master member of the target. Finally, the
   * reference count of the result is incremented by the reference count of the target result. See
   * the discussion above for more information. */
  void pass_through(Result &target);

  /* Steal the allocated data from the given source result and assign it to this result, then
   * remove any references to the data from the source result. It is assumed that:
   *
   *   - Both results are of the same type.
   *   - This result is not allocated but the source result is allocated.
   *   - Neither of the results is a proxy one, that is, has a master result.
   *
   * This is different from proxy results and the pass_through mechanism in that it can be used on
   * temporary results. This is most useful in multi-step compositor operations where some steps
   * can be optional, in that case, intermediate results can be temporary results that can
   * eventually be stolen by the actual output of the operation. See the uses of the method for
   * a practical example of use. */
  void steal_data(Result &source);

  /* Set up the result to wrap an external GPU texture that is not allocated nor managed by the
   * result. The is_external_ member will be set to true, the domain will be set to have the same
   * size as the texture, and the texture will be set to the given texture. See the is_external_
   * member for more information. The given texture should have the same format as the result and
   * is assumed to have a lifetime that covers the evaluation of the compositor. */
  void wrap_external(GPUTexture *texture);

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

  /* Set the value of initial_reference_count_, see that member for more details. This should be
   * called after constructing the result to declare the number of operations that needs it. */
  void set_initial_reference_count(int count);

  /* Reset the result to prepare it for a new evaluation. This should be called before evaluating
   * the operation that computes this result. Keep the type, precision, context, and initial
   * reference count, and rest all other members to their default value. Finally, set the value of
   * reference_count_ to the value of initial_reference_count_ since reference_count_ may have
   * already been decremented to zero in a previous evaluation. */
  void reset();

  /* Increment the reference count of the result by the given count. If this result have a master
   * result, the reference count of the master result is incremented instead. */
  void increment_reference_count(int count = 1);

  /* Decrement the reference count of the result by the given count. If this result have a master
   * result, the reference count of the master result is decremented instead. */
  void decrement_reference_count(int count = 1);

  /* Decrement the reference count of the result and free its data if it reaches zero. If this
   * result have a master result, the master result is released instead. */
  void release();

  /* Frees the result data. If the result is not allocated or wraps external data, then this does
   * nothing. If this result have a master result, the master result is freed instead. */
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

  /* Returns the reference count of the result. If this result have a master result, then the
   * reference count of the master result is returned instead. */
  int reference_count() const;

  /* Returns a reference to the domain of the result. See the Domain class. */
  const Domain &domain() const;

  /* Computes the number of channels of the result based on its type. */
  int64_t channels_count() const;

  GPUTexture *gpu_texture() const;

  GMutableSpan cpu_data() const;

  /* Gets the single value stored in the result. Assumes the result stores a value of the given
   * template type. */
  template<typename T> const T &get_single_value() const;
  template<typename T> T &get_single_value();

  /* Gets the single value stored in the result, if the result is not a single value, the given
   * default value is returned. Assumes the result stores a value of the same type as the template
   * type. */
  template<typename T> T get_single_value_default(const T &default_value) const;

  /* Sets the single value of the result to the given value, which also involves setting the single
   * pixel in the image to that value. See the class description for more information. Assumes
   * the result stores a value of the given template type. */
  template<typename T> void set_single_value(const T &value);

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
   * texelFetch function in GLSL works.  */
  float4 load_pixel_generic_type(const int2 &texel) const;

  /* Stores the given pixel value in the pixel at the given texel coordinates. Assumes the result
   * stores a value of the given template type. */
  template<typename T> void store_pixel(const int2 &texel, const T &pixel_value);

  /* Similar to store_pixel, but can write to a result whose types is not known at compile time.
   * While a float4 is given, only the number of channels of the result will be written, while the
   * rest of the float4 will be ignored. This is similar to how the imageStore function in GLSL
   * works. */
  void store_pixel_generic_type(const int2 &texel, const float4 &pixel_value);

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
  /* Allocates the image data for the given size, either on the GPU or CPU based on the result's
   * context. See the allocate_texture method for information about the from_pool argument. */
  void allocate_data(int2 size, bool from_pool);

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
      return 1;
    case ResultType::Float2:
    case ResultType::Int2:
      return 2;
    case ResultType::Float3:
      return 3;
    case ResultType::Color:
    case ResultType::Float4:
      return 4;
  }
  return 4;
}

BLI_INLINE_METHOD GPUTexture *Result::gpu_texture() const
{
  BLI_assert(storage_type_ == ResultStorageType::GPU);
  return gpu_texture_;
}

BLI_INLINE_METHOD GMutableSpan Result::cpu_data() const
{
  BLI_assert(storage_type_ == ResultStorageType::CPU);
  return cpu_data_;
}

template<typename T> BLI_INLINE_METHOD const T &Result::get_single_value() const
{
  BLI_assert(this->is_single_value());

  return std::get<T>(single_value_);
}

template<typename T> BLI_INLINE_METHOD T &Result::get_single_value()
{
  return const_cast<T &>(std::as_const(*this).get_single_value<T>());
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

  switch (storage_type_) {
    case ResultStorageType::GPU:
      if constexpr (is_same_any_v<T, int, int2>) {
        if constexpr (std::is_scalar_v<T>) {
          GPU_texture_update(this->gpu_texture(), GPU_DATA_INT, &value);
        }
        else {
          GPU_texture_update(this->gpu_texture(), GPU_DATA_INT, value);
        }
      }
      else {
        if constexpr (std::is_scalar_v<T>) {
          GPU_texture_update(this->gpu_texture(), GPU_DATA_FLOAT, &value);
        }
        else {
          if constexpr (std::is_same_v<T, float3>) {
            /* Float3 results are stored in 4-component textures due to hardware limitations. So
             * pad the value with a zero before updating. */
            const float4 vector_value = float4(value, 0.0f);
            GPU_texture_update(this->gpu_texture(), GPU_DATA_FLOAT, vector_value);
          }
          else {
            GPU_texture_update(this->gpu_texture(), GPU_DATA_FLOAT, value);
          }
        }
      }
      break;
    case ResultStorageType::CPU:
      this->cpu_data().typed<T>()[0] = value;
      break;
  }
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

/**
 * Given a Result as the userdata argument, sample it at the given coordinates using extended
 * boundary condition and write the result to the result argument.
 */
static void sample_ewa_extended_read_callback(void *userdata, int x, int y, float result[4])
{
  const Result *input = static_cast<const Result *>(userdata);
  const float4 sampled_result = input->load_pixel_extended<float4>(int2(x, y));
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
  const float4 sampled_result = input->load_pixel_zero<float4>(int2(x, y));
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
