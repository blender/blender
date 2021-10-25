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

#ifndef LIBMV_IMAGE_SAMPLE_H_
#define LIBMV_IMAGE_SAMPLE_H_

#include "libmv/image/image.h"

namespace libmv {

/// Nearest neighbor interpolation.
template<typename T>
inline T SampleNearest(const Array3D<T> &image,
                       float y, float x, int v = 0) {
  const int i = int(round(y));
  const int j = int(round(x));
  return image(i, j, v);
}

inline void LinearInitAxis(float x, int size,
                           int *x1, int *x2,
                           float *dx) {
  const int ix = static_cast<int>(x);
  if (ix < 0) {
    *x1 = 0;
    *x2 = 0;
    *dx = 1.0;
  } else if (ix > size - 2) {
    *x1 = size - 1;
    *x2 = size - 1;
    *dx = 1.0;
  } else {
    *x1 = ix;
    *x2 = ix + 1;
    *dx = *x2 - x;
  }
}

/// Linear interpolation.
template<typename T>
inline T SampleLinear(const Array3D<T> &image, float y, float x, int v = 0) {
  int x1, y1, x2, y2;
  float dx, dy;

  LinearInitAxis(y, image.Height(), &y1, &y2, &dy);
  LinearInitAxis(x, image.Width(),  &x1, &x2, &dx);

  const T im11 = image(y1, x1, v);
  const T im12 = image(y1, x2, v);
  const T im21 = image(y2, x1, v);
  const T im22 = image(y2, x2, v);

  return T(     dy  * (dx * im11 + (1.0 - dx) * im12) +
           (1 - dy) * (dx * im21 + (1.0 - dx) * im22));
}

/// Linear interpolation, of all channels. The sample is assumed to have the
/// same size as the number of channels in image.
template<typename T>
inline void SampleLinear(const Array3D<T> &image, float y, float x, T *sample) {
  int x1, y1, x2, y2;
  float dx, dy;

  LinearInitAxis(y, image.Height(), &y1, &y2, &dy);
  LinearInitAxis(x, image.Width(),  &x1, &x2, &dx);

  for (int i = 0; i < image.Depth(); ++i) {
    const T im11 = image(y1, x1, i);
    const T im12 = image(y1, x2, i);
    const T im21 = image(y2, x1, i);
    const T im22 = image(y2, x2, i);

    sample[i] = T(     dy  * (dx * im11 + (1.0 - dx) * im12) +
                  (1 - dy) * (dx * im21 + (1.0 - dx) * im22));
  }
}

// Downsample all channels by 2. If the image has odd width or height, the last
// row or column is ignored.
// FIXME(MatthiasF): this implementation shouldn't be in an interface file
inline void DownsampleChannelsBy2(const Array3Df &in, Array3Df *out) {
  int height = in.Height() / 2;
  int width = in.Width() / 2;
  int depth = in.Depth();

  out->Resize(height, width, depth);

  // 2x2 box filter downsampling.
  for (int r = 0; r < height; ++r) {
    for (int c = 0; c < width; ++c) {
      for (int k = 0; k < depth; ++k) {
        (*out)(r, c, k) = (in(2 * r,     2 * c,     k) +
                           in(2 * r + 1, 2 * c,     k) +
                           in(2 * r,     2 * c + 1, k) +
                           in(2 * r + 1, 2 * c + 1, k)) / 4.0f;
      }
    }
  }
}

// Sample a region centered at x,y in image with size extending by half_width
// from x,y. Channels specifies the number of channels to sample from.
inline void SamplePattern(const FloatImage &image,
                   double x, double y,
                   int half_width,
                   int channels,
                   FloatImage *sampled) {
  sampled->Resize(2 * half_width + 1, 2 * half_width + 1, channels);
  for (int r = -half_width; r <= half_width; ++r) {
    for (int c = -half_width; c <= half_width; ++c) {
      for (int i = 0; i < channels; ++i) {
        (*sampled)(r + half_width, c + half_width, i) =
            SampleLinear(image, y + r, x + c, i);
      }
    }
  }
}

}  // namespace libmv

#endif  // LIBMV_IMAGE_SAMPLE_H_
