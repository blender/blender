/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2013, Blender Foundation.
 */

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
    float frameSpaceCorners[4][2]; /* Corners coordinates in pixel space. */
    float perspectiveMatrix[3][3];
  };
  MotionSample m_samples[PLANE_DISTORT_MAX_SAMPLES];
  int m_motion_blur_samples;
  float m_motion_blur_shutter;

 public:
  PlaneDistortBaseOperation();

  void setMotionBlurSamples(int samples)
  {
    BLI_assert(samples <= PLANE_DISTORT_MAX_SAMPLES);
    m_motion_blur_samples = samples;
  }
  void setMotionBlurShutter(float shutter)
  {
    m_motion_blur_shutter = shutter;
  }

  virtual void calculateCorners(const float corners[4][2], bool normalized, int sample);

 private:
  friend class PlaneTrackCommon;
};

class PlaneDistortWarpImageOperation : public PlaneDistortBaseOperation {
 protected:
  SocketReader *m_pixelReader;

 public:
  PlaneDistortWarpImageOperation();

  void calculateCorners(const float corners[4][2], bool normalized, int sample) override;

  void initExecution() override;
  void deinitExecution() override;

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class PlaneDistortMaskOperation : public PlaneDistortBaseOperation {
 protected:
  int m_osa;
  float m_jitter[32][2];

 public:
  PlaneDistortMaskOperation();

  void initExecution() override;

  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

 private:
  int get_jitter_samples_inside_count(int x, int y, MotionSample &sample_data);
};

}  // namespace blender::compositor
