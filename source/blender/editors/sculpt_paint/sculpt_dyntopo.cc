/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */

/** \file
 * \ingroup edsculpt
 */

#include <cmath>
#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_array.hh"
#include "BLI_blenlib.h"
#include "BLI_compiler_attrs.h"
#include "BLI_hash.h"
#include "BLI_index_range.hh"
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
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pbvh.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"

#include "BLI_index_range.hh"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_undo.h"
#include "sculpt_intern.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "../../bmesh/intern/bmesh_idmap.h"
#include "bmesh.h"
#include "bmesh_log.h"
#include "bmesh_tools.h"

#include <math.h>
#include <stdlib.h>

using blender::IndexRange;
using blender::int2;
using blender::Vector;

BMesh *SCULPT_dyntopo_empty_bmesh()
{
  return BKE_sculptsession_empty_bmesh_create();
}
// TODO: check if (mathematically speaking) is it really necassary
// to sort the edge lists around verts

// from http://rodolphe-vaillant.fr/?e=20
static float tri_voronoi_area(const float p[3], const float q[3], const float r[3])
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

void SCULPT_dyntopo_get_cotangents(SculptSession *ss,
                                   PBVHVertRef vertex,
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
  float totarea = 0.0f;
  // float totw = 0.0f;

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
    // totw += r_ws[i];

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
                                 PBVHVertRef vertex,
                                 float *r_ws,
                                 float *r_cot1,
                                 float *r_cot2,
                                 float *r_area,
                                 float *r_totarea)
{
  // sculpt vemap should always be sorted in disk cycle order

  float totarea = 0.0;

  Span<int> elem = ss->vemap[vertex.i];
  const int count = elem.size();

  for (int i = 0; i < count; i++) {
    int i1 = (i + count - 1) % count;
    int i2 = i;
    int i3 = (i + 1) % count;

    const float *v = ss->vert_positions[vertex.i];
    const int2 &e1 = ss->edges[elem[i1]];
    const int2 &e2 = ss->edges[elem[i2]];
    const int2 &e3 = ss->edges[elem[i3]];

    const float *v1 = (unsigned int)vertex.i == e1[0] ? ss->vert_positions[e1[1]] :
                                                        ss->vert_positions[e1[0]];
    const float *v2 = (unsigned int)vertex.i == e2[0] ? ss->vert_positions[e2[1]] :
                                                        ss->vert_positions[e2[0]];
    const float *v3 = (unsigned int)vertex.i == e3[0] ? ss->vert_positions[e3[1]] :
                                                        ss->vert_positions[e3[0]];

    float cot1 = cotangent_tri_weight_v3(v1, v, v2);
    float cot2 = cotangent_tri_weight_v3(v3, v2, v);

    float area = tri_voronoi_area(v, v1, v2);

    r_ws[i] = (cot1 + cot2);

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

  for (int i = 0; i < count; i++) {
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
        PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);
        SCULPT_dyntopo_check_disk_sort(ss, vertex);
      }
      break;
    }
    case PBVH_FACES: {
      Mesh *mesh = BKE_object_get_original_mesh(ob);

      SCULPT_ensure_vemap(ss);

      break;
    }
    case PBVH_GRIDS:  // not supported yet
      break;
  }
}

void SCULPT_get_cotangents(SculptSession *ss,
                           PBVHVertRef vertex,
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
        /* Not supported, return uniform weights. */

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

/* Returns true if edge disk list around vertex was sorted. */
bool SCULPT_dyntopo_check_disk_sort(SculptSession *ss, PBVHVertRef vertex)
{
  BMVert *v = (BMVert *)vertex.i;
  uint8_t *flag = blender::bke::paint::vertex_attr_ptr<uint8_t>(vertex, ss->attrs.flags);

  if (*flag & SCULPTFLAG_NEED_DISK_SORT) {
    *flag &= ~SCULPTFLAG_NEED_DISK_SORT;

    BM_sort_disk_cycle(v);

    return true;
  }

  return false;
}

/*
Copies the bmesh, but orders the elements
according to PBVH node to improve memory locality
*/
void SCULPT_reorder_bmesh(SculptSession * /*ss*/)
{
#if 0
  SCULPT_face_random_access_ensure(ss);
  SCULPT_vertex_random_access_ensure(ss);

  int actv = ss->active_vertex.i ?
                 BKE_pbvh_vertex_to_index(ss->pbvh, ss->active_vertex) :
                 -1;
  int actf = ss->active_face.i ?
                 BKE_pbvh_face_to_index(ss->pbvh, ss->active_face) :
                 -1;

  if (ss->bm_log) {
    BM_log_full_mesh(ss->bm, ss->bm_log);
  }

  ss->bm = BKE_pbvh_reorder_bmesh(ss->pbvh);

  SCULPT_face_random_access_ensure(ss);
  SCULPT_vertex_random_access_ensure(ss);

  if (actv >= 0) {
    ss->active_vertex = BKE_pbvh_index_to_vertex(ss->pbvh, actv);
  }
  if (actf >= 0) {
    ss->active_face = BKE_pbvh_index_to_face(ss->pbvh, actf);
  }

  BKE_sculptsession_update_attr_refs(ob);

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
  LinkNode *f_double = nullptr;

  Vector<BMFace *>(faces_array);

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    if (f->len <= 3) {
      continue;
    }

    bool sel = BM_elem_flag_test(f, BM_ELEM_SELECT);

    int faces_array_tot = f->len;
    faces_array.resize(faces_array_tot);

    BM_face_triangulate(bm,
                        f,
                        faces_array.data(),
                        &faces_array_tot,
                        nullptr,
                        nullptr,
                        &f_double,
                        MOD_TRIANGULATE_QUAD_BEAUTY,
                        MOD_TRIANGULATE_NGON_EARCLIP,
                        true,
                        pf_arena,
                        nullptr);

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
    BM_face_kill(bm, (BMFace *)f_double->link);
    MEM_freeN(f_double);
    f_double = next;
  }

  BLI_memarena_free(pf_arena);

  ss->totfaces = ss->totpoly = ss->bm->totface;
  ss->totvert = ss->bm->totvert;
}

void SCULPT_pbvh_clear(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  ss->pmap = {};
  ss->vert_to_poly_indices = {};
  ss->vert_to_poly_offsets = {};
  ss->epmap = {};
  ss->edge_to_poly_indices = {};
  ss->edge_to_poly_offsets = {};
  ss->vemap = {};
  ss->vert_to_edge_indices = {};
  ss->vert_to_edge_offsets = {};

  /* Clear out any existing DM and PBVH. */
  if (ss->pbvh) {
    BKE_pbvh_free(ss->pbvh);
    ss->pbvh = nullptr;
  }

  BKE_object_free_derived_caches(ob);

  /* Tag to rebuild PBVH in depsgraph. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
}

/**
 * Syncs customdata layers with internal bmesh, but ignores deleted layers.
 */
void SCULPT_dynamic_topology_sync_layers(Object *ob, Mesh *me)
{
  BKE_sculptsession_sync_attributes(ob, me);
}

static void customdata_strip_templayers(CustomData *cdata, int totelem)
{
  for (int i = 0; i < cdata->totlayer; i++) {
    CustomDataLayer *layer = cdata->layers + i;

    if (layer->flag & CD_FLAG_TEMPORARY) {
      CustomData_free_layer(cdata, eCustomDataType(layer->type), totelem, i);
      i--;
    }
  }
}

void SCULPT_dynamic_topology_enable_ex(Main *bmain,
                                       Depsgraph *depsgraph,
                                       Scene * /*scene*/,
                                       Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = BKE_object_get_original_mesh(ob);

  customdata_strip_templayers(&me->vdata, me->totvert);
  customdata_strip_templayers(&me->pdata, me->totpoly);

  if (!ss->pmap.is_empty()) {
    ss->pmap = {};
    ss->vert_to_poly_indices = {};
    ss->vert_to_poly_offsets = {};
    ss->epmap = {};
    ss->edge_to_poly_indices = {};
    ss->edge_to_poly_offsets = {};
    ss->vemap = {};
    ss->vert_to_edge_indices = {};
    ss->vert_to_edge_offsets = {};
  }

  if (!ss->bm || !ss->pbvh || BKE_pbvh_type(ss->pbvh) != PBVH_BMESH) {
    SCULPT_pbvh_clear(ob);
  }

  PBVH *pbvh = ss->pbvh;

  if (pbvh) {
    BMesh *bm = BKE_pbvh_get_bmesh(pbvh);

    if (!ss->bm) {
      ss->bm = bm;
    }
    else if (ss->bm != bm) {
      printf("%s: bmesh differed!\n", __func__);
      SCULPT_pbvh_clear(ob);
    }
  }

  /* Dynamic topology doesn't ensure selection state is valid, so remove #36280. */
  BKE_mesh_mselect_clear(me);
  bool tag_update = false;

  if (!ss->bm) {
    ss->bm = BKE_sculptsession_empty_bmesh_create();

    BMeshFromMeshParams params = {};
    params.use_shapekey = true;
    params.create_shapekey_layers = true;
    params.active_shapekey = ob->shapenr;

    BM_mesh_bm_from_me(ss->bm, me, &params);
    tag_update = true;
  }

#ifndef DYNTOPO_DYNAMIC_TESS
  SCULPT_dynamic_topology_triangulate(ss, ss->bm);
#endif

  if (ss->pbvh) {
    BKE_sculptsession_update_attr_refs(ob);
  }

  /* XXX Delete this block of code? Might be old fake quadrangulation edge hiding. */
  BMIter iter;
  BMEdge *e;
  BM_ITER_MESH (e, &iter, ss->bm, BM_EDGES_OF_MESH) {
    e->head.hflag |= BM_ELEM_DRAW;
  }

  /* Calculate normals. */
  BM_mesh_normals_update(ss->bm);

  /* Enable dynamic topology. */
  me->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;

  if (!ss->bm_idmap) {
    ss->bm_idmap = BM_idmap_new(ss->bm, BM_VERT | BM_EDGE | BM_FACE);
    BM_idmap_check_ids(ss->bm_idmap);
    BKE_sculptsession_update_attr_refs(ob);
  }

  /* Enable logging for undo/redo. */
  if (!ss->bm_log) {
    ss->bm_log = BM_log_create(ss->bm, ss->bm_idmap);
  }

  tag_update |= !ss->pbvh || BKE_pbvh_type(ss->pbvh) != PBVH_BMESH;

  /* Update dependency graph, so modifiers that depend on dyntopo being enabled
   * are re-evaluated and the PBVH is re-created. */
  if (tag_update) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    BKE_scene_graph_update_tagged(depsgraph, bmain);
  }

  /* ss->pbvh should exist by this point. */

  if (ss->pbvh) {
    BKE_sculpt_ensure_sculpt_layers(ob);
    BKE_sculpt_ensure_origco(ob);
    blender::bke::paint::load_all_original(ob);
  }

  SCULPT_update_all_valence_boundary(ob);

  if (ss->pbvh && SCULPT_has_persistent_base(ss)) {
    SCULPT_ensure_persistent_layers(ss, ob);
  }

  if (!CustomData_has_layer(&ss->bm->vdata, CD_PAINT_MASK)) {
    BM_data_layer_add(ss->bm, &ss->bm->vdata, CD_PAINT_MASK);
    BKE_sculptsession_update_attr_refs(ob);
  }
}

void SCULPT_update_all_valence_boundary(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  /* Do bmesh seperately to avoid needing the PBVH, which we might not
   * have inside the undo code.
   */

  if (ss->bm) {
    SCULPT_vertex_random_access_ensure(ss);
    BMIter iter;
    BMVert *v;

    int cd_flag = CustomData_get_offset_named(
        &ss->bm->vdata, CD_PROP_INT8, SCULPT_ATTRIBUTE_NAME(flags));
    int cd_boundary = CustomData_get_offset_named(
        &ss->bm->vdata, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(boundary_flags));
    int cd_valence = CustomData_get_offset_named(
        &ss->bm->vdata, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(valence));

    BLI_assert(cd_flag != -1 && cd_boundary != -1 && cd_valence != -1);

    BM_ITER_MESH (v, &iter, ss->bm, BM_VERTS_OF_MESH) {
      *BM_ELEM_CD_PTR<uint8_t *>(v, cd_flag) = SCULPTFLAG_NEED_DISK_SORT |
                                               SCULPTFLAG_NEED_TRIANGULATE;
      BM_ELEM_CD_SET_INT(v, cd_valence, BM_vert_edge_count(v));
      BM_ELEM_CD_SET_INT(v, cd_boundary, SCULPT_BOUNDARY_NEEDS_UPDATE);

      /* Update boundary if we have a pbvh. */
      if (ss->pbvh) {
        PBVHVertRef vertex = {reinterpret_cast<intptr_t>(v)};
        SCULPT_vertex_is_boundary(ss, vertex, SCULPT_BOUNDARY_ALL);
      }
    }

    return;
  }
  if (!ss->pbvh) {
    return;
  }

  int verts_count = SCULPT_vertex_count_get(ss);
  for (int i = 0; i < verts_count; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    blender::bke::paint::vertex_attr_set<int>(
        vertex,
        ss->attrs.flags,
        SCULPTFLAG_NEED_VALENCE | SCULPTFLAG_NEED_TRIANGULATE | SCULPTFLAG_NEED_DISK_SORT);
    BKE_sculpt_boundary_flag_update(ss, vertex);
    SCULPT_vertex_valence_get(ss, vertex);
    SCULPT_vertex_is_boundary(ss, vertex, SCULPT_BOUNDARY_ALL);
  }
}

/* Free the sculpt BMesh and BMLog
 *
 * If 'unode' is given, the BMesh's data is copied out to the unode
 * before the BMesh is deleted so that it can be restored from. */
static void SCULPT_dynamic_topology_disable_ex(
    Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob, SculptUndoNode * /*unode*/)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = static_cast<Mesh *>(ob->data);

  /* Destroy temporary layers. */
  BKE_sculpt_attribute_destroy_temporary_all(ob);

  if (ss->attrs.dyntopo_node_id_vertex) {
    BKE_sculpt_attribute_destroy(ob, ss->attrs.dyntopo_node_id_vertex);
  }

  if (ss->attrs.dyntopo_node_id_face) {
    BKE_sculpt_attribute_destroy(ob, ss->attrs.dyntopo_node_id_face);
  }

  BKE_sculptsession_update_attr_refs(ob);
  BKE_sculptsession_bm_to_me(ob, true);
  SCULPT_pbvh_clear(ob);

  /* Sync the visibility to vertices manually as the pmap is still not initialized. */
  bool *hide_vert = (bool *)CustomData_get_layer_named_for_write(
      &me->vdata, CD_PROP_BOOL, ".hide_vert", me->totvert);
  if (hide_vert != nullptr) {
    memset(static_cast<void *>(hide_vert), 0, sizeof(bool) * me->totvert);
  }

  /* Clear data. */
  me->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;

  if (ss->bm_idmap) {
    BM_idmap_destroy(ss->bm_idmap);
    ss->bm_idmap = nullptr;
  }

  if (ss->bm_log) {
    BM_log_free(ss->bm_log);
    ss->bm_log = nullptr;
  }

  /* Typically valid but with global-undo they can be nullptr, see: T36234. */
  if (ss->bm) {
    BM_mesh_free(ss->bm);
    ss->bm = nullptr;
  }

  SCULPT_pbvh_clear(ob);

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
  if (ss->bm != nullptr) {
    /* May be false in background mode. */
    const bool use_undo = G.background ? (ED_undo_stack_get() != nullptr) : true;
    if (use_undo) {
      SCULPT_undo_push_begin_ex(ob, "Dynamic topology disable");
      SCULPT_undo_push_node(ob, nullptr, SCULPT_UNDO_DYNTOPO_END);
    }
    SCULPT_dynamic_topology_disable_ex(bmain, depsgraph, scene, ob, nullptr);
    if (use_undo) {
      SCULPT_undo_push_end(ob);
    }

    ss->active_vertex.i = ss->active_face.i = 0;
  }
}

static void sculpt_dynamic_topology_enable_with_undo(Main *bmain,
                                                     Depsgraph *depsgraph,
                                                     Scene *scene,
                                                     Object *ob)
{
  SculptSession *ss = ob->sculpt;

  if (ss->bm == nullptr) {
    /* May be false in background mode. */
    const bool use_undo = G.background ? (ED_undo_stack_get() != nullptr) : true;
    if (use_undo) {
      SCULPT_undo_push_begin_ex(ob, "Dynamic topology enable");
    }
    SCULPT_dynamic_topology_enable_ex(bmain, depsgraph, scene, ob);
    if (use_undo) {
      SCULPT_undo_push_node(ob, nullptr, SCULPT_UNDO_DYNTOPO_BEGIN);
      SCULPT_undo_push_end(ob);
    }

    ss->active_vertex.i = ss->active_face.i = 0;
  }
}

static int sculpt_dynamic_topology_toggle_exec(bContext *C, wmOperator * /*op*/)
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
  WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, nullptr);

  return OPERATOR_FINISHED;
}

static int dyntopo_error_popup(bContext *C, wmOperatorType * /*ot*/, enum eDynTopoWarnFlag flag)
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

  uiItemFullO_ptr(layout, ot, IFACE_("OK"), ICON_NONE, nullptr, WM_OP_EXEC_DEFAULT, 0, nullptr);

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

enum eDynTopoWarnFlag SCULPT_dynamic_topology_check(Scene *scene, Object *ob)
{
  SculptSession *ss = ob->sculpt;

  enum eDynTopoWarnFlag flag = eDynTopoWarnFlag(0);

  BLI_assert(ss->bm == nullptr);
  UNUSED_VARS_NDEBUG(ss);

  {
    VirtualModifierData virtualModifierData;
    ModifierData *md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);

    /* Exception for shape keys because we can edit those. */
    for (; md; md = md->next) {
      const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(md->type));
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
                                                 const wmEvent * /*event*/)
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
