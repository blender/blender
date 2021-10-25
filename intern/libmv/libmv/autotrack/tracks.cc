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

#include "libmv/autotrack/tracks.h"

#include <algorithm>
#include <vector>
#include <iterator>

#include "libmv/numeric/numeric.h"

namespace mv {

Tracks::Tracks(const Tracks& other) {
  markers_ = other.markers_;
}

Tracks::Tracks(const vector<Marker>& markers) : markers_(markers) {}

bool Tracks::GetMarker(int clip, int frame, int track, Marker* marker) const {
  for (int i = 0; i < markers_.size(); ++i) {
    if (markers_[i].clip  == clip &&
        markers_[i].frame == frame &&
        markers_[i].track == track) {
      *marker = markers_[i];
      return true;
    }
  }
  return false;
}

void Tracks::GetMarkersForTrack(int track, vector<Marker>* markers) const {
  for (int i = 0; i < markers_.size(); ++i) {
    if (track == markers_[i].track) {
      markers->push_back(markers_[i]);
    }
  }
}

void Tracks::GetMarkersForTrackInClip(int clip,
                                      int track,
                                      vector<Marker>* markers) const {
  for (int i = 0; i < markers_.size(); ++i) {
    if (clip  == markers_[i].clip &&
        track == markers_[i].track) {
      markers->push_back(markers_[i]);
    }
  }
}

void Tracks::GetMarkersInFrame(int clip,
                               int frame,
                               vector<Marker>* markers) const {
  for (int i = 0; i < markers_.size(); ++i) {
    if (markers_[i].clip  == clip &&
        markers_[i].frame == frame) {
      markers->push_back(markers_[i]);
    }
  }
}

void Tracks::GetMarkersForTracksInBothImages(int clip1, int frame1,
                                             int clip2, int frame2,
                                             vector<Marker>* markers) const {
  std::vector<int> image1_tracks;
  std::vector<int> image2_tracks;

  // Collect the tracks in each of the two images.
  for (int i = 0; i < markers_.size(); ++i) {
    int clip = markers_[i].clip;
    int frame = markers_[i].frame;
    if (clip == clip1 && frame == frame1) {
      image1_tracks.push_back(markers_[i].track);
    } else if (clip == clip2 && frame == frame2) {
      image2_tracks.push_back(markers_[i].track);
    }
  }

  // Intersect the two sets to find the tracks of interest.
  std::sort(image1_tracks.begin(), image1_tracks.end());
  std::sort(image2_tracks.begin(), image2_tracks.end());
  std::vector<int> intersection;
  std::set_intersection(image1_tracks.begin(), image1_tracks.end(),
                        image2_tracks.begin(), image2_tracks.end(),
                        std::back_inserter(intersection));

  // Scan through and get the relevant tracks from the two images.
  for (int i = 0; i < markers_.size(); ++i) {
    // Save markers that are in either frame and are in our candidate set.
    if (((markers_[i].clip  == clip1 &&
          markers_[i].frame == frame1) ||
         (markers_[i].clip  == clip2 &&
          markers_[i].frame == frame2)) &&
         std::binary_search(intersection.begin(),
                            intersection.end(),
                            markers_[i].track)) {
      markers->push_back(markers_[i]);
    }
  }
}

void Tracks::AddMarker(const Marker& marker) {
  // TODO(keir): This is quadratic for repeated insertions. Fix this by adding
  // a smarter data structure like a set<>.
  for (int i = 0; i < markers_.size(); ++i) {
    if (markers_[i].clip  == marker.clip &&
        markers_[i].frame == marker.frame &&
        markers_[i].track == marker.track) {
      markers_[i] = marker;
      return;
    }
  }
  markers_.push_back(marker);
}

void Tracks::SetMarkers(vector<Marker>* markers) {
  std::swap(markers_, *markers);
}

bool Tracks::RemoveMarker(int clip, int frame, int track) {
  int size = markers_.size();
  for (int i = 0; i < markers_.size(); ++i) {
    if (markers_[i].clip  == clip &&
        markers_[i].frame == frame &&
        markers_[i].track == track) {
      markers_[i] = markers_[size - 1];
      markers_.resize(size - 1);
      return true;
    }
  }
  return false;
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

int Tracks::MaxClip() const {
  int max_clip = 0;
  for (int i = 0; i < markers_.size(); ++i) {
    max_clip = std::max(markers_[i].clip, max_clip);
  }
  return max_clip;
}

int Tracks::MaxFrame(int clip) const {
  int max_frame = 0;
  for (int i = 0; i < markers_.size(); ++i) {
    if (markers_[i].clip == clip) {
      max_frame = std::max(markers_[i].frame, max_frame);
    }
  }
  return max_frame;
}

int Tracks::MaxTrack() const {
  int max_track = 0;
  for (int i = 0; i < markers_.size(); ++i) {
    max_track = std::max(markers_[i].track, max_track);
  }
  return max_track;
}

int Tracks::NumMarkers() const {
  return markers_.size();
}

}  // namespace mv
