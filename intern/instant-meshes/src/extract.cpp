/*
    extract.h: Mesh extraction from existing orientation/position fields

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include "extract.h"
#include "field.h"
#include "dset.h"
#include "dedge.h"
#include "reorder.h"
#include "bvh.h"
#include "cleanup.h"
#include <tbb/concurrent_vector.h>
#include <parallel_stable_sort.h>
#include <unordered_set>
#include <tuple>
#include <set>

typedef std::pair<uint32_t, uint32_t> Edge;

void
extract_graph(const MultiResolutionHierarchy &mRes, bool extrinsic, int rosy, int posy,
              std::vector<std::vector<TaggedLink> > &adj_new,
              MatrixXf &O_new, MatrixXf &N_new,
              const std::set<uint32_t> &crease_in,
              std::set<uint32_t> &crease_out,
              bool deterministic, bool remove_spurious_vertices,
              bool remove_unnecessary_edges,
              bool snap_vertices) {

    Float scale = mRes.scale(), inv_scale = 1 / scale;

    auto compat_orientation = rosy == 2 ? compat_orientation_extrinsic_2 :
        (rosy == 4 ? compat_orientation_extrinsic_4 : compat_orientation_extrinsic_6);
    auto compat_position = posy == 4 ? compat_position_extrinsic_index_4 : compat_position_extrinsic_index_3;

    const MatrixXf &Q = mRes.Q(), &O = mRes.O(), &N = mRes.N(), &V = mRes.V();
    const VectorXf &COw = mRes.COw();
    const AdjacencyMatrix &adj = mRes.adj();

    Timer<> timer;

    {
        DisjointSets dset(mRes.size());
        adj_new.clear();
        adj_new.resize(mRes.size());
        typedef std::pair<Edge, float> WeightedEdge;

        tbb::concurrent_vector<WeightedEdge> collapse_edge_vec;
        collapse_edge_vec.reserve((uint32_t) (mRes.size()*2.5f));

        auto classify_edges = [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t i = range.begin(); i<range.end(); ++i) {
                while (!dset.try_lock(i))
                    ;

                for (Link *link = adj[i]; link != adj[i+1]; ++link) {
                    uint32_t j = link->id;

                    if (j < i)
                        continue;

                    std::pair<Vector3f, Vector3f> Q_rot = compat_orientation(
                            Q.col(i), N.col(i), Q.col(j), N.col(j));

                    Float error = 0;
                    std::pair<Vector2i, Vector2i> shift = compat_position(
                            V.col(i), N.col(i), Q_rot.first, O.col(i),
                            V.col(j), N.col(j), Q_rot.second, O.col(j),
                            scale, inv_scale, &error);

                    Vector2i absDiff = (shift.first-shift.second).cwiseAbs();

                    if (absDiff.maxCoeff() > 1 || (absDiff == Vector2i(1, 1) && posy == 4))
                        continue; /* Ignore longer-distance links and diagonal lines for quads */
                    bool collapse = absDiff.sum() == 0;

                    if (collapse) {
                        collapse_edge_vec.push_back(std::make_pair(std::make_pair(i, j), error));
                    } else {
                        while (!dset.try_lock(j))
                            ;
                        adj_new[i].push_back(j);
                        adj_new[j].push_back(i);
                        dset.unlock(j);
                    }
                }

                dset.unlock(i);
            }
        };

        std::atomic<uint32_t> nConflicts(0), nItem(0);
        std::vector<uint16_t> nCollapses(mRes.size(), 0);
        auto collapse_edges = [&](const tbb::blocked_range<uint32_t> &range) {
            std::set<uint32_t> temp;

            for (uint32_t i = range.begin(); i != range.end(); ++i) {
                const WeightedEdge &we = collapse_edge_vec[nItem++];
                Edge edge = we.first;

                /* Lock both sets and determine the current representative ID */
                bool ignore_edge = false;
                do {
                    if (edge.first > edge.second)
                        std::swap(edge.first, edge.second);
                    if (!dset.try_lock(edge.first))
                        continue;
                    if (!dset.try_lock(edge.second)) {
                        dset.unlock(edge.first);
                        if (edge.second == edge.first) {
                            ignore_edge = true;
                            break;
                        }
                        continue;
                    }
                    break;
                } while(true);

                if (ignore_edge)
                    continue;

                bool contained = false;
                for (auto neighbor : adj_new[edge.first]) {
                    if (dset.find(neighbor.id) == edge.second) {
                        contained = true;
                        break;
                    }
                }

                if (contained) {
                    dset.unlock(edge.first);
                    dset.unlock(edge.second);
                    nConflicts++;
                    continue;
                }

                temp.clear();
                for (auto neighbor : adj_new[edge.first])
                    temp.insert(dset.find(neighbor.id));
                for (auto neighbor : adj_new[edge.second])
                    temp.insert(dset.find(neighbor.id));

                uint32_t target_idx = dset.unite_index_locked(edge.first, edge.second);
                adj_new[edge.first].clear();
                adj_new[edge.second].clear();
                adj_new[target_idx].reserve(temp.size());
                for (auto j : temp)
                    adj_new[target_idx].push_back(j);
                nCollapses[target_idx] = nCollapses[edge.first] + nCollapses[edge.second] + 1;
                adj_new[edge.first].shrink_to_fit();
                adj_new[edge.second].shrink_to_fit();

                dset.unite_unlock(edge.first, edge.second);
            }
        };

        size_t nEdges = adj[mRes.size()]-adj[0];
        cout << "Step 1: Classifying " << nEdges << " edges " << (deterministic ? "in parallel " : "") << ".. ";
        cout.flush();
        tbb::blocked_range<uint32_t> range1(0u, (uint32_t) V.cols(), GRAIN_SIZE);
        if (!deterministic)
            tbb::parallel_for(range1, classify_edges);
        else
            classify_edges(range1);
        cout << "done. (took " << timeString(timer.reset()) << ")" << endl;

        cout << "Step 2: Collapsing " << collapse_edge_vec.size() << " edges .. ";
        cout.flush();

        struct WeightedEdgeComparator {
            bool operator()(const WeightedEdge& e1, const WeightedEdge& e2) const { return e1.second < e2.second; }
        };

        if (deterministic)
            pss::parallel_stable_sort(collapse_edge_vec.begin(), collapse_edge_vec.end(), WeightedEdgeComparator());
        else
            tbb::parallel_sort(collapse_edge_vec.begin(), collapse_edge_vec.end(), WeightedEdgeComparator());

        tbb::blocked_range<uint32_t> range2(0u, (uint32_t) collapse_edge_vec.size(), GRAIN_SIZE);
        if (!deterministic)
            tbb::parallel_for(range2, collapse_edges);
        else
            collapse_edges(range2);
        cout << "done. (ignored " << nConflicts << " conflicting edges, took " << timeString(timer.reset()) << ")" << endl;

        cout << "Step 3: Assigning vertices .. ";
        cout.flush();

        uint32_t nVertices = 0;
        std::map<uint32_t, uint32_t> vertex_map;
        Float avg_collapses = 0;
        for (uint32_t i=0; i<adj_new.size(); ++i) {
            if (adj_new[i].empty())
                continue;
            if (i != nVertices) {
                adj_new[nVertices].swap(adj_new[i]);
                std::swap(nCollapses[nVertices], nCollapses[i]);
            }
            avg_collapses += nCollapses[nVertices];
            vertex_map[i] = nVertices++;
        }
        adj_new.resize(nVertices);
        adj_new.shrink_to_fit();
        avg_collapses /= nVertices;

        tbb::parallel_for(
            tbb::blocked_range<uint32_t>(0u, nVertices, GRAIN_SIZE),
            [&](const tbb::blocked_range<uint32_t> &range) {
                std::set<uint32_t> temp;
                for (uint32_t i = range.begin(); i != range.end(); ++i) {
                    temp.clear();
                    for (auto k : adj_new[i])
                        temp.insert(vertex_map[dset.find(k.id)]);
                    std::vector<TaggedLink> new_vec;
                    new_vec.reserve(temp.size());
                    for (auto j : temp)
                        new_vec.push_back(TaggedLink(j));
                    adj_new[i] = std::move(new_vec);
                }
            }
        );

        cout << "done. (" << vertex_map.size() << " vertices, took " << timeString(timer.reset()) << ")" << endl;

        if (remove_spurious_vertices) {
            cout << "Step 3a: Removing spurious vertices .. ";
            cout.flush();
            uint32_t removed = 0;
            for (uint32_t i=0; i<adj_new.size(); ++i) {
                if (nCollapses[i] > avg_collapses/10)
                    continue;

                for (auto neighbor : adj_new[i]) {
                    auto &a = adj_new[neighbor.id];
                    a.erase(std::remove_if(a.begin(), a.end(), [&](const TaggedLink &v) { return v.id == i; }), a.end());
                }

                adj_new[i].clear();
                ++removed;
            }
            cout << "done. (removed " << removed << " vertices, took " << timeString(timer.reset()) << ")" << endl;
        }

        cout << "Step 4: Assigning positions to vertices .. ";
        cout.flush();

        O_new.resize(3, nVertices);
        N_new.resize(3, nVertices);
        O_new.setZero();
        N_new.setZero();

        crease_out.clear();
        for (auto i : crease_in) {
            auto it = vertex_map.find(dset.find(i));
            if (it == vertex_map.end())
                continue;
            crease_out.insert(it->second);
        }

        {
            Eigen::VectorXf cluster_weight(nVertices);
            cluster_weight.setZero();

            tbb::blocked_range<uint32_t> range(0u, V.cols(), GRAIN_SIZE);
            tbb::spin_mutex mutex;

            auto map = [&](const tbb::blocked_range<uint32_t> &range) {
                for (uint32_t i = range.begin(); i < range.end(); ++i) {
                    auto it = vertex_map.find(dset.find(i));
                    if (it == vertex_map.end())
                        continue;
                    uint32_t j = it->second;

                    Float weight = std::exp(-(O.col(i)-V.col(i)).squaredNorm() * inv_scale * inv_scale * 9);
                    if (COw.size() != 0 && COw[i] != 0) {
                        tbb::spin_mutex::scoped_lock lock(mutex);
                        crease_out.insert(j);
                    }

                    for (uint32_t k=0; k<3; ++k) {
                        atomicAdd(&O_new.coeffRef(k, j), O(k, i)*weight);
                        atomicAdd(&N_new.coeffRef(k, j), N(k, i)*weight);
                    }
                    atomicAdd(&cluster_weight[j], weight);
                }
            };

            if (!deterministic)
                tbb::parallel_for(range, map);
            else
                map(range);

            for (uint32_t i=0; i<nVertices; ++i) {
                if (cluster_weight[i] == 0) {
                    cout << "Warning: vertex " << i << " did not receive any contributions!" << endl;
                    continue;
                }
                O_new.col(i) /= cluster_weight[i];
                N_new.col(i).normalize();
            }

            cout << "done. (took " << timeString(timer.reset()) << ")" << endl;
        }
    }

    if (remove_unnecessary_edges) {
        cout << "Step 5: Snapping and removing unnecessary edges .";
        cout.flush();
        bool changed;
        uint32_t nRemoved = 0, nSnapped = 0;
        do {
            changed = false;
            cout << ".";
            cout.flush();

            bool changed_inner;
            do {
                changed_inner = false;
                Float thresh = 0.3f * scale;

                std::vector<std::tuple<Float, uint32_t, uint32_t, uint32_t>> candidates;
                for (uint32_t i_id=0; i_id<adj_new.size(); ++i_id) {
                    auto const &adj_i = adj_new[i_id];
                    const Vector3f p_i = O_new.col(i_id);
                    for (uint32_t j=0; j<adj_i.size(); ++j) {
                        uint32_t j_id = adj_i[j].id;
                        const Vector3f p_j = O_new.col(j_id);
                        auto const &adj_j = adj_new[j_id];

                        for (uint32_t k=0; k<adj_j.size(); ++k) {
                            uint32_t k_id = adj_j[k].id;
                            if (k_id == i_id)
                                continue;
                            const Vector3f p_k = O_new.col(k_id);
                            Float a = (p_j-p_k).norm(), b = (p_i-p_j).norm(), c = (p_i-p_k).norm();
                            if (a > std::max(b, c)) {
                                Float s = 0.5f * (a+b+c);
                                Float height = 2*std::sqrt(s*(s-a)*(s-b)*(s-c))/a;
                                if (height < thresh)
                                    candidates.push_back(std::make_tuple(height, i_id, j_id, k_id));
                            }
                        }
                    }
                }

                std::sort(candidates.begin(), candidates.end(), [&](
                    const decltype(candidates)::value_type &v0,
                    const decltype(candidates)::value_type &v1)
                        { return std::get<0>(v0) < std::get<0>(v1); });

                for (auto t : candidates) {
                    uint32_t i = std::get<1>(t), j= std::get<2>(t), k= std::get<3>(t);
                    bool edge1 = std::find_if(adj_new[i].begin(), adj_new[i].end(),
                        [j](const TaggedLink &l) { return l.id == j;} ) != adj_new[i].end();
                    bool edge2 = std::find_if(adj_new[j].begin(), adj_new[j].end(),
                        [k](const TaggedLink &l) { return l.id == k;} ) != adj_new[j].end();
                    bool edge3 = std::find_if(adj_new[k].begin(), adj_new[k].end(),
                        [i](const TaggedLink &l) { return l.id == i;} ) != adj_new[k].end();

                    if (!edge1 || !edge2)
                        continue;

                    const Vector3f p_i = O_new.col(i), p_j = O_new.col(j), p_k = O_new.col(k);
                    Float a = (p_j-p_k).norm(), b = (p_i-p_j).norm(), c = (p_i-p_k).norm();
                    Float s = 0.5f * (a+b+c);
                    Float height = 2*std::sqrt(s*(s-a)*(s-b)*(s-c))/a;
                    if (height != std::get<0>(t))
                        continue;
                    if ((p_i-p_j).norm() < thresh || (p_i-p_k).norm() < thresh) {
                        uint32_t merge_id = (p_i-p_j).norm() < thresh ? j : k;
                        O_new.col(i) = (O_new.col(i) + O_new.col(merge_id)) * 0.5f;
                        N_new.col(i) = (N_new.col(i) + N_new.col(merge_id)) * 0.5f;
                        std::set<uint32_t> adj_updated;
                        for (auto const &n : adj_new[merge_id]) {
                            if (n.id == i)
                                continue;
                            adj_updated.insert(n.id);
                            for (auto &n2 : adj_new[n.id]) {
                                if (n2.id == merge_id)
                                    n2.id = i;
                            }
                        }
                        for (auto &n : adj_new[i])
                            adj_updated.insert(n.id);
                        adj_updated.erase(i);
                        adj_updated.erase(merge_id);
                        adj_new[merge_id].clear();
                        adj_new[i].clear();
                        if (crease_out.find(merge_id) != crease_out.end()) {
                            crease_out.erase(merge_id);
                            crease_out.insert(i);
                        }
                        for (uint32_t l : adj_updated)
                            adj_new[i].push_back(l);
                    } else {
                        Vector3f n_k = N_new.col(k), n_j = N_new.col(j);
                        //Vector3f dp = p_k - p_j, dn = n_k - n_j;
                        //Float t = dp.dot(p_i-p_j) / dp.dot(dp);
                        //O_new.col(i) = p_j + t*dp;
                        //N_new.col(i) = (n_j + t*dn).normalized();
                        O_new.col(i) = (p_j + p_k) * 0.5f;
                        N_new.col(i) = (n_j + n_k).normalized();

                        if (crease_out.find(j) != crease_out.end() &&
                            crease_out.find(k) != crease_out.end())
                            crease_out.insert(i);

                        adj_new[j].erase(std::remove_if(adj_new[j].begin(), adj_new[j].end(),
                            [k](const TaggedLink &l) { return l.id == k; }), adj_new[j].end());
                        adj_new[k].erase(std::remove_if(adj_new[k].begin(), adj_new[k].end(),
                            [j](const TaggedLink &l) { return l.id == j; }), adj_new[k].end());

                        if (!edge3) {
                            adj_new[i].push_back(k);
                            adj_new[k].push_back(i);
                        }
                    }

                    changed = true;
                    changed_inner = true;
                    ++nSnapped;
                }
            } while (changed_inner);

            if (posy == 4) {
                std::vector<std::pair<Float, Edge>> candidates;
                for (size_t i=0; i<adj_new.size(); ++i) {
                    auto const &adj_i = adj_new[i];
                    const Vector3f p_i = O_new.col(i);
                    for (uint32_t j=0; j<adj_i.size(); ++j) {
                        uint32_t j_id = adj_i[j].id;
                        const Vector3f p_j = O_new.col(j_id);

                        uint32_t nTris = 0;
                        Float length = 0.0f;
                        for (uint32_t k=0; k<adj_i.size(); ++k) {
                            uint32_t k_id = adj_i[k].id;
                            if (k_id == j_id)
                                continue;
                            const Vector3f p_k = O_new.col(k_id);
                            if (std::find_if(adj_new[j_id].begin(), adj_new[j_id].end(),
                                [k_id](const TaggedLink &l) { return l.id == k_id;} ) == adj_new[j_id].end())
                                continue;
                            nTris++;
                            length += (p_k - p_i).norm() + (p_k - p_j).norm();
                        }

                        if (nTris == 2) {
                            Float exp_diag = length / 4 * std::sqrt(2.f);
                            Float diag = (p_i - p_j).norm();
                            Float score = std::abs((diag - exp_diag) / std::min(diag, exp_diag));
                            candidates.push_back(std::make_pair(std::abs(score), std::make_pair(i, j_id)));
                        }
                    }
                }
                std::sort(candidates.begin(), candidates.end(), [&](
                    const std::pair<Float, Edge> &v0,
                    const std::pair<Float, Edge> &v1) { return v0.first < v1.first; });

                for (auto c : candidates) {
                    uint32_t i_id = c.second.first, j_id = c.second.second;
                    auto const &adj_i = adj_new[i_id];
                    uint32_t nTris = 0;
                    for (uint32_t k=0; k<adj_i.size(); ++k) {
                        uint32_t k_id = adj_i[k].id;
                        if (std::find_if(adj_new[j_id].begin(), adj_new[j_id].end(),
                            [k_id](const TaggedLink &l) { return l.id == k_id;} ) == adj_new[j_id].end())
                            continue;
                        nTris++;
                    }
                    if (nTris == 2) {
                        adj_new[i_id].erase(std::remove_if(adj_new[i_id].begin(), adj_new[i_id].end(),
                            [j_id](const TaggedLink &l) { return l.id == j_id; }), adj_new[i_id].end());
                        adj_new[j_id].erase(std::remove_if(adj_new[j_id].begin(), adj_new[j_id].end(),
                            [i_id](const TaggedLink &l) { return l.id == i_id; }), adj_new[j_id].end());
                        changed = true;
                        ++nRemoved;
                    }
                }
            }
        } while (changed);
        cout << " done. (snapped " << nSnapped << " vertices, removed " << nRemoved << " edges, took " << timeString(timer.reset()) << ")" << endl;
    }

    cout << "Step 6: Orienting edges .. ";
    cout.flush();

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) O_new.cols(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t i=range.begin(); i != range.end(); ++i) {
                Vector3f s, t, p = O_new.col(i);
                coordinate_system(N_new.col(i), s, t);

                std::sort(adj_new[i].begin(), adj_new[i].end(),
                    [&](const TaggedLink &j0, const TaggedLink &j1) {
                        Vector3f v0 = O_new.col(j0.id)-p, v1 = O_new.col(j1.id)-p;
                        return std::atan2(t.dot(v0), s.dot(v0)) > std::atan2(t.dot(v1), s.dot(v1));
                    }
                );
            }
        }
    );


    cout << "done. (took " << timeString(timer.reset()) << ")" << endl;
}

void extract_faces(std::vector<std::vector<TaggedLink> > &adj, MatrixXf &O,
                   MatrixXf &N, MatrixXf &Nf, MatrixXu &F, int posy,
                   Float scale, std::set<uint32_t> &crease, bool fill_holes,
                   bool pure_quad, BVH *bvh, int smooth_iterations) {

    uint32_t nF = 0, nV = O.cols(), nV_old = O.cols();
    F.resize(posy, posy == 4 ? O.cols() : O.cols()*2);

    auto extract_face = [&](uint32_t cur, uint32_t curIdx, size_t targetSize,
            std::vector<std::pair<uint32_t, uint32_t>> &result) {
        uint32_t initial = cur;
        bool success = false;
        result.clear();
        for (;;) {
            if (adj[cur][curIdx].used() ||
                (targetSize > 0 && result.size() + 1 > targetSize))
                break;

            result.push_back(std::make_pair(cur, curIdx));

            uint32_t next = adj[cur][curIdx].id,
                     next_rank = adj[next].size(),
                     idx = (uint32_t)-1;

            for (uint32_t j=0; j<next_rank; ++j) {
                if (adj[next][j].id == cur) {
                    idx = j; break;
                }
            }

            if (idx == (uint32_t) -1 || next_rank == 1)
                break;

            cur = next;
            curIdx = (idx+1)%next_rank;
            if (cur == initial) {
                success = targetSize == 0 || result.size() == targetSize;
                break;
            }
        }

        if (success) {
            for (auto kv : result)
                adj[kv.first][kv.second].markUsed();
        } else {
            result.clear();
        }
        return success;
    };

    std::vector<std::vector<uint32_t>> irregular_faces;
    auto fill_face = [&](std::vector<std::pair<uint32_t, uint32_t>> &verts) -> std::vector<uint32_t> {
        std::vector<uint32_t> irregular;
        while (verts.size() > 2) {
            if (verts.size() == (size_t) posy) {
                uint32_t idx = nF++;
                if (nF > F.cols())
                    F.conservativeResize(F.rows(), F.cols() * 2);
                for (int i=0; i < posy; ++i)
                    F(i, idx) = verts[i].first;
                break;
            } else if (verts.size() > (size_t) posy + 1 || posy == 3) {
                Float best_score = std::numeric_limits<Float>::infinity();
                uint32_t best_idx = (uint32_t) -1;

                for (uint32_t i=0; i<verts.size(); ++i) {
                    Float score = 0.f;
                    for (int k = 0; k < posy; ++k) {
                        Vector3f v0 = O.col(verts[(i+k)%verts.size()].first);
                        Vector3f v1 = O.col(verts[(i+k+1)%verts.size()].first);
                        Vector3f v2 = O.col(verts[(i+k+2)%verts.size()].first);
                        Vector3f d0 = (v0-v1).normalized();
                        Vector3f d1 = (v2-v1).normalized();
                        Float angle = std::acos(d0.dot(d1)) * 180.0f/M_PI;
                        score += std::abs(angle - (posy == 4 ? 90 : 60));
                    }

                    if (score < best_score) {
                        best_score = score;
                        best_idx = i;
                    }
                }
                uint32_t idx = nF++;
                if (nF > F.cols())
                    F.conservativeResize(F.rows(), F.cols() * 2);

                for (int i=0; i < posy; ++i) {
                    uint32_t &j = verts[(best_idx + i) % verts.size()].first;
                    F(i, idx) = j;
                    if (i != 0 && (int) i != posy-1)
                        j = (uint32_t) -1;
                }
                verts.erase(std::remove_if(verts.begin(), verts.end(),
                    [](const std::pair<uint32_t, uint32_t> &v) { return v.first == (uint32_t) -1; }), verts.end());
            } else {
                if (posy != 4)
                    throw std::runtime_error("Internal error in fill_hole");
                Vector3f centroid = Vector3f::Zero();
                Vector3f centroid_normal = Vector3f::Zero();
                for (uint32_t k=0; k<verts.size(); ++k) {
                    centroid += O.col(verts[k].first);
                    centroid_normal += N.col(verts[k].first);
                }
                uint32_t idx_centroid = nV++;
                if (nV > O.cols()) {
                    O.conservativeResize(O.rows(), O.cols() * 2);
                    N.conservativeResize(O.rows(), O.cols());
                }
                O.col(idx_centroid) = centroid / verts.size();
                N.col(idx_centroid) = centroid_normal.normalized();

                for (uint32_t i=0; i<verts.size(); ++i) {
                    uint32_t idx = nF++;
                    if (nF > F.cols())
                        F.conservativeResize(F.rows(), F.cols() * 2);

                    F.col(idx) = Vector4u(
                        verts[i].first,
                        verts[(i+1)%verts.size()].first,
                        idx_centroid,
                        idx_centroid
                    );
                    irregular.push_back(idx);
                }
                break;
            }
        }
        return irregular;
    };

    VectorXu stats(10);
    stats.setZero();
    Timer<> timer;


    cout << "Step 7: Extracting faces .. ";
    cout.flush();
    uint32_t nFaces = 0, nHoles = 0;
    std::vector<std::pair<uint32_t, uint32_t>> result;
    for (uint32_t _deg = 3; _deg <= 8; _deg++) {
        uint32_t deg = _deg;
        if (posy == 4 && (deg == 3 || deg == 4))
            deg = 7-deg;

        for (uint32_t i=0; i<nV_old; ++i) {
            for (uint32_t j=0; j<adj[i].size(); ++j) {
                if (!extract_face(i, j, _deg, result))
                    continue;
                stats[result.size()]++;
                std::vector<uint32_t> irregular = fill_face(result);
                if (!irregular.empty())
                    irregular_faces.push_back(std::move(irregular));
                nFaces++;
            }
        }
    }
    cout << "done. (" << nFaces << " faces, took " << timeString(timer.reset()) << ")" << endl;

    if (fill_holes) {
        cout << "Step 8: Filling holes .. ";
        cout.flush();
        for (uint32_t i=0; i<nV_old; ++i) {
            for (uint32_t j=0; j<adj[i].size(); ++j) {
                if (!adj[i][j].used()) {
                    uint32_t j_id = adj[i][j].id;
                    bool found = false;
                    for (uint32_t k=0; k<adj[j_id].size(); ++k) {
                        if (adj[j_id][k].id == i) {
                            found = true;
                            if (adj[j_id][k].used()) {
                                adj[i][j].flag |= 2;
                                adj[j_id][k].flag |= 2;
                            }
                            break;
                        }
                    }
                    if (!found)
                        cout << "Internal error" << endl;
                }
            }
        }

        uint32_t linksLeft = 0;
        for (uint32_t i=0; i<nV_old; ++i) {
            adj[i].erase(std::remove_if(adj[i].begin(), adj[i].end(),
                [](const TaggedLink &l) { return (l.flag & 2) == 0; }), adj[i].end());
            linksLeft += adj[i].size();
        }

        for (uint32_t i=0; i<nV_old; ++i) {
            for (uint32_t j=0; j<adj[i].size(); ++j) {
                if (!extract_face(i, j, 0, result))
                    continue;
                if (result.size() >= 7) {
                    cout << "Not trying to fill a hole of degree " << result.size() << endl;
                    continue;
                }
                if (result.size() >= (size_t) stats.size()) {
                    uint32_t oldSize = stats.size();
                    stats.conservativeResize(result.size() + 1);
                    stats.tail(stats.size() - oldSize).setZero();
                }
                stats[result.size()]++;
                std::vector<uint32_t> irregular = fill_face(result);
                if (!irregular.empty())
                    irregular_faces.push_back(std::move(irregular));
                nHoles++;
            }
        }
        cout << "done. (" << nHoles << " holes, took " << timeString(timer.reset()) << ")" << endl;
    }

    {
        bool first = true;
        cout << "Intermediate mesh statistics: ";
        for (int i=0; i<stats.size(); ++i) {
            if (stats[i] == 0)
                continue;
            if (!first)
                cout << ", ";
            cout << "degree "<< i << ": " << stats[i] << (stats[i] == 1 ? " face" : " faces");
            first = false;
        }
        cout << endl;
    }

    if (posy == 4 && pure_quad) {
        F.conservativeResize(posy, nF);
        N.conservativeResize(3, nV);
        O.conservativeResize(3, nV);
        VectorXu V2E, E2E, E2E_v;
        VectorXb nonManifold, boundary;
        build_dedge(F, O, V2E, E2E, boundary, nonManifold, ProgressCallback(), true);
        E2E_v.resize(E2E.size());
        E2E_v.setConstant((uint32_t) -1);
        uint32_t nF_old = nF;
        cout << "Step 9: Regular subdivision into pure quad mesh .. ";
        cout.flush();

        for (uint32_t i=0; i<nF_old; ++i) {
            Vector4u face = F.col(i);
            if (face[2] == face[3])
                continue;
            Vector3f fc = O.col(face[0]) + O.col(face[1]) + O.col(face[2]) + O.col(face[3]);
            Vector3f fc_n = N.col(face[0]) + N.col(face[1]) + N.col(face[2]) + N.col(face[3]);
            uint32_t idx_fc = nV++;
            bool all_crease = true;
            for (int k=0; k<4; ++k) {
                if (crease.find(face[k]) == crease.end()) {
                    all_crease = false;
                    break;
                }
            }
            if (all_crease)
                crease.insert(idx_fc);

            if (nV > O.cols()) {
                O.conservativeResize(O.rows(), O.cols() * 2);
                N.conservativeResize(O.rows(), O.cols());
            }
            O.col(idx_fc) = fc * 1.f/4.f;
            N.col(idx_fc) = fc_n.normalized();

            Vector4u idx_ecs = Vector4u::Constant(INVALID);
            for (uint32_t j=0; j<4; ++j) {
                uint32_t i0 = face[j], i1 = face[(j+1)%4];
                if (i0 == i1)
                    continue;
                uint32_t edge = i*4 + j;
                uint32_t edge_opp = E2E[edge];

                uint32_t idx_ec = INVALID;
                if (edge_opp != INVALID)
                    idx_ec = E2E_v[edge_opp];

                if (idx_ec == INVALID) {
                    idx_ec = nV++;
                    if (nV > O.cols()) {
                        O.conservativeResize(O.rows(), O.cols() * 2);
                        N.conservativeResize(O.rows(), O.cols());
                    }
                    O.col(idx_ec) = (O.col(i0) + O.col(i1)) * 0.5f;
                    N.col(idx_ec) = (N.col(i0) + N.col(i1)).normalized();
                    if (crease.find(i0) != crease.end() &&
                        crease.find(i1) != crease.end())
                        crease.insert(idx_ec);
                    E2E_v[edge] = idx_ec;
                }
                idx_ecs[j] = idx_ec;
            }

            bool first = true;
            for (uint32_t j=0; j<4; ++j) {
                uint32_t i0 = face[j], i1 = face[(j+1)%4];
                if (i0 == i1)
                    continue;
                uint32_t idx_ec0 = idx_ecs[j],
                         idx_ec1 = idx_ecs[(j+1)%4];
                if (idx_ec0 == INVALID || idx_ec1 == INVALID)
                    continue;
                uint32_t idx_f;
                if (first) {
                    idx_f = i;
                    first = false;
                } else {
                    idx_f = nF++;
                    if (nF > F.cols())
                        F.conservativeResize(F.rows(), F.cols() * 2);
                }
                F.col(idx_f) = Vector4u(idx_ec0, i1, idx_ec1, idx_fc);
            }
        }

        std::vector<uint32_t> ecs;
        for (auto f : irregular_faces) {
            int idx_fc = F.col(f[0])[3];
            ecs.clear();
            for (uint32_t i=0; i<f.size(); ++i) {
                uint32_t edge = 4*f[i], edge_opp = E2E[edge];
                uint32_t i0 = F.col(f[i])[0], i1 = F.col(f[i])[1];

                uint32_t idx_ec = INVALID;
                if (edge_opp != INVALID)
                    idx_ec = E2E_v[edge_opp];

                if (idx_ec == INVALID) {
                    idx_ec = nV++;
                    if (nV > O.cols()) {
                        O.conservativeResize(O.rows(), O.cols() * 2);
                        N.conservativeResize(O.rows(), O.cols());
                    }
                    O.col(idx_ec) = (O.col(i0) + O.col(i1)) * 0.5f;
                    N.col(idx_ec) = (N.col(i0) + N.col(i1)).normalized();
                    if (crease.find(i0) != crease.end() &&
                        crease.find(i1) != crease.end())
                        crease.insert(idx_ec);
                    E2E_v[edge] = idx_ec;
                }

                ecs.push_back(i0);
                ecs.push_back(idx_ec);
            }
            for (uint32_t i=0; i<f.size(); ++i)
                F.col(f[i]) << ecs[2*i+1], ecs[(2*i+2)%ecs.size()], ecs[(2*i+3)%ecs.size()], idx_fc;
        }
        cout << "done. (took " << timeString(timer.reset()) << ")" << endl;
    }

    F.conservativeResize(posy, nF);
    N.conservativeResize(3, nV);
    O.conservativeResize(3, nV);

    if (smooth_iterations > 0) {
        cout << "Step 10: Running " << smooth_iterations << " smoothing & reprojection steps ..";
        cout.flush();

        std::vector<std::set<uint32_t>> adj_new(nV);
        std::vector<tbb::spin_mutex> locks(nV);
        tbb::parallel_for(
            tbb::blocked_range<uint32_t>(0u, nF, GRAIN_SIZE),
            [&](const tbb::blocked_range<uint32_t> &range) {
                for (uint32_t f = range.begin(); f != range.end(); ++f) {
                    if (posy == 4 && F(2, f) == F(3, f)) {
                        /* Irregular face */
                        if (pure_quad) /* Should never get these when subdivision is on */
                            throw std::runtime_error("Internal error in extraction");
                        uint32_t i0 = F(0, f), i1 = F(1, f);
                        if (i0 < i1)
                            std::swap(i1, i0);
                        if (i0 == i1)
                            continue;
                        tbb::spin_mutex::scoped_lock lock1(locks[i0]);
                        tbb::spin_mutex::scoped_lock lock2(locks[i1]);
                        adj_new[i0].insert(i1);
                        adj_new[i1].insert(i0);
                    } else {
                        for (int j=0; j<F.rows(); ++j) {
                            uint32_t i0 = F(j, f), i1 = F((j+1)%F.rows(), f);
                            if (i0 < i1)
                                std::swap(i1, i0);
                            if (i0 == i1)
                                continue;
                            tbb::spin_mutex::scoped_lock lock1(locks[i0]);
                            tbb::spin_mutex::scoped_lock lock2(locks[i1]);
                            adj_new[i0].insert(i1);
                            adj_new[i1].insert(i0);
                        }
                    }
                }
            }
        );

        for (int it=0; it<smooth_iterations; ++it) {
            MatrixXf O_prime(O.rows(), O.cols());
            MatrixXf N_prime(O.rows(), O.cols());
            cout << ".";
            cout.flush();

            tbb::parallel_for(
                tbb::blocked_range<uint32_t>(0u, (uint32_t) O.cols(), GRAIN_SIZE),
                [&](const tbb::blocked_range<uint32_t> &range) {
                    std::set<uint32_t> temp;
                    for (uint32_t i = range.begin(); i != range.end(); ++i) {
                        bool is_crease = crease.find(i) != crease.end();
                        if (adj_new[i].size() > 0 && !is_crease) {
                            Vector3f centroid = Vector3f::Zero(), avgNormal = Vector3f::Zero();
                            for (auto j : adj_new[i]) {
                                centroid += O.col(j);
                                avgNormal += N.col(j);
                            }
                            avgNormal += N.col(i);
                            centroid /= adj_new[i].size();
                            Matrix3f cov = Matrix3f::Zero();
                            for (auto j : adj_new[i])
                                cov += (O.col(j)-centroid) * (O.col(j)-centroid).transpose();
                            Vector3f n = cov.jacobiSvd(Eigen::ComputeFullU).matrixU().col(2).normalized();
                            n *= signum(avgNormal.dot(n));

                            if (bvh && bvh->F()->size() > 0) {
                                Ray ray1(centroid,  n, 0, scale / 2);
                                Ray ray2(centroid, -n, 0, scale / 2);
                                uint32_t idx1 = 0, idx2 = 0;
                                Float t1 = 0, t2 = 0;
                                bvh->rayIntersect(ray1, idx1, t1);
                                bvh->rayIntersect(ray2, idx2, t2);
                                if (std::min(t1, t2) < scale*0.5f)
                                    centroid = t1 < t2 ? ray1(t1) : ray2(t2);
                            }
                            O_prime.col(i) = centroid;
                            N_prime.col(i) = n;
                        } else {
                            O_prime.col(i) = O.col(i);
                            N_prime.col(i) = N.col(i);
                        }
                    }
                }
            );
            O_prime.swap(O);
            N_prime.swap(N);
        }
        cout << " done. (took " << timeString(timer.reset()) << ")" << endl;
    }

    Nf.resize(3, F.cols());
    Nf.setZero();
    for (uint32_t i=0; i < F.cols(); ++i) {
        Vector3f centroid = Vector3f::Zero(), avgNormal = Vector3f::Zero();
        for (int j=0; j<F.rows(); ++j) {
            uint32_t k = F(j, i);
            centroid += O.col(k);
            avgNormal += N.col(k);
        }
        centroid /= F.rows();
        Matrix3f cov = Matrix3f::Zero();
        for (int j=0; j<F.rows(); ++j) {
            uint32_t k = F(j, i);
            cov += (O.col(k)-centroid) * (O.col(k)-centroid).transpose();
        }
        Vector3f n = cov.jacobiSvd(Eigen::ComputeFullU).matrixU().col(2).normalized();
        Nf.col(i) = n * signum(avgNormal.dot(n));
    }

    if (posy == 4 && !pure_quad) {
        for (auto f : irregular_faces) {
            Vector3f centroid = Vector3f::Zero(), avgNormal = Vector3f::Zero();
            for (uint32_t i=0; i<f.size(); ++i) {
                uint32_t k = F(0, f[i]);
                centroid += O.col(k);
                avgNormal += N.col(k);
            }
            centroid /= f.size();
            Matrix3f cov = Matrix3f::Zero();
            for (uint32_t i=0; i<f.size(); ++i) {
                uint32_t k = F(0, f[i]);
                cov += (O.col(k)-centroid) * (O.col(k)-centroid).transpose();
            }
            Vector3f n = cov.jacobiSvd(Eigen::ComputeFullU).matrixU().col(2).normalized();
            n *= signum(avgNormal.dot(n));
            for (uint32_t i=0; i<f.size(); ++i)
                Nf.col(f[i]) = n;
        }
    }

#if REMOVE_NONMANIFOLD
    cout << "Step 11: Removing nonmanifold elements.. ";
    remove_nonmanifold(F, O, Nf);
    cout << "done. (took " << timeString(timer.reset()) << ")" << endl;
#endif

    cout << "Step 12: Reordering mesh for efficient access .. ";
    cout.flush();
    std::vector<MatrixXf> V_vec(2), F_vec(1);
    V_vec[0].swap(O);
    V_vec[1].swap(N);
    F_vec[0].swap(Nf);
    reorder_mesh(F, V_vec, F_vec);
    V_vec[0].swap(O);
    V_vec[1].swap(N);
    F_vec[0].swap(Nf);
    cout << "done. (took " << timeString(timer.value()) << ")" << endl;

}
