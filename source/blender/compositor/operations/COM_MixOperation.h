/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

  bool value_alpha_multiply_;
  bool use_clamp_;

  inline void clamp_if_needed(float color[4])
  {
    if (use_clamp_) {
      clamp_v4(color, 0.0f, 1.0f);
    }
  }

 public:
  MixBaseOperation();

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
 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixBlendOperation : public MixBaseOperation {
 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixColorBurnOperation : public MixBaseOperation {
 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixColorOperation : public MixBaseOperation {
 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixDarkenOperation : public MixBaseOperation {
 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixDifferenceOperation : public MixBaseOperation {
 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixExclusionOperation : public MixBaseOperation {
 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixDivideOperation : public MixBaseOperation {
 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixDodgeOperation : public MixBaseOperation {
 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixGlareOperation : public MixBaseOperation {
 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixHueOperation : public MixBaseOperation {
 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixLightenOperation : public MixBaseOperation {
 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixLinearLightOperation : public MixBaseOperation {
 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixMultiplyOperation : public MixBaseOperation {
 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixOverlayOperation : public MixBaseOperation {
 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixSaturationOperation : public MixBaseOperation {
 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixScreenOperation : public MixBaseOperation {
 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixSoftLightOperation : public MixBaseOperation {
 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixSubtractOperation : public MixBaseOperation {
 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

class MixValueOperation : public MixBaseOperation {
 protected:
  void update_memory_buffer_row(PixelCursor &p) override;
};

}  // namespace blender::compositor
