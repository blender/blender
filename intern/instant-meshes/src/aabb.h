/*
    aabb.h -- basic axis-aligned bounding box & ray intersection code

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include "common.h"

struct Ray {
    Vector3f o, d;
    Float mint, maxt;

    Ray(const Vector3f &o, const Vector3f &d) :
        o(o), d(d), mint(0), maxt(std::numeric_limits<Float>::infinity()) { }

    Ray(const Vector3f &o, const Vector3f &d, Float mint, Float maxt) :
        o(o), d(d), mint(mint), maxt(maxt) { }

    Vector3f operator()(Float t) const { return o + t*d; }
};

struct AABB {
    Vector3f min, max;

    AABB() { clear(); }

    AABB(const Vector3f &min, const Vector3f &max) : min(min), max(max) {}

    void clear() {
        const Float inf = std::numeric_limits<Float>::infinity();
        min.setConstant(inf);
        max.setConstant(-inf);
    }

    void expandBy(const Vector3f &p) {
        min = min.cwiseMin(p);
        max = max.cwiseMax(p);
    }

    void expandBy(const AABB &aabb) {
        min = min.cwiseMin(aabb.min);
        max = max.cwiseMax(aabb.max);
    }

    bool contains(const Vector3f &p) {
        return (p.array() >= min.array()).all() &&
               (p.array() <= max.array()).all();
    }

    bool rayIntersect(const Ray &ray) const {
        Float nearT = -std::numeric_limits<Float>::infinity();
        Float farT = std::numeric_limits<Float>::infinity();

        for (int i=0; i<3; i++) {
            Float origin = ray.o[i];
            Float minVal = min[i], maxVal = max[i];

            if (ray.d[i] == 0) {
                if (origin < minVal || origin > maxVal)
                    return false;
            } else {
                Float t1 = (minVal - origin) / ray.d[i];
                Float t2 = (maxVal - origin) / ray.d[i];

                if (t1 > t2)
                    std::swap(t1, t2);

                nearT = std::max(t1, nearT);
                farT = std::min(t2, farT);

                if (!(nearT <= farT))
                    return false;
            }
        }

        return ray.mint <= farT && nearT <= ray.maxt;
    }

    Float squaredDistanceTo(const Vector3f &p) const {
        Float result = 0;
        for (int i=0; i<3; ++i) {
            Float value = 0;
            if (p[i] < min[i])
                value = min[i] - p[i];
            else if (p[i] > max[i])
                value = p[i] - max[i];
            result += value*value;
        }
        return result;
    }

    int largestAxis() const {
        Vector3f extents = max-min;

        if (extents[0] >= extents[1] && extents[0] >= extents[2])
            return 0;
        else if (extents[1] >= extents[0] && extents[1] >= extents[2])
            return 1;
        else
            return 2;
    }

    Float surfaceArea() const {
        Vector3f d = max - min;
        return 2.0f * (d[0]*d[1] + d[0]*d[2] + d[1]*d[2]);
    }

    Vector3f center() const {
        return 0.5f * (min + max);
    }

    static AABB merge(const AABB &aabb1, const AABB &aabb2) {
        return AABB(aabb1.min.cwiseMin(aabb2.min), aabb1.max.cwiseMax(aabb2.max));
    }
};
