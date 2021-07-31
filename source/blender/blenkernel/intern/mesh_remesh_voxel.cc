/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2019 by Blender Foundation
 * All rights reserved.
 */

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
#include "BLI_blenlib.h"
#include "BLI_float3.hh"
#include "BLI_index_range.hh"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_bvhutils.h"
#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_remesh_voxel.h" /* own include */
#include "BKE_mesh_runtime.h"

#include "bmesh_tools.h"

#ifdef WITH_OPENVDB
#  include "openvdb_capi.h"
#endif

#ifdef WITH_QUADRIFLOW
#  include "quadriflow_capi.hpp"
#endif

using blender::Array;
using blender::float3;
using blender::IndexRange;

#ifdef WITH_QUADRIFLOW
static Mesh *remesh_quadriflow(Mesh *input_mesh,
                               int target_faces,
                               int seed,
                               bool preserve_sharp,
                               bool preserve_boundary,
                               bool adaptive_scale,
                               void (*update_cb)(void *, float progress, int *cancel),
                               void *update_cb_data)
{
  /* Ensure that the triangulated mesh data is up to data */
  BKE_mesh_runtime_looptri_recalc(input_mesh);
  const MLoopTri *looptri = BKE_mesh_runtime_looptri_ensure(input_mesh);

  /* Gather the required data for export to the internal quadiflow mesh format */
  MVertTri *verttri = (MVertTri *)MEM_callocN(
      sizeof(*verttri) * BKE_mesh_runtime_looptri_len(input_mesh), "remesh_looptri");
  BKE_mesh_runtime_verttri_from_looptri(
      verttri, input_mesh->mloop, looptri, BKE_mesh_runtime_looptri_len(input_mesh));

  const int totfaces = BKE_mesh_runtime_looptri_len(input_mesh);
  const int totverts = input_mesh->totvert;
  Array<float3> verts(totverts);
  Array<int> faces(totfaces * 3);

  for (const int i : IndexRange(totverts)) {
    verts[i] = input_mesh->mvert[i].co;
  }

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
  qrd.verts = (float *)verts.data();
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

  MEM_freeN(verttri);

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
  Mesh *mesh = BKE_mesh_new_nomain(qrd.out_totverts, 0, 0, qrd.out_totfaces * 4, qrd.out_totfaces);

  for (const int i : IndexRange(qrd.out_totverts)) {
    copy_v3_v3(mesh->mvert[i].co, &qrd.out_verts[i * 3]);
  }

  for (const int i : IndexRange(qrd.out_totfaces)) {
    MPoly &poly = mesh->mpoly[i];
    const int loopstart = i * 4;
    poly.loopstart = loopstart;
    poly.totloop = 4;
    mesh->mloop[loopstart].v = qrd.out_faces[loopstart];
    mesh->mloop[loopstart + 1].v = qrd.out_faces[loopstart + 1];
    mesh->mloop[loopstart + 2].v = qrd.out_faces[loopstart + 2];
    mesh->mloop[loopstart + 3].v = qrd.out_faces[loopstart + 3];
  }

  BKE_mesh_calc_edges(mesh, false, false);
  BKE_mesh_calc_normals(mesh);

  MEM_freeN(qrd.out_faces);
  MEM_freeN(qrd.out_verts);

  return mesh;
}
#endif

Mesh *BKE_mesh_remesh_quadriflow(Mesh *mesh,
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
static struct OpenVDBLevelSet *remesh_voxel_level_set_create(Mesh *mesh,
                                                             struct OpenVDBTransform *transform)
{
  BKE_mesh_runtime_looptri_recalc(mesh);
  const MLoopTri *looptri = BKE_mesh_runtime_looptri_ensure(mesh);
  MVertTri *verttri = (MVertTri *)MEM_callocN(
      sizeof(*verttri) * BKE_mesh_runtime_looptri_len(mesh), "remesh_looptri");
  BKE_mesh_runtime_verttri_from_looptri(
      verttri, mesh->mloop, looptri, BKE_mesh_runtime_looptri_len(mesh));

  const int totfaces = BKE_mesh_runtime_looptri_len(mesh);
  const int totverts = mesh->totvert;
  Array<float3> verts(totverts);
  Array<int> faces(totfaces * 3);

  for (const int i : IndexRange(totverts)) {
    verts[i] = mesh->mvert[i].co;
  }

  for (const int i : IndexRange(totfaces)) {
    MVertTri &vt = verttri[i];
    faces[i * 3] = vt.tri[0];
    faces[i * 3 + 1] = vt.tri[1];
    faces[i * 3 + 2] = vt.tri[2];
  }

  struct OpenVDBLevelSet *level_set = OpenVDBLevelSet_create(false, nullptr);
  OpenVDBLevelSet_mesh_to_level_set(
      level_set, (const float *)verts.data(), faces.data(), totverts, totfaces, transform);

  MEM_freeN(verttri);

  return level_set;
}

static Mesh *remesh_voxel_volume_to_mesh(struct OpenVDBLevelSet *level_set,
                                         double isovalue,
                                         double adaptivity,
                                         bool relax_disoriented_triangles)
{
  struct OpenVDBVolumeToMeshData output_mesh;
  OpenVDBLevelSet_volume_to_mesh(
      level_set, &output_mesh, isovalue, adaptivity, relax_disoriented_triangles);

  Mesh *mesh = BKE_mesh_new_nomain(output_mesh.totvertices,
                                   0,
                                   0,
                                   (output_mesh.totquads * 4) + (output_mesh.tottriangles * 3),
                                   output_mesh.totquads + output_mesh.tottriangles);

  for (const int i : IndexRange(output_mesh.totvertices)) {
    copy_v3_v3(mesh->mvert[i].co, &output_mesh.vertices[i * 3]);
  }

  for (const int i : IndexRange(output_mesh.totquads)) {
    MPoly &poly = mesh->mpoly[i];
    const int loopstart = i * 4;
    poly.loopstart = loopstart;
    poly.totloop = 4;
    mesh->mloop[loopstart].v = output_mesh.quads[loopstart];
    mesh->mloop[loopstart + 1].v = output_mesh.quads[loopstart + 1];
    mesh->mloop[loopstart + 2].v = output_mesh.quads[loopstart + 2];
    mesh->mloop[loopstart + 3].v = output_mesh.quads[loopstart + 3];
  }

  const int triangle_poly_start = output_mesh.totquads;
  const int triangle_loop_start = output_mesh.totquads * 4;
  for (const int i : IndexRange(output_mesh.tottriangles)) {
    MPoly &poly = mesh->mpoly[triangle_poly_start + i];
    const int loopstart = triangle_loop_start + i * 3;
    poly.loopstart = loopstart;
    poly.totloop = 3;
    mesh->mloop[loopstart].v = output_mesh.triangles[i * 3 + 2];
    mesh->mloop[loopstart + 1].v = output_mesh.triangles[i * 3 + 1];
    mesh->mloop[loopstart + 2].v = output_mesh.triangles[i * 3];
  }

  BKE_mesh_calc_edges(mesh, false, false);
  BKE_mesh_calc_normals(mesh);

  MEM_freeN(output_mesh.quads);
  MEM_freeN(output_mesh.vertices);

  if (output_mesh.tottriangles > 0) {
    MEM_freeN(output_mesh.triangles);
  }

  return mesh;
}
#endif

Mesh *BKE_mesh_remesh_voxel(Mesh *mesh, float voxel_size, float adaptivity, float isovalue)
{
#ifdef WITH_OPENVDB
  struct OpenVDBTransform *xform = OpenVDBTransform_create();
  OpenVDBTransform_create_linear_transform(xform, (double)voxel_size);
  struct OpenVDBLevelSet *level_set = remesh_voxel_level_set_create(mesh, xform);
  Mesh *new_mesh = remesh_voxel_volume_to_mesh(
      level_set, (double)isovalue, (double)adaptivity, false);
  OpenVDBLevelSet_free(level_set);
  OpenVDBTransform_free(xform);
  return new_mesh;
#else
  UNUSED_VARS(mesh, voxel_size, adaptivity, isovalue);
  return nullptr;
#endif
}

void BKE_mesh_remesh_reproject_paint_mask(Mesh *target, Mesh *source)
{
  BVHTreeFromMesh bvhtree = {{nullptr}};
  BKE_bvhtree_from_mesh_get(&bvhtree, source, BVHTREE_FROM_VERTS, 2);
  MVert *target_verts = (MVert *)CustomData_get_layer(&target->vdata, CD_MVERT);

  float *target_mask;
  if (CustomData_has_layer(&target->vdata, CD_PAINT_MASK)) {
    target_mask = (float *)CustomData_get_layer(&target->vdata, CD_PAINT_MASK);
  }
  else {
    target_mask = (float *)CustomData_add_layer(
        &target->vdata, CD_PAINT_MASK, CD_CALLOC, nullptr, target->totvert);
  }

  float *source_mask;
  if (CustomData_has_layer(&source->vdata, CD_PAINT_MASK)) {
    source_mask = (float *)CustomData_get_layer(&source->vdata, CD_PAINT_MASK);
  }
  else {
    source_mask = (float *)CustomData_add_layer(
        &source->vdata, CD_PAINT_MASK, CD_CALLOC, nullptr, source->totvert);
  }

  for (int i = 0; i < target->totvert; i++) {
    float from_co[3];
    BVHTreeNearest nearest;
    nearest.index = -1;
    nearest.dist_sq = FLT_MAX;
    copy_v3_v3(from_co, target_verts[i].co);
    BLI_bvhtree_find_nearest(bvhtree.tree, from_co, &nearest, bvhtree.nearest_callback, &bvhtree);
    if (nearest.index != -1) {
      target_mask[i] = source_mask[nearest.index];
    }
  }
  free_bvhtree_from_mesh(&bvhtree);
}

void BKE_remesh_reproject_sculpt_face_sets(Mesh *target, Mesh *source)
{
  BVHTreeFromMesh bvhtree = {{nullptr}};

  const MPoly *target_polys = (const MPoly *)CustomData_get_layer(&target->pdata, CD_MPOLY);
  const MVert *target_verts = (const MVert *)CustomData_get_layer(&target->vdata, CD_MVERT);
  const MLoop *target_loops = (const MLoop *)CustomData_get_layer(&target->ldata, CD_MLOOP);

  int *target_face_sets;
  if (CustomData_has_layer(&target->pdata, CD_SCULPT_FACE_SETS)) {
    target_face_sets = (int *)CustomData_get_layer(&target->pdata, CD_SCULPT_FACE_SETS);
  }
  else {
    target_face_sets = (int *)CustomData_add_layer(
        &target->pdata, CD_SCULPT_FACE_SETS, CD_CALLOC, nullptr, target->totpoly);
  }

  int *source_face_sets;
  if (CustomData_has_layer(&source->pdata, CD_SCULPT_FACE_SETS)) {
    source_face_sets = (int *)CustomData_get_layer(&source->pdata, CD_SCULPT_FACE_SETS);
  }
  else {
    source_face_sets = (int *)CustomData_add_layer(
        &source->pdata, CD_SCULPT_FACE_SETS, CD_CALLOC, nullptr, source->totpoly);
  }

  const MLoopTri *looptri = BKE_mesh_runtime_looptri_ensure(source);
  BKE_bvhtree_from_mesh_get(&bvhtree, source, BVHTREE_FROM_LOOPTRI, 2);

  for (int i = 0; i < target->totpoly; i++) {
    float from_co[3];
    BVHTreeNearest nearest;
    nearest.index = -1;
    nearest.dist_sq = FLT_MAX;
    const MPoly *mpoly = &target_polys[i];
    BKE_mesh_calc_poly_center(mpoly, &target_loops[mpoly->loopstart], target_verts, from_co);
    BLI_bvhtree_find_nearest(bvhtree.tree, from_co, &nearest, bvhtree.nearest_callback, &bvhtree);
    if (nearest.index != -1) {
      target_face_sets[i] = source_face_sets[looptri[nearest.index].poly];
    }
    else {
      target_face_sets[i] = 1;
    }
  }
  free_bvhtree_from_mesh(&bvhtree);
}

void BKE_remesh_reproject_vertex_paint(Mesh *target, Mesh *source)
{
  BVHTreeFromMesh bvhtree = {{nullptr}};
  BKE_bvhtree_from_mesh_get(&bvhtree, source, BVHTREE_FROM_VERTS, 2);

  int tot_color_layer = CustomData_number_of_layers(&source->vdata, CD_PROP_COLOR);

  for (int layer_n = 0; layer_n < tot_color_layer; layer_n++) {
    const char *layer_name = CustomData_get_layer_name(&source->vdata, CD_PROP_COLOR, layer_n);
    CustomData_add_layer_named(
        &target->vdata, CD_PROP_COLOR, CD_CALLOC, nullptr, target->totvert, layer_name);

    MPropCol *target_color = (MPropCol *)CustomData_get_layer_n(
        &target->vdata, CD_PROP_COLOR, layer_n);
    MVert *target_verts = (MVert *)CustomData_get_layer(&target->vdata, CD_MVERT);
    MPropCol *source_color = (MPropCol *)CustomData_get_layer_n(
        &source->vdata, CD_PROP_COLOR, layer_n);
    for (int i = 0; i < target->totvert; i++) {
      BVHTreeNearest nearest;
      nearest.index = -1;
      nearest.dist_sq = FLT_MAX;
      BLI_bvhtree_find_nearest(
          bvhtree.tree, target_verts[i].co, &nearest, bvhtree.nearest_callback, &bvhtree);
      if (nearest.index != -1) {
        copy_v4_v4(target_color[i].color, source_color[nearest.index].color);
      }
    }
  }
  free_bvhtree_from_mesh(&bvhtree);
}

struct Mesh *BKE_mesh_remesh_voxel_fix_poles(struct Mesh *mesh)
{
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(mesh);
  BMesh *bm;

  const BMeshCreateParams bmesh_create_params = {true};
  bm = BM_mesh_create(&allocsize, &bmesh_create_params);

  BMeshFromMeshParams bmesh_from_mesh_params{};
  bmesh_from_mesh_params.calc_face_normal = true;
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
      mul_v3_fl(co, 1.0f / (float)BM_vert_edge_count(v));
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

  BKE_id_free(nullptr, mesh);
  BM_mesh_free(bm);
  return result;
}
