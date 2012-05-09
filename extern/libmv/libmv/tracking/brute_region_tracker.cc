// Copyright (c) 2011 libmv authors.
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

#include "libmv/tracking/brute_region_tracker.h"

#ifdef __SSE2__
#include <emmintrin.h>
#endif

#if !defined(__APPLE__) && !defined(__FreeBSD__)
// Needed for memalign on Linux and _aligned_alloc on Windows.
#ifdef FREE_WINDOWS
/* make sure _aligned_malloc is included */
#ifdef __MSVCRT_VERSION__
#undef __MSVCRT_VERSION__
#endif

#define __MSVCRT_VERSION__ 0x0700
#endif

#include <malloc.h>
#else
// Apple's malloc is 16-byte aligned, and does not have malloc.h, so include
// stdilb instead.
#include <cstdlib>
#endif

#include "libmv/image/image.h"
#include "libmv/image/convolve.h"
#include "libmv/image/correlation.h"
#include "libmv/image/sample.h"
#include "libmv/logging/logging.h"

namespace libmv {
namespace {

// TODO(keir): It's stupid that this is needed here. Push this somewhere else.
void *aligned_malloc(int size, int alignment) {
#ifdef _WIN32
  return _aligned_malloc(size, alignment);
#elif __APPLE__
  // On Mac OS X, both the heap and the stack are guaranteed 16-byte aligned so
  // they work natively with SSE types with no further work.
  CHECK_EQ(alignment, 16);
  return malloc(size);
#elif __FreeBSD__
  void *result;

  if(posix_memalign(&result, alignment, size)) {
    // non-zero means allocation error
    // either no allocation or bad alignment value
    return NULL;
  }
  return result;
#else // This is for Linux.
  return memalign(alignment, size);
#endif
}

void aligned_free(void *ptr) {
#ifdef _WIN32
  _aligned_free(ptr);
#else
  free(ptr);
#endif
}

bool RegionIsInBounds(const FloatImage &image1,
                      double x, double y,
                      int half_window_size) {
  // Check the minimum coordinates.
  int min_x = floor(x) - half_window_size - 1;
  int min_y = floor(y) - half_window_size - 1;
  if (min_x < 0.0 ||
      min_y < 0.0) {
    return false;
  }

  // Check the maximum coordinates.
  int max_x = ceil(x) + half_window_size + 1;
  int max_y = ceil(y) + half_window_size + 1;
  if (max_x > image1.cols() ||
      max_y > image1.rows()) {
    return false;
  }

  // Ok, we're good.
  return true;
}

#ifdef __SSE2__

// Compute the sub of absolute differences between the arrays "a" and "b". 
// The array "a" is assumed to be 16-byte aligned, while "b" is not. The
// result is returned as the first and third elements of __m128i if
// interpreted as a 4-element 32-bit integer array. The SAD is the sum of the
// elements.
//
// The function requires size % 16 valid extra elements at the end of both "a"
// and "b", since the SSE load instructionst will pull in memory past the end
// of the arrays if their size is not a multiple of 16.
inline static __m128i SumOfAbsoluteDifferencesContiguousSSE(
    const unsigned char *a,  // aligned
    const unsigned char *b,  // not aligned
    unsigned int size,
    __m128i sad) {
  // Do the bulk of the work as 16-way integer operations.
  for(unsigned int j = 0; j < size / 16; j++) {
    sad = _mm_add_epi32(sad, _mm_sad_epu8( _mm_load_si128 ((__m128i*)(a + 16 * j)),
                                           _mm_loadu_si128((__m128i*)(b + 16 * j))));
  }
  // Handle the trailing end.
  // TODO(keir): Benchmark to verify that the below SSE is a win compared to a
  // hand-rolled loop. It's not clear that the hand rolled loop would be slower
  // than the potential cache miss when loading the immediate table below.
  //
  // An alternative to this version is to take a packet of all 1's then do a
  // 128-bit shift. The issue is that the shift instruction needs an immediate
  // amount rather than a variable amount, so the branch instruction here must
  // remain. See _mm_srli_si128 and  _mm_slli_si128.
  unsigned int remainder = size % 16u;
  if (remainder) {
    unsigned int j = size / 16;
    __m128i a_trail = _mm_load_si128 ((__m128i*)(a + 16 * j));
    __m128i b_trail = _mm_loadu_si128((__m128i*)(b + 16 * j));
    __m128i mask;
    switch (remainder) {
#define X 0xff
      case  1: mask = _mm_setr_epi8(X, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); break;
      case  2: mask = _mm_setr_epi8(X, X, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); break;
      case  3: mask = _mm_setr_epi8(X, X, X, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); break;
      case  4: mask = _mm_setr_epi8(X, X, X, X, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); break;
      case  5: mask = _mm_setr_epi8(X, X, X, X, X, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); break;
      case  6: mask = _mm_setr_epi8(X, X, X, X, X, X, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0); break;
      case  7: mask = _mm_setr_epi8(X, X, X, X, X, X, X, 0, 0, 0, 0, 0, 0, 0, 0, 0); break;
      case  8: mask = _mm_setr_epi8(X, X, X, X, X, X, X, X, 0, 0, 0, 0, 0, 0, 0, 0); break;
      case  9: mask = _mm_setr_epi8(X, X, X, X, X, X, X, X, X, 0, 0, 0, 0, 0, 0, 0); break;
      case 10: mask = _mm_setr_epi8(X, X, X, X, X, X, X, X, X, X, 0, 0, 0, 0, 0, 0); break;
      case 11: mask = _mm_setr_epi8(X, X, X, X, X, X, X, X, X, X, X, 0, 0, 0, 0, 0); break;
      case 12: mask = _mm_setr_epi8(X, X, X, X, X, X, X, X, X, X, X, X, 0, 0, 0, 0); break;
      case 13: mask = _mm_setr_epi8(X, X, X, X, X, X, X, X, X, X, X, X, X, 0, 0, 0); break;
      case 14: mask = _mm_setr_epi8(X, X, X, X, X, X, X, X, X, X, X, X, X, X, 0, 0); break;
      case 15: mask = _mm_setr_epi8(X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, 0); break;
#undef X
    }
    sad = _mm_add_epi32(sad, _mm_sad_epu8(_mm_and_si128(mask, a_trail),
                                          _mm_and_si128(mask, b_trail)));
  }
  return sad;
}
#endif

// Computes the sum of absolute differences between pattern and image. Pattern
// must be 16-byte aligned, and the stride must be a multiple of 16. The image
// does pointer does not have to be aligned.
int SumOfAbsoluteDifferencesContiguousImage(
    const unsigned char *pattern,
    unsigned int pattern_width,
    unsigned int pattern_height,
    unsigned int pattern_stride,
    const unsigned char *image,
    unsigned int image_stride) {
#ifdef __SSE2__
  // TODO(keir): Add interleaved accumulation, where accumulation is done into
  // two or more SSE registers that then get combined at the end. This reduces
  // instruction dependency; in Eigen's squared norm code, splitting the
  // accumulation produces a ~2x speedup. It's not clear it will help here,
  // where the number of SSE instructions in the inner loop is smaller.
  __m128i sad = _mm_setzero_si128();
  for (int r = 0; r < pattern_height; ++r) {
    sad = SumOfAbsoluteDifferencesContiguousSSE(&pattern[pattern_stride * r],
                                                &image[image_stride * r],
                                                pattern_width,
                                                sad);
  }
  return _mm_cvtsi128_si32(
             _mm_add_epi32(sad,
                 _mm_shuffle_epi32(sad, _MM_SHUFFLE(3, 0, 1, 2))));
#else
  int sad = 0;
  for (int r = 0; r < pattern_height; ++r) {
    for (int c = 0; c < pattern_width; ++c) {
      sad += abs(pattern[pattern_stride * r + c] - image[image_stride * r + c]);
    }
  }
  return sad;
#endif
}

// Sample a region of size width, height centered at x,y in image, converting
// from float to byte in the process. Samples from the first channel. Puts
// result into *pattern.
void SampleRectangularPattern(const FloatImage &image,
                              double x, double y,
                              int width,
                              int height,
                              int pattern_stride,
                              unsigned char *pattern) {
  // There are two cases for width and height: even or odd. If it's odd, then
  // the bounds [-width / 2, width / 2] works as expected. However, for even,
  // this results in one extra access past the end. So use < instead of <= in
  // the loops below, but increase the end limit by one in the odd case.
  int end_width = (width / 2) + (width % 2);
  int end_height = (height / 2) + (height % 2);
  for (int r = -height / 2; r < end_height; ++r) {
    for (int c = -width / 2; c < end_width; ++c) {
      pattern[pattern_stride * (r + height / 2) +  c + width / 2] =
          SampleLinear(image, y + r, x + c, 0) * 255.0;
    }
  }
}

// Returns x rounded up to the nearest multiple of alignment.
inline int PadToAlignment(int x, int alignment) {
  if (x % alignment != 0) {
    x += alignment - (x % alignment);
  }
  return x;
}

// Sample a region centered at x,y in image with size extending by half_width
// from x. Samples from the first channel. The resulting array is placed in
// *pattern, and the stride, which will be a multiple of 16 if SSE is enabled,
// is returned in *pattern_stride.
//
// NOTE: Caller must free *pattern with aligned_malloc() from above.
void SampleSquarePattern(const FloatImage &image,
                         double x, double y,
                         int half_width,
                         unsigned char **pattern,
                         int *pattern_stride) {
  int width = 2 * half_width + 1;
  // Allocate an aligned block with padding on the end so each row of the
  // pattern starts on a 16-byte boundary.
  *pattern_stride = PadToAlignment(width, 16);
  int pattern_size_bytes = *pattern_stride * width;
  *pattern = static_cast<unsigned char *>(
      aligned_malloc(pattern_size_bytes, 16));
  SampleRectangularPattern(image, x, y, width, width,
                           *pattern_stride,
                           *pattern);
}

// NOTE: Caller must free *image with aligned_malloc() from above.
void FloatArrayToByteArrayWithPadding(const FloatImage &float_image,
                                      unsigned char **image,
                                      int *image_stride) {
  // Allocate enough so that accessing 16 elements past the end is fine.
  *image_stride = float_image.Width() + 16;
  *image = static_cast<unsigned char *>(
      aligned_malloc(*image_stride * float_image.Height(), 16));
  for (int i = 0; i < float_image.Height(); ++i) {
    for (int j = 0; j < float_image.Width(); ++j) {
      (*image)[*image_stride * i + j] =
          static_cast<unsigned char>(255.0 * float_image(i, j, 0));
    }
  }
}

}  // namespace

// TODO(keir): Compare the "sharpness" of the peak around the best pixel. It's
// probably worth plotting a few examples to see what the histogram of SAD
// values for every hypothesis looks like.
//
// TODO(keir): Priority queue for multiple hypothesis.
bool BruteRegionTracker::Track(const FloatImage &image1,
                               const FloatImage &image2,
                               double  x1, double  y1,
                               double *x2, double *y2) const {
  if (!RegionIsInBounds(image1, x1, y1, half_window_size)) {
    LG << "Fell out of image1's window with x1=" << x1 << ", y1=" << y1
       << ", hw=" << half_window_size << ".";
    return false;
  }
  int pattern_width = 2 * half_window_size + 1;

  Array3Df image_and_gradient1;
  Array3Df image_and_gradient2;
  BlurredImageAndDerivativesChannels(image1, 0.9, &image_and_gradient1);
  BlurredImageAndDerivativesChannels(image2, 0.9, &image_and_gradient2);

  // Sample the pattern to get it aligned to an image grid.
  unsigned char *pattern;
  int pattern_stride;
  SampleSquarePattern(image_and_gradient1, x1, y1, half_window_size,
                      &pattern,
                      &pattern_stride);

  // Convert the search area directly to bytes without sampling.
  unsigned char *search_area;
  int search_area_stride;
  FloatArrayToByteArrayWithPadding(image_and_gradient2, &search_area, &search_area_stride);

  // Try all possible locations inside the search area. Yes, everywhere.
  int best_i = -1, best_j = -1, best_sad = INT_MAX;
  for (int i = 0; i < image2.Height() - pattern_width; ++i) {
    for (int j = 0; j < image2.Width() - pattern_width; ++j) {
      int sad = SumOfAbsoluteDifferencesContiguousImage(pattern,
                                                        pattern_width,
                                                        pattern_width,
                                                        pattern_stride,
                                                        search_area + search_area_stride * i + j,
                                                        search_area_stride);
      if (sad < best_sad) {
        best_i = i;
        best_j = j;
        best_sad = sad;
      }
    }
  }

  CHECK_NE(best_i, -1);
  CHECK_NE(best_j, -1);

  aligned_free(pattern);
  aligned_free(search_area);

  if (best_sad == INT_MAX) {
    LG << "Hit INT_MAX in SAD; failing.";
    return false;
  }

  *x2 = best_j + half_window_size;
  *y2 = best_i + half_window_size;

  // Calculate the shift done by the fine tracker.
  double dx2 = *x2 - x1;
  double dy2 = *y2 - y1;
  double fine_shift = sqrt(dx2 * dx2 + dy2 * dy2);
  LG << "Brute shift: dx=" << dx2 << " dy=" << dy2 << ", d=" << fine_shift;

  if (minimum_correlation <= 0) {
    // No correlation checking requested; nothing else to do.
    LG << "No correlation checking; returning success. best_sad: " << best_sad;
    return true;
  }

  Array3Df image_and_gradient1_sampled, image_and_gradient2_sampled;
  SamplePattern(image_and_gradient1, x1, y1, half_window_size, 3,
                &image_and_gradient1_sampled);
  SamplePattern(image_and_gradient2, *x2, *y2, half_window_size, 3,
                &image_and_gradient2_sampled);

  // Compute the Pearson product-moment correlation coefficient to check
  // for sanity.
  double correlation = PearsonProductMomentCorrelation(image_and_gradient1_sampled,
                                                       image_and_gradient2_sampled,
                                                       pattern_width);
  LG << "Final correlation: " << correlation;

  if (correlation < minimum_correlation) {
    LG << "Correlation " << correlation << " greater than "
       << minimum_correlation << "; bailing.";
    return false;
  }
  return true;
}

}  // namespace libmv
