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

#include "libmv/simple_pipeline/tracks.h"

#include <algorithm>
#include <vector>
#include <iterator>

#include "libmv/numeric/numeric.h"

namespace libmv {

Tracks::Tracks(const Tracks &other) {
  markers_ = other.markers_;
}

Tracks::Tracks(const vector<Marker> &markers) : markers_(markers) {}

void Tracks::Insert(int image, int track, double x, double y, double weight) {
  // TODO(keir): Wow, this is quadratic for repeated insertions. Fix this by
  // adding a smarter data structure like a set<>.
  for (int i = 0; i < markers_.size(); ++i) {
    if (markers_[i].image == image &&
        markers_[i].track == track) {
      markers_[i].x = x;
      markers_[i].y = y;
      return;
    }
  }
  Marker marker = { image, track, x, y, weight };
  markers_.push_back(marker);
}

vector<Marker> Tracks::AllMarkers() const {
  return markers_;
}

vector<Marker> Tracks::MarkersInImage(int image) const {
  vector<Marker> markers;
  for (int i = 0; i < markers_.size(); ++i) {
    if (image == markers_[i].image) {
      markers.push_back(markers_[i]);
    }
  }
  return markers;
}

vector<Marker> Tracks::MarkersForTrack(int track) const {
  vector<Marker> markers;
  for (int i = 0; i < markers_.size(); ++i) {
    if (track == markers_[i].track) {
      markers.push_back(markers_[i]);
    }
  }
  return markers;
}

vector<Marker> Tracks::MarkersInBothImages(int image1, int image2) const {
  vector<Marker> markers;
  for (int i = 0; i < markers_.size(); ++i) {
    int image = markers_[i].image;
    if (image == image1 || image == image2)
      markers.push_back(markers_[i]);
  }
  return markers;
}

vector<Marker> Tracks::MarkersForTracksInBothImages(int image1,
                                                    int image2) const {
  std::vector<int> image1_tracks;
  std::vector<int> image2_tracks;

  for (int i = 0; i < markers_.size(); ++i) {
    int image = markers_[i].image;
    if (image == image1) {
      image1_tracks.push_back(markers_[i].track);
    } else if (image == image2) {
      image2_tracks.push_back(markers_[i].track);
    }
  }

  std::sort(image1_tracks.begin(), image1_tracks.end());
  std::sort(image2_tracks.begin(), image2_tracks.end());

  std::vector<int> intersection;
  std::set_intersection(image1_tracks.begin(), image1_tracks.end(),
                        image2_tracks.begin(), image2_tracks.end(),
                        std::back_inserter(intersection));

  vector<Marker> markers;
  for (int i = 0; i < markers_.size(); ++i) {
    if ((markers_[i].image == image1 || markers_[i].image == image2) &&
        std::binary_search(intersection.begin(), intersection.end(),
                           markers_[i].track)) {
      markers.push_back(markers_[i]);
    }
  }
  return markers;
}

Marker Tracks::MarkerInImageForTrack(int image, int track) const {
  for (int i = 0; i < markers_.size(); ++i) {
    if (markers_[i].image == image && markers_[i].track == track) {
      return markers_[i];
    }
  }
  Marker null = { -1, -1, -1, -1, 0.0 };
  return null;
}

void Tracks::RemoveMarkersForTrack(int track) {
  int size = 0;
  for (int i = 0; i < markers_.size(); ++i) {
    if (markers_[i].track != track) {
      markers_[size++] = markers_[i];
    }
  }
  markers_.resize(size);
}

void Tracks::RemoveMarker(int image, int track) {
  int size = 0;
  for (int i = 0; i < markers_.size(); ++i) {
    if (markers_[i].image != image || markers_[i].track != track) {
      markers_[size++] = markers_[i];
    }
  }
  markers_.resize(size);
}

int Tracks::MaxImage() const {
  // TODO(MatthiasF): maintain a max_image_ member (updated on Insert)
  int max_image = 0;
  for (int i = 0; i < markers_.size(); ++i) {
    max_image = std::max(markers_[i].image, max_image);
  }
  return max_image;
}

int Tracks::MaxTrack() const {
  // TODO(MatthiasF): maintain a max_track_ member (updated on Insert)
  int max_track = 0;
  for (int i = 0; i < markers_.size(); ++i) {
    max_track = std::max(markers_[i].track, max_track);
  }
  return max_track;
}

int Tracks::NumMarkers() const {
  return markers_.size();
}

void CoordinatesForMarkersInImage(const vector<Marker> &markers,
                                  int image,
                                  Mat *coordinates) {
  vector<Vec2> coords;
  for (int i = 0; i < markers.size(); ++i) {
    const Marker &marker = markers[i];
    if (markers[i].image == image) {
      coords.push_back(Vec2(marker.x, marker.y));
    }
  }
  coordinates->resize(2, coords.size());
  for (int i = 0; i < coords.size(); i++) {
    coordinates->col(i) = coords[i];
  }
}

}  // namespace libmv
