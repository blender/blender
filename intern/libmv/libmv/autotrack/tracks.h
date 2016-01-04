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

#ifndef LIBMV_AUTOTRACK_TRACKS_H_
#define LIBMV_AUTOTRACK_TRACKS_H_

#include "libmv/base/vector.h"
#include "libmv/autotrack/marker.h"

namespace mv {

using libmv::vector;

// The Tracks container stores correspondences between frames.
class Tracks {
 public:
  Tracks() { }
  Tracks(const Tracks &other);

  // Create a tracks object with markers already initialized. Copies markers.
  explicit Tracks(const vector<Marker>& markers);

  // All getters append to the output argument vector.
  bool GetMarker(int clip, int frame, int track, Marker* marker) const;
  void GetMarkersForTrack(int track, vector<Marker>* markers) const;
  void GetMarkersForTrackInClip(int clip,
                                int track,
                                vector<Marker>* markers) const;
  void GetMarkersInFrame(int clip, int frame, vector<Marker>* markers) const;

  // Get the markers in frame1 and frame2 which have a common track.
  //
  // This is not the same as the union of the markers in frame1 and
  // frame2; each marker is for a track that appears in both images.
  void GetMarkersForTracksInBothImages(int clip1, int frame1,
                                       int clip2, int frame2,
                                       vector<Marker>* markers) const;

  void AddMarker(const Marker& marker);

  // Moves the contents of *markers over top of the existing markers. This
  // destroys *markers in the process (but avoids copies).
  void SetMarkers(vector<Marker>* markers);
  bool RemoveMarker(int clip, int frame, int track);
  void RemoveMarkersForTrack(int track);

  int MaxClip() const;
  int MaxFrame(int clip) const;
  int MaxTrack() const;
  int NumMarkers() const;

  const vector<Marker>& markers() const { return markers_; }

 private:
  vector<Marker> markers_;

  // TODO(keir): Consider adding access-map data structures to avoid all the
  // linear lookup penalties for the accessors.
};

}  // namespace mv

#endif  // LIBMV_AUTOTRACK_TRACKS_H_
