/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"

#include "DNA_mask_types.h"

#include "COM_cached_resource.hh"
#include "COM_result.hh"

namespace blender::compositor {

class Context;

/* ------------------------------------------------------------------------------------------------
 * Cached Mask Key.
 */
class CachedMaskKey {
 public:
  int2 size;
  float aspect_ratio;
  bool use_feather;
  int motion_blur_samples;
  float motion_blur_shutter;

  CachedMaskKey(int2 size,
                float aspect_ratio,
                bool use_feather,
                int motion_blur_samples,
                float motion_blur_shutter);

  uint64_t hash() const;
};

bool operator==(const CachedMaskKey &a, const CachedMaskKey &b);

/* -------------------------------------------------------------------------------------------------
 * Cached Mask.
 *
 * A cached resource that computes and caches a result containing the result of evaluating the
 * given mask ID on a space that spans the given size, parameterized by the given parameters. */
class CachedMask : public CachedResource {
 public:
  Result result;

  CachedMask(Context &context,
             Mask *mask,
             int2 size,
             int frame,
             float aspect_ratio,
             bool use_feather,
             int motion_blur_samples,
             float motion_blur_shutter);

  ~CachedMask();
};

/* ------------------------------------------------------------------------------------------------
 * Cached Mask Container.
 */
class CachedMaskContainer : CachedResourceContainer {
 private:
  Map<std::string, Map<CachedMaskKey, std::unique_ptr<CachedMask>>> map_;

  /* A map that stores the update counts of the masks at the moment they were cached. */
  Map<std::string, uint64_t> update_counts_;

 public:
  void reset() override;

  /* Check if the given mask ID has changed since the last time it was retrieved through its
   * recalculate flag, and if so, invalidate its corresponding cached mask and reset the
   * recalculate flag to ready it to track the next change. Then, check if there is an available
   * CachedMask cached resource with the given parameters in the container, if one exists, return
   * it, otherwise, return a newly created one and add it to the container. In both cases, tag the
   * cached resource as needed to keep it cached for the next evaluation. */
  Result &get(Context &context,
              Mask *mask,
              int2 size,
              float aspect_ratio,
              bool use_feather,
              int motion_blur_samples,
              float motion_blur_shutter);
};

}  // namespace blender::compositor
