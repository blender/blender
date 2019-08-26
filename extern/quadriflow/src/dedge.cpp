#include "dedge.hpp"
#include "config.hpp"

#include <atomic>
#include <fstream>
#include <iostream>
#include <set>
#include <vector>
#include "compare-key.hpp"
#ifdef WITH_TBB
#include "tbb/tbb.h"
#endif
namespace qflow {

inline int dedge_prev(int e, int deg) { return (e % deg == 0u) ? e + (deg - 1) : e - 1; }

inline bool atomicCompareAndExchange(volatile int* v, uint32_t newValue, int oldValue) {
#if defined(_WIN32)
    return _InterlockedCompareExchange(reinterpret_cast<volatile long*>(v), (long)newValue,
                                       (long)oldValue) == (long)oldValue;
#else
    return __sync_bool_compare_and_swap(v, oldValue, newValue);
#endif
}

const int INVALID = -1;

#undef max
#undef min
bool compute_direct_graph(MatrixXd& V, MatrixXi& F, VectorXi& V2E, VectorXi& E2E,
                          VectorXi& boundary, VectorXi& nonManifold) {
    V2E.resize(V.cols());
    V2E.setConstant(INVALID);

    uint32_t deg = F.rows();
    std::vector<std::pair<uint32_t, uint32_t>> tmp(F.size());

#ifdef WITH_TBB
    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t)F.cols(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t>& range) {
            for (uint32_t f = range.begin(); f != range.end(); ++f) {
                for (uint32_t i = 0; i < deg; ++i) {
                    uint32_t idx_cur = F(i, f), idx_next = F((i + 1) % deg, f),
                             edge_id = deg * f + i;
                    if (idx_cur >= V.cols() || idx_next >= V.cols())
                        throw std::runtime_error(
                            "Mesh data contains an out-of-bounds vertex reference!");
                    if (idx_cur == idx_next) continue;

                    tmp[edge_id] = std::make_pair(idx_next, INVALID);
                    if (!atomicCompareAndExchange(&V2E[idx_cur], edge_id, INVALID)) {
                        uint32_t idx = V2E[idx_cur];
                        while (!atomicCompareAndExchange((int*)&tmp[idx].second, edge_id, INVALID))
                            idx = tmp[idx].second;
                    }
                }
            }
        });
#else
    for (int f = 0; f < F.cols(); ++f) {
        for (unsigned int i = 0; i < deg; ++i) {
            unsigned int idx_cur = F(i, f), idx_next = F((i + 1) % deg, f), edge_id = deg * f + i;
            if (idx_cur >= V.cols() || idx_next >= V.cols())
                throw std::runtime_error("Mesh data contains an out-of-bounds vertex reference!");
            if (idx_cur == idx_next) continue;

            tmp[edge_id] = std::make_pair(idx_next, -1);
            if (V2E[idx_cur] == -1)
                V2E[idx_cur] = edge_id;
            else {
                unsigned int idx = V2E[idx_cur];
                while (tmp[idx].second != -1) {
                    idx = tmp[idx].second;
                }
                tmp[idx].second = edge_id;
            }
        }
    }
#endif

    nonManifold.resize(V.cols());
    nonManifold.setConstant(false);

    E2E.resize(F.cols() * deg);
    E2E.setConstant(INVALID);

#ifdef WITH_OMP
#pragma omp parallel for
#endif
    for (int f = 0; f < F.cols(); ++f) {
        for (uint32_t i = 0; i < deg; ++i) {
            uint32_t idx_cur = F(i, f), idx_next = F((i + 1) % deg, f), edge_id_cur = deg * f + i;

            if (idx_cur == idx_next) continue;

            uint32_t it = V2E[idx_next], edge_id_opp = INVALID;
            while (it != INVALID) {
                if (tmp[it].first == idx_cur) {
                    if (edge_id_opp == INVALID) {
                        edge_id_opp = it;
                    } else {
                        nonManifold[idx_cur] = true;
                        nonManifold[idx_next] = true;
                        edge_id_opp = INVALID;
                        break;
                    }
                }
                it = tmp[it].second;
            }

            if (edge_id_opp != INVALID && edge_id_cur < edge_id_opp) {
                E2E[edge_id_cur] = edge_id_opp;
                E2E[edge_id_opp] = edge_id_cur;
            }
        }
    }
    std::atomic<uint32_t> nonManifoldCounter(0), boundaryCounter(0), isolatedCounter(0);

    boundary.resize(V.cols());
    boundary.setConstant(false);

    /* Detect boundary regions of the mesh and adjust vertex->edge pointers*/
#ifdef WITH_OMP
#pragma omp parallel for
#endif
    for (int i = 0; i < V.cols(); ++i) {
        uint32_t edge = V2E[i];
        if (edge == INVALID) {
            isolatedCounter++;
            continue;
        }
        if (nonManifold[i]) {
            nonManifoldCounter++;
            V2E[i] = INVALID;
            continue;
        }

        /* Walk backwards to the first boundary edge (if any) */
        uint32_t start = edge, v2e = INVALID;
        do {
            v2e = std::min(v2e, edge);
            uint32_t prevEdge = E2E[dedge_prev(edge, deg)];
            if (prevEdge == INVALID) {
                /* Reached boundary -- update the vertex->edge link */
                v2e = edge;
                boundary[i] = true;
                boundaryCounter++;
                break;
            }
            edge = prevEdge;
        } while (edge != start);
        V2E[i] = v2e;
    }
#ifdef LOG_OUTPUT
    printf("counter triangle %d %d\n", (int)boundaryCounter, (int)nonManifoldCounter);
#endif
    return true;
    std::vector<std::vector<int>> vert_to_edges(V2E.size());
    for (int i = 0; i < F.cols(); ++i) {
        for (int j = 0; j < 3; ++j) {
            int v = F(j, i);
            vert_to_edges[v].push_back(i * 3 + j);
        }
    }
    std::vector<int> colors(F.cols() * 3, -1);
    bool update = false;
    int num_v = V.cols();
    std::map<int, int> new_vertices;
    for (int i = 0; i < vert_to_edges.size(); ++i) {
        int num_color = 0;
        for (int j = 0; j < vert_to_edges[i].size(); ++j) {
            int deid0 = vert_to_edges[i][j];
            if (colors[deid0] == -1) {
                int deid = deid0;
                do {
                    colors[deid] = num_color;
                    if (num_color != 0) F(deid % 3, deid / 3) = num_v;
                    deid = deid / 3 * 3 + (deid + 2) % 3;
                    deid = E2E[deid];
                } while (deid != deid0);
                num_color += 1;
                if (num_color > 1) {
                    update = true;
                    new_vertices[num_v] = i;
                    num_v += 1;
                }
            }
        }
    }
    if (update) {
        V.conservativeResize(3, num_v);
        for (auto& p : new_vertices) {
            V.col(p.first) = V.col(p.second);
        }
        return false;
    }
    return true;
}

void compute_direct_graph_quad(std::vector<Vector3d>& V, std::vector<Vector4i>& F, std::vector<int>& V2E, std::vector<int>& E2E, VectorXi& boundary, VectorXi& nonManifold) {
    V2E.clear();
    E2E.clear();
    boundary = VectorXi();
    nonManifold = VectorXi();
    V2E.resize(V.size(), INVALID);

    uint32_t deg = 4;
    std::vector<std::pair<uint32_t, uint32_t>> tmp(F.size() * deg);

#ifdef WITH_TBB
    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t)F.size(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t>& range) {
            for (uint32_t f = range.begin(); f != range.end(); ++f) {
                for (uint32_t i = 0; i < deg; ++i) {
                    uint32_t idx_cur = F[f][i], idx_next = F[f][(i + 1) % deg],
                             edge_id = deg * f + i;
                    if (idx_cur >= V.size() || idx_next >= V.size())
                        throw std::runtime_error(
                            "Mesh data contains an out-of-bounds vertex reference!");
                    if (idx_cur == idx_next) continue;

                    tmp[edge_id] = std::make_pair(idx_next, INVALID);
                    if (!atomicCompareAndExchange(&V2E[idx_cur], edge_id, INVALID)) {
                        uint32_t idx = V2E[idx_cur];
                        while (!atomicCompareAndExchange((int*)&tmp[idx].second, edge_id, INVALID))
                            idx = tmp[idx].second;
                    }
                }
            }
        });
#else
    for (int f = 0; f < F.size(); ++f) {
        for (unsigned int i = 0; i < deg; ++i) {
            unsigned int idx_cur = F[f][i], idx_next = F[f][(i + 1) % deg], edge_id = deg * f + i;
            if (idx_cur >= V.size() || idx_next >= V.size())
                throw std::runtime_error("Mesh data contains an out-of-bounds vertex reference!");
            if (idx_cur == idx_next) continue;
            tmp[edge_id] = std::make_pair(idx_next, -1);
            if (V2E[idx_cur] == -1) {
                V2E[idx_cur] = edge_id;
            }
            else {
                unsigned int idx = V2E[idx_cur];
                while (tmp[idx].second != -1) {
                    idx = tmp[idx].second;
                }
                tmp[idx].second = edge_id;
            }
        }
    }
#endif
    nonManifold.resize(V.size());
    nonManifold.setConstant(false);

    E2E.resize(F.size() * deg, INVALID);

#ifdef WITH_OMP
#pragma omp parallel for
#endif
    for (int f = 0; f < F.size(); ++f) {
        for (uint32_t i = 0; i < deg; ++i) {
            uint32_t idx_cur = F[f][i], idx_next = F[f][(i + 1) % deg], edge_id_cur = deg * f + i;

            if (idx_cur == idx_next) continue;

            uint32_t it = V2E[idx_next], edge_id_opp = INVALID;
            while (it != INVALID) {
                if (tmp[it].first == idx_cur) {
                    if (edge_id_opp == INVALID) {
                        edge_id_opp = it;
                    } else {
                        nonManifold[idx_cur] = true;
                        nonManifold[idx_next] = true;
                        edge_id_opp = INVALID;
                        break;
                    }
                }
                it = tmp[it].second;
            }

            if (edge_id_opp != INVALID && edge_id_cur < edge_id_opp) {
                E2E[edge_id_cur] = edge_id_opp;
                E2E[edge_id_opp] = edge_id_cur;
            }
        }
    }
    std::atomic<uint32_t> nonManifoldCounter(0), boundaryCounter(0), isolatedCounter(0);

    boundary.resize(V.size());
    boundary.setConstant(false);

    /* Detect boundary regions of the mesh and adjust vertex->edge pointers*/
#ifdef WITH_OMP
#pragma omp parallel for
#endif
    for (int i = 0; i < V.size(); ++i) {
        uint32_t edge = V2E[i];
        if (edge == INVALID) {
            isolatedCounter++;
            continue;
        }
        if (nonManifold[i]) {
            nonManifoldCounter++;
            V2E[i] = INVALID;
            continue;
        }

        /* Walk backwards to the first boundary edge (if any) */
        uint32_t start = edge, v2e = INVALID;
        do {
            v2e = std::min(v2e, edge);
            uint32_t prevEdge = E2E[dedge_prev(edge, deg)];
            if (prevEdge == INVALID) {
                /* Reached boundary -- update the vertex->edge link */
                v2e = edge;
                boundary[i] = true;
                boundaryCounter++;
                break;
            }
            edge = prevEdge;
        } while (edge != start);
        V2E[i] = v2e;
    }
#ifdef LOG_OUTPUT
    printf("counter %d %d\n", (int)boundaryCounter, (int)nonManifoldCounter);
#endif
}

void remove_nonmanifold(std::vector<Vector4i>& F, std::vector<Vector3d>& V) {
    typedef std::pair<uint32_t, uint32_t> Edge;

    int degree = 4;
    std::map<uint32_t, std::map<uint32_t, std::pair<uint32_t, uint32_t>>> irregular;
    std::vector<std::set<int>> E(V.size());
    std::vector<std::set<int>> VF(V.size());

    auto kill_face_single = [&](uint32_t f) {
        if (F[f][0] == INVALID) return;
        for (int i = 0; i < degree; ++i) E[F[f][i]].erase(F[f][(i + 1) % degree]);
        F[f].setConstant(INVALID);
    };

    auto kill_face = [&](uint32_t f) {
        if (degree == 4 && F[f][2] == F[f][3]) {
            auto it = irregular.find(F[f][2]);
            if (it != irregular.end()) {
                for (auto& item : it->second) {
                    kill_face_single(item.second.second);
                }
            }
        }
        kill_face_single(f);
    };

    uint32_t nm_edge = 0, nm_vert = 0;

    for (uint32_t f = 0; f < (uint32_t)F.size(); ++f) {
        if (F[f][0] == INVALID) continue;
        if (degree == 4 && F[f][2] == F[f][3]) {
            /* Special handling of irregular faces */
            irregular[F[f][2]][F[f][0]] = std::make_pair(F[f][1], f);
            continue;
        }

        bool nonmanifold = false;
        for (uint32_t e = 0; e < degree; ++e) {
            uint32_t v0 = F[f][e], v1 = F[f][(e + 1) % degree], v2 = F[f][(e + 2) % degree];
            if (E[v0].find(v1) != E[v0].end() || (degree == 4 && E[v0].find(v2) != E[v0].end()))
                nonmanifold = true;
        }

        if (nonmanifold) {
            nm_edge++;
            F[f].setConstant(INVALID);
            continue;
        }

        for (uint32_t e = 0; e < degree; ++e) {
            uint32_t v0 = F[f][e], v1 = F[f][(e + 1) % degree], v2 = F[f][(e + 2) % degree];

            E[v0].insert(v1);
            if (degree == 4) E[v0].insert(v2);
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
                if (next == pred) break;
                if (E[cur].find(next) != E[cur].end() && it == 1) nonmanifold = true;
                edges.push_back(Edge(cur, next));
                next = face[next].first;
            }
            if (cur == stop) break;
        }

        if (nonmanifold) {
            nm_edge++;
            for (auto& i : item.second) F[i.second.second].setConstant(INVALID);
            continue;
        } else {
            for (auto e : edges) {
                E[e.first].insert(e.second);

                for (auto e2 : face) VF[e.first].insert(e2.second.second);
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
            for (uint32_t j = 0; j < degree; ++j) {
                uint32_t k = F[f][j];
                if (v_unmarked.find(k) == v_unmarked.end() || /* if not unmarked OR */
                    v_marked.find(k) != v_marked.end())       /* if already marked */
                    continue;
                dfs(k);
            }
        }
    };

    for (uint32_t i = 0; i < (uint32_t)V.size(); ++i) {
        v_marked.clear();
        v_unmarked.clear();
        f_adjacent.clear();

        for (uint32_t f : VF[i]) {
            if (F[f][0] == INVALID) continue;

            for (uint32_t k = 0; k < degree; ++k) v_unmarked.insert(F[f][k]);

            f_adjacent.insert(f);
        }

        if (v_unmarked.empty()) continue;
        v_marked.insert(i);
        v_unmarked.erase(i);

        dfs(*v_unmarked.begin());

        if (v_unmarked.size() > 0) {
            nm_vert++;
            for (uint32_t f : f_adjacent) kill_face(f);
        }
    }

    if (nm_vert > 0 || nm_edge > 0) {
        std::cout << "Non-manifold elements:  vertices=" << nm_vert << ", edges=" << nm_edge
                  << std::endl;
    }
    uint32_t nFaces = 0, nFacesOrig = F.size();
    for (uint32_t f = 0; f < (uint32_t)F.size(); ++f) {
        if (F[f][0] == INVALID) continue;
        if (nFaces != f) {
            F[nFaces] = F[f];
        }
        ++nFaces;
    }

    if (nFacesOrig != nFaces) {
        F.resize(nFaces);
        std::cout << "Faces reduced from " << nFacesOrig << " -> " << nFaces << std::endl;
    }
}

} // namespace qflow
