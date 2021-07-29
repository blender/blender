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

#ifndef LIBMV_SIMPLE_PIPELINE_PIPELINE_H_
#define LIBMV_SIMPLE_PIPELINE_PIPELINE_H_

#include "libmv/simple_pipeline/callbacks.h"
#include "libmv/simple_pipeline/tracks.h"
#include "libmv/simple_pipeline/reconstruction.h"

namespace libmv {

/*!
    Estimate camera poses and scene 3D coordinates for all frames and tracks.

    This method should be used once there is an initial reconstruction in
    place, for example by reconstructing from two frames that have a sufficient
    baseline and number of tracks in common. This function iteratively
    triangulates points that are visible by cameras that have their pose
    estimated, then resections (i.e. estimates the pose) of cameras that are
    not estimated yet that can see triangulated points. This process is
    repeated until all points and cameras are estimated. Periodically, bundle
    adjustment is run to ensure a quality reconstruction.

    \a tracks should contain markers used in the reconstruction.
    \a reconstruction should contain at least some 3D points or some estimated
    cameras. The minimum number of cameras is two (with no 3D points) and the
    minimum number of 3D points (with no estimated cameras) is 5.

    \sa EuclideanResect, EuclideanIntersect, EuclideanBundle
*/
void EuclideanCompleteReconstruction(
        const Tracks &tracks,
        EuclideanReconstruction *reconstruction,
        ProgressUpdateCallback *update_callback = NULL);

/*!
    Estimate camera matrices and homogeneous 3D coordinates for all frames and
    tracks.

    This method should be used once there is an initial reconstruction in
    place, for example by reconstructing from two frames that have a sufficient
    baseline and number of tracks in common. This function iteratively
    triangulates points that are visible by cameras that have their pose
    estimated, then resections (i.e. estimates the pose) of cameras that are
    not estimated yet that can see triangulated points. This process is
    repeated until all points and cameras are estimated. Periodically, bundle
    adjustment is run to ensure a quality reconstruction.

    \a tracks should contain markers used in the reconstruction.
    \a reconstruction should contain at least some 3D points or some estimated
    cameras. The minimum number of cameras is two (with no 3D points) and the
    minimum number of 3D points (with no estimated cameras) is 5.

    \sa ProjectiveResect, ProjectiveIntersect, ProjectiveBundle
*/
void ProjectiveCompleteReconstruction(const Tracks &tracks,
                                      ProjectiveReconstruction *reconstruction);


class CameraIntrinsics;

// TODO(keir): Decide if we want these in the public API, and if so, what the
// appropriate include file is.

double EuclideanReprojectionError(const Tracks &image_tracks,
                                  const EuclideanReconstruction &reconstruction,
                                  const CameraIntrinsics &intrinsics);

double ProjectiveReprojectionError(
    const Tracks &image_tracks,
    const ProjectiveReconstruction &reconstruction,
    const CameraIntrinsics &intrinsics);

void InvertIntrinsicsForTracks(const Tracks &raw_tracks,
                               const CameraIntrinsics &camera_intrinsics,
                               Tracks *calibrated_tracks);

}  // namespace libmv

#endif  // LIBMV_SIMPLE_PIPELINE_PIPELINE_H_
