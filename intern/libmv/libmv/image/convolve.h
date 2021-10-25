// Copyright (c) 2007, 2008, 2011 libmv authors.
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

#ifndef LIBMV_IMAGE_CONVOLVE_H_
#define LIBMV_IMAGE_CONVOLVE_H_

#include "libmv/image/image.h"
#include "libmv/numeric/numeric.h"

namespace libmv {

// TODO(keir): Find a better place for these functions. gaussian.h in numeric?

// Zero mean Gaussian.
inline double Gaussian(double x, double sigma) {
  return 1/sqrt(2*M_PI*sigma*sigma) * exp(-(x*x/2/sigma/sigma));
}
// 2D gaussian (zero mean)
// (9) in http://mathworld.wolfram.com/GaussianFunction.html
inline double Gaussian2D(double x, double y, double sigma) {
  return 1.0/(2.0*M_PI*sigma*sigma) * exp( -(x*x+y*y)/(2.0*sigma*sigma));
}
inline double GaussianDerivative(double x, double sigma) {
  return -x / sigma / sigma * Gaussian(x, sigma);
}
// Solve the inverse of the Gaussian for positive x.
inline double GaussianInversePositive(double y, double sigma) {
  return sqrt(-2 * sigma * sigma * log(y * sigma * sqrt(2*M_PI)));
}

void ComputeGaussianKernel(double sigma, Vec *kernel, Vec *derivative);
void ConvolveHorizontal(const FloatImage &in,
                        const Vec &kernel,
                        FloatImage *out_pointer,
                        int plane = -1);
void ConvolveVertical(const FloatImage &in,
                      const Vec &kernel,
                      FloatImage *out_pointer,
                      int plane = -1);
void ConvolveGaussian(const FloatImage &in,
                      double sigma,
                      FloatImage *out_pointer);

void ImageDerivatives(const FloatImage &in,
                      double sigma,
                      FloatImage *gradient_x,
                      FloatImage *gradient_y);

void BlurredImageAndDerivatives(const FloatImage &in,
                                double sigma,
                                FloatImage *blurred_image,
                                FloatImage *gradient_x,
                                FloatImage *gradient_y);

// Blur and take the gradients of an image, storing the results inside the
// three channels of blurred_and_gradxy.
void BlurredImageAndDerivativesChannels(const FloatImage &in,
                                        double sigma,
                                        FloatImage *blurred_and_gradxy);

void BoxFilterHorizontal(const FloatImage &in,
                         int window_size,
                         FloatImage *out_pointer);

void BoxFilterVertical(const FloatImage &in,
                       int window_size,
                       FloatImage *out_pointer);

void BoxFilter(const FloatImage &in,
               int box_width,
               FloatImage *out);

/*!
    Convolve \a src into \a dst with the discrete laplacian operator.

    \a src and \a dst should be \a width x \a height images.
    \a strength is an interpolation coefficient (0-256) between original image and the laplacian.

    \note Make sure the search region is filtered with the same strength as the pattern.
*/
void LaplaceFilter(unsigned char* src,
                   unsigned char* dst,
                   int width,
                   int height,
                   int strength);

}  // namespace libmv

#endif  // LIBMV_IMAGE_CONVOLVE_H_

