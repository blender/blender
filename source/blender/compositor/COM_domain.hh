/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>

#include "BLI_math_interp.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_texture.hh"

namespace blender::compositor {

/* Possible interpolations to use when realizing an input result of some domain on another domain.
 * See the RealizationOptions struct for more information. */
enum class Interpolation : uint8_t {
  Nearest,
  Bilinear,
  Bicubic,
  Anisotropic,
};

/* Possible extension modes when computing samples in the domain's exterior. */
enum class ExtensionMode : uint8_t {
  /* Areas outside of the image are filled with zero. */
  Clip,
  /* Areas outside of the image are filled with the closest boundary pixel in the image. */
  Extend,
  /* Areas outside of the image are filled with repetitions of the image. */
  Repeat,
};

/* ------------------------------------------------------------------------------------------------
 * Realization Options
 *
 * The options that describe how an input result prefer to be realized on some other domain. This
 * is used by the Realize On Domain and Transform algorithms to identify the appropriate method of
 * realization. See the Domain class for more information. */
struct RealizationOptions {
  /* The interpolation method that should be used when performing realization. Since realizing a
   * result involves projecting it on a different domain, which in turn, involves sampling the
   * result at arbitrary locations, the interpolation identifies the method used for computing the
   * value at those arbitrary locations. */
  Interpolation interpolation = Interpolation::Bilinear;
  /* The extend mode for the x-axis. Defaults to Zero padding. */
  ExtensionMode extension_x = ExtensionMode::Clip;
  /* The extend mode for the y-axis. Defaults to Zero padding. */
  ExtensionMode extension_y = ExtensionMode::Clip;
};

/* ------------------------------------------------------------------------------------------------
 * Domain
 *
 * The compositor is designed in such a way as to allow compositing in an infinite virtual
 * compositing space. Consequently, any result of an operation is not only represented by its image
 * output, but also by its transformation in that virtual space. The transformation of the result
 * together with the dimension of its image is stored and represented by a Domain. In the figure
 * below, two results of different domains are illustrated on the virtual compositing space. One of
 * the results is centered in space with an image dimension of 800px x 600px, and the other result
 * is scaled down and translated such that it lies in the upper right quadrant of the space with an
 * image dimension of 800px × 400px. The position of the domain is in pixel space, and the domain
 * is considered centered if it has an identity transformation. Note that both results have the
 * same resolution, but occupy different areas of the virtual compositing space.
 *
 *                                          y
 *                                          ^
 *                           800px x 600px  |
 *                    .---------------------|---------------------.
 *                    |                     |    800px x 600px    |
 *                    |                     |   .-------------.   |
 *                    |                     |   |             |   |
 *                    |                     |   '-------------'   |
 *              ------|---------------------|---------------------|------> x
 *                    |                     |                     |
 *                    |                     |                     |
 *                    |                     |                     |
 *                    |                     |                     |
 *                    '---------------------|---------------------'
 *                                          |
 *
 * By default, results have domains of identity transformations, that is, they are centered in
 * space, but a transformation operation like the rotate, translate, or transform operations will
 * adjust the transformation to make the result reside somewhere different in space. The domain of
 * a single value result is irrelevant and always set to an identity domain.
 *
 * An operation is typically only concerned about a subset of the virtual compositing space, this
 * subset is represented by a domain which is called the Operation Domain. It follows that before
 * the operation itself is executed, inputs will typically be realized on the operation domain to
 * be in the same domain and have the same dimension as that of the operation domain. This process
 * is called Domain Realization and is implemented using an operation called the Realize On Domain
 * Operation. Realization involves projecting the result onto the target domain, copying the area
 * of the result that intersects the target domain, and filling the rest with zeros or repetitions
 * of the result depending on the realization options that can be set by the user. Consequently,
 * operations can generally expect their inputs to have the same dimension and can operate on them
 * directly and transparently. For instance, if an operation takes both results illustrated in
 * the figure above, and the operation has an operation domain that matches the bigger domain, the
 * result with the bigger domain will not need to be realized because it already has a domain that
 * matches that of the operation domain, but the result with the smaller domain will have to be
 * realized into a new result that has the same domain as the domain of the bigger result. Assuming
 * no repetition, the output of the realization will be an all zeros image with dimension 800px ×
 * 600px with a small scaled version of the smaller result copied into the upper right quadrant of
 * the image. The following figure illustrates the realization process on a different set of
 * results
 *
 *                                   Realized Result
 *             +-------------+       +-------------+
 *             |  Operation  |       |             |
 *             |   Domain    |       |    Zeros    |
 *             |             | ----> |             |
 *       +-----|-----+       |       |-----+       |
 *       |     |  C  |       |       |  C  |       |
 *       |     +-----|-------+       +-----'-------+
 *       | Domain Of |
 *       |   Input   |
 *       +-----------+
 *
 * An operation can operate in an arbitrary operation domain, but in most cases, the operation
 * domain is inferred from the inputs of the operation. In particular, one of the inputs is said to
 * be the Domain Input of the operation and the operation domain is inferred from its domain. It
 * follows that this particular input will not need realization, because it already has the correct
 * domain. The domain input selection mechanism is as follows. Each of the inputs are assigned a
 * value by the developer called the Domain Priority, the domain input is then chosen as the
 * non-single value input with the highest domain priority, zero being the highest priority. See
 * Operation::compute_domain for more information.
 *
 * The aforementioned logic for operation domain computation is only a default that works for most
 * cases, but an operation can override the compute_domain method to implement a different logic.
 * For instance, output nodes have an operation domain the same size as the viewport and with an
 * identity transformation, their operation domain doesn't depend on the inputs at all.
 *
 * For instance, a filter operation has two inputs, a factor and a color, the latter of which is
 * assigned a domain priority of 0 and the former is assigned a domain priority of 1. If the color
 * input is not a single value input, then the color input is considered to be the domain input of
 * the operation and the operation domain is computed to be the same domain as the color input,
 * because it has the highest priority. It follows that if the factor input has a different domain
 * than the computed domain of the operation, it will be projected and realized on it to have the
 * same domain as described above. On the other hand, if the color input is a single value input,
 * then the factor input is considered to be the domain input and the operation domain will be the
 * same as the domain of the factor input, because it has the second highest domain priority.
 * Finally, if both inputs are single value inputs, the operation domain will be an identity domain
 * and is irrelevant, because the output will be a domain-less single value. */
class Domain {
 public:
  /* The size of the domain in pixels. */
  int2 size;
  /* The 2D transformation of the domain defining its translation in pixels, rotation, and scale in
   * the virtual compositing space. */
  float3x3 transformation;
  /* The options that describe how this domain prefer to be realized on some other domain. See the
   * RealizationOptions struct for more information. */
  RealizationOptions realization_options;

  /* A size only constructor that sets the transformation to identity. */
  Domain(const int2 &size);

  Domain(const int2 &size, const float3x3 &transformation);

  /* Transform the domain by the given transformation. This effectively pre-multiply the given
   * transformation by the current transformation of the domain. */
  void transform(const float3x3 &input_transformation);

  /* Returns a transposed version of itself, that is, with the x and y sizes swapped. */
  Domain transposed() const;

  /* Returns a domain of size 1x1 and an identity transformation. */
  static Domain identity();

  /* Compare the size and transformation of the domain. Transformations are compared within the
   * given epsilon. The realization_options are not compared because they only describe the method
   * of realization on another domain, which is not technically a property of the domain itself. */
  static bool is_equal(const Domain &a, const Domain &b, const float epsilon = 0.0f);
};

/* Identical to the is_equal static method with zero epsilon. */
bool operator==(const Domain &a, const Domain &b);
bool operator!=(const Domain &a, const Domain &b);

math::InterpWrapMode map_extension_mode_to_wrap_mode(const ExtensionMode &mode);
GPUSamplerExtendMode map_extension_mode_to_extend_mode(const ExtensionMode &mode);

}  // namespace blender::compositor
