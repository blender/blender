//
//  post-solver.cpp
//  parametrize
//
//  Created by Jingwei on 2/5/18.
//
#include <algorithm>
#include <boost/program_options.hpp>
#include <cmath>
#include <cstdio>
#include <string>

#include "ceres/ceres.h"
#include "ceres/rotation.h"

#include "post-solver.hpp"
#include "serialize.hpp"

namespace qflow {

/// Coefficient of area constraint.  The magnitude is 1 if area is equal to 0.
const double COEFF_AREA = 1;
/// Coefficient of tangent constraint.  The magnitude is 0.03 if the bais is reference_length.
/// This is because current tangent constraint is not very accurate.
/// This optimization conflicts with COEFF_AREA.
const double COEFF_TANGENT = 0.02;
/// Coefficient of normal constraint.  The magnitude is the arc angle.
const double COEFF_NORMAL = 1;
/// Coefficient of normal constraint.  The magnitude is the arc angle.
const double COEFF_FLOW = 1;
/// Coefficient of orthogonal edge.  The magnitude is the arc angle.
const double COEFF_ORTH = 1;
/// Coefficient of edge length.  The magnitude is the arc angle.
const double COEFF_LENGTH = 1;
/// Number of iterations of the CGNR solver
const int N_ITER = 100;

template <typename T, typename T2>
T DotProduct(const T a[3], const T2 b[3]) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

template <typename T>
T Length2(const T a[3]) {
    return DotProduct(a, a);
}

namespace ceres {
inline double min(const double f, const double g) { return std::min(f, g); }

template <typename T, int N>
inline Jet<T, N> min(const Jet<T, N>& f, const Jet<T, N>& g) {
    if (f.a < g.a)
        return f;
    else
        return g;
}
}  // namespace ceres

bool DEBUG = 0;
struct FaceConstraint {
    FaceConstraint(double coeff_area, double coeff_normal, double coeff_flow, double coeff_orth,
                   double length, Vector3d normal[4], Vector3d Q0[4], Vector3d Q1[4])
        : coeff_area(coeff_area),
          coeff_normal(coeff_normal),
          coeff_flow(coeff_flow),
          coeff_orth(coeff_orth),
          area0(length * length),
          normal0{
              normal[0],
              normal[1],
              normal[2],
              normal[3],
          },
          Q0{Q0[0], Q0[1], Q0[2], Q0[3]},
          Q1{Q1[0], Q1[1], Q1[2], Q1[3]} {}

    template <typename T>
    bool operator()(const T* p0, const T* p1, const T* p2, const T* p3, T* r) const {
        const T* p[] = {p0, p1, p2, p3};
        r[12] = T();
        for (int k = 0; k < 4; ++k) {
            auto pc = p[k];
            auto pa = p[(k + 1) % 4];
            auto pb = p[(k + 3) % 4];

            T a[3]{pa[0] - pc[0], pa[1] - pc[1], pa[2] - pc[2]};
            T b[3]{pb[0] - pc[0], pb[1] - pc[1], pb[2] - pc[2]};

            T length_a = ceres::sqrt(Length2(a));
            T length_b = ceres::sqrt(Length2(b));
            T aa[3]{a[0] / length_a, a[1] / length_a, a[2] / length_a};
            T bb[3]{b[0] / length_b, b[1] / length_b, b[2] / length_b};
            r[3 * k + 0] = coeff_orth * DotProduct(aa, bb);

            T degree_edge0 = ceres::abs(DotProduct(aa, &Q0[k][0]));
            T degree_edge1 = ceres::abs(DotProduct(aa, &Q1[k][0]));
            T degree_edge = ceres::min(degree_edge0, degree_edge1);
            r[3 * k + 1] = coeff_flow * degree_edge;

            T normal[3];
            ceres::CrossProduct(a, b, normal);
            T area = ceres::sqrt(Length2(normal));
            r[12] += area;

            assert(area != T());
            for (int i = 0; i < 3; ++i) normal[i] /= area;
            T degree_normal = DotProduct(normal, &normal0[k][0]) - T(1);
            r[3 * k + 2] = coeff_normal * degree_normal * degree_normal;
        }
        r[12] = coeff_area * (r[12] / (4.0 * area0) - 1.0);
        return true;
    }

    static ceres::CostFunction* create(double coeff_area, double coeff_normal, double coeff_flow,
                                       double coeff_orth, double length, Vector3d normal[4],
                                       Vector3d Q0[4], Vector3d Q1[4]) {
        return new ceres::AutoDiffCostFunction<FaceConstraint, 13, 3, 3, 3, 3>(new FaceConstraint(
            coeff_area, coeff_normal, coeff_flow, coeff_orth, length, normal, Q0, Q1));
    }

    double coeff_area;
    double coeff_normal;
    double coeff_flow;
    double coeff_orth;

    double area0;
    Vector3d normal0[4];
    Vector3d Q0[4], Q1[4];
};

struct VertexConstraint {
    VertexConstraint(double coeff_tangent, Vector3d normal, double bias, double length)
        : coeff{coeff_tangent / length * 10}, bias0{bias}, normal0{normal} {}

    template <typename T>
    bool operator()(const T* p, T* r) const {
        r[0] = coeff * (DotProduct(p, &normal0[0]) - bias0);
        return true;
    }

    static ceres::CostFunction* create(double coeff_tangent, Vector3d normal, double bias,
                                       double length) {
        return new ceres::AutoDiffCostFunction<VertexConstraint, 1, 3>(
            new VertexConstraint(coeff_tangent, normal, bias, length));
    }

    double coeff;
    double bias0;
    Vector3d normal0;
};

void solve(std::vector<Vector3d>& O_quad, std::vector<Vector3d>& N_quad,
           std::vector<Vector3d>& Q_quad, std::vector<Vector4i>& F_quad,
           std::vector<double>& B_quad, MatrixXd& V, MatrixXd& N, MatrixXd& Q, MatrixXd& O,
           MatrixXi& F, double reference_length, double coeff_area, double coeff_tangent,
           double coeff_normal, double coeff_flow, double coeff_orth) {
    printf("Parameter:  \n");
    printf("    coeff_area:      %.4f\n", coeff_area);
    printf("    coeff_tangent:   %.4f\n", coeff_tangent);
    printf("    coeff_normal:    %.4f\n", coeff_normal);
    printf("    coeff_flow:      %.4f\n", coeff_flow);
    printf("    coeff_orth:      %.4f\n\n", coeff_orth);
    int n_quad = Q_quad.size();

    ceres::Problem problem;
    std::vector<double> solution(n_quad * 3);
    for (int vquad = 0; vquad < n_quad; ++vquad) {
        solution[3 * vquad + 0] = O_quad[vquad][0];
        solution[3 * vquad + 1] = O_quad[vquad][1];
        solution[3 * vquad + 2] = O_quad[vquad][2];
    }

    // Face constraint (area and normal direction)
    for (int fquad = 0; fquad < F_quad.size(); ++fquad) {
        auto v = F_quad[fquad];
        Vector3d normal[4], Q0[4], Q1[4];
        for (int k = 0; k < 4; ++k) {
            normal[k] = N_quad[v[k]];
            Q0[k] = Q_quad[v[k]];
            Q1[k] = Q0[k].cross(normal[k]).normalized();
        }
        ceres::CostFunction* cost_function = FaceConstraint::create(
            coeff_area, coeff_normal, coeff_flow, coeff_orth, reference_length, normal, Q0, Q1);
        problem.AddResidualBlock(cost_function, nullptr, &solution[3 * v[0]], &solution[3 * v[1]],
                                 &solution[3 * v[2]], &solution[3 * v[3]]);
    }

    // Tangent constraint
    for (int vquad = 0; vquad < O_quad.size(); ++vquad) {
        ceres::CostFunction* cost_function = VertexConstraint::create(
            coeff_tangent, N_quad[vquad], B_quad[vquad], reference_length);
        problem.AddResidualBlock(cost_function, nullptr, &solution[3 * vquad]);
    }

    // Flow constraint

    ceres::Solver::Options options;
    options.num_threads = 1;
    options.max_num_iterations = N_ITER;
    options.initial_trust_region_radius = 1;
    options.linear_solver_type = ceres::CGNR;
    options.minimizer_progress_to_stdout = true;
    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);

    std::cout << summary.BriefReport() << std::endl;

    for (int vquad = 0; vquad < n_quad; ++vquad) {
        O_quad[vquad][0] = solution[3 * vquad + 0];
        O_quad[vquad][1] = solution[3 * vquad + 1];
        O_quad[vquad][2] = solution[3 * vquad + 2];
    }

    return;
}

void optimize_quad_positions(std::vector<Vector3d>& O_quad, std::vector<Vector3d>& N_quad,
                             std::vector<Vector3d>& Q_quad, std::vector<Vector4i>& F_quad,
                             VectorXi& V2E_quad, std::vector<int>& E2E_quad, MatrixXd& V,
                             MatrixXd& N, MatrixXd& Q, MatrixXd& O, MatrixXi& F, VectorXi& V2E,
                             VectorXi& E2E, DisajointTree& disajoint_tree, double reference_length,
                             bool just_serialize) {
    printf("Quad mesh info:\n");
    printf("Number of vertices with normals and orientations: %d = %d = %d\n", (int)O_quad.size(),
           (int)N_quad.size(), (int)Q_quad.size());
    printf("Number of faces: %d\n", (int)F_quad.size());
    printf("Number of directed edges: %d\n", (int)E2E_quad.size());
    // Information for the original mesh
    printf("Triangle mesh info:\n");
    printf(
        "Number of vertices with normals, "
        "orientations and associated quad positions: "
        "%d = %d = %d = %d\n",
        (int)V.cols(), (int)N.cols(), (int)Q.cols(), (int)O.cols());
    printf("Number of faces: %d\n", (int)F.cols());
    printf("Number of directed edges: %d\n", (int)E2E.size());
    printf("Reference length: %.2f\n", reference_length);

    int flip_count = 0;
    for (int i = 0; i < F_quad.size(); ++i) {
        bool flipped = false;
        for (int j = 0; j < 4; ++j) {
            int v1 = F_quad[i][j];
            int v2 = F_quad[i][(j + 1) % 4];
            int v3 = F_quad[i][(j + 3) % 4];

            Vector3d face_norm = (O_quad[v2] - O_quad[v1]).cross(O_quad[v3] - O_quad[v1]);
            Vector3d vertex_norm = N_quad[v1];
            if (face_norm.dot(vertex_norm) < 0) {
                flipped = true;
            }
        }
        if (flipped) {
            flip_count++;
        }
    }
    printf("Flipped Quads: %d\n", flip_count);

    int n_quad = O_quad.size();
    int n_trig = O.cols();
    std::vector<double> B_quad(n_quad);  // Average bias for quad vertex
    std::vector<int> B_weight(n_quad);

    printf("ntrig: %d, disjoint_tree.size: %d\n", n_trig, (int)disajoint_tree.indices.size());
    for (int vtrig = 0; vtrig < n_trig; ++vtrig) {
        int vquad = disajoint_tree.Index(vtrig);
        double b = N_quad[vquad].dot(O.col(vtrig));
        B_quad[vquad] += b;
        B_weight[vquad] += 1;
    }
    for (int vquad = 0; vquad < n_quad; ++vquad) {
        assert(B_weight[vquad]);
        B_quad[vquad] /= B_weight[vquad];
    }

    puts("Save parameters to post.bin for optimization");
    FILE* out = fopen("post.bin", "wb");
    assert(out);
    Save(out, O_quad);
    Save(out, N_quad);
    Save(out, Q_quad);
    Save(out, F_quad);
    Save(out, B_quad);
    Save(out, V);
    Save(out, N);
    Save(out, Q);
    Save(out, O);
    Save(out, F);
    Save(out, reference_length);
    fclose(out);

    if (!just_serialize) {
        puts("Start post optimization");
        solve(O_quad, N_quad, Q_quad, F_quad, B_quad, V, N, Q, O, F, reference_length, COEFF_AREA,
              COEFF_TANGENT, COEFF_NORMAL, COEFF_FLOW, COEFF_ORTH);
    }
}

#ifdef POST_SOLVER

void SaveObj(const std::string& fname, std::vector<Vector3d> O_quad,
             std::vector<Vector4i> F_quad) {
    std::ofstream os(fname);
    for (int i = 0; i < (int)O_quad.size(); ++i) {
        os << "v " << O_quad[i][0] << " " << O_quad[i][1] << " " << O_quad[i][2] << "\n";
    }
    for (int i = 0; i < (int)F_quad.size(); ++i) {
        os << "f " << F_quad[i][0] + 1 << " " << F_quad[i][1] + 1 << " " << F_quad[i][2] + 1 << " "
           << F_quad[i][3] + 1 << "\n";
    }
    os.close();
}

int main(int argc, char* argv[]) {
    double coeff_area;
    double coeff_tangent;
    double coeff_normal;
    double coeff_flow;
    double coeff_orth;

    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()  // clang-format off
        ("help,h", "produce help message")
        ("area,a", po::value<double>(&coeff_area)->default_value(COEFF_AREA), "Set the coefficient of area constraint")
        ("tangent,t", po::value<double>(&coeff_tangent)->default_value(COEFF_TANGENT), "Set the coefficient of tangent constraint")
        ("normal,n", po::value<double>(&coeff_normal)->default_value(COEFF_NORMAL), "Set the coefficient of normal constraint")
        ("flow,f", po::value<double>(&coeff_flow)->default_value(COEFF_FLOW), "Set the coefficient of flow (Q) constraint")
        ("orth,o", po::value<double>(&coeff_orth)->default_value(COEFF_ORTH), "Set the coefficient of orthogonal constraint");

    // clang-format on
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 1;
    }

    std::vector<Vector3d> O_quad;
    std::vector<Vector3d> N_quad;
    std::vector<Vector3d> Q_quad;
    std::vector<Vector4i> F_quad;
    std::vector<double> B_quad;
    MatrixXd V;
    MatrixXd N;
    MatrixXd Q;
    MatrixXd O;
    MatrixXi F;
    double reference_length;

    puts("Read parameters from post.bin");
    FILE* in = fopen("post.bin", "rb");
    assert(in);
    Read(in, O_quad);
    Read(in, N_quad);
    Read(in, Q_quad);
    Read(in, F_quad);
    Read(in, B_quad);
    Read(in, V);
    Read(in, N);
    Read(in, Q);
    Read(in, O);
    Read(in, F);
    Read(in, reference_length);
    fclose(in);
    printf("reference_length: %.2f\n", reference_length);
    SaveObj("presolver.obj", O_quad, F_quad);

    int n_flip = 0;
    double sum_degree = 0;
    for (int i = 0; i < F_quad.size(); ++i) {
        bool flipped = false;
        for (int j = 0; j < 4; ++j) {
            int v1 = F_quad[i][j];
            int v2 = F_quad[i][(j + 1) % 4];
            int v3 = F_quad[i][(j + 3) % 4];

            Vector3d face_norm =
                (O_quad[v2] - O_quad[v1]).cross(O_quad[v3] - O_quad[v1]).normalized();
            Vector3d vertex_norm = N_quad[v1];
            if (face_norm.dot(vertex_norm) < 0) {
                flipped = true;
            }
            double degree = std::acos(face_norm.dot(vertex_norm));
            assert(degree >= 0);
            // printf("cos theta = %.2f\n", degree);
            sum_degree += degree * degree;
        }
        n_flip += flipped;
    }
    printf("n_flip: %d\nsum_degree: %.3f\n", n_flip, sum_degree);

    puts("Start post optimization");
    solve(O_quad, N_quad, Q_quad, F_quad, B_quad, V, N, Q, O, F, reference_length, coeff_area,
          coeff_tangent, coeff_normal, coeff_flow, coeff_orth);
    SaveObj("postsolver.obj", O_quad, F_quad);

    n_flip = 0;
    sum_degree = 0;
    for (int i = 0; i < F_quad.size(); ++i) {
        bool flipped = false;
        for (int j = 0; j < 4; ++j) {
            int v1 = F_quad[i][j];
            int v2 = F_quad[i][(j + 1) % 4];
            int v3 = F_quad[i][(j + 3) % 4];

            Vector3d face_norm =
                (O_quad[v2] - O_quad[v1]).cross(O_quad[v3] - O_quad[v1]).normalized();
            Vector3d vertex_norm = N_quad[v1];
            if (face_norm.dot(vertex_norm) < 0) {
                flipped = true;
            }
            double degree = std::acos(face_norm.dot(vertex_norm));
            assert(degree >= 0);
            sum_degree += degree * degree;
        }
        n_flip += flipped;
    }
    printf("n_flip: %d\nsum_degree: %.3f\n", n_flip, sum_degree);
    return 0;
}

#endif

} // namespace qflow
