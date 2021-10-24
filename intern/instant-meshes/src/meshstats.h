/*
    meshstats.h: Routines to efficiently compute various mesh statistics such
    as the bounding box, surface area, etc.

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include "aabb.h"

struct MeshStats {
    AABB mAABB;
    Vector3f mWeightedCenter;
    double mAverageEdgeLength;
    double mMaximumEdgeLength;
    double mSurfaceArea;

    MeshStats() :
        mWeightedCenter(Vector3f::Zero()),
        mAverageEdgeLength(0.0f),
        mMaximumEdgeLength(0.0f),
        mSurfaceArea(0.0f) { }
};

extern MeshStats
compute_mesh_stats(const MatrixXu &F, const MatrixXf &V,
                   bool deterministic = false,
                   const ProgressCallback &progress = ProgressCallback());

void compute_dual_vertex_areas(
    const MatrixXu &F, const MatrixXf &V, const VectorXu &V2E,
    const VectorXu &E2E, const VectorXb &nonManifold, VectorXf &A,
    const ProgressCallback &progress = ProgressCallback());
