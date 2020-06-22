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

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
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

#ifdef WITH_OPENVDB
struct OpenVDBLevelSet *BKE_mesh_remesh_voxel_ovdb_mesh_to_level_set_create(
    Mesh *mesh, struct OpenVDBTransform *transform)
{
  BKE_mesh_runtime_looptri_recalc(mesh);
  const MLoopTri *looptri = BKE_mesh_runtime_looptri_ensure(mesh);
  MVertTri *verttri = MEM_callocN(sizeof(*verttri) * BKE_mesh_runtime_looptri_len(mesh),
                                  "remesh_looptri");
  BKE_mesh_runtime_verttri_from_looptri(
      verttri, mesh->mloop, looptri, BKE_mesh_runtime_looptri_len(mesh));

  unsigned int totfaces = BKE_mesh_runtime_looptri_len(mesh);
  unsigned int totverts = mesh->totvert;
  float *verts = (float *)MEM_malloc_arrayN(totverts * 3, sizeof(float), "remesh_input_verts");
  unsigned int *faces = (unsigned int *)MEM_malloc_arrayN(
      totfaces * 3, sizeof(unsigned int), "remesh_intput_faces");

  for (unsigned int i = 0; i < totverts; i++) {
    MVert *mvert = &mesh->mvert[i];
    verts[i * 3] = mvert->co[0];
    verts[i * 3 + 1] = mvert->co[1];
    verts[i * 3 + 2] = mvert->co[2];
  }

  for (unsigned int i = 0; i < totfaces; i++) {
    MVertTri *vt = &verttri[i];
    faces[i * 3] = vt->tri[0];
    faces[i * 3 + 1] = vt->tri[1];
    faces[i * 3 + 2] = vt->tri[2];
  }

  struct OpenVDBLevelSet *level_set = OpenVDBLevelSet_create(false, NULL);
  OpenVDBLevelSet_mesh_to_level_set(level_set, verts, faces, totverts, totfaces, transform);

  MEM_freeN(verts);
  MEM_freeN(faces);
  MEM_freeN(verttri);

  return level_set;
}

Mesh *BKE_mesh_remesh_voxel_ovdb_volume_to_mesh_nomain(struct OpenVDBLevelSet *level_set,
                                                       double isovalue,
                                                       double adaptivity,
                                                       bool relax_disoriented_triangles)
{
#  ifdef WITH_OPENVDB
  struct OpenVDBVolumeToMeshData output_mesh;
  OpenVDBLevelSet_volume_to_mesh(
      level_set, &output_mesh, isovalue, adaptivity, relax_disoriented_triangles);
#  endif

  Mesh *mesh = BKE_mesh_new_nomain(output_mesh.totvertices,
                                   0,
                                   0,
                                   (output_mesh.totquads * 4) + (output_mesh.tottriangles * 3),
                                   output_mesh.totquads + output_mesh.tottriangles);

  for (int i = 0; i < output_mesh.totvertices; i++) {
    copy_v3_v3(mesh->mvert[i].co, &output_mesh.vertices[i * 3]);
  }

  MPoly *mp = mesh->mpoly;
  MLoop *ml = mesh->mloop;
  for (int i = 0; i < output_mesh.totquads; i++, mp++, ml += 4) {
    mp->loopstart = (int)(ml - mesh->mloop);
    mp->totloop = 4;

    ml[0].v = output_mesh.quads[i * 4 + 3];
    ml[1].v = output_mesh.quads[i * 4 + 2];
    ml[2].v = output_mesh.quads[i * 4 + 1];
    ml[3].v = output_mesh.quads[i * 4];
  }

  for (int i = 0; i < output_mesh.tottriangles; i++, mp++, ml += 3) {
    mp->loopstart = (int)(ml - mesh->mloop);
    mp->totloop = 3;

    ml[0].v = output_mesh.triangles[i * 3 + 2];
    ml[1].v = output_mesh.triangles[i * 3 + 1];
    ml[2].v = output_mesh.triangles[i * 3];
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

#ifdef WITH_QUADRIFLOW
static Mesh *BKE_mesh_remesh_quadriflow(Mesh *input_mesh,
                                        int target_faces,
                                        int seed,
                                        bool preserve_sharp,
                                        bool preserve_boundary,
                                        bool adaptive_scale,
                                        void *update_cb,
                                        void *update_cb_data)
{
  /* Ensure that the triangulated mesh data is up to data */
  BKE_mesh_runtime_looptri_recalc(input_mesh);
  const MLoopTri *looptri = BKE_mesh_runtime_looptri_ensure(input_mesh);

  /* Gather the required data for export to the internal quadiflow mesh format */
  MVertTri *verttri = MEM_callocN(sizeof(*verttri) * BKE_mesh_runtime_looptri_len(input_mesh),
                                  "remesh_looptri");
  BKE_mesh_runtime_verttri_from_looptri(
      verttri, input_mesh->mloop, looptri, BKE_mesh_runtime_looptri_len(input_mesh));

  unsigned int totfaces = BKE_mesh_runtime_looptri_len(input_mesh);
  unsigned int totverts = input_mesh->totvert;
  float *verts = (float *)MEM_malloc_arrayN(totverts * 3, sizeof(float), "remesh_input_verts");
  unsigned int *faces = (unsigned int *)MEM_malloc_arrayN(
      totfaces * 3, sizeof(unsigned int), "remesh_intput_faces");

  for (unsigned int i = 0; i < totverts; i++) {
    MVert *mvert = &input_mesh->mvert[i];
    verts[i * 3] = mvert->co[0];
    verts[i * 3 + 1] = mvert->co[1];
    verts[i * 3 + 2] = mvert->co[2];
  }

  for (unsigned int i = 0; i < totfaces; i++) {
    MVertTri *vt = &verttri[i];
    faces[i * 3] = vt->tri[0];
    faces[i * 3 + 1] = vt->tri[1];
    faces[i * 3 + 2] = vt->tri[2];
  }

  /* Fill out the required input data */
  QuadriflowRemeshData qrd;

  qrd.totfaces = totfaces;
  qrd.totverts = totverts;
  qrd.verts = verts;
  qrd.faces = faces;
  qrd.target_faces = target_faces;

  qrd.preserve_sharp = preserve_sharp;
  qrd.preserve_boundary = preserve_boundary;
  qrd.adaptive_scale = adaptive_scale;
  qrd.minimum_cost_flow = 0;
  qrd.aggresive_sat = 0;
  qrd.rng_seed = seed;

  qrd.out_faces = NULL;

  /* Run the remesher */
  QFLOW_quadriflow_remesh(&qrd, update_cb, update_cb_data);

  MEM_freeN(verts);
  MEM_freeN(faces);
  MEM_freeN(verttri);

  if (qrd.out_faces == NULL) {
    /* The remeshing was canceled */
    return NULL;
  }

  if (qrd.out_totfaces == 0) {
    /* Meshing failed */
    MEM_freeN(qrd.out_faces);
    MEM_freeN(qrd.out_verts);
    return NULL;
  }

  /* Construct the new output mesh */
  Mesh *mesh = BKE_mesh_new_nomain(
      qrd.out_totverts, 0, 0, (qrd.out_totfaces * 4), qrd.out_totfaces);

  for (int i = 0; i < qrd.out_totverts; i++) {
    copy_v3_v3(mesh->mvert[i].co, &qrd.out_verts[i * 3]);
  }

  MPoly *mp = mesh->mpoly;
  MLoop *ml = mesh->mloop;
  for (int i = 0; i < qrd.out_totfaces; i++, mp++, ml += 4) {
    mp->loopstart = (int)(ml - mesh->mloop);
    mp->totloop = 4;

    ml[0].v = qrd.out_faces[i * 4];
    ml[1].v = qrd.out_faces[i * 4 + 1];
    ml[2].v = qrd.out_faces[i * 4 + 2];
    ml[3].v = qrd.out_faces[i * 4 + 3];
  }

  BKE_mesh_calc_edges(mesh, false, false);
  BKE_mesh_calc_normals(mesh);

  MEM_freeN(qrd.out_faces);
  MEM_freeN(qrd.out_verts);

  return mesh;
}
#endif

Mesh *BKE_mesh_remesh_quadriflow_to_mesh_nomain(Mesh *mesh,
                                                int target_faces,
                                                int seed,
                                                bool preserve_sharp,
                                                bool preserve_boundary,
                                                bool adaptive_scale,
                                                void *update_cb,
                                                void *update_cb_data)
{
  Mesh *new_mesh = NULL;
#ifdef WITH_QUADRIFLOW
  if (target_faces <= 0) {
    target_faces = -1;
  }
  new_mesh = BKE_mesh_remesh_quadriflow(mesh,
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
#endif
  return new_mesh;
}

Mesh *BKE_mesh_remesh_voxel_to_mesh_nomain(Mesh *mesh,
                                           float voxel_size,
                                           float adaptivity,
                                           float isovalue)
{
  Mesh *new_mesh = NULL;
#ifdef WITH_OPENVDB
  struct OpenVDBLevelSet *level_set;
  struct OpenVDBTransform *xform = OpenVDBTransform_create();
  OpenVDBTransform_create_linear_transform(xform, (double)voxel_size);
  level_set = BKE_mesh_remesh_voxel_ovdb_mesh_to_level_set_create(mesh, xform);
  new_mesh = BKE_mesh_remesh_voxel_ovdb_volume_to_mesh_nomain(
      level_set, (double)isovalue, (double)adaptivity, false);
  OpenVDBLevelSet_free(level_set);
  OpenVDBTransform_free(xform);
#else
  UNUSED_VARS(mesh, voxel_size, adaptivity, isovalue);
#endif
  return new_mesh;
}

void BKE_mesh_remesh_reproject_paint_mask(Mesh *target, Mesh *source)
{
  BVHTreeFromMesh bvhtree = {
      .nearest_callback = NULL,
  };
  BKE_bvhtree_from_mesh_get(&bvhtree, source, BVHTREE_FROM_VERTS, 2);
  MVert *target_verts = CustomData_get_layer(&target->vdata, CD_MVERT);

  float *target_mask;
  if (CustomData_has_layer(&target->vdata, CD_PAINT_MASK)) {
    target_mask = CustomData_get_layer(&target->vdata, CD_PAINT_MASK);
  }
  else {
    target_mask = CustomData_add_layer(
        &target->vdata, CD_PAINT_MASK, CD_CALLOC, NULL, target->totvert);
  }

  float *source_mask;
  if (CustomData_has_layer(&source->vdata, CD_PAINT_MASK)) {
    source_mask = CustomData_get_layer(&source->vdata, CD_PAINT_MASK);
  }
  else {
    source_mask = CustomData_add_layer(
        &source->vdata, CD_PAINT_MASK, CD_CALLOC, NULL, source->totvert);
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
  BVHTreeFromMesh bvhtree = {
      .nearest_callback = NULL,
  };

  const MPoly *target_polys = CustomData_get_layer(&target->pdata, CD_MPOLY);
  const MVert *target_verts = CustomData_get_layer(&target->vdata, CD_MVERT);
  const MLoop *target_loops = CustomData_get_layer(&target->ldata, CD_MLOOP);

  int *target_face_sets;
  if (CustomData_has_layer(&target->pdata, CD_SCULPT_FACE_SETS)) {
    target_face_sets = CustomData_get_layer(&target->pdata, CD_SCULPT_FACE_SETS);
  }
  else {
    target_face_sets = CustomData_add_layer(
        &target->pdata, CD_SCULPT_FACE_SETS, CD_CALLOC, NULL, target->totpoly);
  }

  int *source_face_sets;
  if (CustomData_has_layer(&source->pdata, CD_SCULPT_FACE_SETS)) {
    source_face_sets = CustomData_get_layer(&source->pdata, CD_SCULPT_FACE_SETS);
  }
  else {
    source_face_sets = CustomData_add_layer(
        &source->pdata, CD_SCULPT_FACE_SETS, CD_CALLOC, NULL, source->totpoly);
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
  BVHTreeFromMesh bvhtree = {
      .nearest_callback = NULL,
  };
  BKE_bvhtree_from_mesh_get(&bvhtree, source, BVHTREE_FROM_VERTS, 2);

  int tot_color_layer = CustomData_number_of_layers(&source->vdata, CD_PROP_COLOR);

  for (int layer_n = 0; layer_n < tot_color_layer; layer_n++) {
    const char *layer_name = CustomData_get_layer_name(&source->vdata, CD_PROP_COLOR, layer_n);
    CustomData_add_layer_named(
        &target->vdata, CD_PROP_COLOR, CD_CALLOC, NULL, target->totvert, layer_name);

    MPropCol *target_color = CustomData_get_layer_n(&target->vdata, CD_PROP_COLOR, layer_n);
    MVert *target_verts = CustomData_get_layer(&target->vdata, CD_MVERT);
    MPropCol *source_color = CustomData_get_layer_n(&source->vdata, CD_PROP_COLOR, layer_n);
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
  bm = BM_mesh_create(&allocsize,
                      &((struct BMeshCreateParams){
                          .use_toolflags = true,
                      }));

  BM_mesh_bm_from_me(bm,
                     mesh,
                     (&(struct BMeshFromMeshParams){
                         .calc_face_normal = true,
                     }));

  BMVert *v;
  BMEdge *ed, *ed_next;
  BMFace *f, *f_next;
  BMIter iter_a, iter_b;

  /* Merge 3 edge poles vertices that exist in the same face */
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
  BM_ITER_MESH_MUTABLE (f, f_next, &iter_a, bm, BM_FACES_OF_MESH) {
    BMVert *v1, *v2;
    v1 = NULL;
    v2 = NULL;
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
      BMEdge *e = BM_edge_create(bm, v1, v2, NULL, BM_CREATE_NOP);
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

  Mesh *result = BKE_mesh_from_bmesh_nomain(bm,
                                            (&(struct BMeshToMeshParams){
                                                .calc_object_remap = false,
                                            }),
                                            mesh);

  BKE_id_free(NULL, mesh);
  BM_mesh_free(bm);
  return result;
}
