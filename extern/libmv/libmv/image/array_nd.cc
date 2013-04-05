// Copyright (c) 2007, 2008 libmv authors.
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

#include "libmv/image/image.h"
#include <iostream>
#include <cmath>

namespace libmv {

void FloatArrayToScaledByteArray(const Array3Df &float_array,
                                 Array3Du *byte_array,
                                 bool automatic_range_detection
                                ) {
  byte_array->ResizeLike(float_array);
  float minval =  HUGE_VAL;
  float maxval = -HUGE_VAL;
  if (automatic_range_detection) {
    for (int i = 0; i < float_array.Height(); ++i) {
      for (int j = 0; j < float_array.Width(); ++j) {
        for (int k = 0; k < float_array.Depth(); ++k) {
          minval = std::min(minval, float_array(i, j, k));
          maxval = std::max(maxval, float_array(i, j, k));
        }
      }
    }
  } else {
    minval = 0;
    maxval = 1;
  }
  for (int i = 0; i < float_array.Height(); ++i) {
    for (int j = 0; j < float_array.Width(); ++j) {
      for (int k = 0; k < float_array.Depth(); ++k) {
        float unscaled = (float_array(i, j, k) - minval) / (maxval - minval);
        (*byte_array)(i, j, k) = (unsigned char)(255 * unscaled);
      }
    }
  }
}

void ByteArrayToScaledFloatArray(const Array3Du &byte_array,
                                 Array3Df *float_array) {
  float_array->ResizeLike(byte_array);
  for (int i = 0; i < byte_array.Height(); ++i) {
    for (int j = 0; j < byte_array.Width(); ++j) {
      for (int k = 0; k < byte_array.Depth(); ++k) {
        (*float_array)(i, j, k) = float(byte_array(i, j, k)) / 255.0f;
      }
    }
  }
}

void SplitChannels(const Array3Df &input,
                          Array3Df *channel0,
                          Array3Df *channel1,
                          Array3Df *channel2) {
  assert(input.Depth() >= 3);
  channel0->Resize(input.Height(), input.Width());
  channel1->Resize(input.Height(), input.Width());
  channel2->Resize(input.Height(), input.Width());
  for (int row = 0; row < input.Height(); ++row) {
    for (int column = 0; column < input.Width(); ++column) {
      (*channel0)(row, column) = input(row, column, 0);
      (*channel1)(row, column) = input(row, column, 1);
      (*channel2)(row, column) = input(row, column, 2);
    }
  }
}

void PrintArray(const Array3Df &array) {
  using namespace std;

  printf("[\n");
  for (int r = 0; r < array.Height(); ++r) {
    printf("[");
    for (int c = 0; c < array.Width(); ++c) {
      if (array.Depth() == 1) {
        printf("%11f, ", array(r, c));
      } else {
        printf("[");
        for (int k = 0; k < array.Depth(); ++k) {
          printf("%11f, ", array(r, c, k));
        }
        printf("],");
      }
    }
    printf("],\n");
  }
  printf("]\n");
}

}  // namespace libmv
