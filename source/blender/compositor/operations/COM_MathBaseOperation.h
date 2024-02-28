/* SPDX-FileCopyrightText: 2011 Blender Authors
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
  bool use_clamp_;

 protected:
  MathBaseOperation();

  float clamp_when_enabled(float value)
  {
    if (use_clamp_) {
      return std::clamp(value, 0.0f, 1.0f);
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

class MathAddOperation : public MathFunctor2Operation<std::plus> {};
class MathSubtractOperation : public MathFunctor2Operation<std::minus> {};
class MathMultiplyOperation : public MathFunctor2Operation<std::multiplies> {};
class MathDivideOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathSineOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathCosineOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathTangentOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathHyperbolicSineOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathHyperbolicCosineOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathHyperbolicTangentOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathArcSineOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathArcCosineOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathArcTangentOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathPowerOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathLogarithmOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathMinimumOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathMaximumOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathRoundOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};
class MathLessThanOperation : public MathFunctor2Operation<std::less> {};
class MathGreaterThanOperation : public MathFunctor2Operation<std::greater> {};

class MathModuloOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathFlooredModuloOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathAbsoluteOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathRadiansOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathDegreesOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathArcTan2Operation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathFloorOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathCeilOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathFractOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathSqrtOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathInverseSqrtOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathSignOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathExponentOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathTruncOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathSnapOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathWrapOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathPingpongOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathCompareOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathMultiplyAddOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathSmoothMinOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

class MathSmoothMaxOperation : public MathBaseOperation {
 protected:
  void update_memory_buffer_partial(BuffersIterator<float> &it) override;
};

}  // namespace blender::compositor
