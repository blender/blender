/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation */

/** \file
 * \ingroup bke
 */

#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_index_range.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_bvhutils.h"
#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_remesh_voxel.h" /* own include */
#include "BKE_mesh_runtime.h"

#include "bmesh_tools.h"

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/MeshToVolume.h>
#  include <openvdb/tools/VolumeToMesh.h>
#endif

#ifdef WITH_QUADRIFLOW
#  include "quadriflow_capi.hpp"
#endif

using blender::Array;
using blender::float3;
using blender::IndexRange;
using blender::MutableSpan;
using blender::Span;

#ifdef WITH_QUADRIFLOW
static Mesh *remesh_quadriflow(const Mesh *input_mesh,
                               int target_faces,
                               int seed,
                               bool preserve_sharp,
                               bool preserve_boundary,
                               bool adaptive_scale,
                               void (*update_cb)(void *, float progress, int *cancel),
                               void *update_cb_data)
{
  const Span<float3> input_positions = input_mesh->vert_positions();
  const Span<int> input_corner_verts = input_mesh->corner_verts();
  const Span<MLoopTri> looptris = input_mesh->looptris();

  /* Gather the required data for export to the internal quadriflow mesh format. */
  Array<MVertTri> verttri(looptris.size());
  BKE_mesh_runtime_verttri_from_looptri(
      verttri.data(), input_corner_verts.data(), looptris.data(), looptris.size());

  const int totfaces = looptris.size();
  const int totverts = input_mesh->totvert;
  Array<int> faces(totfaces * 3);

  for (const int i : IndexRange(totfaces)) {
    MVertTri &vt = verttri[i];
    faces[i * 3] = vt.tri[0];
    faces[i * 3 + 1] = vt.tri[1];
    faces[i * 3 + 2] = vt.tri[2];
  }

  /* Fill out the required input data */
  QuadriflowRemeshData qrd;

  qrd.totfaces = totfaces;
  qrd.totverts = totverts;
  qrd.verts = (float *)input_positions.data();
  qrd.faces = faces.data();
  qrd.target_faces = target_faces;

  qrd.preserve_sharp = preserve_sharp;
  qrd.preserve_boundary = preserve_boundary;
  qrd.adaptive_scale = adaptive_scale;
  qrd.minimum_cost_flow = false;
  qrd.aggresive_sat = false;
  qrd.rng_seed = seed;

  qrd.out_faces = nullptr;

  /* Run the remesher */
  QFLOW_quadriflow_remesh(&qrd, update_cb, update_cb_data);

  if (qrd.out_faces == nullptr) {
    /* The remeshing was canceled */
    return nullptr;
  }

  if (qrd.out_totfaces == 0) {
    /* Meshing failed */
    MEM_freeN(qrd.out_faces);
    MEM_freeN(qrd.out_verts);
    return nullptr;
  }

  /* Construct the new output mesh */
  Mesh *mesh = BKE_mesh_new_nomain(qrd.out_totverts, 0, qrd.out_totfaces, qrd.out_totfaces * 4);
  BKE_mesh_copy_parameters(mesh, input_mesh);
  MutableSpan<int> poly_offsets = mesh->poly_offsets_for_write();
  MutableSpan<int> corner_verts = mesh->corner_verts_for_write();

  poly_offsets.fill(4);
  blender::offset_indices::accumulate_counts_to_offsets(poly_offsets);

  mesh->vert_positions_for_write().copy_from(
      Span(reinterpret_cast<float3 *>(qrd.out_verts), qrd.out_totverts));

  for (const int i : IndexRange(qrd.out_totfaces)) {
    const int loopstart = i * 4;
    corner_verts[loopstart] = qrd.out_faces[loopstart];
    corner_verts[loopstart + 1] = qrd.out_faces[loopstart + 1];
    corner_verts[loopstart + 2] = qrd.out_faces[loopstart + 2];
    corner_verts[loopstart + 3] = qrd.out_faces[loopstart + 3];
  }

  BKE_mesh_calc_edges(mesh, false, false);

  MEM_freeN(qrd.out_faces);
  MEM_freeN(qrd.out_verts);

  return mesh;
}
#endif

Mesh *BKE_mesh_remesh_quadriflow(const Mesh *mesh,
                                 int target_faces,
                                 int seed,
                                 bool preserve_sharp,
                                 bool preserve_boundary,
                                 bool adaptive_scale,
                                 void (*update_cb)(void *, float progress, int *cancel),
                                 void *update_cb_data)
{
#ifdef WITH_QUADRIFLOW
  if (target_faces <= 0) {
    target_faces = -1;
  }
  return remesh_quadriflow(mesh,
                           target_faces,
                           seed,
                           preserve_sharp,
                           preserve_boundary,
                           adaptive_scale,
                           update_cb,
                           update_cb_data);
#else
  UNUSED_VARS(mesh,
              target_faces,
              seed,
              preserve_sharp,
              preserve_boundary,
              adaptive_scale,
              update_cb,
              update_cb_data);
  return nullptr;
#endif
}

#ifdef WITH_OPENVDB
static openvdb::FloatGrid::Ptr remesh_voxel_level_set_create(const Mesh *mesh,
                                                             const float voxel_size)
{
  const Span<float3> positions = mesh->vert_positions();
  const Span<int> corner_verts = mesh->corner_verts();
  const Span<MLoopTri> looptris = mesh->looptris();

  std::vector<openvdb::Vec3s> points(mesh->totvert);
  std::vector<openvdb::Vec3I> triangles(looptris.size());

  for (const int i : IndexRange(mesh->totvert)) {
    const float3 &co = positions[i];
    points[i] = openvdb::Vec3s(co.x, co.y, co.z);
  }

  for (const int i : IndexRange(looptris.size())) {
    const MLoopTri &loop_tri = looptris[i];
    triangles[i] = openvdb::Vec3I(corner_verts[loop_tri.tri[0]],
                                  corner_verts[loop_tri.tri[1]],
                                  corner_verts[loop_tri.tri[2]]);
  }

  openvdb::math::Transform::Ptr transform = openvdb::math::Transform::createLinearTransform(
      voxel_size);
  openvdb::FloatGrid::Ptr grid = openvdb::tools::meshToLevelSet<openvdb::FloatGrid>(
      *transform, points, triangles, 1.0f);

  return grid;
}

static Mesh *remesh_voxel_volume_to_mesh(const openvdb::FloatGrid::Ptr level_set_grid,
                                         const float isovalue,
                                         const float adaptivity,
                                         const bool relax_disoriented_triangles)
{
  std::vector<openvdb::Vec3s> vertices;
  std::vector<openvdb::Vec4I> quads;
  std::vector<openvdb::Vec3I> tris;
  openvdb::tools::volumeToMesh<openvdb::FloatGrid>(
      *level_set_grid, vertices, tris, quads, isovalue, adaptivity, relax_disoriented_triangles);

  Mesh *mesh = BKE_mesh_new_nomain(
      vertices.size(), 0, quads.size() + tris.size(), quads.size() * 4 + tris.size() * 3);
  MutableSpan<float3> vert_positions = mesh->vert_positions_for_write();
  MutableSpan<int> poly_offsets = mesh->poly_offsets_for_write();
  MutableSpan<int> mesh_corner_verts = mesh->corner_verts_for_write();

  if (!poly_offsets.is_empty()) {
    poly_offsets.take_front(quads.size()).fill(4);
    poly_offsets.drop_front(quads.size()).fill(3);
    blender::offset_indices::accumulate_counts_to_offsets(poly_offsets);
  }

  for (const int i : vert_positions.index_range()) {
    vert_positions[i] = float3(vertices[i].x(), vertices[i].y(), vertices[i].z());
  }

  for (const int i : IndexRange(quads.size())) {
    const int loopstart = i * 4;
    mesh_corner_verts[loopstart] = quads[i][0];
    mesh_corner_verts[loopstart + 1] = quads[i][3];
    mesh_corner_verts[loopstart + 2] = quads[i][2];
    mesh_corner_verts[loopstart + 3] = quads[i][1];
  }

  const int triangle_loop_start = quads.size() * 4;
  for (const int i : IndexRange(tris.size())) {
    const int loopstart = triangle_loop_start + i * 3;
    mesh_corner_verts[loopstart] = tris[i][2];
    mesh_corner_verts[loopstart + 1] = tris[i][1];
    mesh_corner_verts[loopstart + 2] = tris[i][0];
  }

  BKE_mesh_calc_edges(mesh, false, false);

  return mesh;
}
#endif

Mesh *BKE_mesh_remesh_voxel(const Mesh *mesh,
                            const float voxel_size,
                            const float adaptivity,
                            const float isovalue)
{
#ifdef WITH_OPENVDB
  openvdb::FloatGrid::Ptr level_set = remesh_voxel_level_set_create(mesh, voxel_size);
  Mesh *result = remesh_voxel_volume_to_mesh(level_set, isovalue, adaptivity, false);
  BKE_mesh_copy_parameters(result, mesh);
  return result;
#else
  UNUSED_VARS(mesh, voxel_size, adaptivity, isovalue);
  return nullptr;
#endif
}

void BKE_mesh_remesh_reproject_paint_mask(Mesh *target, const Mesh *source)
{
  BVHTreeFromMesh bvhtree = {nullptr};
  BKE_bvhtree_from_mesh_get(&bvhtree, source, BVHTREE_FROM_VERTS, 2);
  const Span<float3> target_positions = target->vert_positions();
  const float *source_mask = (const float *)CustomData_get_layer(&source->vdata, CD_PAINT_MASK);
  if (source_mask == nullptr) {
    return;
  }

  float *target_mask;
  if (CustomData_has_layer(&target->vdata, CD_PAINT_MASK)) {
    target_mask = (float *)CustomData_get_layer(&target->vdata, CD_PAINT_MASK);
  }
  else {
    target_mask = (float *)CustomData_add_layer(
        &target->vdata, CD_PAINT_MASK, CD_CONSTRUCT, target->totvert);
  }

  blender::threading::parallel_for(IndexRange(target->totvert), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      BVHTreeNearest nearest;
      nearest.index = -1;
      nearest.dist_sq = FLT_MAX;
      BLI_bvhtree_find_nearest(
          bvhtree.tree, target_positions[i], &nearest, bvhtree.nearest_callback, &bvhtree);
      if (nearest.index != -1) {
        target_mask[i] = source_mask[nearest.index];
      }
    }
  });
  free_bvhtree_from_mesh(&bvhtree);
}

void BKE_remesh_reproject_sculpt_face_sets(Mesh *target, const Mesh *source)
{
  using namespace blender;
  using namespace blender::bke;
  const AttributeAccessor src_attributes = source->attributes();
  MutableAttributeAccessor dst_attributes = target->attributes_for_write();
  const Span<float3> target_positions = target->vert_positions();
  const OffsetIndices target_polys = target->polys();
  const Span<int> target_corner_verts = target->corner_verts();

  const VArray src_face_sets = *src_attributes.lookup<int>(".sculpt_face_set", ATTR_DOMAIN_FACE);
  if (!src_face_sets) {
    return;
  }
  SpanAttributeWriter<int> dst_face_sets = dst_attributes.lookup_or_add_for_write_only_span<int>(
      ".sculpt_face_set", ATTR_DOMAIN_FACE);
  if (!dst_face_sets) {
    return;
  }

  const VArraySpan<int> src(src_face_sets);
  MutableSpan<int> dst = dst_face_sets.span;

  const blender::Span<int> looptri_polys = source->looptri_polys();
  BVHTreeFromMesh bvhtree = {nullptr};
  BKE_bvhtree_from_mesh_get(&bvhtree, source, BVHTREE_FROM_LOOPTRI, 2);

  blender::threading::parallel_for(IndexRange(target->totpoly), 2048, [&](const IndexRange range) {
    for (const int i : range) {
      BVHTreeNearest nearest;
      nearest.index = -1;
      nearest.dist_sq = FLT_MAX;
      const float3 from_co = mesh::poly_center_calc(target_positions,
                                                    target_corner_verts.slice(target_polys[i]));
      BLI_bvhtree_find_nearest(
          bvhtree.tree, from_co, &nearest, bvhtree.nearest_callback, &bvhtree);
      if (nearest.index != -1) {
        dst[i] = src[looptri_polys[nearest.index]];
      }
      else {
        dst[i] = 1;
      }
    }
  });
  free_bvhtree_from_mesh(&bvhtree);
  dst_face_sets.finish();
}

void BKE_remesh_reproject_vertex_paint(Mesh *target, const Mesh *source)
{
  BVHTreeFromMesh bvhtree = {nullptr};
  BKE_bvhtree_from_mesh_get(&bvhtree, source, BVHTREE_FROM_VERTS, 2);

  int i = 0;
  const CustomDataLayer *layer;

  MeshElemMap *source_lmap = nullptr;
  int *source_lmap_mem = nullptr;
  MeshElemMap *target_lmap = nullptr;
  int *target_lmap_mem = nullptr;

  while ((layer = BKE_id_attribute_from_index(
              const_cast<ID *>(&source->id), i++, ATTR_DOMAIN_MASK_COLOR, CD_MASK_COLOR_ALL)))
  {
    eAttrDomain domain = BKE_id_attribute_domain(&source->id, layer);
    const eCustomDataType type = eCustomDataType(layer->type);

    CustomData *target_cdata = domain == ATTR_DOMAIN_POINT ? &target->vdata : &target->ldata;
    const CustomData *source_cdata = domain == ATTR_DOMAIN_POINT ? &source->vdata : &source->ldata;

    /* Check attribute exists in target. */
    int layer_i = CustomData_get_named_layer_index(target_cdata, type, layer->name);
    if (layer_i == -1) {
      int elem_num = domain == ATTR_DOMAIN_POINT ? target->totvert : target->totloop;

      CustomData_add_layer_named(target_cdata, type, CD_SET_DEFAULT, elem_num, layer->name);
      layer_i = CustomData_get_named_layer_index(target_cdata, type, layer->name);
    }

    size_t data_size = CustomData_sizeof(type);
    void *target_data = target_cdata->layers[layer_i].data;
    void *source_data = layer->data;
    const Span<float3> target_positions = target->vert_positions();

    if (domain == ATTR_DOMAIN_POINT) {
      blender::threading::parallel_for(
          IndexRange(target->totvert), 4096, [&](const IndexRange range) {
            for (const int i : range) {
              BVHTreeNearest nearest;
              nearest.index = -1;
              nearest.dist_sq = FLT_MAX;
              BLI_bvhtree_find_nearest(
                  bvhtree.tree, target_positions[i], &nearest, bvhtree.nearest_callback, &bvhtree);
              if (nearest.index != -1) {
                memcpy(POINTER_OFFSET(target_data, size_t(i) * data_size),
                       POINTER_OFFSET(source_data, size_t(nearest.index) * data_size),
                       data_size);
              }
            }
          });
    }
    else {
      /* Lazily init vertex -> loop maps. */
      if (!source_lmap) {
        BKE_mesh_vert_loop_map_create(&source_lmap,
                                      &source_lmap_mem,
                                      source->polys(),
                                      source->corner_verts().data(),
                                      source->totvert);

        BKE_mesh_vert_loop_map_create(&target_lmap,
                                      &target_lmap_mem,
                                      target->polys(),
                                      target->corner_verts().data(),
                                      target->totvert);
      }

      blender::threading::parallel_for(
          IndexRange(target->totvert), 2048, [&](const IndexRange range) {
            for (const int i : range) {
              BVHTreeNearest nearest;
              nearest.index = -1;
              nearest.dist_sq = FLT_MAX;
              BLI_bvhtree_find_nearest(
                  bvhtree.tree, target_positions[i], &nearest, bvhtree.nearest_callback, &bvhtree);

              if (nearest.index == -1) {
                continue;
              }

              MeshElemMap *source_loops = source_lmap + nearest.index;
              MeshElemMap *target_loops = target_lmap + i;

              if (target_loops->count == 0 || source_loops->count == 0) {
                continue;
              }

              /*
               * Average color data for loops around the source vertex into
               * the first target loop around the target vertex
               */

              CustomData_interp(source_cdata,
                                target_cdata,
                                source_loops->indices,
                                nullptr,
                                nullptr,
                                source_loops->count,
                                target_loops->indices[0]);

              void *elem = POINTER_OFFSET(target_data,
                                          size_t(target_loops->indices[0]) * data_size);

              /* Copy to rest of target loops. */
              for (int j = 1; j < target_loops->count; j++) {
                memcpy(POINTER_OFFSET(target_data, size_t(target_loops->indices[j]) * data_size),
                       elem,
                       data_size);
              }
            }
          });
    }
  }

  /* Make sure active/default color attribute (names) are brought over. */
  if (source->active_color_attribute) {
    MEM_SAFE_FREE(target->active_color_attribute);
    target->active_color_attribute = BLI_strdup(source->active_color_attribute);
  }
  if (source->default_color_attribute) {
    MEM_SAFE_FREE(target->default_color_attribute);
    target->default_color_attribute = BLI_strdup(source->default_color_attribute);
  }

  MEM_SAFE_FREE(source_lmap);
  MEM_SAFE_FREE(source_lmap_mem);
  MEM_SAFE_FREE(target_lmap);
  MEM_SAFE_FREE(target_lmap_mem);
  free_bvhtree_from_mesh(&bvhtree);
}

struct Mesh *BKE_mesh_remesh_voxel_fix_poles(const Mesh *mesh)
{
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(mesh);

  BMeshCreateParams bmesh_create_params{};
  bmesh_create_params.use_toolflags = true;
  BMesh *bm = BM_mesh_create(&allocsize, &bmesh_create_params);

  BMeshFromMeshParams bmesh_from_mesh_params{};
  bmesh_from_mesh_params.calc_face_normal = true;
  bmesh_from_mesh_params.calc_vert_normal = true;
  BM_mesh_bm_from_me(bm, mesh, &bmesh_from_mesh_params);

  BMVert *v;
  BMEdge *ed, *ed_next;
  BMFace *f, *f_next;
  BMIter iter_a, iter_b;

  /* Merge 3 edge poles vertices that exist in the same face */
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
  BM_ITER_MESH_MUTABLE (f, f_next, &iter_a, bm, BM_FACES_OF_MESH) {
    BMVert *v1, *v2;
    v1 = nullptr;
    v2 = nullptr;
    BM_ITER_ELEM (v, &iter_b, f, BM_VERTS_OF_FACE) {
      if (BM_vert_edge_count(v) == 3) {
        if (v1) {
          v2 = v;
        }
        else {
          v1 = v;
        }
      }
    }
    if (v1 && v2 && (v1 != v2) && !BM_edge_exists(v1, v2)) {
      BM_face_kill(bm, f);
      BMEdge *e = BM_edge_create(bm, v1, v2, nullptr, BM_CREATE_NOP);
      BM_elem_flag_set(e, BM_ELEM_TAG, true);
    }
  }

  BM_ITER_MESH_MUTABLE (ed, ed_next, &iter_a, bm, BM_EDGES_OF_MESH) {
    if (BM_elem_flag_test(ed, BM_ELEM_TAG)) {
      float co[3];
      mid_v3_v3v3(co, ed->v1->co, ed->v2->co);
      BMVert *vc = BM_edge_collapse(bm, ed, ed->v1, true, true);
      copy_v3_v3(vc->co, co);
    }
  }

  /* Delete faces with a 3 edge pole in all their vertices */
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
  BM_ITER_MESH (f, &iter_a, bm, BM_FACES_OF_MESH) {
    bool dissolve = true;
    BM_ITER_ELEM (v, &iter_b, f, BM_VERTS_OF_FACE) {
      if (BM_vert_edge_count(v) != 3) {
        dissolve = false;
      }
    }
    if (dissolve) {
      BM_ITER_ELEM (v, &iter_b, f, BM_VERTS_OF_FACE) {
        BM_elem_flag_set(v, BM_ELEM_TAG, true);
      }
    }
  }
  BM_mesh_delete_hflag_context(bm, BM_ELEM_TAG, DEL_VERTS);

  BM_ITER_MESH (ed, &iter_a, bm, BM_EDGES_OF_MESH) {
    if (BM_edge_face_count(ed) != 2) {
      BM_elem_flag_set(ed, BM_ELEM_TAG, true);
    }
  }
  BM_mesh_edgenet(bm, false, true);

  /* Smooth the result */
  for (int i = 0; i < 4; i++) {
    BM_ITER_MESH (v, &iter_a, bm, BM_VERTS_OF_MESH) {
      float co[3];
      zero_v3(co);
      BM_ITER_ELEM (ed, &iter_b, v, BM_EDGES_OF_VERT) {
        BMVert *vert = BM_edge_other_vert(ed, v);
        add_v3_v3(co, vert->co);
      }
      mul_v3_fl(co, 1.0f / float(BM_vert_edge_count(v)));
      mid_v3_v3v3(v->co, v->co, co);
    }
  }

  BM_mesh_normals_update(bm);

  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_SELECT, false);
  BM_mesh_elem_hflag_enable_all(bm, BM_FACE, BM_ELEM_TAG, false);
  BMO_op_callf(bm,
               (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
               "recalc_face_normals faces=%hf",
               BM_ELEM_TAG);
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);

  BMeshToMeshParams bmesh_to_mesh_params{};
  bmesh_to_mesh_params.calc_object_remap = false;
  Mesh *result = BKE_mesh_from_bmesh_nomain(bm, &bmesh_to_mesh_params, mesh);

  BM_mesh_free(bm);
  return result;
}
