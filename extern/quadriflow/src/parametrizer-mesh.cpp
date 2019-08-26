#include "config.hpp"
#include "dedge.hpp"
#include "field-math.hpp"
#include "loader.hpp"
#include "merge-vertex.hpp"
#include "parametrizer.hpp"
#include "subdivide.hpp"
#include "dedge.hpp"
#include <queue>

namespace qflow {

void Parametrizer::NormalizeMesh() {
    double maxV[3] = {-1e30, -1e30, -1e30};
    double minV[3] = {1e30, 1e30, 1e30};

    for (int i = 0; i < V.cols(); ++i) {
        for (int j = 0; j < 3; ++j) {
            maxV[j] = std::max(maxV[j], V(j, i));
            minV[j] = std::min(minV[j], V(j, i));
        }
    }
    double scale =
    std::max(std::max(maxV[0] - minV[0], maxV[1] - minV[1]), maxV[2] - minV[2]) * 0.5;
#ifdef WITH_OMP
#pragma omp parallel for
#endif
    for (int i = 0; i < V.cols(); ++i) {
        for (int j = 0; j < 3; ++j) {
            V(j, i) = (V(j, i) - (maxV[j] + minV[j]) * 0.5) / scale;
        }
    }
#ifdef LOG_OUTPUT
    printf("vertices size: %d\n", (int)V.cols());
    printf("faces size: %d\n", (int)F.cols());
#endif
    this->normalize_scale = scale;
    this->normalize_offset = Vector3d(0.5 * (maxV[0] + minV[0]), 0.5 * (maxV[1] + minV[1]), 0.5 * (maxV[2] + minV[2]));
    //    merge_close(V, F, 1e-6);
}

void Parametrizer::Load(const char* filename) {
    load(filename, V, F);
    NormalizeMesh();
}

void Parametrizer::Initialize(int faces) {
    ComputeMeshStatus();
    //ComputeCurvature(V, F, rho);
    rho.resize(V.cols(), 1);
    for (int i = 0; i < V.cols(); ++i) {
        rho[i] = 1;
    }
#ifdef PERFORMANCE_TEST
    scale = sqrt(surface_area / (V.cols() * 10));
#else
    if (faces <= 0) {
        scale = sqrt(surface_area / V.cols());
    } else {
        scale = std::sqrt(surface_area / faces);
    }
#endif
    double target_len = std::min(scale / 2, average_edge_length * 2);
#ifdef PERFORMANCE_TEST
    scale = sqrt(surface_area / V.cols());
#endif

    if (target_len < max_edge_length) {
        while (!compute_direct_graph(V, F, V2E, E2E, boundary, nonManifold))
            ;
        subdivide(F, V, rho, V2E, E2E, boundary, nonManifold, target_len);
    }
    
    while (!compute_direct_graph(V, F, V2E, E2E, boundary, nonManifold))
        ;
    generate_adjacency_matrix_uniform(F, V2E, E2E, nonManifold, adj);

    for (int iter = 0; iter < 5; ++iter) {
        VectorXd r(rho.size());
        for (int i = 0; i < rho.size(); ++i) {
            r[i] = rho[i];
            for (auto& id : adj[i]) {
                r[i] = std::min(r[i], rho[id.id]);
            }
        }
        rho = r;
    }
    ComputeSharpEdges();
    ComputeSmoothNormal();
    ComputeVertexArea();
    
    if (flag_adaptive_scale)
        ComputeInverseAffine();
    
#ifdef LOG_OUTPUT
    printf("V: %d F: %d\n", (int)V.cols(), (int)F.cols());
#endif
    hierarchy.mA[0] = std::move(A);
    hierarchy.mAdj[0] = std::move(adj);
    hierarchy.mN[0] = std::move(N);
    hierarchy.mV[0] = std::move(V);
    hierarchy.mE2E = std::move(E2E);
    hierarchy.mF = std::move(F);
    hierarchy.Initialize(scale, flag_adaptive_scale);
}

void Parametrizer::ComputeMeshStatus() {
    surface_area = 0;
    average_edge_length = 0;
    max_edge_length = 0;
    for (int f = 0; f < F.cols(); ++f) {
        Vector3d v[3] = {V.col(F(0, f)), V.col(F(1, f)), V.col(F(2, f))};
        double area = 0.5f * (v[1] - v[0]).cross(v[2] - v[0]).norm();
        surface_area += area;
        for (int i = 0; i < 3; ++i) {
            double len = (v[(i + 1) % 3] - v[i]).norm();
            average_edge_length += len;
            if (len > max_edge_length) max_edge_length = len;
        }
    }
    average_edge_length /= (F.cols() * 3);
}

void Parametrizer::ComputeSharpEdges() {
    sharp_edges.resize(F.cols() * 3, 0);

    if (flag_preserve_boundary) {
        for (int i = 0; i < sharp_edges.size(); ++i) {
            int re = E2E[i];
            if (re == -1) {
                sharp_edges[i] = 1;
            }
        }
    }

    if (flag_preserve_sharp == 0)
        return;

    std::vector<Vector3d> face_normals(F.cols());
    for (int i = 0; i < F.cols(); ++i) {
        Vector3d p1 = V.col(F(0, i));
        Vector3d p2 = V.col(F(1, i));
        Vector3d p3 = V.col(F(2, i));
        face_normals[i] = (p2 - p1).cross(p3 - p1).normalized();
    }

    double cos_thres = cos(60.0/180.0*3.141592654);
    for (int i = 0; i < sharp_edges.size(); ++i) {
        int e = i;
        int re = E2E[e];
        Vector3d& n1 = face_normals[e/3];
        Vector3d& n2 = face_normals[re/3];
        if (n1.dot(n2) < cos_thres) {
            sharp_edges[i] = 1;
        }
    }
}

void Parametrizer::ComputeSharpO() {
    auto& F = hierarchy.mF;
    auto& V = hierarchy.mV[0];
    auto& O = hierarchy.mO[0];
    auto& E2E = hierarchy.mE2E;
    DisajointTree tree(V.cols());
    for (int i = 0; i < edge_diff.size(); ++i) {
        if (edge_diff[i][0] == 0 && edge_diff[i][1] == 0) {
            tree.Merge(edge_values[i].x, edge_values[i].y);
        }
    }
    std::map<DEdge, std::vector<Vector3d> > edge_normals;
    for (int i = 0; i < F.cols(); ++i) {
        int pv[] = {tree.Parent(F(0, i)), tree.Parent(F(1, i)), tree.Parent(F(2, i))};
        if (pv[0] == pv[1] || pv[1] == pv[2] || pv[2] == pv[0])
            continue;
        DEdge e[] = {DEdge(pv[0], pv[1]), DEdge(pv[1], pv[2]), DEdge(pv[2], pv[0])};
        Vector3d d1 = O.col(F(1, i)) - O.col(F(0, i));
        Vector3d d2 = O.col(F(2, i)) - O.col(F(0, i));
        Vector3d n = d1.cross(d2).normalized();
        for (int j = 0; j < 3; ++j) {
            if (edge_normals.count(e[j]) == 0)
                edge_normals[e[j]] = std::vector<Vector3d>();
            edge_normals[e[j]].push_back(n);
        }
    }
    std::map<DEdge, int> sharps;
    for (auto& info : edge_normals) {
        auto& normals = info.second;
        bool sharp = false;
        for (int i = 0; i < normals.size(); ++i) {
            for (int j = i + 1; j < normals.size(); ++j) {
                if (normals[i].dot(normals[j]) < cos(60.0 / 180.0 * 3.141592654)) {
                    sharp = true;
                    break;
                }
            }
            if (sharp)
                break;
        }
        if (sharp) {
            int s = sharps.size();
            sharps[info.first] = s;
        }
    }
    for (auto& s : sharp_edges)
        s = 0;
    std::vector<int> sharp_hash(sharps.size(), 0);
    for (int i = 0; i < F.cols(); ++i) {
        for (int j = 0; j < 3; ++j) {
            int v1 = tree.Parent(F(j, i));
            int v2 = tree.Parent(F((j + 1) % 3, i));
            DEdge e(v1, v2);
            if (sharps.count(e) == 0)
                continue;
            int id = sharps[e];
            if (sharp_hash[id])
                continue;
            sharp_hash[id] = 1;
            sharp_edges[i * 3 + j] = 1;
            sharp_edges[E2E[i * 3 + j]] = 1;
        }
    }
    
}

void Parametrizer::ComputeSmoothNormal() {
    /* Compute face normals */
    Nf.resize(3, F.cols());
#ifdef WITH_OMP
#pragma omp parallel for
#endif
    for (int f = 0; f < F.cols(); ++f) {
        Vector3d v0 = V.col(F(0, f)), v1 = V.col(F(1, f)), v2 = V.col(F(2, f)),
        n = (v1 - v0).cross(v2 - v0);
        double norm = n.norm();
        if (norm < RCPOVERFLOW) {
            n = Vector3d::UnitX();
        } else {
            n /= norm;
        }
        Nf.col(f) = n;
    }
    
    N.resize(3, V.cols());
#ifdef WITH_OMP
#pragma omp parallel for
#endif
    for (int i = 0; i < V2E.rows(); ++i) {
        int edge = V2E[i];
        if (nonManifold[i] || edge == -1) {
            N.col(i) = Vector3d::UnitX();
            continue;
        }
        
        
        int stop = edge;
        do {
            if (sharp_edges[edge])
                break;
            edge = E2E[edge];
            if (edge != -1)
                edge = dedge_next_3(edge);
        } while (edge != stop && edge != -1);
        if (edge == -1)
            edge = stop;
        else
            stop = edge;
        Vector3d normal = Vector3d::Zero();
        do {
            int idx = edge % 3;
            
            Vector3d d0 = V.col(F((idx + 1) % 3, edge / 3)) - V.col(i);
            Vector3d d1 = V.col(F((idx + 2) % 3, edge / 3)) - V.col(i);
            double angle = fast_acos(d0.dot(d1) / std::sqrt(d0.squaredNorm() * d1.squaredNorm()));
            
            /* "Computing Vertex Normals from Polygonal Facets"
             by Grit Thuermer and Charles A. Wuethrich, JGT 1998, Vol 3 */
            if (std::isfinite(angle)) normal += Nf.col(edge / 3) * angle;
            
            int opp = E2E[edge];
            if (opp == -1) break;
            
            edge = dedge_next_3(opp);
            if (sharp_edges[edge])
                break;
        } while (edge != stop);
        double norm = normal.norm();
        N.col(i) = norm > RCPOVERFLOW ? Vector3d(normal / norm) : Vector3d::UnitX();
    }
}

void Parametrizer::ComputeVertexArea() {
    A.resize(V.cols());
    A.setZero();
    
#ifdef WITH_OMP
#pragma omp parallel for
#endif
    for (int i = 0; i < V2E.size(); ++i) {
        int edge = V2E[i], stop = edge;
        if (nonManifold[i] || edge == -1) continue;
        double vertex_area = 0;
        do {
            int ep = dedge_prev_3(edge), en = dedge_next_3(edge);
            
            Vector3d v = V.col(F(edge % 3, edge / 3));
            Vector3d vn = V.col(F(en % 3, en / 3));
            Vector3d vp = V.col(F(ep % 3, ep / 3));
            
            Vector3d face_center = (v + vp + vn) * (1.0f / 3.0f);
            Vector3d prev = (v + vp) * 0.5f;
            Vector3d next = (v + vn) * 0.5f;
            
            vertex_area += 0.5f * ((v - prev).cross(v - face_center).norm() +
                                   (v - next).cross(v - face_center).norm());
            
            int opp = E2E[edge];
            if (opp == -1) break;
            edge = dedge_next_3(opp);
        } while (edge != stop);
        
        A[i] = vertex_area;
    }
}

void Parametrizer::FixValence()
{
    // Remove Valence 2
    while (true) {
        bool update = false;
        std::vector<int> marks(V2E_compact.size(), 0);
        std::vector<int> erasedF(F_compact.size(), 0);
        for (int i = 0; i < V2E_compact.size(); ++i) {
            int deid0 = V2E_compact[i];
            if (marks[i] || deid0 == -1)
                continue;
            int deid = deid0;
            std::vector<int> dedges;
            do {
                dedges.push_back(deid);
                int deid1 = deid / 4 * 4 + (deid + 3) % 4;
                deid = E2E_compact[deid1];
            } while (deid != deid0 && deid != -1);
            if (dedges.size() == 2) {
                int v1 = F_compact[dedges[0]/4][(dedges[0] + 1)%4];
                int v2 = F_compact[dedges[0]/4][(dedges[0] + 2)%4];
                int v3 = F_compact[dedges[1]/4][(dedges[1] + 1)%4];
                int v4 = F_compact[dedges[1]/4][(dedges[1] + 2)%4];
                if (marks[v1] || marks[v2] || marks[v3] || marks[v4])
                    continue;
                marks[v1] = true;
                marks[v2] = true;
                marks[v3] = true;
                marks[v4] = true;
                if (v1 == v2 || v1 == v3 || v1 == v4 || v2 == v3 || v2 == v4 || v3 == v4) {
                    erasedF[dedges[0]/4] = 1;
                } else {
                    F_compact[dedges[0]/4] = Vector4i(v1, v2, v3, v4);
                }
                erasedF[dedges[1]/4] = 1;
                update = true;
            }
        }
        if (update) {
            int top = 0;
            for (int i = 0; i < erasedF.size(); ++i) {
                if (erasedF[i] == 0) {
                    F_compact[top++] = F_compact[i];
                }
            }
            F_compact.resize(top);
            compute_direct_graph_quad(O_compact, F_compact, V2E_compact, E2E_compact, boundary_compact,
                                      nonManifold_compact);
        } else {
            break;
        }
    }
    std::vector<std::vector<int> > v_dedges(V2E_compact.size());
    for (int i = 0; i < F_compact.size(); ++i) {
        for (int j = 0; j < 4; ++j) {
            v_dedges[F_compact[i][j]].push_back(i * 4 + j);
        }
    }
    int top = V2E_compact.size();
    for (int i = 0; i < v_dedges.size(); ++i) {
        std::map<int, int> groups;
        int group_id = 0;
        for (int j = 0; j < v_dedges[i].size(); ++j) {
            int deid = v_dedges[i][j];
            if (groups.count(deid))
                continue;
            int deid0 = deid;
            do {
                groups[deid] = group_id;
                deid = deid / 4 * 4 + (deid + 3) % 4;
                deid = E2E_compact[deid];
            } while (deid != deid0 && deid != -1);
            if (deid == -1) {
                deid = deid0;
                while (E2E_compact[deid] != -1) {
                    deid = E2E_compact[deid];
                    deid = deid / 4 * 4 + (deid + 1) % 4;
                    groups[deid] = group_id;
                }
            }
            group_id += 1;
        }
        if (group_id > 1) {
            for (auto& g : groups) {
                if (g.second >= 1)
                    F_compact[g.first/4][g.first%4] = top - 1 + g.second;
            }
            for (int j = 1; j < group_id; ++j) {
                Vset.push_back(Vset[i]);
                N_compact.push_back(N_compact[i]);
                Q_compact.push_back(Q_compact[i]);
                O_compact.push_back(O_compact[i]);
            }
            top = O_compact.size();
        }
    }
    compute_direct_graph_quad(O_compact, F_compact, V2E_compact, E2E_compact, boundary_compact,
                              nonManifold_compact);
    
    // Decrease Valence
    while (true) {
        bool update = false;
        std::vector<int> marks(V2E_compact.size(), 0);
        std::vector<int> valences(V2E_compact.size(), 0);
        for (int i = 0; i < V2E_compact.size(); ++i) {
            int deid0 = V2E_compact[i];
            if (deid0 == -1)
                continue;
            int deid = deid0;
            int count = 0;
            do {
                count += 1;
                int deid1 = E2E_compact[deid];
                if (deid1 == -1) {
                    count += 1;
                    break;
                }
                deid = deid1 / 4 * 4 + (deid1 + 1) % 4;
            } while (deid != deid0 && deid != -1);
            if (deid == -1)
                count += 1;
            valences[i] = count;
        }
        std::priority_queue<std::pair<int, int> > prior_queue;
        for (int i = 0; i < valences.size(); ++i) {
            if (valences[i] > 5)
                prior_queue.push(std::make_pair(valences[i], i));
        }
        while (!prior_queue.empty()) {
            auto info = prior_queue.top();
            prior_queue.pop();
            if (marks[info.second])
                continue;
            int deid0 = V2E_compact[info.second];
            if (deid0 == -1)
                continue;
            int deid = deid0;
            std::vector<int> loop_vertices, loop_dedges;;
            bool marked = false;
            do {
                int v = F_compact[deid/4][(deid+1)%4];
                loop_dedges.push_back(deid);
                loop_vertices.push_back(v);
                if (marks[v])
                    marked = true;
                int deid1 = E2E_compact[deid];
                if (deid1 == -1)
                    break;
                deid = deid1 / 4 * 4 + (deid1 + 1) % 4;
            } while (deid != deid0 && deid != -1);
            if (marked)
                continue;

            if (deid != -1) {
                int step = (info.first + 1) / 2;
                std::pair<int, int> min_val(0x7fffffff, 0x7fffffff);
                int split_idx = -1;
                for (int i = 0; i < loop_vertices.size(); ++i) {
                    if (i + step >= loop_vertices.size())
                        continue;
                    int v1 = valences[loop_vertices[i]];
                    int v2 = valences[loop_vertices[i + step]];
                    if (v1 < v2)
                        std::swap(v1, v2);
                    auto key = std::make_pair(v1, v2);
                    if (key < min_val) {
                        min_val = key;
                        split_idx = i + 1;
                    }
                }
                if (min_val.first >= info.first)
                    continue;
                update = true;
                for (int id = split_idx; id < split_idx + step; ++id) {
                    F_compact[loop_dedges[id]/4][loop_dedges[id]%4] = O_compact.size();
                }
                F_compact.push_back(Vector4i(O_compact.size(), loop_vertices[(split_idx+loop_vertices.size()-1)%loop_vertices.size()],info.second, loop_vertices[(split_idx + step - 1 + loop_vertices.size()) % loop_vertices.size()]));
            } else {
                for (int id = loop_vertices.size() / 2; id < loop_vertices.size(); ++id) {
                    F_compact[loop_dedges[id]/4][loop_dedges[id]%4] = O_compact.size();
                }
                update = true;
            }
            marks[info.second] = 1;
            for (int i = 0; i < loop_vertices.size(); ++i) {
                marks[loop_vertices[i]] = 1;
            }
            Vset.push_back(Vset[info.second]);
            O_compact.push_back(O_compact[info.second]);
            N_compact.push_back(N_compact[info.second]);
            Q_compact.push_back(Q_compact[info.second]);
        }
        if (!update) {
            break;
        } else {
            compute_direct_graph_quad(O_compact, F_compact, V2E_compact, E2E_compact, boundary_compact,
                                      nonManifold_compact);
        }
    }
    // Remove Zero Valence
    std::vector<int> valences(V2E_compact.size(), 0);
    for (int i = 0; i < F_compact.size(); ++i) {
        for (int j = 0; j < 4; ++j) {
            valences[F_compact[i][j]] = 1;
        }
    }
    top = 0;
    std::vector<int> compact_indices(valences.size());
    for (int i = 0; i < valences.size(); ++i) {
        if (valences[i] == 0)
            continue;
        N_compact[top] = N_compact[i];
        O_compact[top] = O_compact[i];
        Q_compact[top] = Q_compact[i];
        Vset[top] = Vset[i];
        compact_indices[i] = top;
        top += 1;
    }
    for (int i = 0; i < F_compact.size(); ++i) {
        for (int j = 0; j < 4; ++j) {
            F_compact[i][j] = compact_indices[F_compact[i][j]];
        }
    }
    N_compact.resize(top);
    O_compact.resize(top);
    Q_compact.resize(top);
    Vset.resize(top);
    compute_direct_graph_quad(O_compact, F_compact, V2E_compact, E2E_compact, boundary_compact,
                              nonManifold_compact);
    {
        compute_direct_graph_quad(O_compact, F_compact, V2E_compact, E2E_compact, boundary_compact,
                                  nonManifold_compact);
        std::vector<int> masks(F_compact.size() * 4, 0);
        for (int i = 0; i < V2E_compact.size(); ++i) {
            int deid0 = V2E_compact[i];
            if (deid0 == -1)
                continue;
            int deid = deid0;
            do {
                masks[deid] = 1;
                deid = E2E_compact[deid];
                if (deid == -1) {
                    break;
                }
                deid = deid / 4 * 4 + (deid + 1) % 4;
            } while (deid != deid0 && deid != -1);
        }
        std::vector<std::vector<int> > v_dedges(V2E_compact.size());
        for (int i = 0; i < F_compact.size(); ++i) {
            for (int j = 0; j < 4; ++j) {
                v_dedges[F_compact[i][j]].push_back(i * 4 + j);
            }
        }
    }
    std::map<int, int> pts;
    for (int i = 0; i < V2E_compact.size(); ++i) {
        int deid0 = V2E_compact[i];
        if (deid0 == -1)
            continue;
        int deid = deid0;
        int count = 0;
        do {
            count += 1;
            int deid1 = E2E_compact[deid];
            if (deid1 == -1)
                break;
            deid = deid1 / 4 * 4 + (deid1 + 1) % 4;
        } while (deid != deid0 && deid != -1);
        if (pts.count(count) == 0)
            pts[count] = 1;
        else
            pts[count] += 1;
    }
    return;
}

void Parametrizer::OutputMesh(const char* obj_name) {
    std::ofstream os(obj_name);
    for (int i = 0; i < O_compact.size(); ++i) {
        auto t = O_compact[i] * this->normalize_scale + this->normalize_offset;
        os << "v " << t[0] << " " << t[1] << " " << t[2] << "\n";
    }
    for (int i = 0; i < F_compact.size(); ++i) {
        os << "f " << F_compact[i][0]+1 << " " << F_compact[i][1]+1
        << " " << F_compact[i][2]+1 << " " << F_compact[i][3]+1
        << "\n";
    }
    os.close();
}

} // namespace qflow
