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

#include "libmv/image/convolve.h"

#include <cmath>

#include "libmv/image/image.h"

namespace libmv {

// Compute a Gaussian kernel and derivative, such that you can take the
// derivative of an image by convolving with the kernel horizontally then the
// derivative vertically to get (eg) the y derivative.
void ComputeGaussianKernel(double sigma, Vec *kernel, Vec *derivative) {
  assert(sigma >= 0.0);

  // 0.004 implies a 3 pixel kernel with 1 pixel sigma.
  const float truncation_factor = 0.004f;

  // Calculate the kernel size based on sigma such that it is odd.
  float precisehalfwidth = GaussianInversePositive(truncation_factor, sigma);
  int width = lround(2*precisehalfwidth);
  if (width % 2 == 0) {
    width++;
  }
  // Calculate the gaussian kernel and its derivative.
  kernel->resize(width);
  derivative->resize(width);
  kernel->setZero();
  derivative->setZero();
  int halfwidth = width / 2;
  for (int i = -halfwidth; i <= halfwidth; ++i)  {
    (*kernel)(i + halfwidth) = Gaussian(i, sigma);
    (*derivative)(i + halfwidth) = GaussianDerivative(i, sigma);
  }
  // Since images should not get brighter or darker, normalize.
  NormalizeL1(kernel);

  // Normalize the derivative differently. See
  // www.cs.duke.edu/courses/spring03/cps296.1/handouts/Image%20Processing.pdf
  double factor = 0.;
  for (int i = -halfwidth; i <= halfwidth; ++i)  {
    factor -= i*(*derivative)(i+halfwidth);
  }
  *derivative /= factor;
}

template <int size, bool vertical>
void FastConvolve(const Vec &kernel, int width, int height,
                  const float* src, int src_stride, int src_line_stride,
                  float* dst, int dst_stride) {
  double coefficients[2 * size + 1];
  for (int k = 0; k < 2 * size + 1; ++k) {
    coefficients[k] = kernel(2 * size - k);
  }
  // Fast path: if the kernel has a certain size, use the constant sized loops.
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      double sum = 0;
      for (int k = -size; k <= size; ++k) {
        if (vertical) {
          if (y + k >= 0 && y + k < height) {
            sum += src[k * src_line_stride] * coefficients[k + size];
          }
        } else {
          if (x + k >= 0 && x + k < width) {
            sum += src[k * src_stride] * coefficients[k + size];
          }
        }
      }
      dst[0] = static_cast<float>(sum);
      src += src_stride;
      dst += dst_stride;
    }
  }
}

template<bool vertical>
void Convolve(const Array3Df &in,
              const Vec &kernel,
              Array3Df *out_pointer,
              int plane) {
  int width = in.Width();
  int height = in.Height();
  Array3Df &out = *out_pointer;
  if (plane == -1) {
    out.ResizeLike(in);
    plane = 0;
  }

  assert(kernel.size() % 2 == 1);
  assert(&in != out_pointer);

  int src_line_stride = in.Stride(0);
  int src_stride = in.Stride(1);
  int dst_stride = out.Stride(1);
  const float* src = in.Data();
  float* dst = out.Data() + plane;

  // Use a dispatch table to make most convolutions used in practice use the
  // fast path.
  int half_width = kernel.size() / 2;
  switch (half_width) {
#define static_convolution(size) case size: \
  FastConvolve<size, vertical>(kernel, width, height, src, src_stride, \
                               src_line_stride, dst, dst_stride); break;
    static_convolution(1)
    static_convolution(2)
    static_convolution(3)
    static_convolution(4)
    static_convolution(5)
    static_convolution(6)
    static_convolution(7)
#undef static_convolution
    default:
      int dynamic_size = kernel.size() / 2;
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          double sum = 0;
          // Slow path: this loop cannot be unrolled.
          for (int k = -dynamic_size; k <= dynamic_size; ++k) {
            if (vertical) {
              if (y + k >= 0 && y + k < height) {
                sum += src[k * src_line_stride] *
                    kernel(2 * dynamic_size - (k + dynamic_size));
              }
            } else {
              if (x + k >= 0 && x + k < width) {
                sum += src[k * src_stride] *
                    kernel(2 * dynamic_size - (k + dynamic_size));
              }
            }
          }
          dst[0] = static_cast<float>(sum);
          src += src_stride;
          dst += dst_stride;
        }
      }
  }
}

void ConvolveHorizontal(const Array3Df &in,
                        const Vec &kernel,
                        Array3Df *out_pointer,
                        int plane) {
  Convolve<false>(in, kernel, out_pointer, plane);
}

void ConvolveVertical(const Array3Df &in,
                      const Vec &kernel,
                      Array3Df *out_pointer,
                      int plane) {
  Convolve<true>(in, kernel, out_pointer, plane);
}

void ConvolveGaussian(const Array3Df &in,
                      double sigma,
                      Array3Df *out_pointer) {
  Vec kernel, derivative;
  ComputeGaussianKernel(sigma, &kernel, &derivative);

  Array3Df tmp;
  ConvolveVertical(in, kernel, &tmp);
  ConvolveHorizontal(tmp, kernel, out_pointer);
}

void ImageDerivatives(const Array3Df &in,
                      double sigma,
                      Array3Df *gradient_x,
                      Array3Df *gradient_y) {
  Vec kernel, derivative;
  ComputeGaussianKernel(sigma, &kernel, &derivative);
  Array3Df tmp;

  // Compute first derivative in x.
  ConvolveVertical(in, kernel, &tmp);
  ConvolveHorizontal(tmp, derivative, gradient_x);

  // Compute first derivative in y.
  ConvolveHorizontal(in, kernel, &tmp);
  ConvolveVertical(tmp, derivative, gradient_y);
}

void BlurredImageAndDerivatives(const Array3Df &in,
                                double sigma,
                                Array3Df *blurred_image,
                                Array3Df *gradient_x,
                                Array3Df *gradient_y) {
  Vec kernel, derivative;
  ComputeGaussianKernel(sigma, &kernel, &derivative);
  Array3Df tmp;

  // Compute convolved image.
  ConvolveVertical(in, kernel, &tmp);
  ConvolveHorizontal(tmp, kernel, blurred_image);

  // Compute first derivative in x (reusing vertical convolution above).
  ConvolveHorizontal(tmp, derivative, gradient_x);

  // Compute first derivative in y.
  ConvolveHorizontal(in, kernel, &tmp);
  ConvolveVertical(tmp, derivative, gradient_y);
}

// Compute the gaussian blur of an image and the derivatives of the blurred
// image, and store the results in three channels. Since the blurred value and
// gradients are closer in memory, this leads to better performance if all
// three values are needed at the same time.
void BlurredImageAndDerivativesChannels(const Array3Df &in,
                                        double sigma,
                                        Array3Df *blurred_and_gradxy) {
  assert(in.Depth() == 1);

  Vec kernel, derivative;
  ComputeGaussianKernel(sigma, &kernel, &derivative);

  // Compute convolved image.
  Array3Df tmp;
  ConvolveVertical(in, kernel, &tmp);
  blurred_and_gradxy->Resize(in.Height(), in.Width(), 3);
  ConvolveHorizontal(tmp, kernel, blurred_and_gradxy, 0);

  // Compute first derivative in x.
  ConvolveHorizontal(tmp, derivative, blurred_and_gradxy, 1);

  // Compute first derivative in y.
  ConvolveHorizontal(in, kernel, &tmp);
  ConvolveVertical(tmp, derivative, blurred_and_gradxy, 2);
}

void BoxFilterHorizontal(const Array3Df &in,
                         int window_size,
                         Array3Df *out_pointer) {
  Array3Df &out = *out_pointer;
  out.ResizeLike(in);
  int half_width = (window_size - 1) / 2;

  for (int k = 0; k < in.Depth(); ++k) {
    for (int i = 0; i < in.Height(); ++i) {
      float sum = 0;
      // Init sum.
      for (int j = 0; j < half_width; ++j) {
        sum += in(i, j, k);
      }
      // Fill left border.
      for (int j = 0; j < half_width + 1; ++j) {
        sum += in(i, j + half_width, k);
        out(i, j, k) = sum;
      }
      // Fill interior.
      for (int j = half_width + 1; j < in.Width()-half_width; ++j) {
        sum -= in(i, j - half_width - 1, k);
        sum += in(i, j + half_width, k);
        out(i, j, k) = sum;
      }
      // Fill right border.
      for (int j = in.Width() - half_width; j < in.Width(); ++j) {
        sum -= in(i, j - half_width - 1, k);
        out(i, j, k) = sum;
      }
    }
  }
}

void BoxFilterVertical(const Array3Df &in,
                       int window_size,
                       Array3Df *out_pointer) {
  Array3Df &out = *out_pointer;
  out.ResizeLike(in);
  int half_width = (window_size - 1) / 2;

  for (int k = 0; k < in.Depth(); ++k) {
    for (int j = 0; j < in.Width(); ++j) {
      float sum = 0;
      // Init sum.
      for (int i = 0; i < half_width; ++i) {
        sum += in(i, j, k);
      }
      // Fill left border.
      for (int i = 0; i < half_width + 1; ++i) {
        sum += in(i + half_width, j, k);
        out(i, j, k) = sum;
      }
      // Fill interior.
      for (int i = half_width + 1; i < in.Height()-half_width; ++i) {
        sum -= in(i - half_width - 1, j, k);
        sum += in(i + half_width, j, k);
        out(i, j, k) = sum;
      }
      // Fill right border.
      for (int i = in.Height() - half_width; i < in.Height(); ++i) {
        sum -= in(i - half_width - 1, j, k);
        out(i, j, k) = sum;
      }
    }
  }
}

void BoxFilter(const Array3Df &in,
               int box_width,
               Array3Df *out) {
  Array3Df tmp;
  BoxFilterHorizontal(in, box_width, &tmp);
  BoxFilterVertical(tmp, box_width, out);
}

void LaplaceFilter(unsigned char* src,
                   unsigned char* dst,
                   int width,
                   int height,
                   int strength) {
  for (int y = 1; y < height-1; y++)
    for (int x = 1; x < width-1; x++) {
      const unsigned char* s = &src[y*width+x];
      int l = 128 +
          s[-width-1] + s[-width] + s[-width+1] +
          s[1]        - 8*s[0]    + s[1]        +
          s[ width-1] + s[ width] + s[ width+1];
      int d = ((256-strength)*s[0] + strength*l) / 256;
      if (d < 0) d=0;
      if (d > 255) d=255;
      dst[y*width+x] = d;
    }
}

}  // namespace libmv
