/*
    dedge.h: Parallel directed edge data structure builder

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include "common.h"

static const uint32_t INVALID = (uint32_t) -1;

inline uint32_t dedge_prev_3(uint32_t e) { return (e % 3 == 0) ? e + 2 : e - 1; }
inline uint32_t dedge_next_3(uint32_t e) { return (e % 3 == 2) ? e - 2 : e + 1; }
inline uint32_t dedge_prev_4(uint32_t e) { return (e % 4 == 0) ? e + 3 : e - 1; }
inline uint32_t dedge_next_4(uint32_t e) { return (e % 4 == 3) ? e - 3 : e + 1; }

inline uint32_t dedge_prev(uint32_t e, uint32_t deg) { return (e % deg == 0u) ? e + (deg - 1) : e - 1; }
inline uint32_t dedge_next(uint32_t e, uint32_t deg) { return (e % deg == deg - 1) ? e - (deg - 1) : e + 1; }

extern void build_dedge(const MatrixXu &F, const MatrixXf &V, VectorXu &V2E,
                         VectorXu &E2E, VectorXb &boundary, VectorXb &nonManifold,
                         const ProgressCallback &progress = ProgressCallback(),
                         bool quiet = false);
