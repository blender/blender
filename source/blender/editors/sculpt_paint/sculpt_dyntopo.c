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

  if (ss->pmap) {
    MEM_freeN(ss->pmap);
    ss->pmap = NULL;
  }

  if (ss->pmap_mem) {
    MEM_freeN(ss->pmap_mem);
    ss->pmap_mem = NULL;
  }

  BKE_object_free_derived_caches(ob);

  /* Tag to rebuild PBVH in depsgraph. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
}

void SCULPT_dyntopo_save_origverts(SculptSession *ss)
{
  BMIter iter;
  BMVert *v;

  BM_ITER_MESH (v, &iter, ss->bm, BM_VERTS_OF_MESH) {
    MDynTopoVert *mv = BKE_PBVH_DYNVERT(ss->cd_dyn_vert, v);

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
    BKE_pbvh_update_offsets(
        ss->pbvh, ss->cd_vert_node_offset, ss->cd_face_node_offset, ss->cd_dyn_vert);
  }
  if (ss->bm_log) {
    BM_log_set_cd_offsets(ss->bm_log, ss->cd_dyn_vert);
  }
}

bool SCULPT_dyntopo_has_templayer(SculptSession *ss, int type, const char *name)
{
  return CustomData_get_named_layer_index(&ss->bm->vdata, type, name) >= 0;
}

void SCULPT_dyntopo_ensure_templayer(SculptSession *ss, int type, const char *name)
{
  int li = CustomData_get_named_layer_index(&ss->bm->vdata, type, name);

  if (li < 0) {
    BM_data_layer_add_named(ss->bm, &ss->bm->vdata, type, name);
    SCULPT_dyntopo_node_layers_update_offsets(ss);

    li = CustomData_get_named_layer_index(&ss->bm->vdata, type, name);
    ss->bm->vdata.layers[li].flag |= CD_FLAG_TEMPORARY;
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

void SCULPT_dyntopo_node_layers_add(SculptSession *ss)
{
  int cd_node_layer_index, cd_face_node_layer_index;

  int cd_origco_index, cd_origno_index, cd_origvcol_index = -1;
  bool have_vcol = CustomData_has_layer(&ss->bm->vdata, CD_PROP_COLOR);

  BMCustomLayerReq vlayers[] = {{CD_PAINT_MASK, NULL, 0},
                                {CD_DYNTOPO_VERT, NULL, CD_FLAG_TEMPORARY},
                                {CD_PROP_INT32, dyntopop_node_idx_layer_id, CD_FLAG_TEMPORARY}};

  BM_data_layers_ensure(ss->bm, &ss->bm->vdata, vlayers, 3);

  cd_face_node_layer_index = CustomData_get_named_layer_index(
      &ss->bm->pdata, CD_PROP_INT32, dyntopop_node_idx_layer_id);
  if (cd_face_node_layer_index == -1) {
    BM_data_layer_add_named(ss->bm, &ss->bm->pdata, CD_PROP_INT32, dyntopop_node_idx_layer_id);
  }

  // get indices again, as they might have changed after adding new layers
  cd_node_layer_index = CustomData_get_named_layer_index(
      &ss->bm->vdata, CD_PROP_INT32, dyntopop_node_idx_layer_id);
  cd_face_node_layer_index = CustomData_get_named_layer_index(
      &ss->bm->pdata, CD_PROP_INT32, dyntopop_node_idx_layer_id);

  ss->cd_origvcol_offset = -1;

  ss->cd_dyn_vert = CustomData_get_offset(&ss->bm->vdata, CD_DYNTOPO_VERT);

  ss->cd_vcol_offset = CustomData_get_offset(&ss->bm->vdata, CD_PROP_COLOR);

  ss->cd_vert_node_offset = CustomData_get_n_offset(
      &ss->bm->vdata,
      CD_PROP_INT32,
      cd_node_layer_index - CustomData_get_layer_index(&ss->bm->vdata, CD_PROP_INT32));

  ss->bm->vdata.layers[cd_node_layer_index].flag |= CD_FLAG_TEMPORARY;

  ss->cd_face_node_offset = CustomData_get_n_offset(
      &ss->bm->pdata,
      CD_PROP_INT32,
      cd_face_node_layer_index - CustomData_get_layer_index(&ss->bm->pdata, CD_PROP_INT32));

  ss->bm->pdata.layers[cd_face_node_layer_index].flag |= CD_FLAG_TEMPORARY;
  ss->cd_faceset_offset = CustomData_get_offset(&ss->bm->pdata, CD_SCULPT_FACE_SETS);
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

      for (int k = baseidx; k < data2->totlayer; k++) {
        CustomDataLayer *cl3 = data2->layers + k;

        if (cl3->type != cl2->type) {
          break;
        }

        // based off of how CustomData_set_layer_XXXX_index works

        cl3->active = (cl2->active + baseidx) - k;
        cl3->active_rnd = (cl2->active_rnd + baseidx) - k;
        cl3->active_mask = (cl2->active_mask + baseidx) - k;
        cl3->active_clone = (cl2->active_clone + baseidx) - k;
      }
    }

    BLI_array_free(newlayers);
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
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(me);

  SCULPT_pbvh_clear(ob);

  ss->bm_smooth_shading = (scene->toolsettings->sculpt->flags & SCULPT_DYNTOPO_SMOOTH_SHADING) !=
                          0;

  /* Dynamic topology doesn't ensure selection state is valid, so remove T36280. */
  BKE_mesh_mselect_clear(me);

  /* Create triangles-only BMesh. */
#if 1
  ss->bm = BM_mesh_create(&allocsize,
                          &((struct BMeshCreateParams){.use_toolflags = false,
                                                       .use_unique_ids = true,
                                                       .use_id_elem_mask = BM_VERT | BM_FACE,
                                                       .use_id_map = true}));

  BM_mesh_bm_from_me(NULL,
                     ss->bm,
                     me,
                     (&(struct BMeshFromMeshParams){
                         .calc_face_normal = true,
                         .use_shapekey = true,
                         .active_shapekey = ob->shapenr,
                     }));
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
  SCULPT_dynamic_topology_triangulate(ss, ss->bm);

  SCULPT_dyntopo_node_layers_add(ss);
  SCULPT_dyntopo_save_origverts(ss);

  BMIter iter;
  BMVert *v;
  int cd_vcol_offset = CustomData_get_offset(&ss->bm->vdata, CD_PROP_COLOR);

  int cd_pers_co = -1, cd_pers_no = -1, cd_pers_disp = -1;
  int cd_layer_disp = -1;

  // convert layer brush data
  if (ss->persistent_base) {
    BMCustomLayerReq layers[] = {{CD_PROP_FLOAT3, SCULPT_LAYER_PERS_CO, CD_FLAG_TEMPORARY},
                                 {CD_PROP_FLOAT3, SCULPT_LAYER_PERS_NO, CD_FLAG_TEMPORARY},
                                 {CD_PROP_FLOAT, SCULPT_LAYER_PERS_DISP, CD_FLAG_TEMPORARY},
                                 {CD_PROP_FLOAT, SCULPT_LAYER_DISP, CD_FLAG_TEMPORARY}};

    BM_data_layers_ensure(ss->bm, &ss->bm->vdata, layers, 4);

    cd_pers_co = SCULPT_dyntopo_get_templayer(ss, CD_PROP_FLOAT3, SCULPT_LAYER_PERS_CO);
    cd_pers_no = SCULPT_dyntopo_get_templayer(ss, CD_PROP_FLOAT3, SCULPT_LAYER_PERS_NO);
    cd_pers_disp = SCULPT_dyntopo_get_templayer(ss, CD_PROP_FLOAT, SCULPT_LAYER_PERS_DISP);
    cd_layer_disp = SCULPT_dyntopo_get_templayer(ss, CD_PROP_FLOAT, SCULPT_LAYER_DISP);

    SCULPT_dyntopo_node_layers_update_offsets(ss);

    cd_vcol_offset = CustomData_get_offset(&ss->bm->vdata, CD_PROP_COLOR);
  }
  else {
    cd_layer_disp = SCULPT_dyntopo_get_templayer(ss, CD_PROP_FLOAT, SCULPT_LAYER_PERS_DISP);
  }

  int i = 0;

  BM_ITER_MESH (v, &iter, ss->bm, BM_VERTS_OF_MESH) {
    MDynTopoVert *mv = BKE_PBVH_DYNVERT(ss->cd_dyn_vert, v);

    if (BM_vert_is_boundary(v)) {
      mv->flag |= DYNVERT_BOUNDARY;
    }

    // persistent base
    if (cd_pers_co >= 0) {
      float *co = BM_ELEM_CD_GET_VOID_P(v, cd_pers_co);
      float *no = BM_ELEM_CD_GET_VOID_P(v, cd_pers_no);
      float *disp = BM_ELEM_CD_GET_VOID_P(v, cd_pers_disp);

      copy_v3_v3(co, ss->persistent_base[i].co);
      copy_v3_v3(no, ss->persistent_base[i].no);
      *disp = ss->persistent_base[i].disp;
    }

    if (cd_layer_disp >= 0) {
      float *disp = BM_ELEM_CD_GET_VOID_P(v, cd_layer_disp);
      *disp = 0.0f;
    }

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
  ss->bm_log = BM_log_create(ss->bm, ss->cd_dyn_vert);

  /* Update dependency graph, so modifiers that depend on dyntopo being enabled
   * are re-evaluated and the PBVH is re-created. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

  // TODO: this line here is being slow, do we need it? - joeedh
  BKE_scene_graph_update_tagged(depsgraph, bmain);
}

void SCULPT_dyntopo_save_persistent_base(SculptSession *ss)
{
  int cd_pers_co = SCULPT_dyntopo_get_templayer(ss, CD_PROP_FLOAT3, SCULPT_LAYER_PERS_CO);
  int cd_pers_no = SCULPT_dyntopo_get_templayer(ss, CD_PROP_FLOAT3, SCULPT_LAYER_PERS_NO);
  int cd_pers_disp = SCULPT_dyntopo_get_templayer(ss, CD_PROP_FLOAT, SCULPT_LAYER_PERS_DISP);

  if (cd_pers_co >= 0) {
    BMIter iter;

    MEM_SAFE_FREE(ss->persistent_base);
    ss->persistent_base = MEM_callocN(sizeof(*ss->persistent_base) * ss->bm->totvert,
                                      "ss->persistent_base");
    BMVert *v;
    int i = 0;

    BM_ITER_MESH (v, &iter, ss->bm, BM_VERTS_OF_MESH) {
      float *co = BM_ELEM_CD_GET_VOID_P(v, cd_pers_co);
      float *no = BM_ELEM_CD_GET_VOID_P(v, cd_pers_no);
      float *disp = BM_ELEM_CD_GET_VOID_P(v, cd_pers_disp);

      copy_v3_v3(ss->persistent_base[i].co, co);
      copy_v3_v3(ss->persistent_base[i].no, no);
      ss->persistent_base[i].disp = *disp;

      i++;
    }
  }
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

  if (unode) {
    /* Free all existing custom data. */
    CustomData_free(&me->vdata, me->totvert);
    CustomData_free(&me->edata, me->totedge);
    CustomData_free(&me->fdata, me->totface);
    CustomData_free(&me->ldata, me->totloop);
    CustomData_free(&me->pdata, me->totpoly);

    /* Copy over stored custom data. */
    SculptUndoNodeGeometry *geometry = &unode->geometry_bmesh_enter;
    me->totvert = geometry->totvert;
    me->totloop = geometry->totloop;
    me->totpoly = geometry->totpoly;
    me->totedge = geometry->totedge;
    me->totface = 0;
    CustomData_copy(
        &geometry->vdata, &me->vdata, CD_MASK_MESH.vmask, CD_DUPLICATE, geometry->totvert);
    CustomData_copy(
        &geometry->edata, &me->edata, CD_MASK_MESH.emask, CD_DUPLICATE, geometry->totedge);
    CustomData_copy(
        &geometry->ldata, &me->ldata, CD_MASK_MESH.lmask, CD_DUPLICATE, geometry->totloop);
    CustomData_copy(
        &geometry->pdata, &me->pdata, CD_MASK_MESH.pmask, CD_DUPLICATE, geometry->totpoly);

    BKE_mesh_update_customdata_pointers(me, false);
  }
  else {
    BKE_sculptsession_bm_to_me(ob, true);

    /* Sync the visibility to vertices manually as the pmap is still not initialized. */
    for (int i = 0; i < me->totvert; i++) {
      me->mvert[i].flag &= ~ME_HIDE;
      me->mvert[i].flag |= ME_VERT_PBVH_UPDATE;
    }
  }

  /* Clear data. */
  me->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;

  bool disp_saved = false;

  if (ss->bm_log) {
    if (ss->bm) {
      disp_saved = true;

      // rebuild ss->persistent_base if necassary
      SCULPT_dyntopo_save_persistent_base(ss);
    }

    BM_log_free(ss->bm_log, true);
    ss->bm_log = NULL;
  }

  /* Typically valid but with global-undo they can be NULL, see: T36234. */
  if (ss->bm) {
    if (!disp_saved) {
      // rebuild ss->persistent_base if necassary
      SCULPT_dyntopo_save_persistent_base(ss);
    }

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

static int dyntopo_warning_popup(bContext *C, wmOperatorType *ot, enum eDynTopoWarnFlag flag)
{
  uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Warning!"), ICON_ERROR);
  uiLayout *layout = UI_popup_menu_layout(pup);

  if (flag & (DYNTOPO_WARN_VDATA | DYNTOPO_WARN_EDATA | DYNTOPO_WARN_LDATA)) {
    const char *msg_error = TIP_("Vertex Data Detected!");
    const char *msg = TIP_("Dyntopo will not preserve vertex colors, UVs, or other customdata");
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

#ifndef DYNTOPO_CD_INTERP
  for (int i = 0; i < CD_NUMTYPES; i++) {
    if (!ELEM(i, CD_MVERT, CD_MEDGE, CD_MFACE, CD_MLOOP, CD_MPOLY, CD_PAINT_MASK, CD_ORIGINDEX)) {
      if (CustomData_has_layer(&me->vdata, i)) {
        flag |= DYNTOPO_WARN_VDATA;
      }
      if (CustomData_has_layer(&me->edata, i)) {
        flag |= DYNTOPO_WARN_EDATA;
      }
      if (CustomData_has_layer(&me->ldata, i)) {
        flag |= DYNTOPO_WARN_LDATA;
      }
    }
  }
#endif

  {
    VirtualModifierData virtualModifierData;
    ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);

    /* Exception for shape keys because we can edit those. */
    for (; md; md = md->next) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);
      if (!BKE_modifier_is_enabled(scene, md, eModifierMode_Realtime)) {
        continue;
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

    if (flag) {
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
  ot->description = "Dynamic topology alters the mesh topology while sculpting";

  /* API callbacks. */
  ot->invoke = sculpt_dynamic_topology_toggle_invoke;
  ot->exec = sculpt_dynamic_topology_toggle_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
