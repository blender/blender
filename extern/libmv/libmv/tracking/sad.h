/****************************************************************************
**
** Copyright (c) 2011 libmv authors.
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to
** deal in the Software without restriction, including without limitation the
** rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
** sell copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
** IN THE SOFTWARE.
**
****************************************************************************/

#ifndef LIBMV_TRACKING_SAD_H_
#define LIBMV_TRACKING_SAD_H_

#ifdef __cplusplus
namespace libmv {
#endif

typedef unsigned char ubyte;
typedef float mat3[9];

/*!
    Sample \a pattern from \a image.

    \a pattern is a 16x16 buffer
    \a warp is the transformation to apply when sampling the \a pattern in \a image.

    \note \a warp might be used by higher level tracking methods (e.g planar)
*/
void SamplePattern(const ubyte* image, int stride, mat3 warp, ubyte* pattern);

/*!
    Track \a pattern in \a image.

    This template matcher computes the
    \link http://en.wikipedia.org/wiki/Sum_of_absolute_differences Sum of Absolute Differences (SAD) \endlink
    for each integer pixel position in the search region and then iteratively
    refine subpixel position using a square search.
    A similar method is used for motion estimation in video encoders.

    \a pattern is a 16x16 single channel image to track.
    \a x, \a y is the initial estimated position in \a image.
    On return, \a x, \a y is the tracked position.
    \a image is a reference to the single channel image to search.
    \a stride is size of \a image lines.

    \note For a 16x speedup, compile this tracker with SSE2 support.
    \note \a stride allow you to reference your search region instead of copying.
*/
bool Track(const ubyte* pattern, const ubyte* image, int stride, int width, int height, float* x, float* y);

#ifdef __cplusplus
}  // namespace libmv
#endif

#endif  // LIBMV_TRACKING_SAD_H_
