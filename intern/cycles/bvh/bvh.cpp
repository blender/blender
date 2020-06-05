/*
 * Adapted from code copyright 2009-2010 NVIDIA Corporation
 * Modifications Copyright 2011, Blender Foundation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "bvh/bvh.h"

#include "render/hair.h"
#include "render/mesh.h"
#include "render/object.h"

#include "bvh/bvh2.h"
#include "bvh/bvh4.h"
#include "bvh/bvh8.h"
#include "bvh/bvh_build.h"
#include "bvh/bvh_embree.h"
#include "bvh/bvh_node.h"
#include "bvh/bvh_optix.h"

#include "util/util_foreach.h"
#include "util/util_logging.h"
#include "util/util_progress.h"

CCL_NAMESPACE_BEGIN

/* BVH Parameters. */

const char *bvh_layout_name(BVHLayout layout)
{
  switch (layout) {
    case BVH_LAYOUT_BVH2:
      return "BVH2";
    case BVH_LAYOUT_BVH4:
      return "BVH4";
    case BVH_LAYOUT_BVH8:
      return "BVH8";
    case BVH_LAYOUT_NONE:
      return "NONE";
    case BVH_LAYOUT_EMBREE:
      return "EMBREE";
    case BVH_LAYOUT_OPTIX:
      return "OPTIX";
    case BVH_LAYOUT_ALL:
      return "ALL";
  }
  LOG(DFATAL) << "Unsupported BVH layout was passed.";
  return "";
}

BVHLayout BVHParams::best_bvh_layout(BVHLayout requested_layout, BVHLayoutMask supported_layouts)
{
  const BVHLayoutMask requested_layout_mask = (BVHLayoutMask)requested_layout;
  /* Check whether requested layout is supported, if so -- no need to do
   * any extra computation.
   */
  if (supported_layouts & requested_layout_mask) {
    return requested_layout;
  }
  /* Some bit magic to get widest supported BVH layout. */
  /* This is a mask of supported BVH layouts which are narrower than the
   * requested one.
   */
  BVHLayoutMask allowed_layouts_mask = (supported_layouts & (requested_layout_mask - 1));
  /* If the requested layout is not supported, choose from the supported layouts instead. */
  if (allowed_layouts_mask == 0) {
    allowed_layouts_mask = supported_layouts;
  }
  /* We get widest from allowed ones and convert mask to actual layout. */
  const BVHLayoutMask widest_allowed_layout_mask = __bsr(allowed_layouts_mask);
  return (BVHLayout)(1 << widest_allowed_layout_mask);
}

/* Pack Utility */

BVHStackEntry::BVHStackEntry(const BVHNode *n, int i) : node(n), idx(i)
{
}

int BVHStackEntry::encodeIdx() const
{
  return (node->is_leaf()) ? ~idx : idx;
}

/* BVH */

BVH::BVH(const BVHParams &params_,
         const vector<Geometry *> &geometry_,
         const vector<Object *> &objects_)
    : params(params_), geometry(geometry_), objects(objects_)
{
}

BVH *BVH::create(const BVHParams &params,
                 const vector<Geometry *> &geometry,
                 const vector<Object *> &objects)
{
  switch (params.bvh_layout) {
    case BVH_LAYOUT_BVH2:
      return new BVH2(params, geometry, objects);
    case BVH_LAYOUT_BVH4:
      return new BVH4(params, geometry, objects);
    case BVH_LAYOUT_BVH8:
      return new BVH8(params, geometry, objects);
    case BVH_LAYOUT_EMBREE:
#ifdef WITH_EMBREE
      return new BVHEmbree(params, geometry, objects);
#else
      break;
#endif
    case BVH_LAYOUT_OPTIX:
#ifdef WITH_OPTIX
      return new BVHOptiX(params, geometry, objects);
#else
      break;
#endif
    case BVH_LAYOUT_NONE:
    case BVH_LAYOUT_ALL:
      break;
  }
  LOG(DFATAL) << "Requested unsupported BVH layout.";
  return NULL;
}

/* Building */

void BVH::build(Progress &progress, Stats *)
{
  progress.set_substatus("Building BVH");

  /* build nodes */
  BVHBuild bvh_build(objects,
                     pack.prim_type,
                     pack.prim_index,
                     pack.prim_object,
                     pack.prim_time,
                     params,
                     progress);
  BVHNode *bvh2_root = bvh_build.run();

  if (progress.get_cancel()) {
    if (bvh2_root != NULL) {
      bvh2_root->deleteSubtree();
    }
    return;
  }

  /* BVH builder returns tree in a binary mode (with two children per inner
   * node. Need to adopt that for a wider BVH implementations. */
  BVHNode *root = widen_children_nodes(bvh2_root);
  if (root != bvh2_root) {
    bvh2_root->deleteSubtree();
  }

  if (progress.get_cancel()) {
    if (root != NULL) {
      root->deleteSubtree();
    }
    return;
  }

  /* pack triangles */
  progress.set_substatus("Packing BVH triangles and strands");
  pack_primitives();

  if (progress.get_cancel()) {
    root->deleteSubtree();
    return;
  }

  /* pack nodes */
  progress.set_substatus("Packing BVH nodes");
  pack_nodes(root);

  /* free build nodes */
  root->deleteSubtree();
}

/* Refitting */

void BVH::refit(Progress &progress)
{
  progress.set_substatus("Packing BVH primitives");
  pack_primitives();

  if (progress.get_cancel())
    return;

  progress.set_substatus("Refitting BVH nodes");
  refit_nodes();
}

void BVH::refit_primitives(int start, int end, BoundBox &bbox, uint &visibility)
{
  /* Refit range of primitives. */
  for (int prim = start; prim < end; prim++) {
    int pidx = pack.prim_index[prim];
    int tob = pack.prim_object[prim];
    Object *ob = objects[tob];

    if (pidx == -1) {
      /* Object instance. */
      bbox.grow(ob->bounds);
    }
    else {
      /* Primitives. */
      if (pack.prim_type[prim] & PRIMITIVE_ALL_CURVE) {
        /* Curves. */
        const Hair *hair = static_cast<const Hair *>(ob->geometry);
        int prim_offset = (params.top_level) ? hair->prim_offset : 0;
        Hair::Curve curve = hair->get_curve(pidx - prim_offset);
        int k = PRIMITIVE_UNPACK_SEGMENT(pack.prim_type[prim]);

        curve.bounds_grow(k, &hair->curve_keys[0], &hair->curve_radius[0], bbox);

        /* Motion curves. */
        if (hair->use_motion_blur) {
          Attribute *attr = hair->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

          if (attr) {
            size_t hair_size = hair->curve_keys.size();
            size_t steps = hair->motion_steps - 1;
            float3 *key_steps = attr->data_float3();

            for (size_t i = 0; i < steps; i++)
              curve.bounds_grow(k, key_steps + i * hair_size, &hair->curve_radius[0], bbox);
          }
        }
      }
      else {
        /* Triangles. */
        const Mesh *mesh = static_cast<const Mesh *>(ob->geometry);
        int prim_offset = (params.top_level) ? mesh->prim_offset : 0;
        Mesh::Triangle triangle = mesh->get_triangle(pidx - prim_offset);
        const float3 *vpos = &mesh->verts[0];

        triangle.bounds_grow(vpos, bbox);

        /* Motion triangles. */
        if (mesh->use_motion_blur) {
          Attribute *attr = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

          if (attr) {
            size_t mesh_size = mesh->verts.size();
            size_t steps = mesh->motion_steps - 1;
            float3 *vert_steps = attr->data_float3();

            for (size_t i = 0; i < steps; i++)
              triangle.bounds_grow(vert_steps + i * mesh_size, bbox);
          }
        }
      }
    }
    visibility |= ob->visibility_for_tracing();
  }
}

/* Triangles */

void BVH::pack_triangle(int idx, float4 tri_verts[3])
{
  int tob = pack.prim_object[idx];
  assert(tob >= 0 && tob < objects.size());
  const Mesh *mesh = static_cast<const Mesh *>(objects[tob]->geometry);

  int tidx = pack.prim_index[idx];
  Mesh::Triangle t = mesh->get_triangle(tidx);
  const float3 *vpos = &mesh->verts[0];
  float3 v0 = vpos[t.v[0]];
  float3 v1 = vpos[t.v[1]];
  float3 v2 = vpos[t.v[2]];

  tri_verts[0] = float3_to_float4(v0);
  tri_verts[1] = float3_to_float4(v1);
  tri_verts[2] = float3_to_float4(v2);
}

void BVH::pack_primitives()
{
  const size_t tidx_size = pack.prim_index.size();
  size_t num_prim_triangles = 0;
  /* Count number of triangles primitives in BVH. */
  for (unsigned int i = 0; i < tidx_size; i++) {
    if ((pack.prim_index[i] != -1)) {
      if ((pack.prim_type[i] & PRIMITIVE_ALL_TRIANGLE) != 0) {
        ++num_prim_triangles;
      }
    }
  }
  /* Reserve size for arrays. */
  pack.prim_tri_index.clear();
  pack.prim_tri_index.resize(tidx_size);
  pack.prim_tri_verts.clear();
  pack.prim_tri_verts.resize(num_prim_triangles * 3);
  pack.prim_visibility.clear();
  pack.prim_visibility.resize(tidx_size);
  /* Fill in all the arrays. */
  size_t prim_triangle_index = 0;
  for (unsigned int i = 0; i < tidx_size; i++) {
    if (pack.prim_index[i] != -1) {
      int tob = pack.prim_object[i];
      Object *ob = objects[tob];
      if ((pack.prim_type[i] & PRIMITIVE_ALL_TRIANGLE) != 0) {
        pack_triangle(i, (float4 *)&pack.prim_tri_verts[3 * prim_triangle_index]);
        pack.prim_tri_index[i] = 3 * prim_triangle_index;
        ++prim_triangle_index;
      }
      else {
        pack.prim_tri_index[i] = -1;
      }
      pack.prim_visibility[i] = ob->visibility_for_tracing();
    }
    else {
      pack.prim_tri_index[i] = -1;
      pack.prim_visibility[i] = 0;
    }
  }
}

/* Pack Instances */

void BVH::pack_instances(size_t nodes_size, size_t leaf_nodes_size)
{
  /* The BVH's for instances are built separately, but for traversal all
   * BVH's are stored in global arrays. This function merges them into the
   * top level BVH, adjusting indexes and offsets where appropriate.
   */
  const bool use_qbvh = (params.bvh_layout == BVH_LAYOUT_BVH4);
  const bool use_obvh = (params.bvh_layout == BVH_LAYOUT_BVH8);

  /* Adjust primitive index to point to the triangle in the global array, for
   * geometry with transform applied and already in the top level BVH.
   */
  for (size_t i = 0; i < pack.prim_index.size(); i++) {
    if (pack.prim_index[i] != -1) {
      pack.prim_index[i] += objects[pack.prim_object[i]]->geometry->prim_offset;
    }
  }

  /* track offsets of instanced BVH data in global array */
  size_t prim_offset = pack.prim_index.size();
  size_t nodes_offset = nodes_size;
  size_t nodes_leaf_offset = leaf_nodes_size;

  /* clear array that gives the node indexes for instanced objects */
  pack.object_node.clear();

  /* reserve */
  size_t prim_index_size = pack.prim_index.size();
  size_t prim_tri_verts_size = pack.prim_tri_verts.size();

  size_t pack_prim_index_offset = prim_index_size;
  size_t pack_prim_tri_verts_offset = prim_tri_verts_size;
  size_t pack_nodes_offset = nodes_size;
  size_t pack_leaf_nodes_offset = leaf_nodes_size;
  size_t object_offset = 0;

  foreach (Geometry *geom, geometry) {
    BVH *bvh = geom->bvh;

    if (geom->need_build_bvh(params.bvh_layout)) {
      prim_index_size += bvh->pack.prim_index.size();
      prim_tri_verts_size += bvh->pack.prim_tri_verts.size();
      nodes_size += bvh->pack.nodes.size();
      leaf_nodes_size += bvh->pack.leaf_nodes.size();
    }
  }

  pack.prim_index.resize(prim_index_size);
  pack.prim_type.resize(prim_index_size);
  pack.prim_object.resize(prim_index_size);
  pack.prim_visibility.resize(prim_index_size);
  pack.prim_tri_verts.resize(prim_tri_verts_size);
  pack.prim_tri_index.resize(prim_index_size);
  pack.nodes.resize(nodes_size);
  pack.leaf_nodes.resize(leaf_nodes_size);
  pack.object_node.resize(objects.size());

  if (params.num_motion_curve_steps > 0 || params.num_motion_triangle_steps > 0) {
    pack.prim_time.resize(prim_index_size);
  }

  int *pack_prim_index = (pack.prim_index.size()) ? &pack.prim_index[0] : NULL;
  int *pack_prim_type = (pack.prim_type.size()) ? &pack.prim_type[0] : NULL;
  int *pack_prim_object = (pack.prim_object.size()) ? &pack.prim_object[0] : NULL;
  uint *pack_prim_visibility = (pack.prim_visibility.size()) ? &pack.prim_visibility[0] : NULL;
  float4 *pack_prim_tri_verts = (pack.prim_tri_verts.size()) ? &pack.prim_tri_verts[0] : NULL;
  uint *pack_prim_tri_index = (pack.prim_tri_index.size()) ? &pack.prim_tri_index[0] : NULL;
  int4 *pack_nodes = (pack.nodes.size()) ? &pack.nodes[0] : NULL;
  int4 *pack_leaf_nodes = (pack.leaf_nodes.size()) ? &pack.leaf_nodes[0] : NULL;
  float2 *pack_prim_time = (pack.prim_time.size()) ? &pack.prim_time[0] : NULL;

  map<Geometry *, int> geometry_map;

  /* merge */
  foreach (Object *ob, objects) {
    Geometry *geom = ob->geometry;

    /* We assume that if mesh doesn't need own BVH it was already included
     * into a top-level BVH and no packing here is needed.
     */
    if (!geom->need_build_bvh(params.bvh_layout)) {
      pack.object_node[object_offset++] = 0;
      continue;
    }

    /* if mesh already added once, don't add it again, but used set
     * node offset for this object */
    map<Geometry *, int>::iterator it = geometry_map.find(geom);

    if (geometry_map.find(geom) != geometry_map.end()) {
      int noffset = it->second;
      pack.object_node[object_offset++] = noffset;
      continue;
    }

    BVH *bvh = geom->bvh;

    int noffset = nodes_offset;
    int noffset_leaf = nodes_leaf_offset;
    int geom_prim_offset = geom->prim_offset;

    /* fill in node indexes for instances */
    if (bvh->pack.root_index == -1)
      pack.object_node[object_offset++] = -noffset_leaf - 1;
    else
      pack.object_node[object_offset++] = noffset;

    geometry_map[geom] = pack.object_node[object_offset - 1];

    /* merge primitive, object and triangle indexes */
    if (bvh->pack.prim_index.size()) {
      size_t bvh_prim_index_size = bvh->pack.prim_index.size();
      int *bvh_prim_index = &bvh->pack.prim_index[0];
      int *bvh_prim_type = &bvh->pack.prim_type[0];
      uint *bvh_prim_visibility = &bvh->pack.prim_visibility[0];
      uint *bvh_prim_tri_index = &bvh->pack.prim_tri_index[0];
      float2 *bvh_prim_time = bvh->pack.prim_time.size() ? &bvh->pack.prim_time[0] : NULL;

      for (size_t i = 0; i < bvh_prim_index_size; i++) {
        if (bvh->pack.prim_type[i] & PRIMITIVE_ALL_CURVE) {
          pack_prim_index[pack_prim_index_offset] = bvh_prim_index[i] + geom_prim_offset;
          pack_prim_tri_index[pack_prim_index_offset] = -1;
        }
        else {
          pack_prim_index[pack_prim_index_offset] = bvh_prim_index[i] + geom_prim_offset;
          pack_prim_tri_index[pack_prim_index_offset] = bvh_prim_tri_index[i] +
                                                        pack_prim_tri_verts_offset;
        }

        pack_prim_type[pack_prim_index_offset] = bvh_prim_type[i];
        pack_prim_visibility[pack_prim_index_offset] = bvh_prim_visibility[i];
        pack_prim_object[pack_prim_index_offset] = 0;  // unused for instances
        if (bvh_prim_time != NULL) {
          pack_prim_time[pack_prim_index_offset] = bvh_prim_time[i];
        }
        pack_prim_index_offset++;
      }
    }

    /* Merge triangle vertices data. */
    if (bvh->pack.prim_tri_verts.size()) {
      const size_t prim_tri_size = bvh->pack.prim_tri_verts.size();
      memcpy(pack_prim_tri_verts + pack_prim_tri_verts_offset,
             &bvh->pack.prim_tri_verts[0],
             prim_tri_size * sizeof(float4));
      pack_prim_tri_verts_offset += prim_tri_size;
    }

    /* merge nodes */
    if (bvh->pack.leaf_nodes.size()) {
      int4 *leaf_nodes_offset = &bvh->pack.leaf_nodes[0];
      size_t leaf_nodes_offset_size = bvh->pack.leaf_nodes.size();
      for (size_t i = 0, j = 0; i < leaf_nodes_offset_size; i += BVH_NODE_LEAF_SIZE, j++) {
        int4 data = leaf_nodes_offset[i];
        data.x += prim_offset;
        data.y += prim_offset;
        pack_leaf_nodes[pack_leaf_nodes_offset] = data;
        for (int j = 1; j < BVH_NODE_LEAF_SIZE; ++j) {
          pack_leaf_nodes[pack_leaf_nodes_offset + j] = leaf_nodes_offset[i + j];
        }
        pack_leaf_nodes_offset += BVH_NODE_LEAF_SIZE;
      }
    }

    if (bvh->pack.nodes.size()) {
      int4 *bvh_nodes = &bvh->pack.nodes[0];
      size_t bvh_nodes_size = bvh->pack.nodes.size();

      for (size_t i = 0, j = 0; i < bvh_nodes_size; j++) {
        size_t nsize, nsize_bbox;
        if (bvh_nodes[i].x & PATH_RAY_NODE_UNALIGNED) {
          if (use_obvh) {
            nsize = BVH_UNALIGNED_ONODE_SIZE;
            nsize_bbox = BVH_UNALIGNED_ONODE_SIZE - 1;
          }
          else {
            nsize = use_qbvh ? BVH_UNALIGNED_QNODE_SIZE : BVH_UNALIGNED_NODE_SIZE;
            nsize_bbox = (use_qbvh) ? BVH_UNALIGNED_QNODE_SIZE - 1 : 0;
          }
        }
        else {
          if (use_obvh) {
            nsize = BVH_ONODE_SIZE;
            nsize_bbox = BVH_ONODE_SIZE - 1;
          }
          else {
            nsize = (use_qbvh) ? BVH_QNODE_SIZE : BVH_NODE_SIZE;
            nsize_bbox = (use_qbvh) ? BVH_QNODE_SIZE - 1 : 0;
          }
        }

        memcpy(pack_nodes + pack_nodes_offset, bvh_nodes + i, nsize_bbox * sizeof(int4));

        /* Modify offsets into arrays */
        int4 data = bvh_nodes[i + nsize_bbox];

        if (use_obvh) {
          int4 data1 = bvh_nodes[i + nsize_bbox - 1];
          data.z += (data.z < 0) ? -noffset_leaf : noffset;
          data.w += (data.w < 0) ? -noffset_leaf : noffset;
          data.x += (data.x < 0) ? -noffset_leaf : noffset;
          data.y += (data.y < 0) ? -noffset_leaf : noffset;
          data1.z += (data1.z < 0) ? -noffset_leaf : noffset;
          data1.w += (data1.w < 0) ? -noffset_leaf : noffset;
          data1.x += (data1.x < 0) ? -noffset_leaf : noffset;
          data1.y += (data1.y < 0) ? -noffset_leaf : noffset;
          pack_nodes[pack_nodes_offset + nsize_bbox] = data;
          pack_nodes[pack_nodes_offset + nsize_bbox - 1] = data1;
        }
        else {
          data.z += (data.z < 0) ? -noffset_leaf : noffset;
          data.w += (data.w < 0) ? -noffset_leaf : noffset;
          if (use_qbvh) {
            data.x += (data.x < 0) ? -noffset_leaf : noffset;
            data.y += (data.y < 0) ? -noffset_leaf : noffset;
          }
          pack_nodes[pack_nodes_offset + nsize_bbox] = data;
        }

        /* Usually this copies nothing, but we better
         * be prepared for possible node size extension.
         */
        memcpy(&pack_nodes[pack_nodes_offset + nsize_bbox + 1],
               &bvh_nodes[i + nsize_bbox + 1],
               sizeof(int4) * (nsize - (nsize_bbox + 1)));

        pack_nodes_offset += nsize;
        i += nsize;
      }
    }

    nodes_offset += bvh->pack.nodes.size();
    nodes_leaf_offset += bvh->pack.leaf_nodes.size();
    prim_offset += bvh->pack.prim_index.size();
  }
}

CCL_NAMESPACE_END
