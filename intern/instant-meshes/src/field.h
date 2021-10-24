/*
    field.h: Routines for averaging orientations and directions subject
    to various symmetry conditions. Also contains the Optimizer class which
    uses these routines to smooth fields hierarchically.

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once


#include "hierarchy.h"
#include <map>

/* Rotation helper functions */
extern Vector3f rotate60(const Vector3f &d, const Vector3f &n);
extern Vector3f rotate90(const Vector3f &d, const Vector3f &n);
extern Vector3f rotate180(const Vector3f &d, const Vector3f &n);
extern Vector3f rotate60_by(const Vector3f &d, const Vector3f &n, int amount);
extern Vector3f rotate90_by(const Vector3f &d, const Vector3f &n, int amount);
extern Vector3f rotate180_by(const Vector3f &d, const Vector3f &n, int amount);
extern Vector2i rshift60(Vector2i shift, int amount);
extern Vector2i rshift90(Vector2i shift, int amount);
extern Vector2i rshift180(Vector2i shift, int amount);
extern Vector3f rotate_vector_into_plane(Vector3f q, const Vector3f &source_normal, const Vector3f &target_normal);

/* Extrinsic & intrinsic orientation symmetry functors */
extern std::pair<Vector3f, Vector3f>
compat_orientation_intrinsic_2(const Vector3f &q0, const Vector3f &n0,
                               const Vector3f &q1, const Vector3f &n1);

extern std::pair<Vector3f, Vector3f>
compat_orientation_intrinsic_4(const Vector3f &q0, const Vector3f &n0,
                               const Vector3f &q1, const Vector3f &n1);

extern std::pair<Vector3f, Vector3f>
compat_orientation_intrinsic_4_knoeppel(const Vector3f &q0, const Vector3f &n0,
                                        const Vector3f &q1, const Vector3f &n1);

extern std::pair<Vector3f, Vector3f>
compat_orientation_intrinsic_6(const Vector3f &q0, const Vector3f &n0,
                               const Vector3f &q1, const Vector3f &n1);

extern std::pair<Vector3f, Vector3f>
compat_orientation_extrinsic_2(const Vector3f &q0, const Vector3f &n0,
                               const Vector3f &q1, const Vector3f &n1);

extern std::pair<Vector3f, Vector3f>
compat_orientation_extrinsic_4(const Vector3f &q0, const Vector3f &n0,
                               const Vector3f &q1, const Vector3f &n1);

extern std::pair<Vector3f, Vector3f>
compat_orientation_extrinsic_6(const Vector3f &q0, const Vector3f &n0,
                               const Vector3f &q1, const Vector3f &n1);

extern std::pair<int, int>
compat_orientation_extrinsic_index_2(const Vector3f &q0, const Vector3f &n0,
                                     const Vector3f &q1, const Vector3f &n1);

extern std::pair<int, int>
compat_orientation_extrinsic_index_4(const Vector3f &q0, const Vector3f &n0,
                                     const Vector3f &q1, const Vector3f &n1);

extern std::pair<int, int>
compat_orientation_extrinsic_index_6(const Vector3f &q0, const Vector3f &n0,
                                     const Vector3f &q1, const Vector3f &n1);

extern std::pair<int, int>
compat_orientation_intrinsic_index_2(const Vector3f &q0, const Vector3f &n0,
                                     const Vector3f &q1, const Vector3f &n1);

extern std::pair<int, int>
compat_orientation_intrinsic_index_4(const Vector3f &q0, const Vector3f &n0,
                                     const Vector3f &q1, const Vector3f &n1);

extern std::pair<int, int>
compat_orientation_intrinsic_index_6(const Vector3f &q0, const Vector3f &n0,
                                     const Vector3f &q1, const Vector3f &n1);

/* Extrinsic & intrinsic position symmetry functors */
extern std::pair<Vector3f, Vector3f> compat_position_extrinsic_3(
    const Vector3f &p0, const Vector3f &n0, const Vector3f &q0,
    const Vector3f &o0, const Vector3f &p1, const Vector3f &n1,
    const Vector3f &q1, const Vector3f &o1, Float scale, Float inv_scale);

extern std::pair<Vector3f, Vector3f> compat_position_extrinsic_4(
    const Vector3f &p0, const Vector3f &n0, const Vector3f &q0,
    const Vector3f &o0, const Vector3f &p1, const Vector3f &n1,
    const Vector3f &q1, const Vector3f &o1, Float scale, Float inv_scale);

extern std::pair<Vector2i, Vector2i> compat_position_extrinsic_index_3(
    const Vector3f &p0, const Vector3f &n0, const Vector3f &q0,
    const Vector3f &o0, const Vector3f &p1, const Vector3f &n1,
    const Vector3f &q1, const Vector3f &o1, Float scale, Float inv_scale,
    Float *error = nullptr);

extern std::pair<Vector2i, Vector2i> compat_position_extrinsic_index_4(
    const Vector3f &p0, const Vector3f &n0, const Vector3f &q0,
    const Vector3f &o0, const Vector3f &p1, const Vector3f &n1,
    const Vector3f &q1, const Vector3f &o1, Float scale, Float inv_scale,
    Float *error = nullptr);

extern std::pair<Vector3f, Vector3f> compat_position_intrinsic_3(
    const Vector3f &p0, const Vector3f &n0, const Vector3f &q0,
    const Vector3f &o0, const Vector3f &p1, const Vector3f &n1,
    const Vector3f &q1, const Vector3f &o1, Float scale, Float inv_scale);

extern std::pair<Vector3f, Vector3f> compat_position_intrinsic_4(
    const Vector3f &p0, const Vector3f &n0, const Vector3f &q0,
    const Vector3f &o0, const Vector3f &p1, const Vector3f &n1,
    const Vector3f &q1, const Vector3f &o1, Float scale, Float inv_scale);

extern std::pair<Vector2i, Vector2i> compat_position_intrinsic_index_3(
    const Vector3f &p0, const Vector3f &n0, const Vector3f &q0,
    const Vector3f &o0, const Vector3f &p1, const Vector3f &n1,
    const Vector3f &q1, const Vector3f &o1, Float scale, Float inv_scale,
    Float *error = nullptr);

extern std::pair<Vector2i, Vector2i> compat_position_intrinsic_index_4(
    const Vector3f &p0, const Vector3f &n0, const Vector3f &q0,
    const Vector3f &o0, const Vector3f &p1, const Vector3f &n1,
    const Vector3f &q1, const Vector3f &o1, Float scale, Float inv_scale,
    Float *error = nullptr);

/* Optimization kernels */

extern Float optimize_orientations(
    MultiResolutionHierarchy &mRes, int level, bool extrinsic, int rosy,
    const std::function<void(uint32_t)> &progress);

extern Float optimize_positions(
    MultiResolutionHierarchy &mRes, int level, bool extrinsic, int posy,
    const std::function<void(uint32_t)> &progress);

/* Singularity computation */

extern void compute_orientation_singularities(
    const MultiResolutionHierarchy &mRes, std::map<uint32_t, uint32_t> &sing,
    bool extrinsic, int rosy);

extern void
compute_position_singularities(const MultiResolutionHierarchy &mRes,
                               const std::map<uint32_t, uint32_t> &orient_sing,
                               std::map<uint32_t, Vector2i> &pos_sing,
                               bool extrinsic, int rosy, int posy);

/* Field optimizer (invokes optimization kernels in a separate thread) */

class Serializer;
class Optimizer {
public:
    Optimizer(MultiResolutionHierarchy &mRes, bool interactive);
    void save(Serializer &state);
    void load(const Serializer &state);

    void stop() {
        if (mOptimizeOrientations)
            mRes.propagateSolution(mRoSy);
        mOptimizePositions = mOptimizeOrientations = false;
        notify(); 
    }

    void shutdown() { mRunning = false; notify(); mThread.join(); }

    bool active() { return mOptimizePositions | mOptimizeOrientations; }
    inline void notify() { mCond.notify_all(); }

    void optimizeOrientations(int level);

    void optimizePositions(int level);

    void wait();

    void setExtrinsic(bool extrinsic) { mExtrinsic = extrinsic; }
    bool extrinsic() const { return mExtrinsic; }

    void setRoSy(int rosy) { mRoSy = rosy; }
    int rosy() const { return mRoSy; }
    void setPoSy(int posy) { mPoSy = posy; }
    int posy() const { return mPoSy; }
    void setLevel(int level) { mLevel = level; }
    int level() const { return mLevel; }
    Float progress() const { return mProgress; }

#ifdef VISUALIZE_ERROR
    const VectorXf &error() { return mError; }
#endif

    void moveSingularity(const std::vector<uint32_t> &path, bool orientations) {
        std::lock_guard<ordered_lock> lock(mRes.mutex());
        mAttractorStrokes.push_back(std::make_pair(orientations, path));
        setLevel(0);
    }

    void run();
protected:
    MultiResolutionHierarchy &mRes;
    std::vector<std::pair<bool, std::vector<uint32_t>>> mAttractorStrokes;
    bool mRunning;
    bool mOptimizeOrientations;
    bool mOptimizePositions;
    std::thread mThread;
    std::condition_variable_any mCond;
    int mLevel, mLevelIterations;
    bool mHierarchical;
    int mRoSy, mPoSy;
    bool mExtrinsic;
    bool mInteractive;
    double mLastUpdate;
    Float mProgress;
#ifdef VISUALIZE_ERROR
    VectorXf mError;
#endif
    Timer<> mTimer;
};
