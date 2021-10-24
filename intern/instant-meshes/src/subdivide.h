/*
    subdivide.h: Subdivides edges in a triangle mesh until all edges
    are below a specified maximum length

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include "common.h"

extern void subdivide(MatrixXu &F, MatrixXf &V, VectorXu &V2E, VectorXu &E2E,
                      VectorXb &boundary, VectorXb &nonmanifold,
                      Float maxLength, bool deterministic = false,
                      const ProgressCallback &progress = ProgressCallback());
