/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string.h>

#include "COM_MultiThreadedOperation.h"

#include "DNA_movieclip_types.h"
#include "DNA_tracking_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

namespace blender::compositor {

#define PLANE_DISTORT_MAX_SAMPLES 64

class PlaneDistortBaseOperation : public MultiThreadedOperation {
 protected:
  struct MotionSample {
    float frame_space_corners[4][2]; /* Corners coordinates in pixel space. */
    float perspective_matrix[3][3];
  };
  MotionSample samples_[PLANE_DISTORT_MAX_SAMPLES];
  int motion_blur_samples_;
  float motion_blur_shutter_;

 public:
  PlaneDistortBaseOperation();

  void set_motion_blur_samples(int samples)
  {
    BLI_assert(samples <= PLANE_DISTORT_MAX_SAMPLES);
    motion_blur_samples_ = samples;
  }
  void set_motion_blur_shutter(float shutter)
  {
    motion_blur_shutter_ = shutter;
  }

  virtual void calculate_corners(const float corners[4][2], bool normalized, int sample);

 private:
  friend class PlaneTrackCommon;
};

class PlaneDistortWarpImageOperation : public PlaneDistortBaseOperation {
 public:
  PlaneDistortWarpImageOperation();

  void calculate_corners(const float corners[4][2], bool normalized, int sample) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class PlaneDistortMaskOperation : public PlaneDistortBaseOperation {
 protected:
  int osa_;
  float jitter_[32][2];

 public:
  PlaneDistortMaskOperation();

  void init_execution() override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

 private:
  int get_jitter_samples_inside_count(int x, int y, MotionSample &sample_data);
};

}  // namespace blender::compositor
