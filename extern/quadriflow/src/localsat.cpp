#ifdef NDEBUG
#undef NDEBUG
#endif

#include "localsat.hpp"
#include "config.hpp"
#include "dedge.hpp"
#include "field-math.hpp"

#include <Eigen/Core>

#include <deque>
#include <memory>
#include <utility>
#include <vector>

namespace qflow {

const int max_depth = 0;

using namespace Eigen;

SolverStatus RunCNF(const std::string &fin_name, int n_variable, int timeout,
                    const std::vector<std::vector<int>> &sat_clause, std::vector<int> &value) {
    int n_sat_variable = 3 * n_variable;
    auto fout_name = fin_name + ".result.txt";

    FILE *fout = fopen(fin_name.c_str(), "w");
    fprintf(fout, "p cnf %d %d\n", n_sat_variable, (int)sat_clause.size());
    for (auto &c : sat_clause) {
        for (auto e : c) fprintf(fout, "%d ", e);
        fputs("0\n", fout);
    }
    fclose(fout);

    char cmd[100];
    snprintf(cmd, 99, "rm %s > /dev/null 2>&1", fout_name.c_str());
    system(cmd);
    snprintf(cmd, 99, "timeout %d minisat %s %s > /dev/null 2>&1", timeout, fin_name.c_str(),
             fout_name.c_str());
    int exit_code = system(cmd);

    FILE *fin = fopen(fout_name.c_str(), "r");
    char buf[16] = {0};
    fscanf(fin, "%15s", buf);
    lprintf("  MiniSAT:");
    if (strcmp(buf, "SAT") != 0) {
        fclose(fin);

        if (exit_code == 124) {
            lprintf("       Timeout! ");
            return SolverStatus::Timeout;
        }
        lprintf(" Unsatisfiable! ");
        return SolverStatus::Unsat;
    };

    lprintf("   Satisfiable! ");
    for (int i = 0; i < n_variable; ++i) {
        int sign[3];
        fscanf(fin, "%d %d %d", sign + 0, sign + 1, sign + 2);

        int nvalue = -2;
        for (int j = 0; j < 3; ++j) {
            assert(abs(sign[j]) == 3 * i + j + 1);
            if ((sign[j] > 0) == (value[i] != j - 1)) {
                assert(nvalue == -2);
                nvalue = j - 1;
            }
        }
        value[i] = nvalue;
    }
    fclose(fin);

    return SolverStatus::Sat;
}

SolverStatus SolveSatProblem(int n_variable, std::vector<int> &value,
                             const std::vector<bool> flexible,  // NOQA
                             const std::vector<Vector3i> &variable_eq,
                             const std::vector<Vector3i> &constant_eq,
                             const std::vector<Vector4i> &variable_ge,
                             const std::vector<Vector2i> &constant_ge,
                             int timeout) {
    for (int v : value) assert(-1 <= v && v <= +1);

    auto VAR = [&](int i, int v) {
        int index = 1 + 3 * i + v + 1;
        // We initialize the SAT problem by setting all the variable to false.
        // This is because minisat by default will try false first.
        if (v == value[i]) index = -index;
        return index;
    };

    int n_flexible = 0;
    std::vector<std::vector<int>> sat_clause;
    std::vector<bool> sat_ishard;

    auto add_clause = [&](const std::vector<int> &clause, bool hard) {
        sat_clause.push_back(clause);
        sat_ishard.push_back(hard);
    };

    for (int i = 0; i < n_variable; ++i) {
        add_clause({-VAR(i, -1), -VAR(i, 0)}, true);
        add_clause({-VAR(i, +1), -VAR(i, 0)}, true);
        add_clause({-VAR(i, -1), -VAR(i, +1)}, true);
        add_clause({VAR(i, -1), VAR(i, 0), VAR(i, +1)}, true);
        if (!flexible[i]) {
            add_clause({VAR(i, value[i])}, true);
        } else {
            ++n_flexible;
        }
    }

    for (int i = 0; i < (int)variable_eq.size(); ++i) {
        auto &var = variable_eq[i];
        auto &cst = constant_eq[i];
        for (int v0 = -1; v0 <= 1; ++v0)
            for (int v1 = -1; v1 <= 1; ++v1)
                for (int v2 = -1; v2 <= 1; ++v2)
                    if (cst[0] * v0 + cst[1] * v1 + cst[2] * v2 != 0) {
                        add_clause({-VAR(var[0], v0), -VAR(var[1], v1), -VAR(var[2], v2)}, true);
                    }
    }

    for (int i = 0; i < (int)variable_ge.size(); ++i) {
        auto &var = variable_ge[i];
        auto &cst = constant_ge[i];
        for (int v0 = -1; v0 <= 1; ++v0)
            for (int v1 = -1; v1 <= 1; ++v1)
                for (int v2 = -1; v2 <= 1; ++v2)
                    for (int v3 = -1; v3 <= 1; ++v3)
                        if (cst[0] * v0 * v1 - cst[1] * v2 * v3 < 0) {
                            add_clause({-VAR(var[0], v0), -VAR(var[1], v1), -VAR(var[2], v2),
                                        -VAR(var[3], v3)},
                                       false);
                        }
    }

    int nflip_before = 0, nflip_after = 0;
    for (int i = 0; i < (int)variable_ge.size(); ++i) {
        auto &var = variable_ge[i];
        auto &cst = constant_ge[i];
        if (value[var[0]] * value[var[1]] * cst[0] - value[var[2]] * value[var[3]] * cst[1] < 0)
            nflip_before++;
    }

    lprintf("  [SAT] nvar: %6d nflip: %3d ", n_flexible * 2, nflip_before);
    auto rcnf = RunCNF("test.out", n_variable, timeout, sat_clause, value);

    for (int i = 0; i < (int)variable_eq.size(); ++i) {
        auto &var = variable_eq[i];
        auto &cst = constant_eq[i];
        assert(cst[0] * value[var[0]] + cst[1] * value[var[1]] + cst[2] * value[var[2]] == 0);
    }
    for (int i = 0; i < (int)variable_ge.size(); ++i) {
        auto &var = variable_ge[i];
        auto &cst = constant_ge[i];
        int area = value[var[0]] * value[var[1]] * cst[0] - value[var[2]] * value[var[3]] * cst[1];
        if (area < 0) ++nflip_after;
    }
    lprintf("nflip: %3d\n", nflip_after);
    return rcnf;
}

void ExportLocalSat(std::vector<Vector2i> &edge_diff, const std::vector<Vector3i> &face_edgeIds,
                    const std::vector<Vector3i> &face_edgeOrients, const MatrixXi &F,
                    const VectorXi &V2E, const VectorXi &E2E) {
    int flip_count = 0;
    int flip_count1 = 0;

    std::vector<int> value(2 * edge_diff.size());
    for (int i = 0; i < (int)edge_diff.size(); ++i) {
        value[2 * i + 0] = edge_diff[i][0];
        value[2 * i + 1] = edge_diff[i][1];
    }

    std::deque<std::pair<int, int>> Q;
    std::vector<bool> mark_vertex(V2E.size(), false);

    assert(F.cols() == (int)face_edgeIds.size());
    std::vector<Vector3i> variable_eq(face_edgeIds.size() * 2);
    std::vector<Vector3i> constant_eq(face_edgeIds.size() * 2);
    std::vector<Vector4i> variable_ge(face_edgeIds.size());
    std::vector<Vector2i> constant_ge(face_edgeIds.size());

    VectorXd face_area(F.cols());

    for (int i = 0; i < (int)face_edgeIds.size(); ++i) {
        Vector2i diff[3];
        Vector2i var[3];
        Vector2i cst[3];
        for (int j = 0; j < 3; ++j) {
            int edgeid = face_edgeIds[i][j];
            diff[j] = rshift90(edge_diff[edgeid], face_edgeOrients[i][j]);
            var[j] = rshift90(Vector2i(edgeid * 2 + 1, edgeid * 2 + 2), face_edgeOrients[i][j]);
            cst[j] = var[j].array().sign();
            var[j] = var[j].array().abs() - 1;
        }

        assert(diff[0] + diff[1] + diff[2] == Vector2i::Zero());
        variable_eq[2 * i + 0] = Vector3i(var[0][0], var[1][0], var[2][0]);
        constant_eq[2 * i + 0] = Vector3i(cst[0][0], cst[1][0], cst[2][0]);
        variable_eq[2 * i + 1] = Vector3i(var[0][1], var[1][1], var[2][1]);
        constant_eq[2 * i + 1] = Vector3i(cst[0][1], cst[1][1], cst[2][1]);

        face_area[i] = diff[0][0] * diff[1][1] - diff[0][1] * diff[1][0];
        if (face_area[i] < 0) {
            printf("[SAT] Face %d's area < 0\n", i);
            for (int j = 0; j < 3; ++j) {
                int v = F(j, i);
                if (mark_vertex[v]) continue;
                Q.push_back(std::make_pair(v, 0));
                mark_vertex[v] = true;
            }
            flip_count += 1;
        }
        variable_ge[i] = Vector4i(var[0][0], var[1][1], var[0][1], var[1][0]);
        constant_ge[i] = Vector2i(cst[0][0] * cst[1][1], cst[0][1] * cst[1][0]);
    }
    for (int i = 0; i < (int)variable_eq.size(); ++i) {
        auto &var = variable_eq[i];
        auto &cst = constant_eq[i];
        assert((0 <= var.array()).all());
        assert((var.array() < value.size()).all());
        assert(cst[0] * value[var[0]] + cst[1] * value[var[1]] + cst[2] * value[var[2]] == 0);
    }

    for (int i = 0; i < (int)variable_ge.size(); ++i) {
        auto &var = variable_ge[i];
        auto &cst = constant_ge[i];
        assert((0 <= variable_ge[i].array()).all());
        assert((variable_ge[i].array() < value.size()).all());
        if (value[var[0]] * value[var[1]] * cst[0] - value[var[2]] * value[var[3]] * cst[1] < 0) {
            assert(face_area[i] < 0);
            flip_count1++;
        }
    }
    assert(flip_count == flip_count1);

    // BFS
    printf("[SAT] Start BFS: Q.size() = %d\n", (int)Q.size());

    int mark_count = Q.size();
    while (!Q.empty()) {
        int vertex = Q.front().first;
        int depth = Q.front().second;
        Q.pop_front();
        mark_count++;
        int e0 = V2E(vertex);

        for (int e = e0;;) {
            int v = F((e + 1) % 3, e / 3);
            if (!mark_vertex[v]) {
                int undirected_edge_id = face_edgeIds[e / 3][e % 3];
                int undirected_edge_length = edge_diff[undirected_edge_id].array().abs().sum() > 0;
                int ndepth = depth + undirected_edge_length;
                if (ndepth <= max_depth) {
                    if (undirected_edge_length == 0)
                        Q.push_front(std::make_pair(v, ndepth));
                    else
                        Q.push_back(std::make_pair(v, ndepth));
                    mark_vertex[v] = true;
                }
            }
            e = dedge_next_3(E2E(e));
            if (e == e0) break;
        }
    }
    printf("[SAT] Mark %d vertices out of %d\n", mark_count, (int)V2E.size());

    std::vector<bool> flexible(value.size(), false);
    for (int i = 0; i < (int)face_edgeIds.size(); ++i) {
        for (int j = 0; j < 3; ++j) {
            int edgeid = face_edgeIds[i][j];
            if (mark_vertex[F(j, i)] || mark_vertex[F((j + 1) % 3, i)]) {
                flexible[edgeid * 2 + 0] = true;
                flexible[edgeid * 2 + 1] = true;
            } else {
                assert(face_area[i] >= 0);
            }
        }
    }

    SolveSatProblem(value.size(), value, flexible, variable_eq, constant_eq, variable_ge,
                    constant_ge);

    for (int i = 0; i < edge_diff.size(); ++i) {
        edge_diff[i][0] = value[2 * i + 0];
        edge_diff[i][1] = value[2 * i + 1];
    }
}

} // namespace qflow
