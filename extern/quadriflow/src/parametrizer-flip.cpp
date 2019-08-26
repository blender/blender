#include "dedge.hpp"
#include "parametrizer.hpp"

#include <algorithm>
#include <queue>
#include <unordered_map>
#include <vector>

namespace qflow {

double Parametrizer::QuadEnergy(std::vector<int>& loop_vertices, std::vector<Vector4i>& res_quads,
                                int level) {
    if (loop_vertices.size() < 4) return 0;
    if (loop_vertices.size() == 4) {
        double energy = 0;
        for (int j = 0; j < 4; ++j) {
            int v0 = loop_vertices[j];
            int v2 = loop_vertices[(j + 1) % 4];
            int v1 = loop_vertices[(j + 3) % 4];
            Vector3d pt1 = (O_compact[v1] - O_compact[v0]).normalized();
            Vector3d pt2 = (O_compact[v2] - O_compact[v0]).normalized();
            Vector3d n = pt1.cross(pt2);
            double sina = n.norm();
            if (n.dot(N_compact[v0]) < 0) sina = -sina;
            double cosa = pt1.dot(pt2);
            double angle = atan2(sina, cosa) / 3.141592654 * 180.0;
            if (angle < 0) angle = 360 + angle;
            energy += angle * angle;
        }
        res_quads.push_back(
            Vector4i(loop_vertices[0], loop_vertices[3], loop_vertices[2], loop_vertices[1]));
        return energy;
    }
    double max_energy = 1e30;
    for (int seg1 = 2; seg1 < loop_vertices.size(); seg1 += 2) {
        for (int seg2 = seg1 + 1; seg2 < loop_vertices.size(); seg2 += 2) {
            std::vector<Vector4i> quads[4];
            std::vector<int> vertices = {loop_vertices[0], loop_vertices[1], loop_vertices[seg1],
                                         loop_vertices[seg2]};
            double energy = 0;
            energy += QuadEnergy(vertices, quads[0], level + 1);
            if (seg1 > 2) {
                std::vector<int> vertices(loop_vertices.begin() + 1, loop_vertices.begin() + seg1);
                vertices.push_back(loop_vertices[seg1]);
                energy += QuadEnergy(vertices, quads[1], level + 1);
            }
            if (seg2 != seg1 + 1) {
                std::vector<int> vertices(loop_vertices.begin() + seg1,
                                          loop_vertices.begin() + seg2);
                vertices.push_back(loop_vertices[seg2]);
                energy += QuadEnergy(vertices, quads[2], level + 2);
            }
            if (seg2 + 1 != loop_vertices.size()) {
                std::vector<int> vertices(loop_vertices.begin() + seg2, loop_vertices.end());
                vertices.push_back(loop_vertices[0]);
                energy += QuadEnergy(vertices, quads[3], level + 1);
            }
            if (max_energy > energy) {
                max_energy = energy;
                res_quads.clear();
                for (int i = 0; i < 4; ++i) {
                    for (auto& v : quads[i]) {
                        res_quads.push_back(v);
                    }
                }
            }
        }
    }
    return max_energy;
}

void Parametrizer::FixHoles(std::vector<int>& loop_vertices) {
    std::vector<std::vector<int>> loop_vertices_array;
    std::unordered_map<int, int> map_loops;
    for (int i = 0; i < loop_vertices.size(); ++i) {
        if (map_loops.count(loop_vertices[i])) {
            int j = map_loops[loop_vertices[i]];
            loop_vertices_array.push_back(std::vector<int>());
            if (i - j > 3 && (i - j) % 2 == 0) {
                for (int k = j; k < i; ++k) {
                    if (map_loops.count(loop_vertices[k])) {
                        loop_vertices_array.back().push_back(loop_vertices[k]);
                        map_loops.erase(loop_vertices[k]);
                    }
                }
            }
        }
        map_loops[loop_vertices[i]] = i;
    }
    if (map_loops.size() >= 3) {
        loop_vertices_array.push_back(std::vector<int>());
        for (int k = 0; k < loop_vertices.size(); ++k) {
            if (map_loops.count(loop_vertices[k])) {
                if (map_loops.count(loop_vertices[k])) {
                    loop_vertices_array.back().push_back(loop_vertices[k]);
                    map_loops.erase(loop_vertices[k]);
                }
            }
        }
    }
    for (int i = 0; i < loop_vertices_array.size(); ++i) {
        auto& loop_vertices = loop_vertices_array[i];
        if (loop_vertices.size() == 0) return;
        std::vector<Vector4i> quads;
#ifdef LOG_OUTPUT
//        printf("Compute energy for loop: %d\n", (int)loop_vertices.size());
#endif
        QuadEnergy(loop_vertices, quads, 0);
#ifdef LOG_OUTPUT
//        printf("quads: %d\n", quads.size());
#endif
        for (auto& p : quads) {
            bool flag = false;
            for (int j = 0; j < 4; ++j) {
                int v1 = p[j];
                int v2 = p[(j + 1) % 4];
                auto key = std::make_pair(v1, v2);
                if (Quad_edges.count(key)) {
                    flag = true;
                    break;
                }
            }
            if (!flag) {
                for (int j = 0; j < 4; ++j) {
                    int v1 = p[j];
                    int v2 = p[(j + 1) % 4];
                    auto key = std::make_pair(v1, v2);
                    Quad_edges.insert(key);
                }
                F_compact.push_back(p);
            }
        }
    }
}

void Parametrizer::FixHoles() {
    for (int i = 0; i < F_compact.size(); ++i) {
        for (int j = 0; j < 4; ++j) {
            int v1 = F_compact[i][j];
            int v2 = F_compact[i][(j + 1) % 4];
            auto key = std::make_pair(v1, v2);
            Quad_edges.insert(key);
        }
    }
    std::vector<int> detected_boundary(E2E_compact.size(), 0);
    for (int i = 0; i < E2E_compact.size(); ++i) {
        if (detected_boundary[i] != 0 || E2E_compact[i] != -1) continue;
        std::vector<int> loop_edges;
        int current_e = i;

        while (detected_boundary[current_e] == 0) {
            detected_boundary[current_e] = 1;
            loop_edges.push_back(current_e);
            current_e = current_e / 4 * 4 + (current_e + 1) % 4;
            while (E2E_compact[current_e] != -1) {
                current_e = E2E_compact[current_e];
                current_e = current_e / 4 * 4 + (current_e + 1) % 4;
            }
        }
        std::vector<int> loop_vertices(loop_edges.size());
        for (int j = 0; j < loop_edges.size(); ++j) {
            loop_vertices[j] = F_compact[loop_edges[j] / 4][loop_edges[j] % 4];
        }
        if (loop_vertices.size() < 25) FixHoles(loop_vertices);
    }
}

void Parametrizer::FixFlipHierarchy() {
    Hierarchy fh;
    fh.DownsampleEdgeGraph(face_edgeOrients, face_edgeIds, edge_diff, allow_changes, -1);
    fh.FixFlip();
    fh.UpdateGraphValue(face_edgeOrients, face_edgeIds, edge_diff);
}

void Parametrizer::FixFlipSat() {
#ifdef LOG_OUTPUT
    printf("Solving SAT!\n");
#endif

    if (!this->flag_aggresive_sat) return;

    for (int threshold = 1; threshold <= 4; ++threshold) {
        lprintf("[FixFlipSat] threshold = %d\n", threshold);

        Hierarchy fh;
        fh.DownsampleEdgeGraph(face_edgeOrients, face_edgeIds, edge_diff, allow_changes, -1);
        int nflip = 0;
        for (int depth = std::min(5, (int)fh.mFQ.size() - 1); depth >= 0; --depth) {
            nflip = fh.FixFlipSat(depth, threshold);
            if (depth > 0) fh.PushDownwardFlip(depth);
            if (nflip == 0) break;
        }
        fh.UpdateGraphValue(face_edgeOrients, face_edgeIds, edge_diff);
        if (nflip == 0) break;
    }
}

void Parametrizer::AdvancedExtractQuad() {
    Hierarchy fh;
    fh.DownsampleEdgeGraph(face_edgeOrients, face_edgeIds, edge_diff, allow_changes, -1);
    auto& V = hierarchy.mV[0];
    auto& F = hierarchy.mF;
    disajoint_tree = DisajointTree(V.cols());
    auto& diffs = fh.mEdgeDiff.front();
    for (int i = 0; i < diffs.size(); ++i) {
        if (diffs[i] == Vector2i::Zero()) {
            disajoint_tree.Merge(edge_values[i].x, edge_values[i].y);
        }
    }
    disajoint_tree.BuildCompactParent();
    auto& F2E = fh.mF2E.back();
    auto& E2F = fh.mE2F.back();
    auto& EdgeDiff = fh.mEdgeDiff.back();
    auto& FQ = fh.mFQ.back();

    std::vector<int> edge(E2F.size());
    std::vector<int> face(F2E.size());
    for (int i = 0; i < diffs.size(); ++i) {
        int t = i;
        for (int j = 0; j < fh.mToUpperEdges.size(); ++j) {
            t = fh.mToUpperEdges[j][t];
            if (t < 0) break;
        }
        if (t >= 0) edge[t] = i;
    }
    for (int i = 0; i < F.cols(); ++i) {
        int t = i;
        for (int j = 0; j < fh.mToUpperFaces.size(); ++j) {
            t = fh.mToUpperFaces[j][t];
            if (t < 0) break;
        }
        if (t >= 0) face[t] = i;
    }
    fh.UpdateGraphValue(face_edgeOrients, face_edgeIds, edge_diff);

    auto& O = hierarchy.mO[0];
    auto& Q = hierarchy.mQ[0];
    auto& N = hierarchy.mN[0];
    int num_v = disajoint_tree.CompactNum();
    Vset.resize(num_v);
    O_compact.resize(num_v, Vector3d::Zero());
    Q_compact.resize(num_v, Vector3d::Zero());
    N_compact.resize(num_v, Vector3d::Zero());
    counter.resize(num_v, 0);
    for (int i = 0; i < O.cols(); ++i) {
        int compact_v = disajoint_tree.Index(i);
        Vset[compact_v].push_back(i);
        O_compact[compact_v] += O.col(i);
        N_compact[compact_v] = N_compact[compact_v] * counter[compact_v] + N.col(i);
        N_compact[compact_v].normalize();
        if (counter[compact_v] == 0)
            Q_compact[compact_v] = Q.col(i);
        else {
            auto pairs = compat_orientation_extrinsic_4(Q_compact[compact_v], N_compact[compact_v],
                                                        Q.col(i), N.col(i));
            Q_compact[compact_v] = (pairs.first * counter[compact_v] + pairs.second).normalized();
        }
        counter[compact_v] += 1;
    }
    for (int i = 0; i < O_compact.size(); ++i) {
        O_compact[i] /= counter[i];
    }

    BuildTriangleManifold(disajoint_tree, edge, face, edge_values, F2E, E2F, EdgeDiff, FQ);
}

void Parametrizer::BuildTriangleManifold(DisajointTree& disajoint_tree, std::vector<int>& edge,
                                         std::vector<int>& face, std::vector<DEdge>& edge_values,
                                         std::vector<Vector3i>& F2E, std::vector<Vector2i>& E2F,
                                         std::vector<Vector2i>& EdgeDiff,
                                         std::vector<Vector3i>& FQ) {
    auto& F = hierarchy.mF;
    std::vector<int> E2E(F2E.size() * 3, -1);
    for (int i = 0; i < E2F.size(); ++i) {
        int v1 = E2F[i][0];
        int v2 = E2F[i][1];
        int t1 = 0;
        int t2 = 2;
        if (v1 != -1)
            while (F2E[v1][t1] != i) t1 += 1;
        if (v2 != -1)
            while (F2E[v2][t2] != i) t2 -= 1;
        t1 += v1 * 3;
        t2 += v2 * 3;
        if (v1 != -1)
            E2E[t1] = (v2 == -1) ? -1 : t2;
        if (v2 != -1)
            E2E[t2] = (v1 == -1) ? -1 : t1;
    }

    std::vector<Vector3i> triangle_vertices(F2E.size(), Vector3i(-1, -1, -1));
    int num_v = 0;
    std::vector<Vector3d> N, Q, O;
    std::vector<std::vector<int>> Vs;
    for (int i = 0; i < F2E.size(); ++i) {
        for (int j = 0; j < 3; ++j) {
            if (triangle_vertices[i][j] != -1) continue;
            int f = face[i];
            int v = disajoint_tree.Index(F(j, f));
            Vs.push_back(Vset[v]);
            Q.push_back(Q_compact[v]);
            N.push_back(N_compact[v]);
            O.push_back(O_compact[v]);
            int deid0 = i * 3 + j;
            int deid = deid0;
            do {
                triangle_vertices[deid / 3][deid % 3] = num_v;
                deid = E2E[deid / 3 * 3 + (deid + 2) % 3];
            } while (deid != deid0 && deid != -1);
            if (deid == -1) {
                deid = deid0;
                do {
                    deid = E2E[deid];
                    if (deid == -1)
                        break;
                    deid = deid / 3 * 3 + (deid + 1) % 3;
                    triangle_vertices[deid/3][deid%3] = num_v;
                } while (deid != -1);
            }
            num_v += 1;
        }
    }

    int num_v0 = num_v;
    do {
        num_v0 = num_v;
        std::vector<std::vector<int>> vert_to_dedge(num_v);
        for (int i = 0; i < triangle_vertices.size(); ++i) {
            Vector3i pt = triangle_vertices[i];
            if (pt[0] == pt[1] || pt[1] == pt[2] || pt[2] == pt[0]) {
                for (int j = 0; j < 3; ++j) {
                    int t = E2E[i * 3 + j];
                    if (t != -1) E2E[t] = -1;
                }
                for (int j = 0; j < 3; ++j) {
                    E2E[i * 3 + j] = -1;
                }
            } else {
                for (int j = 0; j < 3; ++j)
                    vert_to_dedge[triangle_vertices[i][j]].push_back(i * 3 + j);
            }
        }
        std::vector<int> colors(triangle_vertices.size() * 3, -1),
            reverse_colors(triangle_vertices.size() * 3, -1);
        for (int i = 0; i < vert_to_dedge.size(); ++i) {
            int num_color = 0;
            for (int j = 0; j < vert_to_dedge[i].size(); ++j) {
                int deid = vert_to_dedge[i][j];
                if (colors[deid] != -1) continue;
                std::list<int> l;
                int deid0 = deid;
                do {
                    l.push_back(deid);
                    deid = deid / 3 * 3 + (deid + 2) % 3;
                    deid = E2E[deid];
                } while (deid != -1 && deid != deid0);
                if (deid == -1) {
                    deid = deid0;
                    do {
                        deid = E2E[deid];
                        if (deid == -1) break;
                        deid = deid / 3 * 3 + (deid + 1) % 3;
                        if (deid == deid0) break;
                        l.push_front(deid);
                    } while (true);
                }
                std::vector<int> dedges;
                for (auto& e : l) dedges.push_back(e);
                std::map<std::pair<int, int>, int> loc;
                std::vector<int> deid_colors(dedges.size(), num_color);
                num_color += 1;
                for (int jj = 0; jj < dedges.size(); ++jj) {
                    int deid = dedges[jj];
                    colors[deid] = 0;
                    int v1 = triangle_vertices[deid / 3][deid % 3];
                    int v2 = triangle_vertices[deid / 3][(deid + 1) % 3];
                    std::pair<int, int> pt(v1, v2);
                    if (loc.count(pt)) {
                        int s = loc[pt];
                        for (int k = s; k < jj; ++k) {
                            int deid1 = dedges[k];
                            int v11 = triangle_vertices[deid1 / 3][deid1 % 3];
                            int v12 = triangle_vertices[deid1 / 3][(deid1 + 1) % 3];
                            std::pair<int, int> pt1(v11, v12);
                            loc.erase(pt1);
                            deid_colors[k] = num_color;
                        }
                        num_color += 1;
                    }
                    loc[pt] = jj;
                }
                for (int j = 0; j < dedges.size(); ++j) {
                    int deid = dedges[j];
                    int color = deid_colors[j];
                    if (color > 0) {
                        triangle_vertices[deid / 3][deid % 3] = num_v + color - 1;
                    }
                }
            }
            if (num_color > 1) {
                for (int j = 0; j < num_color - 1; ++j) {
                    Vs.push_back(Vs[i]);
                    O.push_back(O[i]);
                    N.push_back(N[i]);
                    Q.push_back(Q[i]);
                }
                num_v += num_color - 1;
            }
        }
    } while (num_v != num_v0);
    int offset = 0;
    std::vector<Vector3i> triangle_edges, triangle_orients;
    for (int i = 0; i < triangle_vertices.size(); ++i) {
        Vector3i pt = triangle_vertices[i];
        if (pt[0] == pt[1] || pt[1] == pt[2] || pt[2] == pt[0]) continue;
        triangle_vertices[offset++] = triangle_vertices[i];
        triangle_edges.push_back(F2E[i]);
        triangle_orients.push_back(FQ[i]);
    }
    triangle_vertices.resize(offset);
    std::set<int> flip_vertices;
    for (int i = 0; i < triangle_vertices.size(); ++i) {
        Vector2i d1 = rshift90(EdgeDiff[triangle_edges[i][0]], triangle_orients[i][0]);
        Vector2i d2 = rshift90(EdgeDiff[triangle_edges[i][1]], triangle_orients[i][1]);
        int area = d1[0] * d2[1] - d1[1] * d2[0];
        if (area < 0) {
            for (int j = 0; j < 3; ++j) {
                flip_vertices.insert(triangle_vertices[i][j]);
            }
        }
    }
    MatrixXd NV(3, num_v);
    MatrixXi NF(3, triangle_vertices.size());
    memcpy(NF.data(), triangle_vertices.data(), sizeof(int) * 3 * triangle_vertices.size());
    VectorXi NV2E, NE2E, NB, NN;
    compute_direct_graph(NV, NF, NV2E, NE2E, NB, NN);

    std::map<DEdge, std::pair<Vector3i, Vector3i>> quads;
    for (int i = 0; i < triangle_vertices.size(); ++i) {
        for (int j = 0; j < 3; ++j) {
            int e = triangle_edges[i][j];
            int v1 = triangle_vertices[i][j];
            int v2 = triangle_vertices[i][(j + 1) % 3];
            int v3 = triangle_vertices[i][(j + 2) % 3];
            if (abs(EdgeDiff[e][0]) == 1 && abs(EdgeDiff[e][1]) == 1) {
                DEdge edge(v1, v2);
                if (quads.count(edge))
                    quads[edge].second = Vector3i(v1, v2, v3);
                else
                    quads[edge] = std::make_pair(Vector3i(v1, v2, v3), Vector3i(-1, -1, -1));
            }
        }
    }

    for (auto& p : quads) {
        if (p.second.second[0] != -1 && p.second.first[2] != p.second.second[2]) {
            F_compact.push_back(Vector4i(p.second.first[1], p.second.first[2], p.second.first[0],
                                         p.second.second[2]));
        }
    }
    std::swap(Vs, Vset);
    std::swap(O_compact, O);
    std::swap(N_compact, N);
    std::swap(Q_compact, Q);
    compute_direct_graph_quad(O_compact, F_compact, V2E_compact, E2E_compact, boundary_compact,
                              nonManifold_compact);

    while (true) {
        std::vector<int> erasedF(F_compact.size(), 0);
        for (int i = 0; i < F_compact.size(); ++i) {
            for (int j = 0; j < 3; ++j) {
                for (int k = j + 1; k < 4; ++k) {
                    if (F_compact[i][j] == F_compact[i][k]) {
                        erasedF[i] = 1;
                    }
                }
            }
        }
        for (int i = 0; i < O_compact.size(); ++i) {
            int v = 0;
            int e0 = V2E_compact[i];
            if (e0 == -1) continue;
            std::vector<int> dedges;
            int e = e0;
            do {
                dedges.push_back(e);
                v += 1;
                e = e / 4 * 4 + (e + 3) % 4;
                e = E2E_compact[e];
            } while (e != e0 && e != -1);
            if (e == -1) {
                int e = e0;
                while (true) {
                    e = E2E_compact[e];
                    if (e == -1) break;
                    e = e / 4 * 4 + (e + 1) % 4;
                    v += 1;
                    dedges.push_back(e);
                }
            }
            if (v == 2) {
                //                erasedF[dedges[1] / 4] = 1;
                //                F_compact[dedges[0]/4][dedges[0]%4] =
                //                F_compact[dedges[1]/4][(dedges[1]+2)%4];
            }
        }
        offset = 0;
        for (int i = 0; i < F_compact.size(); ++i) {
            if (erasedF[i] == 0) F_compact[offset++] = F_compact[i];
        }
        if (offset == F_compact.size()) break;
        F_compact.resize(offset);
        compute_direct_graph_quad(O_compact, F_compact, V2E_compact, E2E_compact, boundary_compact,
                                  nonManifold_compact);
    }
    FixHoles();
    compute_direct_graph_quad(O_compact, F_compact, V2E_compact, E2E_compact, boundary_compact,
                              nonManifold_compact);

    /*
    for (auto& p : flip_vertices) {
        int deid0 = V2E_compact[p];
        int deid = deid0;
        std::list<int> dedges;
        if (deid0 != -1) {
            do {
                dedges.push_back(deid);
                deid = E2E_compact[deid/4*4 + (deid+3) % 4];
            } while (deid != -1 && deid != deid0);
            if (deid == -1) {
                deid = deid0;
                do {
                    deid = E2E_compact[deid];
                    if (deid == -1)
                        break;
                    deid = deid/4*4 + (deid +1) % 4;
                    dedges.push_front(deid);
                } while (deid != -1 && deid != deid0);
            }
            std::set<int> eraseF;
            std::set<int> valid_dedges;
            std::set<int> boundaries;
            std::vector<int> loop_vertices;
            for (auto& dedge : dedges) {
                int f = dedge / 4;
                eraseF.insert(f);
                valid_dedges.insert(E2E_compact[f * 4 + (dedge+1)%4]);
                valid_dedges.insert(E2E_compact[f * 4 + (dedge+2)%4]);
                loop_vertices.push_back(F_compact[f][(dedge+1)%4]);
                loop_vertices.push_back(F_compact[f][(dedge+2)%4]);
                boundaries.insert(F_compact[f][(dedge+1)%4]);
                boundaries.insert(F_compact[f][(dedge+2)%4]);
            }
            int offset = 0;
            auto it = eraseF.begin();
            for (int i = 0; i < F_compact.size(); ++i) {
                if (it == eraseF.end() || i != *it) {
                    bool need_erase = false;
                    for (int j = 0; j < 4; ++j) {
                        if (valid_dedges.count(i * 4 + j) == 0 && boundaries.count(F_compact[i][j])
    && boundaries.count(F_compact[i][(j + 1) % 4])) { need_erase = true;
                        }
                    }
                    if (!need_erase)
                        F_compact[offset++] = F_compact[i];
                } else {
                    it++;
                }
            }
            F_compact.resize(offset);
            compute_direct_graph_quad(O_compact, F_compact, V2E_compact, E2E_compact,
    boundary_compact, nonManifold_compact); std::reverse(loop_vertices.begin(),
    loop_vertices.end()); FixHoles(loop_vertices); compute_direct_graph_quad(O_compact, F_compact,
    V2E_compact, E2E_compact, boundary_compact, nonManifold_compact);
        }
    }
    FixHoles();
    compute_direct_graph_quad(O_compact, F_compact, V2E_compact, E2E_compact, boundary_compact,
                              nonManifold_compact);
     */
}

} // namespace qflow
