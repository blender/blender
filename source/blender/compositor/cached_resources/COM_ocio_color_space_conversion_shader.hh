/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>
#include <string>

#include "BLI_map.hh"

#include "DNA_color_types.h"

#include "COM_cached_resource.hh"

namespace blender {

namespace gpu {
class Shader;
}  // namespace gpu

namespace compositor {

class Context;

/* ------------------------------------------------------------------------------------------------
 * OCIO Color Space Conversion Shader Key.
 */
class OCIOColorSpaceConversionShaderKey {
 public:
  std::string source;
  std::string target;
  std::string config_cache_id;

  OCIOColorSpaceConversionShaderKey(const std::string &source,
                                    const std::string &target,
                                    const std::string &config_cache_id);

  uint64_t hash() const;
};

bool operator==(const OCIOColorSpaceConversionShaderKey &a,
                const OCIOColorSpaceConversionShaderKey &b);

class GPUShaderCreator;

/* -------------------------------------------------------------------------------------------------
 * OCIO Color Space Conversion Shader.
 *
 * A cached resource that creates and caches a GPU shader that converts the source OCIO color space
 * of an image into a different target OCIO color space. */
class OCIOColorSpaceConversionShader : public CachedResource {
 private:
  std::shared_ptr<GPUShaderCreator> shader_creator_;

 public:
  OCIOColorSpaceConversionShader(Context &context, std::string source, std::string target);

  gpu::Shader *bind_shader_and_resources();

  void unbind_shader_and_resources();

  const char *input_sampler_name();

  const char *output_image_name();
};

/* ------------------------------------------------------------------------------------------------
 * OCIO Color Space Conversion Shader Container.
 */
class OCIOColorSpaceConversionShaderContainer : CachedResourceContainer {
 private:
  Map<OCIOColorSpaceConversionShaderKey, std::unique_ptr<OCIOColorSpaceConversionShader>> map_;

 public:
  void reset() override;

  /* Check if there is an available OCIOColorSpaceConversionShader cached resource with the given
   * parameters in the container, if one exists, return it, otherwise, return a newly created one
   * and add it to the container. In both cases, tag the cached resource as needed to keep it
   * cached for the next evaluation. */
  OCIOColorSpaceConversionShader &get(Context &context, std::string source, std::string target);
};

/* ------------------------------------------------------------------------------------------------
 * OCIO To Display Shader Key.
 */
class OCIOToDisplayShaderKey {
 public:
  std::string display_device;
  std::string view_transform;
  std::string look;
  bool inverse;
  std::string config_cache_id;

  OCIOToDisplayShaderKey(const ColorManagedDisplaySettings &display_settings,
                         const ColorManagedViewSettings &view_settings,
                         const bool inverse,
                         const std::string &config_cache_id);

  uint64_t hash() const;
};

bool operator==(const OCIOToDisplayShaderKey &a, const OCIOToDisplayShaderKey &b);

class GPUShaderCreator;

/* -------------------------------------------------------------------------------------------------
 * OCIO To Display Shader.
 *
 * A cached resource that creates and caches a GPU shader that converts the source OCIO color space
 * of an image into a different target OCIO color space. */
class OCIOToDisplayShader : public CachedResource {
 private:
  std::shared_ptr<GPUShaderCreator> shader_creator_;

 public:
  OCIOToDisplayShader(Context &context,
                      const ColorManagedDisplaySettings &display_settings,
                      const ColorManagedViewSettings &view_settings,
                      const bool inverse);

  gpu::Shader *bind_shader_and_resources();

  void unbind_shader_and_resources();

  const char *input_sampler_name();

  const char *output_image_name();
};

/* ------------------------------------------------------------------------------------------------
 * OCIO To Display Shader Container.
 */
class OCIOToDisplayShaderContainer : CachedResourceContainer {
 private:
  Map<OCIOToDisplayShaderKey, std::unique_ptr<OCIOToDisplayShader>> map_;

 public:
  void reset() override;

  /* Check if there is an available OCIOToDisplayShader cached resource with the given
   * parameters in the container, if one exists, return it, otherwise, return a newly created one
   * and add it to the container. In both cases, tag the cached resource as needed to keep it
   * cached for the next evaluation. */
  OCIOToDisplayShader &get(Context &context,
                           const ColorManagedDisplaySettings &display_settings,
                           const ColorManagedViewSettings &view_settings,
                           const bool inverse);
};

}  // namespace compositor
}  // namespace blender
