/*
 * Original code Copyright 2017, Intel Corporation
 * Modifications Copyright 2018, Blender Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of Intel Corporation nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bvh/bvh8.h"

#include "render/mesh.h"
#include "render/object.h"

#include "bvh/bvh_node.h"
#include "bvh/bvh_unaligned.h"

CCL_NAMESPACE_BEGIN

BVH8::BVH8(const BVHParams &params_, const vector<Object *> &objects_) : BVH(params_, objects_)
{
}

namespace {

BVHNode *bvh_node_merge_children_recursively(const BVHNode *node)
{
  if (node->is_leaf()) {
    return new LeafNode(*reinterpret_cast<const LeafNode *>(node));
  }
  /* Collect nodes of two layer deeper, allowing us to have more childrem in
   * an inner layer. */
  assert(node->num_children() <= 2);
  const BVHNode *children[8];
  const BVHNode *child0 = node->get_child(0);
  const BVHNode *child1 = node->get_child(1);
  int num_children = 0;
  if (child0->is_leaf()) {
    children[num_children++] = child0;
  }
  else {
    const BVHNode *child00 = child0->get_child(0), *child01 = child0->get_child(1);
    if (child00->is_leaf()) {
      children[num_children++] = child00;
    }
    else {
      children[num_children++] = child00->get_child(0);
      children[num_children++] = child00->get_child(1);
    }
    if (child01->is_leaf()) {
      children[num_children++] = child01;
    }
    else {
      children[num_children++] = child01->get_child(0);
      children[num_children++] = child01->get_child(1);
    }
  }
  if (child1->is_leaf()) {
    children[num_children++] = child1;
  }
  else {
    const BVHNode *child10 = child1->get_child(0), *child11 = child1->get_child(1);
    if (child10->is_leaf()) {
      children[num_children++] = child10;
    }
    else {
      children[num_children++] = child10->get_child(0);
      children[num_children++] = child10->get_child(1);
    }
    if (child11->is_leaf()) {
      children[num_children++] = child11;
    }
    else {
      children[num_children++] = child11->get_child(0);
      children[num_children++] = child11->get_child(1);
    }
  }
  /* Merge children in subtrees. */
  BVHNode *children4[8];
  for (int i = 0; i < num_children; ++i) {
    children4[i] = bvh_node_merge_children_recursively(children[i]);
  }
  /* Allocate new node. */
  BVHNode *node8 = new InnerNode(node->bounds, children4, num_children);
  /* TODO(sergey): Consider doing this from the InnerNode() constructor.
   * But in order to do this nicely need to think of how to pass all the
   * parameters there. */
  if (node->is_unaligned) {
    node8->is_unaligned = true;
    node8->aligned_space = new Transform();
    *node8->aligned_space = *node->aligned_space;
  }
  return node8;
}

}  // namespace

BVHNode *BVH8::widen_children_nodes(const BVHNode *root)
{
  if (root == NULL) {
    return NULL;
  }
  if (root->is_leaf()) {
    return const_cast<BVHNode *>(root);
  }
  BVHNode *root8 = bvh_node_merge_children_recursively(root);
  /* TODO(sergey): Pack children nodes to parents which has less that 4
   * children. */
  return root8;
}

void BVH8::pack_leaf(const BVHStackEntry &e, const LeafNode *leaf)
{
  float4 data[BVH_ONODE_LEAF_SIZE];
  memset(data, 0, sizeof(data));
  if (leaf->num_triangles() == 1 && pack.prim_index[leaf->lo] == -1) {
    /* object */
    data[0].x = __int_as_float(~(leaf->lo));
    data[0].y = __int_as_float(0);
  }
  else {
    /* triangle */
    data[0].x = __int_as_float(leaf->lo);
    data[0].y = __int_as_float(leaf->hi);
  }
  data[0].z = __uint_as_float(leaf->visibility);
  if (leaf->num_triangles() != 0) {
    data[0].w = __uint_as_float(pack.prim_type[leaf->lo]);
  }

  memcpy(&pack.leaf_nodes[e.idx], data, sizeof(float4) * BVH_ONODE_LEAF_SIZE);
}

void BVH8::pack_inner(const BVHStackEntry &e, const BVHStackEntry *en, int num)
{
  bool has_unaligned = false;
  /* Check whether we have to create unaligned node or all nodes are aligned
   * and we can cut some corner here.
   */
  if (params.use_unaligned_nodes) {
    for (int i = 0; i < num; i++) {
      if (en[i].node->is_unaligned) {
        has_unaligned = true;
        break;
      }
    }
  }
  if (has_unaligned) {
    /* There's no unaligned children, pack into AABB node. */
    pack_unaligned_inner(e, en, num);
  }
  else {
    /* Create unaligned node with orientation transform for each of the
     * children.
     */
    pack_aligned_inner(e, en, num);
  }
}

void BVH8::pack_aligned_inner(const BVHStackEntry &e, const BVHStackEntry *en, int num)
{
  BoundBox bounds[8];
  int child[8];
  for (int i = 0; i < num; ++i) {
    bounds[i] = en[i].node->bounds;
    child[i] = en[i].encodeIdx();
  }
  pack_aligned_node(
      e.idx, bounds, child, e.node->visibility, e.node->time_from, e.node->time_to, num);
}

void BVH8::pack_aligned_node(int idx,
                             const BoundBox *bounds,
                             const int *child,
                             const uint visibility,
                             const float time_from,
                             const float time_to,
                             const int num)
{
  float8 data[8];
  memset(data, 0, sizeof(data));

  data[0].a = __uint_as_float(visibility & ~PATH_RAY_NODE_UNALIGNED);
  data[0].b = time_from;
  data[0].c = time_to;

  for (int i = 0; i < num; i++) {
    float3 bb_min = bounds[i].min;
    float3 bb_max = bounds[i].max;

    data[1][i] = bb_min.x;
    data[2][i] = bb_max.x;
    data[3][i] = bb_min.y;
    data[4][i] = bb_max.y;
    data[5][i] = bb_min.z;
    data[6][i] = bb_max.z;

    data[7][i] = __int_as_float(child[i]);
  }

  for (int i = num; i < 8; i++) {
    /* We store BB which would never be recorded as intersection
     * so kernel might safely assume there are always 4 child nodes.
     */
    data[1][i] = FLT_MAX;
    data[2][i] = -FLT_MAX;

    data[3][i] = FLT_MAX;
    data[4][i] = -FLT_MAX;

    data[5][i] = FLT_MAX;
    data[6][i] = -FLT_MAX;

    data[7][i] = __int_as_float(0);
  }

  memcpy(&pack.nodes[idx], data, sizeof(float4) * BVH_ONODE_SIZE);
}

void BVH8::pack_unaligned_inner(const BVHStackEntry &e, const BVHStackEntry *en, int num)
{
  Transform aligned_space[8];
  BoundBox bounds[8];
  int child[8];
  for (int i = 0; i < num; ++i) {
    aligned_space[i] = en[i].node->get_aligned_space();
    bounds[i] = en[i].node->bounds;
    child[i] = en[i].encodeIdx();
  }
  pack_unaligned_node(e.idx,
                      aligned_space,
                      bounds,
                      child,
                      e.node->visibility,
                      e.node->time_from,
                      e.node->time_to,
                      num);
}

void BVH8::pack_unaligned_node(int idx,
                               const Transform *aligned_space,
                               const BoundBox *bounds,
                               const int *child,
                               const uint visibility,
                               const float time_from,
                               const float time_to,
                               const int num)
{
  float8 data[BVH_UNALIGNED_ONODE_SIZE];
  memset(data, 0, sizeof(data));

  data[0].a = __uint_as_float(visibility | PATH_RAY_NODE_UNALIGNED);
  data[0].b = time_from;
  data[0].c = time_to;

  for (int i = 0; i < num; i++) {
    Transform space = BVHUnaligned::compute_node_transform(bounds[i], aligned_space[i]);

    data[1][i] = space.x.x;
    data[2][i] = space.x.y;
    data[3][i] = space.x.z;

    data[4][i] = space.y.x;
    data[5][i] = space.y.y;
    data[6][i] = space.y.z;

    data[7][i] = space.z.x;
    data[8][i] = space.z.y;
    data[9][i] = space.z.z;

    data[10][i] = space.x.w;
    data[11][i] = space.y.w;
    data[12][i] = space.z.w;

    data[13][i] = __int_as_float(child[i]);
  }

  for (int i = num; i < 8; i++) {
    /* We store BB which would never be recorded as intersection
     * so kernel might safely assume there are always 4 child nodes.
     */

    data[1][i] = NAN;
    data[2][i] = NAN;
    data[3][i] = NAN;

    data[4][i] = NAN;
    data[5][i] = NAN;
    data[6][i] = NAN;

    data[7][i] = NAN;
    data[8][i] = NAN;
    data[9][i] = NAN;

    data[10][i] = NAN;
    data[11][i] = NAN;
    data[12][i] = NAN;

    data[13][i] = __int_as_float(0);
  }

  memcpy(&pack.nodes[idx], data, sizeof(float4) * BVH_UNALIGNED_ONODE_SIZE);
}

/* Quad SIMD Nodes */

void BVH8::pack_nodes(const BVHNode *root)
{
  /* Calculate size of the arrays required. */
  const size_t num_nodes = root->getSubtreeSize(BVH_STAT_NODE_COUNT);
  const size_t num_leaf_nodes = root->getSubtreeSize(BVH_STAT_LEAF_COUNT);
  assert(num_leaf_nodes <= num_nodes);
  const size_t num_inner_nodes = num_nodes - num_leaf_nodes;
  size_t node_size;
  if (params.use_unaligned_nodes) {
    const size_t num_unaligned_nodes = root->getSubtreeSize(BVH_STAT_UNALIGNED_INNER_COUNT);
    node_size = (num_unaligned_nodes * BVH_UNALIGNED_ONODE_SIZE) +
                (num_inner_nodes - num_unaligned_nodes) * BVH_ONODE_SIZE;
  }
  else {
    node_size = num_inner_nodes * BVH_ONODE_SIZE;
  }
  /* Resize arrays. */
  pack.nodes.clear();
  pack.leaf_nodes.clear();
  /* For top level BVH, first merge existing BVH's so we know the offsets. */
  if (params.top_level) {
    pack_instances(node_size, num_leaf_nodes * BVH_ONODE_LEAF_SIZE);
  }
  else {
    pack.nodes.resize(node_size);
    pack.leaf_nodes.resize(num_leaf_nodes * BVH_ONODE_LEAF_SIZE);
  }

  int nextNodeIdx = 0, nextLeafNodeIdx = 0;

  vector<BVHStackEntry> stack;
  stack.reserve(BVHParams::MAX_DEPTH * 2);
  if (root->is_leaf()) {
    stack.push_back(BVHStackEntry(root, nextLeafNodeIdx++));
  }
  else {
    stack.push_back(BVHStackEntry(root, nextNodeIdx));
    nextNodeIdx += root->has_unaligned() ? BVH_UNALIGNED_ONODE_SIZE : BVH_ONODE_SIZE;
  }

  while (stack.size()) {
    BVHStackEntry e = stack.back();
    stack.pop_back();

    if (e.node->is_leaf()) {
      /* leaf node */
      const LeafNode *leaf = reinterpret_cast<const LeafNode *>(e.node);
      pack_leaf(e, leaf);
    }
    else {
      /* Inner node. */
      /* Collect nodes. */
      const BVHNode *children[8];
      int num_children = e.node->num_children();
      /* Push entries on the stack. */
      for (int i = 0; i < num_children; ++i) {
        int idx;
        children[i] = e.node->get_child(i);
        if (children[i]->is_leaf()) {
          idx = nextLeafNodeIdx++;
        }
        else {
          idx = nextNodeIdx;
          nextNodeIdx += children[i]->has_unaligned() ? BVH_UNALIGNED_ONODE_SIZE : BVH_ONODE_SIZE;
        }
        stack.push_back(BVHStackEntry(children[i], idx));
      }
      /* Set node. */
      pack_inner(e, &stack[stack.size() - num_children], num_children);
    }
  }

  assert(node_size == nextNodeIdx);
  /* Root index to start traversal at, to handle case of single leaf node. */
  pack.root_index = (root->is_leaf()) ? -1 : 0;
}

void BVH8::refit_nodes()
{
  assert(!params.top_level);

  BoundBox bbox = BoundBox::empty;
  uint visibility = 0;
  refit_node(0, (pack.root_index == -1) ? true : false, bbox, visibility);
}

void BVH8::refit_node(int idx, bool leaf, BoundBox &bbox, uint &visibility)
{
  if (leaf) {
    int4 *data = &pack.leaf_nodes[idx];
    int4 c = data[0];
    /* Refit leaf node. */
    for (int prim = c.x; prim < c.y; prim++) {
      int pidx = pack.prim_index[prim];
      int tob = pack.prim_object[prim];
      Object *ob = objects[tob];

      if (pidx == -1) {
        /* Object instance. */
        bbox.grow(ob->bounds);
      }
      else {
        /* Primitives. */
        const Mesh *mesh = ob->mesh;

        if (pack.prim_type[prim] & PRIMITIVE_ALL_CURVE) {
          /* Curves. */
          int str_offset = (params.top_level) ? mesh->curve_offset : 0;
          Mesh::Curve curve = mesh->get_curve(pidx - str_offset);
          int k = PRIMITIVE_UNPACK_SEGMENT(pack.prim_type[prim]);

          curve.bounds_grow(k, &mesh->curve_keys[0], &mesh->curve_radius[0], bbox);

          visibility |= PATH_RAY_CURVE;

          /* Motion curves. */
          if (mesh->use_motion_blur) {
            Attribute *attr = mesh->curve_attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

            if (attr) {
              size_t mesh_size = mesh->curve_keys.size();
              size_t steps = mesh->motion_steps - 1;
              float3 *key_steps = attr->data_float3();

              for (size_t i = 0; i < steps; i++) {
                curve.bounds_grow(k, key_steps + i * mesh_size, &mesh->curve_radius[0], bbox);
              }
            }
          }
        }
        else {
          /* Triangles. */
          int tri_offset = (params.top_level) ? mesh->tri_offset : 0;
          Mesh::Triangle triangle = mesh->get_triangle(pidx - tri_offset);
          const float3 *vpos = &mesh->verts[0];

          triangle.bounds_grow(vpos, bbox);

          /* Motion triangles. */
          if (mesh->use_motion_blur) {
            Attribute *attr = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

            if (attr) {
              size_t mesh_size = mesh->verts.size();
              size_t steps = mesh->motion_steps - 1;
              float3 *vert_steps = attr->data_float3();

              for (size_t i = 0; i < steps; i++) {
                triangle.bounds_grow(vert_steps + i * mesh_size, bbox);
              }
            }
          }
        }
      }

      visibility |= ob->visibility;
    }

    float4 leaf_data[BVH_ONODE_LEAF_SIZE];
    leaf_data[0].x = __int_as_float(c.x);
    leaf_data[0].y = __int_as_float(c.y);
    leaf_data[0].z = __uint_as_float(visibility);
    leaf_data[0].w = __uint_as_float(c.w);
    memcpy(&pack.leaf_nodes[idx], leaf_data, sizeof(float4) * BVH_ONODE_LEAF_SIZE);
  }
  else {
    float8 *data = (float8 *)&pack.nodes[idx];
    bool is_unaligned = (__float_as_uint(data[0].a) & PATH_RAY_NODE_UNALIGNED) != 0;
    /* Refit inner node, set bbox from children. */
    BoundBox child_bbox[8] = {BoundBox::empty,
                              BoundBox::empty,
                              BoundBox::empty,
                              BoundBox::empty,
                              BoundBox::empty,
                              BoundBox::empty,
                              BoundBox::empty,
                              BoundBox::empty};
    int child[8];
    uint child_visibility[8] = {0};
    int num_nodes = 0;

    for (int i = 0; i < 8; ++i) {
      child[i] = __float_as_int(data[(is_unaligned) ? 13 : 7][i]);

      if (child[i] != 0) {
        refit_node((child[i] < 0) ? -child[i] - 1 : child[i],
                   (child[i] < 0),
                   child_bbox[i],
                   child_visibility[i]);
        ++num_nodes;
        bbox.grow(child_bbox[i]);
        visibility |= child_visibility[i];
      }
    }

    if (is_unaligned) {
      Transform aligned_space[8] = {transform_identity(),
                                    transform_identity(),
                                    transform_identity(),
                                    transform_identity(),
                                    transform_identity(),
                                    transform_identity(),
                                    transform_identity(),
                                    transform_identity()};
      pack_unaligned_node(
          idx, aligned_space, child_bbox, child, visibility, 0.0f, 1.0f, num_nodes);
    }
    else {
      pack_aligned_node(idx, child_bbox, child, visibility, 0.0f, 1.0f, num_nodes);
    }
  }
}

CCL_NAMESPACE_END
