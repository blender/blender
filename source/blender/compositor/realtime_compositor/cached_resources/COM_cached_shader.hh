/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "BLI_map.hh"

#include "GPU_shader.hh"

#include "COM_cached_resource.hh"
#include "COM_result.hh"

namespace blender::realtime_compositor {

/* ------------------------------------------------------------------------------------------------
 * Cached Shader Key.
 */

class CachedShaderKey {
 public:
  std::string info_name;
  ResultPrecision precision;

  CachedShaderKey(const char *info_name, ResultPrecision precision);

  uint64_t hash() const;
};

bool operator==(const CachedShaderKey &a, const CachedShaderKey &b);

/* -------------------------------------------------------------------------------------------------
 * Cached Shader.
 *
 * A cached resource that constructs and caches a GPU shader from the given info name with its
 * output images' precision changed to the given precision. */
class CachedShader : public CachedResource {
 private:
  GPUShader *shader_ = nullptr;

 public:
  CachedShader(const char *info_name, ResultPrecision precision);

  ~CachedShader();

  GPUShader *shader() const;
};

/* ------------------------------------------------------------------------------------------------
 * Cached Shader Container.
 */
class CachedShaderContainer : public CachedResourceContainer {
 private:
  Map<CachedShaderKey, std::unique_ptr<CachedShader>> map_;

 public:
  void reset() override;

  /* Check if there is an available CachedShader cached resource with the given parameters in the
   * container, if one exists, return its shader, otherwise, return the shader of a newly created
   * one and add it to the container. In both cases, tag the cached resource as needed to keep it
   * cached for the next evaluation. */
  GPUShader *get(const char *info_name, ResultPrecision precision);
};

}  // namespace blender::realtime_compositor
