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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 * Implements the Sculpt Mode tools
 */

/** \file
 * \ingroup edsculpt
 */

#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_listbase.h"
#include "BLI_ghash.h"
#include "BLI_task.h"
#include "BLI_threads.h"

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_mesh_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_workspace_types.h"

#include "BKE_ccg.h"
#include "BKE_context.h"
#include "BKE_multires.h"
#include "BKE_paint.h"
#include "BKE_key.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_scene.h"
#include "BKE_subsurf.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_undo_system.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_paint.h"
#include "ED_object.h"
#include "ED_sculpt.h"
#include "ED_undo.h"

#include "bmesh.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

typedef struct UndoSculpt {
  ListBase nodes;

  size_t undo_size;
} UndoSculpt;

static UndoSculpt *sculpt_undo_get_nodes(void);

static void update_cb(PBVHNode *node, void *rebuild)
{
  BKE_pbvh_node_mark_update(node);
  if (*((bool *)rebuild)) {
    BKE_pbvh_node_mark_rebuild_draw(node);
  }
  BKE_pbvh_node_fully_hidden_set(node, 0);
}

struct PartialUpdateData {
  PBVH *pbvh;
  bool rebuild;
};

/**
 * A version of #update_cb that tests for 'ME_VERT_PBVH_UPDATE'
 */
static void update_cb_partial(PBVHNode *node, void *userdata)
{
  struct PartialUpdateData *data = userdata;
  if (BKE_pbvh_node_vert_update_check_any(data->pbvh, node)) {
    update_cb(node, &(data->rebuild));
  }
}

static bool test_swap_v3_v3(float a[3], float b[3])
{
  /* no need for float comparison here (memory is exactly equal or not) */
  if (memcmp(a, b, sizeof(float[3])) != 0) {
    swap_v3_v3(a, b);
    return true;
  }
  else {
    return false;
  }
}

static bool sculpt_undo_restore_deformed(
    const SculptSession *ss, SculptUndoNode *unode, int uindex, int oindex, float coord[3])
{
  if (test_swap_v3_v3(coord, unode->orig_co[uindex])) {
    copy_v3_v3(unode->co[uindex], ss->deform_cos[oindex]);
    return true;
  }
  else {
    return false;
  }
}

static bool sculpt_undo_restore_coords(bContext *C, SculptUndoNode *unode)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  SculptSession *ss = ob->sculpt;
  SubdivCCG *subdiv_ccg = ss->subdiv_ccg;
  MVert *mvert;
  int *index;

  if (unode->maxvert) {
    /* regular mesh restore */

    if (ss->kb && !STREQ(ss->kb->name, unode->shapeName)) {
      /* shape key has been changed before calling undo operator */

      Key *key = BKE_key_from_object(ob);
      KeyBlock *kb = key ? BKE_keyblock_find_name(key, unode->shapeName) : NULL;

      if (kb) {
        ob->shapenr = BLI_findindex(&key->block, kb) + 1;

        BKE_sculpt_update_object_for_edit(depsgraph, ob, false, false);
        WM_event_add_notifier(C, NC_OBJECT | ND_DATA, ob);
      }
      else {
        /* key has been removed -- skip this undo node */
        return 0;
      }
    }

    /* no need for float comparison here (memory is exactly equal or not) */
    index = unode->index;
    mvert = ss->mvert;

    if (ss->kb) {
      float(*vertCos)[3];
      vertCos = BKE_keyblock_convert_to_vertcos(ob, ss->kb);

      if (unode->orig_co) {
        if (ss->modifiers_active) {
          for (int i = 0; i < unode->totvert; i++) {
            sculpt_undo_restore_deformed(ss, unode, i, index[i], vertCos[index[i]]);
          }
        }
        else {
          for (int i = 0; i < unode->totvert; i++) {
            swap_v3_v3(vertCos[index[i]], unode->orig_co[i]);
          }
        }
      }
      else {
        for (int i = 0; i < unode->totvert; i++) {
          swap_v3_v3(vertCos[index[i]], unode->co[i]);
        }
      }

      /* propagate new coords to keyblock */
      sculpt_vertcos_to_key(ob, ss->kb, vertCos);

      /* pbvh uses it's own mvert array, so coords should be */
      /* propagated to pbvh here */
      BKE_pbvh_apply_vertCos(ss->pbvh, vertCos, ss->kb->totelem);

      MEM_freeN(vertCos);
    }
    else {
      if (unode->orig_co) {
        if (ss->modifiers_active) {
          for (int i = 0; i < unode->totvert; i++) {
            if (sculpt_undo_restore_deformed(ss, unode, i, index[i], mvert[index[i]].co)) {
              mvert[index[i]].flag |= ME_VERT_PBVH_UPDATE;
            }
          }
        }
        else {
          for (int i = 0; i < unode->totvert; i++) {
            if (test_swap_v3_v3(mvert[index[i]].co, unode->orig_co[i])) {
              mvert[index[i]].flag |= ME_VERT_PBVH_UPDATE;
            }
          }
        }
      }
      else {
        for (int i = 0; i < unode->totvert; i++) {
          if (test_swap_v3_v3(mvert[index[i]].co, unode->co[i])) {
            mvert[index[i]].flag |= ME_VERT_PBVH_UPDATE;
          }
        }
      }
    }
  }
  else if (unode->maxgrid && subdiv_ccg != NULL) {
    /* multires restore */
    CCGElem **grids, *grid;
    CCGKey key;
    float(*co)[3];
    int gridsize;

    grids = subdiv_ccg->grids;
    gridsize = subdiv_ccg->grid_size;
    BKE_subdiv_ccg_key_top_level(&key, subdiv_ccg);

    co = unode->co;
    for (int j = 0; j < unode->totgrid; j++) {
      grid = grids[unode->grids[j]];

      for (int i = 0; i < gridsize * gridsize; i++, co++) {
        swap_v3_v3(CCG_elem_offset_co(&key, grid, i), co[0]);
      }
    }
  }

  return 1;
}

static bool sculpt_undo_restore_hidden(bContext *C, SculptUndoNode *unode)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  SculptSession *ss = ob->sculpt;
  SubdivCCG *subdiv_ccg = ss->subdiv_ccg;
  int i;

  if (unode->maxvert) {
    MVert *mvert = ss->mvert;

    for (i = 0; i < unode->totvert; i++) {
      MVert *v = &mvert[unode->index[i]];
      if ((BLI_BITMAP_TEST(unode->vert_hidden, i) != 0) != ((v->flag & ME_HIDE) != 0)) {
        BLI_BITMAP_FLIP(unode->vert_hidden, i);
        v->flag ^= ME_HIDE;
        v->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  else if (unode->maxgrid && subdiv_ccg != NULL) {
    BLI_bitmap **grid_hidden = subdiv_ccg->grid_hidden;

    for (i = 0; i < unode->totgrid; i++) {
      SWAP(BLI_bitmap *, unode->grid_hidden[i], grid_hidden[unode->grids[i]]);
    }
  }

  return 1;
}

static bool sculpt_undo_restore_mask(bContext *C, SculptUndoNode *unode)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  SculptSession *ss = ob->sculpt;
  SubdivCCG *subdiv_ccg = ss->subdiv_ccg;
  MVert *mvert;
  float *vmask;
  int *index, i, j;

  if (unode->maxvert) {
    /* regular mesh restore */

    index = unode->index;
    mvert = ss->mvert;
    vmask = ss->vmask;

    for (i = 0; i < unode->totvert; i++) {
      if (vmask[index[i]] != unode->mask[i]) {
        SWAP(float, vmask[index[i]], unode->mask[i]);
        mvert[index[i]].flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  else if (unode->maxgrid && subdiv_ccg != NULL) {
    /* multires restore */
    CCGElem **grids, *grid;
    CCGKey key;
    float *mask;
    int gridsize;

    grids = subdiv_ccg->grids;
    gridsize = subdiv_ccg->grid_size;
    BKE_subdiv_ccg_key_top_level(&key, subdiv_ccg);

    mask = unode->mask;
    for (j = 0; j < unode->totgrid; j++) {
      grid = grids[unode->grids[j]];

      for (i = 0; i < gridsize * gridsize; i++, mask++) {
        SWAP(float, *CCG_elem_offset_mask(&key, grid, i), *mask);
      }
    }
  }

  return 1;
}

static void sculpt_undo_bmesh_restore_generic_task_cb(
    void *__restrict userdata, const int n, const ParallelRangeTLS *__restrict UNUSED(tls))
{
  PBVHNode **nodes = userdata;

  BKE_pbvh_node_mark_redraw(nodes[n]);
}

static void sculpt_undo_bmesh_restore_generic(bContext *C,
                                              SculptUndoNode *unode,
                                              Object *ob,
                                              SculptSession *ss)
{
  if (unode->applied) {
    BM_log_undo(ss->bm, ss->bm_log);
    unode->applied = false;
  }
  else {
    BM_log_redo(ss->bm, ss->bm_log);
    unode->applied = true;
  }

  if (unode->type == SCULPT_UNDO_MASK) {
    int totnode;
    PBVHNode **nodes;
    Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

    BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

    ParallelRangeSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.use_threading = ((sd->flags & SCULPT_USE_OPENMP) && totnode > SCULPT_THREADED_LIMIT);
    BLI_task_parallel_range(
        0, totnode, nodes, sculpt_undo_bmesh_restore_generic_task_cb, &settings);

    if (nodes) {
      MEM_freeN(nodes);
    }
  }
  else {
    sculpt_pbvh_clear(ob);
  }
}

/* Create empty sculpt BMesh and enable logging */
static void sculpt_undo_bmesh_enable(Object *ob, SculptUndoNode *unode)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = ob->data;

  sculpt_pbvh_clear(ob);

  /* Create empty BMesh and enable logging */
  ss->bm = BM_mesh_create(&bm_mesh_allocsize_default,
                          &((struct BMeshCreateParams){
                              .use_toolflags = false,
                          }));
  BM_data_layer_add(ss->bm, &ss->bm->vdata, CD_PAINT_MASK);
  sculpt_dyntopo_node_layers_add(ss);
  me->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;

  /* Restore the BMLog using saved entries */
  ss->bm_log = BM_log_from_existing_entries_create(ss->bm, unode->bm_entry);
}

static void sculpt_undo_bmesh_restore_begin(bContext *C,
                                            SculptUndoNode *unode,
                                            Object *ob,
                                            SculptSession *ss)
{
  if (unode->applied) {
    sculpt_dynamic_topology_disable(C, unode);
    unode->applied = false;
  }
  else {
    sculpt_undo_bmesh_enable(ob, unode);

    /* Restore the mesh from the first log entry */
    BM_log_redo(ss->bm, ss->bm_log);

    unode->applied = true;
  }
}

static void sculpt_undo_bmesh_restore_end(bContext *C,
                                          SculptUndoNode *unode,
                                          Object *ob,
                                          SculptSession *ss)
{
  if (unode->applied) {
    sculpt_undo_bmesh_enable(ob, unode);

    /* Restore the mesh from the last log entry */
    BM_log_undo(ss->bm, ss->bm_log);

    unode->applied = false;
  }
  else {
    /* Disable dynamic topology sculpting */
    sculpt_dynamic_topology_disable(C, NULL);
    unode->applied = true;
  }
}

/* Handle all dynamic-topology updates
 *
 * Returns true if this was a dynamic-topology undo step, otherwise
 * returns false to indicate the non-dyntopo code should run. */
static int sculpt_undo_bmesh_restore(bContext *C,
                                     SculptUndoNode *unode,
                                     Object *ob,
                                     SculptSession *ss)
{
  switch (unode->type) {
    case SCULPT_UNDO_DYNTOPO_BEGIN:
      sculpt_undo_bmesh_restore_begin(C, unode, ob, ss);
      return true;

    case SCULPT_UNDO_DYNTOPO_END:
      sculpt_undo_bmesh_restore_end(C, unode, ob, ss);
      return true;

    default:
      if (ss->bm_log) {
        sculpt_undo_bmesh_restore_generic(C, unode, ob, ss);
        return true;
      }
      break;
  }

  return false;
}

static void sculpt_undo_restore_list(bContext *C, ListBase *lb)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  Object *ob = OBACT(view_layer);
  Depsgraph *depsgraph = CTX_data_depsgraph(C);
  SculptSession *ss = ob->sculpt;
  SubdivCCG *subdiv_ccg = ss->subdiv_ccg;
  SculptUndoNode *unode;
  bool update = false, rebuild = false;
  bool need_mask = false;
  bool partial_update = true;

  for (unode = lb->first; unode; unode = unode->next) {
    if (STREQ(unode->idname, ob->id.name)) {
      if (unode->type == SCULPT_UNDO_MASK) {
        /* is possible that we can't do the mask undo (below)
         * because of the vertex count */
        need_mask = true;
        break;
      }
    }
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);

  BKE_sculpt_update_object_for_edit(depsgraph, ob, false, need_mask);

  if (lb->first && sculpt_undo_bmesh_restore(C, lb->first, ob, ss)) {
    return;
  }

  for (unode = lb->first; unode; unode = unode->next) {
    if (!STREQ(unode->idname, ob->id.name)) {
      continue;
    }

    /* check if undo data matches current data well enough to
     * continue */
    if (unode->maxvert) {
      if (ss->totvert != unode->maxvert) {
        continue;
      }
    }
    else if (unode->maxgrid && subdiv_ccg != NULL) {
      if ((subdiv_ccg->num_grids != unode->maxgrid) ||
          (subdiv_ccg->grid_size != unode->gridsize)) {
        continue;
      }

      /* multi-res can't do partial updates since it doesn't flag edited vertices */
      partial_update = false;
    }

    switch (unode->type) {
      case SCULPT_UNDO_COORDS:
        if (sculpt_undo_restore_coords(C, unode)) {
          update = true;
        }
        break;
      case SCULPT_UNDO_HIDDEN:
        if (sculpt_undo_restore_hidden(C, unode)) {
          rebuild = true;
        }
        break;
      case SCULPT_UNDO_MASK:
        if (sculpt_undo_restore_mask(C, unode)) {
          update = true;
        }
        break;

      case SCULPT_UNDO_DYNTOPO_BEGIN:
      case SCULPT_UNDO_DYNTOPO_END:
      case SCULPT_UNDO_DYNTOPO_SYMMETRIZE:
        BLI_assert(!"Dynamic topology should've already been handled");
        break;
    }
  }

  if (update || rebuild) {
    bool tag_update = false;
    /* we update all nodes still, should be more clever, but also
     * needs to work correct when exiting/entering sculpt mode and
     * the nodes get recreated, though in that case it could do all */
    if (partial_update) {
      struct PartialUpdateData data = {
          .rebuild = rebuild,
          .pbvh = ss->pbvh,
      };
      BKE_pbvh_search_callback(ss->pbvh, NULL, NULL, update_cb_partial, &data);
    }
    else {
      BKE_pbvh_search_callback(ss->pbvh, NULL, NULL, update_cb, &rebuild);
    }
    BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateBB | PBVH_UpdateOriginalBB | PBVH_UpdateRedraw);

    if (BKE_sculpt_multires_active(scene, ob)) {
      if (rebuild) {
        multires_mark_as_modified(ob, MULTIRES_HIDDEN_MODIFIED);
      }
      else {
        multires_mark_as_modified(ob, MULTIRES_COORDS_MODIFIED);
      }
    }

    tag_update |= ((Mesh *)ob->data)->id.us > 1 || !BKE_sculptsession_use_pbvh_draw(ob, v3d);

    if (ss->kb || ss->modifiers_active) {
      Mesh *mesh = ob->data;
      BKE_mesh_calc_normals(mesh);

      BKE_sculptsession_free_deformMats(ss);
      tag_update |= true;
    }

    if (tag_update) {
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }
    else {
      sculpt_update_object_bounding_box(ob);
    }
  }
}

static void sculpt_undo_free_list(ListBase *lb)
{
  SculptUndoNode *unode = lb->first;
  while (unode != NULL) {
    SculptUndoNode *unode_next = unode->next;
    if (unode->co) {
      MEM_freeN(unode->co);
    }
    if (unode->no) {
      MEM_freeN(unode->no);
    }
    if (unode->index) {
      MEM_freeN(unode->index);
    }
    if (unode->grids) {
      MEM_freeN(unode->grids);
    }
    if (unode->orig_co) {
      MEM_freeN(unode->orig_co);
    }
    if (unode->vert_hidden) {
      MEM_freeN(unode->vert_hidden);
    }
    if (unode->grid_hidden) {
      for (int i = 0; i < unode->totgrid; i++) {
        if (unode->grid_hidden[i]) {
          MEM_freeN(unode->grid_hidden[i]);
        }
      }
      MEM_freeN(unode->grid_hidden);
    }
    if (unode->mask) {
      MEM_freeN(unode->mask);
    }

    if (unode->bm_entry) {
      BM_log_entry_drop(unode->bm_entry);
    }

    if (unode->bm_enter_totvert) {
      CustomData_free(&unode->bm_enter_vdata, unode->bm_enter_totvert);
    }
    if (unode->bm_enter_totedge) {
      CustomData_free(&unode->bm_enter_edata, unode->bm_enter_totedge);
    }
    if (unode->bm_enter_totloop) {
      CustomData_free(&unode->bm_enter_ldata, unode->bm_enter_totloop);
    }
    if (unode->bm_enter_totpoly) {
      CustomData_free(&unode->bm_enter_pdata, unode->bm_enter_totpoly);
    }

    MEM_freeN(unode);

    unode = unode_next;
  }
}

/* Most likely we don't need this. */
#if 0
static bool sculpt_undo_cleanup(bContext *C, ListBase *lb)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  SculptUndoNode *unode;

  unode = lb->first;

  if (unode && !STREQ(unode->idname, ob->id.name)) {
    if (unode->bm_entry) {
      BM_log_cleanup_entry(unode->bm_entry);
    }

    return true;
  }

  return false;
}
#endif

SculptUndoNode *sculpt_undo_get_node(PBVHNode *node)
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();

  if (usculpt == NULL) {
    return NULL;
  }

  return BLI_findptr(&usculpt->nodes, node, offsetof(SculptUndoNode, node));
}

static void sculpt_undo_alloc_and_store_hidden(PBVH *pbvh, SculptUndoNode *unode)
{
  PBVHNode *node = unode->node;
  BLI_bitmap **grid_hidden;
  int i, *grid_indices, totgrid;

  grid_hidden = BKE_pbvh_grid_hidden(pbvh);

  BKE_pbvh_node_get_grids(pbvh, node, &grid_indices, &totgrid, NULL, NULL, NULL);

  unode->grid_hidden = MEM_mapallocN(sizeof(*unode->grid_hidden) * totgrid, "unode->grid_hidden");

  for (i = 0; i < totgrid; i++) {
    if (grid_hidden[grid_indices[i]]) {
      unode->grid_hidden[i] = MEM_dupallocN(grid_hidden[grid_indices[i]]);
    }
    else {
      unode->grid_hidden[i] = NULL;
    }
  }
}

static SculptUndoNode *sculpt_undo_alloc_node(Object *ob, PBVHNode *node, SculptUndoType type)
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();
  SculptUndoNode *unode;
  SculptSession *ss = ob->sculpt;
  int totvert, allvert, totgrid, maxgrid, gridsize, *grids;

  unode = MEM_callocN(sizeof(SculptUndoNode), "SculptUndoNode");
  BLI_strncpy(unode->idname, ob->id.name, sizeof(unode->idname));
  unode->type = type;
  unode->node = node;

  if (node) {
    BKE_pbvh_node_num_verts(ss->pbvh, node, &totvert, &allvert);
    BKE_pbvh_node_get_grids(ss->pbvh, node, &grids, &totgrid, &maxgrid, &gridsize, NULL);

    unode->totvert = totvert;
  }
  else {
    maxgrid = 0;
  }

  /* we will use this while sculpting, is mapalloc slow to access then? */

  /* general TODO, fix count_alloc */
  switch (type) {
    case SCULPT_UNDO_COORDS:
      unode->co = MEM_mapallocN(sizeof(float[3]) * allvert, "SculptUndoNode.co");
      unode->no = MEM_mapallocN(sizeof(short[3]) * allvert, "SculptUndoNode.no");

      usculpt->undo_size = (sizeof(float[3]) + sizeof(short[3]) + sizeof(int)) * allvert;
      break;
    case SCULPT_UNDO_HIDDEN:
      if (maxgrid) {
        sculpt_undo_alloc_and_store_hidden(ss->pbvh, unode);
      }
      else {
        unode->vert_hidden = BLI_BITMAP_NEW(allvert, "SculptUndoNode.vert_hidden");
      }

      break;
    case SCULPT_UNDO_MASK:
      unode->mask = MEM_mapallocN(sizeof(float) * allvert, "SculptUndoNode.mask");

      usculpt->undo_size += (sizeof(float) * sizeof(int)) * allvert;

      break;
    case SCULPT_UNDO_DYNTOPO_BEGIN:
    case SCULPT_UNDO_DYNTOPO_END:
    case SCULPT_UNDO_DYNTOPO_SYMMETRIZE:
      BLI_assert(!"Dynamic topology should've already been handled");
      break;
  }

  BLI_addtail(&usculpt->nodes, unode);

  if (maxgrid) {
    /* multires */
    unode->maxgrid = maxgrid;
    unode->totgrid = totgrid;
    unode->gridsize = gridsize;
    unode->grids = MEM_mapallocN(sizeof(int) * totgrid, "SculptUndoNode.grids");
  }
  else {
    /* regular mesh */
    unode->maxvert = ss->totvert;
    unode->index = MEM_mapallocN(sizeof(int) * allvert, "SculptUndoNode.index");
  }

  if (ss->modifiers_active) {
    unode->orig_co = MEM_callocN(allvert * sizeof(*unode->orig_co), "undoSculpt orig_cos");
  }

  return unode;
}

static void sculpt_undo_store_coords(Object *ob, SculptUndoNode *unode)
{
  SculptSession *ss = ob->sculpt;
  PBVHVertexIter vd;

  BKE_pbvh_vertex_iter_begin(ss->pbvh, unode->node, vd, PBVH_ITER_ALL)
  {
    copy_v3_v3(unode->co[vd.i], vd.co);
    if (vd.no) {
      copy_v3_v3_short(unode->no[vd.i], vd.no);
    }
    else {
      normal_float_to_short_v3(unode->no[vd.i], vd.fno);
    }

    if (ss->modifiers_active) {
      copy_v3_v3(unode->orig_co[vd.i], ss->orig_cos[unode->index[vd.i]]);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void sculpt_undo_store_hidden(Object *ob, SculptUndoNode *unode)
{
  PBVH *pbvh = ob->sculpt->pbvh;
  PBVHNode *node = unode->node;

  if (unode->grids) {
    /* already stored during allocation */
  }
  else {
    MVert *mvert;
    const int *vert_indices;
    int allvert;
    int i;

    BKE_pbvh_node_num_verts(pbvh, node, NULL, &allvert);
    BKE_pbvh_node_get_verts(pbvh, node, &vert_indices, &mvert);
    for (i = 0; i < allvert; i++) {
      BLI_BITMAP_SET(unode->vert_hidden, i, mvert[vert_indices[i]].flag & ME_HIDE);
    }
  }
}

static void sculpt_undo_store_mask(Object *ob, SculptUndoNode *unode)
{
  SculptSession *ss = ob->sculpt;
  PBVHVertexIter vd;

  BKE_pbvh_vertex_iter_begin(ss->pbvh, unode->node, vd, PBVH_ITER_ALL)
  {
    unode->mask[vd.i] = *vd.mask;
  }
  BKE_pbvh_vertex_iter_end;
}

static SculptUndoNode *sculpt_undo_bmesh_push(Object *ob, PBVHNode *node, SculptUndoType type)
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();
  SculptSession *ss = ob->sculpt;
  PBVHVertexIter vd;

  SculptUndoNode *unode = usculpt->nodes.first;

  if (unode == NULL) {
    unode = MEM_callocN(sizeof(*unode), __func__);

    BLI_strncpy(unode->idname, ob->id.name, sizeof(unode->idname));
    unode->type = type;
    unode->applied = true;

    if (type == SCULPT_UNDO_DYNTOPO_END) {
      unode->bm_entry = BM_log_entry_add(ss->bm_log);
      BM_log_before_all_removed(ss->bm, ss->bm_log);
    }
    else if (type == SCULPT_UNDO_DYNTOPO_BEGIN) {
      Mesh *me = ob->data;

      /* Store a copy of the mesh's current vertices, loops, and
       * polys. A full copy like this is needed because entering
       * dynamic-topology immediately does topological edits
       * (converting polys to triangles) that the BMLog can't
       * fully restore from */
      CustomData_copy(
          &me->vdata, &unode->bm_enter_vdata, CD_MASK_MESH.vmask, CD_DUPLICATE, me->totvert);
      CustomData_copy(
          &me->edata, &unode->bm_enter_edata, CD_MASK_MESH.emask, CD_DUPLICATE, me->totedge);
      CustomData_copy(
          &me->ldata, &unode->bm_enter_ldata, CD_MASK_MESH.lmask, CD_DUPLICATE, me->totloop);
      CustomData_copy(
          &me->pdata, &unode->bm_enter_pdata, CD_MASK_MESH.pmask, CD_DUPLICATE, me->totpoly);
      unode->bm_enter_totvert = me->totvert;
      unode->bm_enter_totedge = me->totedge;
      unode->bm_enter_totloop = me->totloop;
      unode->bm_enter_totpoly = me->totpoly;

      unode->bm_entry = BM_log_entry_add(ss->bm_log);
      BM_log_all_added(ss->bm, ss->bm_log);
    }
    else {
      unode->bm_entry = BM_log_entry_add(ss->bm_log);
    }

    BLI_addtail(&usculpt->nodes, unode);
  }

  if (node) {
    switch (type) {
      case SCULPT_UNDO_COORDS:
      case SCULPT_UNDO_MASK:
        /* Before any vertex values get modified, ensure their
         * original positions are logged */
        BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_ALL)
        {
          BM_log_vert_before_modified(ss->bm_log, vd.bm_vert, vd.cd_vert_mask_offset);
        }
        BKE_pbvh_vertex_iter_end;
        break;

      case SCULPT_UNDO_HIDDEN: {
        GSetIterator gs_iter;
        GSet *faces = BKE_pbvh_bmesh_node_faces(node);
        BKE_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_ALL)
        {
          BM_log_vert_before_modified(ss->bm_log, vd.bm_vert, vd.cd_vert_mask_offset);
        }
        BKE_pbvh_vertex_iter_end;

        GSET_ITER (gs_iter, faces) {
          BMFace *f = BLI_gsetIterator_getKey(&gs_iter);
          BM_log_face_modified(ss->bm_log, f);
        }
        break;
      }

      case SCULPT_UNDO_DYNTOPO_BEGIN:
      case SCULPT_UNDO_DYNTOPO_END:
      case SCULPT_UNDO_DYNTOPO_SYMMETRIZE:
        break;
    }
  }

  return unode;
}

SculptUndoNode *sculpt_undo_push_node(Object *ob, PBVHNode *node, SculptUndoType type)
{
  SculptSession *ss = ob->sculpt;
  SculptUndoNode *unode;

  /* list is manipulated by multiple threads, so we lock */
  BLI_thread_lock(LOCK_CUSTOM1);

  if (ss->bm || ELEM(type, SCULPT_UNDO_DYNTOPO_BEGIN, SCULPT_UNDO_DYNTOPO_END)) {
    /* Dynamic topology stores only one undo node per stroke,
     * regardless of the number of PBVH nodes modified */
    unode = sculpt_undo_bmesh_push(ob, node, type);
    BLI_thread_unlock(LOCK_CUSTOM1);
    return unode;
  }
  else if ((unode = sculpt_undo_get_node(node))) {
    BLI_thread_unlock(LOCK_CUSTOM1);
    return unode;
  }

  unode = sculpt_undo_alloc_node(ob, node, type);

  /* NOTE: If this ever becomes a bottleneck, make a lock inside of the node.
   * so we release global lock sooner, but keep data locked for until it is
   * fully initialized.
   */

  if (unode->grids) {
    int totgrid, *grids;
    BKE_pbvh_node_get_grids(ss->pbvh, node, &grids, &totgrid, NULL, NULL, NULL);
    memcpy(unode->grids, grids, sizeof(int) * totgrid);
  }
  else {
    const int *vert_indices;
    int allvert;
    BKE_pbvh_node_num_verts(ss->pbvh, node, NULL, &allvert);
    BKE_pbvh_node_get_verts(ss->pbvh, node, &vert_indices, NULL);
    memcpy(unode->index, vert_indices, sizeof(int) * unode->totvert);
  }

  switch (type) {
    case SCULPT_UNDO_COORDS:
      sculpt_undo_store_coords(ob, unode);
      break;
    case SCULPT_UNDO_HIDDEN:
      sculpt_undo_store_hidden(ob, unode);
      break;
    case SCULPT_UNDO_MASK:
      sculpt_undo_store_mask(ob, unode);
      break;
    case SCULPT_UNDO_DYNTOPO_BEGIN:
    case SCULPT_UNDO_DYNTOPO_END:
    case SCULPT_UNDO_DYNTOPO_SYMMETRIZE:
      BLI_assert(!"Dynamic topology should've already been handled");
      break;
  }

  /* store active shape key */
  if (ss->kb) {
    BLI_strncpy(unode->shapeName, ss->kb->name, sizeof(ss->kb->name));
  }
  else {
    unode->shapeName[0] = '\0';
  }

  BLI_thread_unlock(LOCK_CUSTOM1);

  return unode;
}

void sculpt_undo_push_begin(const char *name)
{
  UndoStack *ustack = ED_undo_stack_get();
  bContext *C = NULL; /* special case, we never read from this. */
  BKE_undosys_step_push_init_with_type(ustack, C, name, BKE_UNDOSYS_TYPE_SCULPT);
}

void sculpt_undo_push_end(void)
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();
  SculptUndoNode *unode;

  /* we don't need normals in the undo stack */
  for (unode = usculpt->nodes.first; unode; unode = unode->next) {
    if (unode->no) {
      MEM_freeN(unode->no);
      unode->no = NULL;
    }

    if (unode->node) {
      BKE_pbvh_node_layer_disp_free(unode->node);
    }
  }

  /* We could remove this and enforce all callers run in an operator using 'OPTYPE_UNDO'. */
  wmWindowManager *wm = G_MAIN->wm.first;
  if (wm->op_undo_depth == 0) {
    UndoStack *ustack = ED_undo_stack_get();
    BKE_undosys_step_push(ustack, NULL, NULL);
  }
}

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

typedef struct SculptUndoStep {
  UndoStep step;
  /* note: will split out into list for multi-object-sculpt-mode. */
  UndoSculpt data;
} SculptUndoStep;

static bool sculpt_undosys_poll(bContext *C)
{
  Object *obact = CTX_data_active_object(C);
  if (obact && obact->type == OB_MESH) {
    if (obact && (obact->mode & OB_MODE_SCULPT)) {
      return true;
    }
  }
  return false;
}

static void sculpt_undosys_step_encode_init(struct bContext *UNUSED(C), UndoStep *us_p)
{
  SculptUndoStep *us = (SculptUndoStep *)us_p;
  /* dummy, memory is cleared anyway. */
  BLI_listbase_clear(&us->data.nodes);
}

static bool sculpt_undosys_step_encode(struct bContext *UNUSED(C),
                                       struct Main *UNUSED(bmain),
                                       UndoStep *us_p)
{
  /* dummy, encoding is done along the way by adding tiles
   * to the current 'SculptUndoStep' added by encode_init. */
  SculptUndoStep *us = (SculptUndoStep *)us_p;
  us->step.data_size = us->data.undo_size;

  SculptUndoNode *unode = us->data.nodes.last;
  if (unode && unode->type == SCULPT_UNDO_DYNTOPO_END) {
    us->step.use_memfile_step = true;
  }
  us->step.is_applied = true;
  return true;
}

static void sculpt_undosys_step_decode_undo_impl(struct bContext *C, SculptUndoStep *us)
{
  BLI_assert(us->step.is_applied == true);
  sculpt_undo_restore_list(C, &us->data.nodes);
  us->step.is_applied = false;
}

static void sculpt_undosys_step_decode_redo_impl(struct bContext *C, SculptUndoStep *us)
{
  BLI_assert(us->step.is_applied == false);
  sculpt_undo_restore_list(C, &us->data.nodes);
  us->step.is_applied = true;
}

static void sculpt_undosys_step_decode_undo(struct bContext *C, SculptUndoStep *us)
{
  SculptUndoStep *us_iter = us;
  while (us_iter->step.next && (us_iter->step.next->type == us_iter->step.type)) {
    if (us_iter->step.next->is_applied == false) {
      break;
    }
    us_iter = (SculptUndoStep *)us_iter->step.next;
  }
  while (us_iter != us) {
    sculpt_undosys_step_decode_undo_impl(C, us_iter);
    us_iter = (SculptUndoStep *)us_iter->step.prev;
  }
}

static void sculpt_undosys_step_decode_redo(struct bContext *C, SculptUndoStep *us)
{
  SculptUndoStep *us_iter = us;
  while (us_iter->step.prev && (us_iter->step.prev->type == us_iter->step.type)) {
    if (us_iter->step.prev->is_applied == true) {
      break;
    }
    us_iter = (SculptUndoStep *)us_iter->step.prev;
  }
  while (us_iter && (us_iter->step.is_applied == false)) {
    sculpt_undosys_step_decode_redo_impl(C, us_iter);
    if (us_iter == us) {
      break;
    }
    us_iter = (SculptUndoStep *)us_iter->step.next;
  }
}

static void sculpt_undosys_step_decode(struct bContext *C,
                                       struct Main *bmain,
                                       UndoStep *us_p,
                                       int dir)
{
  /* Ensure sculpt mode. */
  {
    Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    /* Sculpt needs evaluated state. */
    BKE_scene_view_layer_graph_evaluated_ensure(bmain, scene, view_layer);
    Object *ob = OBACT(view_layer);
    if (ob && (ob->type == OB_MESH)) {
      Depsgraph *depsgraph = CTX_data_depsgraph(C);
      if (ob->mode & OB_MODE_SCULPT) {
        /* pass */
      }
      else {
        ED_object_mode_generic_exit(bmain, depsgraph, scene, ob);
        Mesh *me = ob->data;
        /* Don't add sculpt topology undo steps when reading back undo state.
         * The undo steps must enter/exit for us. */
        me->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;
        ED_object_sculptmode_enter_ex(bmain, depsgraph, scene, ob, true, NULL);
      }
      BLI_assert(sculpt_undosys_poll(C));
    }
    else {
      BLI_assert(0);
      return;
    }
  }

  SculptUndoStep *us = (SculptUndoStep *)us_p;
  if (dir < 0) {
    sculpt_undosys_step_decode_undo(C, us);
  }
  else {
    sculpt_undosys_step_decode_redo(C, us);
  }
}

static void sculpt_undosys_step_free(UndoStep *us_p)
{
  SculptUndoStep *us = (SculptUndoStep *)us_p;
  sculpt_undo_free_list(&us->data.nodes);
}

/* Export for ED_undo_sys. */
void ED_sculpt_undosys_type(UndoType *ut)
{
  ut->name = "Sculpt";
  ut->poll = sculpt_undosys_poll;
  ut->step_encode_init = sculpt_undosys_step_encode_init;
  ut->step_encode = sculpt_undosys_step_encode;
  ut->step_decode = sculpt_undosys_step_decode;
  ut->step_free = sculpt_undosys_step_free;

  ut->use_context = true;

  ut->step_size = sizeof(SculptUndoStep);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

static UndoSculpt *sculpt_undosys_step_get_nodes(UndoStep *us_p)
{
  SculptUndoStep *us = (SculptUndoStep *)us_p;
  return &us->data;
}

static UndoSculpt *sculpt_undo_get_nodes(void)
{
  UndoStack *ustack = ED_undo_stack_get();
  UndoStep *us = BKE_undosys_stack_init_or_active_with_type(ustack, BKE_UNDOSYS_TYPE_SCULPT);
  return sculpt_undosys_step_get_nodes(us);
}

/** \} */
