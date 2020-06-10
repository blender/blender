/*
 * Copyright 2019, NVIDIA Corporation.
 * Copyright 2019, Blender Foundation.
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

#ifdef WITH_OPTIX

#  include "bvh/bvh_optix.h"

#  include "device/device.h"

#  include "render/geometry.h"
#  include "render/hair.h"
#  include "render/mesh.h"
#  include "render/object.h"

#  include "util/util_foreach.h"
#  include "util/util_logging.h"
#  include "util/util_progress.h"

CCL_NAMESPACE_BEGIN

BVHOptiX::BVHOptiX(const BVHParams &params_,
                   const vector<Geometry *> &geometry_,
                   const vector<Object *> &objects_)
    : BVH(params_, geometry_, objects_)
{
}

BVHOptiX::~BVHOptiX()
{
}

void BVHOptiX::build(Progress &, Stats *)
{
  if (params.top_level)
    pack_tlas();
  else
    pack_blas();
}

void BVHOptiX::copy_to_device(Progress &progress, DeviceScene *dscene)
{
  progress.set_status("Updating Scene BVH", "Building OptiX acceleration structure");

  Device *const device = dscene->bvh_nodes.device;
  if (!device->build_optix_bvh(this))
    progress.set_error("Failed to build OptiX acceleration structure");
}

void BVHOptiX::pack_blas()
{
  // Bottom-level BVH can contain multiple primitive types, so merge them:
  assert(geometry.size() == 1 && objects.size() == 1);  // These are built per-mesh
  Geometry *const geom = geometry[0];

  if (geom->type == Geometry::HAIR) {
    Hair *const hair = static_cast<Hair *const>(geom);
    if (hair->num_curves() > 0) {
      const size_t num_curves = hair->num_curves();
      const size_t num_segments = hair->num_segments();
      pack.prim_type.reserve(pack.prim_type.size() + num_segments);
      pack.prim_index.reserve(pack.prim_index.size() + num_segments);
      pack.prim_object.reserve(pack.prim_object.size() + num_segments);
      // 'pack.prim_time' is only used in geom_curve_intersect.h
      // It is not needed because of OPTIX_MOTION_FLAG_[START|END]_VANISH

      uint type = (hair->use_motion_blur &&
                   hair->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION)) ?
                      ((hair->curve_shape == CURVE_RIBBON) ? PRIMITIVE_MOTION_CURVE_RIBBON :
                                                             PRIMITIVE_MOTION_CURVE_THICK) :
                      ((hair->curve_shape == CURVE_RIBBON) ? PRIMITIVE_CURVE_RIBBON :
                                                             PRIMITIVE_CURVE_THICK);

      for (size_t j = 0; j < num_curves; ++j) {
        const Hair::Curve curve = hair->get_curve(j);
        for (size_t k = 0; k < curve.num_segments(); ++k) {
          pack.prim_type.push_back_reserved(PRIMITIVE_PACK_SEGMENT(type, k));
          // Each curve segment points back to its curve index
          pack.prim_index.push_back_reserved(j);
          pack.prim_object.push_back_reserved(0);
        }
      }
    }
  }
  else if (geom->type == Geometry::MESH) {
    Mesh *const mesh = static_cast<Mesh *const>(geom);
    if (mesh->num_triangles() > 0) {
      const size_t num_triangles = mesh->num_triangles();
      pack.prim_type.reserve(pack.prim_type.size() + num_triangles);
      pack.prim_index.reserve(pack.prim_index.size() + num_triangles);
      pack.prim_object.reserve(pack.prim_object.size() + num_triangles);

      uint type = PRIMITIVE_TRIANGLE;
      if (mesh->use_motion_blur && mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION))
        type = PRIMITIVE_MOTION_TRIANGLE;

      for (size_t k = 0; k < num_triangles; ++k) {
        pack.prim_type.push_back_reserved(type);
        pack.prim_index.push_back_reserved(k);
        pack.prim_object.push_back_reserved(0);
      }
    }
  }

  // Initialize visibility to zero and later update it during top-level build
  uint prev_visibility = objects[0]->visibility;
  objects[0]->visibility = 0;

  // Update 'pack.prim_tri_index', 'pack.prim_tri_verts' and 'pack.prim_visibility'
  pack_primitives();

  // Reset visibility after packing
  objects[0]->visibility = prev_visibility;
}

void BVHOptiX::pack_tlas()
{
  // Calculate total packed size
  size_t prim_index_size = 0;
  size_t prim_tri_verts_size = 0;
  foreach (Geometry *geom, geometry) {
    BVH *const bvh = geom->bvh;
    prim_index_size += bvh->pack.prim_index.size();
    prim_tri_verts_size += bvh->pack.prim_tri_verts.size();
  }

  if (prim_index_size == 0)
    return;  // Abort right away if this is an empty BVH

  size_t pack_offset = 0;
  size_t pack_verts_offset = 0;

  pack.prim_type.resize(prim_index_size);
  int *pack_prim_type = pack.prim_type.data();
  pack.prim_index.resize(prim_index_size);
  int *pack_prim_index = pack.prim_index.data();
  pack.prim_object.resize(prim_index_size);
  int *pack_prim_object = pack.prim_object.data();
  pack.prim_visibility.resize(prim_index_size);
  uint *pack_prim_visibility = pack.prim_visibility.data();
  pack.prim_tri_index.resize(prim_index_size);
  uint *pack_prim_tri_index = pack.prim_tri_index.data();
  pack.prim_tri_verts.resize(prim_tri_verts_size);
  float4 *pack_prim_tri_verts = pack.prim_tri_verts.data();

  // Top-level BVH should only contain instances, see 'Geometry::need_build_bvh'
  // Iterate over scene mesh list instead of objects, since the 'prim_offset' is calculated based
  // on that list, which may be ordered differently from the object list.
  foreach (Geometry *geom, geometry) {
    PackedBVH &bvh_pack = geom->bvh->pack;
    int geom_prim_offset = geom->prim_offset;

    // Merge visibility flags of all objects and fix object indices for non-instanced geometry
    int object_index = 0;  // Unused for instanced geometry
    int object_visibility = 0;
    foreach (Object *ob, objects) {
      if (ob->geometry == geom) {
        object_visibility |= ob->visibility_for_tracing();
        if (!geom->is_instanced()) {
          object_index = ob->get_device_index();
          break;
        }
      }
    }

    // Merge primitive, object and triangle indexes
    if (!bvh_pack.prim_index.empty()) {
      int *bvh_prim_type = &bvh_pack.prim_type[0];
      int *bvh_prim_index = &bvh_pack.prim_index[0];
      uint *bvh_prim_tri_index = &bvh_pack.prim_tri_index[0];
      uint *bvh_prim_visibility = &bvh_pack.prim_visibility[0];

      for (size_t i = 0; i < bvh_pack.prim_index.size(); i++, pack_offset++) {
        if (bvh_pack.prim_type[i] & PRIMITIVE_ALL_CURVE) {
          pack_prim_index[pack_offset] = bvh_prim_index[i] + geom_prim_offset;
          pack_prim_tri_index[pack_offset] = -1;
        }
        else {
          pack_prim_index[pack_offset] = bvh_prim_index[i] + geom_prim_offset;
          pack_prim_tri_index[pack_offset] = bvh_prim_tri_index[i] + pack_verts_offset;
        }

        pack_prim_type[pack_offset] = bvh_prim_type[i];
        pack_prim_object[pack_offset] = object_index;
        pack_prim_visibility[pack_offset] = bvh_prim_visibility[i] | object_visibility;
      }
    }

    // Merge triangle vertex data
    if (!bvh_pack.prim_tri_verts.empty()) {
      const size_t prim_tri_size = bvh_pack.prim_tri_verts.size();
      memcpy(pack_prim_tri_verts + pack_verts_offset,
             bvh_pack.prim_tri_verts.data(),
             prim_tri_size * sizeof(float4));
      pack_verts_offset += prim_tri_size;
    }
  }
}

void BVHOptiX::pack_nodes(const BVHNode *)
{
}

void BVHOptiX::refit_nodes()
{
  // TODO(pmours): Implement?
  VLOG(1) << "Refit is not yet implemented for OptiX BVH.";
}

BVHNode *BVHOptiX::widen_children_nodes(const BVHNode *)
{
  return NULL;
}

CCL_NAMESPACE_END

#endif /* WITH_OPTIX */
