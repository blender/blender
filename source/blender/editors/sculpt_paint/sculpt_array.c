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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_hash.h"
#include "BLI_math.h"
#include "BLI_task.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_scene.h"

#include "paint_intern.h"
#include "sculpt_intern.h"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "ED_sculpt.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include <math.h>
#include <stdlib.h>

static const char array_symmetry_pass_cd_name[] = "v_symmetry_pass";
static const char array_instance_cd_name[] = "v_array_instance";

#define ARRAY_INSTANCE_ORIGINAL -1

static void sculpt_vertex_array_data_get(SculptArray *array,
                                         const int vertex,
                                         int *r_copy,
                                         int *r_symmetry_pass)
{
  if (!array->copy_index) {
    printf("NO ARRAY COPY\n");
    *r_copy = ARRAY_INSTANCE_ORIGINAL;
    *r_symmetry_pass = 0;
    return;
  }
  *r_copy = array->copy_index[vertex];
  *r_symmetry_pass = array->symmetry_pass[vertex];
}

static void sculpt_array_datalayers_init(SculptArray *array, SculptSession *ss)
{
  SculptLayerParams params = {.permanent = true, .simple_array = false};

  if (!array->scl_inst) {
    array->scl_inst = MEM_callocN(sizeof(SculptCustomLayer), __func__);
  }

  if (!array->scl_sym) {
    array->scl_sym = MEM_callocN(sizeof(SculptCustomLayer), __func__);
  }

  SCULPT_temp_customlayer_ensure(
      ss, ATTR_DOMAIN_POINT, CD_PROP_INT32, array_instance_cd_name, &params);
  SCULPT_temp_customlayer_get(
      ss, ATTR_DOMAIN_POINT, CD_PROP_INT32, array_instance_cd_name, array->scl_inst, &params);

  SCULPT_temp_customlayer_ensure(
      ss, ATTR_DOMAIN_POINT, CD_PROP_INT32, array_symmetry_pass_cd_name, &params);
  SCULPT_temp_customlayer_get(
      ss, ATTR_DOMAIN_POINT, CD_PROP_INT32, array_symmetry_pass_cd_name, array->scl_sym, &params);
}

static void sculpt_array_datalayers_add(SculptArray *array, SculptSession *ss, Mesh *mesh)
{

  // int *v_array_instance = CustomData_add_layer_named(
  //    &mesh->vdata, CD_PROP_INT32, CD_CALLOC, NULL, mesh->totvert, array_instance_cd_name);
  int totvert = SCULPT_vertex_count_get(ss);
  const SculptCustomLayer *scl = array->scl_inst;

  for (int i = 0; i < totvert; i++) {
    SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

    *(int *)SCULPT_temp_cdata_get(vertex, scl) = ARRAY_INSTANCE_ORIGINAL;
  }
}

ATTR_NO_OPT void SCULPT_array_datalayers_free(SculptArray *array, Object *ob)
{
  SculptSession *ss = ob->sculpt;

  if (array->scl_inst) {
    SCULPT_temp_customlayer_release(ss, array->scl_inst);
  }

  if (array->scl_sym) {
    SCULPT_temp_customlayer_release(ss, array->scl_sym);
  }

  array->scl_inst = NULL;
  array->scl_sym = NULL;
  return;
  Mesh *mesh = BKE_object_get_original_mesh(ob);
  int v_layer_index = CustomData_get_named_layer_index(
      &mesh->vdata, CD_PROP_INT32, array_instance_cd_name);
  if (v_layer_index != -1) {
    CustomData_free_layer(&mesh->vdata, CD_PROP_INT32, mesh->totvert, v_layer_index);
  }

  v_layer_index = CustomData_get_named_layer_index(
      &mesh->vdata, CD_PROP_INT32, array_symmetry_pass_cd_name);
  if (v_layer_index != -1) {
    CustomData_free_layer(&mesh->vdata, CD_PROP_INT32, mesh->totvert, v_layer_index);
  }
}

const float source_geometry_threshold = 0.5f;

static BMesh *sculpt_array_source_build(Object *ob, Brush *brush, SculptArray *array)
{
  bool have_bmesh = ob->sculpt->bm && ob->sculpt->pbvh &&
                    BKE_pbvh_type(ob->sculpt->pbvh) == PBVH_BMESH;

  Mesh *sculpt_mesh = BKE_object_get_original_mesh(ob);

  BMesh *srcbm;

  BMesh *BM_mesh_copy_ex(BMesh * bm_old, struct BMeshCreateParams * params);

  if (have_bmesh) {
    srcbm = BM_mesh_copy_ex(
        ob->sculpt->bm,
        &((struct BMeshCreateParams){.use_toolflags = true,
                                     .id_map = false,
                                     .id_elem_mask = ob->sculpt->bm->idmap.flag &
                                                     (BM_VERT | BM_EDGE | BM_FACE | BM_LOOP),
                                     .create_unique_ids = true,
                                     .copy_all_layers = true}));
  }
  else {
    const BMAllocTemplate allocsizea = BMALLOC_TEMPLATE_FROM_ME(sculpt_mesh);
    srcbm = BM_mesh_create(&allocsizea,
                           &((struct BMeshCreateParams){
                               .use_toolflags = true,
                           }));

    BM_mesh_bm_from_me(NULL,
                       srcbm,
                       sculpt_mesh,
                       &((struct BMeshFromMeshParams){
                           .calc_face_normal = true,
                       }));
  }

  BM_mesh_elem_table_ensure(srcbm, BM_VERT);
  BM_mesh_elem_index_ensure(srcbm, BM_VERT);

  int vert_count = 0;
  zero_v3(array->source_origin);

  SculptSession *ss = ob->sculpt;
  for (int i = 0; i < srcbm->totvert; i++) {
    SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

    const float automask = SCULPT_automasking_factor_get(ss->cache->automasking, ss, vertex);
    const float mask = 1.0f - SCULPT_vertex_mask_get(ss, vertex);
    const float influence = mask * automask;

    BMVert *vert = BM_vert_at_index(srcbm, i);
    if (influence >= source_geometry_threshold) {
      vert_count++;
      add_v3_v3(array->source_origin, vert->co);
      continue;
    }
    BM_elem_flag_set(vert, BM_ELEM_TAG, true);
  }

  if (vert_count == 0) {
    return srcbm;
  }

  mul_v3_fl(array->source_origin, 1.0f / vert_count);

  /* TODO(pablodp606): Handle individual Face Sets for Face Set automasking. */
  BM_mesh_delete_hflag_context(srcbm, BM_ELEM_TAG, DEL_VERTS);

  const bool fill_holes = brush->flag2 & BRUSH_ARRAY_FILL_HOLES;
  if (fill_holes) {
    BM_mesh_elem_hflag_disable_all(srcbm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
    BM_mesh_elem_hflag_enable_all(srcbm, BM_EDGE, BM_ELEM_TAG, false);
    BM_mesh_edgenet(srcbm, false, true);
    BM_mesh_normals_update(srcbm);
    BMO_op_callf(srcbm,
                 (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
                 "triangulate faces=%hf quad_method=%i ngon_method=%i",
                 BM_ELEM_TAG,
                 0,
                 0);

    BM_mesh_elem_hflag_enable_all(srcbm, BM_FACE, BM_ELEM_TAG, false);
    BMO_op_callf(srcbm,
                 (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
                 "recalc_face_normals faces=%hf",
                 BM_ELEM_TAG);
    BM_mesh_elem_hflag_disable_all(srcbm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);
  }

  return srcbm;
}

ATTR_NO_OPT void sculpt_array_source_datalayer_update(BMesh *bm,
                                                      const int symm_pass,
                                                      const int copy_index)
{
  const int cd_array_instance_index = CustomData_get_named_layer_index(
      &bm->vdata, CD_PROP_INT32, array_instance_cd_name);
  const int cd_array_instance_offset = CustomData_get_n_offset(
      &bm->vdata, CD_PROP_INT32, cd_array_instance_index);

  const int cd_array_symm_pass_index = CustomData_get_named_layer_index(
      &bm->vdata, CD_PROP_INT32, array_symmetry_pass_cd_name);
  const int cd_array_symm_pass_offset = CustomData_get_n_offset(
      &bm->vdata, CD_PROP_INT32, cd_array_symm_pass_index);

  BM_mesh_elem_table_ensure(bm, BM_VERT);
  BM_mesh_elem_index_ensure(bm, BM_VERT);

  BMVert *v;
  BMIter iter;
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    BM_ELEM_CD_SET_INT(v, cd_array_instance_offset, copy_index);
    BM_ELEM_CD_SET_INT(v, cd_array_symm_pass_offset, symm_pass);
  }
}

static void sculpt_array_final_mesh_write(Object *ob, BMesh *final_mesh)
{
  SculptSession *ss = ob->sculpt;
  Mesh *sculpt_mesh = BKE_object_get_original_mesh(ob);
  Mesh *result = BKE_mesh_from_bmesh_for_eval_nomain(final_mesh, NULL, sculpt_mesh);
  result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
  BKE_mesh_nomain_to_mesh(result, ob->data, ob, &CD_MASK_MESH, true);
  BKE_mesh_batch_cache_dirty_tag(ob->data, BKE_MESH_BATCH_DIRTY_ALL);

  const int next_face_set_id = ED_sculpt_face_sets_find_next_available_id(ob->data);
  ED_sculpt_face_sets_initialize_none_to_id(ob->data, next_face_set_id);

  ss->needs_pbvh_rebuild = true;
}

ATTR_NO_OPT static void sculpt_array_ensure_geometry_indices(Object *ob, SculptArray *array)
{
  Mesh *mesh = BKE_object_get_original_mesh(ob);

  if (array->copy_index) {
    return;
  }

  printf("ALLOCATION COPY INDEX\n");

  SculptSession *ss = ob->sculpt;
  int totvert = SCULPT_vertex_count_get(ss);

  array->copy_index = MEM_malloc_arrayN(totvert, sizeof(int), "array copy index");
  array->symmetry_pass = MEM_malloc_arrayN(totvert, sizeof(int), "array symmetry pass index");

  for (int i = 0; i < totvert; i++) {
    SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

    array->copy_index[i] = *(int *)SCULPT_temp_cdata_get(vertex, array->scl_inst);
    array->symmetry_pass[i] = *(int *)SCULPT_temp_cdata_get(vertex, array->scl_sym);
  }

  SCULPT_array_datalayers_free(array, ob);
}

ATTR_NO_OPT static void sculpt_array_mesh_build(Sculpt *sd, Object *ob, SculptArray *array)
{
  bool have_bmesh = ob->sculpt->bm && ob->sculpt->pbvh &&
                    BKE_pbvh_type(ob->sculpt->pbvh) == PBVH_BMESH;

  Mesh *sculpt_mesh = BKE_object_get_original_mesh(ob);
  Brush *brush = BKE_paint_brush(&sd->paint);

  sculpt_array_datalayers_init(array, ob->sculpt);
  sculpt_array_datalayers_add(array, ob->sculpt, sculpt_mesh);

  BMesh *srcbm = sculpt_array_source_build(ob, brush, array);

  BMesh *destbm;
  const BMAllocTemplate allocsizeb = BMALLOC_TEMPLATE_FROM_ME(sculpt_mesh);

  if (!have_bmesh) {
    destbm = BM_mesh_create(&allocsizeb,
                            &((struct BMeshCreateParams){
                                .use_toolflags = true,
                            }));
    BM_mesh_bm_from_me(NULL,
                       destbm,
                       sculpt_mesh,
                       &((struct BMeshFromMeshParams){
                           .calc_face_normal = true,
                       }));
  }
  else {
    destbm = ob->sculpt->bm;
  }

  BM_mesh_toolflags_set(destbm, true);
  BM_mesh_toolflags_set(srcbm, true);

  BM_mesh_elem_toolflags_ensure(destbm);
  BM_mesh_elem_toolflags_ensure(srcbm);

  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char symm_it = 0; symm_it <= symm; symm_it++) {
    if (!SCULPT_is_symmetry_iteration_valid(symm_it, symm)) {
      continue;
    }

    for (int copy_index = 0; copy_index < array->num_copies; copy_index++) {
      sculpt_array_source_datalayer_update(srcbm, symm_it, copy_index);

      if (1) {  //! have_bmesh) {
        // BM_mesh_copy_init_customdata(destbm, srcbm, &bm_mesh_allocsize_default);
      }

      const int opflag = (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE);
      BMO_op_callf(srcbm, opflag, "duplicate geom=%avef dest=%p", destbm);
    }
  }

  if (!have_bmesh) {
    sculpt_array_final_mesh_write(ob, destbm);
    BM_mesh_free(destbm);
  }
  else {
    SCULPT_update_customdata_refs(ob->sculpt);
    ob->sculpt->needs_pbvh_rebuild = true;
  }

  BM_mesh_free(srcbm);
}

static SculptArray *sculpt_array_cache_create(Object *ob,
                                              eBrushArrayDeformType deform_type,
                                              const int num_copies)
{

  SculptArray *array = MEM_callocN(sizeof(SculptArray), "Sculpt Array");
  array->num_copies = num_copies;

  array->mode = deform_type;

  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char symm_it = 0; symm_it <= symm; symm_it++) {
    if (!SCULPT_is_symmetry_iteration_valid(symm_it, symm)) {
      continue;
    }
    array->copies[symm_it] = MEM_malloc_arrayN(
        num_copies, sizeof(SculptArrayCopy), "Sculpt array copies");
  }
  return array;
}

static void sculpt_array_cache_free(SculptArray *array)
{
  return;
  for (int symm_pass = 0; symm_pass < PAINT_SYMM_AREAS; symm_pass++) {
    MEM_SAFE_FREE(array->copies[symm_pass]);
  }
  MEM_freeN(array->copy_index);
  MEM_freeN(array->symmetry_pass);
  MEM_freeN(array);
}

static void sculpt_array_init(Object *ob, Brush *brush, SculptArray *array)
{
  SculptSession *ss = ob->sculpt;

  /* TODO: add options. */
  copy_v3_v3(array->normal, ss->cache->view_normal);
  array->radial_angle = 2.0f * M_PI;

  for (int symm_pass = 0; symm_pass < PAINT_SYMM_AREAS; symm_pass++) {
    if (array->copies[symm_pass] == NULL) {
      continue;
    }
    for (int copy_index = 0; copy_index < array->num_copies; copy_index++) {
      SculptArrayCopy *copy = &array->copies[symm_pass][copy_index];
      unit_m4(copy->mat);
      copy->symm_pass = symm_pass;
      copy->index = copy_index;
      float symm_location[3];
      flip_v3_v3(symm_location, ss->cache->location, symm_pass);
      copy_v3_v3(copy->origin, ss->cache->location);
    }
  }
}

static void sculpt_array_position_in_path_search(
    float *r_position, float *r_direction, float *r_scale, SculptArray *array, const int index)
{
  const float path_length = array->path.points[array->path.tot_points - 1].length;
  const float step_distance = path_length / (float)array->num_copies;
  const float copy_distance = step_distance * (index + 1);

  if (array->path.tot_points == 1) {
    zero_v3(r_position);
    if (r_direction) {
      zero_v3(r_direction);
    }
    if (r_scale) {
      *r_scale = 1.0f;
    }
    return;
  }

  for (int i = 1; i < array->path.tot_points; i++) {
    ScultpArrayPathPoint *path_point = &array->path.points[i];
    if (copy_distance >= path_point->length) {
      continue;
    }
    ScultpArrayPathPoint *prev_path_point = &array->path.points[i - 1];

    const float remaining_dist = copy_distance - prev_path_point->length;
    const float segment_length = path_point->length - prev_path_point->length;
    const float interp_factor = remaining_dist / segment_length;
    interp_v3_v3v3(r_position, prev_path_point->co, path_point->co, interp_factor);
    if (r_direction) {
      if (i == array->path.tot_points - 1) {
        copy_v3_v3(r_direction, prev_path_point->direction);
      }
      else {
        copy_v3_v3(r_direction, path_point->direction);
      }
    }
    if (r_scale) {
      const float s = 1.0f - interp_factor;
      *r_scale = s * prev_path_point->strength + interp_factor * path_point->strength;
    }
    return;
  }

  ScultpArrayPathPoint *last_path_point = &array->path.points[array->path.tot_points - 1];
  copy_v3_v3(r_position, last_path_point->co);
  if (r_direction) {
    ScultpArrayPathPoint *prev_path_point = &array->path.points[array->path.tot_points - 2];
    copy_v3_v3(r_direction, prev_path_point->direction);
  }
  if (r_scale) {
    ScultpArrayPathPoint *prev_path_point = &array->path.points[array->path.tot_points - 2];
    *r_scale = prev_path_point->strength;
  }
}

static void scultp_array_basis_from_direction(float r_mat[4][4],
                                              SculptArray *array,
                                              const float direction[3])
{
  float direction_normalized[3];
  normalize_v3_v3(direction_normalized, direction);
  copy_v3_v3(r_mat[0], direction_normalized);
  cross_v3_v3v3(r_mat[2], r_mat[0], array->normal);
  cross_v3_v3v3(r_mat[1], r_mat[0], r_mat[2]);
  normalize_v3(r_mat[0]);
  normalize_v3(r_mat[1]);
  normalize_v3(r_mat[2]);
}

static float *sculpt_array_delta_from_path(SculptArray *array)
{
  return array->path.points[array->path.tot_points - 1].co;
}

static void sculpt_array_update_copy(StrokeCache *cache,
                                     SculptArray *array,
                                     SculptArrayCopy *copy,
                                     Brush *brush)
{

  float copy_position[3];
  unit_m4(copy->mat);

  float scale = 1.0f;
  float direction[3];

  eBrushArrayDeformType array_type = brush->array_deform_type;
  float delta[3];
  copy_v3_v3(delta, sculpt_array_delta_from_path(array));

  switch (array_type) {
    case BRUSH_ARRAY_DEFORM_LINEAR: {
      const float fade = ((float)copy->index + 1.0f) / (float)(array->num_copies);
      mul_v3_v3fl(copy->mat[3], delta, fade);
      normalize_v3_v3(direction, delta);
      scale = cache->bstrength;
    } break;

    case BRUSH_ARRAY_DEFORM_RADIAL: {
      float pos[3];
      const float fade = ((float)copy->index + 1.0f) / (float)(array->num_copies);
      copy_v3_v3(pos, delta);
      rotate_v3_v3v3fl(copy->mat[3], pos, array->normal, fade * array->radial_angle);
      copy_v3_v3(direction, copy->mat[3]);
      // sub_v3_v3v3(direction, copy->mat[3], array->source_origin);
      scale = cache->bstrength;
    } break;
    case BRUSH_ARRAY_DEFORM_PATH:
      sculpt_array_position_in_path_search(copy->mat[3], direction, &scale, array, copy->index);
      break;
  }

  if (!(brush->flag2 & BRUSH_ARRAY_LOCK_ORIENTATION)) {
    scultp_array_basis_from_direction(copy->mat, array, direction);
  }

  /*
  copy->mat[3][0] += (BLI_hash_int_01(copy->index) * 2.0f - 0.5f) * cache->radius;
  copy->mat[3][1] += (BLI_hash_int_01(copy->index + 1) * 2.0f - 0.5f) * cache->radius;
  copy->mat[3][2] += (BLI_hash_int_01(copy->index + 2) * 2.0f - 0.5f) * cache->radius;
  */

  mul_v3_fl(copy->mat[0], scale);
  mul_v3_fl(copy->mat[1], scale);
  mul_v3_fl(copy->mat[2], scale);

  /*
  copy->mat[0][0] = scale;
  copy->mat[1][1] = scale;
  copy->mat[2][2] = scale;
  */
}

static void sculpt_array_update(Object *ob, Brush *brush, SculptArray *array)
{
  SculptSession *ss = ob->sculpt;

  /* Main symmetry pass. */
  for (int copy_index = 0; copy_index < array->num_copies; copy_index++) {
    SculptArrayCopy *copy = &array->copies[0][copy_index];
    unit_m4(copy->mat);
    sculpt_array_update_copy(ss->cache, array, copy, brush);
  }

  for (int symm_pass = 1; symm_pass < PAINT_SYMM_AREAS; symm_pass++) {
    if (array->copies[symm_pass] == NULL) {
      continue;
    }

    float symm_orig[3];
    flip_v3_v3(symm_orig, array->source_origin, symm_pass);

    for (int copy_index = 0; copy_index < array->num_copies; copy_index++) {
      SculptArrayCopy *copy = &array->copies[symm_pass][copy_index];
      SculptArrayCopy *main_copy = &array->copies[0][copy_index];
      unit_m4(copy->mat);
      for (int m = 0; m < 4; m++) {
        flip_v3_v3(copy->mat[m], main_copy->mat[m], symm_pass);
      }
    }
  }

  for (int symm_pass = 0; symm_pass < PAINT_SYMM_AREAS; symm_pass++) {

    if (array->copies[symm_pass] == NULL) {
      continue;
    }
    for (int copy_index = 0; copy_index < array->num_copies; copy_index++) {
      SculptArrayCopy *copy = &array->copies[symm_pass][copy_index];
      invert_m4_m4(copy->imat, copy->mat);
    }
  }
}

ATTR_NO_OPT static void do_array_deform_task_cb_ex(void *__restrict userdata,
                                                   const int n,
                                                   const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  SculptArray *array = ss->array;

  Mesh *mesh = BKE_object_get_original_mesh(data->ob);

  bool any_modified = false;

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    int array_index = ARRAY_INSTANCE_ORIGINAL;
    int array_symm_pass = 0;
    sculpt_vertex_array_data_get(array, vd.index, &array_index, &array_symm_pass);

    if (array_index == ARRAY_INSTANCE_ORIGINAL) {
      continue;
    }

    SculptArrayCopy *copy = &array->copies[array_symm_pass][array_index];

    float co[3];
    copy_v3_v3(co, array->orco[vd.index]);
    mul_v3_m4v3(co, array->source_imat, array->orco[vd.index]);
    mul_v3_m4v3(co, copy->mat, co);
    float source_origin_symm[3];
    flip_v3_v3(source_origin_symm, array->source_origin, array_symm_pass);
    add_v3_v3v3(vd.co, co, source_origin_symm);

    any_modified = true;

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

  if (any_modified) {
    BKE_pbvh_node_mark_update(data->nodes[n]);
  }
}

static void sculpt_array_deform(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  /* Threaded loop over nodes. */
  SculptSession *ss = ob->sculpt;
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_array_deform_task_cb_ex, &settings);
}

static void do_array_smooth_task_cb_ex(void *__restrict userdata,
                                       const int n,
                                       const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  SculptArray *array = ss->array;

  Mesh *mesh = BKE_object_get_original_mesh(data->ob);

  bool any_modified = false;

  bool check_fsets = ss->cache->brush->flag2 & BRUSH_SMOOTH_PRESERVE_FACE_SETS;
  const bool weighted = (ss->cache->brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    int array_index = ARRAY_INSTANCE_ORIGINAL;
    int array_symm_pass = 0;
    sculpt_vertex_array_data_get(array, vd.index, &array_index, &array_symm_pass);

    const float fade = array->smooth_strength[vd.index];

    if (fade == 0.0f) {
      continue;
    }

    float smooth_co[3];
    SCULPT_neighbor_coords_average(
        ss, smooth_co, vd.vertex, ss->cache->brush->autosmooth_projection, check_fsets, weighted);
    float disp[3];
    sub_v3_v3v3(disp, smooth_co, vd.co);
    mul_v3_fl(disp, fade);
    add_v3_v3(vd.co, disp);

    /*
        if (array_index == ARRAY_INSTANCE_ORIGINAL) {
          continue;
        }

        bool do_smooth = false;
        SculptVertexNeighborIter ni;
        SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.index, ni) {
          int neighbor_array_index = ARRAY_INSTANCE_ORIGINAL;
          int neighbor_symm_pass = 0;
          sculpt_vertex_array_data_get(array, ni.index, &neighbor_array_index,&neighbor_symm_pass);
          if (neighbor_array_index != array_index) {
            do_smooth = true;
          }
        }
        SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

        if (!do_smooth) {
          continue;
        }
        */

    any_modified = true;

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

  if (any_modified) {
    BKE_pbvh_node_mark_update(data->nodes[n]);
  }
}

static void sculpt_array_smooth(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{

  /* Threaded loop over nodes. */
  SculptSession *ss = ob->sculpt;
  SculptArray *array = ss->array;

  if (!array) {
    return;
  }

  if (!array->smooth_strength) {
    return;
  }
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_array_smooth_task_cb_ex, &settings);
}

static void sculpt_array_ensure_original_coordinates(Object *ob, SculptArray *array)
{
  SculptSession *ss = ob->sculpt;
  Mesh *sculpt_mesh = BKE_object_get_original_mesh(ob);
  const int totvert = SCULPT_vertex_count_get(ss);

  if (array->orco) {
    return;
  }

  array->orco = MEM_malloc_arrayN(totvert, sizeof(float) * 3, "array orco");

  for (int i = 0; i < totvert; i++) {
    SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

    copy_v3_v3(array->orco[i], SCULPT_vertex_co_get(ss, vertex));
  }
}

static void sculpt_array_ensure_base_transform(Sculpt *sd, Object *ob, SculptArray *array)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  Mesh *sculpt_mesh = BKE_object_get_original_mesh(ob);
  const int totvert = SCULPT_vertex_count_get(ss);

  if (array->source_mat_valid) {
    return;
  }

  unit_m4(array->source_mat);

  if (brush->flag2 & BRUSH_ARRAY_LOCK_ORIENTATION) {
    unit_m4(array->source_mat);
    copy_v3_v3(array->source_mat[3], array->source_origin);
    invert_m4_m4(array->source_imat, array->source_mat);
    array->source_mat_valid = true;
    return;
  }

  if (is_zero_v3(ss->cache->grab_delta)) {
    return;
  }

  scultp_array_basis_from_direction(array->source_mat, array, ss->cache->grab_delta);
  copy_v3_v3(array->source_mat[3], array->source_origin);
  invert_m4_m4(array->source_imat, array->source_mat);

  array->source_mat_valid = true;
  return;
}

static void sculpt_array_path_point_update(SculptArray *array, const int path_point_index)
{
  if (path_point_index == 0) {
    return;
  }

  const int prev_path_point_index = path_point_index - 1;

  ScultpArrayPathPoint *path_point = &array->path.points[path_point_index];
  ScultpArrayPathPoint *prev_path_point = &array->path.points[prev_path_point_index];

  if (len_v3v3(prev_path_point->co, path_point->co) <= 0.0001f) {
    return;
  }
  sub_v3_v3v3(prev_path_point->direction, path_point->co, prev_path_point->co);
  path_point->length = prev_path_point->length + normalize_v3(prev_path_point->direction);
}

static void sculpt_array_stroke_sample_add(Object *ob, SculptArray *array)
{
  SculptSession *ss = ob->sculpt;

  if (!array->path.points) {
    array->path.points = MEM_malloc_arrayN(9999, sizeof(ScultpArrayPathPoint), "Array Path");
  }

  const int current_point_index = array->path.tot_points;
  const int prev_point_index = current_point_index - 1;

  ScultpArrayPathPoint *path_point = &array->path.points[current_point_index];

  // add_v3_v3v3(path_point->co, ss->cache->orig_grab_location, ss->cache->grab_delta);
  copy_v3_v3(path_point->co, ss->cache->grab_delta);
  path_point->strength = ss->cache->bstrength;

  if (current_point_index == 0) {
    /* First point of the path. */
    path_point->length = 0.0f;
  }
  else {
    ScultpArrayPathPoint *prev_path_point = &array->path.points[prev_point_index];
    if (len_v3v3(prev_path_point->co, path_point->co) <= 0.0001f) {
      return;
    }
    sub_v3_v3v3(prev_path_point->direction, path_point->co, prev_path_point->co);
    path_point->length = prev_path_point->length + normalize_v3(prev_path_point->direction);
  }

  array->path.tot_points++;
}

ATTR_NO_OPT void SCULPT_do_array_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (ss->cache->invert) {
    if (!ss->array) {
      return;
    }

    if (SCULPT_stroke_is_first_brush_step(ss->cache)) {
      SculptArray *array = ss->array;
      const int totvert = SCULPT_vertex_count_get(ss);

      /* Rebuild smooth strength cache. */
      MEM_SAFE_FREE(array->smooth_strength);
      array->smooth_strength = MEM_calloc_arrayN(sizeof(float), totvert, "smooth_strength");

      for (int i = 0; i < totvert; i++) {
        SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

        int array_index = ARRAY_INSTANCE_ORIGINAL;
        int array_symm_pass = 0;
        sculpt_vertex_array_data_get(array, i, &array_index, &array_symm_pass);

        if (array_index == ARRAY_INSTANCE_ORIGINAL) {
          continue;
        }

        /* TODO: this can be cached. */
        SculptVertexNeighborIter ni;
        SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
          int neighbor_array_index = ARRAY_INSTANCE_ORIGINAL;
          int neighbor_symm_pass = 0;
          sculpt_vertex_array_data_get(
              array, ni.index, &neighbor_array_index, &neighbor_symm_pass);
          if (neighbor_array_index != array_index) {
            array->smooth_strength[i] = 1.0f;
            break;
          }
        }
        SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
      }

      for (int smooth_iterations = 0; smooth_iterations < 4; smooth_iterations++) {
        for (int i = 0; i < totvert; i++) {
          SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

          float avg = array->smooth_strength[i];
          int count = 1;
          SculptVertexNeighborIter ni;
          SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
            avg += array->smooth_strength[ni.index];
            count++;
          }
          SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
          array->smooth_strength[i] = avg / count;
        }
      }

      /* Update Array Path Orco. */
      for (int i = 0; i < array->path.tot_points; i++) {
        ScultpArrayPathPoint *point = &array->path.points[i];
        copy_v3_v3(point->orco, point->co);
      }
      array->initial_radial_angle = array->radial_angle;

      /* Update Geometry Orco. */
      for (int i = 0; i < totvert; i++) {
        SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

        int array_index = ARRAY_INSTANCE_ORIGINAL;
        int array_symm_pass = 0;
        sculpt_vertex_array_data_get(array, i, &array_index, &array_symm_pass);

        if (array_index == ARRAY_INSTANCE_ORIGINAL) {
          continue;
        }
        SculptArrayCopy *copy = &array->copies[array_symm_pass][array_index];
        // sub_v3_v3v3(array->orco[i], SCULPT_vertex_co_get(ss, i), copy->mat[3]);
        float co[3];
        float source_origin_symm[3];
        copy_v3_v3(co, SCULPT_vertex_co_get(ss, vertex));
        flip_v3_v3(source_origin_symm, array->source_origin, array_symm_pass);
        mul_v3_m4v3(co, copy->imat, co);
        mul_v3_m4v3(co, array->source_imat, co);
        // sub_v3_v3v3(co, co, source_origin_symm);

        copy_v3_v3(array->orco[i], co);
      }
    }

    SculptArray *array = ss->array;
    if (array->mode == BRUSH_ARRAY_DEFORM_PATH) {
      /* Deform path */
      for (int i = 0; i < array->path.tot_points; i++) {
        ScultpArrayPathPoint *point = &array->path.points[i];
        float point_co[3];
        add_v3_v3v3(point_co, point->orco, array->source_origin);
        const float len = len_v3v3(ss->cache->true_location, point_co);
        const float fade = ss->cache->bstrength *
                           BKE_brush_curve_strength(brush, len, ss->cache->radius);
        if (fade <= 0.0f) {
          continue;
        }
        madd_v3_v3v3fl(point->co, point->orco, ss->cache->grab_delta, fade);
      }
      for (int i = 0; i < array->path.tot_points; i++) {
        sculpt_array_path_point_update(array, i);
      }
    }
    else {
      /* Tweak radial angle. */
      /*
      const float factor = 1.0f - ( len_v3(ss->cache->grab_delta) / ss->cache->initial_radius);
      array->radial_angle = array->initial_radial_angle * clamp_f(factor, 0.0f, 1.0f);
      */

      float array_disp_co[3];
      float brush_co[3];
      add_v3_v3v3(brush_co, ss->cache->initial_location, ss->cache->grab_delta);
      sub_v3_v3(brush_co, array->source_origin);
      normalize_v3(brush_co);
      normalize_v3_v3(array_disp_co, sculpt_array_delta_from_path(array));
      array->radial_angle = angle_signed_on_axis_v3v3_v3(brush_co, array_disp_co, array->normal);
    }

    sculpt_array_update(ob, brush, ss->array);
    sculpt_array_deform(sd, ob, nodes, totnode);
    for (int i = 0; i < 5; i++) {
      sculpt_array_smooth(sd, ob, nodes, totnode);
    }

    return;
  }

  if (brush->array_count == 0) {
    return;
  }

  if (!SCULPT_stroke_is_main_symmetry_pass(ss->cache)) {
    /* This brush manages its own symmetry. */
    return;
  }

  if (SCULPT_stroke_is_first_brush_step(ss->cache)) {
    if (ss->array) {
      sculpt_array_cache_free(ss->array);
    }

    ss->array = sculpt_array_cache_create(ob, brush->array_deform_type, brush->array_count);
    sculpt_array_init(ob, brush, ss->array);
    sculpt_array_stroke_sample_add(ob, ss->array);
    sculpt_array_mesh_build(sd, ob, ss->array);
    /* Original coordinates can't be stored yet as the SculptSession data needs to be updated after
     * the mesh modifications performed when building the array geometry. */
    return;
  }

  SCULPT_vertex_random_access_ensure(ss);

  sculpt_array_ensure_base_transform(sd, ob, ss->array);
  sculpt_array_ensure_original_coordinates(ob, ss->array);
  sculpt_array_ensure_geometry_indices(ob, ss->array);

  sculpt_array_stroke_sample_add(ob, ss->array);

  sculpt_array_update(ob, brush, ss->array);

  sculpt_array_deform(sd, ob, nodes, totnode);
}

void SCULPT_array_path_draw(const uint gpuattr, Brush *brush, SculptSession *ss)
{

  SculptArray *array = ss->array;

  /* Disable debug drawing. */
  return;

  if (!array) {
    return;
  }

  if (!array->path.points) {
    return;
  }

  if (array->path.tot_points < 2) {
    return;
  }

  const int tot_points = array->path.tot_points;
  immBegin(GPU_PRIM_LINE_STRIP, tot_points);
  for (int i = 0; i < tot_points; i++) {
    float co[3];
    copy_v3_v3(co, array->path.points[i].co);
    add_v3_v3(co, array->source_origin);
    immVertex3fv(gpuattr, co);
  }
  immEnd();
}
