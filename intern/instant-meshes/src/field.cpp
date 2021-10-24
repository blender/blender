/*
    field.cpp: Routines for averaging orientations and directions subject
    to various symmetry conditions. Also contains the Optimizer class which
    uses these routines to smooth fields hierarchically.

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include "field.h"
#include "serializer.h"

static const Float sqrt_3_over_4 = 0.866025403784439f;
static const uint32_t INVALID = (uint32_t) -1;

Vector3f rotate180(const Vector3f &q, const Vector3f &/* unused */) {
    return -q;
}

Vector3f rotate180_by(const Vector3f &q, const Vector3f &/* unused */, int amount) {
    return (amount & 1) ? Vector3f(-q) : q;
}

Vector2i rshift180(Vector2i shift, int amount) {
    if (amount & 1)
        shift = -shift;
    return shift;
}

Vector3f rotate90(const Vector3f &q, const Vector3f &n) {
    return n.cross(q);
}

Vector3f rotate90_by(const Vector3f &q, const Vector3f &n, int amount) {
    return ((amount & 1) ? (n.cross(q)) : q) * (amount < 2 ? 1.0f : -1.0f);
}

Vector2i rshift90(Vector2i shift, int amount) {
    if (amount & 1)
        shift = Vector2i(-shift.y(), shift.x());
    if (amount >= 2)
        shift = -shift;
    return shift;
}

Vector3f rotate60(const Vector3f &d, const Vector3f &n) {
    return sqrt_3_over_4 * n.cross(d) + 0.5f*(d + n * n.dot(d));
}

Vector2i rshift60(Vector2i shift, int amount) {
    for (int i=0; i<amount; ++i)
        shift = Vector2i(-shift.y(), shift.x() + shift.y());
    return shift;
}

Vector3f rotate60_by(const Vector3f &d, const Vector3f &n, int amount) {
    switch (amount) {
        case 0: return d;
        case 1: return rotate60(d, n);
        case 2: return -rotate60(d, -n);
        case 3: return -d;
        case 4: return -rotate60(d, n);
        case 5: return rotate60(d, -n);
    }
    throw std::runtime_error("rotate60: invalid argument");
}

Vector3f rotate_vector_into_plane(Vector3f q, const Vector3f &source_normal, const Vector3f &target_normal) {
    const Float cosTheta = source_normal.dot(target_normal);
    if (cosTheta < 0.9999f) {
        Vector3f axis = source_normal.cross(target_normal);
        q = q * cosTheta + axis.cross(q) +
             axis * (axis.dot(q) * (1.0f - cosTheta) / axis.dot(axis));
    }
    return q;
}

inline Vector3f middle_point(const Vector3f &p0, const Vector3f &n0, const Vector3f &p1, const Vector3f &n1) {
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
    Float n0p0 = n0.dot(p0), n0p1 = n0.dot(p1),
          n1p0 = n1.dot(p0), n1p1 = n1.dot(p1),
          n0n1 = n0.dot(n1),
          denom = 1.0f / (1.0f - n0n1*n0n1 + 1e-4f),
          lambda_0 = 2.0f*(n0p1 - n0p0 - n0n1*(n1p0 - n1p1))*denom,
          lambda_1 = 2.0f*(n1p0 - n1p1 - n0n1*(n0p1 - n0p0))*denom;

    return 0.5f * (p0 + p1) - 0.25f * (n0 * lambda_0 + n1 * lambda_1);
}

std::pair<Vector3f, Vector3f> compat_orientation_intrinsic_2(
    const Vector3f &q0, const Vector3f &n0, const Vector3f &_q1, const Vector3f &n1) {
    const Vector3f q1 = rotate_vector_into_plane(_q1, n1, n0);
    return std::make_pair(q0, q1 * signum(q1.dot(q0)));
}

std::pair<Vector3f, Vector3f> compat_orientation_intrinsic_4(
    const Vector3f &q0, const Vector3f &n0, const Vector3f &_q1, const Vector3f &n1) {
    const Vector3f q1 = rotate_vector_into_plane(_q1, n1, n0);
    const Vector3f t1 = n0.cross(q1);
    const Float dp0 = q1.dot(q0), dp1 = t1.dot(q0);

    if (std::abs(dp0) > std::abs(dp1))
        return std::make_pair(q0, q1 * signum(dp0));
    else
        return std::make_pair(q0, t1 * signum(dp1));
}

std::pair<Vector3f, Vector3f>
compat_orientation_intrinsic_6(const Vector3f &q0,  const Vector3f &n0,
                               const Vector3f &_q1, const Vector3f &n1) {
    const Vector3f q1 = rotate_vector_into_plane(_q1, n1, n0);
    const Vector3f t1[3] = { rotate60(q1, -n0), q1, rotate60(q1, n0) };
    const Float dp[3] = { t1[0].dot(q0), t1[1].dot(q0), t1[2].dot(q0) };
    const Float abs_dp[3] = { std::abs(dp[0]), std::abs(dp[1]), std::abs(dp[2]) };

    if (abs_dp[0] >= abs_dp[1] && abs_dp[0] >= abs_dp[2])
        return std::make_pair(q0, t1[0] * signum(dp[0]));
    else if (abs_dp[1] >= abs_dp[0] && abs_dp[1] >= abs_dp[2])
        return std::make_pair(q0, t1[1] * signum(dp[1]));
    else
        return std::make_pair(q0, t1[2] * signum(dp[2]));
}

std::pair<int, int>
compat_orientation_intrinsic_index_2(const Vector3f &q0,  const Vector3f &n0,
                                     const Vector3f &_q1, const Vector3f &n1) {
    const Vector3f q1 = rotate_vector_into_plane(_q1, n1, n0);
    return std::make_pair(0, q1.dot(q0) > 0 ? 0 : 1);
}

std::pair<int, int>
compat_orientation_intrinsic_index_4(const Vector3f &q0,  const Vector3f &n0,
                                const Vector3f &_q1, const Vector3f &n1) {
    const Vector3f q1 = rotate_vector_into_plane(_q1, n1, n0);
    const Float dp0 = q1.dot(q0), dp1 = n0.cross(q1).dot(q0);

    if (std::abs(dp0) > std::abs(dp1))
        return std::make_pair(0, dp0 > 0 ? 0 : 2);
    else
        return std::make_pair(0, dp1 > 0 ? 1 : 3);
}

std::pair<int, int>
compat_orientation_intrinsic_index_6(const Vector3f &q0,  const Vector3f &n0,
                                const Vector3f &_q1, const Vector3f &n1) {
    const Vector3f q1 = rotate_vector_into_plane(_q1, n1, n0);
    const Vector3f t1[3] = { rotate60(q1, -n0), q1, rotate60(q1, n0) };
    const Float dp[3] = { t1[0].dot(q0), t1[1].dot(q0), t1[2].dot(q0) };
    const Float abs_dp[3] = { std::abs(dp[0]), std::abs(dp[1]), std::abs(dp[2]) };

    if (abs_dp[0] >= abs_dp[1] && abs_dp[0] >= abs_dp[2])
        return std::make_pair(0, dp[0] > 0 ? 5 : 2);
    else if (abs_dp[1] >= abs_dp[0] && abs_dp[1] >= abs_dp[2])
        return std::make_pair(0, dp[1] > 0 ? 0 : 3);
    else
        return std::make_pair(0, dp[2] > 0 ? 1 : 4);
}

std::pair<Vector3f, Vector3f>
compat_orientation_extrinsic_2(const Vector3f &q0, const Vector3f &n0,
                               const Vector3f &q1, const Vector3f &n1) {
    return std::make_pair(q0, q1 * signum(q0.dot(q1)));
}

std::pair<Vector3f, Vector3f>
compat_orientation_extrinsic_4(const Vector3f &q0, const Vector3f &n0,
                               const Vector3f &q1, const Vector3f &n1) {
    const Vector3f A[2] = { q0, n0.cross(q0) };
    const Vector3f B[2] = { q1, n1.cross(q1) };

    Float best_score = -std::numeric_limits<Float>::infinity();
    int best_a = 0, best_b = 0;

    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            Float score = std::abs(A[i].dot(B[j]));
            if (score > best_score) {
                best_a = i;
                best_b = j;
                best_score = score;
            }
        }
    }

    const Float dp = A[best_a].dot(B[best_b]);
    return std::make_pair(A[best_a], B[best_b] * signum(dp));
}

std::pair<Vector3f, Vector3f>
compat_orientation_extrinsic_6(const Vector3f &q0, const Vector3f &n0,
                               const Vector3f &q1, const Vector3f &n1) {
    const Vector3f A[3] = { rotate60(q0, -n0), q0, rotate60(q0, n0) };
    const Vector3f B[3] = { rotate60(q1, -n1), q1, rotate60(q1, n1) };

    Float best_score = -std::numeric_limits<Float>::infinity();
    int best_a = 0, best_b = 0;

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            Float score = std::abs(A[i].dot(B[j]));
            if (score > best_score) {
                best_a = i;
                best_b = j;
                best_score = score;
            }
        }
    }

    const Float dp = A[best_a].dot(B[best_b]);
    return std::make_pair(A[best_a], B[best_b] * signum(dp));
}

std::pair<int, int>
compat_orientation_extrinsic_index_2(const Vector3f &q0, const Vector3f &n0,
                                     const Vector3f &q1, const Vector3f &n1) {
    return std::make_pair(0, q0.dot(q1) < 0 ? 1 : 0);
}

std::pair<int, int>
compat_orientation_extrinsic_index_4(const Vector3f &q0, const Vector3f &n0,
                                     const Vector3f &q1, const Vector3f &n1) {
    const Vector3f A[2] = { q0, n0.cross(q0) };
    const Vector3f B[2] = { q1, n1.cross(q1) };

    Float best_score = -std::numeric_limits<Float>::infinity();
    int best_a = 0, best_b = 0;

    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            Float score = std::abs(A[i].dot(B[j]));
            if (score > best_score) {
                best_a = i;
                best_b = j;
                best_score = score;
            }
        }
    }

    if (A[best_a].dot(B[best_b]) < 0)
        best_b += 2;

    return std::make_pair(best_a, best_b);
}

std::pair<int, int>
compat_orientation_extrinsic_index_6(const Vector3f &q0, const Vector3f &n0,
                                     const Vector3f &q1, const Vector3f &n1) {
    const Vector3f A[3] = { rotate60(q0, -n0), q0, rotate60(q0, n0) };
    const Vector3f B[3] = { rotate60(q1, -n1), q1, rotate60(q1, n1) };

    Float best_score = -std::numeric_limits<Float>::infinity();
    int best_a = 0, best_b = 0;

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            Float score = std::abs(A[i].dot(B[j]));
            if (score > best_score) {
                best_a = i;
                best_b = j;
                best_score = score;
            }
        }
    }

    if (A[best_a].dot(B[best_b]) < 0)
        best_b += 3;

    return std::make_pair(best_a, best_b);
}

inline Vector3f position_floor_4(const Vector3f &o, const Vector3f &q,
                                 const Vector3f &n, const Vector3f &p,
                                 Float scale, Float inv_scale) {
    Vector3f t = n.cross(q);
    Vector3f d = p - o;
    return o +
        q * std::floor(q.dot(d) * inv_scale) * scale +
        t * std::floor(t.dot(d) * inv_scale) * scale;
}

inline Vector2i position_floor_index_4(const Vector3f &o, const Vector3f &q,
                                       const Vector3f &n, const Vector3f &p,
                                       Float /* unused */, Float inv_scale) {
    Vector3f t = n.cross(q);
    Vector3f d = p - o;
    return Vector2i(
        (int) std::floor(q.dot(d) * inv_scale),
        (int) std::floor(t.dot(d) * inv_scale));
}

inline Vector3f position_round_4(const Vector3f &o, const Vector3f &q,
                                 const Vector3f &n, const Vector3f &p,
                                 Float scale, Float inv_scale) {
    Vector3f t = n.cross(q);
    Vector3f d = p - o;
    return o +
        q * std::round(q.dot(d) * inv_scale) * scale +
        t * std::round(t.dot(d) * inv_scale) * scale;
}

inline Vector2i position_round_index_4(const Vector3f &o, const Vector3f &q,
                                       const Vector3f &n, const Vector3f &p,
                                       Float /* unused */, Float inv_scale) {
    Vector3f t = n.cross(q);
    Vector3f d = p - o;
    return Vector2i(
        (int) std::round(q.dot(d) * inv_scale),
        (int) std::round(t.dot(d) * inv_scale));
}

inline Vector3f position_round_3(const Vector3f &o, const Vector3f &q,
                                 const Vector3f &n, const Vector3f &p,
                                 Float scale, Float inv_scale) {
    Vector3f t = rotate60(q, n);
    Vector3f d = p - o;

    Float dpq = q.dot(d), dpt = t.dot(d);
    Float u = std::floor(( 4*dpq - 2*dpt) * (1.0f / 3.0f) * inv_scale);
    Float v = std::floor((-2*dpq + 4*dpt) * (1.0f / 3.0f) * inv_scale);

    Float best_cost = std::numeric_limits<Float>::infinity();
    int best_i = -1;

    for (int i=0; i<4; ++i) {
        Vector3f ot = o + (q*(u+(i&1)) + t*(v+((i&2)>>1))) * scale;
        Float cost = (ot-p).squaredNorm();
        if (cost < best_cost) {
            best_i = i;
            best_cost = cost;
        }
    }

    return o + (q*(u+(best_i&1)) + t*(v+((best_i&2)>>1))) * scale;
}


inline Vector2i position_round_index_3(const Vector3f &o, const Vector3f &q,
                                       const Vector3f &n, const Vector3f &p,
                                       Float scale, Float inv_scale) {
    Vector3f t = rotate60(q, n);
    Vector3f d = p - o;
    Float dpq = q.dot(d), dpt = t.dot(d);
    int u = (int) std::floor(( 4*dpq - 2*dpt) * (1.0f / 3.0f) * inv_scale);
    int v = (int) std::floor((-2*dpq + 4*dpt) * (1.0f / 3.0f) * inv_scale);

    Float best_cost = std::numeric_limits<Float>::infinity();
    int best_i = -1;

    for (int i=0; i<4; ++i) {
        Vector3f ot = o + (q*(u+(i&1)) + t * (v + ((i&2)>>1))) * scale;
        Float cost = (ot-p).squaredNorm();
        if (cost < best_cost) {
            best_i = i;
            best_cost = cost;
        }
    }

    return Vector2i(
        u+(best_i&1), v+((best_i&2)>>1)
    );
}

inline Vector3f position_floor_3(const Vector3f &o, const Vector3f &q,
                                 const Vector3f &n, const Vector3f &p,
                                 Float scale, Float inv_scale) {
    Vector3f t = rotate60(q, n);
    Vector3f d = p - o;
    Float dpq = q.dot(d), dpt = t.dot(d);
    Float u = std::floor(( 4*dpq - 2*dpt) * (1.0f / 3.0f) * inv_scale);
    Float v = std::floor((-2*dpq + 4*dpt) * (1.0f / 3.0f) * inv_scale);

    return o + (q*u + t*v) * scale;
}

inline Vector2i position_floor_index_3(const Vector3f &o, const Vector3f &q,
                                       const Vector3f &n, const Vector3f &p,
                                       Float /* scale */, Float inv_scale) {
    Vector3f t = rotate60(q, n);
    Vector3f d = p - o;
    Float dpq = q.dot(d), dpt = t.dot(d);
    int u = (int) std::floor(( 4*dpq - 2*dpt) * (1.0f / 3.0f) * inv_scale);
    int v = (int) std::floor((-2*dpq + 4*dpt) * (1.0f / 3.0f) * inv_scale);

    return Vector2i(u, v);
}

std::pair<Vector3f, Vector3f> compat_position_intrinsic_4(
        const Vector3f &p0, const Vector3f &n0, const Vector3f &q0, const Vector3f &o0,
        const Vector3f &p1, const Vector3f &n1, const Vector3f &_q1, const Vector3f &_o1,
        Float scale, Float inv_scale) {
    Float cosTheta = n1.dot(n0);
    Vector3f q1 = _q1, o1 = _o1;

    if (cosTheta < 0.9999f) {
        Vector3f axis = n1.cross(n0);
        Float factor = (1.0f - cosTheta) / axis.dot(axis);
        Vector3f middle = middle_point(p0, n0, p1, n1);
        o1 -= middle;
        q1 = q1 * cosTheta + axis.cross(q1) + axis * (axis.dot(q1) * factor);
        o1 = o1 * cosTheta + axis.cross(o1) + axis * (axis.dot(o1) * factor) + middle;
    }

    return std::make_pair(
        o0, position_round_4(o1, q1, n0, o0, scale, inv_scale)
    );
}

std::pair<Vector2i, Vector2i> compat_position_intrinsic_index_4(
        const Vector3f &p0, const Vector3f &n0, const Vector3f &q0, const Vector3f &o0,
        const Vector3f &p1, const Vector3f &n1, const Vector3f &_q1, const Vector3f &_o1,
        Float scale, Float inv_scale, Float *error) {
    Vector3f q1 = _q1, o1 = _o1;
    Float cosTheta = n1.dot(n0);

    if (cosTheta < 0.9999f) {
        Vector3f axis = n1.cross(n0);
        Float factor = (1.0f - cosTheta) / axis.dot(axis);
        Vector3f middle = middle_point(p0, n0, p1, n1);
        o1 -= middle;
        q1 = q1 * cosTheta + axis.cross(q1) + axis * (axis.dot(q1) * factor);
        o1 = o1 * cosTheta + axis.cross(o1) + axis * (axis.dot(o1) * factor) + middle;
    }

    if (error)
        *error = (o0 - position_round_4(o1, q1, n0, o0, scale, inv_scale)).squaredNorm();

    return std::make_pair(
        Vector2i::Zero(), position_round_index_4(o1, q1, n0, o0, scale, inv_scale)
    );
}

std::pair<Vector3f, Vector3f> compat_position_intrinsic_3(
        const Vector3f &p0, const Vector3f &n0, const Vector3f &q0, const Vector3f &o0,
        const Vector3f &p1, const Vector3f &n1, const Vector3f &_q1, const Vector3f &_o1,
        Float scale, Float inv_scale) {
    Float cosTheta = n1.dot(n0);
    Vector3f q1 = _q1, o1 = _o1;

    if (cosTheta < 0.9999f) {
        Vector3f axis = n1.cross(n0);
        Float factor = (1.0f - cosTheta) / axis.dot(axis);
        Vector3f middle = middle_point(p0, n0, p1, n1);
        o1 -= middle;
        q1 = q1 * cosTheta + axis.cross(q1) + axis * (axis.dot(q1) * factor);
        o1 = o1 * cosTheta + axis.cross(o1) + axis * (axis.dot(o1) * factor) + middle;
    }

    return std::make_pair(
        o0, position_round_3(o1, q1, n0, o0, scale, inv_scale)
    );
}

std::pair<Vector2i, Vector2i> compat_position_intrinsic_index_3(
        const Vector3f &p0, const Vector3f &n0, const Vector3f &q0, const Vector3f &o0,
        const Vector3f &p1, const Vector3f &n1, const Vector3f &_q1, const Vector3f &_o1,
        Float scale, Float inv_scale, Float *error) {
    Vector3f q1 = _q1, o1 = _o1;
    Float cosTheta = n1.dot(n0);

    if (cosTheta < 0.9999f) {
        Vector3f axis = n1.cross(n0);
        Float factor = (1.0f - cosTheta) / axis.dot(axis);
        Vector3f middle = middle_point(p0, n0, p1, n1);
        o1 -= middle;
        q1 = q1 * cosTheta + axis.cross(q1) + axis * (axis.dot(q1) * factor);
        o1 = o1 * cosTheta + axis.cross(o1) + axis * (axis.dot(o1) * factor) + middle;
    }

    if (error)
        *error = (o0 - position_round_3(o1, q1, n0, o0, scale, inv_scale)).squaredNorm();

    return std::make_pair(
        Vector2i::Zero(), position_round_index_3(o1, q1, n0, o0, scale, inv_scale)
    );
}


inline std::pair<Vector3f, Vector3f> compat_position_extrinsic_4(
        const Vector3f &p0, const Vector3f &n0, const Vector3f &q0, const Vector3f &o0,
        const Vector3f &p1, const Vector3f &n1, const Vector3f &q1, const Vector3f &o1,
        Float scale, Float inv_scale) {

    Vector3f t0 = n0.cross(q0), t1 = n1.cross(q1);
    Vector3f middle = middle_point(p0, n0, p1, n1);
    Vector3f o0p = position_floor_4(o0, q0, n0, middle, scale, inv_scale);
    Vector3f o1p = position_floor_4(o1, q1, n1, middle, scale, inv_scale);

    Float best_cost = std::numeric_limits<Float>::infinity();
    int best_i = -1, best_j = -1;

    for (int i=0; i<4; ++i) {
        Vector3f o0t = o0p + (q0 * (i&1) + t0 * ((i&2) >> 1)) * scale;
        for (int j=0; j<4; ++j) {
            Vector3f o1t = o1p + (q1 * (j&1) + t1 * ((j&2) >> 1)) * scale;
            Float cost = (o0t-o1t).squaredNorm();

            if (cost < best_cost) {
                best_i = i;
                best_j = j;
                best_cost = cost;
            }
        }
    }

    return std::make_pair(
        o0p + (q0 * (best_i & 1) + t0 * ((best_i & 2) >> 1)) * scale,
        o1p + (q1 * (best_j & 1) + t1 * ((best_j & 2) >> 1)) * scale);
}

std::pair<Vector2i, Vector2i> compat_position_extrinsic_index_4(
        const Vector3f &p0, const Vector3f &n0, const Vector3f &q0, const Vector3f &o0,
        const Vector3f &p1, const Vector3f &n1, const Vector3f &q1, const Vector3f &o1,
        Float scale, Float inv_scale, Float *error) {
    Vector3f t0 = n0.cross(q0), t1 = n1.cross(q1);
    Vector3f middle = middle_point(p0, n0, p1, n1);
    Vector2i o0p = position_floor_index_4(o0, q0, n0, middle, scale, inv_scale);
    Vector2i o1p = position_floor_index_4(o1, q1, n1, middle, scale, inv_scale);

    Float best_cost = std::numeric_limits<Float>::infinity();
    int best_i = -1, best_j = -1;

    for (int i=0; i<4; ++i) {
        Vector3f o0t = o0 + (q0 * ((i&1)+o0p[0]) + t0 * (((i&2) >> 1) + o0p[1])) * scale;
        for (int j=0; j<4; ++j) {
            Vector3f o1t = o1 + (q1 * ((j&1)+o1p[0]) + t1 * (((j&2) >> 1) + o1p[1])) * scale;
            Float cost = (o0t-o1t).squaredNorm();

            if (cost < best_cost) {
                best_i = i;
                best_j = j;
                best_cost = cost;
            }
        }
    }
    if (error)
        *error = best_cost;

    return std::make_pair(
        Vector2i((best_i & 1) + o0p[0], ((best_i & 2) >> 1) + o0p[1]),
        Vector2i((best_j & 1) + o1p[0], ((best_j & 2) >> 1) + o1p[1]));
}

std::pair<Vector3f, Vector3f> compat_position_extrinsic_3(
        const Vector3f &p0, const Vector3f &n0, const Vector3f &q0, const Vector3f &_o0,
        const Vector3f &p1, const Vector3f &n1, const Vector3f &q1, const Vector3f &_o1,
        Float scale, Float inv_scale) {
    Vector3f middle = middle_point(p0, n0, p1, n1);
    Vector3f o0 = position_floor_3(_o0, q0, n0, middle, scale, inv_scale);
    Vector3f o1 = position_floor_3(_o1, q1, n1, middle, scale, inv_scale);

    Vector3f t0 = rotate60(q0, n0), t1 = rotate60(q1, n1);
    Float best_cost = std::numeric_limits<Float>::infinity();
    int best_i = -1, best_j = -1;
    for (int i=0; i<4; ++i) {
        Vector3f o0t = o0 + (q0*(i&1) + t0*((i&2)>>1)) * scale;
        for (int j=0; j<4; ++j) {
            Vector3f o1t = o1 + (q1*(j&1) + t1*((j&2)>>1)) * scale;
            Float cost = (o0t-o1t).squaredNorm();

            if (cost < best_cost) {
                best_i = i;
                best_j = j;
                best_cost = cost;
            }
        }
    }

    return std::make_pair(
        o0 + (q0*(best_i&1) + t0*((best_i&2)>>1)) * scale,
        o1 + (q1*(best_j&1) + t1*((best_j&2)>>1)) * scale
    );
}

std::pair<Vector2i, Vector2i> compat_position_extrinsic_index_3(
        const Vector3f &p0, const Vector3f &n0, const Vector3f &q0, const Vector3f &o0,
        const Vector3f &p1, const Vector3f &n1, const Vector3f &q1, const Vector3f &o1,
        Float scale, Float inv_scale, Float *error) {
    Vector3f t0 = rotate60(q0, n0), t1 = rotate60(q1, n1);
    Vector3f middle = middle_point(p0, n0, p1, n1);
    Vector2i o0i = position_floor_index_3(o0, q0, n0, middle, scale, inv_scale);
    Vector2i o1i = position_floor_index_3(o1, q1, n1, middle, scale, inv_scale);

    Float best_cost = std::numeric_limits<Float>::infinity();
    int best_i = -1, best_j = -1;
    for (int i=0; i<4; ++i) {
        Vector3f o0t = o0 + (q0*(o0i.x() + (i&1)) + t0*(o0i.y() + ((i&2)>>1))) * scale;
        for (int j=0; j<4; ++j) {
            Vector3f o1t = o1 + (q1*(o1i.x() + (j&1)) + t1*(o1i.y() + ((j&2)>>1))) * scale;
            Float cost = (o0t-o1t).squaredNorm();

            if (cost < best_cost) {
                best_i = i;
                best_j = j;
                best_cost = cost;
            }
        }
    }
    if (error)
        *error = best_cost;

    return std::make_pair(
        Vector2i(o0i.x()+(best_i&1), o0i.y()+((best_i&2)>>1)),
        Vector2i(o1i.x()+(best_j&1), o1i.y()+((best_j&2)>>1)));
}

template <typename Compat, typename Rotate>
static inline Float
optimize_orientations_impl(MultiResolutionHierarchy &mRes, int level,
                           Compat compat, Rotate rotate,
                           const std::function<void(uint32_t)> &progress) {
    const std::vector<std::vector<uint32_t>> &phases = mRes.phases(level);
    const AdjacencyMatrix &adj = mRes.adj(level);
    const MatrixXf &N = mRes.N(level);
    const MatrixXf &CQ = mRes.CQ(level);
    const VectorXf &CQw = mRes.CQw(level);
    MatrixXf &Q = mRes.Q(level);
    const std::vector<uint32_t> *phase = nullptr;

    auto solve_normal = [&](const tbb::blocked_range<uint32_t> &range) {
        for (uint32_t phaseIdx = range.begin(); phaseIdx<range.end(); ++phaseIdx) {
            const uint32_t i = (*phase)[phaseIdx];
            const Vector3f n_i = N.col(i);
            Float weight_sum = 0.0f;
            Vector3f sum = Q.col(i);
            for (Link *link = adj[i]; link != adj[i+1]; ++link) {
                const uint32_t j = link->id;
                const Float weight = link->weight;
                if (weight == 0)
                    continue;
                const Vector3f n_j = N.col(j);
                Vector3f q_j = Q.col(j);
                std::pair<Vector3f, Vector3f> value = compat(sum, n_i, q_j, n_j);
                sum = value.first * weight_sum + value.second * weight;
                sum -= n_i*n_i.dot(sum);
                weight_sum += weight;

                Float norm = sum.norm();
                if (norm > RCPOVERFLOW)
                    sum /= norm;
            }

            if (CQw.size() > 0) {
                Float cw = CQw[i];
                if (cw != 0) {
                    std::pair<Vector3f, Vector3f> value = compat(sum, n_i, CQ.col(i), n_i);
                    sum = value.first * (1 - cw) + value.second * cw;
                    sum -= n_i*n_i.dot(sum);

                    Float norm = sum.norm();
                    if (norm > RCPOVERFLOW)
                        sum /= norm;
                }
            }

            if (weight_sum > 0)
                Q.col(i) = sum;
        }
    };

    auto solve_frozen = [&](const tbb::blocked_range<uint32_t> &range) {
        for (uint32_t phaseIdx = range.begin(); phaseIdx<range.end(); ++phaseIdx) {
            const uint32_t i = (*phase)[phaseIdx];
            const Vector3f n_i = N.col(i);
            Float weight_sum = 0.0f;
            Vector3f sum = Vector3f::Zero();

            for (Link *link = adj[i]; link != adj[i+1]; ++link) {
                const uint32_t j = link->id;
                const Float weight = link->weight;
                if (weight == 0)
                    continue;
                const Vector3f n_j = N.col(j);
                Vector3f q_j = Q.col(j);

                Vector3f temp = rotate(q_j, n_j, link->ivar[1].rot);
                sum += weight * rotate(temp, -n_i, link->ivar[0].rot);
                weight_sum += weight;
            }

            sum -= n_i*n_i.dot(sum);
            Float norm = sum.norm();
            if (norm > RCPOVERFLOW)
                sum /= norm;

            if (weight_sum > 0)
                Q.col(i) = sum;
        }
    };

    Float error = 0.0f;
    for (const std::vector<uint32_t> &phase_ : phases) {
        tbb::blocked_range<uint32_t> range(0u, (uint32_t)phase_.size(), GRAIN_SIZE);
        phase = &phase_;
        if (mRes.frozenQ())
            tbb::parallel_for(range, solve_frozen);
        else
            tbb::parallel_for(range, solve_normal);
        progress(phase_.size());
    }

    return error;
}

template <typename Functor>
static inline Float error_orientations_impl(const MultiResolutionHierarchy &mRes,
                                            int level, Functor functor) {
    const AdjacencyMatrix &adj = mRes.adj(level);
    const MatrixXf &N = mRes.N(level);
    const MatrixXf &Q = mRes.Q(level);

    auto map = [&](const tbb::blocked_range<uint32_t> &range, Float error) -> Float {
        for (uint32_t i = range.begin(); i<range.end(); ++i) {
            Vector3f q_i = Q.col(i).normalized(), n_i = N.col(i);
            for (Link *link = adj[i]; link != adj[i+1]; ++link) {
                const uint32_t j = link->id;
                Vector3f q_j = Q.col(j).normalized(), n_j = N.col(j);
                std::pair<Vector3f, Vector3f> value =
                    functor(q_i.normalized(), n_i, q_j.normalized(), n_j);
                Float angle = fast_acos(std::min((Float) 1, value.first.dot(value.second))) * 180 / M_PI;
                error += angle*angle;
            }
        }
        return error;
    };

    auto reduce = [](Float error1, Float error2) -> Float {
        return error1 + error2;
    };

    return tbb::parallel_reduce(
        tbb::blocked_range<uint32_t>(0, mRes.size(level), GRAIN_SIZE), 0.0,
        map, reduce
    ) / (Float) (adj[mRes.size(level)] - adj[0]);
}

Float optimize_orientations(MultiResolutionHierarchy &mRes, int level,
                            bool extrinsic, int rosy,
                            const std::function<void(uint32_t)> &progress) {
    if (rosy == 2) {
        if (extrinsic)
            return optimize_orientations_impl(mRes, level, compat_orientation_extrinsic_2, rotate180_by, progress);
        else
            return optimize_orientations_impl(mRes, level, compat_orientation_intrinsic_2, rotate180_by, progress);
    } else if (rosy == 4) {
        if (extrinsic)
            return optimize_orientations_impl(mRes, level, compat_orientation_extrinsic_4, rotate90_by, progress);
        else
            return optimize_orientations_impl(mRes, level, compat_orientation_intrinsic_4, rotate90_by, progress);
    } else if (rosy == 6) {
        if (extrinsic)
            return optimize_orientations_impl(mRes, level, compat_orientation_extrinsic_6, rotate60_by, progress);
        else
            return optimize_orientations_impl(mRes, level, compat_orientation_intrinsic_6, rotate60_by, progress);
    } else {
        throw std::runtime_error("Invalid rotation symmetry type " + std::to_string(rosy) + "!");
    }
}

Float error_orientations(MultiResolutionHierarchy &mRes, int level,
                         bool extrinsic, int rosy) {
    if (rosy == 2) {
        if (extrinsic)
            return error_orientations_impl(mRes, level, compat_orientation_extrinsic_2);
        else
            return error_orientations_impl(mRes, level, compat_orientation_intrinsic_2);
    } else if (rosy == 4) {
        if (extrinsic)
            return error_orientations_impl(mRes, level, compat_orientation_extrinsic_4);
        else
            return error_orientations_impl(mRes, level, compat_orientation_intrinsic_4);
    } else if (rosy == 6) {
        if (extrinsic)
            return error_orientations_impl(mRes, level, compat_orientation_extrinsic_6);
        else
            return error_orientations_impl(mRes, level, compat_orientation_intrinsic_6);
    } else {
        throw std::runtime_error("Invalid rotation symmetry type " + std::to_string(rosy) + "!");
    }
}

template <typename Functor>
static inline void
freeze_ivars_orientations_impl(MultiResolutionHierarchy &mRes, int level,
                              Functor functor) {
    const AdjacencyMatrix &adj = mRes.adj(level);
    const MatrixXf &N = mRes.N(level);
    const MatrixXf &Q = mRes.Q(level);

    auto map = [&](const tbb::blocked_range<uint32_t> &range) {
        for (uint32_t i = range.begin(); i<range.end(); ++i) {
            const Vector3f &q_i = Q.col(i), &n_i = N.col(i);
            for (Link *link = adj[i]; link != adj[i+1]; ++link) {
                const uint32_t j = link->id;
                const Vector3f &q_j = Q.col(j), &n_j = N.col(j);
                std::pair<int, int> value =
                    functor(q_i.normalized(), n_i, q_j.normalized(), n_j);
                link->ivar[0].rot = value.first;
                link->ivar[1].rot = value.second;
            }
        }
    };

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0, mRes.size(level), GRAIN_SIZE), map);
    mRes.setFrozenQ(true);
}

void freeze_ivars_orientations(MultiResolutionHierarchy &mRes, int level,
                               bool extrinsic, int rosy) {
    if (rosy != 4) /// only rosy=4 for now.
        return;
    if (rosy == 2) {
        if (extrinsic)
            freeze_ivars_orientations_impl(mRes, level, compat_orientation_extrinsic_index_2);
        else
            freeze_ivars_orientations_impl(mRes, level, compat_orientation_intrinsic_index_2);
    } else if (rosy == 4) {
        if (extrinsic)
            freeze_ivars_orientations_impl(mRes, level, compat_orientation_extrinsic_index_4);
        else
            freeze_ivars_orientations_impl(mRes, level, compat_orientation_intrinsic_index_4);
    } else if (rosy == 6) {
        if (extrinsic)
            freeze_ivars_orientations_impl(mRes, level, compat_orientation_extrinsic_index_6);
        else
            freeze_ivars_orientations_impl(mRes, level, compat_orientation_intrinsic_index_6);
    } else {
        throw std::runtime_error("Invalid rotation symmetry type " + std::to_string(rosy) + "!");
    }
}

template <int rosy, typename Functor> inline static void compute_orientation_singularities_impl(
        const MultiResolutionHierarchy &mRes, std::map<uint32_t, uint32_t> &sing, Functor functor) {
    const MatrixXf &N = mRes.N(), &Q = mRes.Q();
    const MatrixXu &F = mRes.F();
    tbb::spin_mutex mutex;
    sing.clear();

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) F.cols(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t f = range.begin(); f < range.end(); ++f) {
                int index = 0;
                for (int k = 0; k < 3; ++k) {
                    int i = F(k, f), j = F(k == 2 ? 0 : (k + 1), f);
                    std::pair<int, int> value = functor(Q.col(i), N.col(i), Q.col(j), N.col(j));
                    index += value.second - value.first;
                }
                index = modulo(index, rosy);
                if (index == 1 || index == rosy-1) {
                    tbb::spin_mutex::scoped_lock lock(mutex);
                    sing[f] = (uint32_t) index;
                }
            }
        }
    );
}

void compute_orientation_singularities(const MultiResolutionHierarchy &mRes, std::map<uint32_t, uint32_t> &sing, bool extrinsic, int rosy) {
    if (rosy == 2) {
        if (extrinsic)
            compute_orientation_singularities_impl<2>(mRes, sing, compat_orientation_extrinsic_index_2);
        else
            compute_orientation_singularities_impl<2>(mRes, sing, compat_orientation_intrinsic_index_2);
    } else if (rosy == 4) {
        if (extrinsic)
            compute_orientation_singularities_impl<4>(mRes, sing, compat_orientation_extrinsic_index_4);
        else
            compute_orientation_singularities_impl<4>(mRes, sing, compat_orientation_intrinsic_index_4);
    } else if (rosy == 6) {
        if (extrinsic)
            compute_orientation_singularities_impl<6>(mRes, sing, compat_orientation_extrinsic_index_6);
        else
            compute_orientation_singularities_impl<6>(mRes, sing, compat_orientation_intrinsic_index_6);
    } else {
        throw std::runtime_error("Unknown rotational symmetry!");
    }
}

template <typename CompatFunctor, typename RoundFunctor> static inline Float optimize_positions_impl(
        MultiResolutionHierarchy &mRes, int level, CompatFunctor compat_functor, RoundFunctor round_functor,
        const std::function<void(uint32_t)> &progress) {
    const std::vector<std::vector<uint32_t>> &phases = mRes.phases(level);
    const AdjacencyMatrix &adj = mRes.adj(level);
    const MatrixXf &N = mRes.N(level), &Q = mRes.Q(level), &V = mRes.V(level);
    const Float scale = mRes.scale(), inv_scale = 1.0f / scale;
    const std::vector<uint32_t> *phase = nullptr;
    const MatrixXf &CQ = mRes.CQ(level);
    const MatrixXf &CO = mRes.CO(level);
    const VectorXf &COw = mRes.COw(level);
    MatrixXf &O = mRes.O(level);

    auto solve_normal = [&](const tbb::blocked_range<uint32_t> &range) {
        for (uint32_t phaseIdx = range.begin(); phaseIdx<range.end(); ++phaseIdx) {
            const uint32_t i = (*phase)[phaseIdx];
            const Vector3f n_i = N.col(i), v_i = V.col(i);
            Vector3f q_i = Q.col(i);

            Vector3f sum = O.col(i);
            Float weight_sum = 0.0f;

            #if 1
                q_i.normalize();
            #endif

            for (Link *link = adj[i]; link != adj[i+1]; ++link) {
                const uint32_t j = link->id;
                const Float weight = link->weight;
                if (weight == 0)
                    continue;

                const Vector3f n_j = N.col(j), v_j = V.col(j);
                Vector3f q_j = Q.col(j), o_j = O.col(j);

                #if 1
                    q_j.normalize();
                #endif

                std::pair<Vector3f, Vector3f> value = compat_functor(
                    v_i, n_i, q_i, sum, v_j, n_j, q_j, o_j, scale, inv_scale);

                sum = value.first*weight_sum + value.second*weight;
                weight_sum += weight;
                if (weight_sum > RCPOVERFLOW)
                    sum /= weight_sum;
                sum -= n_i.dot(sum - v_i)*n_i;
            }

            if (COw.size() > 0) {
                Float cw = COw[i];
                if (cw != 0) {
                    Vector3f co = CO.col(i), cq = CQ.col(i);
                    Vector3f d = co - sum;
                    d -= cq.dot(d)*cq;
                    sum += cw * d;
                    sum -= n_i.dot(sum - v_i)*n_i;
                }
            }

            if (weight_sum > 0)
                O.col(i) = round_functor(sum, q_i, n_i, v_i, scale, inv_scale);
        }
    };

    auto solve_frozen = [&](const tbb::blocked_range<uint32_t> &range) {
        for (uint32_t phaseIdx = range.begin(); phaseIdx<range.end(); ++phaseIdx) {
            const uint32_t i = (*phase)[phaseIdx];
            const Vector3f n_i = N.col(i), v_i = V.col(i);
            Vector3f q_i = Q.col(i);

            Vector3f sum = Vector3f::Zero();
            Float weight_sum = 0.0f;
            #if 1
                q_i.normalize();
            #endif
            const Vector3f t_i = n_i.cross(q_i);

            for (Link *link = adj[i]; link != adj[i+1]; ++link) {
                const uint32_t j = link->id;
                const Float weight = link->weight;
                if (weight == 0)
                    continue;

                const Vector3f n_j = N.col(j);
                Vector3f q_j = Q.col(j), o_j = O.col(j);

                #if 1
                    q_j.normalize();
                #endif
                const Vector3f t_j = n_j.cross(q_j);

                sum += o_j + scale * (
                      q_j * link->ivar[1].translate_u
                    + t_j * link->ivar[1].translate_v
                    - q_i * link->ivar[0].translate_u
                    - t_i * link->ivar[0].translate_v);

                weight_sum += weight;
            }
            sum /= weight_sum;
            sum -= n_i.dot(sum - v_i)*n_i;

            if (weight_sum > 0)
                O.col(i) = sum;
        }
    };

    Float error = 0.0f;
    for (const std::vector<uint32_t> &phase_ : phases) {
        tbb::blocked_range<uint32_t> range(0u, (uint32_t)phase_.size(), GRAIN_SIZE);
        phase = &phase_;
        if (mRes.frozenO())
            tbb::parallel_for(range, solve_frozen);
        else
            tbb::parallel_for(range, solve_normal);
        progress(phase_.size());
    }

    return error;
}

template <typename Functor>
static inline Float error_positions_impl(const MultiResolutionHierarchy &mRes,
                                         int level, Functor functor) {
    const AdjacencyMatrix &adj = mRes.adj(level);
    const MatrixXf &N = mRes.N(level), &Q = mRes.Q(level);
    const MatrixXf &O = mRes.O(level), &V = mRes.V(level);
    const Float scale = mRes.scale(), inv_scale = 1.0f / scale;

    auto map = [&](const tbb::blocked_range<uint32_t> &range, Float error) -> Float {
        for (uint32_t i = range.begin(); i<range.end(); ++i) {
            const Vector3f &n_i = N.col(i), &v_i = V.col(i), &o_i = O.col(i);
            Vector3f q_i = Q.col(i);
            #if 1
                q_i.normalize();
            #endif
            for (Link *link = adj[i]; link != adj[i+1]; ++link) {
                const uint32_t j = link->id;
                const Vector3f &n_j = N.col(j), &v_j = V.col(j), &o_j = O.col(j);
                Vector3f q_j = Q.col(j);

                #if 1
                    q_j.normalize();
                #endif

                std::pair<Vector3f, Vector3f> value = functor(
                    v_i, n_i, q_i, o_i, v_j, n_j, q_j, o_j, scale, inv_scale);

                error += (value.first-value.second).cast<double>().squaredNorm();
            }
        }
        return error;
    };

    auto reduce = [&](double error1, double error2) -> double {
        return error1 + error2;
    };

    double total = tbb::parallel_reduce(
        tbb::blocked_range<uint32_t>(0, mRes.size(level), GRAIN_SIZE), 0.0,
        map, reduce
    );
    return total / (double) (adj[mRes.size(level)] - adj[0]);
}

Float optimize_positions(MultiResolutionHierarchy &mRes, int level,
                         bool extrinsic, int posy,
                         const std::function<void(uint32_t)> &progress) {
    if (posy == 3) {
        if (extrinsic)
            return optimize_positions_impl(mRes, level, compat_position_extrinsic_3, position_round_3, progress);
        else
            return optimize_positions_impl(mRes, level, compat_position_intrinsic_3, position_round_3, progress);
    } else if (posy == 4) {
        if (extrinsic)
            return optimize_positions_impl(mRes, level, compat_position_extrinsic_4, position_round_4, progress);
        else
            return optimize_positions_impl(mRes, level, compat_position_intrinsic_4, position_round_4, progress);
    } else {
        throw std::runtime_error("Invalid position symmetry type " + std::to_string(posy) + "!");
    }
}

Float error_positions(MultiResolutionHierarchy &mRes, int level, bool extrinsic,
                      int posy) {
    if (posy == 3) {
        if (extrinsic)
            return error_positions_impl(mRes, level, compat_position_extrinsic_3);
        else
            return error_positions_impl(mRes, level, compat_position_intrinsic_3);
    } else if (posy == 4) {
        if (extrinsic)
            return error_positions_impl(mRes, level, compat_position_extrinsic_4);
        else
            return error_positions_impl(mRes, level, compat_position_intrinsic_4);
    } else {
        throw std::runtime_error("Invalid position symmetry type " + std::to_string(posy) + "!");
    }
}

template <int rosy, bool extrinsic, typename RotateFunctorRoSy,
          typename RotateShiftFunctor,
          typename CompatPositionIndex>
void compute_position_singularities(
    const MultiResolutionHierarchy &mRes,
    const std::map<uint32_t, uint32_t> &orient_sing,
    std::map<uint32_t, Vector2i> &pos_sing,
    RotateFunctorRoSy rotateFunctor_rosy,
    RotateShiftFunctor rshift, CompatPositionIndex compatPositionIndex) {
    const MatrixXf &V = mRes.V(), &N = mRes.N(), &Q = mRes.Q(), &O = mRes.O();
    const MatrixXu &F = mRes.F();
    tbb::spin_mutex mutex;
    pos_sing.clear();

    const Float scale = mRes.scale(), inv_scale = 1.0f / scale;

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) F.cols(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t f = range.begin(); f<range.end(); ++f) {
                if (orient_sing.find(f) != orient_sing.end())
                    continue;
                Vector2i index = Vector2i::Zero();
                uint32_t i0 = F(0, f), i1 = F(1, f), i2 = F(2, f);

                Vector3f q[3] = { Q.col(i0).normalized(), Q.col(i1).normalized(), Q.col(i2).normalized() };
                Vector3f n[3] = { N.col(i0), N.col(i1), N.col(i2) };
                Vector3f o[3] = { O.col(i0), O.col(i1), O.col(i2) };
                Vector3f v[3] = { V.col(i0), V.col(i1), V.col(i2) };

                int best[3];
                Float best_dp = -std::numeric_limits<double>::infinity();
                for (int i=0; i<rosy; ++i) {
                    Vector3f v0 = rotateFunctor_rosy(q[0], n[0], i);
                    for (int j=0; j<rosy; ++j) {
                        Vector3f v1 = rotateFunctor_rosy(q[1], n[1], j);
                        for (int k=0; k<rosy; ++k) {
                            Vector3f v2 = rotateFunctor_rosy(q[2], n[2], k);
                            Float dp = std::min(std::min(v0.dot(v1), v1.dot(v2)), v2.dot(v0));
                            if (dp > best_dp) {
                                best_dp = dp;
                                best[0] = i; best[1] = j; best[2] = k;
                            }
                        }
                    }
                }
                for (int k=0; k<3; ++k)
                    q[k] = rotateFunctor_rosy(q[k], n[k], best[k]);

                for (int k=0; k<3; ++k) {
                    int kn = k == 2 ? 0 : (k+1);

                    std::pair<Vector2i, Vector2i> value =
                        compatPositionIndex(
                            v[k],  n[k],  q[k],  o[k],
                            v[kn], n[kn], q[kn], o[kn],
                            scale, inv_scale, nullptr);

                    index += value.first - value.second;
                }

                if (index != Vector2i::Zero()) {
                    tbb::spin_mutex::scoped_lock lock(mutex);
                    pos_sing[f] = rshift(index, best[0]);
                }
            }
        }
    );
}

template <typename Functor>
static inline void freeze_ivars_positions_impl(MultiResolutionHierarchy &mRes,
                                               int level, Functor functor) {
    const AdjacencyMatrix &adj = mRes.adj(level);
    const MatrixXf &N = mRes.N(level), &Q = mRes.Q(level), &V = mRes.V(level), &O = mRes.O(level);
    const Float scale = mRes.scale(), inv_scale = 1.0f / scale;

    auto map = [&](const tbb::blocked_range<uint32_t> &range) {
        for (uint32_t i = range.begin(); i<range.end(); ++i) {
            const Vector3f n_i = N.col(i), v_i = V.col(i), o_i = O.col(i);
            Vector3f q_i = Q.col(i);
            #if 1
                q_i.normalize();
            #endif

            for (Link *link = adj[i]; link != adj[i+1]; ++link) {
                const uint32_t j = link->id;
                const Vector3f n_j = N.col(j), v_j = V.col(j);
                Vector3f q_j = Q.col(j), o_j = O.col(j);

                #if 1
                    q_j.normalize();
                #endif

                std::pair<Vector2i, Vector2i> value = functor(
                    v_i, n_i, q_i, o_i,
                    v_j, n_j, q_j, o_j,
                    scale, inv_scale, nullptr);

                link->ivar[0].translate_u = value.first.x();
                link->ivar[0].translate_v = value.first.y();
                link->ivar[1].translate_u = value.second.x();
                link->ivar[1].translate_v = value.second.y();
            }
        }
    };

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0, mRes.size(level), GRAIN_SIZE), map);

    mRes.setFrozenO(true);
}

void freeze_ivars_positions(MultiResolutionHierarchy &mRes, int level,
                         bool extrinsic, int posy) {
    if (posy != 4) /// only rosy=4 for now.
        return;
    if (posy == 3) {
        if (extrinsic)
            freeze_ivars_positions_impl(mRes, level, compat_position_extrinsic_index_3);
        else
            freeze_ivars_positions_impl(mRes, level, compat_position_intrinsic_index_3);
    } else if (posy == 4) {
        if (extrinsic)
            freeze_ivars_positions_impl(mRes, level, compat_position_extrinsic_index_4);
        else
            freeze_ivars_positions_impl(mRes, level, compat_position_intrinsic_index_4);
    } else {
        throw std::runtime_error("Invalid position symmetry type " + std::to_string(posy) + "!");
    }
}

void
compute_position_singularities(const MultiResolutionHierarchy &mRes,
                               const std::map<uint32_t, uint32_t> &orient_sing,
                               std::map<uint32_t, Vector2i> &pos_sing,
                               bool extrinsic, int rosy, int posy) {
    /* Some combinations don't make much sense, but let's support them anyways .. */
    if (rosy == 2) {
        if (posy == 3) {
            if (extrinsic)
                compute_position_singularities<2, true>(
                    mRes, orient_sing, pos_sing, rotate180_by, rshift180,
                    compat_position_extrinsic_index_3);
            else
                compute_position_singularities<2, false>(
                    mRes, orient_sing, pos_sing, rotate180_by, rshift180,
                    compat_position_intrinsic_index_3);
        } else if (posy == 4) {
            if (extrinsic)
                compute_position_singularities<2, true>(
                    mRes, orient_sing, pos_sing, rotate180_by, rshift180,
                    compat_position_extrinsic_index_4);
            else
                compute_position_singularities<2, false>(
                    mRes, orient_sing, pos_sing, rotate180_by, rshift180,
                    compat_position_intrinsic_index_4);
        } else {
            throw std::runtime_error(
                "compute_position_singularities: unsupported!");
        }
    } else if (rosy == 4) {
        if (posy == 3) {
            if (extrinsic)
                compute_position_singularities<4, true>(
                    mRes, orient_sing, pos_sing, rotate90_by, rshift90,
                    compat_position_extrinsic_index_3);
            else
                compute_position_singularities<4, false>(
                    mRes, orient_sing, pos_sing, rotate90_by, rshift90,
                    compat_position_intrinsic_index_3);
        } else if (posy == 4) {
            if (extrinsic)
                compute_position_singularities<4, true>(
                    mRes, orient_sing, pos_sing, rotate90_by, rshift90,
                    compat_position_extrinsic_index_4);
            else
                compute_position_singularities<4, false>(
                    mRes, orient_sing, pos_sing, rotate90_by, rshift90,
                    compat_position_intrinsic_index_4);
        } else {
            throw std::runtime_error(
                "compute_position_singularities: unsupported!");
        }
    } else if (rosy == 6) {
        if (posy == 3) {
            if (extrinsic)
                compute_position_singularities<6, true>(
                    mRes, orient_sing, pos_sing, rotate60_by, rshift60,
                    compat_position_extrinsic_index_3);
            else
                compute_position_singularities<6, false>(
                    mRes, orient_sing, pos_sing, rotate60_by, rshift60,
                    compat_position_intrinsic_index_3);
        } else if (posy == 4) {
            if (extrinsic)
                compute_position_singularities<6, true>(
                    mRes, orient_sing, pos_sing, rotate60_by, rshift60,
                    compat_position_extrinsic_index_4);
            else
                compute_position_singularities<6, false>(
                    mRes, orient_sing, pos_sing, rotate60_by, rshift60,
                    compat_position_intrinsic_index_4);
        } else {
            throw std::runtime_error("compute_position_singularities: unsupported!");
        }
    } else {
        throw std::runtime_error("compute_position_singularities: unsupported!");
    }
}

bool move_orientation_singularity(MultiResolutionHierarchy &mRes, uint32_t f_src, uint32_t f_target) {
    int edge_idx[2], found = 0;
    cout << "Moving orientation singularity from face " << f_src << " to " << f_target << endl;
    const MatrixXu &F = mRes.F();
    const MatrixXf &N = mRes.N(), &Q = mRes.Q();
    AdjacencyMatrix &adj = mRes.adj();

    for (int i=0; i<3; ++i)
        for (int j=0; j<3; ++j)
            if (F(i, f_src) == F(j, f_target))
                edge_idx[found++] = F(i, f_src);

    if (found != 2)
        throw std::runtime_error("move_orientation_singularity: invalid argument");

    int index = 0;
    for (int i=0; i<3; ++i) {
        uint32_t idx_cur = F(i, f_src), idx_next = F(i == 2 ? 0 : (i+1), f_src);
        const Link &l = search_adjacency(adj, idx_cur, idx_next);
        index += l.ivar[1].rot - l.ivar[0].rot;
    }

    index = modulo(index, 4);
    if (index == 0) {
        cout << "Warning: Starting point was not a singularity!" << endl;
        return false;
    } else {
        cout << "Singularity index is " << index << endl;
    }

    Link &l0 = search_adjacency(adj, edge_idx[0], edge_idx[1]);
    Link &l1 = search_adjacency(adj, edge_idx[1], edge_idx[0]);
    l1.ivar[0].rot = l0.ivar[1].rot;
    l1.ivar[1].rot = l0.ivar[0].rot;
    auto rotate = rotate90_by;

    Vector3f n0 = N.col(edge_idx[0]);
    Vector3f n1 = N.col(edge_idx[1]);
    Vector3f q0 = rotate(Q.col(edge_idx[0]).normalized(), n0, l0.ivar[0].rot);
    Vector3f q1 = rotate(Q.col(edge_idx[1]).normalized(), n1, l0.ivar[1].rot);

    Vector3f q0p = n0.cross(q0), q1p = n1.cross(q1);

    if (std::abs(q0p.dot(q1)) > std::abs(q1p.dot(q0)))
        l0.ivar[0].rot = l1.ivar[1].rot = modulo(l0.ivar[0].rot + (q0p.dot(q1) > 0 ? 1 : 3), 4);
    else
        l0.ivar[1].rot = l1.ivar[0].rot = modulo(l0.ivar[1].rot + (q1p.dot(q0) > 0 ? 1 : 3), 4);

    return true;
}

bool move_position_singularity(MultiResolutionHierarchy &mRes, uint32_t f_src, uint32_t f_target) {
    cout << "Moving position singularity from face " << f_src << " to " << f_target << endl;
    const MatrixXu &F = mRes.F();
    const MatrixXf &N = mRes.N(), &Q = mRes.Q();
    AdjacencyMatrix &adj = mRes.adj();

    auto rotate = rotate90_by;
    auto rshift = rshift90;
    int rosy = 4;

    Vector3f q[3] = { Q.col(F(0, f_src)).normalized(), Q.col(F(1, f_src)).normalized(), Q.col(F(2, f_src)).normalized() };
    Vector3f n[3] = { N.col(F(0, f_src)), N.col(F(1, f_src)), N.col(F(2, f_src)) };

    int best[3];
    Float best_dp = 0;
    for (int i=0; i<rosy; ++i) {
        Vector3f v0 = rotate(q[0], n[0], i);
        for (int j=0; j<rosy; ++j) {
            Vector3f v1 = rotate(q[1], n[1], j);
            for (int k=0; k<rosy; ++k) {
                Vector3f v2 = rotate(q[2], n[2], k);
                Float dp = std::min(std::min(v0.dot(v1), v1.dot(v2)), v2.dot(v0));
                if (dp > best_dp) {
                    best_dp = dp;
                    best[0] = i; best[1] = j; best[2] = k;
                }
            }
        }
    }

    for (int i=0; i<3; ++i)
        q[i] = rotate(q[i], n[i], best[i]);

    Vector2i index = Vector2i::Zero();
    for (int i=0; i<3; ++i) {
        int j = (i+1) % 3;
        Link &l0 = search_adjacency(adj, F(i, f_src), F(j, f_src));
        index += rshift(l0.ivar[1].shift(),  modulo(-best[j], 4)) -
                 rshift(l0.ivar[0].shift(),  modulo(-best[i], 4));
    }

    if (index == Vector2i::Zero()) {
        cout << "Warning: Starting point was not a singularity!" << endl;
        return false;
    } else if (index.array().abs().sum() != 1) {
        cout << "Warning: Starting point is a high-degree singularity " << index.transpose() << endl;
        return false;
    } else {
        cout << "Singularity index is " << index.transpose() << endl;
    }

    int index_f[2], found = 0;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (F(i, f_src) == F(j, f_target))
                index_f[found++] = i;

    if (found != 2)
        throw std::runtime_error("Internal error!");

    if (index_f[0] == 0 && index_f[1] == 2)
        std::swap(index_f[0], index_f[1]);

    Link &l0 = search_adjacency(adj, F(index_f[0], f_src), F(index_f[1], f_src));
    Link &l1 = search_adjacency(adj, F(index_f[1], f_src), F(index_f[0], f_src));

    if (l0.ivar[1].shift()  != l1.ivar[0].shift() ||
        l0.ivar[0].shift()  != l1.ivar[1].shift())
        throw std::runtime_error("Non-symmetry detected!");

    Vector2i delta_0 = rshift( index, best[index_f[0]]);
    Vector2i delta_1 = rshift(-index, best[index_f[1]]);

    int magnitude_0 = (l0.ivar[0].shift() + delta_0).cwiseAbs().maxCoeff();
    int magnitude_1 = (l0.ivar[1].shift() + delta_1).cwiseAbs().maxCoeff();

    if (magnitude_0 < magnitude_1) {
        Vector2i tmp = l0.ivar[0].shift() + delta_0;
        l0.ivar[0].setShift(tmp);
        l1.ivar[1].setShift(tmp);
    } else {
        Vector2i tmp = l0.ivar[1].shift() + delta_1;
        l0.ivar[1].setShift(tmp);
        l1.ivar[0].setShift(tmp);
    }

    index = Vector2i::Zero();
    for (int i=0; i<3; ++i) {
        int j = (i+1) % 3;
        Link &l = search_adjacency(adj, F(i, f_src), F(j, f_src));
        index += rshift(l.ivar[1].shift(), modulo(-best[j], 4)) -
                 rshift(l.ivar[0].shift(), modulo(-best[i], 4));
    }
    cout << "Afterwards = " << index.transpose() << endl;

    return true;
}

Optimizer::Optimizer(MultiResolutionHierarchy &mRes, bool interactive)
    : mRes(mRes), mRunning(true), mOptimizeOrientations(false),
      mOptimizePositions(false), mLevel(-1), mLevelIterations(0),
      mHierarchical(false), mRoSy(-1), mPoSy(-1), mExtrinsic(true),
      mInteractive(interactive), mLastUpdate(0.0f), mProgress(1.f) {
    mThread = std::thread(&Optimizer::run, this);
}

void Optimizer::save(Serializer &state) {
    state.set("running", mRunning);
    state.set("optimizeOrientations", mOptimizeOrientations);
    state.set("optimizePositions", mOptimizePositions);
    state.set("hierarchical", mHierarchical);
    state.set("progress", mProgress);
    state.set("extrinsic", mExtrinsic);
    state.set("levelIterations", mLevelIterations);
    state.set("rosy", mRoSy);
    state.set("posy", mPoSy);
    state.set("lastUpdate", mLastUpdate);
    state.set("level", mLevel);
}

void Optimizer::load(const Serializer &state) {
    state.get("running", mRunning);
    state.get("optimizeOrientations", mOptimizeOrientations);
    state.get("optimizePositions", mOptimizePositions);
    state.get("hierarchical", mHierarchical);
    state.get("progress", mProgress);
    state.get("extrinsic", mExtrinsic);
    state.get("levelIterations", mLevelIterations);
    state.get("rosy", mRoSy);
    state.get("posy", mPoSy);
    state.get("lastUpdate", mLastUpdate);
    state.get("level", mLevel);
}

void Optimizer::optimizeOrientations(int level) {
    if (level >= 0) {
        mLevel = level;
        mHierarchical = false;
    } else {
        mLevel = mRes.levels() - 1;
        mHierarchical = true;
    }
    if (level != 0)
        mRes.setFrozenQ(false);
    mLevelIterations = 0;
    mOptimizePositions = false;
    mOptimizeOrientations = true;
#ifdef VISUALIZE_ERROR
    mError.resize(0);
#endif
    mTimer.reset();
}

void Optimizer::optimizePositions(int level) {
    if (level >= 0) {
        mLevel = level;
        mHierarchical = false;
    } else {
        mLevel = mRes.levels() - 1;
        mHierarchical = true;
    }
    if (level != 0)
        mRes.setFrozenO(false);
    mLevelIterations = 0;
    mOptimizePositions = true;
    mOptimizeOrientations = false;
#ifdef VISUALIZE_ERROR
    mError.resize(0);
#endif
    mTimer.reset();
}

void Optimizer::wait() {
    std::lock_guard<ordered_lock> lock(mRes.mutex());
    while (mRunning && (mOptimizePositions || mOptimizeOrientations))
        mCond.wait(mRes.mutex());
}
extern int nprocs;

void Optimizer::run() {
    const int levelIterations = 6;
    uint32_t operations = 0;
    tbb::task_scheduler_init init(nprocs);

    auto progress = [&](uint32_t ops) {
        operations += ops;
        if (mHierarchical)
            mProgress = operations / (Float) (mRes.totalSize() * levelIterations);
        else
            mProgress = 1.f;
    };

    while (true) {
        std::lock_guard<ordered_lock> lock(mRes.mutex());
        while (mRunning && (mRes.levels() == 0 || (!mOptimizePositions && !mOptimizeOrientations)))
            mCond.wait(mRes.mutex());

        if (!mRunning)
            break;
        int level = mLevel;
        if (mLevelIterations++ == 0 && mHierarchical && level == mRes.levels() - 1)
            operations = 0;

        bool lastIterationAtLevel = mHierarchical &&
                                    mLevelIterations >= levelIterations;

        bool updateView = (mInteractive && mTimer.value() > 500) || !mHierarchical;
#ifdef VISUALIZE_ERROR
        updateView = true;
#endif

        Timer<> timer;

        if (mOptimizeOrientations) {
            optimize_orientations(mRes, level, mExtrinsic, mRoSy, progress);

            if (level > 0 && (lastIterationAtLevel || updateView)) {
                int targetLevel = updateView ? 0 : (level - 1);

                for (int i=level-1; i>=targetLevel; --i) {
                    const MatrixXf &srcField = mRes.Q(i + 1);
                    const MatrixXu &toUpper = mRes.toUpper(i);
                    MatrixXf &destField = mRes.Q(i);
                    const MatrixXf &N = mRes.N(i);
                    tbb::parallel_for(0u, (uint32_t) srcField.cols(), [&](uint32_t j) {
                        for (int k = 0; k<2; ++k) {
                            uint32_t dest = toUpper(k, j);
                            if (dest == INVALID)
                                continue;
                            Vector3f q = srcField.col(j), n = N.col(dest);
                            destField.col(dest) = q - n * n.dot(q);
                        }
                    });
                }
            }
            if (updateView || (level == 0 && lastIterationAtLevel)) {
                mRes.setIterationsQ(mRes.iterationsQ() + 1);
#ifdef VISUALIZE_ERROR
                const int max = 1000;
                if (mError.size() < max)
                    mError.conservativeResize(mError.size() + 1);
                else
                    mError.head(max-1) = mError.tail(max-1).eval();
                Float value = error_orientations(mRes, 0, mExtrinsic, mRoSy);
                mError[mError.size()-1] = value;
#endif
            }
        }
        if (mOptimizePositions) {
            optimize_positions(mRes, level, mExtrinsic, mPoSy, progress);

            if (level > 0 && (lastIterationAtLevel || updateView)) {
                int targetLevel = updateView ? 0 : (level - 1);

                for (int i=level-1; i>=targetLevel; --i) {
                    const MatrixXf &srcField = mRes.O(i + 1);
                    MatrixXf &destField = mRes.O(i);
                    const MatrixXf &N = mRes.N(i);
                    const MatrixXf &V = mRes.V(i);
                    const MatrixXu &toUpper = mRes.toUpper(i);
                    tbb::parallel_for(0u, (uint32_t) srcField.cols(), [&](uint32_t j) {
                        for (int k=0; k<2; ++k) {
                            uint32_t dest = toUpper(k, j);
                            if (dest == INVALID)
                                continue;
                            Vector3f o = srcField.col(j), n = N.col(dest), v = V.col(dest);
                            o -= n * n.dot(o-v);
                            destField.col(dest) = o;
                        }
                    });
                }
                if (targetLevel == 0)
                    mRes.setIterationsO(mRes.iterationsO() + 1);
            }
            if (updateView || (level == 0 && lastIterationAtLevel)) {
                mRes.setIterationsO(mRes.iterationsO() + 1);
#ifdef VISUALIZE_ERROR
                const int max = 1000;
                if (mError.size() < max)
                    mError.conservativeResize(mError.size() + 1);
                else
                    mError.head(max-1) = mError.tail(max-1).eval();
                Float value = error_positions(mRes, 0, mExtrinsic, mPoSy);
                mError[mError.size()-1] = value;
#endif
            }
        }

        if (mHierarchical && mLevelIterations >= levelIterations) {
            if (--mLevel < 0) {
                if (mOptimizeOrientations) {
                    if (!mRes.frozenQ())
                        freeze_ivars_orientations(mRes, 0, mExtrinsic, mRoSy);
                }
                if (mOptimizePositions) {
                    if (!mRes.frozenO())
                        freeze_ivars_positions(mRes, 0, mExtrinsic, mPoSy);
                }
                stop();
            }
            mLevelIterations = 0;
        }

        if (mAttractorStrokes.size() > 0 && mLevel == 0) {
            auto &value = mAttractorStrokes[mAttractorStrokes.size()-1];
            bool orientation = value.first;
            auto &stroke = value.second;
            if (stroke.size() < 2) {
                mAttractorStrokes.pop_back();
            } else {
                if (orientation) {
                    if (move_orientation_singularity(mRes, stroke[stroke.size()-1], stroke[stroke.size()-2])) {
                        stroke.pop_back();
                    } else {
                        mAttractorStrokes.pop_back();
                    }
                } else {
                    if (move_position_singularity(mRes, stroke[stroke.size()-1], stroke[stroke.size()-2])) {
                        stroke.pop_back();
                    } else {
                        mAttractorStrokes.pop_back();
                    }
                }
            }
            if (mAttractorStrokes.empty()) {
                mHierarchical = true;
                mLevelIterations = 0;
            }
        }
        if (updateView)
            mTimer.reset();
    }
}

