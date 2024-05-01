/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "DNA_movieclip_types.h"
#include "DNA_tracking_types.h"

#include "COM_cached_resource.hh"

namespace blender::realtime_compositor {

class Context;

/* ------------------------------------------------------------------------------------------------
 * Keying Screen Key.
 */
class KeyingScreenKey {
 public:
  int2 frame;
  float smoothness;

  KeyingScreenKey(int frame, float smoothness);

  uint64_t hash() const;
};

bool operator==(const KeyingScreenKey &a, const KeyingScreenKey &b);

/* -------------------------------------------------------------------------------------------------
 * Keying Screen.
 *
 * A cached resource that computes and caches a GPU texture containing the keying screen computed
 * by interpolating the markers of the given movie tracking object in the given movie clip. */
class KeyingScreen : public CachedResource {
 private:
  GPUTexture *texture_ = nullptr;

 public:
  KeyingScreen(Context &context,
               MovieClip *movie_clip,
               MovieTrackingObject *movie_tracking_object,
               float smoothness);

  ~KeyingScreen();

  void bind_as_texture(GPUShader *shader, const char *texture_name) const;

  void unbind_as_texture() const;

  GPUTexture *texture() const;
};

/* ------------------------------------------------------------------------------------------------
 * Keying Screen Container.
 */
class KeyingScreenContainer : CachedResourceContainer {
 private:
  Map<std::string, Map<KeyingScreenKey, std::unique_ptr<KeyingScreen>>> map_;

 public:
  void reset() override;

  /* Check if the given movie clip ID has changed since the last time it was retrieved through its
   * recalculate flag, and if so, invalidate its corresponding cached keying screens and reset the
   * recalculate flag to ready it to track the next change. Then, check if there is an available
   * KeyingScreen cached resource with the given parameters in the container, if one exists, return
   * it, otherwise, return a newly created one and add it to the container. In both cases, tag the
   * cached resource as needed to keep it cached for the next evaluation. */
  KeyingScreen &get(Context &context,
                    MovieClip *movie_clip,
                    MovieTrackingObject *movie_tracking_object,
                    float smoothness);
};

}  // namespace blender::realtime_compositor
