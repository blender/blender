#ifndef FIELD_MATH_H_
#define FIELD_MATH_H_

#ifdef WITH_CUDA
#    include <glm/glm.hpp>
#endif
#include <Eigen/Core>
#include <Eigen/Dense>
#include <algorithm>
#include <vector>

namespace qflow {

using namespace Eigen;

struct DEdge
{
    DEdge()
    : x(0), y(0)
    {}
    DEdge(int _x, int _y) {
        if (_x > _y)
            x = _y, y = _x;
        else
            x = _x, y = _y;
    }
    bool operator<(const DEdge& e) const {
        return (x < e.x) || (x == e.x && y < e.y);
    }
    bool operator==(const DEdge& e) const {
        return x == e.x && y == e.y;
    }
    bool operator!=(const DEdge& e) const {
        return x != e.x || y != e.y;
    }
    int x, y;
};

inline int get_parents(std::vector<std::pair<int, int>>& parents, int j) {
    if (j == parents[j].first) return j;
    int k = get_parents(parents, parents[j].first);
    parents[j].second = (parents[j].second + parents[parents[j].first].second) % 4;
    parents[j].first = k;
    return k;
}

inline int get_parents_orient(std::vector<std::pair<int, int>>& parents, int j) {
    if (j == parents[j].first) return parents[j].second;
    return (parents[j].second + get_parents_orient(parents, parents[j].first)) % 4;
}

inline double fast_acos(double x) {
    double negate = double(x < 0.0f);
    x = std::abs(x);
    double ret = -0.0187293f;
    ret *= x;
    ret = ret + 0.0742610f;
    ret *= x;
    ret = ret - 0.2121144f;
    ret *= x;
    ret = ret + 1.5707288f;
    ret = ret * std::sqrt(1.0f - x);
    ret = ret - 2.0f * negate * ret;
    return negate * (double)M_PI + ret;
}

inline double signum(double value) { return std::copysign((double)1, value); }

/// Always-positive modulo function (assumes b > 0)
inline int modulo(int a, int b) {
    int r = a % b;
    return (r < 0) ? r + b : r;
}

inline Vector3d rotate90_by(const Vector3d &q, const Vector3d &n, int amount) {
    return ((amount & 1) ? (n.cross(q)) : q) * (amount < 2 ? 1.0f : -1.0f);
}

inline Vector2i rshift90(Vector2i shift, int amount) {
    if (amount & 1) shift = Vector2i(-shift.y(), shift.x());
    if (amount >= 2) shift = -shift;
    return shift;
}

inline std::pair<int, int> compat_orientation_extrinsic_index_4(const Vector3d &q0,
                                                                const Vector3d &n0,
                                                                const Vector3d &q1,
                                                                const Vector3d &n1) {
    const Vector3d A[2] = {q0, n0.cross(q0)};
    const Vector3d B[2] = {q1, n1.cross(q1)};

    double best_score = -std::numeric_limits<double>::infinity();
    int best_a = 0, best_b = 0;

    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            double score = std::abs(A[i].dot(B[j]));
            if (score > best_score) {
                best_a = i;
                best_b = j;
                best_score = score;
            }
        }
    }

    if (A[best_a].dot(B[best_b]) < 0) best_b += 2;

    return std::make_pair(best_a, best_b);
}

inline std::pair<Vector3d, Vector3d> compat_orientation_extrinsic_4(const Vector3d &q0,
                                                                    const Vector3d &n0,
                                                                    const Vector3d &q1,
                                                                    const Vector3d &n1) {
    const Vector3d A[2] = {q0, n0.cross(q0)};
    const Vector3d B[2] = {q1, n1.cross(q1)};

    double best_score = -std::numeric_limits<double>::infinity();
    int best_a = 0, best_b = 0;

    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            double score = std::abs(A[i].dot(B[j]));
            if (score > best_score + 1e-6) {
                best_a = i;
                best_b = j;
                best_score = score;
            }
        }
    }

    const double dp = A[best_a].dot(B[best_b]);
    return std::make_pair(A[best_a], B[best_b] * signum(dp));
}

inline Vector3d middle_point(const Vector3d &p0, const Vector3d &n0, const Vector3d &p1,
                             const Vector3d &n1) {
    /* How was this derived?
     *
     * Minimize \|x-p0\|^2 + \|x-p1\|^2, where
     * dot(n0, x) == dot(n0, p0)
     * dot(n1, x) == dot(n1, p1)
     *
     * -> Lagrange multipliers, set derivative = 0
     *  Use first 3 equalities to write x in terms of
     *  lambda_1 and lambda_2. Substitute that into the last
     *  two equations and solve for the lambdas. Finally,
     *  add a small epsilon term to avoid issues when n1=n2.
     */
    double n0p0 = n0.dot(p0), n0p1 = n0.dot(p1), n1p0 = n1.dot(p0), n1p1 = n1.dot(p1),
           n0n1 = n0.dot(n1), denom = 1.0f / (1.0f - n0n1 * n0n1 + 1e-4f),
           lambda_0 = 2.0f * (n0p1 - n0p0 - n0n1 * (n1p0 - n1p1)) * denom,
           lambda_1 = 2.0f * (n1p0 - n1p1 - n0n1 * (n0p1 - n0p0)) * denom;

    return 0.5f * (p0 + p1) - 0.25f * (n0 * lambda_0 + n1 * lambda_1);
}

inline Vector3d position_floor_4(const Vector3d &o, const Vector3d &q, const Vector3d &n,
                                 const Vector3d &p, double scale_x, double scale_y,
                                 double inv_scale_x, double inv_scale_y) {
    Vector3d t = n.cross(q);
    Vector3d d = p - o;
    return o + q * std::floor(q.dot(d) * inv_scale_x) * scale_x +
           t * std::floor(t.dot(d) * inv_scale_y) * scale_y;
}

inline std::pair<Vector3d, Vector3d> compat_position_extrinsic_4(
    const Vector3d &p0, const Vector3d &n0, const Vector3d &q0, const Vector3d &o0,
    const Vector3d &p1, const Vector3d &n1, const Vector3d &q1, const Vector3d &o1, double scale_x,
    double scale_y, double inv_scale_x, double inv_scale_y, double scale_x_1, double scale_y_1,
    double inv_scale_x_1, double inv_scale_y_1) {
    Vector3d t0 = n0.cross(q0), t1 = n1.cross(q1);
    Vector3d middle = middle_point(p0, n0, p1, n1);
    Vector3d o0p =
        position_floor_4(o0, q0, n0, middle, scale_x, scale_y, inv_scale_x, inv_scale_y);
    Vector3d o1p =
        position_floor_4(o1, q1, n1, middle, scale_x_1, scale_y_1, inv_scale_x_1, inv_scale_y_1);

    double best_cost = std::numeric_limits<double>::infinity();
    int best_i = -1, best_j = -1;

    for (int i = 0; i < 4; ++i) {
        Vector3d o0t = o0p + (q0 * (i & 1) * scale_x + t0 * ((i & 2) >> 1) * scale_y);
        for (int j = 0; j < 4; ++j) {
            Vector3d o1t = o1p + (q1 * (j & 1) * scale_x_1 + t1 * ((j & 2) >> 1) * scale_y_1);
            double cost = (o0t - o1t).squaredNorm();

            if (cost < best_cost) {
                best_i = i;
                best_j = j;
                best_cost = cost;
            }
        }
    }

    return std::make_pair(
        o0p + (q0 * (best_i & 1) * scale_x + t0 * ((best_i & 2) >> 1) * scale_y),
        o1p + (q1 * (best_j & 1) * scale_x_1 + t1 * ((best_j & 2) >> 1) * scale_y_1));
}

inline Vector3d position_round_4(const Vector3d &o, const Vector3d &q, const Vector3d &n,
                                 const Vector3d &p, double scale_x, double scale_y,
                                 double inv_scale_x, double inv_scale_y) {
    Vector3d t = n.cross(q);
    Vector3d d = p - o;
    return o + q * std::round(q.dot(d) * inv_scale_x) * scale_x +
           t * std::round(t.dot(d) * inv_scale_y) * scale_y;
}

inline Vector2i position_floor_index_4(const Vector3d &o, const Vector3d &q, const Vector3d &n,
                                       const Vector3d &p, double /* unused */, double /* unused */,
                                       double inv_scale_x, double inv_scale_y) {
    Vector3d t = n.cross(q);
    Vector3d d = p - o;
    return Vector2i((int)std::floor(q.dot(d) * inv_scale_x),
                    (int)std::floor(t.dot(d) * inv_scale_y));
}

inline std::pair<Vector2i, Vector2i> compat_position_extrinsic_index_4(
    const Vector3d &p0, const Vector3d &n0, const Vector3d &q0, const Vector3d &o0,
    const Vector3d &p1, const Vector3d &n1, const Vector3d &q1, const Vector3d &o1, double scale_x,
    double scale_y, double inv_scale_x, double inv_scale_y, double scale_x_1, double scale_y_1,
    double inv_scale_x_1, double inv_scale_y_1, double *error) {
    Vector3d t0 = n0.cross(q0), t1 = n1.cross(q1);
    Vector3d middle = middle_point(p0, n0, p1, n1);
    Vector2i o0p =
        position_floor_index_4(o0, q0, n0, middle, scale_x, scale_y, inv_scale_x, inv_scale_y);
    Vector2i o1p = position_floor_index_4(o1, q1, n1, middle, scale_x_1, scale_y_1, inv_scale_x_1,
                                          inv_scale_y_1);

    double best_cost = std::numeric_limits<double>::infinity();
    int best_i = -1, best_j = -1;

    for (int i = 0; i < 4; ++i) {
        Vector3d o0t =
            o0 + (q0 * ((i & 1) + o0p[0]) * scale_x + t0 * (((i & 2) >> 1) + o0p[1]) * scale_y);
        for (int j = 0; j < 4; ++j) {
            Vector3d o1t = o1 + (q1 * ((j & 1) + o1p[0]) * scale_x_1 +
                                 t1 * (((j & 2) >> 1) + o1p[1]) * scale_y_1);
            double cost = (o0t - o1t).squaredNorm();

            if (cost < best_cost) {
                best_i = i;
                best_j = j;
                best_cost = cost;
            }
        }
    }
    if (error) *error = best_cost;

    return std::make_pair(Vector2i((best_i & 1) + o0p[0], ((best_i & 2) >> 1) + o0p[1]),
                          Vector2i((best_j & 1) + o1p[0], ((best_j & 2) >> 1) + o1p[1]));
}

inline void coordinate_system(const Vector3d &a, Vector3d &b, Vector3d &c) {
    if (std::abs(a.x()) > std::abs(a.y())) {
        double invLen = 1.0f / std::sqrt(a.x() * a.x() + a.z() * a.z());
        c = Vector3d(a.z() * invLen, 0.0f, -a.x() * invLen);
    } else {
        double invLen = 1.0f / std::sqrt(a.y() * a.y() + a.z() * a.z());
        c = Vector3d(0.0f, a.z() * invLen, -a.y() * invLen);
    }
    b = c.cross(a);
}

inline Vector3d rotate_vector_into_plane(Vector3d q, const Vector3d &source_normal,
                                         const Vector3d &target_normal) {
    const double cosTheta = source_normal.dot(target_normal);
    if (cosTheta < 0.9999f) {
        if (cosTheta < -0.9999f) return -q;
        Vector3d axis = source_normal.cross(target_normal);
        q = q * cosTheta + axis.cross(q) +
            axis * (axis.dot(q) * (1.0 - cosTheta) / axis.dot(axis));
    }
    return q;
}

inline Vector3d Travel(Vector3d p, const Vector3d &dir, double &len, int &f, VectorXi &E2E,
                       MatrixXd &V, MatrixXi &F, MatrixXd &NF,
                       std::vector<MatrixXd> &triangle_space, double *tx = 0, double *ty = 0) {
    Vector3d N = NF.col(f);
    Vector3d pt = (dir - dir.dot(N) * N).normalized();
    int prev_id = -1;
    int count = 0;
    while (len > 0) {
        count += 1;
        Vector3d t1 = V.col(F(1, f)) - V.col(F(0, f));
        Vector3d t2 = V.col(F(2, f)) - V.col(F(0, f));
        Vector3d N = NF.col(f);
        //		printf("point dis: %f\n", (p - V.col(F(1, f))).dot(N));
        int edge_id = f * 3;
        double max_len = 1e30;
        bool found = false;
        int next_id, next_f;
        Vector3d next_q;
        Matrix3d m, n;
        m.col(0) = t1;
        m.col(1) = t2;
        m.col(2) = N;
        n = m.inverse();
        MatrixXd &T = triangle_space[f];
        VectorXd coord = T * Vector3d(p - V.col(F(0, f)));
        VectorXd dirs = (T * pt);

        double lens[3];
        lens[0] = -coord.y() / dirs.y();
        lens[1] = (1 - coord.x() - coord.y()) / (dirs.x() + dirs.y());
        lens[2] = -coord.x() / dirs.x();
        for (int fid = 0; fid < 3; ++fid) {
            if (fid + edge_id == prev_id) continue;

            if (lens[fid] >= 0 && lens[fid] < max_len) {
                max_len = lens[fid];
                next_id = E2E[edge_id + fid];
                next_f = next_id;
                if (next_f != -1) next_f /= 3;
                found = true;
            }
        }
        if (!found) {
            printf("error...\n");
            exit(0);
        }
        //		printf("status: %f %f %d\n", len, max_len, f);
        if (max_len >= len) {
            if (tx && ty) {
                *tx = coord.x() + dirs.x() * len;
                *ty = coord.y() + dirs.y() * len;
            }
            p = p + len * pt;
            len = 0;
            return p;
        }
        p = V.col(F(0, f)) + t1 * (coord.x() + dirs.x() * max_len) +
            t2 * (coord.y() + dirs.y() * max_len);
        len -= max_len;
        if (next_f == -1) {
            if (tx && ty) {
                *tx = coord.x() + dirs.x() * max_len;
                *ty = coord.y() + dirs.y() * max_len;
            }
            return p;
        }
        pt = rotate_vector_into_plane(pt, NF.col(f), NF.col(next_f));
        f = next_f;
        prev_id = next_id;
    }
    return p;
}
inline Vector3d TravelField(Vector3d p, Vector3d &pt, double &len, int &f, VectorXi &E2E,
                            MatrixXd &V, MatrixXi &F, MatrixXd &NF, MatrixXd &QF, MatrixXd &QV,
                            MatrixXd &NV, std::vector<MatrixXd> &triangle_space, double *tx = 0,
                            double *ty = 0, Vector3d *dir_unfold = 0) {
    Vector3d N = NF.col(f);
    pt = (pt - pt.dot(N) * N).normalized();
    int prev_id = -1;
    int count = 0;
    std::vector<Vector3d> Ns;

    auto FaceQFromVertices = [&](int f, double tx, double ty) {
        const Vector3d &n = NF.col(f);
        const Vector3d &q_1 = QV.col(F(0, f)), &q_2 = QV.col(F(1, f)), &q_3 = QV.col(F(2, f));
        const Vector3d &n_1 = NV.col(F(0, f)), &n_2 = NV.col(F(1, f)), &n_3 = NV.col(F(2, f));
        Vector3d q_1n = rotate_vector_into_plane(q_1, n_1, n);
        Vector3d q_2n = rotate_vector_into_plane(q_2, n_2, n);
        Vector3d q_3n = rotate_vector_into_plane(q_3, n_3, n);
        auto orient = compat_orientation_extrinsic_4(q_1n, n, q_2n, n);
        Vector3d q = (orient.first * tx + orient.second * ty).normalized();
        orient = compat_orientation_extrinsic_4(q, n, q_3n, n);
        q = (orient.first * (tx + ty) + orient.second * (1 - tx - ty)).normalized();
        return q;
    };

    auto BestQFromGivenQ = [&](const Vector3d &n, const Vector3d &q, const Vector3d &given_q) {
        Vector3d q_1 = n.cross(q);
        double t1 = q.dot(given_q);
        double t2 = q_1.dot(given_q);
        if (fabs(t1) > fabs(t2)) {
            if (t1 > 0.0)
                return Vector3d(q);
            else
                return Vector3d(-q);
        } else {
            if (t2 > 0.0)
                return Vector3d(q_1);
            else
                return Vector3d(-q_1);
        }
    };

    while (len > 0) {
        count += 1;
        Vector3d t1 = V.col(F(1, f)) - V.col(F(0, f));
        Vector3d t2 = V.col(F(2, f)) - V.col(F(0, f));
        Vector3d N = NF.col(f);
        Ns.push_back(N);
        //		printf("point dis: %f\n", (p - V.col(F(1, f))).dot(N));
        int edge_id = f * 3;
        double max_len = 1e30;
        bool found = false;
        int next_id = -1, next_f = -1;
        Vector3d next_q;
        Matrix3d m, n;
        m.col(0) = t1;
        m.col(1) = t2;
        m.col(2) = N;
        n = m.inverse();
        MatrixXd &T = triangle_space[f];
        VectorXd coord = T * Vector3d(p - V.col(F(0, f)));
        VectorXd dirs = (T * pt);
        double lens[3];
        lens[0] = -coord.y() / dirs.y();
        lens[1] = (1 - coord.x() - coord.y()) / (dirs.x() + dirs.y());
        lens[2] = -coord.x() / dirs.x();
        for (int fid = 0; fid < 3; ++fid) {
            if (fid + edge_id == prev_id) continue;

            if (lens[fid] >= 0 && lens[fid] < max_len) {
                max_len = lens[fid];
                next_id = E2E[edge_id + fid];
                next_f = next_id;
                if (next_f != -1) next_f /= 3;
                found = true;
            }
        }
        double w1 = (coord.x() + dirs.x() * max_len);
        double w2 = (coord.y() + dirs.y() * max_len);
        if (w1 < 0) w1 = 0.0f;
        if (w2 < 0) w2 = 0.0f;
        if (w1 + w2 > 1) {
            double w = w1 + w2;
            w1 /= w;
            w2 /= w;
        }

        if (!found) {
            printf("error...\n");
            exit(0);
        }
        //		printf("status: %f %f %d\n", len, max_len, f);
        if (max_len >= len) {
            if (tx && ty) {
                *tx = w1;
                *ty = w2;
            }
            Vector3d ideal_q = FaceQFromVertices(f, *tx, *ty);
            *dir_unfold = BestQFromGivenQ(NF.col(f), ideal_q, *dir_unfold);
            for (int i = Ns.size() - 1; i > 0; --i) {
                *dir_unfold = rotate_vector_into_plane(*dir_unfold, Ns[i], Ns[i - 1]);
            }
            p = p + len * pt;
            len = 0;
            return p;
        }
        p = V.col(F(0, f)) + t1 * w1 + t2 * w2;
        len -= max_len;
        if (next_f == -1) {
            if (tx && ty) {
                *tx = w1;
                *ty = w2;
            }
            Vector3d ideal_q = FaceQFromVertices(f, *tx, *ty);
            *dir_unfold = BestQFromGivenQ(NF.col(f), ideal_q, *dir_unfold);
            for (int i = Ns.size() - 1; i > 0; --i) {
                *dir_unfold = rotate_vector_into_plane(*dir_unfold, Ns[i], Ns[i - 1]);
            }
            return p;
        }
        pt = rotate_vector_into_plane(pt, NF.col(f), NF.col(next_f));
        //		pt = BestQFromGivenQ(NF.col(next_f), QF.col(next_f), pt);
        if (dir_unfold) {
            *dir_unfold = BestQFromGivenQ(NF.col(next_f), QF.col(next_f), *dir_unfold);
        }
        f = next_f;
        prev_id = next_id;
    }

    return p;
}

} // namespace qflow

#endif
