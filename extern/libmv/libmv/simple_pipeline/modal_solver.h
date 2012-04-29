// Copyright (c) 2012 libmv authors.
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

#ifndef LIBMV_SIMPLE_PIPELINE_MODAL_SOLVER_H_
#define LIBMV_SIMPLE_PIPELINE_MODAL_SOLVER_H_

#include "libmv/simple_pipeline/tracks.h"
#include "libmv/simple_pipeline/reconstruction.h"
#include "libmv/simple_pipeline/callbacks.h"

namespace libmv {

/*!
    This solver solves such camera motion as tripod rotation, reconstructing
    only camera motion itself. Bundles are not reconstructing properly, they're
    just getting projected onto sphere.

    Markers from tracks object would be used for recosntruction, and algorithm
    assumes thir's positions are undistorted already and they're in nnormalized
    space.

    Reconstructed cameras and projected bundles would be added to reconstruction
    object.
*/
void ModalSolver(Tracks &tracks,
                 EuclideanReconstruction *reconstruction,
                 ProgressUpdateCallback *update_callback = NULL);

}  // namespace libmv

#endif  // LIBMV_SIMPLE_PIPELINE_MODAL_SOLVER_H_
