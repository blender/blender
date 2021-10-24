/*
    aabb.cpp -- functionality for creating adjacency matrices together with
                uniform or cotangent weights. Also contains data structures
                used to store integer variables.

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include "adjacency.h"
#include "dedge.h"
#include "bvh.h"
#include "meshstats.h"
#include "dset.h"
#include <set>
#include <map>

AdjacencyMatrix generate_adjacency_matrix_uniform(
    const MatrixXu &F, const VectorXu &V2E, const VectorXu &E2E,
    const VectorXb &nonManifold, const ProgressCallback &progress) {
    VectorXu neighborhoodSize(V2E.size() + 1);
    cout << "Generating adjacency matrix .. ";
    cout.flush();
    Timer<> timer;

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) V2E.size(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t i = range.begin(); i != range.end(); ++i) {
                uint32_t edge = V2E[i], stop = edge;
                if (nonManifold[i] || edge == INVALID) {
                    neighborhoodSize[i+1] = 0;
                    continue;
                }
                uint32_t nNeighbors = 0;
                do {
                    uint32_t opp = E2E[edge];
                    if (opp == INVALID) {
                        nNeighbors += 2;
                        break;
                    }
                    edge = dedge_next_3(opp);
                    nNeighbors++;
                } while (edge != stop);
                neighborhoodSize[i+1] = nNeighbors;
            }
            SHOW_PROGRESS_RANGE(range, V2E.size(), "Generating adjacency matrix (1/2)");
        }
    );

    neighborhoodSize[0] = 0;
    for (uint32_t i=0; i<neighborhoodSize.size()-1; ++i)
        neighborhoodSize[i+1] += neighborhoodSize[i];

    AdjacencyMatrix adj = new Link*[V2E.size() + 1];
    uint32_t nLinks = neighborhoodSize[neighborhoodSize.size()-1];
    Link *links = new Link[nLinks];
    for (uint32_t i=0; i<neighborhoodSize.size(); ++i)
        adj[i] = links + neighborhoodSize[i];

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) V2E.size(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t i = range.begin(); i != range.end(); ++i) {
                uint32_t edge = V2E[i], stop = edge;
                if (nonManifold[i] || edge == INVALID)
                    continue;
                Link *ptr = adj[i];

                int it = 0;
                do {
                    uint32_t base = edge % 3, f = edge / 3;
                    uint32_t opp = E2E[edge], next = dedge_next_3(opp);
                    if (it == 0)
                        *ptr++ = Link(F((base + 2)%3, f));
                    if (opp == INVALID || next != stop) {
                        *ptr++ = Link(F((base + 1)%3, f));
                        if (opp == INVALID)
                            break;
                    }
                    edge = next;
                    ++it;
                } while (edge != stop);
            }
            SHOW_PROGRESS_RANGE(range, V2E.size(), "Generating adjacency matrix (2/2)");
        }
    );

    cout << "done. (took " << timeString(timer.value()) << ")" << endl;

    return adj;
}

AdjacencyMatrix
generate_adjacency_matrix_cotan(const MatrixXu &F, const MatrixXf &V,
                                const VectorXu &V2E, const VectorXu &E2E,
                                const VectorXb &nonManifold,
                                const ProgressCallback &progress) {
    VectorXu neighborhoodSize(V2E.size() + 1);
    cout << "Computing cotangent Laplacian .. ";
    cout.flush();
    Timer<> timer;

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) V2E.size(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t i = range.begin(); i != range.end(); ++i) {
                uint32_t edge = V2E[i], stop = edge;
                if (nonManifold[i] || edge == INVALID) {
                    neighborhoodSize[i+1] = 0;
                    continue;
                }
                uint32_t nNeighbors = 0;
                do {
                    uint32_t opp = E2E[edge];
                    if (opp == INVALID) {
                        nNeighbors += 2;
                        break;
                    }
                    edge = dedge_next_3(opp);
                    nNeighbors++;
                } while (edge != stop);
                neighborhoodSize[i+1] = nNeighbors;
            }
            SHOW_PROGRESS_RANGE(range, V2E.size(), "Computing cotangent Laplacian (1/2)");
        }
    );

    neighborhoodSize[0] = 0;
    for (uint32_t i=0; i<neighborhoodSize.size()-1; ++i)
        neighborhoodSize[i+1] += neighborhoodSize[i];

    AdjacencyMatrix adj = new Link*[V2E.size() + 1];
    uint32_t nLinks = neighborhoodSize[neighborhoodSize.size()-1];
    Link *links = new Link[nLinks];
    for (uint32_t i=0; i<neighborhoodSize.size(); ++i)
        adj[i] = links + neighborhoodSize[i];

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t)V.cols(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t i = range.begin(); i != range.end(); ++i) {
                uint32_t edge = V2E[i], stop = edge;
                if (nonManifold[i] || edge == INVALID)
                    continue;
                Link *ptr = adj[i];

                int it = 0;
                do {
                    uint32_t f = edge / 3, curr_idx = edge % 3,
                             next_idx = (curr_idx + 1) % 3,
                             prev_idx = (curr_idx + 2) % 3;

                    if (it == 0) {
                        Vector3f p  = V.col(F(next_idx, f)),
                                 d0 = V.col(F(prev_idx, f)) - p,
                                 d1 = V.col(F(curr_idx, f)) - p;
                        Float cot_weight = 0.0f,
                              sin_alpha = d0.cross(d1).norm();
                        if (sin_alpha > RCPOVERFLOW)
                            cot_weight = d0.dot(d1) / sin_alpha;

                        uint32_t opp = E2E[dedge_prev_3(edge)];
                        if (opp != INVALID) {
                            uint32_t o_f = opp / 3, o_curr_idx = opp % 3,
                                     o_next_idx = (o_curr_idx + 1) % 3,
                                     o_prev_idx = (o_curr_idx + 2) % 3;
                            p  = V.col(F(o_prev_idx, o_f));
                            d0 = V.col(F(o_curr_idx, o_f)) - p;
                            d1 = V.col(F(o_next_idx, o_f)) - p;
                            sin_alpha = d0.cross(d1).norm();
                            if (sin_alpha > RCPOVERFLOW)
                                cot_weight += d0.dot(d1) / sin_alpha;
                        }

                        *ptr++ = Link(F(prev_idx, f), (float) cot_weight * 0.5f);
                    }
                    uint32_t opp = E2E[edge], next = dedge_next_3(opp);
                    if (opp == INVALID || next != stop) {
                        Vector3f p  = V.col(F(prev_idx, f)),
                                 d0 = V.col(F(curr_idx, f)) - p,
                                 d1 = V.col(F(next_idx, f)) - p;
                        Float cot_weight = 0.0f,
                              sin_alpha = d0.cross(d1).norm();
                        if (sin_alpha > RCPOVERFLOW)
                            cot_weight = d0.dot(d1) / sin_alpha;

                        if (opp != INVALID) {
                            uint32_t o_f = opp / 3, o_curr_idx = opp % 3,
                                     o_next_idx = (o_curr_idx + 1) % 3,
                                     o_prev_idx = (o_curr_idx + 2) % 3;
                            p  = V.col(F(o_prev_idx, o_f));
                            d0 = V.col(F(o_curr_idx, o_f)) - p;
                            d1 = V.col(F(o_next_idx, o_f)) - p;
                            sin_alpha = d0.cross(d1).norm();
                            if (sin_alpha > RCPOVERFLOW)
                                cot_weight += d0.dot(d1) / sin_alpha;
                        }

                        *ptr++ = Link(F(next_idx, f), (float) cot_weight * 0.5f);
                        if (opp == INVALID)
                            break;
                    }
                    edge = next;
                    ++it;
                } while (edge != stop);
            }
            SHOW_PROGRESS_RANGE(range, V.cols(), "Computing cotangent Laplacian (2/2)");
        }
    );
    cout << "done. (took " << timeString(timer.value()) << ")" << endl;
    return adj;
}

AdjacencyMatrix generate_adjacency_matrix_pointcloud(
    MatrixXf &V, MatrixXf &N, const BVH *bvh, MeshStats &stats, uint32_t knn_points,
    bool deterministic, const ProgressCallback &progress) {
    Timer<> timer;
    cout << "Generating adjacency matrix .. ";
    cout.flush();

    stats.mAverageEdgeLength = bvh->diskRadius();
    const Float maxQueryRadius = bvh->diskRadius() * 3;

    uint32_t *adj_sets = new uint32_t[V.cols() * (size_t) knn_points];

    DisjointSets dset(V.cols());
    VectorXu adj_size(V.cols());
    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) V.cols(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            std::vector<std::pair<Float, uint32_t>> result;
            for (uint32_t i = range.begin(); i < range.end(); ++i) {
                uint32_t *adj_set = adj_sets + (size_t) i * (size_t) knn_points;
                memset(adj_set, 0xFF, sizeof(uint32_t) * knn_points);
                Float radius = maxQueryRadius;
                bvh->findKNearest(V.col(i), N.col(i), knn_points, radius, result);
                uint32_t ctr = 0;
                for (auto k : result) {
                    if (k.second == i)
                        continue;
                    adj_set[ctr++] = k.second;
                    dset.unite(k.second, i);
                }
                adj_size[i] = ctr;
            }
            SHOW_PROGRESS_RANGE(range, V.cols(), "Generating adjacency matrix");
        }
    );

    std::map<uint32_t, uint32_t> dset_size;
    for (uint32_t i=0; i<V.cols(); ++i) {
        dset_size[dset.find(i)]++;
        uint32_t *adj_set_i = adj_sets + (size_t) i * (size_t) knn_points;

        for (uint32_t j=0; j<knn_points; ++j) {
            uint32_t k = adj_set_i[j];
            if (k == INVALID)
                break;
            uint32_t *adj_set_k = adj_sets + (size_t) k * (size_t) knn_points;
            bool found = false;
            for (uint32_t l=0; l<knn_points; ++l) {
                uint32_t value = adj_set_k[l];
                if (value == i) { found = true; break; }
                if (value == INVALID) break;
            }
            if (!found)
                adj_size[k]++;
        }
    }

    size_t nLinks = 0;
    for (uint32_t i=0; i<V.cols(); ++i) {
        uint32_t dsetSize = dset_size[dset.find(i)];
        if (dsetSize < V.cols() * 0.01f) {
            adj_size[i] = INVALID;
            V.col(i) = Vector3f::Constant(1e6);
            continue;
        }
        nLinks += adj_size[i];
    }

    cout << "allocating " << memString(sizeof(Link) * nLinks) << " .. ";
    cout.flush();

    AdjacencyMatrix adj = new Link*[V.size() + 1];
    adj[0] = new Link[nLinks];
    for (uint32_t i=1; i<=V.cols(); ++i) {
        uint32_t size = adj_size[i-1];
        if (size == INVALID)
            size = 0;
        adj[i] = adj[i-1] + size;
    }

    VectorXu adj_offset(V.cols());
    adj_offset.setZero();

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) V.cols(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t i = range.begin(); i < range.end(); ++i) {
                uint32_t *adj_set_i = adj_sets + (size_t) i * (size_t) knn_points;
                if (adj_size[i] == INVALID)
                    continue;

                for (uint32_t j=0; j<knn_points; ++j) {
                    uint32_t k = adj_set_i[j];
                    if (k == INVALID)
                        break;
                    adj[i][atomicAdd(&adj_offset.coeffRef(i), 1)-1] = Link(k);

                    uint32_t *adj_set_k = adj_sets + (size_t) k * (size_t) knn_points;
                    bool found = false;
                    for (uint32_t l=0; l<knn_points; ++l) {
                        uint32_t value = adj_set_k[l];
                        if (value == i) { found = true; break; }
                        if (value == INVALID) break;
                    }
                    if (!found)
                        adj[k][atomicAdd(&adj_offset.coeffRef(k), 1)-1] = Link(i);
                }
            }
        }
    );

    /* Use a heuristic to estimate some useful quantities for point clouds (this
       is a biased estimate due to the kNN queries, but it's convenient and
       reasonably accurate) */
    stats.mSurfaceArea = M_PI * stats.mAverageEdgeLength*stats.mAverageEdgeLength * 0.5f * V.cols();

    cout << "done. (took " << timeString(timer.value()) << ")" << endl;
    return adj;
}
