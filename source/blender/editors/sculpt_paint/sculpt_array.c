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

#include "bmesh.h"

#include <math.h>
#include <stdlib.h>


static const char array_symmetry_pass_cd_name[] = "v_symmetry_pass";
static const char array_instance_cd_name[] = "v_array_instance";

#define SCULPT_ARRAY_COUNT 5
#define ARRAY_INSTANCE_ORIGINAL -1 


static void sculpt_array_datalayers_add(Mesh *mesh) {
  int *v_array_instance = CustomData_add_layer_named(&mesh->vdata,
                                                    CD_PROP_INT32,
                                                    CD_CALLOC,
                                                    NULL,
                                                    mesh->totvert,
                                                    array_instance_cd_name);
  for (int i = 0; i < mesh->totvert; i++) {
    v_array_instance[i] = ARRAY_INSTANCE_ORIGINAL;
  }

  CustomData_add_layer_named(&mesh->vdata,
                                                    CD_PROP_INT32,
                                                    CD_CALLOC,
                                                    NULL,
                                                    mesh->totvert,
                                                    array_symmetry_pass_cd_name);
}

void SCULPT_array_datalayers_free(Object *ob) {
  Mesh *mesh = BKE_object_get_original_mesh(ob);
  int v_layer_index = CustomData_get_named_layer_index(&mesh->vdata, CD_PROP_INT32, array_instance_cd_name);
  if (v_layer_index != -1) {
    CustomData_free_layer(&mesh->vdata, CD_PROP_INT32, mesh->totvert, v_layer_index);
  }

  v_layer_index = CustomData_get_named_layer_index(&mesh->vdata, CD_PROP_INT32, array_symmetry_pass_cd_name);
  if (v_layer_index != -1) {
    CustomData_free_layer(&mesh->vdata, CD_PROP_INT32, mesh->totvert, v_layer_index);
  }
}

const float source_geometry_threshold = 0.5f;

static BMesh *sculpt_array_source_build(Object *ob, Brush *brush) {
  Mesh *sculpt_mesh = BKE_object_get_original_mesh(ob);

  BMesh *srcbm;
  const BMAllocTemplate allocsizea = BMALLOC_TEMPLATE_FROM_ME(sculpt_mesh);
  srcbm = BM_mesh_create(&allocsizea,
                      &((struct BMeshCreateParams){
                          .use_toolflags = true,
                      }));

  BM_mesh_bm_from_me(srcbm,
                     sculpt_mesh,
                     &((struct BMeshFromMeshParams){
                         .calc_face_normal = true,
                     }));

  BM_mesh_elem_table_ensure(srcbm, BM_VERT);
  BM_mesh_elem_index_ensure(srcbm, BM_VERT); 

  SculptSession *ss = ob->sculpt;
  for (int i = 0; i < srcbm->totvert; i++) {
	const float automask = SCULPT_automasking_factor_get(ss->cache->automasking, ss, i);
	const float mask = 1.0f - SCULPT_vertex_mask_get(ss, i);
	const float influence = mask * automask;
	if (influence >= source_geometry_threshold) {
		continue;
	}
	BMVert *vert = BM_vert_at_index(srcbm, i);
	BM_elem_flag_set(vert, BM_ELEM_TAG, true);
  }
  
  /* TODO(pablodp606): Handle individual Face Sets for Face Set automasking. */
  BM_mesh_delete_hflag_context(srcbm, BM_ELEM_TAG, DEL_VERTS);

  return srcbm;
}

void sculpt_array_source_datalayer_update(BMesh *bm, const int symm_pass, const int copy_index) {
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

static void sculpt_array_final_mesh_write(Object *ob, BMesh *final_mesh) {
  SculptSession *ss = ob->sculpt;
  Mesh *sculpt_mesh = BKE_object_get_original_mesh(ob);
  Mesh *result = BKE_mesh_from_bmesh_for_eval_nomain(final_mesh, NULL, sculpt_mesh);
  result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
  BKE_mesh_nomain_to_mesh(result, ob->data, ob, &CD_MASK_MESH, true);
  BKE_mesh_free(result);
  BKE_mesh_batch_cache_dirty_tag(ob->data, BKE_MESH_BATCH_DIRTY_ALL);
  ss->needs_pbvh_rebuild = true;
}

static void sculpt_array_mesh_build(Sculpt *sd, Object *ob) {
  Mesh *sculpt_mesh = BKE_object_get_original_mesh(ob);
  sculpt_array_datalayers_add(sculpt_mesh);

  BMesh *srcbm = sculpt_array_source_build(ob, NULL);
  
  BMesh *destbm;
  const BMAllocTemplate allocsizeb = BMALLOC_TEMPLATE_FROM_ME(sculpt_mesh);
  destbm = BM_mesh_create(&allocsizeb,
                      &((struct BMeshCreateParams){
                          .use_toolflags = true,
                      }));
  BM_mesh_bm_from_me(destbm,
                     sculpt_mesh,
                     &((struct BMeshFromMeshParams){
                         .calc_face_normal = true,
                     }));


  BM_mesh_elem_toolflags_ensure(destbm);
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char symm_it = 0; symm_it <= symm; symm_it++) {
    if (!SCULPT_is_symmetry_iteration_valid(symm_it, symm)) {
      continue;
    }

  for (int copy_index = 0; copy_index < SCULPT_ARRAY_COUNT; copy_index++) {
    sculpt_array_source_datalayer_update(srcbm, symm_it, copy_index);

    /* TODO: ya tal */
    BM_mesh_copy_init_customdata(destbm, srcbm, &bm_mesh_allocsize_default);
  	const int opflag = (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE);
  	BMO_op_callf(srcbm, opflag, "duplicate geom=%avef dest=%p", destbm);
  }
  }

  sculpt_array_final_mesh_write(ob, destbm);
  BM_mesh_free(srcbm);
  BM_mesh_free(destbm);
}

static SculptArray *sculpt_array_cache_create(Object *ob, const int num_copies) {

  SculptArray *array = MEM_callocN(sizeof(SculptArray), "Sculpt Array");
  array->num_copies = num_copies;

  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char symm_it = 0; symm_it <= symm; symm_it++) {
    if (!SCULPT_is_symmetry_iteration_valid(symm_it, symm)) {
      continue;
    }
    array->copies[symm_it] = MEM_malloc_arrayN(num_copies, sizeof(SculptArrayCopy), "Sculpt array copies");
  }
  return array;
}

static void sculpt_array_cache_free(SculptArray *array) {
  for (int symm_pass = 0; symm_pass < PAINT_SYMM_AREAS; symm_pass++) {
      MEM_SAFE_FREE(array->copies[symm_pass]);
  }
  MEM_freeN(array);
}

static void sculpt_array_init(Object *ob, SculptArray *array) {
  SculptSession *ss = ob->sculpt;
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
        copy_v3_v3(copy->origin, symm_location);
      }
  }
}

static void sculpt_array_update_copy(StrokeCache *cache, SculptArray *array, SculptArrayCopy *copy) {
  const float fade = ((float)copy->index + 1.0f) / (float)(array->num_copies);
  float delta[3];
  flip_v3_v3(delta, cache->grab_delta, copy->symm_pass);
  mul_v3_v3fl(copy->mat[3], delta, fade);


  const float scale = cache->bstrength;
  copy->mat[0][0] = scale;
  copy->mat[1][1] = scale;
  copy->mat[2][2] = scale;

}

static void sculpt_array_update(Object *ob, Brush *brush, SculptArray *array) {
  SculptSession *ss = ob->sculpt;

  for (int symm_pass = 0; symm_pass < PAINT_SYMM_AREAS; symm_pass++) {
      if (array->copies[symm_pass] == NULL) {
        continue;
      }
      for (int copy_index = 0; copy_index < array->num_copies; copy_index++) {
        SculptArrayCopy *copy = &array->copies[symm_pass][copy_index];
        sculpt_array_update_copy(ss->cache, array, copy);
      }
  }
}

static void do_array_deform_task_cb_ex(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  SculptArray *array = ss->cache->array;

    Mesh *mesh = BKE_object_get_original_mesh(data->ob);

    int *cd_array_instance = CustomData_get_layer_named(
    &mesh->vdata, CD_PROP_INT32, array_instance_cd_name);

    int *cd_array_symm_pass = CustomData_get_layer_named(
    &mesh->vdata, CD_PROP_INT32, array_symmetry_pass_cd_name);

    bool any_modified = false;

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
   	const int array_index = cd_array_instance[vd.index];
    if (array_index == -1) {
      continue;
    }
   	const int array_symm_pass = cd_array_symm_pass[vd.index];
    if (array_symm_pass != ss->cache->mirror_symmetry_pass) {
      continue;
    }


    SculptArrayCopy *copy = &array->copies[array_symm_pass][array_index];
    mul_v3_m4v3(vd.co, copy->mat, array->orco[vd.index]);

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

static void sculpt_array_deform(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode) {
    SculptSession *ss = ob->sculpt;
  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(
      0, totnode, &data, do_array_deform_task_cb_ex, &settings);
}

static void sculpt_array_ensure_original_coordinates(Object *ob, SculptArray *array){
  SculptSession *ss = ob->sculpt;
  Mesh *sculpt_mesh = BKE_object_get_original_mesh(ob);
  const int totvert = SCULPT_vertex_count_get(ss);

   if (array->orco) {
     return;
   }

  array->orco = MEM_malloc_arrayN(sculpt_mesh->totvert, sizeof(float) * 3, "array orco");
  for (int i = 0; i < totvert; i++) {
	  copy_v3_v3(array->orco[i], SCULPT_vertex_co_get(ss, i));
  }
}


static void sculpt_array_stroke_sample_add(Object *ob, SculptArray *array) {
  SculptSession *ss = ob->sculpt;

  if (!array->path.points) {
    array->path.points = MEM_malloc_arrayN(9999, sizeof(ScultpArrayPathPoint), "Array Path");
  }

  const int current_point_index = array->path.tot_points;
  const int prev_point_index = current_point_index - 1;
  
  ScultpArrayPathPoint *path_point = &array->path.points[current_point_index];
  add_v3_v3v3(path_point->co, ss->cache->true_initial_location, ss->cache->grab_delta);
  if (current_point_index == 0) {
    /* First point of the path. */
    path_point->length = 0.0f;
  }
  else {
    ScultpArrayPathPoint *prev_path_point = &array->path.points[prev_point_index];
    sub_v3_v3v3(prev_path_point->direction, path_point->co, prev_path_point->co);
    path_point->length += normalize_v3(prev_path_point->direction);
  }

  array->path.tot_points++;
}

void SCULPT_do_array_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (!SCULPT_stroke_is_main_symmetry_pass(ss->cache)) {
      /* This brush manages its own symmetry. */
      return;
  }

  if (SCULPT_stroke_is_first_brush_step(ss->cache)) {
    sculpt_array_mesh_build(sd, ob);
    ss->cache->array = sculpt_array_cache_create(ob, SCULPT_ARRAY_COUNT);
    sculpt_array_init(ob, ss->cache->array);
    /* Original coordinates can't be stored yet as the SculptSession data needs to be updated after the mesh modifications performed when building the array geometry. */
	  return;
  }
   
   sculpt_array_ensure_original_coordinates(ob, ss->cache->array);
   sculpt_array_update(ob, brush, ss->cache->array);
   sculpt_array_deform(sd, ob, nodes, totnode);

}