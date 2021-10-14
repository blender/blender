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

namespace blender::compositor {

/**
 * All this programs converts an input color to an output value.
 * it assumes we are in sRGB color space.
 */

class MixBaseOperation : public MultiThreadedOperation {
 protected:
  struct PixelCursor {
    float *out;
    const float *row_end;
    const float *value;
    const float *color1;
    const float *color2;
    int out_stride;
    int value_stride;
    int color1_stride;
    int color2_stride;

    void next()
    {
      BLI_assert(out < row_end);
      out += out_stride;
      value += value_stride;
      color1 += color1_stride;
      color2 += color2_stride;
    }
  };

  /**
   * Prefetched reference to the input_program
   */
  SocketReader *input_value_operation_;
  SocketReader *input_color1_operation_;
  SocketReader *input_color2_operation_;
  bool value_alpha_multiply_;
  bool use_clamp_;

  inline void clamp_if_needed(float color[4])
  {
    if (use_clamp_) {
      clamp_v4(color, 0.0f, 1.0f);
    }
  }

 public:
  /**
   * Default constructor
   */
  MixBaseOperation();

  /**
   * The inner loop of this operation.
   */
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  /**
   * Initialize the execution
   */
  void init_execution() override;

  /**
   * Deinitialize the execution
   */
  void deinit_execution() override;

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  void set_use_value_alpha_multiply(const bool value)
  {
    value_alpha_multiply_ = value;
  }
  inline bool use_value_alpha_multiply()
  {
    return value_alpha_multiply_;
  }
  void set_use_clamp(bool value)
  {
    use_clamp_ = value;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) final;

 protected:
  virtual void update_memory_buffer_row(PixelCursor &p);
};

class MixAddOperation : public MixBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixBlendOperation : public MixBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixColorBurnOperation : public MixBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixColorOperation : public MixBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixDarkenOperation : public MixBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixDifferenceOperation : public MixBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixDivideOperation : public MixBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixDodgeOperation : public MixBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixGlareOperation : public MixBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixHueOperation : public MixBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixLightenOperation : public MixBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixLinearLightOperation : public MixBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixMultiplyOperation : public MixBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixOverlayOperation : public MixBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixSaturationOperation : public MixBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixScreenOperation : public MixBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixSoftLightOperation : public MixBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixSubtractOperation : public MixBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixValueOperation : public MixBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

}  // namespace blender::compositor
