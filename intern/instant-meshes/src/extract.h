/*
    extract.h: Mesh extraction from existing orientation/position fields

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include "hierarchy.h"
#include <set>

struct TaggedLink {
    uint32_t id;
    uint8_t flag;
    TaggedLink(uint32_t id) : id(id), flag(0) { }
    bool used() const { return flag & 1; }
    void markUsed() { flag |= 1; }
};

class BVH;

extern void
extract_graph(const MultiResolutionHierarchy &mRes, bool extrinsic, int rosy, int posy,
              std::vector<std::vector<TaggedLink>> &adj_new,
              MatrixXf &O_new, MatrixXf &N_new,
              const std::set<uint32_t> &crease_in,
              std::set<uint32_t> &crease_out,
              bool deterministic, bool remove_spurious_vertices = true,
              bool remove_unnecessary_triangles = true,
              bool snap_vertices = true);

extern void extract_faces(std::vector<std::vector<TaggedLink> > &adj,
                          MatrixXf &O, MatrixXf &N, MatrixXf &Nf, MatrixXu &F,
                          int posy, Float scale, std::set<uint32_t> &crease,
                          bool fill_holes = true, bool pure_quad = true,
                          BVH *bvh = nullptr, int smooth_iterations = 2);
