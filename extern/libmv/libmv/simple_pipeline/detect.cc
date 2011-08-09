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

#include "libmv/simple_pipeline/detect.h"
#include <third_party/fast/fast.h>
#include <stdlib.h>

namespace libmv {

std::vector<Corner> Detect(const unsigned char* data, int width, int height, int stride,
                           int margin, int min_trackness, int min_distance) {
  std::vector<Corner> corners;
  data += margin*width + margin;
  // TODO(MatthiasF): Support targetting a feature count (binary search trackness)
  int num_corners;
  xy* all = fast9_detect(data, width-2*margin, height-2*margin,
                         stride, min_trackness, &num_corners);
  if(num_corners == 0) {
    free(all);
    return corners;
  }
  int* scores = fast9_score(data, stride, all, num_corners, min_trackness);
  // TODO: merge with close feature suppression
  xy* nonmax = nonmax_suppression(all, scores, num_corners, &num_corners);
  free(all);
  // Remove too close features
  // TODO(MatthiasF): A resolution independent parameter would be better than distance
  // e.g. a coefficient going from 0 (no minimal distance) to 1 (optimal circle packing)
  // FIXME(MatthiasF): this method will not necessarily give all maximum markers
  if(num_corners) corners.reserve(num_corners);
  for(int i = 0; i < num_corners; ++i) {
    xy xy = nonmax[i];
    Corner a = { xy.x+margin, xy.y+margin, scores[i], 7 };
    // compare each feature against filtered set
    for(int j = 0; j < corners.size(); j++) {
      Corner& b = corners[j];
      if ( (a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y) < min_distance*min_distance ) {
        // already a nearby feature
        goto skip;
      }
    }
    // otherwise add the new feature
    corners.push_back(a);
    skip: ;
  }
  free(scores);
  free(nonmax);
  return corners;
}
}
