/*
    normal.h: Helper routines for computing vertex normals

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include "common.h"
#include <map>
#include <set>

extern void
generate_smooth_normals(const MatrixXu &F, const MatrixXf &V, MatrixXf &N,
                        bool deterministic,
                        const ProgressCallback &progress = ProgressCallback());

extern void
generate_smooth_normals(const MatrixXu &F, const MatrixXf &V,
                        const VectorXu &V2E, const VectorXu &E2E,
                        const VectorXb &nonManifold, MatrixXf &N,
                        const ProgressCallback &progress = ProgressCallback());

extern void
generate_crease_normals(MatrixXu &F, MatrixXf &V, const VectorXu &V2E,
                        const VectorXu &E2E, const VectorXb boundary,
                        const VectorXb &nonManifold, Float angleThreshold,
                        MatrixXf &N, std::map<uint32_t, uint32_t> &creases,
                        const ProgressCallback &progress = ProgressCallback());

extern void
generate_crease_normals(
    const MatrixXu &F, const MatrixXf &V, const VectorXu &V2E, const VectorXu &E2E,
    const VectorXb boundary, const VectorXb &nonManifold, Float angleThreshold,
    MatrixXf &N, std::set<uint32_t> &creases,
    const ProgressCallback &progress = ProgressCallback());
