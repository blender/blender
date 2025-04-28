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
 * Pixel Coordinates Key.
 */
class PixelCoordinatesKey {
 public:
  int2 size;

  PixelCoordinatesKey(const int2 &size);

  uint64_t hash() const;
};

bool operator==(const PixelCoordinatesKey &a, const PixelCoordinatesKey &b);

/* -------------------------------------------------------------------------------------------------
 * Pixel Coordinates.
 *
 * A cached resource that computes and caches a result containing the pixel coordinates of an image
 * with the given size. The coordinates represent the center of pixels, so they include half pixel
 * offsets. */
class PixelCoordinates : public CachedResource {
 public:
  Result result;

  PixelCoordinates(Context &context, const int2 &size);

  ~PixelCoordinates();

 private:
  void compute_gpu(Context &context);

  void compute_cpu();
};

/* ------------------------------------------------------------------------------------------------
 * Pixel Coordinates Container.
 */
class PixelCoordinatesContainer : CachedResourceContainer {
 private:
  Map<PixelCoordinatesKey, std::unique_ptr<PixelCoordinates>> map_;

 public:
  void reset() override;

  /* Check if there is an available PixelCoordinates cached resource with the given parameters in
   * the container, if one exists, return it, otherwise, return a newly created one and add it to
   * the container. In both cases, tag the cached resource as needed to keep it cached for the next
   * evaluation. */
  Result &get(Context &context, const int2 &size);
};

}  // namespace blender::compositor
