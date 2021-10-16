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

#include "BLI_alloca.h"
#include "BLI_array.h"
#include "BLI_blenlib.h"
#include "BLI_compiler_attrs.h"
#include "BLI_hash.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_polyfill_2d.h"
#include "BLI_task.h"

#include "BLT_translation.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BKE_brush.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pbvh.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_undo.h"
#include "ED_view3d.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include <math.h>
#include <stdlib.h>

BMesh *SCULPT_dyntopo_empty_bmesh()
{
  const BMAllocTemplate allocsize = {
      .totvert = 2048 * 16, .totface = 2048 * 16, .totloop = 4196 * 16, .totedge = 2048 * 16};

  BMesh *bm = BM_mesh_create(
      &allocsize,
      &((struct BMeshCreateParams){.use_toolflags = false,
                                   .create_unique_ids = true,
                                   .id_elem_mask = BM_VERT | BM_EDGE | BM_FACE,
                                   .id_map = true,
                                   .temporary_ids = false,
                                   .no_reuse_ids = false}));

  return bm;
}
// TODO: check if (mathematically speaking) is it really necassary
// to sort the edge lists around verts

// from http://rodolphe-vaillant.fr/?e=20
static float tri_voronoi_area(float p[3], float q[3], float r[3])
{
  float pr[3];
  float pq[3];

  sub_v3_v3v3(pr, p, r);
  sub_v3_v3v3(pq, p, q);

  float angles[3];

  angle_tri_v3(angles, p, q, r);

  if (angles[0] > (float)M_PI * 0.5f) {
    return area_tri_v3(p, q, r) / 2.0f;
  }
  else if (angles[1] > (float)M_PI * 0.5f || angles[2] > (float)M_PI * 0.5f) {
    return area_tri_v3(p, q, r) / 4.0f;
  }
  else {

    float dpr = dot_v3v3(pr, pr);
    float dpq = dot_v3v3(pq, pq);

    float area = (1.0f / 8.0f) *
                 (dpr * cotangent_tri_weight_v3(q, p, r) + dpq * cotangent_tri_weight_v3(r, q, p));

    return area;
  }
}

static float cotangent_tri_weight_v3_proj(const float n[3],
                                          const float v1[3],
                                          const float v2[3],
                                          const float v3[3])
{
  float a[3], b[3], c[3], c_len;

  sub_v3_v3v3(a, v2, v1);
  sub_v3_v3v3(b, v3, v1);

  madd_v3_v3fl(a, n, -dot_v3v3(n, a));
  madd_v3_v3fl(b, n, -dot_v3v3(n, b));

  cross_v3_v3v3(c, a, b);

  c_len = len_v3(c);

  if (c_len > FLT_EPSILON) {
    return dot_v3v3(a, b) / c_len;
  }

  return 0.0f;
}

void SCULPT_dyntopo_get_cotangents(SculptSession *ss,
                                   SculptVertRef vertex,
                                   float *r_ws,
                                   float *r_cot1,
                                   float *r_cot2,
                                   float *r_area,
                                   float *r_totarea)
{
  SCULPT_dyntopo_check_disk_sort(ss, vertex);

  BMVert *v = (BMVert *)vertex.i;
  BMEdge *e = v->e;

  if (!e) {
    return;
  }

  int i = 0;
  float totarea = 0.0;
  float totw = 0.0;

  do {
    BMEdge *eprev = v == e->v1 ? e->v1_disk_link.prev : e->v2_disk_link.prev;
    BMEdge *enext = v == e->v1 ? e->v1_disk_link.next : e->v2_disk_link.next;

    BMVert *v1 = BM_edge_other_vert(eprev, v);
    BMVert *v2 = BM_edge_other_vert(e, v);
    BMVert *v3 = BM_edge_other_vert(enext, v);

    float cot1 = cotangent_tri_weight_v3(v1->co, v->co, v2->co);
    float cot2 = cotangent_tri_weight_v3(v3->co, v2->co, v->co);

    float area = tri_voronoi_area(v->co, v1->co, v2->co);

    r_ws[i] = (cot1 + cot2);
    totw += r_ws[i];

    totarea += area;

    if (r_cot1) {
      r_cot1[i] = cot1;
    }

    if (r_cot2) {
      r_cot2[i] = cot2;
    }

    if (r_area) {
      r_area[i] = area;
    }

    i++;
    e = enext;
  } while (e != v->e);

  if (r_totarea) {
    *r_totarea = totarea;
  }

  int count = i;

  float mul = 1.0f / (totarea * 2.0);

  for (i = 0; i < count; i++) {
    r_ws[i] *= mul;
  }
}

void SCULPT_faces_get_cotangents(SculptSession *ss,
                                 SculptVertRef vertex,
                                 float *r_ws,
                                 float *r_cot1,
                                 float *r_cot2,
                                 float *r_area,
                                 float *r_totarea)
{
  // sculpt vemap should always be sorted in disk cycle order

  float totarea = 0.0;
  float totw = 0.0;

  MeshElemMap *elem = ss->vemap + vertex.i;
  for (int i = 0; i < elem->count; i++) {
    int i1 = (i + elem->count - 1) % elem->count;
    int i2 = i;
    int i3 = (i + 1) % elem->count;

    MVert *v = ss->mvert + vertex.i;
    MEdge *e1 = ss->medge + elem->indices[i1];
    MEdge *e2 = ss->medge + elem->indices[i2];
    MEdge *e3 = ss->medge + elem->indices[i3];

    MVert *v1 = (unsigned int)vertex.i == e1->v1 ? ss->mvert + e1->v2 : ss->mvert + e1->v1;
    MVert *v2 = (unsigned int)vertex.i == e2->v1 ? ss->mvert + e2->v2 : ss->mvert + e2->v1;
    MVert *v3 = (unsigned int)vertex.i == e3->v1 ? ss->mvert + e3->v2 : ss->mvert + e3->v1;

    float cot1 = cotangent_tri_weight_v3(v1->co, v->co, v2->co);
    float cot2 = cotangent_tri_weight_v3(v3->co, v2->co, v->co);

    float area = tri_voronoi_area(v->co, v1->co, v2->co);

    r_ws[i] = (cot1 + cot2);
    totw += r_ws[i];

    totarea += area;

    if (r_cot1) {
      r_cot1[i] = cot1;
    }

    if (r_cot2) {
      r_cot2[i] = cot2;
    }

    if (r_area) {
      r_area[i] = area;
    }
  }

  if (r_totarea) {
    *r_totarea = totarea;
  }

  float mul = 1.0f / (totarea * 2.0);

  for (int i = 0; i < elem->count; i++) {
    r_ws[i] *= mul;
  }
}

void SCULPT_cotangents_begin(Object *ob, SculptSession *ss)
{
  SCULPT_vertex_random_access_ensure(ss);
  int totvert = SCULPT_vertex_count_get(ss);

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      for (int i = 0; i < totvert; i++) {
        SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);
        SCULPT_dyntopo_check_disk_sort(ss, vertex);
      }
      break;
    }
    case PBVH_FACES: {
      Mesh *mesh = BKE_object_get_original_mesh(ob);

      if (!ss->vemap) {
        BKE_mesh_vert_edge_map_create(&ss->vemap,
                                      &ss->vemap_mem,
                                      mesh->mvert,
                                      mesh->medge,
                                      mesh->totvert,
                                      mesh->totedge,
                                      true);
      }

      break;
    }
    case PBVH_GRIDS:  // not supported yet
      break;
  }
}

void SCULPT_get_cotangents(SculptSession *ss,
                           SculptVertRef vertex,
                           float *r_ws,
                           float *r_cot1,
                           float *r_cot2,
                           float *r_area,
                           float *r_totarea)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH:
      SCULPT_dyntopo_get_cotangents(ss, vertex, r_ws, r_cot1, r_cot2, r_area, r_totarea);
      break;
    case PBVH_FACES:
      SCULPT_faces_get_cotangents(ss, vertex, r_ws, r_cot1, r_cot2, r_area, r_totarea);
      break;
    case PBVH_GRIDS: {
      {
        // not supported, return uniform weights;

        int val = SCULPT_vertex_valence_get(ss, vertex);

        for (int i = 0; i < val; i++) {
          r_ws[i] = 1.0f;
        }
      }
      break;
    }
  }
}

void SCULT_dyntopo_flag_all_disk_sort(SculptSession *ss)
{
  BKE_pbvh_bmesh_flag_all_disk_sort(ss->pbvh);
}

// returns true if edge disk list around vertex was sorted
bool SCULPT_dyntopo_check_disk_sort(SculptSession *ss, SculptVertRef vertex)
{
  BMVert *v = (BMVert *)vertex.i;
  MSculptVert *mv = BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, v);

  if (mv->flag & SCULPTVERT_NEED_DISK_SORT) {
    mv->flag &= ~SCULPTVERT_NEED_DISK_SORT;

    BM_sort_disk_cycle(v);

    return true;
  }

  return false;
}

/*
Copies the bmesh, but orders the elements
according to PBVH node to improve memory locality
*/
void SCULPT_reorder_bmesh(SculptSession *ss)
{
#if 0
  SCULPT_face_random_access_ensure(ss);
  SCULPT_vertex_random_access_ensure(ss);

  int actv = ss->active_vertex_index.i ?
                 BKE_pbvh_vertex_index_to_table(ss->pbvh, ss->active_vertex_index) :
                 -1;
  int actf = ss->active_face_index.i ?
                 BKE_pbvh_face_index_to_table(ss->pbvh, ss->active_face_index) :
                 -1;

  if (ss->bm_log) {
    BM_log_full_mesh(ss->bm, ss->bm_log);
  }

  ss->bm = BKE_pbvh_reorder_bmesh(ss->pbvh);

  SCULPT_face_random_access_ensure(ss);
  SCULPT_vertex_random_access_ensure(ss);

  if (actv >= 0) {
    ss->active_vertex_index = BKE_pbvh_table_index_to_vertex(ss->pbvh, actv);
  }
  if (actf >= 0) {
    ss->active_face_index = BKE_pbvh_table_index_to_face(ss->pbvh, actf);
  }

  SCULPT_dyntopo_node_layers_update_offsets(ss);

  if (ss->bm_log) {
    BM_log_set_bm(ss->bm, ss->bm_log);
  }
#endif
}

void SCULPT_dynamic_topology_triangulate(SculptSession *ss, BMesh *bm)
{
  if (bm->totloop == bm->totface * 3) {
    ss->totfaces = ss->totpoly = ss->bm->totface;
    ss->totvert = ss->bm->totvert;

    return;
  }

  BMIter iter;
  BMFace *f;

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    BM_elem_flag_enable(f, BM_ELEM_TAG);
  }

  MemArena *pf_arena = BLI_memarena_new(BLI_POLYFILL_ARENA_SIZE, __func__);
  LinkNode *f_double = NULL;

  BMFace **faces_array = NULL;
  BLI_array_declare(faces_array);

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (f->len <= 3) {
      continue;
    }

    bool sel = BM_elem_flag_test(f, BM_ELEM_SELECT);

    int faces_array_tot = f->len;
    BLI_array_clear(faces_array);
    BLI_array_grow_items(faces_array, faces_array_tot);
    // BMFace **faces_array = BLI_array_alloca(faces_array, faces_array_tot);

    BM_face_triangulate(bm,
                        f,
                        faces_array,
                        &faces_array_tot,
                        NULL,
                        NULL,
                        &f_double,
                        MOD_TRIANGULATE_QUAD_BEAUTY,
                        MOD_TRIANGULATE_NGON_EARCLIP,
                        true,
                        pf_arena,
                        NULL);

    for (int i = 0; i < faces_array_tot; i++) {
      BMFace *f2 = faces_array[i];

      // forcibly copy selection state
      if (sel) {
        BM_face_select_set(bm, f2, true);

        // restore original face selection state too, triangulate code unset it
        BM_face_select_set(bm, f, true);
      }

      // paranoia check that tag flag wasn't copied over
      BM_elem_flag_disable(f2, BM_ELEM_TAG);
    }
  }

  while (f_double) {
    LinkNode *next = f_double->next;
    BM_face_kill(bm, f_double->link);
    MEM_freeN(f_double);
    f_double = next;
  }

  BLI_memarena_free(pf_arena);
  MEM_SAFE_FREE(faces_array);

  ss->totfaces = ss->totpoly = ss->bm->totface;
  ss->totvert = ss->bm->totvert;

  //  BM_mesh_triangulate(
  //      bm, MOD_TRIANGULATE_QUAD_BEAUTY, MOD_TRIANGULATE_NGON_EARCLIP, 4, false, NULL, NULL,
  //      NULL);
}

void SCULPT_pbvh_clear(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  /* Clear out any existing DM and PBVH. */
  if (ss->pbvh) {
    BKE_pbvh_free(ss->pbvh);
    ss->pbvh = NULL;
  }

  MEM_SAFE_FREE(ss->pmap);

  MEM_SAFE_FREE(ss->pmap_mem);

  BKE_object_free_derived_caches(ob);

  /* Tag to rebuild PBVH in depsgraph. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
}

void SCULPT_dyntopo_save_origverts(SculptSession *ss)
{
  BMIter iter;
  BMVert *v;

  BM_ITER_MESH (v, &iter, ss->bm, BM_VERTS_OF_MESH) {
    MSculptVert *mv = BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, v);

    copy_v3_v3(mv->origco, v->co);
    copy_v3_v3(mv->origno, v->no);

    if (ss->cd_vcol_offset >= 0) {
      MPropCol *mp = (MPropCol *)BM_ELEM_CD_GET_VOID_P(v, ss->cd_vcol_offset);
      copy_v4_v4(mv->origcolor, mp->color);
    }
  }
}

char dyntopop_node_idx_layer_id[] = "_dyntopo_node_id";

void SCULPT_dyntopo_node_layers_update_offsets(SculptSession *ss)
{
  SCULPT_dyntopo_node_layers_add(ss);
  if (ss->pbvh) {
    BKE_pbvh_update_offsets(ss->pbvh,
                            ss->cd_vert_node_offset,
                            ss->cd_face_node_offset,
                            ss->cd_sculpt_vert,
                            ss->cd_face_areas);
  }
  if (ss->bm_log) {
    BM_log_set_cd_offsets(ss->bm_log, ss->cd_sculpt_vert);
  }
}

bool SCULPT_dyntopo_has_templayer(SculptSession *ss, int type, const char *name)
{
  return CustomData_get_named_layer_index(&ss->bm->vdata, type, name) >= 0;
}

void SCULPT_dyntopo_ensure_templayer(SculptSession *ss,
                                     int type,
                                     const char *name,
                                     bool not_temporary)
{
  if (ss->save_temp_layers) {
    not_temporary = true;
  }

  int li = CustomData_get_named_layer_index(&ss->bm->vdata, type, name);

  if (li < 0) {
    BM_data_layer_add_named(ss->bm, &ss->bm->vdata, type, name);
    SCULPT_update_customdata_refs(ss);

    li = CustomData_get_named_layer_index(&ss->bm->vdata, type, name);
    ss->bm->vdata.layers[li].flag |= not_temporary ? 0 : CD_FLAG_TEMPORARY | CD_FLAG_NOCOPY;
  }
}

int SCULPT_dyntopo_get_templayer(SculptSession *ss, int type, const char *name)
{
  int li = CustomData_get_named_layer_index(&ss->bm->vdata, type, name);

  if (li < 0) {
    return -1;
  }

  return CustomData_get_n_offset(
      &ss->bm->vdata, type, li - CustomData_get_layer_index(&ss->bm->vdata, type));
}

char dyntopop_faces_areas_layer_id[] = "__dyntopo_face_areas";

void SCULPT_dyntopo_node_layers_add(SculptSession *ss)
{
  int cd_node_layer_index, cd_face_node_layer_index;

  int cd_origco_index, cd_origno_index, cd_origvcol_index = -1;
  bool have_vcol = CustomData_has_layer(&ss->bm->vdata, CD_PROP_COLOR);

  BMCustomLayerReq vlayers[] = {
      {CD_PAINT_MASK, NULL, 0},
      {CD_DYNTOPO_VERT, NULL, CD_FLAG_TEMPORARY | CD_FLAG_NOCOPY},
      {CD_PROP_INT32, dyntopop_node_idx_layer_id, CD_FLAG_TEMPORARY | CD_FLAG_NOCOPY}};

  BM_data_layers_ensure(ss->bm, &ss->bm->vdata, vlayers, 3);

  ss->cd_vert_mask_offset = CustomData_get_offset(&ss->bm->vdata, CD_PAINT_MASK);

  BMCustomLayerReq flayers[] = {
      {CD_PROP_INT32, dyntopop_node_idx_layer_id, CD_FLAG_TEMPORARY | CD_FLAG_NOCOPY},
      {CD_PROP_FLOAT, dyntopop_faces_areas_layer_id, CD_FLAG_TEMPORARY | CD_FLAG_NOCOPY},
  };
  BM_data_layers_ensure(ss->bm, &ss->bm->pdata, flayers, 2);

  // get indices again, as they might have changed after adding new layers
  cd_node_layer_index = CustomData_get_named_layer_index(
      &ss->bm->vdata, CD_PROP_INT32, dyntopop_node_idx_layer_id);
  cd_face_node_layer_index = CustomData_get_named_layer_index(
      &ss->bm->pdata, CD_PROP_INT32, dyntopop_node_idx_layer_id);

  ss->cd_origvcol_offset = -1;

  ss->cd_sculpt_vert = CustomData_get_offset(&ss->bm->vdata, CD_DYNTOPO_VERT);

  ss->cd_vcol_offset = CustomData_get_offset(&ss->bm->vdata, CD_PROP_COLOR);

  ss->cd_vert_node_offset = CustomData_get_n_offset(
      &ss->bm->vdata,
      CD_PROP_INT32,
      cd_node_layer_index - CustomData_get_layer_index(&ss->bm->vdata, CD_PROP_INT32));

  ss->bm->vdata.layers[cd_node_layer_index].flag |= CD_FLAG_TEMPORARY | CD_FLAG_NOCOPY;

  ss->cd_face_node_offset = CustomData_get_n_offset(
      &ss->bm->pdata,
      CD_PROP_INT32,
      cd_face_node_layer_index - CustomData_get_layer_index(&ss->bm->pdata, CD_PROP_INT32));

  ss->bm->pdata.layers[cd_face_node_layer_index].flag |= CD_FLAG_TEMPORARY | CD_FLAG_NOCOPY;
  ss->cd_faceset_offset = CustomData_get_offset(&ss->bm->pdata, CD_SCULPT_FACE_SETS);

  ss->cd_face_areas = CustomData_get_named_layer(
      &ss->bm->pdata, CD_PROP_FLOAT, dyntopop_faces_areas_layer_id);
  ss->cd_face_areas = ss->bm->pdata.layers[ss->cd_face_areas].offset;
}

/**
  Syncs customdata layers with internal bmesh, but ignores deleted layers.
*/
void SCULPT_dynamic_topology_sync_layers(Object *ob, Mesh *me)
{
  SculptSession *ss = ob->sculpt;

  if (!ss || !ss->bm) {
    return;
  }

  bool modified = false;
  BMesh *bm = ss->bm;

  CustomData *cd1[4] = {&me->vdata, &me->edata, &me->ldata, &me->pdata};
  CustomData *cd2[4] = {&bm->vdata, &bm->edata, &bm->ldata, &bm->pdata};
  int types[4] = {BM_VERT, BM_EDGE, BM_LOOP, BM_FACE};
  int badmask = CD_MASK_MLOOP | CD_MASK_MVERT | CD_MASK_MEDGE | CD_MASK_MPOLY | CD_MASK_ORIGINDEX |
                CD_MASK_ORIGSPACE | CD_MASK_MFACE;

  for (int i = 0; i < 4; i++) {
    CustomDataLayer **newlayers = NULL;
    BLI_array_declare(newlayers);

    CustomData *data1 = cd1[i];
    CustomData *data2 = cd2[i];

    if (!data1->layers) {
      modified |= data2->layers != NULL;
      continue;
    }

    for (int j = 0; j < data1->totlayer; j++) {
      CustomDataLayer *cl1 = data1->layers + j;

      if ((1 << cl1->type) & badmask) {
        continue;
      }

      int idx = CustomData_get_named_layer_index(data2, cl1->type, cl1->name);
      if (idx < 0) {
        BLI_array_append(newlayers, cl1);
      }
    }

    for (int j = 0; j < BLI_array_len(newlayers); j++) {
      BM_data_layer_add_named(bm, data2, newlayers[j]->type, newlayers[j]->name);
      modified = true;
    }

    bool typemap[CD_NUMTYPES] = {0};

    for (int j = 0; j < data1->totlayer; j++) {
      CustomDataLayer *cl1 = data1->layers + j;

      if ((1 << cl1->type) & badmask) {
        continue;
      }

      if (typemap[cl1->type]) {
        continue;
      }

      typemap[cl1->type] = true;

      // find first layer
      int baseidx = CustomData_get_layer_index(data2, cl1->type);

      if (baseidx < 0) {
        modified |= true;
        continue;
      }

      CustomDataLayer *cl2 = data2->layers + baseidx;

      int idx = CustomData_get_named_layer_index(data2, cl1->type, cl1[cl1->active].name);
      if (idx >= 0) {
        modified |= idx - baseidx != cl2->active;
        cl2->active = idx - baseidx;
      }

      idx = CustomData_get_named_layer_index(data2, cl1->type, cl1[cl1->active_rnd].name);
      if (idx >= 0) {
        modified |= idx - baseidx != cl2->active_rnd;
        cl2->active_rnd = idx - baseidx;
      }

      idx = CustomData_get_named_layer_index(data2, cl1->type, cl1[cl1->active_mask].name);
      if (idx >= 0) {
        modified |= idx - baseidx != cl2->active_mask;
        cl2->active_mask = idx - baseidx;
      }

      idx = CustomData_get_named_layer_index(data2, cl1->type, cl1[cl1->active_clone].name);
      if (idx >= 0) {
        modified |= idx - baseidx != cl2->active_clone;
        cl2->active_clone = idx - baseidx;
      }
    }

    BLI_array_free(newlayers);
  }

  if (modified && ss->bm) {
    CustomData_regen_active_refs(&ss->bm->vdata);
    CustomData_regen_active_refs(&ss->bm->edata);
    CustomData_regen_active_refs(&ss->bm->ldata);
    CustomData_regen_active_refs(&ss->bm->pdata);
  }

  if (modified) {
    SCULPT_dyntopo_node_layers_update_offsets(ss);
  }
}

BMesh *BM_mesh_bm_from_me_threaded(BMesh *bm,
                                   Object *ob,
                                   const Mesh *me,
                                   const struct BMeshFromMeshParams *params);

void SCULPT_dynamic_topology_enable_ex(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = ob->data;

  if (ss->bm) {
    bool ok = ss->bm->totvert == me->totvert && ss->bm->totedge == me->totedge &&
              ss->bm->totloop == me->totloop && ss->bm->totface == me->totpoly;

    if (!ok) {
      BM_mesh_free(ss->bm);
      ss->bm = NULL;
    }
  }

  if (!ss->bm || !ss->pbvh || BKE_pbvh_type(ss->pbvh) != PBVH_BMESH) {
    SCULPT_pbvh_clear(ob);
  }
  else {
    /*sculpt session was set up by paint.c. just call SCULPT_update_customdata_refs to be safe*/
    SCULPT_update_customdata_refs(ss);

    /* also check bm_log */
    if (!ss->bm_log) {
      ss->bm_log = BM_log_create(ss->bm, ss->cd_sculpt_vert);
    }

    return;
  }

  const BMAllocTemplate allocsize = {
      .totvert = 2048 * 16, .totface = 2048 * 16, .totloop = 4196 * 16, .totedge = 2048 * 16};

  SCULPT_clear_scl_pointers(ss);

  if (ss->mdyntopo_verts) {
    MEM_freeN(ss->mdyntopo_verts);
    ss->mdyntopo_verts = NULL;
  }

  ss->bm_smooth_shading = (scene->toolsettings->sculpt->flags & SCULPT_DYNTOPO_SMOOTH_SHADING) !=
                          0;

  /* Dynamic topology doesn't ensure selection state is valid, so remove T36280. */
  BKE_mesh_mselect_clear(me);

#if 1

  if (!ss->bm) {
    ss->bm = BM_mesh_create(
        &allocsize,
        &((struct BMeshCreateParams){.use_toolflags = false,
                                     .create_unique_ids = true,
                                     .id_elem_mask = BM_VERT | BM_EDGE | BM_FACE,
                                     .id_map = true,
                                     .temporary_ids = false,
                                     .no_reuse_ids = false}));

    BM_mesh_bm_from_me(NULL,
                       ss->bm,
                       me,
                       (&(struct BMeshFromMeshParams){
                           .calc_face_normal = true,
                           .use_shapekey = true,
                           .create_shapekey_layers = true,
                           .active_shapekey = ob->shapenr,
                       }));
  }
#else
  ss->bm = BM_mesh_bm_from_me_threaded(NULL,
                                       NULL,
                                       me,
                                       (&(struct BMeshFromMeshParams){
                                           .calc_face_normal = true,
                                           .use_shapekey = true,
                                           .active_shapekey = ob->shapenr,
                                       }));
#endif

#ifndef DYNTOPO_DYNAMIC_TESS
  SCULPT_dynamic_topology_triangulate(ss, ss->bm);
#endif

  SCULPT_dyntopo_node_layers_add(ss);
  SCULPT_dyntopo_save_origverts(ss);

  if (SCULPT_has_persistent_base(ss)) {
    SCULPT_ensure_persistent_layers(ss);
  }

  SCULPT_update_customdata_refs(ss);

  BMIter iter;
  BMVert *v;

  int cd_vcol_offset = CustomData_get_offset(&ss->bm->vdata, CD_PROP_COLOR);

  int i = 0;
  BMEdge *e;

  BM_ITER_MESH (e, &iter, ss->bm, BM_EDGES_OF_MESH) {
    e->head.hflag |= BM_ELEM_DRAW;
  }

  BM_ITER_MESH (v, &iter, ss->bm, BM_VERTS_OF_MESH) {
    MSculptVert *mv = BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, v);

    mv->flag |= SCULPTVERT_NEED_DISK_SORT | SCULPTVERT_NEED_VALENCE;

    BKE_pbvh_update_vert_boundary(ss->cd_sculpt_vert,
                                  ss->cd_faceset_offset,
                                  ss->cd_vert_node_offset,
                                  ss->cd_face_node_offset,
                                  ss->cd_vcol_offset,
                                  v,
                                  ss->boundary_symmetry);
    BKE_pbvh_bmesh_update_valence(ss->cd_sculpt_vert, (SculptVertRef){.i = (intptr_t)v});

    copy_v3_v3(mv->origco, v->co);
    copy_v3_v3(mv->origno, v->no);

    if (ss->cd_vcol_offset >= 0) {
      MPropCol *color = (MPropCol *)BM_ELEM_CD_GET_VOID_P(v, cd_vcol_offset);
      copy_v4_v4(mv->origcolor, color->color);
    }

    i++;
  }

  /* Make sure the data for existing faces are initialized. */
  if (me->totpoly != ss->bm->totface) {
    BM_mesh_normals_update(ss->bm);
  }

  /* Enable dynamic topology. */
  me->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;

  /* Enable logging for undo/redo. */
  if (!ss->bm_log) {
    ss->bm_log = BM_log_create(ss->bm, ss->cd_sculpt_vert);
  }

  /* Update dependency graph, so modifiers that depend on dyntopo being enabled
   * are re-evaluated and the PBVH is re-created. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

  // TODO: this line here is being slow, do we need it? - joeedh
  BKE_scene_graph_update_tagged(depsgraph, bmain);
}

/* Free the sculpt BMesh and BMLog
 *
 * If 'unode' is given, the BMesh's data is copied out to the unode
 * before the BMesh is deleted so that it can be restored from. */
static void SCULPT_dynamic_topology_disable_ex(
    Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob, SculptUndoNode *unode)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = ob->data;

  SCULPT_pbvh_clear(ob);
  SCULPT_clear_scl_pointers(ss);

  BKE_sculptsession_bm_to_me(ob, true);

  /* Sync the visibility to vertices manually as the pmap is still not initialized. */
  for (int i = 0; i < me->totvert; i++) {
    me->mvert[i].flag &= ~ME_HIDE;
    me->mvert[i].flag |= ME_VERT_PBVH_UPDATE;
  }

  /* Clear data. */
  me->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;

  if (ss->bm_log) {
    BM_log_free(ss->bm_log, true);
    ss->bm_log = NULL;
  }

  /* Typically valid but with global-undo they can be NULL, see: T36234. */
  if (ss->bm) {
    BM_mesh_free(ss->bm);
    ss->bm = NULL;
  }

  BKE_particlesystem_reset_all(ob);
  BKE_ptcache_object_reset(scene, ob, PTCACHE_RESET_OUTDATED);

  /* Update dependency graph, so modifiers that depend on dyntopo being enabled
   * are re-evaluated and the PBVH is re-created. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  BKE_scene_graph_update_tagged(depsgraph, bmain);
}

void SCULPT_dynamic_topology_disable(bContext *C, SculptUndoNode *unode)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  SCULPT_dynamic_topology_disable_ex(bmain, depsgraph, scene, ob, unode);
}

void sculpt_dynamic_topology_disable_with_undo(Main *bmain,
                                               Depsgraph *depsgraph,
                                               Scene *scene,
                                               Object *ob)
{
  SculptSession *ss = ob->sculpt;
  if (ss->bm != NULL) {
    /* May be false in background mode. */
    const bool use_undo = G.background ? (ED_undo_stack_get() != NULL) : true;
    if (use_undo) {
      SCULPT_undo_push_begin(ob, "Dynamic topology disable");
      SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_DYNTOPO_END);
    }
    SCULPT_dynamic_topology_disable_ex(bmain, depsgraph, scene, ob, NULL);
    if (use_undo) {
      SCULPT_undo_push_end();
    }

    ss->active_vertex_index.i = ss->active_face_index.i = 0;
  }
}

static void sculpt_dynamic_topology_enable_with_undo(Main *bmain,
                                                     Depsgraph *depsgraph,
                                                     Scene *scene,
                                                     Object *ob)
{
  SculptSession *ss = ob->sculpt;

  if (ss->bm == NULL) {
    /* May be false in background mode. */
    const bool use_undo = G.background ? (ED_undo_stack_get() != NULL) : true;
    if (use_undo) {
      SCULPT_undo_push_begin(ob, "Dynamic topology enable");
    }
    SCULPT_dynamic_topology_enable_ex(bmain, depsgraph, scene, ob);
    if (use_undo) {
      SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_DYNTOPO_BEGIN);
      SCULPT_undo_push_end();
    }

    ss->active_vertex_index.i = ss->active_face_index.i = 0;
  }
}

static int sculpt_dynamic_topology_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  WM_cursor_wait(true);

  if (ss->bm) {
    sculpt_dynamic_topology_disable_with_undo(bmain, depsgraph, scene, ob);
  }
  else {
    sculpt_dynamic_topology_enable_with_undo(bmain, depsgraph, scene, ob);
  }

  WM_cursor_wait(false);
  WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, NULL);

  return OPERATOR_FINISHED;
}

static int dyntopo_error_popup(bContext *C, wmOperatorType *ot, enum eDynTopoWarnFlag flag)
{
  uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Error!"), ICON_ERROR);
  uiLayout *layout = UI_popup_menu_layout(pup);

  if (flag & DYNTOPO_ERROR_MULTIRES) {
    const char *msg_error = TIP_("Multires modifier detected; cannot enable dyntopo.");
    const char *msg = TIP_("Dyntopo and multires cannot be mixed.");

    uiItemL(layout, msg_error, ICON_INFO);
    uiItemL(layout, msg, ICON_NONE);
    uiItemS(layout);
  }

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

static int dyntopo_warning_popup(bContext *C, wmOperatorType *ot, enum eDynTopoWarnFlag flag)
{
  uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Warning!"), ICON_ERROR);
  uiLayout *layout = UI_popup_menu_layout(pup);

  if (flag & (DYNTOPO_WARN_EDATA)) {
    const char *msg_error = TIP_("Edge Data Detected!");
    const char *msg = TIP_("Dyntopo will not preserve custom edge attributes");
    uiItemL(layout, msg_error, ICON_INFO);
    uiItemL(layout, msg, ICON_NONE);
    uiItemS(layout);
  }

  if (flag & DYNTOPO_WARN_MODIFIER) {
    const char *msg_error = TIP_("Generative Modifiers Detected!");
    const char *msg = TIP_(
        "Keeping the modifiers will increase polycount when returning to object mode");

    uiItemL(layout, msg_error, ICON_INFO);
    uiItemL(layout, msg, ICON_NONE);
    uiItemS(layout);
  }

  uiItemFullO_ptr(layout, ot, IFACE_("OK"), ICON_NONE, NULL, WM_OP_EXEC_DEFAULT, 0, NULL);

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

enum eDynTopoWarnFlag SCULPT_dynamic_topology_check(Scene *scene, Object *ob)
{
  Mesh *me = ob->data;
  SculptSession *ss = ob->sculpt;

  enum eDynTopoWarnFlag flag = 0;

  BLI_assert(ss->bm == NULL);
  UNUSED_VARS_NDEBUG(ss);

  {
    VirtualModifierData virtualModifierData;
    ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);

    /* Exception for shape keys because we can edit those. */
    for (; md; md = md->next) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);
      if (!BKE_modifier_is_enabled(scene, md, eModifierMode_Realtime)) {
        continue;
      }

      if (md->type == eModifierType_Multires) {
        flag |= DYNTOPO_ERROR_MULTIRES;
      }

      if (mti->type == eModifierTypeType_Constructive) {
        flag |= DYNTOPO_WARN_MODIFIER;
        break;
      }
    }
  }

  return flag;
}

static int sculpt_dynamic_topology_toggle_invoke(bContext *C,
                                                 wmOperator *op,
                                                 const wmEvent *UNUSED(event))
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  if (!ss->bm) {
    Scene *scene = CTX_data_scene(C);
    enum eDynTopoWarnFlag flag = SCULPT_dynamic_topology_check(scene, ob);

    if (flag & DYNTOPO_ERROR_MULTIRES) {
      return dyntopo_error_popup(C, op->type, flag);
    }
    else if (flag) {
      /* The mesh has customdata that will be lost, let the user confirm this is OK. */
      return dyntopo_warning_popup(C, op->type, flag);
    }
  }

  return sculpt_dynamic_topology_toggle_exec(C, op);
}

void SCULPT_OT_dynamic_topology_toggle(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Dynamic Topology Toggle";
  ot->idname = "SCULPT_OT_dynamic_topology_toggle";
  ot->description =
      "Dynamic mode; note that you must now check the DynTopo"
      "option to enable dynamic remesher (which updates topology will sculpting)"
      "this is on by default.";

  /* API callbacks. */
  ot->invoke = sculpt_dynamic_topology_toggle_invoke;
  ot->exec = sculpt_dynamic_topology_toggle_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

#define MAXUVLOOPS 32
#define MAXUVNEIGHBORS 32

typedef struct UVSmoothVert {
  double uv[2];
  float co[3];  // world co
  BMVert *v;
  double w;
  int totw;
  bool pinned, boundary;
  BMLoop *ls[MAXUVLOOPS];
  struct UVSmoothVert *neighbors[MAXUVNEIGHBORS];
  int totloop, totneighbor;
  float brushfade;
} UVSmoothVert;

typedef struct UVSmoothTri {
  UVSmoothVert *vs[3];
  float area2d, area3d;
} UVSmoothTri;

#define CON_MAX_VERTS 16
typedef struct UVSmoothConstraint {
  int type;
  double k;
  UVSmoothVert *vs[CON_MAX_VERTS];
  UVSmoothTri *tri;
  double gs[CON_MAX_VERTS][2];
  int totvert;
  double params[8];
} UVSmoothConstraint;

enum { CON_ANGLES = 0, CON_AREA = 1 };

typedef struct UVSolver {
  BLI_mempool *verts;
  BLI_mempool *tris;
  int totvert, tottri;
  float snap_limit;
  BLI_mempool *constraints;
  GHash *vhash;
  GHash *fhash;
  int cd_uv;

  double totarea3d;
  double totarea2d;

  double strength;
  int cd_sculpt_vert;
} UVSolver;

/*that that currently this tool is *not* threaded*/

typedef struct SculptUVThreadData {
  SculptThreadedTaskData data;
  UVSolver *solver;
} SculptUVThreadData;

static UVSolver *uvsolver_new(int cd_uv)
{
  UVSolver *solver = MEM_callocN(sizeof(*solver), "solver");

  solver->strength = 1.0;
  solver->cd_uv = cd_uv;
  solver->snap_limit = 0.0025;

  solver->verts = BLI_mempool_create(sizeof(UVSmoothVert), 0, 512, BLI_MEMPOOL_ALLOW_ITER);
  solver->tris = BLI_mempool_create(sizeof(UVSmoothTri), 0, 512, BLI_MEMPOOL_ALLOW_ITER);
  solver->constraints = BLI_mempool_create(
      sizeof(UVSmoothConstraint), 0, 512, BLI_MEMPOOL_ALLOW_ITER);

  solver->vhash = BLI_ghash_ptr_new("uvsolver");
  solver->fhash = BLI_ghash_ptr_new("uvsolver");

  return solver;
}

static void uvsolver_free(UVSolver *solver)
{
  BLI_mempool_destroy(solver->verts);
  BLI_mempool_destroy(solver->tris);
  BLI_mempool_destroy(solver->constraints);

  BLI_ghash_free(solver->vhash, NULL, NULL);
  BLI_ghash_free(solver->fhash, NULL, NULL);

  MEM_freeN(solver);
}

void *uvsolver_calc_loop_key(UVSolver *solver, BMLoop *l)
{
  // return (void *)l->v;
  MLoopUV *uv = BM_ELEM_CD_GET_VOID_P(l, solver->cd_uv);

  // float u = floorf(uv->uv[0] / solver->snap_limit) * solver->snap_limit;
  // float v = floorf(uv->uv[1] / solver->snap_limit) * solver->snap_limit;

  intptr_t x = (intptr_t)(uv->uv[0] * 16384.0);
  intptr_t y = (intptr_t)(uv->uv[1] * 16384.0);
  intptr_t key;
  MSculptVert *mv = BKE_PBVH_SCULPTVERT(solver->cd_sculpt_vert, l->v);

  if ((mv->flag & SCULPTVERT_SEAM_BOUNDARY) ||
      (l->e->head.hflag | l->prev->e->head.hflag) & BM_ELEM_SEAM) {
    key = y * 16384LL + x;
  }
  else {
    key = (intptr_t)l->v;
  }

  return POINTER_FROM_INT(key);
}

static UVSmoothVert *uvsolver_get_vert(UVSolver *solver, BMLoop *l)
{
  MLoopUV *uv = BM_ELEM_CD_GET_VOID_P(l, solver->cd_uv);

  void *pkey = uvsolver_calc_loop_key(solver, l);
  void **entry = NULL;
  UVSmoothVert *v;

  if (!BLI_ghash_ensure_p(solver->vhash, pkey, &entry)) {
    v = BLI_mempool_alloc(solver->verts);
    memset(v, 0, sizeof(*v));

    MSculptVert *mv = BKE_PBVH_SCULPTVERT(solver->cd_sculpt_vert, l->v);
    MSculptVert *mv2 = BKE_PBVH_SCULPTVERT(solver->cd_sculpt_vert, l->prev->v);
    MSculptVert *mv3 = BKE_PBVH_SCULPTVERT(solver->cd_sculpt_vert, l->next->v);

    v->boundary = mv->flag & SCULPTVERT_SEAM_BOUNDARY;
    if ((mv->flag | mv2->flag | mv3->flag) & SCULPTVERT_SHARP_CORNER) {
      v->pinned = true;
    }

    // copy_v2_v2(v->uv, uv->uv);
    v->uv[0] = (double)uv->uv[0];
    v->uv[1] = (double)uv->uv[1];

    if (isnan(v->uv[0]) || !isfinite(v->uv[0])) {
      v->uv[0] = 0.0f;
    }
    if (isnan(v->uv[1]) || !isfinite(v->uv[1])) {
      v->uv[1] = 0.0f;
    }

    copy_v3_v3(v->co, l->v->co);
    v->v = l->v;

    *entry = (void *)v;
  }

  v = (UVSmoothVert *)*entry;

  if (v->totloop < MAXUVLOOPS) {
    v->ls[v->totloop++] = l;
  }

  return v;
}

MINLINE double area_tri_signed_v2_db(const double v1[2], const double v2[2], const double v3[2])
{
  return 0.5f * ((v1[0] - v2[0]) * (v2[1] - v3[1]) + (v1[1] - v2[1]) * (v3[0] - v2[0]));
}

MINLINE double area_tri_v2_db(const double v1[2], const double v2[2], const double v3[2])
{
  return fabsf(area_tri_signed_v2_db(v1, v2, v3));
}

void cross_tri_v3_db(double n[3], const double v1[3], const double v2[3], const double v3[3])
{
  double n1[3], n2[3];

  n1[0] = v1[0] - v2[0];
  n2[0] = v2[0] - v3[0];
  n1[1] = v1[1] - v2[1];
  n2[1] = v2[1] - v3[1];
  n1[2] = v1[2] - v2[2];
  n2[2] = v2[2] - v3[2];
  n[0] = n1[1] * n2[2] - n1[2] * n2[1];
  n[1] = n1[2] * n2[0] - n1[0] * n2[2];
  n[2] = n1[0] * n2[1] - n1[1] * n2[0];
}

double area_tri_v3_db(const double v1[3], const double v2[3], const double v3[3])
{
  double n[3];
  cross_tri_v3_db(n, v1, v2, v3);
  return len_v3_db(n) * 0.5;
}

static UVSmoothTri *uvsolver_ensure_face(UVSolver *solver, BMFace *f)
{
  void **entry = NULL;

  if (BLI_ghash_ensure_p(solver->fhash, (void *)f, &entry)) {
    return (UVSmoothTri *)*entry;
  }

  UVSmoothTri *tri = BLI_mempool_alloc(solver->tris);
  memset((void *)tri, 0, sizeof(*tri));
  *entry = (void *)tri;

  BMLoop *l = f->l_first;

  bool nocon = false;
  int i = 0;
  do {
    UVSmoothVert *sv = uvsolver_get_vert(solver, l);

    if (BM_elem_flag_test(l->e, BM_ELEM_SEAM)) {
      nocon = true;
    }

    tri->vs[i] = sv;

    if (i > 3) {
      // bad!
      break;
    }

    i++;
  } while ((l = l->next) != f->l_first);

  double area3d = (double)area_tri_v3(tri->vs[0]->co, tri->vs[1]->co, tri->vs[2]->co);
  double area2d = area_tri_v2_db(tri->vs[0]->uv, tri->vs[1]->uv, tri->vs[2]->uv);

  if (area2d < 0.000001) {
    tri->vs[0]->uv[0] -= 0.0001;
    tri->vs[0]->uv[1] -= 0.0001;
    tri->vs[1]->uv[0] += 0.0001;
    tri->vs[2]->uv[1] += 0.0001;
  }

  solver->totarea2d += area2d;
  solver->totarea3d += area3d;

  tri->area2d = area2d;
  tri->area3d = area3d;

  for (int i = 0; !nocon && i < 3; i++) {

    UVSmoothConstraint *con = BLI_mempool_alloc(solver->constraints);
    memset((void *)con, 0, sizeof(*con));
    con->type = CON_ANGLES;
    con->k = 0.5;

    UVSmoothVert *v0 = tri->vs[(i + 2) % 3];
    UVSmoothVert *v1 = tri->vs[i];
    UVSmoothVert *v2 = tri->vs[(i + 1) % 3];

    con->vs[0] = v0;
    con->vs[1] = v1;
    con->vs[2] = v2;
    con->totvert = 3;

    float t1[3], t2[3];

    sub_v3_v3v3(t1, v0->co, v1->co);
    sub_v3_v3v3(t2, v2->co, v1->co);

    normalize_v3(t1);
    normalize_v3(t2);

    float th3d = saacosf(dot_v3v3(t1, t2));

    con->params[0] = (double)th3d;

    // area constraint
    con = BLI_mempool_alloc(solver->constraints);
    memset((void *)con, 0, sizeof(*con));

    con->vs[0] = v0;
    con->vs[1] = v1;
    con->vs[2] = v2;
    con->totvert = 3;
    con->tri = tri;
    con->type = CON_AREA;
    con->k = 1.0;
  }

#if 1
  for (int i = 0; i < 3; i++) {
    UVSmoothVert *v1 = tri->vs[i];
    UVSmoothVert *v2 = tri->vs[(i + 1) % 3];

    bool ok = true;

    for (int j = 0; j < v1->totneighbor; j++) {
      if (v1->neighbors[j] == v2) {
        ok = false;
        break;
      }
    }

    ok = ok && v1->totneighbor < MAXUVNEIGHBORS && v2->totneighbor < MAXUVNEIGHBORS;

    if (!ok) {
      continue;
    }

    v1->neighbors[v1->totneighbor++] = v2;
    v2->neighbors[v2->totneighbor++] = v1;
  }
#endif

  return tri;
}

static double normalize_v2_db(double v[2])
{
  double len = v[0] * v[0] + v[1] * v[1];

  if (len < 0.0000001) {
    v[0] = v[1] = 0.0;
    return 0.0;
  }

  len = sqrt(len);

  double mul = 1.0 / len;

  v[0] *= mul;
  v[1] *= mul;

  return len;
}

static double uvsolver_eval_constraint(UVSolver *solver, UVSmoothConstraint *con)
{
  switch (con->type) {
    case CON_ANGLES: {
      UVSmoothVert *v0 = con->vs[0];
      UVSmoothVert *v1 = con->vs[1];
      UVSmoothVert *v2 = con->vs[2];
      double t1[2], t2[2];

      sub_v2_v2v2_db(t1, v0->uv, v1->uv);
      sub_v2_v2v2_db(t2, v2->uv, v1->uv);

      normalize_v2_db(t1);
      normalize_v2_db(t2);

      double th = saacos(dot_v2v2_db(t1, t2));

      double wind = t1[0] * t2[1] - t1[1] * t2[0];

      if (wind >= 0.0) {
        th = M_PI - th;
      }

      return th - con->params[0];
    }
    case CON_AREA: {
      UVSmoothVert *v0 = con->vs[0];
      UVSmoothVert *v1 = con->vs[1];
      UVSmoothVert *v2 = con->vs[2];

      if (con->tri->area3d == 0.0 || solver->totarea3d == 0.0) {
        return 0.0;
      }

      double area2d = area_tri_signed_v2_db(v0->uv, v1->uv, v2->uv);
      double goal = con->tri->area3d * solver->totarea2d / solver->totarea3d;

      con->tri->area2d = area2d;
      return (area2d - goal) * 1024.0;
    }
    default:
      return 0.0f;
  }
}

BLI_INLINE float uvsolver_vert_weight(UVSmoothVert *sv)
{
  double w = 1.0;

  if (sv->pinned || sv->boundary || sv->brushfade == 0.0f) {
    w = 100000.0;
  }
  else {
    return 1.0 / sv->brushfade;
  }

  return w;
}

static void uvsolver_solve_begin(UVSolver *solver)
{
  UVSmoothVert *sv;
  BLI_mempool_iter iter;

  BLI_mempool_iternew(solver->verts, &iter);
  sv = BLI_mempool_iterstep(&iter);
  BMIter liter;

  for (; sv; sv = BLI_mempool_iterstep(&iter)) {
    BMLoop *l;
    sv->pinned = false;

    BM_ITER_ELEM (l, &liter, sv->v, BM_LOOPS_OF_VERT) {
      if (!BLI_ghash_haskey(solver->fhash, (void *)l->f)) {
        sv->pinned = true;
      }
    }
  }
}

static void uvsolver_simple_relax(UVSolver *solver, float strength)
{
  BLI_mempool_iter iter;

  UVSmoothVert *sv1;
  BLI_mempool_iternew(solver->verts, &iter);

  sv1 = BLI_mempool_iterstep(&iter);
  for (; sv1; sv1 = BLI_mempool_iterstep(&iter)) {
    double uv[2] = {0.0, 0.0};
    double tot = 0.0;

    if (!sv1->totneighbor || sv1->pinned) {
      continue;
    }

    for (int i = 0; i < sv1->totneighbor; i++) {
      UVSmoothVert *sv2 = sv1->neighbors[i];

      if (!sv2 || (sv1->boundary && !sv2->boundary)) {
        continue;
      }

      uv[0] += sv2->uv[0];
      uv[1] += sv2->uv[1];
      tot += 1.0;
    }

    if (tot < 2.0) {
      continue;
    }

    uv[0] /= tot;
    uv[1] /= tot;

    sv1->uv[0] += (uv[0] - sv1->uv[0]) * strength;
    sv1->uv[1] += (uv[1] - sv1->uv[1]) * strength;
  }

  // update real uvs

  const int cd_uv = solver->cd_uv;

  BLI_mempool_iternew(solver->verts, &iter);
  UVSmoothVert *sv = BLI_mempool_iterstep(&iter);
  for (; sv; sv = BLI_mempool_iterstep(&iter)) {
    for (int i = 0; i < sv->totloop; i++) {
      BMLoop *l = sv->ls[i];
      MLoopUV *uv = BM_ELEM_CD_GET_VOID_P(l, cd_uv);

      uv->uv[0] = (float)sv->uv[0];
      uv->uv[1] = (float)sv->uv[1];
    }
  }
}

static float uvsolver_solve_step(UVSolver *solver)
{
  BLI_mempool_iter iter;

  if (solver->strength < 0) {
    uvsolver_simple_relax(solver, fabs(solver->strength));
    return 0.0f;
  }
  else {
    uvsolver_simple_relax(solver, solver->strength * 0.1f);
  }

  double error = 0.0;

  const double eval_limit = 0.00001;
  const double df = 0.0001;
  int totcon = 0;

  BLI_mempool_iternew(solver->constraints, &iter);
  UVSmoothConstraint *con = BLI_mempool_iterstep(&iter);
  for (; con; con = BLI_mempool_iterstep(&iter)) {
    double r1 = uvsolver_eval_constraint(solver, con);

    if (fabs(r1) < eval_limit) {
      totcon++;
      continue;
    }

    error += fabs(r1);
    totcon++;

    double totg = 0.0;
    double totw = 0.0;

    for (int i = 0; i < con->totvert; i++) {
      UVSmoothVert *sv = con->vs[i];

      for (int j = 0; j < 2; j++) {
        double orig = sv->uv[j];
        sv->uv[j] += df;

        double r2 = uvsolver_eval_constraint(solver, con);
        double g = (r2 - r1) / df;

        con->gs[i][j] = g;
        totg += g * g;

        sv->uv[j] = orig;

        totw += uvsolver_vert_weight(sv);
      }
    }

    if (totg < eval_limit) {
      continue;
    }

    r1 *= -solver->strength * 0.75 * con->k / totg;

    if (totw == 0.0) {
      continue;
    }

    totw = 1.0 / totw;

    for (int i = 0; i < con->totvert; i++) {
      UVSmoothVert *sv = con->vs[i];
      double w = uvsolver_vert_weight(sv) * totw * sv->brushfade;
      w = MIN2(w, 1.0);

      for (int j = 0; j < 2; j++) {
        double off = r1 * con->gs[i][j] * w;

        CLAMP(off, -0.1, 0.1);
        sv->uv[j] += off;
      }
    }
  }

  // update real uvs

  const int cd_uv = solver->cd_uv;

  BLI_mempool_iternew(solver->verts, &iter);
  UVSmoothVert *sv = BLI_mempool_iterstep(&iter);
  for (; sv; sv = BLI_mempool_iterstep(&iter)) {
    for (int i = 0; i < sv->totloop; i++) {
      BMLoop *l = sv->ls[i];
      MLoopUV *uv = BM_ELEM_CD_GET_VOID_P(l, cd_uv);

      uv->uv[0] = (float)sv->uv[0];
      uv->uv[1] = (float)sv->uv[1];
    }
  }

  return (float)error / (float)totcon;
}

static void sculpt_uv_brush_cb(void *__restrict userdata,
                               const int n,
                               const TaskParallelTLS *__restrict tls)
{
  SculptUVThreadData *data1 = userdata;
  SculptThreadedTaskData *data = &data1->data;
  SculptSession *ss = data->ob->sculpt;
  // const Brush *brush = data->brush;
  // const float *offset = data->offset;

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  PBVHNode *node = data->nodes[n];
  TableGSet *faces = BKE_pbvh_bmesh_node_faces(node);
  BMFace *f;
  const int cd_uv = CustomData_get_offset(&ss->bm->ldata, CD_MLOOPUV);

  if (cd_uv < 0) {
    return;  // no uv layers
  }

  float bstrength = ss->cache->bstrength;
  const int cd_mask = CustomData_get_offset(&ss->bm->vdata, CD_PAINT_MASK);

  BKE_pbvh_node_mark_update_color(node);

  TGSET_ITER (f, faces) {
    BMLoop *l = f->l_first;
    // float mask = 0.0f;
    float cent[3] = {0};
    int tot = 0;

    // uvsolver_get_vert
    do {
      add_v3_v3(cent, l->v->co);
      tot++;
    } while ((l = l->next) != f->l_first);

    mul_v3_fl(cent, 1.0f / (float)tot);

    if (!sculpt_brush_test_sq_fn(&test, cent)) {
      continue;
    }

    BM_log_face_modified(ss->bm_log, f);
    uvsolver_ensure_face(data1->solver, f);

    do {
      BMIter iter;
      BMLoop *l2;
      int tot2 = 0;
      float uv[2] = {0};
      bool ok = true;
      UVSmoothVert *lastv = NULL;

      BM_ITER_ELEM (l2, &iter, l->v, BM_LOOPS_OF_VERT) {
        if (l2->v != l->v) {
          l2 = l2->prev->v == l->v ? l2->prev : l2->next;
        }

        UVSmoothVert *sv = uvsolver_get_vert(data1->solver, l2);

        if (lastv && lastv != sv) {
          ok = false;
          lastv->boundary = true;
          sv->boundary = true;
        }

        lastv = sv;

        MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l2, cd_uv);

        add_v2_v2(uv, luv->uv);
        tot2++;

        if (BM_elem_flag_test(l2->e, BM_ELEM_SEAM)) {
          ok = false;
          sv->boundary = true;
        }
      }

      ok = ok && tot2;

      if (ok) {
        mul_v2_fl(uv, 1.0f / (float)tot2);

        BM_ITER_ELEM (l2, &iter, l->v, BM_LOOPS_OF_VERT) {
          if (l2->v != l->v) {
            l2 = l2->next;
          }

          MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l2, cd_uv);

          if (len_v2v2(luv->uv, uv) < 0.02) {
            copy_v2_v2(luv->uv, uv);
          }
        }
      }
    } while ((l = l->next) != f->l_first);

#if 0
    do {
      if (!sculpt_brush_test_sq_fn(&test, l->v->co)) {
        continue;
      }

      if (cd_mask >= 0) {
        mask = BM_ELEM_CD_GET_FLOAT(l->v, cd_mask);
      }

      SculptVertRef vertex = {(intptr_t)l->v};

      float direction2[3];
      const float fade =
          bstrength *
          SCULPT_brush_strength_factor(
              ss, brush, vd.co, sqrtf(test.dist), NULL, l->v->no, mask, vertex, thread_id) *
          ss->cache->pressure;

    } while ((l = l->next) != f->l_first);
#endif
  }
  TGSET_ITER_END;
}

void SCULPT_uv_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = ss->cache ? ss->cache->brush : BKE_paint_brush(&sd->paint);
  float offset[3];
  const float bstrength = ss->cache->bstrength;

  if (!ss->bm || BKE_pbvh_type(ss->pbvh) != PBVH_BMESH) {
    // dyntopo only
    return;
  }

  const int cd_uv = CustomData_get_offset(&ss->bm->ldata, CD_MLOOPUV);
  if (cd_uv < 0) {
    return;  // no uv layer?
  }

  // add undo log subentry
  BM_log_entry_add_ex(ss->bm, ss->bm_log, true);

  BKE_curvemapping_init(brush->curve);

  UVSolver *solver = uvsolver_new(cd_uv);
  solver->cd_sculpt_vert = ss->cd_sculpt_vert;
  solver->strength = ss->cache->bstrength;

  /* Threaded loop over nodes. */
  SculptUVThreadData data = {.solver = solver,
                             .data = {
                                 .sd = sd,
                                 .ob = ob,
                                 .brush = brush,
                                 .nodes = nodes,
                                 .offset = offset,
                             }};

  TaskParallelSettings settings;

  // for now, be single-threaded
  BKE_pbvh_parallel_range_settings(&settings, false, totnode);
  BLI_task_parallel_range(0, totnode, &data, sculpt_uv_brush_cb, &settings);

  uvsolver_solve_begin(solver);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, brush->falloff_shape);

  BLI_mempool_iter iter;
  BLI_mempool_iternew(solver->verts, &iter);
  UVSmoothVert *sv = BLI_mempool_iterstep(&iter);

  for (; sv; sv = BLI_mempool_iterstep(&iter)) {
    if (!sculpt_brush_test_sq_fn(&test, sv->v->co)) {
      sv->brushfade = 0.0f;
      continue;
    }

    sv->brushfade = SCULPT_brush_strength_factor(ss,
                                                 brush,
                                                 sv->v->co,
                                                 sqrtf(test.dist),
                                                 NULL,
                                                 sv->v->no,
                                                 0.0f,
                                                 (SculptVertRef){.i = (intptr_t)sv->v},
                                                 0);
  }

  for (int i = 0; i < 5; i++) {
    uvsolver_solve_step(solver);
  }

  // tear down solver
  uvsolver_free(solver);
}
