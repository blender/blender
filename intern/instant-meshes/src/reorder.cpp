/*
    reorder.cpp: Reorder mesh face/vertex indices to improve coherence in various
    applications

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include "reorder.h"
#include "dedge.h"

#include <unordered_set>
#include <unordered_map>
#include <queue>

void reorder_mesh(MatrixXu &F, std::vector<MatrixXf> &V_vec, std::vector<MatrixXf> &F_vec,
                  const ProgressCallback &progress) {
    /* Build a directed edge data structure */
    VectorXu V2E, E2E;
    VectorXb boundary, nonManifold;
    build_dedge(F, V_vec[0], V2E, E2E, boundary, nonManifold, progress, true);

    std::unordered_set<uint32_t> unprocessed(F.cols());
    for (uint32_t i=0; i<F.cols(); ++i)
        unprocessed.insert(i);

    std::queue<uint32_t> queue;
    std::unordered_map<uint32_t, uint32_t> v_map(V_vec[0].cols());

    uint32_t nF = 0, nV = 0;

    MatrixXu Fp(F.rows(), F.cols());
    std::vector<MatrixXf> V_new(V_vec.size()), F_new(F_vec.size());

    for (uint32_t i=0; i<V_vec.size(); ++i) {
        if (V_vec[i].cols() != V_vec[0].cols())
            throw std::runtime_error("reorder_mesh: inconsistent input!");
        V_new[i].resize(V_vec[i].rows(), V_vec[i].cols());
    }

    for (uint32_t i=0; i<F_vec.size(); ++i) {
        if (F_vec[i].cols() != F.cols())
            throw std::runtime_error("reorder_mesh: inconsistent input!");
        F_new[i].resize(F_vec[i].rows(), F_vec[i].cols());
    }

    while (!unprocessed.empty()) {
        if (queue.empty()) {
            auto it = unprocessed.begin();
            queue.push(*it);
            unprocessed.erase(it);
        }

        while (!queue.empty()) {
            uint32_t f_old = queue.front();
            uint32_t f_new = nF++;
            queue.pop();
            for (uint32_t j=0; j<F_vec.size(); ++j)
                F_new[j].col(f_new) = F_vec[j].col(f_old);

            for (uint32_t i=0; i<F.rows(); ++i) {
                uint32_t v_old = F(i, f_old);

                uint32_t v_new;
                auto it_v = v_map.find(v_old);
                if (it_v != v_map.end()) {
                    v_new = it_v->second;
                } else {
                    v_new = v_map[v_old] = nV++;
                    for (uint32_t j=0; j<V_vec.size(); ++j)
                        V_new[j].col(v_new) = V_vec[j].col(v_old);
                }

                Fp(i, f_new) = v_new;

                uint32_t edge_other = E2E[f_old * F.rows() + i];
                if (edge_other != INVALID) {
                    uint32_t f_neighbor = edge_other / F.rows();
                    auto it_f = unprocessed.find(f_neighbor);
                    if (it_f != unprocessed.end()) {
                        queue.push(f_neighbor);
                        unprocessed.erase(f_neighbor);
                    }
                }
            }

            if (progress && unprocessed.size() % 10000 == 0)
                progress("Reordering mesh indices", 1-unprocessed.size() / (Float) F.cols());
        }
    }

    F = std::move(Fp);
    for (uint32_t i=0; i<V_vec.size(); ++i) {
        V_vec[i] = std::move(V_new[i]);
        V_vec[i].conservativeResize(V_vec[i].rows(), nV);
    }
    for (uint32_t i=0; i<F_vec.size(); ++i) {
        F_vec[i] = std::move(F_new[i]);
        F_vec[i].conservativeResize(F_vec[i].rows(), nF);
    }
}

void replicate_vertices(MatrixXu &F, std::vector<MatrixXf> &V) {
    std::vector<MatrixXf> R(V.size());

    for (uint32_t i=0; i<V.size(); ++i)
        R[i].resize(V[i].rows(), F.size());

    for (uint32_t i=0; i<F.size(); ++i) {
        uint32_t &idx = F.data()[i];
        for (uint32_t j=0; j<V.size(); ++j)
            R[j].col(i) = V[j].col(idx);
        idx = i;
    }

    V = std::move(R);
}
