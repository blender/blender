/*
    subdivide.cpp: Subdivides edges in a triangle mesh until all edges
    are below a specified maximum length

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include "subdivide.h"
#include "dedge.h"

void subdivide(MatrixXu &F, MatrixXf &V, VectorXu &V2E, VectorXu &E2E,
               VectorXb &boundary, VectorXb &nonmanifold, Float maxLength,
               bool deterministic,
               const ProgressCallback &progress) {
    typedef std::pair<uint32_t, Float> Edge;

    struct EdgeComp {
        bool operator()(const Edge& u, const Edge& v) const {
            return u.second < v.second;
        }
    };

    tbb::concurrent_priority_queue<Edge, EdgeComp> queue;

    maxLength *= maxLength;

    cout << "Subdividing mesh .. ";
    cout.flush();
    Timer<> timer;

    if (progress)
        progress("Subdividing mesh", 0.0f);

    tbb::blocked_range<uint32_t> range(0u, (uint32_t) E2E.size(), GRAIN_SIZE);

    auto subdiv = [&](const tbb::blocked_range<uint32_t> &range) {
        for (uint32_t i = range.begin(); i<range.end(); ++i) {
            uint32_t v0 = F(i%3, i/3), v1 = F((i+1)%3, i/3);
            if (nonmanifold[v0] || nonmanifold[v1])
                continue;
            Float length = (V.col(v0) - V.col(v1)).squaredNorm();
            if (length > maxLength) {
                uint32_t other = E2E[i];
                if (other == INVALID || other > i)
                    queue.push(Edge(i, length));
            }
        }
        SHOW_PROGRESS_RANGE(range, E2E.size(), "Subdividing mesh (1/2)");
    };

    if (!deterministic)
        tbb::parallel_for(range, subdiv);
    else
        subdiv(range);

    uint32_t nV = V.cols(), nF = F.cols(), nSplit = 0;
    /*
           /   v0  \
         v1p 1 | 0 v0p
           \   v1  /

           /   v0  \
          /  1 | 0  \
         v1p - vn - v0p
          \  2 | 3  /
           \   v1  /

        f0: vn, v0p, v0
        f1: vn, v0, v1p
        f2: vn, v1p, v1
        f3: vn, v1, v0p
   */

    while (!queue.empty()) {
        Edge edge;
        if (!queue.try_pop(edge))
            return;
        uint32_t e0 = edge.first, e1 = E2E[e0];
        bool is_boundary = e1 == INVALID;
        uint32_t f0 = e0/3, f1 = is_boundary ? INVALID : (e1 / 3);
        uint32_t v0 = F(e0%3, f0), v0p = F((e0+2)%3, f0), v1 = F((e0+1)%3, f0);
        if ((V.col(v0) - V.col(v1)).squaredNorm() != edge.second)
            continue;

        uint32_t v1p = is_boundary ? INVALID : F((e1+2)%3, f1);
        uint32_t vn = nV++;
        nSplit++;

        /* Update V */
        if (nV > V.cols()) {
            V.conservativeResize(V.rows(), V.cols() * 2);
            V2E.conservativeResize(V.cols());
            boundary.conservativeResize(V.cols());
            nonmanifold.conservativeResize(V.cols());
        }

        /* Update V */
        V.col(vn) = (V.col(v0) + V.col(v1)) * 0.5f;
        nonmanifold[vn] = false;
        boundary[vn] = is_boundary;

        /* Update F and E2E */
        uint32_t f2 = is_boundary ? INVALID : (nF++);
        uint32_t f3 = nF++;

        if (nF > F.cols()) {
            F.conservativeResize(F.rows(), std::max(nF, (uint32_t) F.cols() * 2));
            E2E.conservativeResize(F.cols()*3);
        }

        /* Update F */
        F.col(f0) << vn, v0p, v0;
        if (!is_boundary) {
            F.col(f1) << vn, v0, v1p;
            F.col(f2) << vn, v1p, v1;
        }
        F.col(f3) << vn, v1, v0p;

        /* Update E2E */
        const uint32_t e0p = E2E[dedge_prev_3(e0)],
                       e0n = E2E[dedge_next_3(e0)];

        #define sE2E(a, b) E2E[a] = b; if (b != INVALID) E2E[b] = a;
        sE2E(3*f0+0, 3*f3+2);
        sE2E(3*f0+1, e0p);
        sE2E(3*f3+1, e0n);
        if (is_boundary) {
            sE2E(3*f0+2, INVALID);
            sE2E(3*f3+0, INVALID);
        } else {
            const uint32_t e1p = E2E[dedge_prev_3(e1)],
                           e1n = E2E[dedge_next_3(e1)];
            sE2E(3*f0+2, 3*f1+0);
            sE2E(3*f1+1, e1n);
            sE2E(3*f1+2, 3*f2+0);
            sE2E(3*f2+1, e1p);
            sE2E(3*f2+2, 3*f3+0);
        }
        #undef sE2E

        /* Update V2E */
        V2E[v0]  = 3*f0 + 2;
        V2E[vn]  = 3*f0 + 0;
        V2E[v1]  = 3*f3 + 1;
        V2E[v0p] = 3*f0 + 1;
        if (!is_boundary)
            V2E[v1p] = 3*f1 + 2;

        auto schedule = [&](uint32_t f) {
            for (int i=0; i<3; ++i) {
                Float length = (V.col(F(i, f))-V.col(F((i+1)%3, f))).squaredNorm();
                if (length > maxLength)
                    queue.push(Edge(f*3+i, length));
            }
        };

        schedule(f0);
        if (!is_boundary) {
            schedule(f2);
            schedule(f1);
        };
        schedule(f3);
    }
    F.conservativeResize(F.rows(), nF);
    V.conservativeResize(V.rows(), nV);
    V2E.conservativeResize(nV);
    boundary.conservativeResize(nV);
    nonmanifold.conservativeResize(nV);
    E2E.conservativeResize(nF*3);

    cout << "done. (split " << nSplit << " edges, took "
         << timeString(timer.value()) << ", new V=" << V.cols()
         << ", F=" << F.cols() << ", took " << timeString(timer.value()) << ")" << endl;
}
