/*
    normal.cpp: Helper routines for computing vertex normals

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include "normal.h"
#include "dedge.h"

extern void
generate_smooth_normals(const MatrixXu &F, const MatrixXf &V, MatrixXf &N,
                        bool deterministic,
                        const ProgressCallback &progress) {
    cout << "Computing vertex normals .. ";
    cout.flush();

    std::atomic<uint32_t> badFaces(0);
    Timer<> timer;
    N.resize(V.rows(), V.cols());
    N.setZero();

    auto map = [&](const tbb::blocked_range<uint32_t> &range) {
        for (uint32_t f = range.begin(); f != range.end(); ++f) {
            Vector3f fn = Vector3f::Zero();
            for (int i=0; i<3; ++i) {
                Vector3f v0 = V.col(F(i, f)),
                         v1 = V.col(F((i+1)%3, f)),
                         v2 = V.col(F((i+2)%3, f)),
                         d0 = v1-v0,
                         d1 = v2-v0;

                if (i == 0) {
                    fn = d0.cross(d1);
                    Float norm = fn.norm();
                    if (norm < RCPOVERFLOW) {
                        badFaces++; /* degenerate */
                        break;
                    }
                    fn /= norm;
                }

                /* "Computing Vertex Normals from Polygonal Facets"
                    by Grit Thuermer and Charles A. Wuethrich, JGT 1998, Vol 3 */
                Float angle = fast_acos(d0.dot(d1) / std::sqrt(d0.squaredNorm() * d1.squaredNorm()));
                for (uint32_t k=0; k<3; ++k)
                    atomicAdd(&N.coeffRef(k, F(i, f)), fn[k]*angle);
            }
        }
        SHOW_PROGRESS_RANGE(range, F.cols(), "Computing vertex normals (1/2)");
    };

    tbb::blocked_range<uint32_t> range(0u, (uint32_t) F.cols(), GRAIN_SIZE);

    if (!deterministic)
        tbb::parallel_for(range, map);
    else
        map(range);

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) V.cols(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t i = range.begin(); i != range.end(); ++i) {
                Float norm = N.col(i).norm();
                if (norm < RCPOVERFLOW) {
                    N.col(i) = Vector3f::UnitX();
                } else {
                    N.col(i) /= norm;
                }
            }
            SHOW_PROGRESS_RANGE(range, V.cols(), "Computing vertex normals (2/2)");
        }
    );

    cout << "done. (";
    if (badFaces > 0)
        cout << badFaces << " degenerate faces, ";
    cout << "took " << timeString(timer.value()) << ")" << endl;
}

void generate_smooth_normals(const MatrixXu &F, const MatrixXf &V, const VectorXu &V2E,
                             const VectorXu &E2E, const VectorXb &nonManifold,
                             MatrixXf &N, const ProgressCallback &progress) {
    cout << "Computing vertex normals .. ";
    cout.flush();

    std::atomic<uint32_t> badFaces(0);
    Timer<> timer;

    /* Compute face normals */
    MatrixXf Nf(3, F.cols());

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) F.cols(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t f = range.begin(); f != range.end(); ++f) {
                Vector3f v0 = V.col(F(0, f)),
                         v1 = V.col(F(1, f)),
                         v2 = V.col(F(2, f)),
                         n = (v1-v0).cross(v2-v0);
                Float norm = n.norm();
                if (norm < RCPOVERFLOW) {
                    badFaces++; /* degenerate */
                    n = Vector3f::UnitX();
                } else {
                    n /= norm;
                }
                Nf.col(f) = n;
            }
            SHOW_PROGRESS_RANGE(range, F.cols(), "Computing vertex normals (1/2)");
        }
    );

    N.resize(3, V.cols());

    /* Finally, compute the normals */
    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) V.cols(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t i = range.begin(); i != range.end(); ++i) {
                uint32_t edge = V2E[i];
                if (nonManifold[i] || edge == INVALID) {
                    N.col(i) = Vector3f::UnitX();
                    continue;
                }

                uint32_t stop = edge;
                Vector3f normal = Vector3f::Zero();
                do {
                    uint32_t idx = edge % 3;

                    Vector3f d0 = V.col(F((idx+1)%3, edge/3)) - V.col(i);
                    Vector3f d1 = V.col(F((idx+2)%3, edge/3)) - V.col(i);
                    Float angle = fast_acos(d0.dot(d1) / std::sqrt(d0.squaredNorm() * d1.squaredNorm()));

                    /* "Computing Vertex Normals from Polygonal Facets"
                        by Grit Thuermer and Charles A. Wuethrich, JGT 1998, Vol 3 */
                    if (std::isfinite(angle))
                        normal += Nf.col(edge/3) * angle;

                    uint32_t opp = E2E[edge];
                    if (opp == INVALID)
                        break;

                    edge = dedge_next_3(opp);
                } while (edge != stop);
                Float norm = normal.norm();
                N.col(i) = norm > RCPOVERFLOW ? Vector3f(normal / norm)
                                              : Vector3f::UnitX();
            }
            SHOW_PROGRESS_RANGE(range, V.cols(), "Computing vertex normals (2/2)");
        }
    );

    cout << "done. (";
    if (badFaces > 0)
        cout << badFaces << " degenerate faces, ";
    cout << "took " << timeString(timer.value()) << ")" << endl;
}

void generate_crease_normals(MatrixXu &F, MatrixXf &V, const VectorXu &_V2E,
                             const VectorXu &E2E, const VectorXb boundary,
                             const VectorXb &nonManifold, Float angleThreshold,
                             MatrixXf &N, std::map<uint32_t, uint32_t> &creases,
                             const ProgressCallback &progress) {
    const Float dpThreshold = std::cos(angleThreshold * M_PI / 180);

    cout << "Computing vertex & crease normals .. ";
    cout.flush();
    creases.clear();

    std::atomic<uint32_t> badFaces(0), creaseVert(0), offset(V.cols());
    Timer<> timer;
    VectorXu V2E(_V2E);

    /* Compute face normals */
    MatrixXf Nf(3, F.cols());

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) F.cols(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t f = range.begin(); f != range.end(); ++f) {
                Vector3f v0 = V.col(F(0, f)),
                         v1 = V.col(F(1, f)),
                         v2 = V.col(F(2, f)),
                         n = (v1-v0).cross(v2-v0);
                Float norm = n.norm();
                if (norm < RCPOVERFLOW) {
                    badFaces++; /* degenerate */
                    n = Vector3f::UnitX();
                } else {
                    n /= norm;
                }
                Nf.col(f) = n;
            }
            SHOW_PROGRESS_RANGE(range, F.cols(), "Computing vertex & crease normals (1/3)");
        }
    );

    /* Determine how many extra vertices are needed, and adjust
       the vertex->edge pointers so that they are located just after
       the first crease edge */
    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) V.cols(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t i = range.begin(); i != range.end(); ++i) {
                uint32_t edge = V2E[i], stop = edge;
                if (nonManifold[i] || edge == INVALID)
                    continue;
                uint32_t creaseEdge = INVALID, nCreaseEdges = 0;
                bool is_boundary = boundary[i];
                do {
                    uint32_t opp = E2E[edge];
                    if (opp == INVALID)
                        break;
                    uint32_t nextEdge = dedge_next_3(opp);
                    if (Nf.col(edge / 3).dot(Nf.col(nextEdge / 3)) < dpThreshold) {
                        nCreaseEdges++;
                        if (creaseEdge == INVALID || creaseEdge < nextEdge)
                            creaseEdge = nextEdge;
                    }
                    edge = nextEdge;
                } while (edge != stop);

                if (creaseEdge != INVALID) {
                    if (!is_boundary)
                        V2E[i] = creaseEdge;
                    creaseVert += nCreaseEdges - (is_boundary ? 0 : 1);
                }
            }
            SHOW_PROGRESS_RANGE(range, V.cols(), "Computing vertex & crease normals (2/3)");
        }
    );

    uint32_t oldSize = V.cols();
    V.conservativeResize(3, V.cols() + creaseVert);
    N.resize(3, V.cols());

    tbb::spin_mutex mutex;

    /* Finally, compute the normals */
    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, oldSize, GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t i = range.begin(); i != range.end(); ++i) {
                uint32_t edge = V2E[i];
                if (nonManifold[i] || edge == INVALID) {
                    N.col(i) = Vector3f::UnitX();
                    continue;
                }

                uint32_t stop = edge, vertexID = i;
                Vector3f normal = Vector3f::Zero();
                do {
                    uint32_t idx = edge % 3;
                    if (vertexID != i)
                        F(idx, edge/3) = vertexID;

                    Vector3f d0 = V.col(F((idx+1)%3, edge/3)) - V.col(i);
                    Vector3f d1 = V.col(F((idx+2)%3, edge/3)) - V.col(i);
                    Float angle = fast_acos(d0.dot(d1) / std::sqrt(d0.squaredNorm() * d1.squaredNorm()));

                    /* "Computing Vertex Normals from Polygonal Facets"
                        by Grit Thuermer and Charles A. Wuethrich, JGT 1998, Vol 3 */
                    if (std::isfinite(angle))
                        normal += Nf.col(edge/3) * angle;

                    uint32_t opp = E2E[edge];
                    if (opp == INVALID) {
                        Float norm = normal.norm();
                        N.col(vertexID) = norm > RCPOVERFLOW ? Vector3f(normal / norm)
                                                             : Vector3f::UnitX();
                        break;
                    }

                    uint32_t nextEdge = dedge_next_3(opp);
                    if (Nf.col(edge / 3).dot(Nf.col(nextEdge / 3)) < dpThreshold ||
                        nextEdge == stop) {
                        Float norm = normal.norm();
                        N.col(vertexID) = norm > RCPOVERFLOW ? Vector3f(normal / norm)
                                                             : Vector3f::UnitX();
                        normal = Vector3f::Zero();
                        if (nextEdge != stop) {
                            vertexID = offset++;
                            V.col(vertexID) = V.col(i);
                            mutex.lock();
                            creases[vertexID] = i;
                            mutex.unlock();
                        }
                    }
                    edge = nextEdge;
                } while (edge != stop);
            }
            SHOW_PROGRESS_RANGE(range, oldSize, "Computing vertex & crease normals (3/3)");
        }
    );

    if (offset != (uint32_t) V.cols())
        throw std::runtime_error("Internal error (incorrect final vertex count)!");

    cout << "done. (";
    if (badFaces > 0)
        cout << badFaces << " degenerate faces, ";
    if (creaseVert > 0)
        cout << creaseVert << " crease vertices, ";
    cout << "took " << timeString(timer.value()) << ")" << endl;
}

void generate_crease_normals(const MatrixXu &F, const MatrixXf &V,
                             const VectorXu &_V2E, const VectorXu &E2E,
                             const VectorXb boundary,
                             const VectorXb &nonManifold, Float angleThreshold,
                             MatrixXf &N, std::set<uint32_t> &creases,
                             const ProgressCallback &progress) {
    const Float dpThreshold = std::cos(angleThreshold * M_PI / 180);

    cout << "Computing vertex & crease normals .. ";
    cout.flush();
    creases.clear();

    Timer<> timer;
    VectorXu V2E(_V2E);

    /* Compute face normals */
    MatrixXf Nf(3, F.cols());
    std::atomic<uint32_t> badFaces(0);

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) F.cols(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t f = range.begin(); f != range.end(); ++f) {
                Vector3f v0 = V.col(F(0, f)),
                         v1 = V.col(F(1, f)),
                         v2 = V.col(F(2, f)),
                         n = (v1-v0).cross(v2-v0);
                Float norm = n.norm();
                if (norm < RCPOVERFLOW) {
                    badFaces++; /* degenerate */
                    n = Vector3f::UnitX();
                } else {
                    n /= norm;
                }
                Nf.col(f) = n;
            }
            SHOW_PROGRESS_RANGE(range, F.cols(), "Computing vertex & crease normals (1/3)");
        }
    );

    /* Determine how many extra vertices are needed, and adjust
       the vertex->edge pointers so that they are located just after
       the first crease edge */
    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) V.cols(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t i = range.begin(); i != range.end(); ++i) {
                uint32_t edge = V2E[i], stop = edge;
                if (nonManifold[i] || edge == INVALID)
                    continue;
                uint32_t creaseEdge = INVALID;
                do {
                    uint32_t opp = E2E[edge];
                    if (opp == INVALID)
                        break;
                    uint32_t nextEdge = dedge_next_3(opp);
                    if (Nf.col(edge / 3).dot(Nf.col(nextEdge / 3)) < dpThreshold) {
                        if (creaseEdge == INVALID || creaseEdge < nextEdge)
                            creaseEdge = nextEdge;
                    }
                    edge = nextEdge;
                } while (edge != stop);

                if (creaseEdge != INVALID) {
                    if (!boundary[i])
                        V2E[i] = creaseEdge;
                }
            }
            SHOW_PROGRESS_RANGE(range, V.cols(), "Computing vertex & crease normals (2/3)");
        }
    );

    N.resize(3, V.cols());

    /* Finally, compute the normals */
    tbb::spin_mutex mutex;
    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, V.cols(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t i = range.begin(); i != range.end(); ++i) {
                uint32_t edge = V2E[i];
                if (nonManifold[i] || edge == INVALID) {
                    N.col(i) = Vector3f::UnitX();
                    continue;
                }

                uint32_t stop = edge;
                Vector3f normal = Vector3f::Zero();
                do {
                    uint32_t idx = edge % 3, face = edge/3;

                    Vector3f d0 = V.col(F((idx+1)%3, face)) - V.col(i);
                    Vector3f d1 = V.col(F((idx+2)%3, face)) - V.col(i);
                    Float angle = fast_acos(d0.dot(d1) / std::sqrt(d0.squaredNorm() * d1.squaredNorm()));

                    /* "Computing Vertex Normals from Polygonal Facets"
                        by Grit Thuermer and Charles A. Wuethrich, JGT 1998, Vol 3 */
                    if (std::isfinite(angle))
                        normal += Nf.col(edge/3) * angle;

                    uint32_t opp = E2E[edge];
                    if (opp == INVALID)
                        break;

                    uint32_t nextEdge = dedge_next_3(opp);
                    if (Nf.col(edge / 3).dot(Nf.col(nextEdge / 3)) < dpThreshold) {
                        mutex.lock();
                        creases.insert(i);
                        mutex.unlock();
                        break;
                    }

                    edge = nextEdge;
                } while (edge != stop);
                Float norm = normal.norm();
                N.col(i) = norm > RCPOVERFLOW ? Vector3f(normal / norm)
                                              : Vector3f::UnitX();
            }
            SHOW_PROGRESS_RANGE(range, V.cols(), "Computing vertex & crease normals (3/3)");
        }
    );

    cout << "done. (";
    if (badFaces > 0)
        cout << badFaces << " degenerate faces, ";
    if (!creases.empty())
        cout << creases.size() << " crease vertices, ";
    cout << "took " << timeString(timer.value()) << ")" << endl;
}
