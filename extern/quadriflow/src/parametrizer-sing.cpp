#include "config.hpp"
#include "field-math.hpp"
#include "parametrizer.hpp"

namespace qflow {

void Parametrizer::ComputeOrientationSingularities() {
    MatrixXd &N = hierarchy.mN[0], &Q = hierarchy.mQ[0];
    const MatrixXi& F = hierarchy.mF;
    singularities.clear();
    for (int f = 0; f < F.cols(); ++f) {
        int index = 0;
        int abs_index = 0;
        for (int k = 0; k < 3; ++k) {
            int i = F(k, f), j = F(k == 2 ? 0 : (k + 1), f);
            auto value =
                compat_orientation_extrinsic_index_4(Q.col(i), N.col(i), Q.col(j), N.col(j));
            index += value.second - value.first;
            abs_index += std::abs(value.second - value.first);
        }
        int index_mod = modulo(index, 4);
        if (index_mod == 1 || index_mod == 3) {
            if (index >= 4 || index < 0) {
                Q.col(F(0, f)) = -Q.col(F(0, f));
            }
            singularities[f] = index_mod;
        }
    }
}

void Parametrizer::ComputePositionSingularities() {
    const MatrixXd &V = hierarchy.mV[0], &N = hierarchy.mN[0], &Q = hierarchy.mQ[0],
                   &O = hierarchy.mO[0];
    const MatrixXi& F = hierarchy.mF;

    pos_sing.clear();
    pos_rank.resize(F.rows(), F.cols());
    pos_index.resize(6, F.cols());
    for (int f = 0; f < F.cols(); ++f) {
        Vector2i index = Vector2i::Zero();
        uint32_t i0 = F(0, f), i1 = F(1, f), i2 = F(2, f);

        Vector3d q[3] = {Q.col(i0).normalized(), Q.col(i1).normalized(), Q.col(i2).normalized()};
        Vector3d n[3] = {N.col(i0), N.col(i1), N.col(i2)};
        Vector3d o[3] = {O.col(i0), O.col(i1), O.col(i2)};
        Vector3d v[3] = {V.col(i0), V.col(i1), V.col(i2)};

        int best[3];
        double best_dp = -std::numeric_limits<double>::infinity();
        for (int i = 0; i < 4; ++i) {
            Vector3d v0 = rotate90_by(q[0], n[0], i);
            for (int j = 0; j < 4; ++j) {
                Vector3d v1 = rotate90_by(q[1], n[1], j);
                for (int k = 0; k < 4; ++k) {
                    Vector3d v2 = rotate90_by(q[2], n[2], k);
                    double dp = std::min(std::min(v0.dot(v1), v1.dot(v2)), v2.dot(v0));
                    if (dp > best_dp) {
                        best_dp = dp;
                        best[0] = i;
                        best[1] = j;
                        best[2] = k;
                    }
                }
            }
        }
        pos_rank(0, f) = best[0];
        pos_rank(1, f) = best[1];
        pos_rank(2, f) = best[2];
        for (int k = 0; k < 3; ++k) q[k] = rotate90_by(q[k], n[k], best[k]);

        for (int k = 0; k < 3; ++k) {
            int kn = k == 2 ? 0 : (k + 1);
            double scale_x = hierarchy.mScale, scale_y = hierarchy.mScale,
                   scale_x_1 = hierarchy.mScale, scale_y_1 = hierarchy.mScale;
            if (flag_adaptive_scale) {
                scale_x *= hierarchy.mS[0](0, F(k, f));
                scale_y *= hierarchy.mS[0](1, F(k, f));
                scale_x_1 *= hierarchy.mS[0](0, F(kn, f));
                scale_y_1 *= hierarchy.mS[0](1, F(kn, f));
                if (best[k] % 2 != 0) std::swap(scale_x, scale_y);
                if (best[kn] % 2 != 0) std::swap(scale_x_1, scale_y_1);
            }
            double inv_scale_x = 1.0 / scale_x, inv_scale_y = 1.0 / scale_y,
                   inv_scale_x_1 = 1.0 / scale_x_1, inv_scale_y_1 = 1.0 / scale_y_1;
            std::pair<Vector2i, Vector2i> value = compat_position_extrinsic_index_4(
                v[k], n[k], q[k], o[k], v[kn], n[kn], q[kn], o[kn], scale_x, scale_y, inv_scale_x,
                inv_scale_y, scale_x_1, scale_y_1, inv_scale_x_1, inv_scale_y_1, nullptr);
            auto diff = value.first - value.second;
            index += diff;
            pos_index(k * 2, f) = diff[0];
            pos_index(k * 2 + 1, f) = diff[1];
        }

        if (index != Vector2i::Zero()) {
            pos_sing[f] = rshift90(index, best[0]);
        }
    }
}

void Parametrizer::AnalyzeValence() {
    auto& F = hierarchy.mF;
    std::map<int, int> sing;
    for (auto& f : singularities) {
        for (int i = 0; i < 3; ++i) {
            sing[F(i, f.first)] = f.second;
        }
    }
    auto& F2E = face_edgeIds;
    auto& E2E = hierarchy.mE2E;
    auto& FQ = face_edgeOrients;
    std::set<int> sing1, sing2;
    for (int i = 0; i < F2E.size(); ++i) {
        for (int j = 0; j < 3; ++j) {
            int deid = i * 3 + j;
            int sum_int = 0;
            std::vector<int> edges;
            std::vector<double> angles;
            do {
                int deid1 = deid / 3 * 3 + (deid + 2) % 3;
                deid = E2E[deid1];
                sum_int += (FQ[deid / 3][deid % 3] + 6 - FQ[deid1 / 3][deid1 % 3]) % 4;
            } while (deid != i * 3 + j);
            if (sum_int % 4 == 2) {
                printf("OMG! valence = 2\n");
                exit(0);
            }
            if (sum_int % 4 == 1) sing1.insert(F(j, i));
            if (sum_int % 4 == 3) sing2.insert(F(j, i));
        }
    }
    int count3 = 0, count4 = 0;
    for (auto& s : singularities) {
        if (s.second == 1)
            count3 += 1;
        else
            count4 += 1;
    }
    printf("singularity: <%d %d> <%d %d>\n", (int)sing1.size(), (int)sing2.size(), count3, count4);
}


} // namespace qflow
