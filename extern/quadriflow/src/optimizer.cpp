#include "optimizer.hpp"

#include <Eigen/Sparse>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <queue>
#include <unordered_map>

#include "config.hpp"
#include "field-math.hpp"
#include "flow.hpp"
#include "parametrizer.hpp"

namespace qflow {

#ifdef WITH_CUDA
#    include <cuda_runtime.h>
#endif

#ifndef EIGEN_MPL2_ONLY
template<class T>
using LinearSolver = Eigen::SimplicialLLT<T>;
#else
template<class T>
using LinearSolver = Eigen::SparseLU<T>;
#endif

Optimizer::Optimizer() {}

void Optimizer::optimize_orientations(Hierarchy& mRes) {
#ifdef WITH_CUDA
    optimize_orientations_cuda(mRes);
    printf("%s\n", cudaGetErrorString(cudaDeviceSynchronize()));
    cudaMemcpy(mRes.mQ[0].data(), mRes.cudaQ[0], sizeof(glm::dvec3) * mRes.mQ[0].cols(),
               cudaMemcpyDeviceToHost);

#else

    int levelIterations = 6;
    for (int level = mRes.mN.size() - 1; level >= 0; --level) {
        AdjacentMatrix& adj = mRes.mAdj[level];
        const MatrixXd& N = mRes.mN[level];
        const MatrixXd& CQ = mRes.mCQ[level];
        const VectorXd& CQw = mRes.mCQw[level];
        MatrixXd& Q = mRes.mQ[level];
        auto& phases = mRes.mPhases[level];
        for (int iter = 0; iter < levelIterations; ++iter) {
            for (int phase = 0; phase < phases.size(); ++phase) {
                auto& p = phases[phase];
#ifdef WITH_OMP
#pragma omp parallel for
#endif
                for (int pi = 0; pi < p.size(); ++pi) {
                    int i = p[pi];
                    const Vector3d n_i = N.col(i);
                    double weight_sum = 0.0f;
                    Vector3d sum = Q.col(i);
                    for (auto& link : adj[i]) {
                        const int j = link.id;
                        const double weight = link.weight;
                        if (weight == 0) continue;
                        const Vector3d n_j = N.col(j);
                        Vector3d q_j = Q.col(j);
                        std::pair<Vector3d, Vector3d> value =
                            compat_orientation_extrinsic_4(sum, n_i, q_j, n_j);
                        sum = value.first * weight_sum + value.second * weight;
                        sum -= n_i * n_i.dot(sum);
                        weight_sum += weight;
                        double norm = sum.norm();
                        if (norm > RCPOVERFLOW) sum /= norm;
                    }

                    if (CQw.size() > 0) {
                        float cw = CQw[i];
                        if (cw != 0) {
                            std::pair<Vector3d, Vector3d> value =
                                compat_orientation_extrinsic_4(sum, n_i, CQ.col(i), n_i);
                            sum = value.first * (1 - cw) + value.second * cw;
                            sum -= n_i * n_i.dot(sum);

                            float norm = sum.norm();
                            if (norm > RCPOVERFLOW) sum /= norm;
                        }
                    }

                    if (weight_sum > 0) {
                        Q.col(i) = sum;
                    }
                }
            }
        }
        if (level > 0) {
            const MatrixXd& srcField = mRes.mQ[level];
            const MatrixXi& toUpper = mRes.mToUpper[level - 1];
            MatrixXd& destField = mRes.mQ[level - 1];
            const MatrixXd& N = mRes.mN[level - 1];
#ifdef WITH_OMP
#pragma omp parallel for
#endif
            for (int i = 0; i < srcField.cols(); ++i) {
                for (int k = 0; k < 2; ++k) {
                    int dest = toUpper(k, i);
                    if (dest == -1) continue;
                    Vector3d q = srcField.col(i), n = N.col(dest);
                    destField.col(dest) = q - n * n.dot(q);
                }
            }
        }
    }

    for (int l = 0; l < mRes.mN.size() - 1; ++l) {
        const MatrixXd& N = mRes.mN[l];
        const MatrixXd& N_next = mRes.mN[l + 1];
        const MatrixXd& Q = mRes.mQ[l];
        MatrixXd& Q_next = mRes.mQ[l + 1];
        auto& toUpper = mRes.mToUpper[l];
#ifdef WITH_OMP
#pragma omp parallel for
#endif
        for (int i = 0; i < toUpper.cols(); ++i) {
            Vector2i upper = toUpper.col(i);
            Vector3d q0 = Q.col(upper[0]);
            Vector3d n0 = N.col(upper[0]);
            Vector3d q;

            if (upper[1] != -1) {
                Vector3d q1 = Q.col(upper[1]);
                Vector3d n1 = N.col(upper[1]);
                auto result = compat_orientation_extrinsic_4(q0, n0, q1, n1);
                q = result.first + result.second;
            } else {
                q = q0;
            }
            Vector3d n = N_next.col(i);
            q -= n.dot(q) * n;
            if (q.squaredNorm() > RCPOVERFLOW) q.normalize();

            Q_next.col(i) = q;
        }
    }

#endif
}

void Optimizer::optimize_scale(Hierarchy& mRes, VectorXd& rho, int adaptive) {
    const MatrixXd& N = mRes.mN[0];
    MatrixXd& Q = mRes.mQ[0];
    MatrixXd& V = mRes.mV[0];
    MatrixXd& S = mRes.mS[0];
    MatrixXd& K = mRes.mK[0];
    MatrixXi& F = mRes.mF;

    if (adaptive) {
        std::vector<Eigen::Triplet<double>> lhsTriplets;

        lhsTriplets.reserve(F.cols() * 6);
        for (int i = 0; i < V.cols(); ++i) {
            for (int j = 0; j < 2; ++j) {
                S(j, i) = 1.0;
                double sc1 = std::max(0.75 * S(j, i), rho[i] * 1.0 / mRes.mScale);
                S(j, i) = std::min(S(j, i), sc1);
            }
        }

        std::vector<std::map<int, double>> entries(V.cols() * 2);
        double lambda = 1;
        for (int i = 0; i < entries.size(); ++i) {
            entries[i][i] = lambda;
        }
        for (int i = 0; i < F.cols(); ++i) {
            for (int j = 0; j < 3; ++j) {
                int v1 = F(j, i);
                int v2 = F((j + 1) % 3, i);
                Vector3d diff = V.col(v2) - V.col(v1);
                Vector3d q_1 = Q.col(v1);
                Vector3d q_2 = Q.col(v2);
                Vector3d n_1 = N.col(v1);
                Vector3d n_2 = N.col(v2);
                Vector3d q_1_y = n_1.cross(q_1);
                auto index = compat_orientation_extrinsic_index_4(q_1, n_1, q_2, n_2);
                int v1_x = v1 * 2, v1_y = v1 * 2 + 1, v2_x = v2 * 2, v2_y = v2 * 2 + 1;

                double dx = diff.dot(q_1);
                double dy = diff.dot(q_1_y);

                double kx_g = K(0, v1);
                double ky_g = K(1, v1);

                if (index.first % 2 != index.second % 2) {
                    std::swap(v2_x, v2_y);
                }
                double scale_x = (fmin(fmax(1 + kx_g * dy, 0.3), 3));
                double scale_y = (fmin(fmax(1 + ky_g * dx, 0.3), 3));
                //                (v2_x - scale_x * v1_x)^2 = 0
                // x^2 - 2s xy + s^2 y^2
                entries[v2_x][v2_x] += 1;
                entries[v1_x][v1_x] += scale_x * scale_x;
                entries[v2_y][v2_y] += 1;
                entries[v1_y][v1_y] += scale_y * scale_y;
                auto it = entries[v1_x].find(v2_x);
                if (it == entries[v1_x].end()) {
                    entries[v1_x][v2_x] = -scale_x;
                    entries[v2_x][v1_x] = -scale_x;
                    entries[v1_y][v2_y] = -scale_y;
                    entries[v2_y][v1_y] = -scale_y;
                } else {
                    it->second -= scale_x;
                    entries[v2_x][v1_x] -= scale_x;
                    entries[v1_y][v2_y] -= scale_y;
                    entries[v2_y][v1_y] -= scale_y;
                }
            }
        }

        Eigen::SparseMatrix<double> A(V.cols() * 2, V.cols() * 2);
        VectorXd rhs(V.cols() * 2);
        rhs.setZero();
        for (int i = 0; i < entries.size(); ++i) {
            rhs(i) = lambda * S(i % 2, i / 2);
            for (auto& rec : entries[i]) {
                lhsTriplets.push_back(Eigen::Triplet<double>(i, rec.first, rec.second));
            }
        }
        A.setFromTriplets(lhsTriplets.begin(), lhsTriplets.end());
        LinearSolver<Eigen::SparseMatrix<double>> solver;
        solver.analyzePattern(A);

        solver.factorize(A);

        VectorXd result = solver.solve(rhs);

        double total_area = 0;
        for (int i = 0; i < V.cols(); ++i) {
            S(0, i) = (result(i * 2));
            S(1, i) = (result(i * 2 + 1));
            total_area += S(0, i) * S(1, i);
        }
        total_area = sqrt(V.cols() / total_area);
        for (int i = 0; i < V.cols(); ++i) {
            //            S(0, i) *= total_area;
            //            S(1, i) *= total_area;
        }
    } else {
        for (int i = 0; i < V.cols(); ++i) {
            S(0, i) = 1;
            S(1, i) = 1;
        }
    }

    for (int l = 0; l < mRes.mS.size() - 1; ++l) {
        const MatrixXd& S = mRes.mS[l];
        MatrixXd& S_next = mRes.mS[l + 1];
        auto& toUpper = mRes.mToUpper[l];
        for (int i = 0; i < toUpper.cols(); ++i) {
            Vector2i upper = toUpper.col(i);
            Vector2d q0 = S.col(upper[0]);

            if (upper[1] != -1) {
                q0 = (q0 + S.col(upper[1])) * 0.5;
            }
            S_next.col(i) = q0;
        }
    }
}

void Optimizer::optimize_positions(Hierarchy& mRes, int with_scale) {
    int levelIterations = 6;
#ifdef WITH_CUDA
    optimize_positions_cuda(mRes);
    cudaMemcpy(mRes.mO[0].data(), mRes.cudaO[0], sizeof(glm::dvec3) * mRes.mO[0].cols(),
               cudaMemcpyDeviceToHost);
#else
    for (int level = mRes.mAdj.size() - 1; level >= 0; --level) {
        for (int iter = 0; iter < levelIterations; ++iter) {
            AdjacentMatrix& adj = mRes.mAdj[level];
            const MatrixXd &N = mRes.mN[level], &Q = mRes.mQ[level], &V = mRes.mV[level];
            const MatrixXd& CQ = mRes.mCQ[level];
            const MatrixXd& CO = mRes.mCO[level];
            const VectorXd& COw = mRes.mCOw[level];
            MatrixXd& O = mRes.mO[level];
            MatrixXd& S = mRes.mS[level];
            auto& phases = mRes.mPhases[level];
            for (int phase = 0; phase < phases.size(); ++phase) {
                auto& p = phases[phase];
#ifdef WITH_OMP
#pragma omp parallel for
#endif
                for (int pi = 0; pi < p.size(); ++pi) {
                    int i = p[pi];
                    double scale_x = mRes.mScale;
                    double scale_y = mRes.mScale;
                    if (with_scale) {
                        scale_x *= S(0, i);
                        scale_y *= S(1, i);
                    }
                    double inv_scale_x = 1.0f / scale_x;
                    double inv_scale_y = 1.0f / scale_y;
                    const Vector3d n_i = N.col(i), v_i = V.col(i);
                    Vector3d q_i = Q.col(i);

                    Vector3d sum = O.col(i);
                    double weight_sum = 0.0f;

                    q_i.normalize();
                    for (auto& link : adj[i]) {
                        const int j = link.id;
                        const double weight = link.weight;
                        if (weight == 0) continue;
                        double scale_x_1 = mRes.mScale;
                        double scale_y_1 = mRes.mScale;
                        if (with_scale) {
                            scale_x_1 *= S(0, j);
                            scale_y_1 *= S(1, j);
                        }
                        double inv_scale_x_1 = 1.0f / scale_x_1;
                        double inv_scale_y_1 = 1.0f / scale_y_1;

                        const Vector3d n_j = N.col(j), v_j = V.col(j);
                        Vector3d q_j = Q.col(j), o_j = O.col(j);

                        q_j.normalize();

                        std::pair<Vector3d, Vector3d> value = compat_position_extrinsic_4(
                            v_i, n_i, q_i, sum, v_j, n_j, q_j, o_j, scale_x, scale_y, inv_scale_x,
                            inv_scale_y, scale_x_1, scale_y_1, inv_scale_x_1, inv_scale_y_1);

                        sum = value.first * weight_sum + value.second * weight;
                        weight_sum += weight;
                        if (weight_sum > RCPOVERFLOW) sum /= weight_sum;
                        sum -= n_i.dot(sum - v_i) * n_i;
                    }

                    if (COw.size() > 0) {
                        float cw = COw[i];
                        if (cw != 0) {
                            Vector3d co = CO.col(i), cq = CQ.col(i);
                            Vector3d d = co - sum;
                            d -= cq.dot(d) * cq;
                            sum += cw * d;
                            sum -= n_i.dot(sum - v_i) * n_i;
                        }
                    }

                    if (weight_sum > 0) {
                        O.col(i) = position_round_4(sum, q_i, n_i, v_i, scale_x, scale_y,
                                                    inv_scale_x, inv_scale_y);
                    }
                }
            }
        }
        if (level > 0) {
            const MatrixXd& srcField = mRes.mO[level];
            const MatrixXi& toUpper = mRes.mToUpper[level - 1];
            MatrixXd& destField = mRes.mO[level - 1];
            const MatrixXd& N = mRes.mN[level - 1];
            const MatrixXd& V = mRes.mV[level - 1];
#ifdef WITH_OMP
#pragma omp parallel for
#endif
            for (int i = 0; i < srcField.cols(); ++i) {
                for (int k = 0; k < 2; ++k) {
                    int dest = toUpper(k, i);
                    if (dest == -1) continue;
                    Vector3d o = srcField.col(i), n = N.col(dest), v = V.col(dest);
                    o -= n * n.dot(o - v);
                    destField.col(dest) = o;
                }
            }
        }
    }
#endif
}

void Optimizer::optimize_positions_dynamic(
    MatrixXi& F, MatrixXd& V, MatrixXd& N, MatrixXd& Q, std::vector<std::vector<int>>& Vset,
    std::vector<Vector3d>& O_compact, std::vector<Vector4i>& F_compact,
    std::vector<int>& V2E_compact, std::vector<int>& E2E_compact, double mScale,
    std::vector<Vector3d>& diffs, std::vector<int>& diff_count,
    std::map<std::pair<int, int>, int>& o2e, std::vector<int>& sharp_o,
    std::map<int, std::pair<Vector3d, Vector3d>>& compact_sharp_constraints, int with_scale) {
    std::set<int> uncertain;
    for (auto& info : o2e) {
        if (diff_count[info.second] == 0) {
            uncertain.insert(info.first.first);
            uncertain.insert(info.first.second);
        }
    }
    std::vector<int> Vind(O_compact.size(), -1);
    std::vector<std::list<int>> links(O_compact.size());
    std::vector<std::list<int>> dedges(O_compact.size());
    std::vector<std::vector<int>> adj(V.cols());
    for (int i = 0; i < F.cols(); ++i) {
        for (int j = 0; j < 3; ++j) {
            int v1 = F(j, i);
            int v2 = F((j + 1) % 3, i);
            adj[v1].push_back(v2);
        }
    }
    auto FindNearest = [&]() {
        for (int i = 0; i < O_compact.size(); ++i) {
            if (Vind[i] == -1) {
                double min_dis = 1e30;
                int min_ind = -1;
                for (auto v : Vset[i]) {
                    double dis = (V.col(v) - O_compact[i]).squaredNorm();
                    if (dis < min_dis) {
                        min_dis = dis;
                        min_ind = v;
                    }
                }
                if (min_ind > -1) {
                    Vind[i] = min_ind;
                    double x = (O_compact[i] - V.col(min_ind)).dot(N.col(min_ind));
                    O_compact[i] -= x * N.col(min_ind);
                }
            } else {
                int current_v = Vind[i];
                Vector3d n = N.col(current_v);
                double current_dis = (O_compact[i] - V.col(current_v)).squaredNorm();
                while (true) {
                    int next_v = -1;
                    for (auto& v : adj[current_v]) {
                        if (N.col(v).dot(n) < cos(10.0 / 180.0 * 3.141592654)) continue;
                        double dis = (O_compact[i] - V.col(v)).squaredNorm();
                        if (dis < current_dis) {
                            current_dis = dis;
                            next_v = v;
                        }
                    }
                    if (next_v == -1) break;
                    // rotate ideal distance
                    Vector3d n1 = N.col(current_v);
                    Vector3d n2 = N.col(next_v);
                    Vector3d axis = n1.cross(n2);
                    double len = axis.norm();
                    double angle = atan2(len, n1.dot(n2));
                    axis.normalized();
                    Matrix3d m = AngleAxisd(angle, axis).toRotationMatrix();
                    for (auto e : dedges[i]) {
                        Vector3d& d = diffs[e];
                        d = m * d;
                    }
                    current_v = next_v;
                }
                Vind[i] = current_v;
            }
        }
    };

    auto BuildConnection = [&]() {
        for (int i = 0; i < links.size(); ++i) {
            int deid0 = V2E_compact[i];
            if (deid0 != -1) {
                std::list<int>& connection = links[i];
                std::list<int>& dedge = dedges[i];
                int deid = deid0;
                do {
                    connection.push_back(F_compact[deid / 4][(deid + 1) % 4]);
                    dedge.push_back(deid);
                    deid = E2E_compact[deid / 4 * 4 + (deid + 3) % 4];
                } while (deid != -1 && deid != deid0);
                if (deid == -1) {
                    deid = deid0;
                    do {
                        deid = E2E_compact[deid];
                        if (deid == -1) break;
                        deid = deid / 4 * 4 + (deid + 1) % 4;
                        connection.push_front(F_compact[deid / 4][(deid + 1) % 4]);
                        dedge.push_front(deid);
                    } while (true);
                }
            }
        }
    };

    std::vector<Vector3d> lines;
    auto ComputeDistance = [&]() {
        std::set<int> unobserved;
        for (auto& info : o2e) {
            if (diff_count[info.second] == 0) {
                unobserved.insert(info.first.first);
            }
        }
        while (true) {
            bool update = false;
            std::set<int> observed;
            for (auto& p : unobserved) {
                std::vector<int> observations, edges;
                int count = 0;
                for (auto& e : dedges[p]) {
                    edges.push_back(e);
                    if (diff_count[e]) {
                        count += 1;
                        observations.push_back(1);
                    } else {
                        observations.push_back(0);
                    }
                }
                if (count <= 1) continue;
                update = true;
                observed.insert(p);
                for (int i = 0; i < observations.size(); ++i) {
                    if (observations[i] == 1) continue;
                    int j = i;
                    std::list<int> interp;
                    while (observations[j] == 0) {
                        interp.push_front(j);
                        j -= 1;
                        if (j < 0) j = edges.size() - 1;
                    }
                    j = (i + 1) % edges.size();
                    while (observations[j] == 0) {
                        interp.push_back(j);
                        j += 1;
                        if (j == edges.size()) j = 0;
                    }
                    Vector3d dl = diffs[edges[(interp.front() + edges.size() - 1) % edges.size()]];
                    double lenl = dl.norm();
                    Vector3d dr = diffs[edges[(interp.back() + 1) % edges.size()]];
                    double lenr = dr.norm();
                    dl /= lenl;
                    dr /= lenr;
                    Vector3d n = dl.cross(dr).normalized();
                    double angle = atan2(dl.cross(dr).norm(), dl.dot(dr));
                    if (angle < 0) angle += 2 * 3.141592654;
                    Vector3d nc = N.col(Vind[p]);
                    if (n.dot(nc) < 0) {
                        n = -n;
                        angle = 2 * 3.141592654 - angle;
                    }
                    double step = (lenr - lenl) / (interp.size() + 1);
                    angle /= interp.size() + 1;
                    Vector3d dlp = nc.cross(dl).normalized();
                    int t = 0;
                    for (auto q : interp) {
                        t += 1;
                        observations[q] = 1;
                        double ad = angle * t;
                        int e = edges[q];
                        int re = E2E_compact[e];
                        diff_count[e] = 2;
                        diffs[e] = (cos(ad) * dl + sin(ad) * dlp) * (lenl + step * t);
                        if (re != -1) {
                            diff_count[re] = 2;
                            diffs[re] = -diffs[e];
                        }
                    }
                    for (int i = 0; i < edges.size(); ++i) {
                        lines.push_back(O_compact[p]);
                        lines.push_back(O_compact[p] + diffs[edges[i]]);
                    }
                }
            }
            if (!update) break;
            for (auto& p : observed) unobserved.erase(p);
        }
    };

    BuildConnection();
    int max_iter = 10;
    for (int iter = 0; iter < max_iter; ++iter) {
        FindNearest();
        ComputeDistance();

        std::vector<std::unordered_map<int, double>> entries(O_compact.size() * 2);
        std::vector<int> fixed_dim(O_compact.size() * 2, 0);
        for (auto& info : compact_sharp_constraints) {
            fixed_dim[info.first * 2 + 1] = 1;
            if (info.second.second.norm() < 0.5) fixed_dim[info.first * 2] = 1;
        }
        std::vector<double> b(O_compact.size() * 2);
        std::vector<double> x(O_compact.size() * 2);
        std::vector<Vector3d> Q_compact(O_compact.size());
        std::vector<Vector3d> N_compact(O_compact.size());
        std::vector<Vector3d> V_compact(O_compact.size());
#ifdef WITH_OMP
#pragma omp parallel for
#endif
        for (int i = 0; i < O_compact.size(); ++i) {
            Q_compact[i] = Q.col(Vind[i]);
            N_compact[i] = N.col(Vind[i]);
            V_compact[i] = V.col(Vind[i]);
            if (fixed_dim[i * 2 + 1] && !fixed_dim[i * 2]) {
                Q_compact[i] = compact_sharp_constraints[i].second;
                V_compact[i] = compact_sharp_constraints[i].first;
            }
        }
        for (int i = 0; i < O_compact.size(); ++i) {
            Vector3d q = Q_compact[i];
            Vector3d n = N_compact[i];
            Vector3d q_y = n.cross(q);
            auto Vi = V_compact[i];
            x[i * 2] = (O_compact[i] - Vi).dot(q);
            x[i * 2 + 1] = (O_compact[i] - Vi).dot(q_y);
        }
        for (int i = 0; i < O_compact.size(); ++i) {
            Vector3d qx = Q_compact[i];
            Vector3d qy = N_compact[i];
            qy = qy.cross(qx);
            auto dedge_it = dedges[i].begin();
            for (auto it = links[i].begin(); it != links[i].end(); ++it, ++dedge_it) {
                int j = *it;
                Vector3d qx2 = Q_compact[j];
                Vector3d qy2 = N_compact[j];
                qy2 = qy2.cross(qx2);

                int de = o2e[std::make_pair(i, j)];
                double lambda = (diff_count[de] == 1) ? 1 : 1;
                Vector3d target_offset = diffs[de];

                auto Vi = V_compact[i];
                auto Vj = V_compact[j];

                Vector3d offset = Vj - Vi;

                //                target_offset.normalize();
                //                target_offset *= mScale;
                Vector3d C = target_offset - offset;
                int vid[] = {j * 2, j * 2 + 1, i * 2, i * 2 + 1};
                Vector3d weights[] = {qx2, qy2, -qx, -qy};
                for (int ii = 0; ii < 4; ++ii) {
                    for (int jj = 0; jj < 4; ++jj) {
                        auto it = entries[vid[ii]].find(vid[jj]);
                        if (it == entries[vid[ii]].end()) {
                            entries[vid[ii]][vid[jj]] = lambda * weights[ii].dot(weights[jj]);
                        } else {
                            entries[vid[ii]][vid[jj]] += lambda * weights[ii].dot(weights[jj]);
                        }
                    }
                    b[vid[ii]] += lambda * weights[ii].dot(C);
                }
            }
        }

        // fix sharp edges
        for (int i = 0; i < entries.size(); ++i) {
            if (entries[i].size() == 0) {
                entries[i][i] = 1;
                b[i] = x[i];
            }
            if (fixed_dim[i]) {
                b[i] = x[i];
                entries[i].clear();
                entries[i][i] = 1;
            } else {
                std::unordered_map<int, double> newmap;
                for (auto& rec : entries[i]) {
                    if (fixed_dim[rec.first]) {
                        b[i] -= rec.second * x[rec.first];
                    } else {
                        newmap[rec.first] = rec.second;
                    }
                }
                std::swap(entries[i], newmap);
            }
        }
        std::vector<Eigen::Triplet<double>> lhsTriplets;
        lhsTriplets.reserve(F_compact.size() * 8);
        Eigen::SparseMatrix<double> A(O_compact.size() * 2, O_compact.size() * 2);
        VectorXd rhs(O_compact.size() * 2);
        rhs.setZero();
        for (int i = 0; i < entries.size(); ++i) {
            rhs(i) = b[i];
            for (auto& rec : entries[i]) {
                lhsTriplets.push_back(Eigen::Triplet<double>(i, rec.first, rec.second));
            }
        }

        A.setFromTriplets(lhsTriplets.begin(), lhsTriplets.end());

#ifdef LOG_OUTPUT
        int t1 = GetCurrentTime64();
#endif

        // FIXME: IncompleteCholesky Preconditioner will fail here so I fallback to Diagonal one.
        // I suspected either there is a implementation bug in IncompleteCholesky Preconditioner
        // or there is a memory corruption somewhere.  However, g++'s address sanitizer does not
        // report anything useful.
        LinearSolver<Eigen::SparseMatrix<double>> solver;
        solver.analyzePattern(A);
        solver.factorize(A);
        //        Eigen::setNbThreads(1);
        //        ConjugateGradient<SparseMatrix<double>, Lower | Upper> solver;
        //        VectorXd x0 = VectorXd::Map(x.data(), x.size());
        //        solver.setMaxIterations(40);

        //        solver.compute(A);
        VectorXd x_new = solver.solve(rhs);  // solver.solveWithGuess(rhs, x0);

#ifdef LOG_OUTPUT
        // std::cout << "[LSQ] n_iteration:" << solver.iterations() << std::endl;
        // std::cout << "[LSQ] estimated error:" << solver.error() << std::endl;
        int t2 = GetCurrentTime64();
        printf("[LSQ] Linear solver uses %lf seconds.\n", (t2 - t1) * 1e-3);
#endif
        for (int i = 0; i < O_compact.size(); ++i) {
            // Vector3d q = Q.col(Vind[i]);
            Vector3d q = Q_compact[i];
            // Vector3d n = N.col(Vind[i]);
            Vector3d n = N_compact[i];
            Vector3d q_y = n.cross(q);
            auto Vi = V_compact[i];
            O_compact[i] = Vi + q * x_new[i * 2] + q_y * x_new[i * 2 + 1];
        }

        // forgive my hack...
        if (iter + 1 == max_iter) {
            for (int iter = 0; iter < 5; ++iter) {
                for (int i = 0; i < O_compact.size(); ++i) {
                    if (sharp_o[i]) continue;
                    if (dedges[i].size() != 4 || uncertain.count(i)) {
                        Vector3d n(0, 0, 0), v(0, 0, 0);
                        Vector3d v0 = O_compact[i];
                        for (auto e : dedges[i]) {
                            Vector3d v1 = O_compact[F_compact[e / 4][(e + 1) % 4]];
                            Vector3d v2 = O_compact[F_compact[e / 4][(e + 3) % 4]];
                            n += (v1 - v0).cross(v2 - v0);
                            v += v1;
                        }
                        n.normalize();
                        Vector3d offset = v / dedges[i].size() - v0;
                        offset -= offset.dot(n) * n;
                        O_compact[i] += offset;
                    }
                }
            }
        }
    }
}

void Optimizer::optimize_positions_sharp(
    Hierarchy& mRes, std::vector<DEdge>& edge_values, std::vector<Vector2i>& edge_diff,
    std::vector<int>& sharp_edges, std::set<int>& sharp_vertices,
    std::map<int, std::pair<Vector3d, Vector3d>>& sharp_constraints, int with_scale) {
    auto& V = mRes.mV[0];
    auto& F = mRes.mF;
    auto& Q = mRes.mQ[0];
    auto& N = mRes.mN[0];
    auto& O = mRes.mO[0];
    auto& S = mRes.mS[0];

    DisajointTree tree(V.cols());
    for (int i = 0; i < edge_diff.size(); ++i) {
        if (edge_diff[i].array().abs().sum() == 0) {
            tree.Merge(edge_values[i].x, edge_values[i].y);
        }
    }
    tree.BuildCompactParent();
    std::map<int, int> compact_sharp_indices;
    std::set<DEdge> compact_sharp_edges;
    for (int i = 0; i < sharp_edges.size(); ++i) {
        if (sharp_edges[i] == 1) {
            int v1 = tree.Index(F(i % 3, i / 3));
            int v2 = tree.Index(F((i + 1) % 3, i / 3));
            compact_sharp_edges.insert(DEdge(v1, v2));
        }
    }
    for (auto& v : sharp_vertices) {
        int p = tree.Index(v);
        if (compact_sharp_indices.count(p) == 0) {
            int s = compact_sharp_indices.size();
            compact_sharp_indices[p] = s;
        }
    }
    std::map<int, std::set<int>> sharp_vertices_links;
    std::set<DEdge> sharp_dedges;
    for (int i = 0; i < sharp_edges.size(); ++i) {
        if (sharp_edges[i]) {
            int v1 = F(i % 3, i / 3);
            int v2 = F((i + 1) % 3, i / 3);
            if (sharp_vertices_links.count(v1) == 0) sharp_vertices_links[v1] = std::set<int>();
            sharp_vertices_links[v1].insert(v2);
            sharp_dedges.insert(DEdge(v1, v2));
        }
    }
    std::vector<std::vector<int>> sharp_to_original_indices(compact_sharp_indices.size());
    for (auto& v : sharp_vertices_links) {
        if (v.second.size() == 2) continue;
        int p = tree.Index(v.first);
        sharp_to_original_indices[compact_sharp_indices[p]].push_back(v.first);
    }
    for (auto& v : sharp_vertices_links) {
        if (v.second.size() != 2) continue;
        int p = tree.Index(v.first);
        sharp_to_original_indices[compact_sharp_indices[p]].push_back(v.first);
    }

    for (int i = 0; i < V.cols(); ++i) {
        if (sharp_vertices.count(i)) continue;
        int p = tree.Index(i);
        if (compact_sharp_indices.count(p))
            sharp_to_original_indices[compact_sharp_indices[p]].push_back(i);
    }

    int num = sharp_to_original_indices.size();
    std::vector<std::set<int>> links(sharp_to_original_indices.size());
    for (int e = 0; e < edge_diff.size(); ++e) {
        int v1 = edge_values[e].x;
        int v2 = edge_values[e].y;
        int p1 = tree.Index(v1);
        int p2 = tree.Index(v2);
        if (p1 == p2 || compact_sharp_edges.count(DEdge(p1, p2)) == 0) continue;
        p1 = compact_sharp_indices[p1];
        p2 = compact_sharp_indices[p2];

        links[p1].insert(p2);
        links[p2].insert(p1);
    }

    std::vector<int> hash(links.size(), 0);
    std::vector<std::vector<Vector3d>> loops;
    for (int i = 0; i < num; ++i) {
        if (hash[i] == 1) continue;
        if (links[i].size() == 2) {
            std::vector<int> q;
            q.push_back(i);
            hash[i] = 1;
            int v = i;
            int prev_v = -1;
            bool is_loop = false;
            while (links[v].size() == 2) {
                int next_v = -1;
                for (auto nv : links[v])
                    if (nv != prev_v) next_v = nv;
                if (hash[next_v]) {
                    is_loop = true;
                    break;
                }
                if (links[next_v].size() == 2) hash[next_v] = true;
                q.push_back(next_v);
                prev_v = v;
                v = next_v;
            }
            if (!is_loop && q.size() >= 2) {
                std::vector<int> q1;
                int v = i;
                int prev_v = q[1];
                while (links[v].size() == 2) {
                    int next_v = -1;
                    for (auto nv : links[v])
                        if (nv != prev_v) next_v = nv;
                    if (hash[next_v]) {
                        is_loop = true;
                        break;
                    }
                    if (links[next_v].size() == 2) hash[next_v] = true;
                    q1.push_back(next_v);
                    prev_v = v;
                    v = next_v;
                }
                std::reverse(q1.begin(), q1.end());
                q1.insert(q1.end(), q.begin(), q.end());
                std::swap(q1, q);
            }
            if (q.size() < 3) continue;
            if (is_loop) q.push_back(q.front());
            double len = 0, scale = 0;
            std::vector<Vector3d> o(q.size()), new_o(q.size());
            std::vector<double> sc(q.size());

            for (int i = 0; i < q.size() - 1; ++i) {
                int v1 = q[i];
                int v2 = q[i + 1];
                auto it = links[v1].find(v2);
                if (it == links[v1].end()) {
                    printf("Non exist!\n");
                    exit(0);
                }
            }

            for (int i = 0; i < q.size(); ++i) {
                if (sharp_to_original_indices[q[i]].size() == 0) {
                  continue;
                }
                o[i] = O.col(sharp_to_original_indices[q[i]][0]);
                Vector3d qx = Q.col(sharp_to_original_indices[q[i]][0]);
                Vector3d qy = Vector3d(N.col(sharp_to_original_indices[q[i]][0])).cross(qx);
                int fst = sharp_to_original_indices[q[1]][0];
                Vector3d dis = (i == 0) ? (Vector3d(O.col(fst)) - o[i]) : o[i] - o[i - 1];
                if (with_scale)
                    sc[i] = (abs(qx.dot(dis)) > abs(qy.dot(dis)))
                                ? S(0, sharp_to_original_indices[q[i]][0])
                                : S(1, sharp_to_original_indices[q[i]][0]);
                else
                    sc[i] = 1;
                new_o[i] = o[i];
            }

            if (is_loop) {
                for (int i = 0; i < q.size(); ++i) {
                    Vector3d dir =
                        (o[(i + 1) % q.size()] - o[(i + q.size() - 1) % q.size()]).normalized();
                    for (auto& ind : sharp_to_original_indices[q[i]]) {
                        sharp_constraints[ind] = std::make_pair(o[i], dir);
                    }
                }
            } else {
                for (int i = 0; i < q.size(); ++i) {
                    Vector3d dir(0, 0, 0);
                    if (i != 0 && i + 1 != q.size())
                        dir = (o[i + 1] - o[i - 1]).normalized();
                    else if (links[q[i]].size() == 1) {
                        if (i == 0)
                            dir = (o[i + 1] - o[i]).normalized();
                        else
                            dir = (o[i] - o[i - 1]).normalized();
                    }
                    for (auto& ind : sharp_to_original_indices[q[i]]) {
                        sharp_constraints[ind] = std::make_pair(o[i], dir);
                    }
                }
            }

            for (int i = 0; i < q.size() - 1; ++i) {
                len += (o[i + 1] - o[i]).norm();
                scale += sc[i];
            }

            int next_m = q.size() - 1;

            double left_norm = len * sc[0] / scale;
            int current_v = 0;
            double current_norm = (o[1] - o[0]).norm();
            for (int i = 1; i < next_m; ++i) {
                while (left_norm >= current_norm) {
                    left_norm -= current_norm;
                    current_v += 1;
                    current_norm = (o[current_v + 1] - o[current_v]).norm();
                }
                new_o[i] =
                    (o[current_v + 1] * left_norm + o[current_v] * (current_norm - left_norm)) /
                    current_norm;
                o[current_v] = new_o[i];
                current_norm -= left_norm;
                left_norm = len * sc[current_v] / scale;
            }

            for (int i = 0; i < q.size(); ++i) {
                for (auto v : sharp_to_original_indices[q[i]]) {
                    O.col(v) = new_o[i];
                }
            }

            loops.push_back(new_o);
        }
    }
    return;
    std::ofstream os("/Users/jingwei/Desktop/sharp.obj");
    for (int i = 0; i < loops.size(); ++i) {
        for (auto& v : loops[i]) {
            os << "v " << v[0] << " " << v[1] << " " << v[2] << "\n";
        }
    }
    int offset = 1;
    for (int i = 0; i < loops.size(); ++i) {
        for (int j = 0; j < loops[i].size() - 1; ++j) {
            os << "l " << offset + j << " " << offset + j + 1 << "\n";
        }
        offset += loops[i].size();
    }
    os.close();
    exit(0);
}

void Optimizer::optimize_positions_fixed(
    Hierarchy& mRes, std::vector<DEdge>& edge_values, std::vector<Vector2i>& edge_diff,
    std::set<int>& sharp_vertices, std::map<int, std::pair<Vector3d, Vector3d>>& sharp_constraints,
    int with_scale) {
    auto& V = mRes.mV[0];
    auto& F = mRes.mF;
    auto& Q = mRes.mQ[0];
    auto& N = mRes.mN[0];
    auto& O = mRes.mO[0];
    auto& S = mRes.mS[0];

    DisajointTree tree(V.cols());
    for (int i = 0; i < edge_diff.size(); ++i) {
        if (edge_diff[i].array().abs().sum() == 0) {
            tree.Merge(edge_values[i].x, edge_values[i].y);
        }
    }
    tree.BuildCompactParent();
    int num = tree.CompactNum();

    // Find the most descriptive vertex
    std::vector<Vector3d> v_positions(num, Vector3d(0, 0, 0));
    std::vector<int> v_count(num);
    std::vector<double> v_distance(num, 1e30);
    std::vector<int> v_index(num, -1);

    for (int i = 0; i < V.cols(); ++i) {
        v_positions[tree.Index(i)] += O.col(i);
        v_count[tree.Index(i)] += 1;
    }
    for (int i = 0; i < num; ++i) {
        if (v_count[i] > 0) v_positions[i] /= v_count[i];
    }
    for (int i = 0; i < V.cols(); ++i) {
        int p = tree.Index(i);
        double dis = (v_positions[p] - V.col(i)).squaredNorm();
        if (dis < v_distance[p]) {
            v_distance[p] = dis;
            v_index[p] = i;
        }
    }

    std::set<int> compact_sharp_vertices;
    for (auto& v : sharp_vertices) {
        v_positions[tree.Index(v)] = O.col(v);
        v_index[tree.Index(v)] = v;
        V.col(v) = O.col(v);
        compact_sharp_vertices.insert(tree.Index(v));
    }
    std::vector<std::map<int, std::pair<int, Vector3d>>> ideal_distances(tree.CompactNum());
    for (int e = 0; e < edge_diff.size(); ++e) {
        int v1 = edge_values[e].x;
        int v2 = edge_values[e].y;

        int p1 = tree.Index(v1);
        int p2 = tree.Index(v2);
        int q1 = v_index[p1];
        int q2 = v_index[p2];

        Vector3d q_1 = Q.col(v1);
        Vector3d q_2 = Q.col(v2);

        Vector3d n_1 = N.col(v1);
        Vector3d n_2 = N.col(v2);
        Vector3d q_1_y = n_1.cross(q_1);
        Vector3d q_2_y = n_2.cross(q_2);
        auto index = compat_orientation_extrinsic_index_4(q_1, n_1, q_2, n_2);
        double s_x1 = S(0, v1), s_y1 = S(1, v1);
        double s_x2 = S(0, v2), s_y2 = S(1, v2);
        int rank_diff = (index.second + 4 - index.first) % 4;
        if (rank_diff % 2 == 1) std::swap(s_x2, s_y2);
        Vector3d qd_x = 0.5 * (rotate90_by(q_2, n_2, rank_diff) + q_1);
        Vector3d qd_y = 0.5 * (rotate90_by(q_2_y, n_2, rank_diff) + q_1_y);
        double scale_x = (with_scale ? 0.5 * (s_x1 + s_x2) : 1) * mRes.mScale;
        double scale_y = (with_scale ? 0.5 * (s_y1 + s_y2) : 1) * mRes.mScale;
        Vector2i diff = edge_diff[e];

        Vector3d origin1 =
            /*(sharp_constraints.count(q1)) ? sharp_constraints[q1].first : */ V.col(q1);
        Vector3d origin2 =
            /*(sharp_constraints.count(q2)) ? sharp_constraints[q2].first : */ V.col(q2);
        Vector3d C = diff[0] * scale_x * qd_x + diff[1] * scale_y * qd_y + origin1 - origin2;
        auto it = ideal_distances[p1].find(p2);
        if (it == ideal_distances[p1].end()) {
            ideal_distances[p1][p2] = std::make_pair(1, C);
        } else {
            it->second.first += 1;
            it->second.second += C;
        }
    }

    std::vector<std::unordered_map<int, double>> entries(num * 2);
    std::vector<double> b(num * 2);

    for (int m = 0; m < num; ++m) {
        int v1 = v_index[m];
        for (auto& info : ideal_distances[m]) {
            int v2 = v_index[info.first];
            Vector3d q_1 = Q.col(v1);
            Vector3d q_2 = Q.col(v2);
            if (sharp_constraints.count(v1)) {
                Vector3d d = sharp_constraints[v1].second;
                if (d != Vector3d::Zero()) q_1 = d;
            }
            if (sharp_constraints.count(v2)) {
                Vector3d d = sharp_constraints[v2].second;
                if (d != Vector3d::Zero()) q_2 = d;
            }

            Vector3d n_1 = N.col(v1);
            Vector3d n_2 = N.col(v2);
            Vector3d q_1_y = n_1.cross(q_1);
            Vector3d q_2_y = n_2.cross(q_2);
            Vector3d weights[] = {q_2, q_2_y, -q_1, -q_1_y};
            int vid[] = {info.first * 2, info.first * 2 + 1, m * 2, m * 2 + 1};
            Vector3d dis = info.second.second / info.second.first;
            double lambda = 1;
            if (sharp_vertices.count(v1) && sharp_vertices.count(v2)) lambda = 1;
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    auto it = entries[vid[i]].find(vid[j]);
                    if (it == entries[vid[i]].end()) {
                        entries[vid[i]][vid[j]] = weights[i].dot(weights[j]) * lambda;
                    } else {
                        entries[vid[i]][vid[j]] += weights[i].dot(weights[j]) * lambda;
                    }
                }
                b[vid[i]] += weights[i].dot(dis) * lambda;
            }
        }
    }

    std::vector<int> fixed_dim(num * 2, 0);
    std::vector<double> x(num * 2);
#ifdef WITH_OMP
#pragma omp parallel for
#endif
    for (int i = 0; i < num; ++i) {
        int p = v_index[i];
        Vector3d q = Q.col(p);

        if (sharp_constraints.count(p)) {
            Vector3d dir = sharp_constraints[p].second;
            fixed_dim[i * 2 + 1] = 1;
            if (dir != Vector3d::Zero()) {
                q = dir;
            } else
                fixed_dim[i * 2] = 1;
        }
        Vector3d n = N.col(p);
        Vector3d q_y = n.cross(q);
        x[i * 2] = (v_positions[i] - V.col(p)).dot(q);
        x[i * 2 + 1] = (v_positions[i] - V.col(p)).dot(q_y);
    }

    // fix sharp edges
    for (int i = 0; i < entries.size(); ++i) {
        if (fixed_dim[i]) {
            b[i] = x[i];
            entries[i].clear();
            entries[i][i] = 1;
        } else {
            std::unordered_map<int, double> newmap;
            for (auto& rec : entries[i]) {
                if (fixed_dim[rec.first]) {
                    b[i] -= rec.second * x[rec.first];
                } else {
                    newmap[rec.first] = rec.second;
                }
            }
            std::swap(entries[i], newmap);
        }
    }
    for (int i = 0; i < entries.size(); ++i) {
        if (entries[i].size() == 0) {
            entries[i][i] = 1;
        }
    }

    std::vector<Eigen::Triplet<double>> lhsTriplets;
    lhsTriplets.reserve(F.cols() * 6);
    Eigen::SparseMatrix<double> A(num * 2, num * 2);
    VectorXd rhs(num * 2);
    rhs.setZero();
    for (int i = 0; i < entries.size(); ++i) {
        rhs(i) = b[i];
        if (std::isnan(b[i])) {
            printf("Equation has nan!\n");
            exit(0);
        }
        for (auto& rec : entries[i]) {
            lhsTriplets.push_back(Eigen::Triplet<double>(i, rec.first, rec.second));
            if (std::isnan(rec.second)) {
                printf("Equation has nan!\n");
                exit(0);
            }
        }
    }
    A.setFromTriplets(lhsTriplets.begin(), lhsTriplets.end());

#ifdef LOG_OUTPUT
    int t1 = GetCurrentTime64();
#endif
    /*
        Eigen::setNbThreads(1);
        ConjugateGradient<SparseMatrix<double>, Lower | Upper> solver;
        VectorXd x0 = VectorXd::Map(x.data(), x.size());
        solver.setMaxIterations(40);

        solver.compute(A);
     */
    LinearSolver<Eigen::SparseMatrix<double>> solver;
    solver.analyzePattern(A);
    solver.factorize(A);

    VectorXd x_new = solver.solve(rhs);
#ifdef LOG_OUTPUT
    // std::cout << "[LSQ] n_iteration:" << solver.iterations() << std::endl;
    // std::cout << "[LSQ] estimated error:" << solver.error() << std::endl;
    int t2 = GetCurrentTime64();
    printf("[LSQ] Linear solver uses %lf seconds.\n", (t2 - t1) * 1e-3);
#endif

    for (int i = 0; i < x.size(); ++i) {
        if (!std::isnan(x_new[i])) {
            if (!fixed_dim[i / 2 * 2 + 1]) {
                double total = 0;
                for (auto& res : entries[i]) {
                    double t = x_new[res.first];
                    if (std::isnan(t)) t = 0;
                    total += t * res.second;
                }
            }
            x[i] = x_new[i];
        }
    }

    for (int i = 0; i < O.cols(); ++i) {
        int p = tree.Index(i);
        int c = v_index[p];
        Vector3d q = Q.col(c);
        if (fixed_dim[p * 2 + 1]) {
            Vector3d dir = sharp_constraints[c].second;
            if (dir != Vector3d::Zero()) q = dir;
        }
        Vector3d n = N.col(c);
        Vector3d q_y = n.cross(q);
        O.col(i) = V.col(c) + q * x[p * 2] + q_y * x[p * 2 + 1];
    }
}

void Optimizer::optimize_integer_constraints(Hierarchy& mRes, std::map<int, int>& singularities,
                                             bool use_minimum_cost_flow) {
    int edge_capacity = 2;
    bool fullFlow = false;
    std::vector<std::vector<int>>& AllowChange = mRes.mAllowChanges;
    for (int level = mRes.mToUpperEdges.size(); level >= 0; --level) {
        auto& EdgeDiff = mRes.mEdgeDiff[level];
        auto& FQ = mRes.mFQ[level];
        auto& F2E = mRes.mF2E[level];
        auto& E2F = mRes.mE2F[level];

        int iter = 0;
        while (!fullFlow) {
            std::vector<Vector4i> edge_to_constraints(E2F.size() * 2, Vector4i(-1, 0, -1, 0));
            std::vector<int> initial(F2E.size() * 2, 0);
            for (int i = 0; i < F2E.size(); ++i) {
                for (int j = 0; j < 3; ++j) {
                    int e = F2E[i][j];
                    Vector2i index = rshift90(Vector2i(e * 2 + 1, e * 2 + 2), FQ[i][j]);
                    for (int k = 0; k < 2; ++k) {
                        int l = abs(index[k]);
                        int s = index[k] / l;
                        int ind = l - 1;
                        int equationID = i * 2 + k;
                        if (edge_to_constraints[ind][0] == -1) {
                            edge_to_constraints[ind][0] = equationID;
                            edge_to_constraints[ind][1] = s;
                        } else {
                            edge_to_constraints[ind][2] = equationID;
                            edge_to_constraints[ind][3] = s;
                        }
                        initial[equationID] += s * EdgeDiff[ind / 2][ind % 2];
                    }
                }
            }
            std::vector<std::pair<Vector2i, int>> arcs;
            std::vector<int> arc_ids;
            for (int i = 0; i < edge_to_constraints.size(); ++i) {
                if (AllowChange[level][i] == 0) continue;
                if (edge_to_constraints[i][0] == -1 || edge_to_constraints[i][2] == -1) continue;
                if (edge_to_constraints[i][1] == -edge_to_constraints[i][3]) {
                    int v1 = edge_to_constraints[i][0];
                    int v2 = edge_to_constraints[i][2];
                    if (edge_to_constraints[i][1] < 0) std::swap(v1, v2);
                    int current_v = EdgeDiff[i / 2][i % 2];
                    arcs.push_back(std::make_pair(Vector2i(v1, v2), current_v));
                    if (AllowChange[level][i] == 1)
                        arc_ids.push_back(i + 1);
                    else {
                        arc_ids.push_back(-(i + 1));
                    }
                }
            }
            int supply = 0;
            int demand = 0;
            for (int i = 0; i < initial.size(); ++i) {
                int init_val = initial[i];
                if (init_val > 0) {
                    arcs.push_back(std::make_pair(Vector2i(-1, i), initial[i]));
                    supply += init_val;
                } else if (init_val < 0) {
                    demand -= init_val;
                    arcs.push_back(std::make_pair(Vector2i(i, initial.size()), -init_val));
                }
            }

            std::unique_ptr<MaxFlowHelper> solver = nullptr;
            if (use_minimum_cost_flow && level == mRes.mToUpperEdges.size()) {
                lprintf("network simplex MCF is used\n");
                solver = std::make_unique<NetworkSimplexFlowHelper>();
            } else if (supply < 20) {
                solver = std::make_unique<ECMaxFlowHelper>();
            } else {
                solver = std::make_unique<BoykovMaxFlowHelper>();
            }

#ifdef WITH_GUROBI
            if (use_minimum_cost_flow && level == mRes.mToUpperEdges.size()) {
                solver = std::make_unique<GurobiFlowHelper>();
            }
#endif
            solver->resize(initial.size() + 2, arc_ids.size());

            std::set<int> ids;
            for (int i = 0; i < arcs.size(); ++i) {
                int v1 = arcs[i].first[0] + 1;
                int v2 = arcs[i].first[1] + 1;
                int c = arcs[i].second;
                if (v1 == 0 || v2 == initial.size() + 1) {
                    solver->addEdge(v1, v2, c, 0, -1);
                } else {
                    if (arc_ids[i] > 0)
                        solver->addEdge(v1, v2, std::max(0, c + edge_capacity),
                                        std::max(0, -c + edge_capacity), arc_ids[i] - 1);
                    else {
                        if (c > 0)
                            solver->addEdge(v1, v2, std::max(0, c - 1),
                                            std::max(0, -c + edge_capacity), -1 - arc_ids[i]);
                        else
                            solver->addEdge(v1, v2, std::max(0, c + edge_capacity),
                                            std::max(0, -c - 1), -1 - arc_ids[i]);
                    }
                }
            }
            int flow_count = solver->compute();

            solver->applyTo(EdgeDiff);

            lprintf("flow_count = %d, supply = %d\n", flow_count, supply);
            if (flow_count == supply) fullFlow = true;
            if (level != 0 || fullFlow) break;
            edge_capacity += 1;
            iter++;
            if (iter == 10) {
              /* Probably won't converge. */
              break;
            }
            lprintf("Not full flow, edge_capacity += 1\n");
        }

        if (level != 0) {
            auto& nEdgeDiff = mRes.mEdgeDiff[level - 1];
            auto& toUpper = mRes.mToUpperEdges[level - 1];
            auto& toUpperOrients = mRes.mToUpperOrients[level - 1];
            for (int i = 0; i < toUpper.size(); ++i) {
                if (toUpper[i] >= 0) {
                    int orient = (4 - toUpperOrients[i]) % 4;
                    nEdgeDiff[i] = rshift90(EdgeDiff[toUpper[i]], orient);
                }
            }
        }
    }
}

#ifdef WITH_CUDA

void Optimizer::optimize_orientations_cuda(Hierarchy& mRes) {
    int levelIterations = 6;
    for (int level = mRes.mN.size() - 1; level >= 0; --level) {
        Link* adj = mRes.cudaAdj[level];
        int* adjOffset = mRes.cudaAdjOffset[level];
        glm::dvec3* N = mRes.cudaN[level];
        glm::dvec3* Q = mRes.cudaQ[level];
        auto& phases = mRes.cudaPhases[level];
        for (int iter = 0; iter < levelIterations; ++iter) {
            for (int phase = 0; phase < phases.size(); ++phase) {
                int* p = phases[phase];
                UpdateOrientation(p, mRes.mPhases[level][phase].size(), N, Q, adj, adjOffset,
                                  mRes.mAdj[level][phase].size());
            }
        }
        if (level > 0) {
            glm::dvec3* srcField = mRes.cudaQ[level];
            glm::ivec2* toUpper = mRes.cudaToUpper[level - 1];
            glm::dvec3* destField = mRes.cudaQ[level - 1];
            glm::dvec3* N = mRes.cudaN[level - 1];
            PropagateOrientationUpper(srcField, mRes.mQ[level].cols(), toUpper, N, destField);
        }
    }

    for (int l = 0; l < mRes.mN.size() - 1; ++l) {
        glm::dvec3* N = mRes.cudaN[l];
        glm::dvec3* N_next = mRes.cudaN[l + 1];
        glm::dvec3* Q = mRes.cudaQ[l];
        glm::dvec3* Q_next = mRes.cudaQ[l + 1];
        glm::ivec2* toUpper = mRes.cudaToUpper[l];

        PropagateOrientationLower(toUpper, Q, N, Q_next, N_next, mRes.mToUpper[l].cols());
    }
}

void Optimizer::optimize_positions_cuda(Hierarchy& mRes) {
    int levelIterations = 6;
    for (int level = mRes.mAdj.size() - 1; level >= 0; --level) {
        Link* adj = mRes.cudaAdj[level];
        int* adjOffset = mRes.cudaAdjOffset[level];
        glm::dvec3* N = mRes.cudaN[level];
        glm::dvec3* Q = mRes.cudaQ[level];
        glm::dvec3* V = mRes.cudaV[level];
        glm::dvec3* O = mRes.cudaO[level];
        std::vector<int*> phases = mRes.cudaPhases[level];
        for (int iter = 0; iter < levelIterations; ++iter) {
            for (int phase = 0; phase < phases.size(); ++phase) {
                int* p = phases[phase];
                UpdatePosition(p, mRes.mPhases[level][phase].size(), N, Q, adj, adjOffset,
                               mRes.mAdj[level][phase].size(), V, O, mRes.mScale);
            }
        }
        if (level > 0) {
            glm::dvec3* srcField = mRes.cudaO[level];
            glm::ivec2* toUpper = mRes.cudaToUpper[level - 1];
            glm::dvec3* destField = mRes.cudaO[level - 1];
            glm::dvec3* N = mRes.cudaN[level - 1];
            glm::dvec3* V = mRes.cudaV[level - 1];
            PropagatePositionUpper(srcField, mRes.mO[level].cols(), toUpper, N, V, destField);
        }
    }
}

#endif

} // namespace qflow
