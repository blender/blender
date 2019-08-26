#include "subdivide.hpp"

#include <fstream>
#include <queue>

#include "dedge.hpp"
#include "disajoint-tree.hpp"
#include "field-math.hpp"
#include "parametrizer.hpp"

namespace qflow {

void subdivide(MatrixXi &F, MatrixXd &V, VectorXd& rho, VectorXi &V2E, VectorXi &E2E, VectorXi &boundary,
               VectorXi &nonmanifold, double maxLength) {
    typedef std::pair<double, int> Edge;

    std::priority_queue<Edge> queue;

    maxLength *= maxLength;

    for (int i = 0; i < E2E.size(); ++i) {
        int v0 = F(i % 3, i / 3), v1 = F((i + 1) % 3, i / 3);
        if (nonmanifold[v0] || nonmanifold[v1]) continue;
        double length = (V.col(v0) - V.col(v1)).squaredNorm();
        if (length > maxLength || length > std::max(maxLength * 0.75, std::min(rho[v0], rho[v1]) * 1.0)) {
            int other = E2E[i];
            if (other == -1 || other > i) queue.push(Edge(length, i));
        }
    }

    int nV = V.cols(), nF = F.cols(), nSplit = 0;
    /*
    /   v0  \
    v1p 1 | 0 v0p
    \   v1  /

    /   v0  \
    /  1 | 0  \
    v1p - vn - v0p
    \  2 | 3  /
    \   v1  /

    f0: vn, v0p, v0
    f1: vn, v0, v1p
    f2: vn, v1p, v1
    f3: vn, v1, v0p
    */
    int counter = 0;
    while (!queue.empty()) {
        counter += 1;
        Edge edge = queue.top();
        queue.pop();
        int e0 = edge.second, e1 = E2E[e0];
        bool is_boundary = e1 == -1;
        int f0 = e0 / 3, f1 = is_boundary ? -1 : (e1 / 3);
        int v0 = F(e0 % 3, f0), v0p = F((e0 + 2) % 3, f0), v1 = F((e0 + 1) % 3, f0);
        if ((V.col(v0) - V.col(v1)).squaredNorm() != edge.first) {
            continue;
        }
        int v1p = is_boundary ? -1 : F((e1 + 2) % 3, f1);
        int vn = nV++;
        nSplit++;
        /* Update V */
        if (nV > V.cols()) {
            V.conservativeResize(V.rows(), V.cols() * 2);
            rho.conservativeResize(V.cols() * 2);
            V2E.conservativeResize(V.cols());
            boundary.conservativeResize(V.cols());
            nonmanifold.conservativeResize(V.cols());
        }

        /* Update V */
        V.col(vn) = (V.col(v0) + V.col(v1)) * 0.5f;
        rho[vn] = 0.5f * (rho[v0], rho[v1]);
        nonmanifold[vn] = false;
        boundary[vn] = is_boundary;

        /* Update F and E2E */
        int f2 = is_boundary ? -1 : (nF++);
        int f3 = nF++;
        if (nF > F.cols()) {
            F.conservativeResize(F.rows(), std::max(nF, (int)F.cols() * 2));
            E2E.conservativeResize(F.cols() * 3);
        }

        /* Update F */
        F.col(f0) << vn, v0p, v0;
        if (!is_boundary) {
            F.col(f1) << vn, v0, v1p;
            F.col(f2) << vn, v1p, v1;
        }
        F.col(f3) << vn, v1, v0p;

        /* Update E2E */
        const int e0p = E2E[dedge_prev_3(e0)], e0n = E2E[dedge_next_3(e0)];

#define sE2E(a, b) \
    E2E[a] = b;    \
    if (b != -1) E2E[b] = a;
        sE2E(3 * f0 + 0, 3 * f3 + 2);
        sE2E(3 * f0 + 1, e0p);
        sE2E(3 * f3 + 1, e0n);
        if (is_boundary) {
            sE2E(3 * f0 + 2, -1);
            sE2E(3 * f3 + 0, -1);
        } else {
            const int e1p = E2E[dedge_prev_3(e1)], e1n = E2E[dedge_next_3(e1)];
            sE2E(3 * f0 + 2, 3 * f1 + 0);
            sE2E(3 * f1 + 1, e1n);
            sE2E(3 * f1 + 2, 3 * f2 + 0);
            sE2E(3 * f2 + 1, e1p);
            sE2E(3 * f2 + 2, 3 * f3 + 0);
        }
#undef sE2E

        /* Update V2E */
        V2E[v0] = 3 * f0 + 2;
        V2E[vn] = 3 * f0 + 0;
        V2E[v1] = 3 * f3 + 1;
        V2E[v0p] = 3 * f0 + 1;
        if (!is_boundary) V2E[v1p] = 3 * f1 + 2;

        auto schedule = [&](int f) {
            for (int i = 0; i < 3; ++i) {
                double length = (V.col(F(i, f)) - V.col(F((i + 1) % 3, f))).squaredNorm();
                if (length > maxLength
                    || length > std::max(maxLength * 0.75, std::min(rho[F(i, f)], rho[F((i + 1) % 3, f)]) * 1.0))
                    queue.push(Edge(length, f * 3 + i));
            }
        };

        schedule(f0);
        if (!is_boundary) {
            schedule(f2);
            schedule(f1);
        };
        schedule(f3);
    }
    F.conservativeResize(F.rows(), nF);
    V.conservativeResize(V.rows(), nV);
    rho.conservativeResize(nV);
    V2E.conservativeResize(nV);
    boundary.conservativeResize(nV);
    nonmanifold.conservativeResize(nV);
    E2E.conservativeResize(nF * 3);
}

void subdivide_edgeDiff(MatrixXi &F, MatrixXd &V, MatrixXd &N, MatrixXd &Q, MatrixXd &O, MatrixXd* S,
                        VectorXi &V2E, VectorXi &E2E, VectorXi &boundary, VectorXi &nonmanifold,
                        std::vector<Vector2i> &edge_diff, std::vector<DEdge> &edge_values,
                        std::vector<Vector3i> &face_edgeOrients, std::vector<Vector3i> &face_edgeIds,
                        std::vector<int>& sharp_edges, std::map<int, int> &singularities, int max_len) {
    struct EdgeLink {
        int id;
        double length;
        Vector2i diff;
        int maxlen() const { return std::max(abs(diff[0]), abs(diff[1])); }
        bool operator<(const EdgeLink &link) const { return maxlen() < link.maxlen(); }
    };

    struct FaceOrient {
        int orient;
        Vector3i d;
        Vector3d q;
        Vector3d n;
    };

    std::vector<FaceOrient> face_spaces(F.cols());
    std::priority_queue<EdgeLink> queue;
    std::vector<Vector2i> diffs(E2E.size());
    for (int i = 0; i < F.cols(); ++i) {
        for (int j = 0; j < 3; ++j) {
            int eid = i * 3 + j;
            diffs[eid] = rshift90(edge_diff[face_edgeIds[i][j]], face_edgeOrients[i][j]);
        }
    }
    for (int i = 0; i < F.cols(); ++i) {
        FaceOrient orient{};
        orient.q = Q.col(F(0, i));
        orient.n = N.col(F(0, i));
        int orient_diff[3];
        for (int j = 0; j < 3; ++j) {
            int final_orient = face_edgeOrients[i][j];
            int eid = face_edgeIds[i][j];
            auto value = compat_orientation_extrinsic_index_4(
                Q.col(edge_values[eid].x), N.col(edge_values[eid].x), orient.q, orient.n);
            int target_orient = (value.second - value.first + 4) % 4;
            if (F(j, i) == edge_values[eid].y) target_orient = (target_orient + 2) % 4;
            orient_diff[j] = (final_orient - target_orient + 4) % 4;
        }
        if (orient_diff[0] == orient_diff[1])
            orient.orient = orient_diff[0];
        else if (orient_diff[0] == orient_diff[2])
            orient.orient = orient_diff[2];
        else if (orient_diff[1] == orient_diff[2])
            orient.orient = orient_diff[1];
        orient.d = Vector3i((orient_diff[0] - orient.orient + 4) % 4,
                            (orient_diff[1] - orient.orient + 4) % 4,
                            (orient_diff[2] - orient.orient + 4) % 4);
        face_spaces[i] = (orient);
    }
    for (int i = 0; i < E2E.size(); ++i) {
        int v0 = F(i % 3, i / 3), v1 = F((i + 1) % 3, i / 3);
        if (nonmanifold[v0] || nonmanifold[v1]) continue;
        double length = (V.col(v0) - V.col(v1)).squaredNorm();
        Vector2i diff = diffs[i];
        if (abs(diff[0]) > max_len || abs(diff[1]) > max_len) {
            int other = E2E[i];
            if (other == -1 || other > i) {
                EdgeLink e;
                e.id = i;
                e.length = length;
                e.diff = diff;
                queue.push(e);
            }
        }
    }
    auto AnalyzeOrient = [&](int f0, const Vector3i &d) {
        for (int j = 0; j < 3; ++j) {
            int orient = face_spaces[f0].orient + d[j];
            int v = std::min(F(j, f0), F((j + 1) % 3, f0));
            auto value = compat_orientation_extrinsic_index_4(
                Q.col(v), N.col(v), face_spaces[f0].q, face_spaces[f0].n);
            if (F(j, f0) != v) orient += 2;
            face_edgeOrients[f0][j] = (orient + value.second - value.first + 4) % 4;
        }
        face_spaces[f0].d = d;
        for (int j = 0; j < 3; ++j) {
            int eid = face_edgeIds[f0][j];
            int orient = face_edgeOrients[f0][j];
            auto diff = rshift90(diffs[f0 * 3 + j], (4 - orient) % 4);
            edge_diff[eid] = diff;
        }
    };
    auto FixOrient = [&](int f0) {
        for (int j = 0; j < 3; ++j) {
            auto diff = edge_diff[face_edgeIds[f0][j]];
            if (rshift90(diff, face_edgeOrients[f0][j]) != diffs[f0 * 3 + j]) {
                int orient = 0;
                while (orient < 4 && rshift90(diff, orient) != diffs[f0 * 3 + j]) orient += 1;
                face_spaces[f0].d[j] =
                    (face_spaces[f0].d[j] + orient - face_edgeOrients[f0][j]) % 4;
                face_edgeOrients[f0][j] = orient;
            }
        }
    };
    /*
    auto Length = [&](int f0) {
        int l = 0;
        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 2; ++k) {
                l += abs(diffs[f0*3+j][k]);
            }
            printf("<%d %d> ", diffs[f0*3+j][0], diffs[f0*3+j][1]);
        }
        printf("\n");
        return l;
    };
    */
    int nV = V.cols(), nF = F.cols(), nSplit = 0;
    /*
     /   v0  \
     v1p 1 | 0 v0p
     \   v1  /

     /   v0  \
     /  1 | 0  \
     v1p - vn - v0p
     \  2 | 3  /
     \   v1  /

     f0: vn, v0p, v0
     f1: vn, v0, v1p
     f2: vn, v1p, v1
     f3: vn, v1, v0p
     */
    int counter = 0;
    while (!queue.empty()) {
        counter += 1;
        EdgeLink edge = queue.top();
        queue.pop();

        int e0 = edge.id, e1 = E2E[e0];
        bool is_boundary = e1 == -1;
        int f0 = e0 / 3, f1 = is_boundary ? -1 : (e1 / 3);
        int v0 = F(e0 % 3, f0), v0p = F((e0 + 2) % 3, f0), v1 = F((e0 + 1) % 3, f0);
        if ((V.col(v0) - V.col(v1)).squaredNorm() != edge.length) {
            continue;
        }
        if (abs(diffs[e0][0]) < 2 && abs(diffs[e0][1]) < 2) continue;
        if (f1 != -1) {
            face_edgeOrients.push_back(Vector3i());
            sharp_edges.push_back(0);
            sharp_edges.push_back(0);
            sharp_edges.push_back(0);
            face_edgeIds.push_back(Vector3i());
        }
        int v1p = is_boundary ? -1 : F((e1 + 2) % 3, f1);
        int vn = nV++;
        nSplit++;
        if (nV > V.cols()) {
            V.conservativeResize(V.rows(), V.cols() * 2);
            N.conservativeResize(N.rows(), N.cols() * 2);
            Q.conservativeResize(Q.rows(), Q.cols() * 2);
            O.conservativeResize(O.rows(), O.cols() * 2);
            if (S)
                S->conservativeResize(S->rows(), S->cols() * 2);
            V2E.conservativeResize(V.cols());
            boundary.conservativeResize(V.cols());
            nonmanifold.conservativeResize(V.cols());
        }

        V.col(vn) = (V.col(v0) + V.col(v1)) * 0.5;
        N.col(vn) = N.col(v0);
        Q.col(vn) = Q.col(v0);
        O.col(vn) = (O.col(v0) + O.col(v1)) * 0.5;
        if (S)
            S->col(vn) = S->col(v0);
        
        nonmanifold[vn] = false;
        boundary[vn] = is_boundary;

        int eid = face_edgeIds[f0][e0 % 3];
        int sharp_eid = sharp_edges[e0];
        int eid01 = face_edgeIds[f0][(e0 + 1) % 3];
        int sharp_eid01 = sharp_edges[f0 * 3 + (e0 + 1) % 3];
        int eid02 = face_edgeIds[f0][(e0 + 2) % 3];
        int sharp_eid02 = sharp_edges[f0 * 3 + (e0 + 2) % 3];

        int eid0, eid1, eid0p, eid1p;
        int sharp_eid0, sharp_eid1, sharp_eid0p, sharp_eid1p;

        eid0 = eid;
        sharp_eid0 = sharp_eid;
        edge_values[eid0] = DEdge(v0, vn);

        eid1 = edge_values.size();
        sharp_eid1 = sharp_eid;
        edge_values.push_back(DEdge(vn, v1));
        edge_diff.push_back(Vector2i());

        eid0p = edge_values.size();
        sharp_eid0p = 0;
        edge_values.push_back(DEdge(vn, v0p));
        edge_diff.push_back(Vector2i());

        int f2 = is_boundary ? -1 : (nF++);
        int f3 = nF++;
        sharp_edges.push_back(0);
        sharp_edges.push_back(0);
        sharp_edges.push_back(0);
        face_edgeIds.push_back(Vector3i());
        face_edgeOrients.push_back(Vector3i());

        if (nF > F.cols()) {
            F.conservativeResize(F.rows(), std::max(nF, (int)F.cols() * 2));
            face_spaces.resize(F.cols());
            E2E.conservativeResize(F.cols() * 3);
            diffs.resize(F.cols() * 3);
        }

        auto D01 = diffs[e0];
        auto D1p = diffs[e0 / 3 * 3 + (e0 + 1) % 3];
        auto Dp0 = diffs[e0 / 3 * 3 + (e0 + 2) % 3];
        
        Vector2i D0n = D01 / 2;

        auto orients1 = face_spaces[f0];
        F.col(f0) << vn, v0p, v0;
        face_edgeIds[f0] = Vector3i(eid0p, eid02, eid0);
        sharp_edges[f0 * 3] = sharp_eid0p;
        sharp_edges[f0 * 3 + 1] = sharp_eid02;
        sharp_edges[f0 * 3 + 2] = sharp_eid0;
        
        diffs[f0 * 3] = D01 + D1p - D0n;
        diffs[f0 * 3 + 1] = Dp0;
        diffs[f0 * 3 + 2] = D0n;
        int o1 = e0 % 3, o2 = e1 % 3;
        AnalyzeOrient(f0, Vector3i(0, orients1.d[(o1 + 2) % 3], orients1.d[o1]));
        if (!is_boundary) {
            auto orients2 = face_spaces[f1];
            int eid11 = face_edgeIds[f1][(e1 + 1) % 3];
            int sharp_eid11 = sharp_edges[f1 * 3 + (e1 + 1) % 3];
            int eid12 = face_edgeIds[f1][(e1 + 2) % 3];
            int sharp_eid12 = sharp_edges[f1 * 3 + (e1 + 2) % 3];

            auto Ds10 = diffs[e1];
            auto Ds0p = diffs[e1 / 3 * 3 + (e1 + 1) % 3];
            
            auto Dsp1 = diffs[e1 / 3 * 3 + (e1 + 2) % 3];
            int orient = 0;
            while (rshift90(D01, orient) != Ds10) orient += 1;
            Vector2i Dsn0 = rshift90(D0n, orient);
            
            F.col(f1) << vn, v0, v1p;
            eid1p = edge_values.size();
            sharp_eid1p = 0;
            edge_values.push_back(DEdge(vn, v1p));
            edge_diff.push_back(Vector2i());

            sharp_edges[f1 * 3] = sharp_eid0;
            sharp_edges[f1 * 3 + 1] = sharp_eid11;
            sharp_edges[f1 * 3 + 2] = sharp_eid1p;
            face_edgeIds[f1] = (Vector3i(eid0, eid11, eid1p));
            diffs[f1 * 3] = Dsn0;
            diffs[f1 * 3 + 1] = Ds0p;
            diffs[f1 * 3 + 2] = Dsp1 + (Ds10 - Dsn0);

            AnalyzeOrient(f1, Vector3i(orients2.d[o2], orients2.d[(o2 + 1) % 3], 0));

            face_spaces[f2] = face_spaces[f1];
            sharp_edges[f2 * 3] = sharp_eid1p;
            sharp_edges[f2 * 3 + 1] = sharp_eid12;
            sharp_edges[f2 * 3 + 2] = sharp_eid1;
            face_edgeIds[f2] = (Vector3i(eid1p, eid12, eid1));
            F.col(f2) << vn, v1p, v1;
            diffs[f2 * 3] = -Dsp1 - (Ds10 - Dsn0);
            diffs[f2 * 3 + 1] = Dsp1;
            diffs[f2 * 3 + 2] = Ds10 - Dsn0;

            AnalyzeOrient(f2, Vector3i(0, orients2.d[(o2 + 2) % 3], orients2.d[o2]));
        }
        face_spaces[f3] = face_spaces[f0];
        sharp_edges[f3 * 3] = sharp_eid1;
        sharp_edges[f3 * 3 + 1] = sharp_eid01;
        sharp_edges[f3 * 3 + 2] = sharp_eid0p;
        face_edgeIds[f3] = (Vector3i(eid1, eid01, eid0p));
        F.col(f3) << vn, v1, v0p;
        diffs[f3 * 3] = D01 - D0n;
        diffs[f3 * 3 + 1] = D1p;
        diffs[f3 * 3 + 2] = D0n - (D01 + D1p);

        AnalyzeOrient(f3, Vector3i(orients1.d[o1], orients1.d[(o1 + 1) % 3], 0));

        FixOrient(f0);
        if (!is_boundary) {
            FixOrient(f1);
            FixOrient(f2);
        }
        FixOrient(f3);

        const int e0p = E2E[dedge_prev_3(e0)], e0n = E2E[dedge_next_3(e0)];

#define sE2E(a, b) \
    E2E[a] = b;    \
    if (b != -1) E2E[b] = a;
        sE2E(3 * f0 + 0, 3 * f3 + 2);
        sE2E(3 * f0 + 1, e0p);
        sE2E(3 * f3 + 1, e0n);
        if (is_boundary) {
            sE2E(3 * f0 + 2, -1);
            sE2E(3 * f3 + 0, -1);
        } else {
            const int e1p = E2E[dedge_prev_3(e1)], e1n = E2E[dedge_next_3(e1)];
            sE2E(3 * f0 + 2, 3 * f1 + 0);
            sE2E(3 * f1 + 1, e1n);
            sE2E(3 * f1 + 2, 3 * f2 + 0);
            sE2E(3 * f2 + 1, e1p);
            sE2E(3 * f2 + 2, 3 * f3 + 0);
        }
#undef sE2E

        V2E[v0] = 3 * f0 + 2;
        V2E[vn] = 3 * f0 + 0;
        V2E[v1] = 3 * f3 + 1;
        V2E[v0p] = 3 * f0 + 1;
        if (!is_boundary) V2E[v1p] = 3 * f1 + 2;

        auto schedule = [&](int f) {
            for (int i = 0; i < 3; ++i) {
                if (abs(diffs[f * 3 + i][0]) > max_len || abs(diffs[f * 3 + i][1]) > max_len) {
                    EdgeLink e;
                    e.id = f * 3 + i;
                    e.length = (V.col(F((i + 1) % 3, f)) - V.col(F(i, f))).squaredNorm();
                    e.diff = diffs[f * 3 + i];
                    queue.push(e);
                }
            }
        };

        schedule(f0);
        if (!is_boundary) {
            schedule(f2);
            schedule(f1);
        };
        schedule(f3);
    }
    F.conservativeResize(F.rows(), nF);
    V.conservativeResize(V.rows(), nV);
    N.conservativeResize(V.rows(), nV);
    Q.conservativeResize(V.rows(), nV);
    O.conservativeResize(V.rows(), nV);
    if (S)
        S->conservativeResize(S->rows(), nV);
    V2E.conservativeResize(nV);
    boundary.conservativeResize(nV);
    nonmanifold.conservativeResize(nV);
    E2E.conservativeResize(nF * 3);
    for (int i = 0; i < F.cols(); ++i) {
        for (int j = 0; j < 3; ++j) {
            auto diff = edge_diff[face_edgeIds[i][j]];
            if (abs(diff[0]) > 1 || abs(diff[1]) > 1) {
                printf("wrong init %d %d!\n", face_edgeIds[i][j], i * 3 + j);
                exit(0);
            }
        }
    }
    for (int i = 0; i < edge_diff.size(); ++i) {
        if (abs(edge_diff[i][0]) > 1 || abs(edge_diff[i][1]) > 1) {
            printf("wrong...\n");
            exit(0);
        }
    }
}

} // namespace qflow
