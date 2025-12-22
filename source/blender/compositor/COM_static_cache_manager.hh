/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_bokeh_kernel.hh"
#include "COM_cached_image.hh"
#include "COM_cached_mask.hh"
#include "COM_cached_shader.hh"
#include "COM_deriche_gaussian_coefficients.hh"
#include "COM_distortion_grid.hh"
#include "COM_fog_glow_kernel.hh"
#include "COM_image_coordinates.hh"
#include "COM_keying_screen.hh"
#include "COM_morphological_distance_feather_weights.hh"
#include "COM_ocio_color_space_conversion_shader.hh"
#include "COM_smaa_precomputed_textures.hh"
#include "COM_symmetric_blur_weights.hh"
#include "COM_symmetric_separable_blur_weights.hh"
#include "COM_van_vliet_gaussian_coefficients.hh"

namespace blender::compositor {

/* -------------------------------------------------------------------------------------------------
 * Static Cache Manager
 *
 * A static cache manager is a collection of cached resources that can be retrieved when needed and
 * created if not already available. In particular, each cached resource type has its own instance
 * of a container derived from the CachedResourceContainer type in the class. All instances of that
 * cached resource type are stored and tracked in the container. See the CachedResource and
 * CachedResourceContainer classes for more information.
 *
 * The manager deletes the cached resources that are no longer needed. A cached resource is said to
 * be not needed when it was not used in the previous evaluation. This is done through the
 * following mechanism:
 *
 * - After every evaluation, do the following:
 *     1. All resources whose CachedResource::needed flag is false are deleted.
 *     2. The CachedResource::needed flag of all remaining resources is set to false.
 * - During evaluation, when retrieving any cached resource, set its CachedResource::needed flag to
 *   true.
 *
 * In effect, any resource that was not used in the previous evaluation will be deleted. This
 * mechanism is implemented in the reset() method of the class, which should be called after every
 * evaluation. */
class StaticCacheManager {
 public:
  SymmetricBlurWeightsContainer symmetric_blur_weights;
  SymmetricSeparableBlurWeightsContainer symmetric_separable_blur_weights;
  MorphologicalDistanceFeatherWeightsContainer morphological_distance_feather_weights;
  CachedMaskContainer cached_masks;
  SMAAPrecomputedTexturesContainer smaa_precomputed_textures;
  OCIOColorSpaceConversionShaderContainer ocio_color_space_conversion_shaders;
  OCIOToDisplayShaderContainer ocio_to_display_shaders;
  DistortionGridContainer distortion_grids;
  KeyingScreenContainer keying_screens;
  CachedShaderContainer cached_shaders;
  BokehKernelContainer bokeh_kernels;
  CachedImageContainer cached_images;
  DericheGaussianCoefficientsContainer deriche_gaussian_coefficients;
  VanVlietGaussianCoefficientsContainer van_vliet_gaussian_coefficients;
  FogGlowKernelContainer fog_glow_kernels;
  ImageCoordinatesContainer image_coordinates;

 public:
  /* Reset the cache manager by deleting the cached resources that are no longer needed because
   * they weren't used in the last evaluation and prepare the remaining cached resources to track
   * their needed status in the next evaluation. See the class description for more information.
   * This should be called before every evaluation. */
  void reset();

  /* Force free all resources even if they are still needed. This is useful to manually destroy the
   * static cache manager, for instance, with a GPU context bound. */
  void free();
};

}  // namespace blender::compositor
