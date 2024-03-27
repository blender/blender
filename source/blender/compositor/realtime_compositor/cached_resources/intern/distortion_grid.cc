/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <memory>

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_index_range.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"

#include "DNA_defaults.h"
#include "DNA_movieclip_types.h"
#include "DNA_tracking_types.h"

#include "GPU_texture.hh"

#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "COM_context.hh"
#include "COM_distortion_grid.hh"
#include "COM_result.hh"

namespace blender::realtime_compositor {

/* --------------------------------------------------------------------
 * Distortion Grid Key.
 */

DistortionGridKey::DistortionGridKey(MovieTrackingCamera camera,
                                     int2 size,
                                     DistortionType type,
                                     int2 calibration_size)
    : camera(camera), size(size), type(type), calibration_size(calibration_size)
{
}

uint64_t DistortionGridKey::hash() const
{
  return get_default_hash(
      BKE_tracking_camera_distortion_hash(&camera), size, type, calibration_size);
}

bool operator==(const DistortionGridKey &a, const DistortionGridKey &b)
{
  return BKE_tracking_camera_distortion_equal(&a.camera, &b.camera) && a.size == b.size &&
         a.type == b.type && a.calibration_size == b.calibration_size;
}

/* --------------------------------------------------------------------
 * Distortion Grid.
 */

DistortionGrid::DistortionGrid(
    Context &context, MovieClip *movie_clip, int2 size, DistortionType type, int2 calibration_size)
{
  MovieDistortion *distortion = BKE_tracking_distortion_new(
      &movie_clip->tracking, calibration_size.x, calibration_size.y);

  Array<float2> distortion_grid(size.x * size.y);
  threading::parallel_for(IndexRange(size.y), 1, [&](const IndexRange sub_y_range) {
    for (const int64_t y : sub_y_range) {
      for (const int64_t x : IndexRange(size.x)) {
        /* The tracking distortion functions expect the coordinates to be in the space of the image
         * where the tracking camera was calibrated. So we first remap the coordinates into that
         * space, apply the distortion, then remap back to the original coordinates space. This is
         * done by dividing the by the size then multiplying by the calibration size, making sure
         * to add 0.5 to evaluate at the center of pixels. */
        float2 coordinates = ((float2(x, y) + 0.5f) / float2(size)) * float2(calibration_size);

        if (type == DistortionType::Undistort) {
          BKE_tracking_distortion_undistort_v2(distortion, coordinates, coordinates);
        }
        else {
          BKE_tracking_distortion_distort_v2(distortion, coordinates, coordinates);
        }

        /* Note that we should remap the coordinates back into the original size by dividing by the
         * calibration size and multiplying by the size, however, we skip the latter to store the
         * coordinates in normalized form, since this is what the shader expects. */
        distortion_grid[y * size.x + x] = coordinates / float2(calibration_size);
      }
    }
  });

  BKE_tracking_distortion_free(distortion);

  texture_ = GPU_texture_create_2d(
      "Distortion Grid",
      size.x,
      size.y,
      1,
      Result::texture_format(ResultType::Float2, context.get_precision()),
      GPU_TEXTURE_USAGE_SHADER_READ,
      *distortion_grid.data());
}

DistortionGrid::~DistortionGrid()
{
  GPU_texture_free(texture_);
}

void DistortionGrid::bind_as_texture(GPUShader *shader, const char *texture_name) const
{
  const int texture_image_unit = GPU_shader_get_sampler_binding(shader, texture_name);
  GPU_texture_bind(texture_, texture_image_unit);
}

void DistortionGrid::unbind_as_texture() const
{
  GPU_texture_unbind(texture_);
}

/* --------------------------------------------------------------------
 * Distortion Grid Container.
 */

void DistortionGridContainer::reset()
{
  /* First, delete all resources that are no longer needed. */
  map_.remove_if([](auto item) { return !item.value->needed; });

  /* Second, reset the needed status of the remaining resources to false to ready them to track
   * their needed status for the next evaluation. */
  for (auto &value : map_.values()) {
    value->needed = false;
  }
}

static int2 get_movie_clip_size(MovieClip *movie_clip, int frame_number)
{
  MovieClipUser user = *DNA_struct_default_get(MovieClipUser);
  BKE_movieclip_user_set_frame(&user, frame_number);

  int2 size;
  BKE_movieclip_get_size(movie_clip, &user, &size.x, &size.y);

  return size;
}

DistortionGrid &DistortionGridContainer::get(
    Context &context, MovieClip *movie_clip, int2 size, DistortionType type, int frame_number)
{
  const int2 calibration_size = get_movie_clip_size(movie_clip, frame_number);

  const DistortionGridKey key(movie_clip->tracking.camera, size, type, calibration_size);

  auto &distortion_grid = *map_.lookup_or_add_cb(key, [&]() {
    return std::make_unique<DistortionGrid>(context, movie_clip, size, type, calibration_size);
  });

  distortion_grid.needed = true;
  return distortion_grid;
}

}  // namespace blender::realtime_compositor
