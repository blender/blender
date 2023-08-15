/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>
#include <string>

#include "COM_cached_resource.hh"

namespace blender::realtime_compositor {

/* ------------------------------------------------------------------------------------------------
 * OCIO Color Space Conversion Shader Key.
 */
class OCIOColorSpaceConversionShaderKey {
 public:
  std::string source;
  std::string target;
  std::string config_cache_id;

  OCIOColorSpaceConversionShaderKey(std::string source,
                                    std::string target,
                                    std::string config_cache_id);

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
  OCIOColorSpaceConversionShader(std::string source, std::string target);

  GPUShader *bind_shader_and_resources();

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
  OCIOColorSpaceConversionShader &get(std::string source, std::string target);
};

}  // namespace blender::realtime_compositor
