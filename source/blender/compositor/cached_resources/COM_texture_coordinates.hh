/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>
#include <memory>

#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"

#include "COM_cached_resource.hh"
#include "COM_result.hh"

namespace blender::compositor {

class Context;

/* ------------------------------------------------------------------------------------------------
 * Texture Coordinates Key.
 */
class TextureCoordinatesKey {
 public:
  int2 size;

  TextureCoordinatesKey(const int2 &size);

  uint64_t hash() const;
};

bool operator==(const TextureCoordinatesKey &a, const TextureCoordinatesKey &b);

/* -------------------------------------------------------------------------------------------------
 * Texture Coordinates.
 *
 * A cached resource that computes and caches a result containing the texture coordinates of an
 * image with the given size. The texture coordinates are the zero centered pixel coordinates
 * normalized along the greater dimension. Pixel coordinates represent the center of pixels, so
 * they include half pixel offsets. */
class TextureCoordinates : public CachedResource {
 public:
  Result result;

  TextureCoordinates(Context &context, const int2 &size);

  ~TextureCoordinates();

 private:
  void compute_gpu(Context &context);

  void compute_cpu();
};

/* ------------------------------------------------------------------------------------------------
 * Texture Coordinates Container.
 */
class TextureCoordinatesContainer : CachedResourceContainer {
 private:
  Map<TextureCoordinatesKey, std::unique_ptr<TextureCoordinates>> map_;

 public:
  void reset() override;

  /* Check if there is an available TextureCoordinates cached resource with the given parameters in
   * the container, if one exists, return it, otherwise, return a newly created one and add it to
   * the container. In both cases, tag the cached resource as needed to keep it cached for the next
   * evaluation. */
  Result &get(Context &context, const int2 &size);
};

}  // namespace blender::compositor
