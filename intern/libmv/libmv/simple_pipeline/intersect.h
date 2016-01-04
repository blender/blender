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

#ifndef LIBMV_SIMPLE_PIPELINE_INTERSECT_H
#define LIBMV_SIMPLE_PIPELINE_INTERSECT_H

#include "libmv/base/vector.h"
#include "libmv/simple_pipeline/tracks.h"
#include "libmv/simple_pipeline/reconstruction.h"

namespace libmv {

/*!
    Estimate the 3D coordinates of a track by intersecting rays from images.

    This takes a set of markers, where each marker is for the same track but
    different images, and reconstructs the 3D position of that track. Each of
    the frames for which there is a marker for that track must have a
    corresponding reconstructed camera in \a *reconstruction.

    \a markers should contain all \l Marker markers \endlink belonging to
       tracks visible in all frames.
    \a reconstruction should contain the cameras for all frames.
       The new \l Point points \endlink will be inserted in \a reconstruction.

    \note This assumes a calibrated reconstruction, e.g. the markers are
          already corrected for camera intrinsics and radial distortion.
    \note This assumes an outlier-free set of markers.

    \sa EuclideanResect
*/
bool EuclideanIntersect(const vector<Marker> &markers,
                        EuclideanReconstruction *reconstruction);

/*!
    Estimate the homogeneous coordinates of a track by intersecting rays.

    This takes a set of markers, where each marker is for the same track but
    different images, and reconstructs the homogeneous 3D position of that
    track. Each of the frames for which there is a marker for that track must
    have a corresponding reconstructed camera in \a *reconstruction.

    \a markers should contain all \l Marker markers \endlink belonging to
       tracks visible in all frames.
    \a reconstruction should contain the cameras for all frames.
       The new \l Point points \endlink will be inserted in \a reconstruction.

    \note This assumes that radial distortion is already corrected for, but
          does not assume that e.g. focal length and principal point are
          accounted for.
    \note This assumes an outlier-free set of markers.

    \sa Resect
*/
bool ProjectiveIntersect(const vector<Marker> &markers,
                         ProjectiveReconstruction *reconstruction);

}  // namespace libmv

#endif  // LIBMV_SIMPLE_PIPELINE_INTERSECT_H
