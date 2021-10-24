/*
    hierarchy.h: Code to generate unstructured multi-resolution hierarchies
    from meshes or point clouds

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include "adjacency.h"

class Serializer;

extern AdjacencyMatrix
downsample_graph(const AdjacencyMatrix adj, const MatrixXf &V,
                 const MatrixXf &N, const VectorXf &areas, MatrixXf &V_p,
                 MatrixXf &V_n, VectorXf &areas_p, MatrixXu &to_upper,
                 VectorXu &to_lower, bool deterministic = false,
                 const ProgressCallback &progress = ProgressCallback());

struct MultiResolutionHierarchy {
    enum { MAX_DEPTH = 25 };
public:
    MultiResolutionHierarchy();
    void free();
    void save(Serializer &state);
    void load(const Serializer &state);

    int levels() const { return (int) mV.size(); }

    void build(bool deterministic = false,
               const ProgressCallback &progress = ProgressCallback());

    void printStatistics() const;
    void resetSolution();

    inline ordered_lock &mutex() { return mMutex; }

    inline const std::vector<std::vector<uint32_t>> &phases(int level) const { return mPhases[level]; }
    inline const AdjacencyMatrix &adj(int level = 0) const { return mAdj[level]; }
    inline AdjacencyMatrix &adj(int level = 0) { return mAdj[level]; }
    inline const MatrixXf &V(int level = 0) const { return mV[level]; }
    inline const MatrixXf &N(int level = 0) const { return mN[level]; }
    inline const VectorXf &A(int level = 0) const { return mA[level]; }
    inline const MatrixXu &toUpper(int level) const { return mToUpper[level]; }
    inline const VectorXu &toLower(int level) const { return mToLower[level]; }
    inline const MatrixXf &Q(int level = 0) const { return mQ[level]; }
    inline const MatrixXf &O(int level = 0) const { return mO[level]; }
    inline const MatrixXf &CQ(int level = 0) const { return mCQ[level]; }
    inline const MatrixXf &CO(int level = 0) const { return mCO[level]; }
    inline const VectorXf &CQw(int level = 0) const { return mCQw[level]; }
    inline const VectorXf &COw(int level = 0) const { return mCOw[level]; }
    inline const MatrixXu &F() const { return mF; }
    inline const VectorXu &E2E() const { return mE2E; }
    inline MatrixXf &Q(int level = 0) { return mQ[level]; }
    inline MatrixXf &O(int level = 0) { return mO[level]; }
    inline MatrixXf &CQ(int level = 0) { return mCQ[level]; }
    inline MatrixXf &CO(int level = 0) { return mCO[level]; }
    inline VectorXf &CQw(int level = 0) { return mCQw[level]; }
    inline VectorXf &COw(int level = 0) { return mCOw[level]; }

    inline void setF(MatrixXu &&F) { mF = std::move(F); }
    inline void setE2E(VectorXu &&E2E) { mE2E = std::move(E2E); }
    inline void setV(MatrixXf &&V) { mV.clear(); mV.push_back(std::move(V)); }
    inline void setN(MatrixXf &&N) { mN.clear(); mN.push_back(std::move(N)); }
    inline void setA(MatrixXf &&A) { mA.clear(); mA.push_back(std::move(A)); }
    inline void setAdj(AdjacencyMatrix &&adj) { mAdj.clear(); mAdj.push_back(std::move(adj)); }

    inline uint32_t size(int level = 0) const { return mV[level].cols(); }

    inline Float scale() const { return mScale; }
    inline void setScale(Float scale) { mScale = scale; }
    inline int iterationsQ() const { return mIterationsQ; }
    inline void setIterationsQ(int iterationsQ) { mIterationsQ = iterationsQ; }
    inline int iterationsO() const { return mIterationsO; }
    inline void setIterationsO(int iterationsO) { mIterationsO = iterationsO; }
    inline size_t totalSize() const { return mTotalSize; }

    void clearConstraints();
    void propagateConstraints(int rosy, int posy);
    void propagateSolution(int rosy);

    inline Vector3f faceCenter(uint32_t idx) const {
        Vector3f pos = Vector3f::Zero();
        for (int i = 0; i < 3; ++i)
            pos += mV[0].col(mF(i, idx));
        return pos * (1.0f / 3.0f);
    }

    inline Vector3f faceNormal(uint32_t idx) const {
        Vector3f p0 = mV[0].col(mF(0, idx)),
                 p1 = mV[0].col(mF(1, idx)),
                 p2 = mV[0].col(mF(2, idx));
        return (p1-p0).cross(p2-p0).normalized();
    }

    /* Flags which indicate whether the integer variables are froen */
    bool frozenQ() const { return mFrozenQ; }
    bool frozenO() const { return mFrozenO; }
    void setFrozenQ(bool frozen) { mFrozenQ = frozen; }
    void setFrozenO(bool frozen) { mFrozenO = frozen; }
public:
    MatrixXu mF;
    VectorXu mE2E;
    std::vector<std::vector<std::vector<uint32_t>>> mPhases;
    std::vector<AdjacencyMatrix> mAdj;
    std::vector<MatrixXf> mV;
    std::vector<MatrixXf> mN;
    std::vector<VectorXf> mA;
    std::vector<VectorXu> mToLower;
    std::vector<MatrixXu> mToUpper;
    std::vector<MatrixXf> mO;
    std::vector<MatrixXf> mQ;
    std::vector<MatrixXf> mCQ;
    std::vector<MatrixXf> mCO;
    std::vector<VectorXf> mCQw;
    std::vector<VectorXf> mCOw;
    bool mFrozenQ, mFrozenO;
    ordered_lock mMutex;
    Float mScale;
    int mIterationsQ;
    int mIterationsO;
    uint32_t mTotalSize;
};
