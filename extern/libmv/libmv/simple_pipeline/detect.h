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

#ifndef LIBMV_SIMPLE_PIPELINE_DETECT_H_
#define LIBMV_SIMPLE_PIPELINE_DETECT_H_

#include <vector>

namespace libmv {

/*!
    A Corner is the 2D location of a detected feature in an image.

    \a x, \a y is the position of the corner in pixels from the top left corner.
    \a score is an estimate of how well the feature will be tracked.
    \a size can be used as an initial pattern size to track the feature.

    \sa Detect
*/
struct Corner {
  /// Position in pixels (from top-left corner)
  /// \note libmv might eventually support subpixel precision.
  float x, y;
  /// Trackness of the feature
  float score;
  /// Size of the feature in pixels
  float size;
};

/*!
    Detect features in an image.

    You need to input a single channel 8-bit image using pointer to image \a data,
    \a width, \a height and \a stride (i.e bytes per line).

    To avoid detecting tracks which will quickly go out of frame, only corners
    further than \a margin pixels from the image edges are considered.

    You can tweak the count of detected corners using \a min_trackness, which is
    the minimum score to add a corner, and \a min_distance which is the minimal
    distance accepted between two corners.

    \note You can binary search over \a min_trackness to get a given corner count.

    \note a way to get an uniform distribution of a given corner count is:
          \a min_distance = \a width * \a height / desired_corner_count ^ 2

    \return All detected corners matching given parameters
*/
std::vector<Corner> Detect(const unsigned char* data, int width, int height,
                           int stride, int margin = 16, int min_trackness = 16,
                           int min_distance = 120);

}

#endif
