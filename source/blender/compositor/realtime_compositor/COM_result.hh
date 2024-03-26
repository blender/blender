/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_domain.hh"
#include "COM_texture_pool.hh"

namespace blender::realtime_compositor {

/* To add a new type, update the format related static methods in the Result class. */
enum class ResultType : uint8_t {
  /* The following types are user facing and can be used as inputs and outputs of operations. They
   * either represent the base type of the result texture or a single value result. The color type
   * represents an RGBA color. And the vector type represents a generic 4-component vector, which
   * can encode two 2D vectors, one 3D vector with the last component ignored, or other dimensional
   * data. */
  Float,
  Vector,
  Color,

  /* The following types are for internal use only, not user facing, and can't be used as inputs
   * and outputs of operations. Furthermore, they can't be single values and thus always need to be
   * allocated as textures. It follows that they needn't be handled in implicit operations like
   * type conversion, shader, or single value reduction operations. */
  Float2,
  Float3,
  Int2,
};

enum class ResultPrecision : uint8_t {
  Full,
  Half,
};

/* ------------------------------------------------------------------------------------------------
 * Result
 *
 * A result represents the computed value of an output of an operation. A result can either
 * represent an image or a single value. A result is typed, and can be of type color, vector, or
 * float. Single value results are stored in 1x1 textures to make them easily accessible in
 * shaders. But the same value is also stored in the value union member of the result for any
 * host-side processing. The texture of the result is allocated from the texture pool referenced by
 * the result.
 *
 * Results are reference counted and their textures are released once their reference count reaches
 * zero. After constructing a result, the set_initial_reference_count method is called to declare
 * the number of operations that needs this result. Once each operation that needs the result no
 * longer needs it, the release method is called and the reference count is decremented, until it
 * reaches zero, where the result's texture is then released. Since results are eventually
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
 * A result can wrap an external texture that is not allocated nor managed by the result. This is
 * set up by a call to the wrap_external method. In that case, when the reference count eventually
 * reach zero, the texture will not be freed. */
class Result {
 private:
  /* The base type of the result's texture or single value. */
  ResultType type_;
  /* The precision of the result's texture, host-side single values are always stored using full
   * precision. */
  ResultPrecision precision_ = ResultPrecision::Half;
  /* If true, the result is a single value, otherwise, the result is a texture. */
  bool is_single_value_;
  /* A GPU texture storing the result data. This will be a 1x1 texture if the result is a single
   * value, the value of which will be identical to that of the value member. See class description
   * for more information. */
  GPUTexture *texture_ = nullptr;
  /* The texture pool used to allocate the texture of the result, this should be initialized during
   * construction. */
  TexturePool *texture_pool_ = nullptr;
  /* The number of operations that currently needs this result. At the time when the result is
   * computed, this member will have a value that matches initial_reference_count_. Once each
   * operation that needs the result no longer needs it, the release method is called and the
   * reference count is decremented, until it reaches zero, where the result's texture is then
   * released. If this result have a master result, then this reference count is irrelevant and
   * shadowed by the reference count of the master result. */
  int reference_count_;
  /* The number of operations that reference and use this result at the time when it was initially
   * computed. Since reference_count_ is decremented and always becomes zero at the end of the
   * evaluation, this member is used to reset the reference count of the results for later
   * evaluations by calling the reset method. This member is also used to determine if this result
   * should be computed by calling the should_compute method. */
  int initial_reference_count_;
  /* If the result is a single value, this member stores the value of the result, the value of
   * which will be identical to that stored in the texture member. The active union member depends
   * on the type of the result. This member is uninitialized and should not be used if the result
   * is a texture. */
  union {
    float float_value_;
    float4 vector_value_;
    float4 color_value_;
  };
  /* The domain of the result. This only matters if the result was a texture. See the discussion in
   * COM_domain.hh for more information. */
  Domain domain_ = Domain::identity();
  /* If not nullptr, then this result wraps and shares the value of another master result. In this
   * case, calls to texture-related methods like increment_reference_count and release should
   * operate on the master result as opposed to this result. This member is typically set upon
   * calling the pass_through method, which sets this result to be the master of a target result.
   * See that method for more information. */
  Result *master_ = nullptr;
  /* If true, then the result wraps an external texture that is not allocated nor managed by the
   * result. This is set up by a call to the wrap_external method. In that case, when the reference
   * count eventually reach zero, the texture will not be freed. */
  bool is_external_ = false;

 public:
  /* Construct a result of the given type and precision with the given texture pool that will be
   * used to allocate and release the result's texture. */
  Result(ResultType type, TexturePool &texture_pool, ResultPrecision precision);

  /* Identical to the standard constructor but initializes the reference count to 1. This is useful
   * to construct temporary results that are created and released by the developer manually, which
   * are typically used in operations that need temporary intermediate results. */
  static Result Temporary(ResultType type, TexturePool &texture_pool, ResultPrecision precision);

  /* Returns the appropriate texture format based on the given result type and precision. */
  static eGPUTextureFormat texture_format(ResultType type, ResultPrecision precision);

  /* Returns the texture format that corresponds to the give one, but with the given precision. */
  static eGPUTextureFormat texture_format(eGPUTextureFormat format, ResultPrecision precision);

  /* Returns the precision of the given format. */
  static ResultPrecision precision(eGPUTextureFormat format);

  /* Returns the type of the given format. */
  static ResultType type(eGPUTextureFormat format);

  /* Returns the appropriate texture format based on the result's type and precision. */
  eGPUTextureFormat get_texture_format() const;

  /* Declare the result to be a texture result, allocate a texture of an appropriate type with
   * the size of the given domain from the result's texture pool, and set the domain of the result
   * to the given domain.
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
  void allocate_texture(Domain domain);

  /* Declare the result to be a single value result, allocate a texture of an appropriate
   * type with size 1x1 from the result's texture pool, and set the domain to be an identity
   * domain. See class description for more information. */
  void allocate_single_value();

  /* Allocate a single value result and set its value to zero. This is called for results whose
   * value can't be computed and are considered invalid. */
  void allocate_invalid();

  /* Bind the texture of the result to the texture image unit with the given name in the currently
   * bound given shader. This also inserts a memory barrier for texture fetches to ensure any prior
   * writes to the texture are reflected before reading from it. */
  void bind_as_texture(GPUShader *shader, const char *texture_name) const;

  /* Bind the texture of the result to the image unit with the given name in the currently bound
   * given shader. If read is true, a memory barrier will be inserted for image reads to ensure any
   * prior writes to the images are reflected before reading from it. */
  void bind_as_image(GPUShader *shader, const char *image_name, bool read = false) const;

  /* Unbind the texture which was previously bound using bind_as_texture. */
  void unbind_as_texture() const;

  /* Unbind the texture which was previously bound using bind_as_image. */
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

  /* Set up the result to wrap an external texture that is not allocated nor managed by the result.
   * The is_external_ member will be set to true, the domain will be set to have the same size as
   * the texture, and the texture will be set to the given texture. See the is_external_ member for
   * more information. The given texture should have the same format as the result and is assumed
   * to have a lifetime that covers the evaluation of the compositor. */
  void wrap_external(GPUTexture *texture);

  /* Sets the transformation of the domain of the result to the given transformation. */
  void set_transformation(const float3x3 &transformation);

  /* Transform the result by the given transformation. This effectively pre-multiply the given
   * transformation by the current transformation of the domain of the result. */
  void transform(const float3x3 &transformation);

  /* Get a reference to the realization options of this result. See the RealizationOptions struct
   * for more information. */
  RealizationOptions &get_realization_options();

  /* If the result is a single value result of type float, return its float value. Otherwise, an
   * uninitialized value is returned. */
  float get_float_value() const;

  /* If the result is a single value result of type vector, return its vector value. Otherwise, an
   * uninitialized value is returned. */
  float4 get_vector_value() const;

  /* If the result is a single value result of type color, return its color value. Otherwise, an
   * uninitialized value is returned. */
  float4 get_color_value() const;

  /* Same as get_float_value but returns a default value if the result is not a single value. */
  float get_float_value_default(float default_value) const;

  /* Same as get_vector_value but returns a default value if the result is not a single value. */
  float4 get_vector_value_default(const float4 &default_value) const;

  /* Same as get_color_value but returns a default value if the result is not a single value. */
  float4 get_color_value_default(const float4 &default_value) const;

  /* If the result is a single value result of type float, set its float value and upload it to the
   * texture. Otherwise, an undefined behavior is invoked. */
  void set_float_value(float value);

  /* If the result is a single value result of type vector, set its vector value and upload it to
   * the texture. Otherwise, an undefined behavior is invoked. */
  void set_vector_value(const float4 &value);

  /* If the result is a single value result of type color, set its color value and upload it to the
   * texture. Otherwise, an undefined behavior is invoked. */
  void set_color_value(const float4 &value);

  /* Set the value of initial_reference_count_, see that member for more details. This should be
   * called after constructing the result to declare the number of operations that needs it. */
  void set_initial_reference_count(int count);

  /* Reset the result to prepare it for a new evaluation. This should be called before evaluating
   * the operation that computes this result. Keep the type, precision, texture pool, and initial
   * reference count, and rest all other members to their default value. Finally, set the value of
   * reference_count_ to the value of initial_reference_count_ since reference_count_ may have
   * already been decremented to zero in a previous evaluation. */
  void reset();

  /* Increment the reference count of the result by the given count. If this result have a master
   * result, the reference count of the master result is incremented instead. */
  void increment_reference_count(int count = 1);

  /* Decrement the reference count of the result and release the its texture back into the texture
   * pool if the reference count reaches zero. This should be called when an operation that used
   * this result no longer needs it. If this result have a master result, the master result is
   * released instead. */
  void release();

  /* Returns true if this result should be computed and false otherwise. The result should be
   * computed if its reference count is not zero, that is, its result is used by at least one
   * operation. */
  bool should_compute();

  /* Returns the type of the result. */
  ResultType type() const;

  /* Returns the precision of the result. */
  ResultPrecision precision() const;

  /* Sets the precision of the result. */
  void set_precision(ResultPrecision precision);

  /* Returns true if the result is a texture and false of it is a single value. */
  bool is_texture() const;

  /* Returns true if the result is a single value and false of it is a texture. */
  bool is_single_value() const;

  /* Returns true if the result is allocated. */
  bool is_allocated() const;

  /* Returns the allocated GPU texture of the result. */
  GPUTexture *texture() const;

  /* Returns the reference count of the result. If this result have a master result, then the
   * reference count of the master result is returned instead. */
  int reference_count() const;

  /* Returns a reference to the domain of the result. See the Domain class. */
  const Domain &domain() const;
};

}  // namespace blender::realtime_compositor
