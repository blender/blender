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
 * Copyright 2011, Blender Foundation.
 */

#pragma once

#include "COM_MultiThreadedOperation.h"
#include "DNA_movieclip_types.h"
#include "MEM_guardedalloc.h"

#include "BKE_tracking.h"

namespace blender::compositor {

class MovieDistortionOperation : public MultiThreadedOperation {
 private:
  SocketReader *inputOperation_;
  MovieClip *movieClip_;
  int margin_[2];

 protected:
  bool apply_;
  int framenumber_;

  struct MovieDistortion *distortion_;
  int calibration_width_, calibration_height_;
  float pixel_aspect_;

 public:
  MovieDistortionOperation(bool distortion);
  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  void init_data() override;
  void initExecution() override;
  void deinitExecution() override;

  void setMovieClip(MovieClip *clip)
  {
    movieClip_ = clip;
  }
  void setFramenumber(int framenumber)
  {
    framenumber_ = framenumber;
  }
  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
