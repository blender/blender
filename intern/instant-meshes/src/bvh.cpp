/*
    bvh.cpp -- bounding volume hierarchy for fast ray-intersection queries

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include "bvh.h"

struct Bins {
    static const int BIN_COUNT = 8;
    Bins() { memset(counts, 0, sizeof(uint32_t) * BIN_COUNT); }
    uint32_t counts[BIN_COUNT];
    AABB bounds[BIN_COUNT];
};

struct BVHBuildTask : public tbb::task {
    enum { SERIAL_THRESHOLD = 32 };
    BVH &bvh;
    uint32_t node_idx;
    uint32_t *start, *end, *temp;

    BVHBuildTask(BVH &bvh, uint32_t node_idx, uint32_t *start, uint32_t *end, uint32_t *temp)
        : bvh(bvh), node_idx(node_idx), start(start), end(end), temp(temp) { }

    task *execute() {
        const MatrixXu &F = *bvh.mF;
        const MatrixXf &V = *bvh.mV;
        bool pointcloud = F.size() == 0;
        uint32_t size = end-start, total_size = pointcloud ? V.cols() : F.cols();
        BVHNode &node = bvh.mNodes[node_idx];

        if (size < SERIAL_THRESHOLD) {
            tbb::blocked_range<uint32_t> range(start-bvh.mIndices, end-bvh.mIndices);
            const ProgressCallback &progress = bvh.mProgress;
            SHOW_PROGRESS_RANGE(range, total_size, "Constructing Bounding Volume Hierarchy");
            execute_serially(bvh, node_idx, start, end, temp);
            return nullptr;
        }

        int axis = node.aabb.largestAxis();
        Float min = node.aabb.min[axis], max = node.aabb.max[axis],
              inv_bin_size = Bins::BIN_COUNT / (max-min);

        Bins bins = tbb::parallel_reduce(
            tbb::blocked_range<uint32_t>(0u, size, GRAIN_SIZE),
            Bins(),
            [&](const tbb::blocked_range<uint32_t> &range, Bins result) {
                for (uint32_t i = range.begin(); i != range.end(); ++i) {
                    uint32_t f = start[i];
                    Float centroid = pointcloud ? V(axis, f)
                         : ((1.0f / 3.0f) * (V(axis, F(0, f)) +
                                             V(axis, F(1, f)) +
                                             V(axis, F(2, f))));

                    int index = std::min(std::max(
                        (int) ((centroid - min) * inv_bin_size), 0),
                        (Bins::BIN_COUNT - 1));

                    result.counts[index]++;
                    AABB &bin_bounds = result.bounds[index];
                    if (!pointcloud) {
                        bin_bounds.expandBy(V.col(F(0, f)));
                        bin_bounds.expandBy(V.col(F(1, f)));
                        bin_bounds.expandBy(V.col(F(2, f)));
                    } else {
                        bin_bounds.expandBy(V.col(f));
                    }
                }
                return result;
            },
            [](const Bins &b1, const Bins &b2) {
                Bins result;
                for (int i=0; i < Bins::BIN_COUNT; ++i) {
                    result.counts[i] = b1.counts[i] + b2.counts[i];
                    result.bounds[i] = AABB::merge(b1.bounds[i], b2.bounds[i]);
                }
                return result;
            }
        );

        AABB bounds_left[Bins::BIN_COUNT];
        bounds_left[0] = bins.bounds[0];
        for (int i=1; i<Bins::BIN_COUNT; ++i) {
            bins.counts[i] += bins.counts[i-1];
            bounds_left[i] = AABB::merge(bounds_left[i-1], bins.bounds[i]);
        }
        AABB bounds_right = bins.bounds[Bins::BIN_COUNT-1];
        int64_t best_index = -1;
        Float best_cost = BVH::T_tri * size;
        Float tri_factor = BVH::T_tri / node.aabb.surfaceArea();
        AABB best_bounds_right;

        for (int i=Bins::BIN_COUNT - 2; i >= 0; --i) {
            uint32_t prims_left = bins.counts[i], prims_right = (end - start) - bins.counts[i];
            Float sah_cost = 2.0f * BVH::T_aabb +
                tri_factor * (prims_left * bounds_left[i].surfaceArea() +
                              prims_right * bounds_right.surfaceArea());
            if (sah_cost < best_cost) {
                best_cost = sah_cost;
                best_index = i;
                best_bounds_right = bounds_right;
            }
            bounds_right = AABB::merge(bounds_right, bins.bounds[i]);
        }

        if (best_index == -1) {
            /* Could not find a good split plane -- retry with
               more careful serial code just to be sure.. */
            execute_serially(bvh, node_idx, start, end, temp);
            return nullptr;
        }

        uint32_t left_count = bins.counts[best_index];
        int node_idx_left = node_idx+1;
        int node_idx_right = node_idx+2*left_count;

        bvh.mNodes[node_idx_left ].aabb = bounds_left[best_index];
        bvh.mNodes[node_idx_right].aabb = best_bounds_right;
        node.inner.rightChild = node_idx_right;
        node.inner.unused = 0;

        std::atomic<uint32_t> offset_left(0), offset_right(bins.counts[best_index]);
        tbb::parallel_for(
            tbb::blocked_range<uint32_t>(0u, size, GRAIN_SIZE),
            [&](const tbb::blocked_range<uint32_t> &range) {
                uint32_t count_left = 0, count_right = 0;
                for (uint32_t i = range.begin(); i != range.end(); ++i) {
                    uint32_t f = start[i];
                    Float centroid = pointcloud ? V(axis, f)
                         : ((1.0f / 3.0f) * (V(axis, F(0, f)) +
                                             V(axis, F(1, f)) +
                                             V(axis, F(2, f))));
                    int index = (int) ((centroid - min) * inv_bin_size);
                    (index <= best_index ? count_left : count_right)++;
                }
                uint32_t idx_l = offset_left.fetch_add(count_left);
                uint32_t idx_r = offset_right.fetch_add(count_right);
                for (uint32_t i = range.begin(); i != range.end(); ++i) {
                    uint32_t f = start[i];
                    Float centroid = pointcloud ? V(axis, f)
                         : ((1.0f / 3.0f) * (V(axis, F(0, f)) +
                                             V(axis, F(1, f)) +
                                             V(axis, F(2, f))));
                    int index = (int) ((centroid - min) * inv_bin_size);
                    if (index <= best_index)
                        temp[idx_l++] = f;
                    else
                        temp[idx_r++] = f;
                }
            }
        );
        memcpy(start, temp, size * sizeof(uint32_t));
        assert(offset_left == left_count && offset_right == size);

        /* Create an empty parent task */
        tbb::task& c = *new (allocate_continuation()) tbb::empty_task;
        c.set_ref_count(2);

        /* Post right subtree to scheduler */
        BVHBuildTask &b = *new (c.allocate_child())
            BVHBuildTask(bvh, node_idx_right, start + left_count,
                         end, temp + left_count);
        spawn(b);

        /* Directly start working on left subtree */
        recycle_as_child_of(c);
        node_idx = node_idx_left;
        end = start + left_count;

        return this;
    }

    static void execute_serially(BVH &bvh, uint32_t node_idx, uint32_t *start, uint32_t *end, uint32_t *temp) {
        uint32_t size = end-start;
        BVHNode &node = bvh.mNodes[node_idx];
        const MatrixXu &F = *bvh.mF;
        const MatrixXf &V = *bvh.mV;
        Float best_cost = BVH::T_tri * size;
        int64_t best_index = -1, best_axis = -1;
        float *left_areas = (float *) temp;
        bool pointcloud = F.size() == 0;

        for (int axis=0; axis<3; ++axis) {
            if (pointcloud) {
                std::sort(start, end, [&](uint32_t f1, uint32_t f2) {
                    return V(axis, f1) < V(axis, f2);
                });
            } else {
                std::sort(start, end, [&](uint32_t f1, uint32_t f2) {
                    return
                        (V(axis, F(0, f1)) + V(axis, F(1, f1)) + V(axis, F(2, f1))) <
                        (V(axis, F(0, f2)) + V(axis, F(1, f2)) + V(axis, F(2, f2)));
                });
            }

            AABB aabb;
            for (uint32_t i = 0; i<size; ++i) {
                uint32_t f = *(start + i);
                if (pointcloud) {
                    aabb.expandBy(V.col(f));
                } else {
                    aabb.expandBy(V.col(F(0, f)));
                    aabb.expandBy(V.col(F(1, f)));
                    aabb.expandBy(V.col(F(2, f)));
                }
                left_areas[i] = (float) aabb.surfaceArea();
            }
            if (axis == 0)
                node.aabb = aabb;

            aabb.clear();

            Float tri_factor = BVH::T_tri / node.aabb.surfaceArea();
            for (uint32_t i = size-1; i>=1; --i) {
                uint32_t f = *(start + i);
                if (pointcloud) {
                    aabb.expandBy(V.col(f));
                } else {
                    aabb.expandBy(V.col(F(0, f)));
                    aabb.expandBy(V.col(F(1, f)));
                    aabb.expandBy(V.col(F(2, f)));
                }

                float left_area = left_areas[i-1];
                float right_area = aabb.surfaceArea();
                uint32_t prims_left = i;
                uint32_t prims_right = size-i;

                Float sah_cost = 2.0f * BVH::T_aabb +
                    tri_factor * (prims_left * left_area +
                                  prims_right * right_area);
                if (sah_cost < best_cost) {
                    best_cost = sah_cost;
                    best_index = i;
                    best_axis = axis;
                }
            }
        }

        if (best_index == -1) {
            /* Splitting does not reduce the cost, make a leaf */
            node.leaf.flag = 1;
            node.leaf.start = start - bvh.mIndices;
            node.leaf.size  = size;
            return;
        }


        if (pointcloud) {
            std::sort(start, end, [&](uint32_t f1, uint32_t f2) {
                return V(best_axis, f1) < V(best_axis, f2);
            });
        } else {
            std::sort(start, end, [&](uint32_t f1, uint32_t f2) {
                return
                    (V(best_axis, F(0, f1)) + V(best_axis, F(1, f1)) + V(best_axis, F(2, f1))) <
                    (V(best_axis, F(0, f2)) + V(best_axis, F(1, f2)) + V(best_axis, F(2, f2)));
            });
        }

        uint32_t left_count = best_index;
        int node_idx_left = node_idx+1;
        int node_idx_right = node_idx+2*left_count;
        node.inner.rightChild = node_idx_right;
        node.inner.unused = 0;

        execute_serially(bvh, node_idx_left, start, start + left_count, temp);
        execute_serially(bvh, node_idx_right, start+left_count, end, temp + left_count);
    }
};

BVH::BVH(const MatrixXu *F, const MatrixXf *V, const MatrixXf *N, const AABB &aabb)
: mIndices(nullptr), mF(F), mV(V), mN(N), mDiskRadius(0.f) {
    if (mF->size() > 0) {
        mNodes.resize(2*mF->cols());
        memset(mNodes.data(), 0, sizeof(BVHNode) * mNodes.size());
        mNodes[0].aabb = aabb;
        mIndices = new uint32_t[mF->cols()];
    } else if (mV->size() > 0) {
        mNodes.resize(2*mV->cols());
        memset(mNodes.data(), 0, sizeof(BVHNode) * mNodes.size());
        mNodes[0].aabb = aabb;
        mIndices = new uint32_t[mV->cols()];
    }
}

void BVH::build(const ProgressCallback &progress) {
    if (mF->cols() == 0 && mV->cols() == 0)
        return;
    mProgress = progress;

#if defined(SINGLE_PRECISION)
    if (sizeof(BVHNode) != 32)
        throw std::runtime_error("BVH Node is not packed! Investigate compiler settings.");
#endif

    cout << "Constructing Bounding Volume Hierarchy .. ";
    cout.flush();

    bool pointcloud = mF->size() == 0;
    uint32_t total_size = pointcloud ? mV->cols() : mF->cols();

    for (uint32_t i = 0; i < total_size; ++i)
        mIndices[i] = i;

    Timer<> timer;
    uint32_t *temp = new uint32_t[total_size];
    BVHBuildTask& task = *new(tbb::task::allocate_root())
        BVHBuildTask(*this, 0u, mIndices, mIndices + total_size, temp);
    tbb::task::spawn_root_and_wait(task);
    delete[] temp;

    std::pair<Float, uint32_t> stats = statistics();
    cout << "done. ("
         << "SAH cost = " << stats.first << ", "
         << "nodes = " << stats.second << ", "
         << "took " << timeString(timer.reset())
         << ")" << endl;

    cout.precision(4);
    cout << "Compressing BVH node storage to "
         << 100 * stats.second / (float) mNodes.size() << "% of its original size .. ";
    cout.flush();

    std::vector<BVHNode> compressed(stats.second);
    std::vector<uint32_t> skipped_accum(mNodes.size());

    for (int64_t i = stats.second-1, j = mNodes.size(), skipped = 0; i >= 0; --i) {
        while (mNodes[--j].isUnused())
            skipped++;
        BVHNode &new_node = compressed[i];
        new_node = mNodes[j];
        skipped_accum[j] = skipped;

        if (new_node.isInner()) {
            new_node.inner.rightChild =
                i + new_node.inner.rightChild - j -
                (skipped - skipped_accum[new_node.inner.rightChild]);
        }
    }

    mNodes = std::move(compressed);

    cout << "done. (took " << timeString(timer.value()) << ")" << endl;

    if (pointcloud) {
        cout << "Assigning disk radius .. ";
        cout.flush();

        auto map = [&](const tbb::blocked_range<uint32_t> &range, double radius_sum) -> double {
            std::vector<std::pair<Float, uint32_t>> result;
            for (uint32_t i = range.begin(); i < range.end(); ++i) {
                Float radius = std::numeric_limits<double>::infinity();
                if (findNearest(mV->col(i), radius) != (uint32_t) -1)
                    radius_sum += radius;
            }

            SHOW_PROGRESS_RANGE(range, mV->cols(), "Assigning disk radius");
            return radius_sum;
        };

        auto reduce = [](double radius_sum1, double radius_sum2) -> double {
            return radius_sum1 + radius_sum2;
        };

        tbb::blocked_range<uint32_t> range(0u, (uint32_t) mV->cols(), GRAIN_SIZE);
        mDiskRadius = tbb::parallel_deterministic_reduce(range, 0, map, reduce) / (double) range.size();
        mDiskRadius *= 3;
        refitBoundingBoxes();
        cout << "done. (took " << timeString(timer.value()) << ")" << endl;
    }

    mProgress = nullptr;
}

bool BVH::rayIntersect(Ray ray, uint32_t &idx, Float &t, Vector2f *uv) const {
    if (mNodes.empty())
        return false;

    uint32_t node_idx = 0, stack[64];
    uint32_t stack_idx = 0;
    bool hit = false;
    t = std::numeric_limits<Float>::infinity();

    if (mF->size() > 0) {
        while (true) {
            const BVHNode &node = mNodes[node_idx];

            if (!node.aabb.rayIntersect(ray)) {
                if (stack_idx == 0)
                    break;
                node_idx = stack[--stack_idx];
                continue;
            }

            if (node.isInner()) {
                stack[stack_idx++] = node.inner.rightChild;
                node_idx++;
                assert(stack_idx<64);
            } else {
                Float _t;
                Vector2f _uv;
                for (uint32_t i = node.start(), end = node.end(); i < end; ++i) {
                    if (rayIntersectTri(ray, mIndices[i], _t, _uv)) {
                        idx = mIndices[i];
                        t = ray.maxt = _t;
                        hit = true;
                        if (uv)
                            *uv = _uv;
                    }
                }
                if (stack_idx == 0)
                    break;
                node_idx = stack[--stack_idx];
                continue;
            }
        }
    } else {
        if (uv)
            *uv = Vector2f::Zero();
        while (true) {
            const BVHNode &node = mNodes[node_idx];

            if (!node.aabb.rayIntersect(ray)) {
                if (stack_idx == 0)
                    break;
                node_idx = stack[--stack_idx];
                continue;
            }

            if (node.isInner()) {
                stack[stack_idx++] = node.inner.rightChild;
                node_idx++;
                assert(stack_idx<64);
            } else {
                Float _t;
                for (uint32_t i = node.start(), end = node.end(); i < end; ++i) {
                    if (rayIntersectDisk(ray, mIndices[i], _t)) {
                        idx = mIndices[i];
                        t = ray.maxt = _t;
                        hit = true;
                    }
                }
                if (stack_idx == 0)
                    break;
                node_idx = stack[--stack_idx];
                continue;
            }
        }
    }

    return hit;
}

bool BVH::rayIntersect(Ray ray) const {
    if (mNodes.empty())
        return false;

    uint32_t node_idx = 0, stack[64];
    uint32_t stack_idx = 0;

    if (mF->size() > 0) {
        while (true) {
            const BVHNode &node = mNodes[node_idx];

            if (!node.aabb.rayIntersect(ray)) {
                if (stack_idx == 0)
                    break;
                node_idx = stack[--stack_idx];
                continue;
            }

            if (node.isInner()) {
                stack[stack_idx++] = node.inner.rightChild;
                node_idx++;
                assert(stack_idx<64);
            } else {
                Float t;
                Vector2f uv;
                for (uint32_t i = node.start(), end = node.end(); i < end; ++i)
                    if (rayIntersectTri(ray, mIndices[i], t, uv))
                        return true;
                if (stack_idx == 0)
                    break;
                node_idx = stack[--stack_idx];
                continue;
            }
        }
    } else {
        while (true) {
            const BVHNode &node = mNodes[node_idx];

            if (!node.aabb.rayIntersect(ray)) {
                if (stack_idx == 0)
                    break;
                node_idx = stack[--stack_idx];
                continue;
            }

            if (node.isInner()) {
                stack[stack_idx++] = node.inner.rightChild;
                node_idx++;
                assert(stack_idx<64);
            } else {
                Float t;
                for (uint32_t i = node.start(), end = node.end(); i < end; ++i)
                    if (rayIntersectDisk(ray, mIndices[i], t))
                        return true;
                if (stack_idx == 0)
                    break;
                node_idx = stack[--stack_idx];
                continue;
            }
        }
    }

    return false;
}

void BVH::findNearestWithRadius(const Vector3f &p, Float radius,
                                std::vector<uint32_t> &result,
                                bool includeSelf) const {
    result.clear();

    uint32_t node_idx = 0, stack[64];
    uint32_t stack_idx = 0;
    Float radius2 = radius*radius;

    while (true) {
        const BVHNode &node = mNodes[node_idx];
        if (node.aabb.squaredDistanceTo(p) > radius2) {
            if (stack_idx == 0)
                break;
            node_idx = stack[--stack_idx];
            continue;
        }

        if (node.isInner()) {
            stack[stack_idx++] = node.inner.rightChild;
            node_idx++;
            assert(stack_idx<64);
        } else {
            uint32_t start = node.leaf.start, end = start + node.leaf.size;
            for (uint32_t i = start; i < end; ++i) {
                uint32_t f = mIndices[i];
                Vector3f pointPos = Vector3f::Zero();
                if (mF->size() > 0) {
                    for (int j=0; j<3; ++j)
                        pointPos += mV->col((*mF)(j, f));
                    pointPos *= 1.0f / 3.0f;
                } else {
                    pointPos = mV->col(f);
                }
                Float pointDist2 = (pointPos-p).squaredNorm();
                if (pointDist2 < radius2 && (pointDist2 != 0 || includeSelf))
                    result.push_back(f);
            }
            if (stack_idx == 0)
                break;
            node_idx = stack[--stack_idx];
            continue;
        }
    }
}

uint32_t BVH::findNearest(const Vector3f &p, Float &radius, bool includeSelf) const {
    uint32_t node_idx = 0, stack[64];
    uint32_t stack_idx = 0;
    Float radius2 = radius*radius;
    uint32_t result = (uint32_t) -1;

    while (true) {
        const BVHNode &node = mNodes[node_idx];
        if (node.aabb.squaredDistanceTo(p) > radius2) {
            if (stack_idx == 0)
                break;
            node_idx = stack[--stack_idx];
            continue;
        }

        if (node.isInner()) {
            uint32_t left = node_idx + 1, right = node.inner.rightChild;
            Float distLeft = mNodes[left].aabb.squaredDistanceTo(p);
            Float distRight = mNodes[right].aabb.squaredDistanceTo(p);
            if (distLeft < distRight) {
                node_idx = left;
                if (distRight < radius2)
                    stack[stack_idx++] = right;
            } else {
                node_idx = right;
                if (distLeft < radius2)
                    stack[stack_idx++] = left;
            }
            assert(stack_idx<64);
        } else {
            uint32_t start = node.leaf.start, end = start + node.leaf.size;
            for (uint32_t i = start; i < end; ++i) {
                uint32_t f = mIndices[i];
                Vector3f pointPos = Vector3f::Zero();
                if (mF->size() > 0) {
                    for (int j=0; j<3; ++j)
                        pointPos += mV->col((*mF)(j, f));
                    pointPos *= 1.0f / 3.0f;
                } else {
                    pointPos = mV->col(f);
                }
                Float pointDist2 = (pointPos-p).squaredNorm();

                if (pointDist2 < radius2 && (pointDist2 != 0 || includeSelf)) {
                    radius2 = pointDist2;
                    result = f;
                }
            }
            if (stack_idx == 0)
                break;
            node_idx = stack[--stack_idx];
            continue;
        }
    }
    radius = std::sqrt(radius2);
    return result;
}

void BVH::findKNearest(const Vector3f &p, uint32_t k, Float &radius,
                       std::vector<std::pair<Float, uint32_t>> &result,
                       bool includeSelf) const {
    result.clear();

    uint32_t node_idx = 0, stack[64];
    uint32_t stack_idx = 0;
    Float radius2 = radius*radius;
    bool isHeap = false;
    auto comp = [](const std::pair<Float, uint32_t> &v1, const std::pair<Float, uint32_t> &v2) {
        return v1.first < v2.first;
    };

    while (true) {
        const BVHNode &node = mNodes[node_idx];
        if (node.aabb.squaredDistanceTo(p) > radius2) {
            if (stack_idx == 0)
                break;
            node_idx = stack[--stack_idx];
            continue;
        }

        if (node.isInner()) {
            uint32_t left = node_idx + 1, right = node.inner.rightChild;
            Float distLeft = mNodes[left].aabb.squaredDistanceTo(p);
            Float distRight = mNodes[right].aabb.squaredDistanceTo(p);
            if (distLeft < distRight) {
                node_idx = left;
                if (distRight < radius2)
                    stack[stack_idx++] = right;
            } else {
                node_idx = right;
                if (distLeft < radius2)
                    stack[stack_idx++] = left;
            }
            assert(stack_idx<64);
        } else {
            uint32_t start = node.leaf.start, end = start + node.leaf.size;
            for (uint32_t i = start; i < end; ++i) {
                uint32_t f = mIndices[i];
                Vector3f pointPos = Vector3f::Zero();
                if (mF->size() > 0) {
                    for (int j=0; j<3; ++j)
                        pointPos += mV->col((*mF)(j, f));
                    pointPos *= 1.0f / 3.0f;
                } else {
                    pointPos = mV->col(f);
                }
                Float pointDist2 = (pointPos-p).squaredNorm();

                if (pointDist2 < radius2 && (pointDist2 != 0 || includeSelf)) {
                    if (result.size() < k) {
                        result.push_back(std::make_pair(pointDist2, f));
                    } else {
                        if (!isHeap) {
                            /* Establish the max-heap property */
                            std::make_heap(result.begin(), result.end(), comp);
                            isHeap = true;
                        }

                        result.push_back(std::make_pair(pointDist2, f));
                        std::push_heap(result.begin(), result.end(), comp);
                        std::pop_heap(result.begin(), result.end(), comp);
                        result.pop_back();

                        /* Reduce the search radius accordingly */
                        radius2 = result[0].first;
                    }
                }
            }
            if (stack_idx == 0)
                break;
            node_idx = stack[--stack_idx];
            continue;
        }
    }
    radius = std::sqrt(radius2);
}

void BVH::findKNearest(const Vector3f &p, const Vector3f &n, uint32_t k,
                       Float &radius,
                       std::vector<std::pair<Float, uint32_t> > &result,
                       Float angleThresh, bool includeSelf) const {
    result.clear();

    uint32_t node_idx = 0, stack[64];
    uint32_t stack_idx = 0;
    Float radius2 = radius*radius;
    bool isHeap = false;
    angleThresh = std::cos(angleThresh * M_PI/180);
    auto comp = [](const std::pair<Float, uint32_t> &v1, const std::pair<Float, uint32_t> &v2) {
        return v1.first < v2.first;
    };

    while (true) {
        const BVHNode &node = mNodes[node_idx];
        if (node.aabb.squaredDistanceTo(p) > radius2) {
            if (stack_idx == 0)
                break;
            node_idx = stack[--stack_idx];
            continue;
        }

        if (node.isInner()) {
            uint32_t left = node_idx + 1, right = node.inner.rightChild;
            Float distLeft = mNodes[left].aabb.squaredDistanceTo(p);
            Float distRight = mNodes[right].aabb.squaredDistanceTo(p);
            if (distLeft < distRight) {
                node_idx = left;
                if (distRight < radius2)
                    stack[stack_idx++] = right;
            } else {
                node_idx = right;
                if (distLeft < radius2)
                    stack[stack_idx++] = left;
            }
            assert(stack_idx<64);
        } else {
            uint32_t start = node.leaf.start, end = start + node.leaf.size;
            for (uint32_t i = start; i < end; ++i) {
                uint32_t f = mIndices[i];
                Vector3f pointPos = Vector3f::Zero();
                if (mF->size() > 0) {
                    for (int j=0; j<3; ++j)
                        pointPos += mV->col((*mF)(j, f));
                    pointPos *= 1.0f / 3.0f;
                } else {
                    pointPos = mV->col(f);
                }
                Vector3f pointNormal = Vector3f::Zero();
                if (mF->size() > 0) {
                    for (int j=0; j<3; ++j)
                        pointNormal += mN->col((*mF)(j, f));
                } else {
                    pointNormal = mN->col(f);
                }
                Float pointDist2 = (pointPos-p).squaredNorm();

                if (pointDist2 < radius2 && (pointDist2 != 0 || includeSelf) && pointNormal.dot(n) > angleThresh) {
                    if (result.size() < k) {
                        result.push_back(std::make_pair(pointDist2, f));
                    } else {
                        if (!isHeap) {
                            /* Establish the max-heap property */
                            std::make_heap(result.begin(), result.end(), comp);
                            isHeap = true;
                        }

                        result.push_back(std::make_pair(pointDist2, f));
                        std::push_heap(result.begin(), result.end(), comp);
                        std::pop_heap(result.begin(), result.end(), comp);
                        result.pop_back();

                        /* Reduce the search radius accordingly */
                        radius2 = result[0].first;
                    }
                }
            }
            if (stack_idx == 0)
                break;
            node_idx = stack[--stack_idx];
            continue;
        }
    }
    radius = std::sqrt(radius2);
}

bool BVH::rayIntersectTri(const Ray &ray, uint32_t i, Float &t, Vector2f &uv) const {
    const Vector3f &p0 = mV->col((*mF)(0, i)),
                   &p1 = mV->col((*mF)(1, i)),
                   &p2 = mV->col((*mF)(2, i));

    Vector3f edge1 = p1 - p0, edge2 = p2 - p0;
    Vector3f pvec = ray.d.cross(edge2);

    Float det = edge1.dot(pvec);
    if (det == 0.0f)
        return false;
    Float inv_det = 1.0f / det;

    Vector3f tvec = ray.o - p0;
    Float u = tvec.dot(pvec) * inv_det;
    if (u < 0.0f || u > 1.0f)
        return false;

    Vector3f qvec = tvec.cross(edge1);
    Float v = ray.d.dot(qvec) * inv_det;

    if (v < 0.0f || u + v > 1.0f)
        return false;

    Float tempT = edge2.dot(qvec) * inv_det;
    if (tempT < ray.mint || tempT > ray.maxt)
        return false;

    t = tempT;
    uv << u, v;
    return true;
}

bool BVH::rayIntersectDisk(const Ray &ray, uint32_t i, Float &t) const {
    Vector3f v = mV->col(i), n = mN->col(i);
    Float dp = ray.d.dot(n);

    if (std::abs(dp) < RCPOVERFLOW)
        return false;

    t = (n.dot(v) - n.dot(ray.o)) / dp;

    return (ray(t)-v).squaredNorm() < mDiskRadius*mDiskRadius;
}

void BVH::printStatistics() const {
    cout << endl;
    cout << "Bounding Volume Hierarchy statistics:" << endl;
    cout << "    Tree nodes         : " << memString(sizeof(BVHNode) * mNodes.size()) << endl;
    cout << "    Index buffer       : " << memString(sizeof(uint32_t) * mF->size()) << endl;
    cout << "    Total              : "
         << memString(sizeof(BVHNode) * mNodes.size() + sizeof(uint32_t) * mF->size()) << endl;
}

std::pair<Float, uint32_t> BVH::statistics(uint32_t node_idx) const {
    const BVHNode &node = mNodes[node_idx];
    if (node.isLeaf()) {
        return std::make_pair(T_tri * node.leaf.size, 1u);
    } else {
        std::pair<Float, uint32_t> stats_left = statistics(node_idx + 1u);
        std::pair<Float, uint32_t> stats_right = statistics(node.inner.rightChild);
        Float saLeft = mNodes[node_idx + 1u].aabb.surfaceArea();
        Float saRight = mNodes[node.inner.rightChild].aabb.surfaceArea();
        Float saCur = node.aabb.surfaceArea();
        Float sahCost = 2 * BVH::T_aabb + (saLeft * stats_left.first +
                                           saRight * stats_right.first) / saCur;

        return std::make_pair(
            sahCost,
            stats_left.second + stats_right.second + 1u
        );
    }
}

BVH::~BVH() {
    delete[] mIndices;
}

void BVH::refitBoundingBoxes(uint32_t node_idx) {
    BVHNode &node = mNodes[node_idx];
    if (node.isLeaf()) {
        for (uint32_t i=node.start(); i<node.end(); ++i) {
            uint32_t j = mIndices[i];
            const Vector3f &p = mV->col(j), &n = mN->col(j);
            Vector3f s, t;
            coordinate_system(n, s, t);
            AABB aabb;
            for (int k=0; k<4; ++k)
                aabb.expandBy(p + mDiskRadius *  (
                    ((k&1)*2 - 1) * s +
                    ((k&2) - 1) * t));
            node.aabb = aabb;
        }
    } else {
        uint32_t left = node_idx + 1u, right = node.inner.rightChild;
        refitBoundingBoxes(left);
        refitBoundingBoxes(right);
        node.aabb = AABB::merge(mNodes[left].aabb, mNodes[right].aabb);
    }
}

