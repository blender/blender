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

#ifndef LIBMV_SIMPLE_PIPELINE_INITIALIZE_RECONSTRUCTION_H
#define LIBMV_SIMPLE_PIPELINE_INITIALIZE_RECONSTRUCTION_H

#include "libmv/base/vector.h"

namespace libmv {

struct Marker;
class EuclideanReconstruction;
class ProjectiveReconstruction;

/*!
    Initialize the \link EuclideanReconstruction reconstruction \endlink using
    two frames.

    \a markers should contain all \l Marker markers \endlink belonging to
    tracks visible in both frames. The pose estimation of the camera for
    these frames will be inserted into \a *reconstruction.

    \note The two frames need to have both enough parallax and enough common tracks
          for accurate reconstruction. At least 8 tracks are suggested.
    \note The origin of the coordinate system is defined to be the camera of
          the first keyframe.
    \note This assumes a calibrated reconstruction, e.g. the markers are
          already corrected for camera intrinsics and radial distortion.
    \note This assumes an outlier-free set of markers.

    \sa EuclideanResect, EuclideanIntersect, EuclideanBundle
*/
bool EuclideanReconstructTwoFrames(const vector<Marker> &markers,
                                   EuclideanReconstruction *reconstruction);

/*!
    Initialize the \link ProjectiveReconstruction reconstruction \endlink using
    two frames.

    \a markers should contain all \l Marker markers \endlink belonging to
    tracks visible in both frames. An estimate of the projection matrices for
    the two frames will get added to the reconstruction.

    \note The two frames need to have both enough parallax and enough common tracks
          for accurate reconstruction. At least 8 tracks are suggested.
    \note The origin of the coordinate system is defined to be the camera of
          the first keyframe.
    \note This assumes the markers are already corrected for radial distortion.
    \note This assumes an outlier-free set of markers.

    \sa ProjectiveResect, ProjectiveIntersect, ProjectiveBundle
*/
bool ProjectiveReconstructTwoFrames(const vector<Marker> &markers,
                                    ProjectiveReconstruction *reconstruction);
}  // namespace libmv

#endif  // LIBMV_SIMPLE_PIPELINE_INITIALIZE_RECONSTRUCTION_H
