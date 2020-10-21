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

#include "libmv/simple_pipeline/packed_intrinsics.h"

namespace libmv {

PackedIntrinsics::PackedIntrinsics() {
  parameters_.fill(0.0);
  known_parameters_.fill(false);
}

void PackedIntrinsics::SetFocalLength(double focal_length) {
  SetParameter(OFFSET_FOCAL_LENGTH, focal_length);
}
double PackedIntrinsics::GetFocalLength() const {
  return GetParameter(OFFSET_FOCAL_LENGTH);
}

void PackedIntrinsics::SetPrincipalPoint(double x, double y) {
  SetParameter(OFFSET_PRINCIPAL_POINT_X, x);
  SetParameter(OFFSET_PRINCIPAL_POINT_Y, y);
}
double PackedIntrinsics::GetPrincipalPointX() const {
  return GetParameter(OFFSET_PRINCIPAL_POINT_X);
}
double PackedIntrinsics::GetPrincipalPointY() const {
  return GetParameter(OFFSET_PRINCIPAL_POINT_Y);
}

void PackedIntrinsics::SetParameter(int index, double value) {
  parameters_.at(index) = value;
  known_parameters_.at(index) = true;
}
double PackedIntrinsics::GetParameter(int index) const {
  // TODO(sergey): Consider adding a check for whether the parameter is known.

  return parameters_.at(index);
}

bool PackedIntrinsics::IsParameterDefined(int offset) {
  return known_parameters_.at(offset);
}

}  // namespace libmv
