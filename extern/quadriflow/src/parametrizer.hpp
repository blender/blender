#ifndef PARAMETRIZER_H_
#define PARAMETRIZER_H_
#include <atomic>
#include <condition_variable>
#ifdef WITH_TBB
#include <tbb/tbb.h>
#endif

#include <Eigen/Core>
#include <Eigen/Dense>
#include <list>
#include <map>
#include <set>
#include <unordered_set>
#include "adjacent-matrix.hpp"
#include "disajoint-tree.hpp"
#include "field-math.hpp"
#include "hierarchy.hpp"
#include "post-solver.hpp"
#include "serialize.hpp"

namespace qflow {

using namespace Eigen;

typedef std::pair<unsigned int, unsigned int> Edge;
typedef std::map<int, std::pair<int, int>> SingDictionary;

struct ExpandInfo {
    ExpandInfo() {}
    int current_v;
    int singularity;
    int step;
    int edge_id;
    int prev;
};

class Parametrizer {
   public:
    Parametrizer() {}
    // Mesh Initialization
    void Load(const char* filename);
    void NormalizeMesh();
    void ComputeMeshStatus();
    void ComputeSmoothNormal();
    void ComputeSharpEdges();
    void ComputeSharpO();
    void ComputeVertexArea();
    void Initialize(int faces);

    // Singularity and Mesh property
    void AnalyzeValence();
    void ComputeOrientationSingularities();
    void ComputePositionSingularities();

    // Integer Grid Map Pipeline
    void ComputeIndexMap(int with_scale = 0);
    void BuildEdgeInfo();
    void ComputeMaxFlow();
    void MarkInteger();
    void BuildIntegerConstraints();

    // Fix Flip
    void FixFlipHierarchy();
    void FixFlipSat();
    void FixHoles();
    void FixHoles(std::vector<int>& loop_vertices);
    void FixValence();
    double QuadEnergy(std::vector<int>& loop_vertices, std::vector<Vector4i>& res_quads,
                      int level);

    // Quadmesh and IO
    void AdvancedExtractQuad();
    void BuildTriangleManifold(DisajointTree& disajoint_tree, std::vector<int>& edge,
                               std::vector<int>& face, std::vector<DEdge>& edge_values,
                               std::vector<Vector3i>& F2E, std::vector<Vector2i>& E2F,
                               std::vector<Vector2i>& EdgeDiff, std::vector<Vector3i>& FQ);
    void OutputMesh(const char* obj_name);

    std::map<int, int> singularities;  // map faceid to valence (1 (valence=3) or 3(valence=5))
    std::map<int, Vector2i> pos_sing;
    MatrixXi pos_rank;   // pos_rank(i, j) i \in [0, 3) jth face ith vertex  rotate by its value so
                         // that all thress vertices are in the same orientation
    MatrixXi pos_index;  // pos_index(i x 2 + dim, j) i \in [0, 6) jth face ith vertex's
                         // (t_ij-t_ji)'s dim's dimenstion in the paper
    // input mesh
    MatrixXd V;
    MatrixXd N;
    MatrixXd Nf;
    MatrixXd FS;
    MatrixXd FQ;
    MatrixXi F;

    double normalize_scale;
    Vector3d normalize_offset;

    // data structures
    VectorXd rho;
    VectorXi V2E;
    VectorXi E2E;
    VectorXi boundary;
    VectorXi nonManifold;  // nonManifold vertices, in boolean
    AdjacentMatrix adj;
    Hierarchy hierarchy;

    // Mesh Status;
    double surface_area;
    double scale;
    double average_edge_length;
    double max_edge_length;
    VectorXd A;

    // just for test
    DisajointTree disajoint_tree;

    int compact_num_v;
    std::vector<std::vector<int>> Vset;
    std::vector<Vector3d> O_compact;
    std::vector<Vector3d> Q_compact;
    std::vector<Vector3d> N_compact;
    std::vector<Vector4i> F_compact;
    std::set<std::pair<int, int>> Quad_edges;
    std::vector<int> V2E_compact;
    std::vector<int> E2E_compact;
    VectorXi boundary_compact;
    VectorXi nonManifold_compact;

    std::vector<int> bad_vertices;
    std::vector<double> counter;
    std::vector<int>
        sharp_edges;  // sharp_edges[deid]: whether deid is a sharp edge that should be preserved
    std::vector<int> allow_changes;   // allow_changes[variable_id]: whether var can be changed
                                      // based on sharp edges
    std::vector<Vector2i> edge_diff;  // edge_diff[edgeIds[i](j)]:  t_ij+t_ji under
                                      // edge_values[edgeIds[i](j)].x's Q value
    std::vector<DEdge> edge_values;   // see above
    std::vector<Vector3i>
        face_edgeIds;  // face_edgeIds[i](j): ith face jth edge's "undirected edge ID"

    // face_edgeOrients[i](j): Rotate from edge_diff space
    //    (a) initially, to F(0, i)'s Q space
    //    (b) later on, to a global Q space where some edges are fixed
    std::vector<Vector3i> face_edgeOrients;

    // variable[i].first: indices of the two equations corresponding to variable i
    // variable[i].second: number of positive minus negative of variables' occurances
    std::vector<std::pair<Vector2i, int>> variables;

    struct QuadInfo {
        QuadInfo() : patchId(-1), coordinate(0x10000000, 0x10000000), singular(0), edge(0) {}
        int patchId;
        Vector2i coordinate;
        int singular;
        int edge;
    };
    std::vector<QuadInfo> quad_info;

    // scale
    void ComputeInverseAffine();
    void EstimateSlope();
    std::vector<MatrixXd> triangle_space;

    // flag
    int flag_preserve_sharp = 0;
    int flag_preserve_boundary = 0;
    int flag_adaptive_scale = 0;
    int flag_aggresive_sat = 0;
    int flag_minimum_cost_flow = 0;
};

extern void generate_adjacency_matrix_uniform(const MatrixXi& F, const VectorXi& V2E,
                                              const VectorXi& E2E, const VectorXi& nonManifold,
                                              AdjacentMatrix& adj);

} // namespace qflow

#endif
