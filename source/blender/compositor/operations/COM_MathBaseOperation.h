/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

/**
 * this program converts an input color to an output value.
 * it assumes we are in sRGB color space.
 */
class MathBaseOperation : public MultiThreadedOperation {
 protected:
  /**
   * Prefetched reference to the input_program
   */
  SocketReader *input_value1_operation_;
  SocketReader *input_value2_operation_;
  SocketReader *input_value3_operation_;

  bool use_clamp_;

 protected:
  /**
   * Default constructor
   */
  MathBaseOperation();

  /* TODO(manzanilla): to be removed with tiled implementation. */
  void clamp_if_needed(float color[4]);

  float clamp_when_enabled(float value)
  {
    if (use_clamp_) {
      return CLAMPIS(value, 0.0f, 1.0f);
    }
    return value;
  }

  void clamp_when_enabled(float *out)
  {
    if (use_clamp_) {
      CLAMP(*out, 0.0f, 1.0f);
    }
  }

 public:
  /**
   * Initialize the execution
   */
  void init_execution() override;

  /**
   * Deinitialize the execution
   */
  void deinit_execution() override;

  /**
   * Determine resolution
   */
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  void set_use_clamp(bool value)
  {
    use_clamp_ = value;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) final;

 protected:
  virtual void update_memory_buffer_partial(BuffersIterator<float> &it) = 0;
};

template<template<typename> typename TFunctor>
class MathFunctor2Operation : public MathBaseOperation {
  void update_memory_buffer_partial(BuffersIterator<float> &it) final
  {
    TFunctor functor;
    for (; !it.is_end(); ++it) {
      *it.out = functor(*it.in(0), *it.in(1));
      clamp_when_enabled(it.out);
    }
  }
};

class MathAddOperation : public MathFunctor2Operation<std::plus> {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;
};
class MathSubtractOperation : public MathFunctor2Operation<std::minus> {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;
};
class MathMultiplyOperation : public MathFunctor2Operation<std::multiplies> {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;
};
class MathDivideOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathSineOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathCosineOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathTangentOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathHyperbolicSineOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathHyperbolicCosineOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathHyperbolicTangentOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathArcSineOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathArcCosineOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathArcTangentOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathPowerOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathLogarithmOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathMinimumOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathMaximumOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathRoundOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathLessThanOperation : public MathFunctor2Operation<std::less> {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;
};
class MathGreaterThanOperation : public MathFunctor2Operation<std::greater> {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;
};

class MathModuloOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathFlooredModuloOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathAbsoluteOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathRadiansOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathDegreesOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathArcTan2Operation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathFloorOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathCeilOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathFractOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathSqrtOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathInverseSqrtOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathSignOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathExponentOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathTruncOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathSnapOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathWrapOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathPingpongOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathCompareOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathMultiplyAddOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathSmoothMinOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathSmoothMaxOperation : public MathBaseOperation {
 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

}  // namespace blender::compositor
