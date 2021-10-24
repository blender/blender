/*
    meshstats.cpp: Routines to efficiently compute various mesh statistics such
    as the bounding box, surface area, etc.

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include "meshstats.h"
#include "dedge.h"

MeshStats compute_mesh_stats(const MatrixXu &F, const MatrixXf &V,
                             bool deterministic,
                             const ProgressCallback &progress) {
    Timer<> timer;
    MeshStats stats;
    if (F.size() != 0) {
        cout << "Computing mesh statistics .. ";
        cout.flush();
        auto map = [&](const tbb::blocked_range<uint32_t> &range, MeshStats stats) -> MeshStats {
            for (uint32_t f = range.begin(); f != range.end(); ++f) {
                Vector3f v[3] = { V.col(F(0, f)), V.col(F(1, f)), V.col(F(2, f)) };
                Vector3f face_center = Vector3f::Zero();

                for (int i=0; i<3; ++i) {
                    Float edge_length = (v[i] - v[i == 2 ? 0 : (i+1)]).norm();
                    stats.mAverageEdgeLength += edge_length;
                    stats.mMaximumEdgeLength = std::max(stats.mMaximumEdgeLength, (double) edge_length);
                    stats.mAABB.expandBy(v[i]);
                    face_center += v[i];
                }
                face_center *= 1.0f / 3.0f;

                Float face_area = 0.5f * (v[1]-v[0]).cross(v[2]-v[0]).norm();
                stats.mSurfaceArea += face_area;
                stats.mWeightedCenter += face_area * face_center;
            }
            SHOW_PROGRESS_RANGE(range, F.cols(), "Computing mesh statistics");
            return stats;
        };

        auto reduce = [](MeshStats s0, MeshStats s1) -> MeshStats {
            MeshStats result;
            result.mSurfaceArea = s0.mSurfaceArea + s1.mSurfaceArea;
            result.mWeightedCenter = s0.mWeightedCenter + s1.mWeightedCenter;
            result.mAverageEdgeLength = 
                s0.mAverageEdgeLength + s1.mAverageEdgeLength;
            result.mMaximumEdgeLength =
                std::max(s0.mMaximumEdgeLength, s1.mMaximumEdgeLength);
            result.mAABB = AABB::merge(s0.mAABB, s1.mAABB);
            return result;
        };

        tbb::blocked_range<uint32_t> range(0u, (uint32_t) F.cols(), GRAIN_SIZE);

        if (deterministic)
            stats = tbb::parallel_deterministic_reduce(range, MeshStats(), map, reduce);
        else
            stats = tbb::parallel_reduce(range, MeshStats(), map, reduce);

        stats.mAverageEdgeLength /= F.cols() * 3;
        stats.mWeightedCenter /= stats.mSurfaceArea;
    } else {
        cout << "Computing point cloud statistics .. ";
        cout.flush();
        auto map = [&](const tbb::blocked_range<uint32_t> &range, MeshStats stats) -> MeshStats {
            for (uint32_t i = range.begin(); i != range.end(); ++i) {
                const Vector3f &v = V.col(i);
                stats.mAABB.expandBy(v);
                stats.mWeightedCenter += v;
            }
            SHOW_PROGRESS_RANGE(range, V.cols(), "Computing point cloud statistics");
            return stats;
        };

        auto reduce = [](MeshStats s0, MeshStats s1) -> MeshStats {
            MeshStats result;
            result.mWeightedCenter = s0.mWeightedCenter + s1.mWeightedCenter;
            result.mAABB = AABB::merge(s0.mAABB, s1.mAABB);
            return result;
        };

        tbb::blocked_range<uint32_t> range(0u, (uint32_t) V.cols(), GRAIN_SIZE);

        if (deterministic)
            stats = tbb::parallel_deterministic_reduce(range, MeshStats(), map, reduce);
        else
            stats = tbb::parallel_reduce(range, MeshStats(), map, reduce);

        stats.mSurfaceArea = stats.mAverageEdgeLength = stats.mMaximumEdgeLength = 0;
        stats.mWeightedCenter /= V.cols();
    }

    cout << "done. (took " << timeString(timer.value()) << ")" << endl;

    return stats;
}

void compute_dual_vertex_areas(const MatrixXu &F, const MatrixXf &V,
                                 const VectorXu &V2E, const VectorXu &E2E,
                                 const VectorXb &nonManifold, VectorXf &A,
                                 const ProgressCallback &progress) {
    A.resize(V.cols());
    A.setZero();
    cout << "Computing dual vertex areas .. ";
    cout.flush();
    Timer<> timer;

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) V.cols(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t i = range.begin(); i != range.end(); ++i) {
                uint32_t edge = V2E[i], stop = edge;
                if (nonManifold[i] || edge == INVALID)
                    continue;
                Float vertex_area = 0;
                do {
                    uint32_t ep = dedge_prev_3(edge), en = dedge_next_3(edge); 

                    Vector3f v = V.col(F(edge%3, edge/3));
                    Vector3f vn = V.col(F(en%3, en/3));
                    Vector3f vp = V.col(F(ep%3, ep/3));

                    Vector3f face_center = (v + vp + vn) * (1.0f / 3.0f);
                    Vector3f prev = (v+vp) * 0.5f;
                    Vector3f next = (v+vn) * 0.5f;

                    vertex_area +=
                        0.5f * ((v - prev).cross(v - face_center).norm() +
                                (v - next).cross(v - face_center).norm());

                    uint32_t opp = E2E[edge];
                    if (opp == INVALID)
                        break;
                    edge = dedge_next_3(opp);
                } while (edge != stop);

                A[i] = vertex_area;
            }
            SHOW_PROGRESS_RANGE(range, V.cols(), "Computing dual vertex areas");
        }
    );

    cout << "done. (took " << timeString(timer.value()) << ")" << endl;
}
