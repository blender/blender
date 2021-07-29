// Copyright (c) 2014 libmv authors.
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
//
// Author: mierle@gmail.com (Keir Mierle)

#ifndef LIBMV_AUTOTRACK_MARKER_H_
#define LIBMV_AUTOTRACK_MARKER_H_

#include <ostream>

#include "libmv/autotrack/quad.h"
#include "libmv/autotrack/region.h"
#include "libmv/numeric/numeric.h"

namespace mv {

using libmv::Vec2f;

// A marker is the 2D location of a tracked region (quad) in an image.
// Note that some of this information could be normalized by having a
// collection of inter-connected structs. Instead the "fat Marker" design below
// trades memory for data structure simplicity.
struct Marker {
  int clip;   // The clip this marker is from.
  int frame;  // The frame within the clip this marker is from.
  int track;  // The track this marker is from.

  // The center of the marker in frame coordinates. This is typically, but not
  // always, the same as the center of the patch.
  Vec2f center;

  // A frame-realtive quad defining the part of the image the marker covers.
  // For reference markers, the pixels in the patch are the tracking pattern.
  Quad2Df patch;

  // Some markers are less certain than others; the weight determines the
  // amount this marker contributes to the error. 1.0 indicates normal
  // contribution; 0.0 indicates a zero-weight track (and will be omitted from
  // bundle adjustment).
  float weight;

  enum Source {
    MANUAL,      // The user placed this marker manually.
    DETECTED,    // A keypoint detector found this point.
    TRACKED,     // The tracking algorithm placed this marker.
    MATCHED,     // A matching algorithm (e.g. SIFT or SURF or ORB) found this.
    PREDICTED,   // A motion model predicted this marker. This is needed for
                 // handling occlusions in some cases where an imaginary marker
                 // is placed to keep camera motion smooth.
  };
  Source source;

  // Markers may be inliers or outliers if the tracking fails; this allows
  // visualizing the markers in the image.
  enum Status {
    UNKNOWN,
    INLIER,
    OUTLIER
  };
  Status status;

  // When doing correlation tracking, where to search in the current frame for
  // the pattern from the reference frame, in absolute frame coordinates.
  Region search_region;

  // For tracked and matched markers, indicates what the reference was.
  int reference_clip;
  int reference_frame;

  // Model related information for non-point tracks.
  //
  // Some tracks are on a larger object, such as a plane or a line or perhaps
  // another primitive (a rectangular prisim). This captures the information
  // needed to say that for example a collection of markers belongs to model #2
  // (and model #2 is a plane).
  enum ModelType {
    POINT,
    PLANE,
    LINE,
    CUBE
  };
  ModelType model_type;

  // The model ID this track (e.g. the second model, which is a plane).
  int model_id;

  // TODO(keir): Add a "int model_argument" to capture that e.g. a marker is on
  // the 3rd face of a cube.

  enum Channel {
    CHANNEL_R = (1 << 0),
    CHANNEL_G = (1 << 1),
    CHANNEL_B = (1 << 2),
  };

  // Channels from the original frame which this marker is unable to see.
  int disabled_channels;

  // Offset everything (center, patch, search) by the given delta.
  template<typename T>
  void Offset(const T& offset) {
    center += offset.template cast<float>();
    patch.coordinates.rowwise() += offset.template cast<int>();
    search_region.Offset(offset);
  }

  // Shift the center to the given new position (and patch, search).
  template<typename T>
  void SetPosition(const T& new_center) {
    Offset(new_center - center);
  }
};

inline std::ostream& operator<<(std::ostream& out, const Marker& marker) {
  out << "{"
      << marker.clip << ", "
      << marker.frame << ", "
      << marker.track << ", ("
      << marker.center.x() << ", "
      << marker.center.y() << ")"
      << "}";
  return out;
}

}  // namespace mv

#endif  // LIBMV_AUTOTRACK_MARKER_H_
