// Copyright (c) 2020 libmv authors.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#ifndef LIBMV_SIMPLE_PIPELINE_PACKED_INTRINSICS_H_
#define LIBMV_SIMPLE_PIPELINE_PACKED_INTRINSICS_H_

#include "libmv/base/array.h"

namespace libmv {

// Intrinsics parameters packed into a single continuous block of memory.
// Used in cases like minimization problems which involves camera intrinsics
// as a minimizing parameters.
//
// It keeps track of which parameters has been specified explicitly, which
// allows to mark parameters which are not used by distortion model as constant,
// which improves minimization quality.
class PackedIntrinsics {
 public:
  // Offsets of corresponding parameters in the array of all parameters.
  enum {
    // Camera calibration values.
    OFFSET_FOCAL_LENGTH,
    OFFSET_PRINCIPAL_POINT_X,
    OFFSET_PRINCIPAL_POINT_Y,
  
    // Distortion model coefficients.
    OFFSET_K1,
    OFFSET_K2,
    OFFSET_K3,
    OFFSET_K4,
    OFFSET_P1,
    OFFSET_P2,
  
    // Number of parameters which are to be stored in the block.
    NUM_PARAMETERS,
  };

  PackedIntrinsics();

  void SetFocalLength(double focal_length);
  double GetFocalLength() const;

  void SetPrincipalPoint(double x, double y);
  double GetPrincipalPointX() const;
  double GetPrincipalPointY() const;

  // TODO(sergey): Consider adding vectorized (Vec2) accessors for the principal
  // point.

#define DEFINE_PARAMETER(parameter_name)                                       \
  void Set ## parameter_name(double value) {                                   \
    SetParameter(OFFSET_ ## parameter_name, value);                            \
  }                                                                            \
  double Get ## parameter_name() const {                                       \
    return GetParameter(OFFSET_ ## parameter_name);                            \
  }                                                                            \

  DEFINE_PARAMETER(K1)
  DEFINE_PARAMETER(K2)
  DEFINE_PARAMETER(K3)
  DEFINE_PARAMETER(K4)

  DEFINE_PARAMETER(P1)
  DEFINE_PARAMETER(P2)

#undef DEFINE_PARAMETER

  double* GetParametersBlock() { return parameters_.data(); }
  const double* GetParametersBlock() const { return parameters_.data(); }

  bool IsParameterDefined(int offset);

 private:
  void SetParameter(int index, double value);
  double GetParameter(int index) const;

  // All intrinsics parameters packed into a single block.
  // Use OFFSET_FOO indexes to access corresponding values.
  array<double,  NUM_PARAMETERS> parameters_;

  // Indexed by parameter offset, set to truth if the value of the parameter is
  // explicitly specified.
  array<bool,  NUM_PARAMETERS> known_parameters_;
};

}  // namespace libmv

#endif  // LIBMV_SIMPLE_PIPELINE_PACKED_INTRINSICS_H_
