/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2004 Blender Foundation */

/** \file
 * \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_alloca.h"
#include "BLI_buffer.h"
#include "BLI_kdtree.h"
#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_DerivedMesh.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_bvh.h"
#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_pbvh.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"

#include "BKE_object.h" /* XXX. only for EDBM_mesh_load(). */

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_transform_snap_object_context.h"
#include "ED_uvedit.h"
#include "ED_view3d.h"

#include "mesh_intern.h" /* own include */

/* -------------------------------------------------------------------- */
/** \name Redo API
 * \{ */

/* Mesh backup implementation.
 * This would greatly benefit from some sort of binary diffing
 * just as the undo stack would.
 * So leaving this as an interface for further work */

BMBackup EDBM_redo_state_store(BMEditMesh *em)
{
  BMBackup backup;
  backup.bmcopy = BM_mesh_copy(em->bm);
  return backup;
}

void EDBM_redo_state_restore(BMBackup *backup, BMEditMesh *em, bool recalc_looptri)
{
  BM_mesh_data_free(em->bm);
  BMesh *tmpbm = BM_mesh_copy(backup->bmcopy);
  *em->bm = *tmpbm;
  MEM_freeN(tmpbm);
  tmpbm = NULL;

  if (recalc_looptri) {
    BKE_editmesh_looptri_calc(em);
  }
}

void EDBM_redo_state_restore_and_free(BMBackup *backup, BMEditMesh *em, bool recalc_looptri)
{
  BM_mesh_data_free(em->bm);
  *em->bm = *backup->bmcopy;
  MEM_freeN(backup->bmcopy);
  backup->bmcopy = NULL;
  if (recalc_looptri) {
    BKE_editmesh_looptri_calc(em);
  }
}

void EDBM_redo_state_free(BMBackup *backup)
{
  if (backup->bmcopy) {
    BM_mesh_data_free(backup->bmcopy);
    MEM_freeN(backup->bmcopy);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BMesh Operator (BMO) API Wrapper
 * \{ */

bool EDBM_op_init(BMEditMesh *em, BMOperator *bmop, wmOperator *op, const char *fmt, ...)
{
  BMesh *bm = em->bm;
  va_list list;

  va_start(list, fmt);

  if (!BMO_op_vinitf(bm, bmop, BMO_FLAG_DEFAULTS, fmt, list)) {
    BKE_reportf(op->reports, RPT_ERROR, "Parse error in %s", __func__);
    va_end(list);
    return false;
  }

  va_end(list);

  return true;
}

bool EDBM_op_finish(BMEditMesh *em, BMOperator *bmop, wmOperator *op, const bool do_report)
{
  const char *errmsg;

#ifndef NDEBUG
  struct {
    int verts_len, edges_len, loops_len, faces_len;
  } em_state_prev = {
      .verts_len = em->bm->totvert,
      .edges_len = em->bm->totedge,
      .loops_len = em->bm->totloop,
      .faces_len = em->bm->totface,
  };
#endif

  BMO_op_finish(em->bm, bmop);

  bool changed = false;
  bool changed_was_set = false;

  eBMOpErrorLevel level;
  while (BMO_error_pop(em->bm, &errmsg, NULL, &level)) {
    eReportType type = RPT_INFO;
    switch (level) {
      case BMO_ERROR_CANCEL: {
        changed_was_set = true;
        break;
      }
      case BMO_ERROR_WARN: {
        type = RPT_WARNING;
        changed_was_set = true;
        changed = true;
        break;
      }
      case BMO_ERROR_FATAL: {
        type = RPT_ERROR;
        changed_was_set = true;
        changed = true;
        break;
      }
    }

    if (do_report) {
      BKE_report(op->reports, type, errmsg);
    }
  }
  if (changed_was_set == false) {
    changed = true;
  }

#ifndef NDEBUG
  if (changed == false) {
    BLI_assert((em_state_prev.verts_len == em->bm->totvert) &&
               (em_state_prev.edges_len == em->bm->totedge) &&
               (em_state_prev.loops_len == em->bm->totloop) &&
               (em_state_prev.faces_len == em->bm->totface));
  }
#endif

  return changed;
}

bool EDBM_op_callf(BMEditMesh *em, wmOperator *op, const char *fmt, ...)
{
  BMesh *bm = em->bm;
  BMOperator bmop;
  va_list list;

  va_start(list, fmt);

  if (!BMO_op_vinitf(bm, &bmop, BMO_FLAG_DEFAULTS, fmt, list)) {
    BKE_reportf(op->reports, RPT_ERROR, "Parse error in %s", __func__);
    va_end(list);
    return false;
  }

  BMO_op_exec(bm, &bmop);

  va_end(list);
  return EDBM_op_finish(em, &bmop, op, true);
}

bool EDBM_op_call_and_selectf(BMEditMesh *em,
                              wmOperator *op,
                              const char *select_slot_out,
                              const bool select_extend,
                              const char *fmt,
                              ...)
{
  BMesh *bm = em->bm;
  BMOperator bmop;
  va_list list;

  va_start(list, fmt);

  if (!BMO_op_vinitf(bm, &bmop, BMO_FLAG_DEFAULTS, fmt, list)) {
    BKE_reportf(op->reports, RPT_ERROR, "Parse error in %s", __func__);
    va_end(list);
    return false;
  }

  BMO_op_exec(bm, &bmop);

  BMOpSlot *slot_select_out = BMO_slot_get(bmop.slots_out, select_slot_out);
  char hflag = slot_select_out->slot_subtype.elem & BM_ALL_NOLOOP;
  BLI_assert(hflag != 0);

  if (select_extend == false) {
    BM_mesh_elem_hflag_disable_all(em->bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_SELECT, false);
  }

  BMO_slot_buffer_hflag_enable(
      em->bm, bmop.slots_out, select_slot_out, hflag, BM_ELEM_SELECT, true);

  va_end(list);
  return EDBM_op_finish(em, &bmop, op, true);
}

bool EDBM_op_call_silentf(BMEditMesh *em, const char *fmt, ...)
{
  BMesh *bm = em->bm;
  BMOperator bmop;
  va_list list;

  va_start(list, fmt);

  if (!BMO_op_vinitf(bm, &bmop, BMO_FLAG_DEFAULTS, fmt, list)) {
    va_end(list);
    return false;
  }

  BMO_op_exec(bm, &bmop);

  va_end(list);
  return EDBM_op_finish(em, &bmop, NULL, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit BMesh API
 *
 * Make/Clear/Free functions.
 * \{ */

void EDBM_mesh_make(Object *ob, const int select_mode, const bool add_key_index)
{
  Mesh *me = ob->data;
  BMesh *bm = BKE_mesh_to_bmesh(me,
                                ob,
                                add_key_index,
                                &((struct BMeshCreateParams){
                                    .use_toolflags = true,
                                }));

  if (me->edit_mesh) {
    /* this happens when switching shape keys */
    EDBM_mesh_free_data(me->edit_mesh);
    MEM_freeN(me->edit_mesh);
  }

  /* Executing operators re-tessellates,
   * so we can avoid doing here but at some point it may need to be added back. */
  me->edit_mesh = BKE_editmesh_create(bm);

  me->edit_mesh->selectmode = me->edit_mesh->bm->selectmode = select_mode;
  me->edit_mesh->mat_nr = (ob->actcol > 0) ? ob->actcol - 1 : 0;

  /* we need to flush selection because the mode may have changed from when last in editmode */
  EDBM_selectmode_flush(me->edit_mesh);
}

void EDBM_mesh_load_ex(Main *bmain, Object *ob, bool free_data)
{
  Mesh *me = ob->data;
  BMesh *bm = me->edit_mesh->bm;

  /* Workaround for #42360, 'ob->shapenr' should be 1 in this case.
   * however this isn't synchronized between objects at the moment. */
  if (UNLIKELY((ob->shapenr == 0) && (me->key && !BLI_listbase_is_empty(&me->key->block)))) {
    bm->shapenr = 1;
  }

  BM_mesh_bm_to_me(bmain,
                   bm,
                   me,
                   (&(struct BMeshToMeshParams){
                       .calc_object_remap = true,
                       .update_shapekey_indices = !free_data,
                   }));
}

void EDBM_mesh_clear(BMEditMesh *em)
{
  /* clear bmesh */
  BM_mesh_clear(em->bm);

  /* free tessellation data */
  em->tottri = 0;
  MEM_SAFE_FREE(em->looptris);
}

void EDBM_mesh_load(Main *bmain, Object *ob)
{
  EDBM_mesh_load_ex(bmain, ob, true);
}

void EDBM_mesh_free_data(BMEditMesh *em)
{
  /* These tables aren't used yet, so it's not strictly necessary
   * to 'end' them but if someone tries to start using them,
   * having these in place will save a lot of pain. */
  ED_mesh_mirror_spatial_table_end(NULL);
  ED_mesh_mirror_topo_table_end(NULL);

  BKE_editmesh_free_data(em);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Selection Utilities
 * \{ */

void EDBM_selectmode_to_scene(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  Object *obedit = CTX_data_edit_object(C);
  BMEditMesh *em = BKE_editmesh_from_object(obedit);

  if (!em) {
    return;
  }

  scene->toolsettings->selectmode = em->selectmode;

  /* Request redraw of header buttons (to show new select mode) */
  WM_event_add_notifier(C, NC_SCENE | ND_TOOLSETTINGS, scene);
}

void EDBM_selectmode_flush_ex(BMEditMesh *em, const short selectmode)
{
  BM_mesh_select_mode_flush_ex(em->bm, selectmode, BM_SELECT_LEN_FLUSH_RECALC_ALL);
}

void EDBM_selectmode_flush(BMEditMesh *em)
{
  EDBM_selectmode_flush_ex(em, em->selectmode);
}

void EDBM_deselect_flush(BMEditMesh *em)
{
  /* function below doesn't use. just do this to keep the values in sync */
  em->bm->selectmode = em->selectmode;
  BM_mesh_deselect_flush(em->bm);
}

void EDBM_select_flush(BMEditMesh *em)
{
  /* function below doesn't use. just do this to keep the values in sync */
  em->bm->selectmode = em->selectmode;
  BM_mesh_select_flush(em->bm);
}

void EDBM_select_more(BMEditMesh *em, const bool use_face_step)
{
  BMOperator bmop;
  const bool use_faces = (em->selectmode == SCE_SELECT_FACE);

  BMO_op_initf(em->bm,
               &bmop,
               BMO_FLAG_DEFAULTS,
               "region_extend geom=%hvef use_contract=%b use_faces=%b use_face_step=%b",
               BM_ELEM_SELECT,
               false,
               use_faces,
               use_face_step);
  BMO_op_exec(em->bm, &bmop);
  /* Don't flush selection in edge/vertex mode. */
  BMO_slot_buffer_hflag_enable(
      em->bm, bmop.slots_out, "geom.out", BM_ALL_NOLOOP, BM_ELEM_SELECT, use_faces ? true : false);
  BMO_op_finish(em->bm, &bmop);

  EDBM_selectmode_flush(em);
}

void EDBM_select_less(BMEditMesh *em, const bool use_face_step)
{
  BMOperator bmop;
  const bool use_faces = (em->selectmode == SCE_SELECT_FACE);

  BMO_op_initf(em->bm,
               &bmop,
               BMO_FLAG_DEFAULTS,
               "region_extend geom=%hvef use_contract=%b use_faces=%b use_face_step=%b",
               BM_ELEM_SELECT,
               true,
               use_faces,
               use_face_step);
  BMO_op_exec(em->bm, &bmop);
  /* Don't flush selection in edge/vertex mode. */
  BMO_slot_buffer_hflag_disable(
      em->bm, bmop.slots_out, "geom.out", BM_ALL_NOLOOP, BM_ELEM_SELECT, use_faces ? true : false);
  BMO_op_finish(em->bm, &bmop);

  EDBM_selectmode_flush(em);

  /* only needed for select less, ensure we don't have isolated elements remaining */
  BM_mesh_select_mode_clean(em->bm);
}

void EDBM_flag_disable_all(BMEditMesh *em, const char hflag)
{
  BM_mesh_elem_hflag_disable_all(em->bm, BM_VERT | BM_EDGE | BM_FACE, hflag, false);
}

void EDBM_flag_enable_all(BMEditMesh *em, const char hflag)
{
  BM_mesh_elem_hflag_enable_all(em->bm, BM_VERT | BM_EDGE | BM_FACE, hflag, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Vertex Map API
 * \{ */

UvVertMap *BM_uv_vert_map_create(BMesh *bm, const bool use_select)
{
  /* NOTE: delimiting on alternate face-winding was once supported and could be useful
   * in some cases. If this is need see: D17137 to restore support. */
  BMVert *ev;
  BMFace *efa;
  BMLoop *l;
  BMIter iter, liter;
  uint a;
  const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_PROP_FLOAT2);

  BM_mesh_elem_index_ensure(bm, BM_VERT | BM_FACE);

  const int totverts = bm->totvert;
  int totuv = 0;

  /* generate UvMapVert array */
  BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
    if ((use_select == false) || BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
      totuv += efa->len;
    }
  }

  if (totuv == 0) {
    return NULL;
  }
  UvVertMap *vmap = (UvVertMap *)MEM_callocN(sizeof(*vmap), "UvVertMap");
  if (!vmap) {
    return NULL;
  }

  vmap->vert = (UvMapVert **)MEM_callocN(sizeof(*vmap->vert) * totverts, "UvMapVert_pt");
  UvMapVert *buf = vmap->buf = (UvMapVert *)MEM_callocN(sizeof(*vmap->buf) * totuv, "UvMapVert");

  if (!vmap->vert || !vmap->buf) {
    BKE_mesh_uv_vert_map_free(vmap);
    return NULL;
  }

  BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, a) {
    if ((use_select == false) || BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
      int i;
      BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
        buf->loop_of_poly_index = i;
        buf->poly_index = a;
        buf->separate = 0;

        buf->next = vmap->vert[BM_elem_index_get(l->v)];
        vmap->vert[BM_elem_index_get(l->v)] = buf;
        buf++;
      }
    }
  }

  /* sort individual uvs for each vert */
  BM_ITER_MESH_INDEX (ev, &iter, bm, BM_VERTS_OF_MESH, a) {
    UvMapVert *newvlist = NULL, *vlist = vmap->vert[a];
    UvMapVert *iterv, *v, *lastv, *next;
    const float *uv, *uv2;

    while (vlist) {
      v = vlist;
      vlist = vlist->next;
      v->next = newvlist;
      newvlist = v;

      efa = BM_face_at_index(bm, v->poly_index);

      l = BM_iter_at_index(bm, BM_LOOPS_OF_FACE, efa, v->loop_of_poly_index);
      uv = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);

      lastv = NULL;
      iterv = vlist;

      while (iterv) {
        next = iterv->next;
        efa = BM_face_at_index(bm, iterv->poly_index);
        l = BM_iter_at_index(bm, BM_LOOPS_OF_FACE, efa, iterv->loop_of_poly_index);
        uv2 = BM_ELEM_CD_GET_FLOAT_P(l, cd_loop_uv_offset);

        if (compare_v2v2(uv2, uv, STD_UV_CONNECT_LIMIT)) {
          if (lastv) {
            lastv->next = next;
          }
          else {
            vlist = next;
          }
          iterv->next = newvlist;
          newvlist = iterv;
        }
        else {
          lastv = iterv;
        }

        iterv = next;
      }

      newvlist->separate = 1;
    }

    vmap->vert[a] = newvlist;
  }

  return vmap;
}

UvMapVert *BM_uv_vert_map_at_index(UvVertMap *vmap, uint v)
{
  return vmap->vert[v];
}

struct UvElement **BM_uv_element_map_ensure_head_table(struct UvElementMap *element_map)
{
  if (element_map->head_table) {
    return element_map->head_table;
  }

  /* For each UvElement, locate the "separate" UvElement that precedes it in the linked list. */
  element_map->head_table = MEM_mallocN(sizeof(*element_map->head_table) * element_map->total_uvs,
                                        "uv_element_map_head_table");
  UvElement **head_table = element_map->head_table;
  for (int i = 0; i < element_map->total_uvs; i++) {
    UvElement *head = element_map->storage + i;
    if (head->separate) {
      UvElement *element = head;
      while (element) {
        head_table[element - element_map->storage] = head;
        element = element->next;
        if (element && element->separate) {
          break;
        }
      }
    }
  }
  return element_map->head_table;
}

int *BM_uv_element_map_ensure_unique_index(struct UvElementMap *element_map)
{
  if (!element_map->unique_index_table) {
    element_map->unique_index_table = MEM_callocN(
        element_map->total_uvs * sizeof(*element_map->unique_index_table), __func__);

    int j = 0;
    for (int i = 0; i < element_map->total_uvs; i++) {
      UvElement *element = element_map->storage + i;
      if (!element->separate) {
        continue;
      }
      BLI_assert(0 <= j);
      BLI_assert(j < element_map->total_unique_uvs);
      while (element) {
        element_map->unique_index_table[element - element_map->storage] = j;
        element = element->next;
        if (!element || element->separate) {
          break;
        }
      }
      j++;
    }
    BLI_assert(j == element_map->total_unique_uvs);
  }

  return element_map->unique_index_table;
}

int BM_uv_element_get_unique_index(struct UvElementMap *element_map, struct UvElement *child)
{
  int *unique_index = BM_uv_element_map_ensure_unique_index(element_map);
  int index = child - element_map->storage;
  BLI_assert(0 <= index);
  BLI_assert(index < element_map->total_uvs);
  return unique_index[index];
}

#define INVALID_ISLAND ((uint)-1)

static void bm_uv_assign_island(UvElementMap *element_map,
                                UvElement *element,
                                int nisland,
                                uint *map,
                                UvElement *islandbuf,
                                int islandbufsize)
{
  element->island = nisland;
  map[element - element_map->storage] = islandbufsize;

  /* Copy *element to islandbuf[islandbufsize]. */
  islandbuf[islandbufsize].l = element->l;
  islandbuf[islandbufsize].separate = element->separate;
  islandbuf[islandbufsize].loop_of_poly_index = element->loop_of_poly_index;
  islandbuf[islandbufsize].island = element->island;
  islandbuf[islandbufsize].flag = element->flag;
}

static int bm_uv_edge_select_build_islands(UvElementMap *element_map,
                                           const Scene *scene,
                                           UvElement *islandbuf,
                                           uint *map,
                                           bool uv_selected,
                                           const BMUVOffsets offsets)
{
  BM_uv_element_map_ensure_head_table(element_map);

  int total_uvs = element_map->total_uvs;

  /* Depth first search the graph, building islands as we go. */
  int nislands = 0;
  int islandbufsize = 0;
  int stack_upper_bound = total_uvs;
  UvElement **stack_uv = MEM_mallocN(sizeof(*stack_uv) * stack_upper_bound,
                                     "uv_island_element_stack");
  int stacksize_uv = 0;
  for (int i = 0; i < total_uvs; i++) {
    UvElement *element = element_map->storage + i;
    if (element->island != INVALID_ISLAND) {
      /* Unique UV (element and all it's children) are already part of an island. */
      continue;
    }

    /* Create a new island, i.e. nislands++. */

    BLI_assert(element->separate); /* Ensure we're the head of this unique UV. */

    /* Seed the graph search. */
    stack_uv[stacksize_uv++] = element;
    while (element) {
      bm_uv_assign_island(element_map, element, nislands, map, islandbuf, islandbufsize++);
      element = element->next;
      if (element && element->separate) {
        break;
      }
    }

    /* Traverse the graph. */
    while (stacksize_uv) {
      BLI_assert(stacksize_uv < stack_upper_bound);
      element = stack_uv[--stacksize_uv];
      while (element) {

        /* Scan forwards around the BMFace that contains element->l. */
        if (!uv_selected || uvedit_edge_select_test(scene, element->l, offsets)) {
          UvElement *next = BM_uv_element_get(element_map, element->l->next);
          if (next && next->island == INVALID_ISLAND) {
            UvElement *tail = element_map->head_table[next - element_map->storage];
            stack_uv[stacksize_uv++] = tail;
            while (tail) {
              bm_uv_assign_island(element_map, tail, nislands, map, islandbuf, islandbufsize++);
              tail = tail->next;
              if (tail && tail->separate) {
                break;
              }
            }
          }
        }

        /* Scan backwards around the BMFace that contains element->l. */
        if (!uv_selected || uvedit_edge_select_test(scene, element->l->prev, offsets)) {
          UvElement *prev = BM_uv_element_get(element_map, element->l->prev);
          if (prev && prev->island == INVALID_ISLAND) {
            UvElement *tail = element_map->head_table[prev - element_map->storage];
            stack_uv[stacksize_uv++] = tail;
            while (tail) {
              bm_uv_assign_island(element_map, tail, nislands, map, islandbuf, islandbufsize++);
              tail = tail->next;
              if (tail && tail->separate) {
                break;
              }
            }
          }
        }

        /* The same for all the UvElements in this unique UV. */
        element = element->next;
        if (element && element->separate) {
          break;
        }
      }
    }
    nislands++;
  }
  BLI_assert(islandbufsize == total_uvs);

  MEM_SAFE_FREE(stack_uv);
  MEM_SAFE_FREE(element_map->head_table);

  return nislands;
}

static void bm_uv_build_islands(UvElementMap *element_map,
                                BMesh *bm,
                                const Scene *scene,
                                bool uv_selected)
{
  int totuv = element_map->total_uvs;
  int nislands = 0;
  int islandbufsize = 0;

  /* map holds the map from current vmap->buf to the new, sorted map */
  uint *map = MEM_mallocN(sizeof(*map) * totuv, "uvelement_remap");
  BMFace **stack = MEM_mallocN(sizeof(*stack) * bm->totface, "uv_island_face_stack");
  UvElement *islandbuf = MEM_callocN(sizeof(*islandbuf) * totuv, "uvelement_island_buffer");
  /* Island number for BMFaces. */
  int *island_number = MEM_callocN(sizeof(*island_number) * bm->totface, "uv_island_number_face");
  copy_vn_i(island_number, bm->totface, INVALID_ISLAND);

  const BMUVOffsets uv_offsets = BM_uv_map_get_offsets(bm);

  const bool use_uv_edge_connectivity = scene->toolsettings->uv_flag & UV_SYNC_SELECTION ?
                                            scene->toolsettings->selectmode & SCE_SELECT_EDGE :
                                            scene->toolsettings->uv_selectmode & UV_SELECT_EDGE;
  if (use_uv_edge_connectivity) {
    nislands = bm_uv_edge_select_build_islands(
        element_map, scene, islandbuf, map, uv_selected, uv_offsets);
    islandbufsize = totuv;
  }

  for (int i = 0; i < totuv; i++) {
    if (element_map->storage[i].island == INVALID_ISLAND) {
      int stacksize = 0;
      element_map->storage[i].island = nislands;
      stack[0] = element_map->storage[i].l->f;
      island_number[BM_elem_index_get(stack[0])] = nislands;
      stacksize = 1;

      while (stacksize > 0) {
        BMFace *efa = stack[--stacksize];

        BMLoop *l;
        BMIter liter;
        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          if (uv_selected && !uvedit_uv_select_test(scene, l, uv_offsets)) {
            continue;
          }

          UvElement *initelement = element_map->vertex[BM_elem_index_get(l->v)];

          for (UvElement *element = initelement; element; element = element->next) {
            if (element->separate) {
              initelement = element;
            }

            if (element->l->f == efa) {
              /* found the uv corresponding to our face and vertex.
               * Now fill it to the buffer */
              bm_uv_assign_island(element_map, element, nislands, map, islandbuf, islandbufsize++);

              for (element = initelement; element; element = element->next) {
                if (element->separate && element != initelement) {
                  break;
                }

                if (island_number[BM_elem_index_get(element->l->f)] == INVALID_ISLAND) {
                  stack[stacksize++] = element->l->f;
                  island_number[BM_elem_index_get(element->l->f)] = nislands;
                }
              }
              break;
            }
          }
        }
      }

      nislands++;
    }
  }

  MEM_SAFE_FREE(island_number);

  /* remap */
  for (int i = 0; i < bm->totvert; i++) {
    /* important since we may do selection only. Some of these may be NULL */
    if (element_map->vertex[i]) {
      element_map->vertex[i] = &islandbuf[map[element_map->vertex[i] - element_map->storage]];
    }
  }

  element_map->island_indices = MEM_callocN(sizeof(*element_map->island_indices) * nislands,
                                            __func__);
  element_map->island_total_uvs = MEM_callocN(sizeof(*element_map->island_total_uvs) * nislands,
                                              __func__);
  element_map->island_total_unique_uvs = MEM_callocN(
      sizeof(*element_map->island_total_unique_uvs) * nislands, __func__);
  int j = 0;
  for (int i = 0; i < totuv; i++) {
    UvElement *next = element_map->storage[i].next;
    islandbuf[map[i]].next = next ? &islandbuf[map[next - element_map->storage]] : NULL;

    if (islandbuf[i].island != j) {
      j++;
      element_map->island_indices[j] = i;
    }
    BLI_assert(islandbuf[i].island == j);
    element_map->island_total_uvs[j]++;
    if (islandbuf[i].separate) {
      element_map->island_total_unique_uvs[j]++;
    }
  }

  MEM_SAFE_FREE(element_map->storage);
  element_map->storage = islandbuf;
  islandbuf = NULL;
  element_map->total_islands = nislands;
  MEM_SAFE_FREE(stack);
  MEM_SAFE_FREE(map);
}

/** Return true if `loop` has UV co-ordinates which match `luv_a` and `luv_b`. */
static bool loop_uv_match(BMLoop *loop,
                          const float luv_a[2],
                          const float luv_b[2],
                          int cd_loop_uv_offset)
{
  const float *luv_c = BM_ELEM_CD_GET_FLOAT_P(loop, cd_loop_uv_offset);
  const float *luv_d = BM_ELEM_CD_GET_FLOAT_P(loop->next, cd_loop_uv_offset);
  return compare_v2v2(luv_a, luv_c, STD_UV_CONNECT_LIMIT) &&
         compare_v2v2(luv_b, luv_d, STD_UV_CONNECT_LIMIT);
}

/**
 * Utility function to implement #seam_connected.
 *
 * Given `edge`, `luv_anchor` & `luv_fan` find if `needle` is connected without
 * seams or disjoint UVs which would delimit causing them not to be considered connected.
 *
 * \note The term *anchor* is used for the vertex at the center of a face-fan
 * which is being stepped over. Even though every connected face may have a different UV,
 * loops are only stepped onto which match the initial `luv_anchor`.
 *
 * \param edge: Search for `needle` in all loops connected to `edge` (recursively).
 * \param luv_anchor: The UV of the anchor (vertex that's being stepped around).
 * \param luv_fan: The UV of the outer edge, this changes as the fan is stepped over.
 * \param needle: Search for this loop, also defines the vertex at the center of the face-fan.
 * \param visited: A set of edges to prevent recursing down the same edge multiple times.
 * \param cd_loop_uv_offset: The UV layer.
 * \return true if there are edges that fan between them that are seam-free.
 * */
static bool seam_connected_recursive(BMEdge *edge,
                                     const float luv_anchor[2],
                                     const float luv_fan[2],
                                     const BMLoop *needle,
                                     GSet *visited,
                                     const int cd_loop_uv_offset)
{
  BMVert *anchor = needle->v;
  BLI_assert(edge->v1 == anchor || edge->v2 == anchor);

  if (BM_elem_flag_test(edge, BM_ELEM_SEAM)) {
    return false; /* Edge is a seam, don't traverse. */
  }

  if (!BLI_gset_add(visited, edge)) {
    return false; /* Already visited. */
  }

  BMLoop *loop;
  BMIter liter;
  BM_ITER_ELEM (loop, &liter, edge, BM_LOOPS_OF_EDGE) {
    if (loop->v == anchor) {
      if (!loop_uv_match(loop, luv_anchor, luv_fan, cd_loop_uv_offset)) {
        continue; /* `loop` is disjoint in UV space. */
      }

      if (loop == needle) {
        return true; /* Success. */
      }

      const float *luv_far = BM_ELEM_CD_GET_FLOAT_P(loop->prev, cd_loop_uv_offset);
      if (seam_connected_recursive(
              loop->prev->e, luv_anchor, luv_far, needle, visited, cd_loop_uv_offset))
      {
        return true;
      }
    }
    else {
      BLI_assert(loop->next->v == anchor);
      if (!loop_uv_match(loop, luv_fan, luv_anchor, cd_loop_uv_offset)) {
        continue; /* `loop` is disjoint in UV space. */
      }

      if (loop->next == needle) {
        return true; /* Success. */
      }

      const float *luv_far = BM_ELEM_CD_GET_FLOAT_P(loop->next->next, cd_loop_uv_offset);
      if (seam_connected_recursive(
              loop->next->e, luv_anchor, luv_far, needle, visited, cd_loop_uv_offset))
      {
        return true;
      }
    }
  }

  return false;
}

/**
 * Given `loop_a` and `loop_b` originate from the same vertex and share a UV,
 *
 * \return true if there are edges that fan between them that are seam-free.
 * return false otherwise.
 */
static bool seam_connected(BMLoop *loop_a, BMLoop *loop_b, GSet *visited, int cd_loop_uv_offset)
{
  BLI_assert(loop_a && loop_b);
  BLI_assert(loop_a != loop_b);
  BLI_assert(loop_a->v == loop_b->v);

  BLI_gset_clear(visited, NULL);

  const float *luv_anchor = BM_ELEM_CD_GET_FLOAT_P(loop_a, cd_loop_uv_offset);
  const float *luv_next_fan = BM_ELEM_CD_GET_FLOAT_P(loop_a->next, cd_loop_uv_offset);
  bool result = seam_connected_recursive(
      loop_a->e, luv_anchor, luv_next_fan, loop_b, visited, cd_loop_uv_offset);
  if (!result) {
    /* Search around `loop_a` in the opposite direction, as one of the edges may be delimited by
     * a boundary, seam or disjoint UV, or itself be one of these. See: #103670, #103787. */
    const float *luv_prev_fan = BM_ELEM_CD_GET_FLOAT_P(loop_a->prev, cd_loop_uv_offset);
    result = seam_connected_recursive(
        loop_a->prev->e, luv_anchor, luv_prev_fan, loop_b, visited, cd_loop_uv_offset);
  }

  return result;
}

UvElementMap *BM_uv_element_map_create(BMesh *bm,
                                       const Scene *scene,
                                       const bool uv_selected,
                                       const bool use_winding,
                                       const bool use_seams,
                                       const bool do_islands)
{
  /* In uv sync selection, all UVs (from unhidden geometry) are visible. */
  const bool face_selected = !(scene->toolsettings->uv_flag & UV_SYNC_SELECTION);

  BMVert *ev;
  BMFace *efa;
  BMIter iter, liter;

  const BMUVOffsets offsets = BM_uv_map_get_offsets(bm);
  if (offsets.uv < 0) {
    return NULL;
  }

  BM_mesh_elem_index_ensure(bm, BM_VERT | BM_FACE);

  /* Count total uvs. */
  int totuv = 0;
  BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
    if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
      continue;
    }

    if (face_selected && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
      continue;
    }

    if (!uv_selected) {
      totuv += efa->len;
    }
    else {
      BMLoop *l;
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (uvedit_uv_select_test(scene, l, offsets)) {
          totuv++;
        }
      }
    }
  }

  if (totuv == 0) {
    return NULL;
  }

  UvElementMap *element_map = (UvElementMap *)MEM_callocN(sizeof(*element_map), "UvElementMap");
  element_map->total_uvs = totuv;
  element_map->vertex = (UvElement **)MEM_callocN(sizeof(*element_map->vertex) * bm->totvert,
                                                  "UvElementVerts");
  element_map->storage = (UvElement *)MEM_callocN(sizeof(*element_map->storage) * totuv,
                                                  "UvElement");

  bool *winding = use_winding ? MEM_callocN(sizeof(*winding) * bm->totface, "winding") : NULL;

  UvElement *buf = element_map->storage;
  int j;
  BM_ITER_MESH_INDEX (efa, &iter, bm, BM_FACES_OF_MESH, j) {

    if (BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
      continue;
    }

    if (face_selected && !BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
      continue;
    }

    int i;
    BMLoop *l;
    BM_ITER_ELEM_INDEX (l, &liter, efa, BM_LOOPS_OF_FACE, i) {
      if (uv_selected && !uvedit_uv_select_test(scene, l, offsets)) {
        continue;
      }

      buf->l = l;
      buf->island = INVALID_ISLAND;
      buf->loop_of_poly_index = i;

      /* Insert to head of linked list associated with BMVert. */
      buf->next = element_map->vertex[BM_elem_index_get(l->v)];
      element_map->vertex[BM_elem_index_get(l->v)] = buf;

      buf++;
    }

    if (winding) {
      winding[j] = BM_face_calc_area_uv_signed(efa, offsets.uv) > 0;
    }
  }

  GSet *seam_visited_gset = use_seams ? BLI_gset_ptr_new(__func__) : NULL;

  /* For each BMVert, sort associated linked list into unique uvs. */
  int ev_index;
  BM_ITER_MESH_INDEX (ev, &iter, bm, BM_VERTS_OF_MESH, ev_index) {
    UvElement *newvlist = NULL;
    UvElement *vlist = element_map->vertex[ev_index];
    while (vlist) {

      /* Detach head from unsorted list. */
      UvElement *v = vlist;
      vlist = vlist->next;
      v->next = newvlist;
      newvlist = v;

      const float *uv = BM_ELEM_CD_GET_VOID_P(v->l, offsets.uv);
      bool uv_vert_sel = uvedit_uv_select_test(scene, v->l, offsets);

      UvElement *lastv = NULL;
      UvElement *iterv = vlist;

      /* Scan through unsorted list, finding UvElements which are connected to `v`. */
      while (iterv) {
        UvElement *next = iterv->next;

        bool connected = true; /* Assume connected unless we can prove otherwise. */

        if (connected) {
          /* Are the two UVs close together? */
          const float *uv2 = BM_ELEM_CD_GET_FLOAT_P(iterv->l, offsets.uv);
          connected = compare_v2v2(uv2, uv, STD_UV_CONNECT_LIMIT);
        }

        if (connected) {
          /* Check if the uv loops share the same selection state (if not, they are not connected
           * as they have been ripped or other edit commands have separated them). */
          const bool uv2_vert_sel = uvedit_uv_select_test(scene, iterv->l, offsets);
          connected = (uv_vert_sel == uv2_vert_sel);
        }

        if (connected && use_winding) {
          connected = winding[BM_elem_index_get(iterv->l->f)] ==
                      winding[BM_elem_index_get(v->l->f)];
        }

        if (connected && use_seams) {
          connected = seam_connected(iterv->l, v->l, seam_visited_gset, offsets.uv);
        }

        if (connected) {
          if (lastv) {
            lastv->next = next;
          }
          else {
            vlist = next;
          }
          iterv->next = newvlist;
          newvlist = iterv;
        }
        else {
          lastv = iterv;
        }

        iterv = next;
      }

      element_map->total_unique_uvs++;
      newvlist->separate = true;
    }

    /* Write back sorted list. */
    element_map->vertex[ev_index] = newvlist;
  }

  if (seam_visited_gset) {
    BLI_gset_free(seam_visited_gset, NULL);
    seam_visited_gset = NULL;
  }
  MEM_SAFE_FREE(winding);

  /* at this point, every UvElement in vert points to a UvElement sharing the same vertex.
   * Now we should sort uv's in islands. */
  if (do_islands) {
    bm_uv_build_islands(element_map, bm, scene, uv_selected);
  }

  /* TODO: Confirm element_map->total_unique_uvs doesn't require recalculating. */
  element_map->total_unique_uvs = 0;
  for (int i = 0; i < element_map->total_uvs; i++) {
    if (element_map->storage[i].separate) {
      element_map->total_unique_uvs++;
    }
  }

  return element_map;
}

void BM_uv_vert_map_free(UvVertMap *vmap)
{
  if (vmap) {
    if (vmap->vert) {
      MEM_freeN(vmap->vert);
    }
    if (vmap->buf) {
      MEM_freeN(vmap->buf);
    }
    MEM_freeN(vmap);
  }
}

void BM_uv_element_map_free(UvElementMap *element_map)
{
  if (element_map) {
    MEM_SAFE_FREE(element_map->storage);
    MEM_SAFE_FREE(element_map->vertex);
    MEM_SAFE_FREE(element_map->head_table);
    MEM_SAFE_FREE(element_map->unique_index_table);
    MEM_SAFE_FREE(element_map->island_indices);
    MEM_SAFE_FREE(element_map->island_total_uvs);
    MEM_SAFE_FREE(element_map->island_total_unique_uvs);
    MEM_SAFE_FREE(element_map);
  }
}

UvElement *BM_uv_element_get(const UvElementMap *element_map, const BMLoop *l)
{
  UvElement *element = element_map->vertex[BM_elem_index_get(l->v)];
  while (element) {
    if (element->l == l) {
      return element;
    }
    element = element->next;
  }

  return NULL;
}

UvElement *BM_uv_element_get_head(UvElementMap *element_map, UvElement *child)
{
  if (!child) {
    return NULL;
  }

  return element_map->vertex[BM_elem_index_get(child->l->v)];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Data Layer Checks
 * \{ */

BMFace *EDBM_uv_active_face_get(BMEditMesh *em, const bool sloppy, const bool selected)
{
  if (!EDBM_uv_check(em)) {
    return NULL;
  }
  BMFace *efa = BM_mesh_active_face_get(em->bm, sloppy, selected);
  if (efa) {
    return efa;
  }

  return NULL;
}

bool EDBM_uv_check(BMEditMesh *em)
{
  /* some of these checks could be a touch overkill */
  return em && em->bm->totface && CustomData_has_layer(&em->bm->ldata, CD_PROP_FLOAT2);
}

bool EDBM_vert_color_check(BMEditMesh *em)
{
  /* some of these checks could be a touch overkill */
  return em && em->bm->totface && CustomData_has_layer(&em->bm->ldata, CD_PROP_BYTE_COLOR);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mirror Cache API
 * \{ */

static BMVert *cache_mirr_intptr_as_bmvert(const intptr_t *index_lookup, int index)
{
  intptr_t eve_i = index_lookup[index];
  return (eve_i == -1) ? NULL : (BMVert *)eve_i;
}

/**
 * Mirror editing API, usage:
 *
 * \code{.c}
 * EDBM_verts_mirror_cache_begin(em, ...);
 *
 * BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
 *     v_mirror = EDBM_verts_mirror_get(em, v);
 *     e_mirror = EDBM_verts_mirror_get_edge(em, e);
 *     f_mirror = EDBM_verts_mirror_get_face(em, f);
 * }
 *
 * EDBM_verts_mirror_cache_end(em);
 * \endcode
 */

/* BM_SEARCH_MAXDIST is too big, copied from 2.6x MOC_THRESH, should become a preference. */
#define BM_SEARCH_MAXDIST_MIRR 0.00002f
#define BM_CD_LAYER_ID "__mirror_index"

void EDBM_verts_mirror_cache_begin_ex(BMEditMesh *em,
                                      const int axis,
                                      const bool use_self,
                                      const bool use_select,
                                      const bool respecthide,
                                      /* extra args */
                                      const bool use_topology,
                                      float maxdist,
                                      int *r_index)
{
  BMesh *bm = em->bm;
  BMIter iter;
  BMVert *v;
  int cd_vmirr_offset = 0;
  int i;
  const float maxdist_sq = square_f(maxdist);

  /* one or the other is used depending if topo is enabled */
  KDTree_3d *tree = NULL;
  MirrTopoStore_t mesh_topo_store = {NULL, -1, -1, -1};

  BM_mesh_elem_table_ensure(bm, BM_VERT);

  if (r_index == NULL) {
    const char *layer_id = BM_CD_LAYER_ID;
    em->mirror_cdlayer = CustomData_get_named_layer_index(&bm->vdata, CD_PROP_INT32, layer_id);
    if (em->mirror_cdlayer == -1) {
      BM_data_layer_add_named(bm, &bm->vdata, CD_PROP_INT32, layer_id);
      em->mirror_cdlayer = CustomData_get_named_layer_index(&bm->vdata, CD_PROP_INT32, layer_id);
    }

    cd_vmirr_offset = CustomData_get_n_offset(
        &bm->vdata,
        CD_PROP_INT32,
        em->mirror_cdlayer - CustomData_get_layer_index(&bm->vdata, CD_PROP_INT32));

    bm->vdata.layers[em->mirror_cdlayer].flag |= CD_FLAG_TEMPORARY;
  }

  BM_mesh_elem_index_ensure(bm, BM_VERT);

  if (use_topology) {
    ED_mesh_mirrtopo_init(em, NULL, &mesh_topo_store, true);
  }
  else {
    tree = BLI_kdtree_3d_new(bm->totvert);
    BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
      if (respecthide && BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
        continue;
      }

      BLI_kdtree_3d_insert(tree, i, v->co);
    }
    BLI_kdtree_3d_balance(tree);
  }

#define VERT_INTPTR(_v, _i) (r_index ? &r_index[_i] : BM_ELEM_CD_GET_VOID_P(_v, cd_vmirr_offset))

  BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
    if (respecthide && BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
      continue;
    }

    if (use_select && !BM_elem_flag_test(v, BM_ELEM_SELECT)) {
      continue;
    }

    BLI_assert(BM_elem_index_get(v) == i);
    BMVert *v_mirr;
    int *idx = VERT_INTPTR(v, i);

    if (use_topology) {
      v_mirr = cache_mirr_intptr_as_bmvert(mesh_topo_store.index_lookup, i);
      if (v_mirr != NULL) {
        if (respecthide && BM_elem_flag_test(v_mirr, BM_ELEM_HIDDEN)) {
          v_mirr = NULL;
        }
      }
    }
    else {
      int i_mirr;
      float co[3];
      copy_v3_v3(co, v->co);
      co[axis] *= -1.0f;

      v_mirr = NULL;
      i_mirr = BLI_kdtree_3d_find_nearest(tree, co, NULL);
      if (i_mirr != -1) {
        BMVert *v_test = BM_vert_at_index(bm, i_mirr);
        if (len_squared_v3v3(co, v_test->co) < maxdist_sq) {
          v_mirr = v_test;
        }
      }
    }

    if (v_mirr && (use_self || (v_mirr != v))) {
      const int i_mirr = BM_elem_index_get(v_mirr);
      *idx = i_mirr;
      idx = VERT_INTPTR(v_mirr, i_mirr);
      *idx = i;
    }
    else {
      *idx = -1;
    }
  }

#undef VERT_INTPTR

  if (use_topology) {
    ED_mesh_mirrtopo_free(&mesh_topo_store);
  }
  else {
    BLI_kdtree_3d_free(tree);
  }
}

void EDBM_verts_mirror_cache_begin(BMEditMesh *em,
                                   const int axis,
                                   const bool use_self,
                                   const bool use_select,
                                   const bool respecthide,
                                   const bool use_topology)
{
  EDBM_verts_mirror_cache_begin_ex(em,
                                   axis,
                                   use_self,
                                   use_select,
                                   respecthide,
                                   /* extra args */
                                   use_topology,
                                   BM_SEARCH_MAXDIST_MIRR,
                                   NULL);
}

BMVert *EDBM_verts_mirror_get(BMEditMesh *em, BMVert *v)
{
  const int *mirr = CustomData_bmesh_get_layer_n(&em->bm->vdata, v->head.data, em->mirror_cdlayer);

  BLI_assert(em->mirror_cdlayer != -1); /* invalid use */

  if (mirr && *mirr >= 0 && *mirr < em->bm->totvert) {
    if (!em->bm->vtable) {
      printf(
          "err: should only be called between "
          "EDBM_verts_mirror_cache_begin and EDBM_verts_mirror_cache_end");
      return NULL;
    }

    return em->bm->vtable[*mirr];
  }

  return NULL;
}

BMEdge *EDBM_verts_mirror_get_edge(BMEditMesh *em, BMEdge *e)
{
  BMVert *v1_mirr, *v2_mirr;
  if ((v1_mirr = EDBM_verts_mirror_get(em, e->v1)) &&
      (v2_mirr = EDBM_verts_mirror_get(em, e->v2)) &&
      /* While highly unlikely, a zero length central edges vertices can match, see #89342. */
      LIKELY(v1_mirr != v2_mirr))
  {
    return BM_edge_exists(v1_mirr, v2_mirr);
  }

  return NULL;
}

BMFace *EDBM_verts_mirror_get_face(BMEditMesh *em, BMFace *f)
{
  BMVert **v_mirr_arr = BLI_array_alloca(v_mirr_arr, f->len);

  BMLoop *l_iter, *l_first;
  uint i = 0;

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    if ((v_mirr_arr[i++] = EDBM_verts_mirror_get(em, l_iter->v)) == NULL) {
      return NULL;
    }
  } while ((l_iter = l_iter->next) != l_first);

  return BM_face_exists(v_mirr_arr, f->len);
}

void EDBM_verts_mirror_cache_clear(BMEditMesh *em, BMVert *v)
{
  int *mirr = CustomData_bmesh_get_layer_n(&em->bm->vdata, v->head.data, em->mirror_cdlayer);

  BLI_assert(em->mirror_cdlayer != -1); /* invalid use */

  if (mirr) {
    *mirr = -1;
  }
}

void EDBM_verts_mirror_cache_end(BMEditMesh *em)
{
  em->mirror_cdlayer = -1;
}

void EDBM_verts_mirror_apply(BMEditMesh *em, const int sel_from, const int sel_to)
{
  BMIter iter;
  BMVert *v;

  BLI_assert((em->bm->vtable != NULL) && ((em->bm->elem_table_dirty & BM_VERT) == 0));

  BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(v, BM_ELEM_SELECT) == sel_from) {
      BMVert *mirr = EDBM_verts_mirror_get(em, v);
      if (mirr) {
        if (BM_elem_flag_test(mirr, BM_ELEM_SELECT) == sel_to) {
          copy_v3_v3(mirr->co, v->co);
          mirr->co[0] *= -1.0f;
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hide/Reveal API
 * \{ */

bool EDBM_mesh_hide(BMEditMesh *em, bool swap)
{
  BMIter iter;
  BMElem *ele;
  int itermode;
  char hflag_swap = swap ? BM_ELEM_SELECT : 0;
  bool changed = true;

  if (em->selectmode & SCE_SELECT_VERTEX) {
    itermode = BM_VERTS_OF_MESH;
  }
  else if (em->selectmode & SCE_SELECT_EDGE) {
    itermode = BM_EDGES_OF_MESH;
  }
  else {
    itermode = BM_FACES_OF_MESH;
  }

  BM_ITER_MESH (ele, &iter, em->bm, itermode) {
    if (!BM_elem_flag_test(ele, BM_ELEM_HIDDEN)) {
      if (BM_elem_flag_test(ele, BM_ELEM_SELECT) ^ hflag_swap) {
        BM_elem_hide_set(em->bm, ele, true);
        changed = true;
      }
    }
  }

  if (changed) {
    EDBM_selectmode_flush(em);
  }
  return changed;

  /* original hide flushing comment (OUTDATED):
   * hide happens on least dominant select mode, and flushes up, not down!
   * (helps preventing errors in subsurf) */
  /* - vertex hidden, always means edge is hidden too
   * - edge hidden, always means face is hidden too
   * - face hidden, only set face hide
   * - then only flush back down what's absolute hidden
   */
}

bool EDBM_mesh_reveal(BMEditMesh *em, bool select)
{
  const char iter_types[3] = {
      BM_VERTS_OF_MESH,
      BM_EDGES_OF_MESH,
      BM_FACES_OF_MESH,
  };

  const bool sels[3] = {
      (em->selectmode & SCE_SELECT_VERTEX) != 0,
      (em->selectmode & SCE_SELECT_EDGE) != 0,
      (em->selectmode & SCE_SELECT_FACE) != 0,
  };
  int i;
  bool changed = false;

  /* Use tag flag to remember what was hidden before all is revealed.
   * BM_ELEM_HIDDEN --> BM_ELEM_TAG */
  for (i = 0; i < 3; i++) {
    BMIter iter;
    BMElem *ele;

    BM_ITER_MESH (ele, &iter, em->bm, iter_types[i]) {
      if (BM_elem_flag_test(ele, BM_ELEM_HIDDEN)) {
        BM_elem_flag_enable(ele, BM_ELEM_TAG);
        changed = true;
      }
      else {
        BM_elem_flag_disable(ele, BM_ELEM_TAG);
      }
    }
  }

  if (!changed) {
    return false;
  }

  /* Reveal everything */
  EDBM_flag_disable_all(em, BM_ELEM_HIDDEN);

  /* Select relevant just-revealed elements */
  for (i = 0; i < 3; i++) {
    BMIter iter;
    BMElem *ele;

    if (!sels[i]) {
      continue;
    }

    BM_ITER_MESH (ele, &iter, em->bm, iter_types[i]) {
      if (BM_elem_flag_test(ele, BM_ELEM_TAG)) {
        BM_elem_select_set(em->bm, ele, select);
      }
    }
  }

  EDBM_selectmode_flush(em);

  /* hidden faces can have invalid normals */
  EDBM_mesh_normals_update(em);

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Update API
 * \{ */

void EDBM_mesh_normals_update_ex(BMEditMesh *em, const struct BMeshNormalsUpdate_Params *params)
{
  BM_mesh_normals_update_ex(em->bm, params);
}

void EDBM_mesh_normals_update(BMEditMesh *em)
{
  EDBM_mesh_normals_update_ex(em,
                              &(const struct BMeshNormalsUpdate_Params){
                                  .face_normals = true,
                              });
}

void EDBM_stats_update(BMEditMesh *em)
{
  const char iter_types[3] = {
      BM_VERTS_OF_MESH,
      BM_EDGES_OF_MESH,
      BM_FACES_OF_MESH,
  };

  BMIter iter;
  BMElem *ele;
  int *tots[3];
  int i;

  tots[0] = &em->bm->totvertsel;
  tots[1] = &em->bm->totedgesel;
  tots[2] = &em->bm->totfacesel;

  em->bm->totvertsel = em->bm->totedgesel = em->bm->totfacesel = 0;

  for (i = 0; i < 3; i++) {
    ele = BM_iter_new(&iter, em->bm, iter_types[i], NULL);
    for (; ele; ele = BM_iter_step(&iter)) {
      if (BM_elem_flag_test(ele, BM_ELEM_SELECT)) {
        (*tots[i])++;
      }
    }
  }
}

void EDBM_update(Mesh *mesh, const struct EDBMUpdate_Params *params)
{
  BMEditMesh *em = mesh->edit_mesh;
  /* Order of calling isn't important. */
  DEG_id_tag_update(&mesh->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, &mesh->id);

  if (params->calc_normals && params->calc_looptri) {
    /* Calculating both has some performance gains. */
    BKE_editmesh_looptri_and_normals_calc(em);
  }
  else {
    if (params->calc_normals) {
      EDBM_mesh_normals_update(em);
    }

    if (params->calc_looptri) {
      BKE_editmesh_looptri_calc(em);
    }
  }

  if (params->is_destructive) {
    /* TODO(@ideasman42): we may be able to remove this now! */
    // BM_mesh_elem_table_free(em->bm, BM_ALL_NOLOOP);
  }
  else {
    /* in debug mode double check we didn't need to recalculate */
    BLI_assert(BM_mesh_elem_table_check(em->bm) == true);
  }
  if (em->bm->spacearr_dirty & BM_SPACEARR_BMO_SET) {
    BM_lnorspace_invalidate(em->bm, false);
    em->bm->spacearr_dirty &= ~BM_SPACEARR_BMO_SET;
  }

#ifdef DEBUG
  {
    BMEditSelection *ese;
    for (ese = em->bm->selected.first; ese; ese = ese->next) {
      BLI_assert(BM_elem_flag_test(ese->ele, BM_ELEM_SELECT));
    }
  }
#endif
}

void EDBM_update_extern(struct Mesh *me, const bool do_tessellation, const bool is_destructive)
{
  EDBM_update(me,
              &(const struct EDBMUpdate_Params){
                  .calc_looptri = do_tessellation,
                  .calc_normals = false,
                  .is_destructive = is_destructive,
              });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Helpers
 * \{ */

bool EDBM_view3d_poll(bContext *C)
{
  if (ED_operator_editmesh(C) && ED_operator_view3d_active(C)) {
    return true;
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BMesh Element API
 * \{ */

BMElem *EDBM_elem_from_selectmode(BMEditMesh *em, BMVert *eve, BMEdge *eed, BMFace *efa)
{
  if ((em->selectmode & SCE_SELECT_VERTEX) && eve) {
    return (BMElem *)eve;
  }
  if ((em->selectmode & SCE_SELECT_EDGE) && eed) {
    return (BMElem *)eed;
  }
  if ((em->selectmode & SCE_SELECT_FACE) && efa) {
    return (BMElem *)efa;
  }
  return NULL;
}

int EDBM_elem_to_index_any(BMEditMesh *em, BMElem *ele)
{
  BMesh *bm = em->bm;
  int index = BM_elem_index_get(ele);

  if (ele->head.htype == BM_VERT) {
    BLI_assert(!(bm->elem_index_dirty & BM_VERT));
  }
  else if (ele->head.htype == BM_EDGE) {
    BLI_assert(!(bm->elem_index_dirty & BM_EDGE));
    index += bm->totvert;
  }
  else if (ele->head.htype == BM_FACE) {
    BLI_assert(!(bm->elem_index_dirty & BM_FACE));
    index += bm->totvert + bm->totedge;
  }
  else {
    BLI_assert(0);
  }

  return index;
}

BMElem *EDBM_elem_from_index_any(BMEditMesh *em, uint index)
{
  BMesh *bm = em->bm;

  if (index < bm->totvert) {
    return (BMElem *)BM_vert_at_index_find_or_table(bm, index);
  }
  index -= bm->totvert;
  if (index < bm->totedge) {
    return (BMElem *)BM_edge_at_index_find_or_table(bm, index);
  }
  index -= bm->totedge;
  if (index < bm->totface) {
    return (BMElem *)BM_face_at_index_find_or_table(bm, index);
  }

  return NULL;
}

int EDBM_elem_to_index_any_multi(
    const Scene *scene, ViewLayer *view_layer, BMEditMesh *em, BMElem *ele, int *r_object_index)
{
  uint bases_len;
  int elem_index = -1;
  *r_object_index = -1;
  Base **bases = BKE_view_layer_array_from_bases_in_edit_mode(scene, view_layer, NULL, &bases_len);
  for (uint base_index = 0; base_index < bases_len; base_index++) {
    Base *base_iter = bases[base_index];
    if (BKE_editmesh_from_object(base_iter->object) == em) {
      *r_object_index = base_index;
      elem_index = EDBM_elem_to_index_any(em, ele);
      break;
    }
  }
  MEM_freeN(bases);
  return elem_index;
}

BMElem *EDBM_elem_from_index_any_multi(const Scene *scene,
                                       ViewLayer *view_layer,
                                       uint object_index,
                                       uint elem_index,
                                       Object **r_obedit)
{
  uint bases_len;
  Base **bases = BKE_view_layer_array_from_bases_in_edit_mode(scene, view_layer, NULL, &bases_len);
  *r_obedit = NULL;
  Object *obedit = (object_index < bases_len) ? bases[object_index]->object : NULL;
  MEM_freeN(bases);
  if (obedit != NULL) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMElem *ele = EDBM_elem_from_index_any(em, elem_index);
    if (ele != NULL) {
      *r_obedit = obedit;
      return ele;
    }
  }
  return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BMesh BVH API
 * \{ */

static BMFace *edge_ray_cast(
    struct BMBVHTree *tree, const float co[3], const float dir[3], float *r_hitout, BMEdge *e)
{
  BMFace *f = BKE_bmbvh_ray_cast(tree, co, dir, 0.0f, NULL, r_hitout, NULL);

  if (f && BM_edge_in_face(e, f)) {
    return NULL;
  }

  return f;
}

static void scale_point(float c1[3], const float p[3], const float s)
{
  sub_v3_v3(c1, p);
  mul_v3_fl(c1, s);
  add_v3_v3(c1, p);
}

bool BMBVH_EdgeVisible(struct BMBVHTree *tree,
                       BMEdge *e,
                       struct Depsgraph *depsgraph,
                       ARegion *region,
                       View3D *v3d,
                       Object *obedit)
{
  BMFace *f;
  float co1[3], co2[3], co3[3], dir1[3], dir2[3], dir3[3];
  float origin[3], invmat[4][4];
  float epsilon = 0.01f;
  float end[3];
  const float mval_f[2] = {
      region->winx / 2.0f,
      region->winy / 2.0f,
  };

  ED_view3d_win_to_segment_clipped(depsgraph, region, v3d, mval_f, origin, end, false);

  invert_m4_m4(invmat, obedit->object_to_world);
  mul_m4_v3(invmat, origin);

  copy_v3_v3(co1, e->v1->co);
  mid_v3_v3v3(co2, e->v1->co, e->v2->co);
  copy_v3_v3(co3, e->v2->co);

  scale_point(co1, co2, 0.99);
  scale_point(co3, co2, 0.99);

  /* OK, idea is to generate rays going from the camera origin to the
   * three points on the edge (v1, mid, v2). */
  sub_v3_v3v3(dir1, origin, co1);
  sub_v3_v3v3(dir2, origin, co2);
  sub_v3_v3v3(dir3, origin, co3);

  normalize_v3_length(dir1, epsilon);
  normalize_v3_length(dir2, epsilon);
  normalize_v3_length(dir3, epsilon);

  /* Offset coordinates slightly along view vectors,
   * to avoid hitting the faces that own the edge. */
  add_v3_v3v3(co1, co1, dir1);
  add_v3_v3v3(co2, co2, dir2);
  add_v3_v3v3(co3, co3, dir3);

  normalize_v3(dir1);
  normalize_v3(dir2);
  normalize_v3(dir3);

  /* do three samplings: left, middle, right */
  f = edge_ray_cast(tree, co1, dir1, NULL, e);
  if (f && !edge_ray_cast(tree, co2, dir2, NULL, e)) {
    return true;
  }
  if (f && !edge_ray_cast(tree, co3, dir3, NULL, e)) {
    return true;
  }
  if (!f) {
    return true;
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BMesh Vertex Projection API
 * \{ */

void EDBM_project_snap_verts(
    bContext *C, Depsgraph *depsgraph, ARegion *region, Object *obedit, BMEditMesh *em)
{
  BMIter iter;
  BMVert *eve;

  ED_view3d_init_mats_rv3d(obedit, region->regiondata);

  Scene *scene = CTX_data_scene(C);
  struct SnapObjectContext *snap_context = ED_transform_snap_object_context_create(scene, 0);

  eSnapTargetOP target_op = SCE_SNAP_TARGET_NOT_ACTIVE;
  const int snap_flag = scene->toolsettings->snap_flag;

  SET_FLAG_FROM_TEST(
      target_op, !(snap_flag & SCE_SNAP_TO_INCLUDE_EDITED), SCE_SNAP_TARGET_NOT_EDITED);
  SET_FLAG_FROM_TEST(
      target_op, !(snap_flag & SCE_SNAP_TO_INCLUDE_NONEDITED), SCE_SNAP_TARGET_NOT_NONEDITED);
  SET_FLAG_FROM_TEST(
      target_op, (snap_flag & SCE_SNAP_TO_ONLY_SELECTABLE), SCE_SNAP_TARGET_ONLY_SELECTABLE);

  BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
      float mval[2], co_proj[3];
      if (ED_view3d_project_float_object(region, eve->co, mval, V3D_PROJ_TEST_NOP) ==
          V3D_PROJ_RET_OK) {
        if (ED_transform_snap_object_project_view3d(snap_context,
                                                    depsgraph,
                                                    region,
                                                    CTX_wm_view3d(C),
                                                    SCE_SNAP_MODE_FACE_RAYCAST,
                                                    &(const struct SnapObjectParams){
                                                        .snap_target_select = target_op,
                                                        .edit_mode_type = SNAP_GEOM_FINAL,
                                                        .use_occlusion_test = true,
                                                    },
                                                    NULL,
                                                    mval,
                                                    NULL,
                                                    NULL,
                                                    co_proj,
                                                    NULL))
        {
          mul_v3_m4v3(eve->co, obedit->world_to_object, co_proj);
        }
      }
    }
  }

  ED_transform_snap_object_context_destroy(snap_context);
}

/** \} */
