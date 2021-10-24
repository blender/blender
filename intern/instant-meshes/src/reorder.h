/*
    reorder.h: Reorder mesh face/vertex indices to improve coherence in various
    applications

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include "common.h"

extern void reorder_mesh(MatrixXu &F, std::vector<MatrixXf> &V_vec, std::vector<MatrixXf> &F_vec,
                         const ProgressCallback &progress = ProgressCallback());

extern void replicate_vertices(MatrixXu &F, std::vector<MatrixXf> &V);
