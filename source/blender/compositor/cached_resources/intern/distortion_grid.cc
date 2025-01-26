/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <memory>

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_math_vector_types.hh"

#include "DNA_defaults.h"
#include "DNA_movieclip_types.h"
#include "DNA_tracking_types.h"

#include "GPU_texture.hh"

#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "COM_context.hh"
#include "COM_distortion_grid.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

/* --------------------------------------------------------------------
 * Distortion Grid Key.
 */

DistortionGridKey::DistortionGridKey(const MovieTrackingCamera &camera,
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
    : result(context.create_result(ResultType::Float2))
{
  MovieDistortion *distortion = BKE_tracking_distortion_new(
      &movie_clip->tracking, calibration_size.x, calibration_size.y);

  /* Compute how much the image would extend outside each of its bounds due to distortion. */
  int right_delta;
  int left_delta;
  int bottom_delta;
  int top_delta;
  BKE_tracking_distortion_bounds_deltas(&movie_clip->tracking,
                                        size,
                                        type == DistortionType::Undistort,
                                        &right_delta,
                                        &left_delta,
                                        &bottom_delta,
                                        &top_delta);

  /* Extend the size by the deltas of the bounds. */
  const int2 extended_size = size + int2(right_delta + left_delta, bottom_delta + top_delta);

  distortion_grid_ = Array<float2>(extended_size.x * extended_size.y);
  parallel_for(extended_size, [&](const int2 texel) {
    /* The tracking distortion functions expect the coordinates to be in the space of the image
     * where the tracking camera was calibrated. So we first remap the coordinates into that space,
     * apply the distortion, then remap back to the original coordinates space. This is done by
     * dividing the by the size then multiplying by the calibration size, making sure to add 0.5 to
     * evaluate at the center of pixels.
     *
     * Subtract the lower left bounds delta since we are looping over the extended domain. */
    float2 coordinates = ((float2(texel - int2(left_delta, bottom_delta)) + 0.5f) / float2(size)) *
                         float2(calibration_size);

    if (type == DistortionType::Undistort) {
      BKE_tracking_distortion_undistort_v2(distortion, coordinates, coordinates);
    }
    else {
      BKE_tracking_distortion_distort_v2(distortion, coordinates, coordinates);
    }

    /* Note that we should remap the coordinates back into the original size by dividing by the
     * calibration size and multiplying by the size, however, we skip the latter to store the
     * coordinates in normalized form, since this is what the shader expects. */
    distortion_grid_[texel.y * extended_size.x + texel.x] = coordinates / float2(calibration_size);
  });

  BKE_tracking_distortion_free(distortion);

  if (context.use_gpu()) {
    this->result.allocate_texture(Domain(extended_size), false);
    GPU_texture_update(this->result, GPU_DATA_FLOAT, distortion_grid_.data());

    /* CPU-side data no longer needed, so free it. */
    distortion_grid_ = Array<float2>();
  }
  else {
    this->result.wrap_external(&distortion_grid_[0].x, extended_size);
  }
}

DistortionGrid::~DistortionGrid()
{
  this->result.release();
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

Result &DistortionGridContainer::get(
    Context &context, MovieClip *movie_clip, int2 size, DistortionType type, int frame_number)
{
  const int2 calibration_size = get_movie_clip_size(movie_clip, frame_number);

  const DistortionGridKey key(movie_clip->tracking.camera, size, type, calibration_size);

  auto &distortion_grid = *map_.lookup_or_add_cb(key, [&]() {
    return std::make_unique<DistortionGrid>(context, movie_clip, size, type, calibration_size);
  });

  distortion_grid.needed = true;
  return distortion_grid.result;
}

}  // namespace blender::compositor
