/*
    cleanup.cpp -- functionality to greedily drop non-manifold elements from a mesh

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include "dedge.h"
#include "field.h"
#include <set>

void remove_nonmanifold(MatrixXu &F, MatrixXf &V, MatrixXf &Nf) {
    typedef std::pair<uint32_t, uint32_t> Edge;

    std::map<uint32_t, std::map<uint32_t, std::pair<uint32_t, uint32_t>>> irregular;
    std::vector<std::set<uint32_t>> E(V.cols());
    std::vector<std::set<uint32_t>> VF(V.cols());

    auto kill_face_single = [&](uint32_t f) {
        if (F(0, f) == INVALID)
            return;
        for (int i=0; i<F.rows(); ++i)
            E[F(i, f)].erase(F((i+1)%F.rows(), f));
        F.col(f).setConstant(INVALID);
    };

    auto kill_face = [&](uint32_t f) {
        if (F.rows() == 4 && F(2, f) == F(3, f)) {
            auto it = irregular.find(F(2, f));
            if (it != irregular.end()) {
                for (auto &item : it->second) {
                    kill_face_single(item.second.second);
                }
            }
        }
        kill_face_single(f);
    };


    uint32_t nm_edge = 0, nm_vert = 0;

    for (uint32_t f=0; f < (uint32_t) F.cols(); ++f) {
        if (F(0, f) == INVALID)
            continue;
        if (F.rows() == 4 && F(2, f) == F(3, f)) {
            /* Special handling of irregular faces */
            irregular[F(2, f)][F(0, f)] = std::make_pair(F(1, f), f);
            continue;
        }

        bool nonmanifold = false;
        for (uint32_t e=0; e<F.rows(); ++e) {
            uint32_t v0 = F(e, f), v1 = F((e + 1) % F.rows(), f), v2 = F((e + 2) % F.rows(), f);
            if (E[v0].find(v1) != E[v0].end() ||
                (F.rows() == 4 && E[v0].find(v2) != E[v0].end()))
                nonmanifold = true;
        }

        if (nonmanifold) {
            nm_edge++;
            F.col(f).setConstant(INVALID);
            continue;
        }

        for (uint32_t e=0; e<F.rows(); ++e) {
            uint32_t v0 = F(e, f), v1 = F((e + 1) % F.rows(), f), v2 = F((e + 2) % F.rows(), f);

            E[v0].insert(v1);
            if (F.rows() == 4)
                E[v0].insert(v2);
            VF[v0].insert(f);
        }
    }

    std::vector<Edge> edges;
    for (auto item : irregular) {
        bool nonmanifold = false;
        auto face = item.second;
        edges.clear();

        uint32_t cur = face.begin()->first, stop = cur;
        while (true) {
            uint32_t pred = cur;
            cur = face[cur].first;
            uint32_t next = face[cur].first, it = 0;
            while (true) {
                ++it;
                if (next == pred)
                    break;
                if (E[cur].find(next) != E[cur].end() && it == 1)
                    nonmanifold = true;
                edges.push_back(Edge(cur, next));
                next = face[next].first;
            }
            if (cur == stop)
                break;
        }

        if (nonmanifold) {
            nm_edge++;
            for (auto &i : item.second)
                F.col(i.second.second).setConstant(INVALID);
            continue;
        } else {
            for (auto e : edges) {
                E[e.first].insert(e.second);

                for (auto e2 : face)
                    VF[e.first].insert(e2.second.second);
            }
        }
    }

    /* Check vertices */
    std::set<uint32_t> v_marked, v_unmarked, f_adjacent;

    std::function<void(uint32_t)> dfs = [&](uint32_t i) {
        v_marked.insert(i);
        v_unmarked.erase(i);

        for (uint32_t f : VF[i]) {
            if (f_adjacent.find(f) == f_adjacent.end()) /* if not part of adjacent face */
                continue;
            for (uint32_t j = 0; j<F.rows(); ++j) {
                uint32_t k = F(j, f);
                if (v_unmarked.find(k) == v_unmarked.end() || /* if not unmarked OR */
                    v_marked.find(k) != v_marked.end()) /* if already marked */
                    continue;
                dfs(k);
            }
        }
    };

    for (uint32_t i = 0; i < (uint32_t) V.cols(); ++i) {
        v_marked.clear();
        v_unmarked.clear();
        f_adjacent.clear();

        for (uint32_t f : VF[i]) {
            if (F(0, f) == INVALID)
                continue;

            for (uint32_t k=0; k<F.rows(); ++k)
                v_unmarked.insert(F(k, f));

            f_adjacent.insert(f);
        }

        if (v_unmarked.empty())
            continue;
        v_marked.insert(i);
        v_unmarked.erase(i);

        dfs(*v_unmarked.begin());

        if (v_unmarked.size() > 0) {
            nm_vert++;
            for (uint32_t f : f_adjacent)
                kill_face(f);
        }
    }

    if (nm_vert > 0 || nm_edge > 0)
        cout << "Non-manifold elements:  vertices=" << nm_vert << ", edges=" << nm_edge << endl;

    uint32_t nFaces = 0, nFacesOrig = F.cols();
    for (uint32_t f = 0; f < (uint32_t) F.cols(); ++f) {
        if (F(0, f) == INVALID)
            continue;
        if (nFaces != f) {
            F.col(nFaces) = F.col(f);
            Nf.col(nFaces) = Nf.col(f);
        }
        ++nFaces;
    }

    if (nFacesOrig != nFaces) {
        F.conservativeResize(F.rows(), nFaces);
        Nf.conservativeResize(Nf.rows(), nFaces);
        cout << "Faces reduced from " << nFacesOrig << " -> " << nFaces << endl;
    }
}

