/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <memory>
#include <string>

#include "BLI_hash.hh"
#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_vector.hh"

#include "IMB_imbuf.hh"

#include "DNA_defaults.h"
#include "DNA_movieclip_types.h"
#include "DNA_tracking_types.h"

#include "GPU_shader.hh"
#include "GPU_storage_buffer.hh"
#include "GPU_texture.hh"

#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "COM_context.hh"
#include "COM_keying_screen.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::realtime_compositor {

/* --------------------------------------------------------------------
 * Keying Screen Key.
 */

KeyingScreenKey::KeyingScreenKey(int frame, float smoothness)
    : frame(frame), smoothness(smoothness)
{
}

uint64_t KeyingScreenKey::hash() const
{
  return get_default_hash(frame, smoothness);
}

bool operator==(const KeyingScreenKey &a, const KeyingScreenKey &b)
{
  return a.frame == b.frame && a.smoothness == b.smoothness;
}

/* --------------------------------------------------------------------
 * Keying Screen.
 */

/* Computes the color and normalized positions of the keying screen markers in the given movie
 * tracking object. The color is computed as the mean color of the search pattern of the marker. */
static void compute_marker_points(MovieClip *movie_clip,
                                  MovieClipUser &movie_clip_user,
                                  MovieTrackingObject *movie_tracking_object,
                                  Vector<float2> &marker_positions,
                                  Vector<float4> &marker_colors)
{
  BLI_assert(marker_positions.is_empty());
  BLI_assert(marker_colors.is_empty());

  ImBuf *image_buffer = BKE_movieclip_get_ibuf(movie_clip, &movie_clip_user);
  if (!image_buffer) {
    return;
  }

  LISTBASE_FOREACH (MovieTrackingTrack *, track, &movie_tracking_object->tracks) {
    const MovieTrackingMarker *marker = BKE_tracking_marker_get(track, movie_clip_user.framenr);
    if (marker->flag & MARKER_DISABLED) {
      continue;
    }

    /* Skip out of bound markers since they have no corresponding color. */
    const float2 position = float2(marker->pos) + float2(track->offset);
    if (math::clamp(position, float2(0.0f), float2(1.0f)) != position) {
      continue;
    }

    ImBuf *pattern_image_buffer = BKE_tracking_get_pattern_imbuf(
        image_buffer, track, marker, true, false);
    if (!pattern_image_buffer) {
      continue;
    }

    /* Find the mean color of the rectangular search pattern of the marker. */
    float4 mean_color = float4(0.0f);
    for (int i = 0; i < pattern_image_buffer->x * pattern_image_buffer->y; i++) {
      if (pattern_image_buffer->float_buffer.data) {
        mean_color += float4(&pattern_image_buffer->float_buffer.data[i * 4]);
      }
      else {
        float4 linear_color;
        uchar4 srgb_color = uchar4(&pattern_image_buffer->byte_buffer.data[i * 4]);
        srgb_to_linearrgb_uchar4(linear_color, srgb_color);
        mean_color += linear_color;
      }
    }
    mean_color /= pattern_image_buffer->x * pattern_image_buffer->y;

    marker_colors.append(mean_color);
    marker_positions.append(position);

    IMB_freeImBuf(pattern_image_buffer);
  }

  IMB_freeImBuf(image_buffer);
}

/* Get a MovieClipUser with an initialized clip frame number. */
static MovieClipUser get_movie_clip_user(Context &context, MovieClip *movie_clip)
{
  MovieClipUser movie_clip_user = *DNA_struct_default_get(MovieClipUser);
  const int scene_frame = context.get_frame_number();
  const int clip_frame = BKE_movieclip_remap_scene_to_clip_frame(movie_clip, scene_frame);
  BKE_movieclip_user_set_frame(&movie_clip_user, clip_frame);
  return movie_clip_user;
}

KeyingScreen::KeyingScreen(Context &context,
                           MovieClip *movie_clip,
                           MovieTrackingObject *movie_tracking_object,
                           float smoothness)
{
  int2 size;
  MovieClipUser movie_clip_user = get_movie_clip_user(context, movie_clip);
  BKE_movieclip_get_size(movie_clip, &movie_clip_user, &size.x, &size.y);

  Vector<float2> marker_positions;
  Vector<float4> marker_colors;
  compute_marker_points(
      movie_clip, movie_clip_user, movie_tracking_object, marker_positions, marker_colors);

  GPUShader *shader = context.get_shader("compositor_keying_screen");
  GPU_shader_bind(shader);

  GPU_shader_uniform_1f(shader, "smoothness", smoothness);
  GPU_shader_uniform_1i(shader, "number_of_markers", marker_positions.size());

  /* SSBO needs to be aligned to 16 bytes, and since sizeof(float2) is only 8 bytes, we need to add
   * a dummy element at the end for odd sizes to satisfy the alignment requirement. Notice that the
   * number_of_markers uniform was already assigned above to the original size, so the dummy
   * element has no effect in the shader. Also notice that the marker colors are always 16 byte
   * aligned since sizeof(float4) is 16 bytes, so not need to add anything there. */
  if (marker_positions.size() % 2 == 1) {
    marker_positions.append(float2(0.0f));
  }

  GPUStorageBuf *positions_ssbo = GPU_storagebuf_create_ex(marker_positions.size() *
                                                               sizeof(float2),
                                                           marker_positions.data(),
                                                           GPU_USAGE_STATIC,
                                                           "Marker Positions");
  const int positions_ssbo_location = GPU_shader_get_ssbo_binding(shader, "marker_positions");
  GPU_storagebuf_bind(positions_ssbo, positions_ssbo_location);

  GPUStorageBuf *colors_ssbo = GPU_storagebuf_create_ex(marker_colors.size() * sizeof(float4),
                                                        marker_colors.data(),
                                                        GPU_USAGE_STATIC,
                                                        "Marker Colors");
  const int colors_ssbo_location = GPU_shader_get_ssbo_binding(shader, "marker_colors");
  GPU_storagebuf_bind(colors_ssbo, colors_ssbo_location);

  texture_ = GPU_texture_create_2d(
      "Keying Screen",
      size.x,
      size.y,
      1,
      Result::texture_format(ResultType::Color, context.get_precision()),
      GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_SHADER_WRITE,
      nullptr);
  const int image_unit = GPU_shader_get_sampler_binding(shader, "output_img");
  GPU_texture_image_bind(texture_, image_unit);

  compute_dispatch_threads_at_least(shader, size);

  GPU_texture_image_unbind(texture_);
  GPU_storagebuf_unbind(positions_ssbo);
  GPU_storagebuf_unbind(colors_ssbo);
  GPU_shader_unbind();

  GPU_storagebuf_free(positions_ssbo);
  GPU_storagebuf_free(colors_ssbo);
}

KeyingScreen::~KeyingScreen()
{
  GPU_texture_free(texture_);
}

void KeyingScreen::bind_as_texture(GPUShader *shader, const char *texture_name) const
{
  const int texture_image_unit = GPU_shader_get_sampler_binding(shader, texture_name);
  GPU_texture_bind(texture_, texture_image_unit);
}

void KeyingScreen::unbind_as_texture() const
{
  GPU_texture_unbind(texture_);
}

GPUTexture *KeyingScreen::texture() const
{
  return texture_;
}

/* --------------------------------------------------------------------
 * Keying Screen Container.
 */

void KeyingScreenContainer::reset()
{
  /* First, delete all cached keying screens that are no longer needed. */
  for (auto &cached_keying_screens_for_id : map_.values()) {
    cached_keying_screens_for_id.remove_if([](auto item) { return !item.value->needed; });
  }
  map_.remove_if([](auto item) { return item.value.is_empty(); });

  /* Second, reset the needed status of the remaining cached keying screens to false to ready them
   * to track their needed status for the next evaluation. */
  for (auto &cached_keying_screens_for_id : map_.values()) {
    for (auto &value : cached_keying_screens_for_id.values()) {
      value->needed = false;
    }
  }
}

KeyingScreen &KeyingScreenContainer::get(Context &context,
                                         MovieClip *movie_clip,
                                         MovieTrackingObject *movie_tracking_object,
                                         float smoothness)
{
  const KeyingScreenKey key(context.get_frame_number(), smoothness);

  /* We concatenate the movie clip ID name with the tracking object name to cache multiple tracking
   * objects per movie clip. */
  const std::string id_name = std::string(movie_clip->id.name) +
                              std::string(movie_tracking_object->name);
  auto &cached_keying_screens_for_id = map_.lookup_or_add_default(id_name);

  /* Invalidate the keying screen cache for that MovieClip ID if it was changed and reset the
   * recalculate flag. */
  if (context.query_id_recalc_flag(reinterpret_cast<ID *>(movie_clip)) & ID_RECALC_ALL) {
    cached_keying_screens_for_id.clear();
  }

  auto &keying_screen = *cached_keying_screens_for_id.lookup_or_add_cb(key, [&]() {
    return std::make_unique<KeyingScreen>(context, movie_clip, movie_tracking_object, smoothness);
  });

  keying_screen.needed = true;
  return keying_screen;
}

}  // namespace blender::realtime_compositor
