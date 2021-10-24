/*
    smoothcurve.h: Helper routines to compute smooth curves on meshes to enable
    intuitive stroke annotations

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/


#pragma once

#include "common.h"

struct CurvePoint {
    Vector3f p;
    Vector3f n;
    uint32_t f;
};

class BVH;

extern bool smooth_curve(const BVH *bvh, const VectorXu &E2E,
                         std::vector<CurvePoint> &curve, bool watertight = false);

extern bool astar(const MatrixXu &F, const VectorXu &E2E, const MatrixXf &V,
                  uint32_t start, uint32_t end, std::vector<uint32_t> &path);
