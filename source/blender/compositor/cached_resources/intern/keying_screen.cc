/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <memory>
#include <string>

#include "BLI_hash.hh"
#include "BLI_listbase.h"
#include "BLI_math_base.hh"
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

#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "COM_context.hh"
#include "COM_keying_screen.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

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
                           const float smoothness)
    : result(context.create_result(ResultType::Color))
{
  int2 size;
  MovieClipUser movie_clip_user = get_movie_clip_user(context, movie_clip);
  BKE_movieclip_get_size(movie_clip, &movie_clip_user, &size.x, &size.y);

  Vector<float2> marker_positions;
  Vector<float4> marker_colors;
  compute_marker_points(
      movie_clip, movie_clip_user, movie_tracking_object, marker_positions, marker_colors);

  if (marker_positions.is_empty()) {
    return;
  }

  this->result.allocate_texture(Domain(size), false);
  if (context.use_gpu()) {
    this->compute_gpu(context, smoothness, marker_positions, marker_colors);
  }
  else {
    this->compute_cpu(smoothness, marker_positions, marker_colors);
  }
}

void KeyingScreen::compute_gpu(Context &context,
                               const float smoothness,
                               Vector<float2> &marker_positions,
                               const Vector<float4> &marker_colors)
{
  gpu::Shader *shader = context.get_shader("compositor_keying_screen");
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

  gpu::StorageBuf *positions_ssbo = GPU_storagebuf_create_ex(marker_positions.size() *
                                                                 sizeof(float2),
                                                             marker_positions.data(),
                                                             GPU_USAGE_STATIC,
                                                             "Marker Positions");
  const int positions_ssbo_location = GPU_shader_get_ssbo_binding(shader, "marker_positions");
  GPU_storagebuf_bind(positions_ssbo, positions_ssbo_location);

  gpu::StorageBuf *colors_ssbo = GPU_storagebuf_create_ex(marker_colors.size() * sizeof(float4),
                                                          marker_colors.data(),
                                                          GPU_USAGE_STATIC,
                                                          "Marker Colors");
  const int colors_ssbo_location = GPU_shader_get_ssbo_binding(shader, "marker_colors");
  GPU_storagebuf_bind(colors_ssbo, colors_ssbo_location);

  this->result.bind_as_image(shader, "output_img");

  compute_dispatch_threads_at_least(shader, this->result.domain().size);

  this->result.unbind_as_image();
  GPU_storagebuf_unbind(positions_ssbo);
  GPU_storagebuf_unbind(colors_ssbo);
  GPU_shader_unbind();

  GPU_storagebuf_free(positions_ssbo);
  GPU_storagebuf_free(colors_ssbo);
}

void KeyingScreen::compute_cpu(const float smoothness,
                               const Vector<float2> &marker_positions,
                               const Vector<float4> &marker_colors)
{
  float squared_shape_parameter = math::square(1.0f / smoothness);
  const int2 size = this->result.domain().size;
  parallel_for(size, [&](const int2 texel) {
    float2 normalized_pixel_location = (float2(texel) + float2(0.5f)) / float2(size);

    /* Interpolate the markers using a Gaussian Radial Basis Function Interpolation with the
     * reciprocal of the smoothness as the shaping parameter. Equal weights are assigned to all
     * markers, so no RBF fitting is required. */
    float sum_of_weights = 0.0f;
    float4 weighted_sum = float4(0.0f);
    for (const int64_t i : marker_positions.index_range()) {
      float2 marker_position = marker_positions[i];
      float2 difference = normalized_pixel_location - marker_position;
      float squared_distance = math::dot(difference, difference);
      float gaussian = math::exp(-squared_distance * squared_shape_parameter);

      float4 marker_color = marker_colors[i];
      weighted_sum += marker_color * gaussian;
      sum_of_weights += gaussian;
    }
    weighted_sum /= sum_of_weights;

    this->result.store_pixel(texel, Color(weighted_sum));
  });
}

KeyingScreen::~KeyingScreen()
{
  this->result.release();
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

Result &KeyingScreenContainer::get(Context &context,
                                   MovieClip *movie_clip,
                                   MovieTrackingObject *movie_tracking_object,
                                   float smoothness)
{
  const KeyingScreenKey key(context.get_frame_number(), smoothness);

  /* We concatenate the movie clip ID name with the tracking object name to cache multiple tracking
   * objects per movie clip. */
  const std::string library_key = movie_clip->id.lib ? movie_clip->id.lib->id.name : "";
  const std::string id_key = std::string(movie_clip->id.name) + library_key;
  const std::string object_key = id_key + movie_tracking_object->name;
  auto &cached_keying_screens_for_id = map_.lookup_or_add_default(object_key);

  /* Invalidate the cache for that movie clip if it was changed since it was cached. */
  if (!cached_keying_screens_for_id.is_empty() &&
      movie_clip->runtime.last_update != update_counts_.lookup(id_key))
  {
    cached_keying_screens_for_id.clear();
  }

  auto &keying_screen = *cached_keying_screens_for_id.lookup_or_add_cb(key, [&]() {
    return std::make_unique<KeyingScreen>(context, movie_clip, movie_tracking_object, smoothness);
  });

  /* Store the current update count to later compare to and check if the movie clip changed. */
  update_counts_.add_overwrite(id_key, movie_clip->runtime.last_update);

  keying_screen.needed = true;
  return keying_screen.result;
}

}  // namespace blender::compositor
