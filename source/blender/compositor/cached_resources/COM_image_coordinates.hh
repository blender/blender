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

enum class CoordinatesType : uint8_t {
  Uniform,
  Normalized,
  Pixel,
};

/* ------------------------------------------------------------------------------------------------
 * Image Coordinates Key.
 */
class ImageCoordinatesKey {
 public:
  int2 size;
  CoordinatesType type;

  ImageCoordinatesKey(const int2 &size, const CoordinatesType type);

  uint64_t hash() const;
};

bool operator==(const ImageCoordinatesKey &a, const ImageCoordinatesKey &b);

/* -------------------------------------------------------------------------------------------------
 * Image Coordinates.
 *
 * A cached resource that computes and caches a result containing the coordinates of the pixels of
 * an image with the given size. */
class ImageCoordinates : public CachedResource {
 public:
  Result result;

  ImageCoordinates(Context &context, const int2 &size, const CoordinatesType type);

  ~ImageCoordinates();

 private:
  void compute_gpu(Context &context, const CoordinatesType type);

  void compute_cpu(const CoordinatesType type);
};

/* ------------------------------------------------------------------------------------------------
 * Image Coordinates Container.
 */
class ImageCoordinatesContainer : CachedResourceContainer {
 private:
  Map<ImageCoordinatesKey, std::unique_ptr<ImageCoordinates>> map_;

 public:
  void reset() override;

  /* Check if there is an available ImageCoordinates cached resource with the given parameters in
   * the container, if one exists, return it, otherwise, return a newly created one and add it to
   * the container. In both cases, tag the cached resource as needed to keep it cached for the next
   * evaluation. */
  Result &get(Context &context, const int2 &size, const CoordinatesType type);
};

}  // namespace blender::compositor
