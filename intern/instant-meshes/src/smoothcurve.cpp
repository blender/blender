/*
    smoothcurve.cpp: Helper routines to compute smooth curves on meshes to enable
    intuitive stroke annotations

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include "smoothcurve.h"
#include "dedge.h"
#include "bvh.h"
#include <queue>
#include <map>
#include <set>

bool smooth_curve(const BVH *bvh, const VectorXu &E2E, std::vector<CurvePoint> &curve, bool watertight) {
    const MatrixXu &F = *bvh->F();
    const MatrixXf &V = *bvh->V(), &N = *bvh->N();
    cout << endl;

    std::vector<CurvePoint> curve_new;
    std::vector<Float> weight;
    std::vector<uint32_t> path;

    cout << "Input: " << curve.size() << " vertices" << endl;

    for (int it=0;; ++it) {
        if (curve.size() < 2)
            return false;

        for (uint32_t it2=0; it2<curve.size(); ++it2) {
            curve_new.clear();
            curve_new.push_back(curve[0]);
            for (uint32_t i=1; i<curve.size()-1; ++i) {
                Vector3f p_new = 0.5f * (curve[i-1].p + curve[i+1].p);
                Vector3f n_new = (curve[i-1].n + curve[i+1].n).normalized();
                Float maxlength = (curve[i-1].p - curve[i+1].p).norm()*2;

                Ray ray1(p_new,  n_new, 0, maxlength);
                Ray ray2(p_new, -n_new, 0, maxlength);
                uint32_t idx1 = 0, idx2 = 0;
                Float t1 = 0, t2 = 0;
                Vector2f uv1, uv2;
                bool hit1 = bvh->rayIntersect(ray1, idx1, t1, &uv1);
                bool hit2 = bvh->rayIntersect(ray2, idx2, t2, &uv2);

                if (!hit1 && !hit2)
                    continue;

                CurvePoint pt;
                if (t1 < t2) {
                    pt.p = ray1(t1);
                    pt.f = idx1;
                    pt.n = ((1 - uv1.sum()) * N.col(F(0, idx1)) + uv1.x() * N.col(F(1, idx1)) + uv1.y() * N.col(F(2, idx1))).normalized();
                } else {
                    pt.p = ray2(t2);
                    pt.f = idx2;
                    pt.n = ((1 - uv2.sum()) * N.col(F(0, idx2)) + uv2.x() * N.col(F(1, idx2)) + uv2.y() * N.col(F(2, idx2))).normalized();
                }
                curve_new.push_back(pt);
            }
            curve_new.push_back(curve[curve.size()-1]);
            curve.swap(curve_new);
        }

        if (!watertight && it == 1)
            break;

        curve_new.clear();
        curve_new.push_back(curve[0]);
        for (uint32_t i=1; i<curve.size(); ++i) {
            if (!astar(F, E2E, V, curve[i-1].f, curve[i].f, path))
                return false;

            auto closest = [](const Vector3f &p0, const Vector3f &p1, const Vector3f &target) -> Vector3f {
                Vector3f d = (p1-p0).normalized();
                return p0 + d * std::min(std::max((target-p0).dot(d), 0.0f), (p0-p1).norm());
            };

            if (path.size() > 2) {
                uint32_t base = curve_new.size() - 1;
                for (uint32_t j=1; j<path.size()-1; ++j) {
                    uint32_t f = path[j];
                    Vector3f p0 = V.col(F(0, f)), p1 = V.col(F(1, f)), p2 = V.col(F(2, f));
                    CurvePoint pt2;
                    pt2.f = f;
                    pt2.n = (p1-p0).cross(p2-p0).normalized();
                    pt2.p = (p0+p1+p2) * (1.0f / 3.0f);
                    curve_new.push_back(pt2);
                }
                curve_new.push_back(curve[i]);

                for (uint32_t q=1; q<path.size()-1; ++q) {
                    for (uint32_t j=1; j<path.size()-1; ++j) {
                        Float bestDist1 = std::numeric_limits<Float>::infinity();
                        Float bestDist2 = std::numeric_limits<Float>::infinity();
                        Vector3f bestPt1 = Vector3f::Zero(), bestPt2 = Vector3f::Zero();
                        uint32_t f = path[j];
                        for (uint32_t k=0; k<3; ++k) {
                            Vector3f closest1 = closest(V.col(F(k, f)), V.col(F((k + 1) % 3, f)), curve_new[base+j-1].p);
                            Vector3f closest2 = closest(V.col(F(k, f)), V.col(F((k + 1) % 3, f)), curve_new[base+j+1].p);
                            Float dist1 = (closest1 - curve_new[base+j-1].p).norm();
                            Float dist2 = (closest2 - curve_new[base+j+1].p).norm();
                            if (dist1 < bestDist1) { bestDist1 = dist1; bestPt1 = closest1; }
                            if (dist2 < bestDist2) { bestDist2 = dist2; bestPt2 = closest2; }
                        }
                        curve_new[base+j].p = (bestPt1 + bestPt2) * 0.5f;
                    }
                }
            } else {
                curve_new.push_back(curve[i]);
            }
        }
        curve.swap(curve_new);

        curve_new.clear();
        curve_new.push_back(curve[0]);
        weight.clear();
        weight.push_back(1.0f);
        for (uint32_t i=0; i<curve.size(); ++i) {
            auto &cur = curve_new[curve_new.size()-1];
            auto &cur_weight = weight[weight.size()-1];
            if (cur.f == curve[i].f) {
                cur.p += curve[i].p;
                cur.n += curve[i].n;
                cur_weight += 1;
            } else {
                curve_new.push_back(curve[i]);
                weight.push_back(1.f);
            }
        }
        for (uint32_t i=0; i<curve_new.size(); ++i) {
            curve_new[i].p /= weight[i];
            curve_new[i].n.normalize();
        }
        curve_new[0] = curve[0];
        curve_new[curve_new.size()-1] = curve[curve.size()-1];
        if (curve_new.size() < 2 || curve_new[0].f == curve_new[curve_new.size()-1].f)
            return false;
        curve_new.swap(curve);

        if (it > 2)
            break;
    }
    cout << "Smoothed curve: " << curve.size() << " vertices" << endl;

    return true;
}

inline bool astar(const MatrixXu &F, const VectorXu &E2E, const MatrixXf &V, uint32_t start, uint32_t end, std::vector<uint32_t> &path) {
    typedef std::pair<uint32_t, Float> Entry;

    struct comp {
        bool operator() (const Entry & lhs, const Entry & rhs) const { return lhs.second > rhs.second; }
    };

    auto eucldist = [&](uint32_t i, uint32_t j) -> Float {
        Vector3f diff = Vector3f::Zero();
        for (uint32_t k=0; k<3; ++k)
            diff += V.col(F(k, i)) - V.col(F(k, j));
        return diff.norm() * (1.0f / 3.0f);
    };

    std::map<uint32_t, Float> g_score;
    std::map<uint32_t, uint32_t> came_from;
    std::set<uint32_t> closed;
    std::priority_queue<Entry, std::vector<Entry>, comp> pq;

    path.clear();
    g_score[start] = 0;
    pq.push(Entry(start, g_score[start] + eucldist(start, end)));

    uint32_t iter = 0;
    while (true) {
        if (pq.empty()) {
            /* Internal error - graph disconnected? */
            return false;
        }
        iter++;
        uint32_t current = pq.top().first;
        Float currentDist = pq.top().second;
        if (current == end)
            break;
        pq.pop();
        if (currentDist != g_score[current] + eucldist(current, end))
            continue;
        closed.insert(current);

        for (uint32_t i=0; i<3; ++i) {
            uint32_t neighbor = E2E[current*3+i];
            if (neighbor == INVALID)
                continue;

            neighbor /= 3;
            if (closed.find(neighbor) != closed.end())
                continue;

            Float tentative_g_score = g_score[current] + eucldist(current, neighbor);

            bool is_open = g_score.find(neighbor) != g_score.end();
            bool closer = is_open && tentative_g_score < g_score[neighbor];

            if (!is_open || closer) {
                g_score[neighbor] = tentative_g_score;
                came_from[neighbor] = current;
                pq.push(Entry(neighbor, g_score[neighbor] + eucldist(neighbor, end)));
            }
        }
    }
    uint32_t current = end;
    path.push_back(current);
    while (came_from.find(path[path.size()-1]) != came_from.end()) {
        current = came_from[current];
        path.push_back(current);
    }
    std::reverse(path.begin(), path.end());
    return true;
}
