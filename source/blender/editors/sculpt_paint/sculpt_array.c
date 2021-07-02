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

#define SCUPT_ARRAY_COUNT 5
#define ARRAY_INSTANCE_ORIGINAL -1 

static void sculpt_array_modify_sculpt_mesh(Object *ob, Mesh *array_mesh)
{
  SculptSession *ss = ob->sculpt;	
  Mesh *sculpt_mesh = BKE_object_get_original_mesh(ob);
  BMesh *bm;
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(sculpt_mesh, array_mesh);
  bm = BM_mesh_create(&allocsize,
                      &((struct BMeshCreateParams){
                          .use_toolflags = false,
                      }));

  BM_mesh_bm_from_me(bm,
                     sculpt_mesh,
                     &((struct BMeshFromMeshParams){
                         .calc_face_normal = true,
                     }));

  BM_mesh_bm_from_me(bm,
                     array_mesh,
                     &((struct BMeshFromMeshParams){
                         .calc_face_normal = true,
                     }));


  Mesh *result = BKE_mesh_from_bmesh_for_eval_nomain(bm, NULL, sculpt_mesh);
  BM_mesh_free(bm);
  result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
  BKE_mesh_nomain_to_mesh(result, ob->data, ob, &CD_MASK_MESH, true);
  BKE_mesh_free(result);
  BKE_mesh_batch_cache_dirty_tag(ob->data, BKE_MESH_BATCH_DIRTY_ALL);
  ss->needs_pbvh_rebuild = true;
}


const float source_geometry_threshold = 0.5f;
static BMesh *sculpt_array_source_mesh_calculate(Sculpt *sd, Object *ob) {
  SculptSession *ss = ob->sculpt;
  Mesh *sculpt_mesh = BKE_object_get_original_mesh(ob);
  const int totvert = SCULPT_vertex_count_get(ss);

  int *v_array_instance = CustomData_add_layer_named(&sculpt_mesh->vdata,
                                                    CD_PROP_INT32,
                                                    CD_CALLOC,
                                                    NULL,
                                                    sculpt_mesh->totvert,
                                                    array_instance_cd_name);

  for (int i = 0; i < sculpt_mesh->totvert; i++) {
    v_array_instance[i] = ARRAY_INSTANCE_ORIGINAL;
  }

  printf("ARRAY INSTANCE SET\n");


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

  /* TODO: Handle individual Face Sets for Face Set automasking. */
  BM_mesh_delete_hflag_context(srcbm, BM_ELEM_TAG, DEL_VERTS);
  
  
  /*
  BMO_op_callf(srcbm,
               (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
               "duplicate geom=%hvef use_select_history=%b use_edge_flip_from_face=%b",
               BM_ELEM_TAG, false, false);
	       */
	       


  //CustomData_bmesh_merge(&srcbm->vdata, &destbm->vdata, CD_NA, 0, destbm, BM_VERT).

  
  
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

/*
  BM_mesh_elem_table_ensure(srcbm, BM_VERT);
  BM_mesh_elem_index_ensure(srcbm, BM_VERT); 
  for (int i = 0; i < srcbm->totvert; i++) {
	BMVert *vert = BM_vert_at_index(srcbm, i);
	BM_elem_flag_set(vert, BM_ELEM_TAG, true);
	BM_elem_flag_set(vert, BM_ELEM_SELECT, true);
  }
  */

/*
  CustomData_bmesh_merge(&srcbm->vdata, &destbm->vdata, &CD_MASK_BMESH, 0, destbm, BM_VERT);
  CustomData_bmesh_merge(&srcbm->edata, &destbm->edata, &CD_MASK_BMESH, 0, destbm, BM_EDGE);
  CustomData_bmesh_merge(&srcbm->pdata, &destbm->pdata, &CD_MASK_BMESH, 0, destbm, BM_FACE);
  CustomData_bmesh_merge(&srcbm->ldata, &destbm->ldata, &CD_MASK_BMESH, 0, destbm, BM_LOOP);
  */


  printf("GET LAYER INDEX\n");

  const int cd_array_instance_index = CustomData_get_named_layer_index(
      &srcbm->vdata, CD_PROP_INT32, array_instance_cd_name);

  printf("DONE\n");

  if (cd_array_instance_index == -1) {
      printf("NO DATALAYER INDEX\n");
  }

  printf("GET LAYER OFFSET\n");

  const int cd_array_instance_offset = CustomData_get_n_offset(
      &srcbm->vdata, CD_PROP_INT32, cd_array_instance_index);

  printf("DONE\n");

  if (cd_array_instance_offset == -1) {
      printf("NO DATALAYER offset\n");
  }


  BM_mesh_elem_toolflags_ensure(destbm);

  BM_mesh_elem_table_ensure(srcbm, BM_VERT);
  BM_mesh_elem_index_ensure(srcbm, BM_VERT); 

  for (int i = 0; i < SCUPT_ARRAY_COUNT; i++) {
  	for (int j = 0; j < srcbm->totvert; j++) {
		BMVert *vert = BM_vert_at_index(srcbm, j);
		printf("SET INDEX %d\n", i);
		BM_ELEM_CD_SET_INT(vert, cd_array_instance_offset, i);
  	}
  	const int opflag = (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE);
        //CustomData_bmesh_merge(&srcbm->vdata, &destbm->vdata, &CD_MASK_BMESH.vmask, 0, destbm, BM_VERT);
        BM_mesh_copy_init_customdata(destbm, srcbm, &bm_mesh_allocsize_default);
  	BMO_op_callf(srcbm, opflag, "duplicate geom=%avef dest=%p", destbm);
  }



  Mesh *result = BKE_mesh_from_bmesh_for_eval_nomain(destbm, NULL, sculpt_mesh);
  result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
  BKE_mesh_nomain_to_mesh(result, ob->data, ob, &CD_MASK_MESH, true);


  BKE_mesh_free(result);
  BKE_mesh_batch_cache_dirty_tag(ob->data, BKE_MESH_BATCH_DIRTY_ALL);
  ss->needs_pbvh_rebuild = true;

  return srcbm;
}

void SCULPT_do_array_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);


  if (SCULPT_stroke_is_first_brush_step(ss->cache)) {
	/* Calculate source array mesh. */
	sculpt_array_source_mesh_calculate(sd, ob);
	
	/* Insert copies and prepare datalayers. */

        Mesh *sculpt_mesh = BKE_object_get_original_mesh(ob);
	int *cd_array_instance = CustomData_get_layer_named(
      &sculpt_mesh->vdata, CD_PROP_INT32, array_instance_cd_name);

	if (!cd_array_instance) {
		printf("NO CD LAYER\n");
		return;
	}

	const int totvert = SCULPT_vertex_count_get(ss);

	for (int i = 0; i < totvert; i++) {
		const int array_index = cd_array_instance[i];
		//printf("array index %d\n", array_index);
	}
	return;
  }
   

    Mesh *sculpt_mesh = BKE_object_get_original_mesh(ob);
    int *cd_array_instance = CustomData_get_layer_named(
    &sculpt_mesh->vdata, CD_PROP_INT32, array_instance_cd_name);
   const int totvert = SCULPT_vertex_count_get(ss);

   if (!ss->cache->array_orco) {
  ss->cache->array_orco = MEM_calloc_arrayN(sculpt_mesh->totvert, sizeof(float) * 3, "array orco");
  for (int i = 0; i < sculpt_mesh->totvert; i++) {
	copy_v3_v3(ss->cache->array_orco[i], sculpt_mesh->mvert[i].co);
  }


   }

   for (int i = 0; i < totvert; i++) {
   	const int array_index = cd_array_instance[i];
	const float fade = ((float)array_index + 1.0f) / (float)(SCUPT_ARRAY_COUNT);
	madd_v3_v3v3fl(ss->mvert[i].co, ss->cache->array_orco[i], ss->cache->grab_delta_symmetry, fade);
   }
 
  for (int i = 0; i < totnode; i++) {
    BKE_pbvh_node_mark_update(nodes[i]);
  }

}