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
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 * Implements the Sculpt Mode tools
 */

/** \file
 * \ingroup edsculpt
 */

#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_ccg.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_scene.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_subsurf.h"
#include "BKE_undo_system.h"

/* TODO(sergey): Ideally should be no direct call to such low level things. */
#include "BKE_subdiv_eval.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_sculpt.h"
#include "ED_undo.h"

#include "bmesh.h"
#include "sculpt_intern.h"

/* Implementation of undo system for objects in sculpt mode.
 *
 * Each undo step in sculpt mode consists of list of nodes, each node contains:
 *  - Node type
 *  - Data for this type.
 *
 * Node type used for undo depends on specific operation and active sculpt mode
 * ("regular" or dynamic topology).
 *
 * Regular sculpt brushes will use COORDS, HIDDEN or MASK nodes. These nodes are
 * created for every BVH node which is affected by the brush. The undo push for
 * the node happens BEFORE modifications. This makes the operation undo to work
 * in the following way: for every node in the undo step swap happens between
 * node in the undo stack and the corresponding value in the BVH. This is how
 * redo is possible after undo.
 *
 * The COORDS, HIDDEN or MASK type of nodes contains arrays of the corresponding
 * values.
 *
 * Operations like Symmetrize are using GEOMETRY type of nodes which pushes the
 * entire state of the mesh to the undo stack. This node contains all CustomData
 * layers.
 *
 * The tricky aspect of this undo node type is that it stores mesh before and
 * after modification. This allows the undo system to both undo and redo the
 * symmetrize operation within the pre-modified-push of other node type
 * behavior, but it uses more memory that it seems it should be.
 *
 * The dynamic topology undo nodes are handled somewhat separately from all
 * other ones and the idea there is to store log of operations: which vertices
 * and faces have been added or removed.
 *
 * Begin of dynamic topology sculpting mode have own node type. It contains an
 * entire copy of mesh since just enabling the dynamic topology mode already
 * does modifications on it.
 *
 * End of dynamic topology and symmetrize in this mode are handled in a special
 * manner as well. */

typedef struct UndoSculpt {
  ListBase nodes;

  size_t undo_size;
} UndoSculpt;

static UndoSculpt *sculpt_undo_get_nodes(void);

static void update_cb(PBVHNode *node, void *rebuild)
{
  BKE_pbvh_node_mark_update(node);
  BKE_pbvh_node_mark_update_mask(node);
  if (*((bool *)rebuild)) {
    BKE_pbvh_node_mark_update_visibility(node);
  }
  BKE_pbvh_node_fully_hidden_set(node, 0);
}

struct PartialUpdateData {
  PBVH *pbvh;
  bool rebuild;
  char *modified_grids;
};

/**
 * A version of #update_cb that tests for 'ME_VERT_PBVH_UPDATE'
 */
static void update_cb_partial(PBVHNode *node, void *userdata)
{
  struct PartialUpdateData *data = userdata;
  if (BKE_pbvh_type(data->pbvh) == PBVH_GRIDS) {
    int *node_grid_indices;
    int totgrid;
    bool update = false;
    BKE_pbvh_node_get_grids(data->pbvh, node, &node_grid_indices, &totgrid, NULL, NULL, NULL);
    for (int i = 0; i < totgrid; i++) {
      if (data->modified_grids[node_grid_indices[i]] == 1) {
        update = true;
      }
    }
    if (update) {
      update_cb(node, &(data->rebuild));
    }
  }
  else {
    if (BKE_pbvh_node_vert_update_check_any(data->pbvh, node)) {
      update_cb(node, &(data->rebuild));
    }
  }
}

static bool test_swap_v3_v3(float a[3], float b[3])
{
  /* No need for float comparison here (memory is exactly equal or not). */
  if (memcmp(a, b, sizeof(float[3])) != 0) {
    swap_v3_v3(a, b);
    return true;
  }
  return false;
}

static bool sculpt_undo_restore_deformed(
    const SculptSession *ss, SculptUndoNode *unode, int uindex, int oindex, float coord[3])
{
  if (test_swap_v3_v3(coord, unode->orig_co[uindex])) {
    copy_v3_v3(unode->co[uindex], ss->deform_cos[oindex]);
    return true;
  }
  return false;
}

static bool sculpt_undo_restore_coords(bContext *C, Depsgraph *depsgraph, SculptUndoNode *unode)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  SculptSession *ss = ob->sculpt;
  SubdivCCG *subdiv_ccg = ss->subdiv_ccg;
  MVert *mvert;
  int *index;

  if (unode->maxvert) {
    /* Regular mesh restore. */

    if (ss->shapekey_active && !STREQ(ss->shapekey_active->name, unode->shapeName)) {
      /* Shape key has been changed before calling undo operator. */

      Key *key = BKE_key_from_object(ob);
      KeyBlock *kb = key ? BKE_keyblock_find_name(key, unode->shapeName) : NULL;

      if (kb) {
        ob->shapenr = BLI_findindex(&key->block, kb) + 1;

        BKE_sculpt_update_object_for_edit(depsgraph, ob, false, false, false);
        WM_event_add_notifier(C, NC_OBJECT | ND_DATA, ob);
      }
      else {
        /* Key has been removed -- skip this undo node. */
        return false;
      }
    }

    /* No need for float comparison here (memory is exactly equal or not). */
    index = unode->index;
    mvert = ss->mvert;

    if (ss->shapekey_active) {
      float(*vertCos)[3];
      vertCos = BKE_keyblock_convert_to_vertcos(ob, ss->shapekey_active);

      if (unode->orig_co) {
        if (ss->deform_modifiers_active) {
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

      /* Propagate new coords to keyblock. */
      SCULPT_vertcos_to_key(ob, ss->shapekey_active, vertCos);

      /* PBVH uses its own mvert array, so coords should be */
      /* propagated to PBVH here. */
      BKE_pbvh_vert_coords_apply(ss->pbvh, vertCos, ss->shapekey_active->totelem);

      MEM_freeN(vertCos);
    }
    else {
      if (unode->orig_co) {
        if (ss->deform_modifiers_active) {
          for (int i = 0; i < unode->totvert; i++) {
            sculpt_undo_restore_deformed(ss, unode, i, index[i], mvert[index[i]].co);
            mvert[index[i]].flag |= ME_VERT_PBVH_UPDATE;
          }
        }
        else {
          for (int i = 0; i < unode->totvert; i++) {
            swap_v3_v3(mvert[index[i]].co, unode->orig_co[i]);
            mvert[index[i]].flag |= ME_VERT_PBVH_UPDATE;
          }
        }
      }
      else {
        for (int i = 0; i < unode->totvert; i++) {
          swap_v3_v3(mvert[index[i]].co, unode->co[i]);
          mvert[index[i]].flag |= ME_VERT_PBVH_UPDATE;
        }
      }
    }
  }
  else if (unode->maxgrid && subdiv_ccg != NULL) {
    /* Multires restore. */
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

  return true;
}

static bool sculpt_undo_restore_hidden(bContext *C, SculptUndoNode *unode)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  SculptSession *ss = ob->sculpt;
  SubdivCCG *subdiv_ccg = ss->subdiv_ccg;

  if (unode->maxvert) {
    MVert *mvert = ss->mvert;

    for (int i = 0; i < unode->totvert; i++) {
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

    for (int i = 0; i < unode->totgrid; i++) {
      SWAP(BLI_bitmap *, unode->grid_hidden[i], grid_hidden[unode->grids[i]]);
    }
  }

  return true;
}

static bool sculpt_undo_restore_color(bContext *C, SculptUndoNode *unode)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  SculptSession *ss = ob->sculpt;

  if (unode->maxvert) {
    /* regular mesh restore */
    int *index = unode->index;
    MVert *mvert = ss->mvert;
    MPropCol *vcol = ss->vcol;

    for (int i = 0; i < unode->totvert; i++) {
      copy_v4_v4(vcol[index[i]].color, unode->col[i]);
      mvert[index[i]].flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  return true;
}

static bool sculpt_undo_restore_mask(bContext *C, SculptUndoNode *unode)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  SculptSession *ss = ob->sculpt;
  SubdivCCG *subdiv_ccg = ss->subdiv_ccg;
  MVert *mvert;
  float *vmask;
  int *index;

  if (unode->maxvert) {
    /* Regular mesh restore. */

    index = unode->index;
    mvert = ss->mvert;
    vmask = ss->vmask;

    for (int i = 0; i < unode->totvert; i++) {
      if (vmask[index[i]] != unode->mask[i]) {
        SWAP(float, vmask[index[i]], unode->mask[i]);
        mvert[index[i]].flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  else if (unode->maxgrid && subdiv_ccg != NULL) {
    /* Multires restore. */
    CCGElem **grids, *grid;
    CCGKey key;
    float *mask;
    int gridsize;

    grids = subdiv_ccg->grids;
    gridsize = subdiv_ccg->grid_size;
    BKE_subdiv_ccg_key_top_level(&key, subdiv_ccg);

    mask = unode->mask;
    for (int j = 0; j < unode->totgrid; j++) {
      grid = grids[unode->grids[j]];

      for (int i = 0; i < gridsize * gridsize; i++, mask++) {
        SWAP(float, *CCG_elem_offset_mask(&key, grid, i), *mask);
      }
    }
  }

  return true;
}

static bool sculpt_undo_restore_face_sets(bContext *C, SculptUndoNode *unode)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  Mesh *me = BKE_object_get_original_mesh(ob);
  int *face_sets = CustomData_get_layer(&me->pdata, CD_SCULPT_FACE_SETS);
  for (int i = 0; i < me->totpoly; i++) {
    face_sets[i] = unode->face_sets[i];
  }
  return false;
}

static void sculpt_undo_bmesh_restore_generic_task_cb(
    void *__restrict userdata, const int n, const TaskParallelTLS *__restrict UNUSED(tls))
{
  PBVHNode **nodes = userdata;

  BKE_pbvh_node_mark_redraw(nodes[n]);
}

static void sculpt_undo_bmesh_restore_generic(SculptUndoNode *unode, Object *ob, SculptSession *ss)
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

    BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

    TaskParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, true, totnode);
    BLI_task_parallel_range(
        0, totnode, nodes, sculpt_undo_bmesh_restore_generic_task_cb, &settings);

    if (nodes) {
      MEM_freeN(nodes);
    }
  }
  else {
    SCULPT_pbvh_clear(ob);
  }
}

/* Create empty sculpt BMesh and enable logging. */
static void sculpt_undo_bmesh_enable(Object *ob, SculptUndoNode *unode)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = ob->data;

  SCULPT_pbvh_clear(ob);

  /* Create empty BMesh and enable logging. */
  ss->bm = BM_mesh_create(&bm_mesh_allocsize_default,
                          &((struct BMeshCreateParams){
                              .use_toolflags = false,
                          }));
  BM_data_layer_add(ss->bm, &ss->bm->vdata, CD_PAINT_MASK);
  SCULPT_dyntopo_node_layers_add(ss);
  me->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;

  /* Restore the BMLog using saved entries. */
  ss->bm_log = BM_log_from_existing_entries_create(ss->bm, unode->bm_entry);
}

static void sculpt_undo_bmesh_restore_begin(bContext *C,
                                            SculptUndoNode *unode,
                                            Object *ob,
                                            SculptSession *ss)
{
  if (unode->applied) {
    SCULPT_dynamic_topology_disable(C, unode);
    unode->applied = false;
  }
  else {
    sculpt_undo_bmesh_enable(ob, unode);

    /* Restore the mesh from the first log entry. */
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

    /* Restore the mesh from the last log entry. */
    BM_log_undo(ss->bm, ss->bm_log);

    unode->applied = false;
  }
  else {
    /* Disable dynamic topology sculpting. */
    SCULPT_dynamic_topology_disable(C, NULL);
    unode->applied = true;
  }
}

static void sculpt_undo_geometry_store_data(SculptUndoNodeGeometry *geometry, Object *object)
{
  Mesh *mesh = object->data;

  BLI_assert(!geometry->is_initialized);
  geometry->is_initialized = true;

  CustomData_copy(&mesh->vdata, &geometry->vdata, CD_MASK_MESH.vmask, CD_DUPLICATE, mesh->totvert);
  CustomData_copy(&mesh->edata, &geometry->edata, CD_MASK_MESH.emask, CD_DUPLICATE, mesh->totedge);
  CustomData_copy(&mesh->ldata, &geometry->ldata, CD_MASK_MESH.lmask, CD_DUPLICATE, mesh->totloop);
  CustomData_copy(&mesh->pdata, &geometry->pdata, CD_MASK_MESH.pmask, CD_DUPLICATE, mesh->totpoly);

  geometry->totvert = mesh->totvert;
  geometry->totedge = mesh->totedge;
  geometry->totloop = mesh->totloop;
  geometry->totpoly = mesh->totpoly;
}

static void sculpt_undo_geometry_restore_data(SculptUndoNodeGeometry *geometry, Object *object)
{
  Mesh *mesh = object->data;

  BLI_assert(geometry->is_initialized);

  CustomData_free(&mesh->vdata, mesh->totvert);
  CustomData_free(&mesh->edata, mesh->totedge);
  CustomData_free(&mesh->fdata, mesh->totface);
  CustomData_free(&mesh->ldata, mesh->totloop);
  CustomData_free(&mesh->pdata, mesh->totpoly);

  mesh->totvert = geometry->totvert;
  mesh->totedge = geometry->totedge;
  mesh->totloop = geometry->totloop;
  mesh->totpoly = geometry->totpoly;
  mesh->totface = 0;

  CustomData_copy(
      &geometry->vdata, &mesh->vdata, CD_MASK_MESH.vmask, CD_DUPLICATE, geometry->totvert);
  CustomData_copy(
      &geometry->edata, &mesh->edata, CD_MASK_MESH.emask, CD_DUPLICATE, geometry->totedge);
  CustomData_copy(
      &geometry->ldata, &mesh->ldata, CD_MASK_MESH.lmask, CD_DUPLICATE, geometry->totloop);
  CustomData_copy(
      &geometry->pdata, &mesh->pdata, CD_MASK_MESH.pmask, CD_DUPLICATE, geometry->totpoly);

  BKE_mesh_update_customdata_pointers(mesh, false);
}

static void sculpt_undo_geometry_free_data(SculptUndoNodeGeometry *geometry)
{
  if (geometry->totvert) {
    CustomData_free(&geometry->vdata, geometry->totvert);
  }
  if (geometry->totedge) {
    CustomData_free(&geometry->edata, geometry->totedge);
  }
  if (geometry->totloop) {
    CustomData_free(&geometry->ldata, geometry->totloop);
  }
  if (geometry->totpoly) {
    CustomData_free(&geometry->pdata, geometry->totpoly);
  }
}

static void sculpt_undo_geometry_restore(SculptUndoNode *unode, Object *object)
{
  if (unode->geometry_clear_pbvh) {
    SCULPT_pbvh_clear(object);
  }

  if (unode->applied) {
    sculpt_undo_geometry_restore_data(&unode->geometry_modified, object);
    unode->applied = false;
  }
  else {
    sculpt_undo_geometry_restore_data(&unode->geometry_original, object);
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
        sculpt_undo_bmesh_restore_generic(unode, ob, ss);
        return true;
      }
      break;
  }

  return false;
}

/* Geometry updates (such as Apply Base, for example) will re-evaluate the object and refine its
 * Subdiv descriptor. Upon undo it is required that mesh, grids, and subdiv all stay consistent
 * with each other. This means that when geometry coordinate changes the undo should refine the
 * subdiv to the new coarse mesh coordinates. Tricky part is: this needs to happen without using
 * dependency graph tag: tagging object for geometry update will either loose sculpted data from
 * the sculpt grids, or will wrongly "commit" them to the CD_MDISPS.
 *
 * So what we do instead is do minimum object evaluation to get base mesh coordinates for the
 * multires modifier input. While this is expensive, it is less expensive than dependency graph
 * evaluation and is only happening when geometry coordinates changes on undo.
 *
 * Note that the dependency graph is ensured to be evaluated prior to the undo step is decoded,
 * so if the object's modifier stack references other object it is all fine. */
static void sculpt_undo_refine_subdiv(Depsgraph *depsgraph,
                                      SculptSession *ss,
                                      Object *object,
                                      struct Subdiv *subdiv)
{
  float(*deformed_verts)[3] = BKE_multires_create_deformed_base_mesh_vert_coords(
      depsgraph, object, ss->multires.modifier, NULL);

  BKE_subdiv_eval_refine_from_mesh(subdiv, object->data, deformed_verts);

  MEM_freeN(deformed_verts);
}

static void sculpt_undo_restore_list(bContext *C, Depsgraph *depsgraph, ListBase *lb)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  Object *ob = OBACT(view_layer);
  SculptSession *ss = ob->sculpt;
  SubdivCCG *subdiv_ccg = ss->subdiv_ccg;
  SculptUndoNode *unode;
  bool update = false, rebuild = false, update_mask = false, update_visibility = false;
  bool need_mask = false;
  bool need_refine_subdiv = false;

  for (unode = lb->first; unode; unode = unode->next) {
    /* Restore pivot. */
    copy_v3_v3(ss->pivot_pos, unode->pivot_pos);
    copy_v3_v3(ss->pivot_rot, unode->pivot_rot);
    if (STREQ(unode->idname, ob->id.name)) {
      if (unode->type == SCULPT_UNDO_MASK) {
        /* Is possible that we can't do the mask undo (below)
         * because of the vertex count. */
        need_mask = true;
        break;
      }
    }
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);

  if (lb->first) {
    unode = lb->first;
    if (unode->type == SCULPT_UNDO_FACE_SETS) {
      sculpt_undo_restore_face_sets(C, unode);

      rebuild = true;
      BKE_pbvh_search_callback(ss->pbvh, NULL, NULL, update_cb, &rebuild);

      BKE_sculpt_update_object_for_edit(depsgraph, ob, true, need_mask, false);

      SCULPT_visibility_sync_all_face_sets_to_vertices(ob);

      BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateVisibility);

      if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES) {
        BKE_mesh_flush_hidden_from_verts(ob->data);
      }

      DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);
      if (!BKE_sculptsession_use_pbvh_draw(ob, v3d)) {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      }

      unode->applied = true;
      return;
    }
  }

  if (lb->first != NULL) {
    /* Only do early object update for edits if first node needs this.
     * Undo steps like geometry does not need object to be updated before they run and will
     * ensure object is updated after the node is handled. */
    const SculptUndoNode *first_unode = (const SculptUndoNode *)lb->first;
    if (first_unode->type != SCULPT_UNDO_GEOMETRY) {
      BKE_sculpt_update_object_for_edit(depsgraph, ob, false, need_mask, false);
    }

    if (sculpt_undo_bmesh_restore(C, lb->first, ob, ss)) {
      return;
    }
  }

  char *undo_modified_grids = NULL;
  bool use_multires_undo = false;

  for (unode = lb->first; unode; unode = unode->next) {

    if (!STREQ(unode->idname, ob->id.name)) {
      continue;
    }

    /* Check if undo data matches current data well enough to
     * continue. */
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

      use_multires_undo = true;
    }

    switch (unode->type) {
      case SCULPT_UNDO_COORDS:
        if (sculpt_undo_restore_coords(C, depsgraph, unode)) {
          update = true;
        }
        break;
      case SCULPT_UNDO_HIDDEN:
        if (sculpt_undo_restore_hidden(C, unode)) {
          rebuild = true;
          update_visibility = true;
        }
        break;
      case SCULPT_UNDO_MASK:
        if (sculpt_undo_restore_mask(C, unode)) {
          update = true;
          update_mask = true;
        }
        break;
      case SCULPT_UNDO_FACE_SETS:
        break;
      case SCULPT_UNDO_COLOR:
        if (sculpt_undo_restore_color(C, unode)) {
          update = true;
        }
        break;

      case SCULPT_UNDO_GEOMETRY:
        need_refine_subdiv = true;
        sculpt_undo_geometry_restore(unode, ob);
        BKE_sculpt_update_object_for_edit(depsgraph, ob, false, need_mask, false);
        break;

      case SCULPT_UNDO_DYNTOPO_BEGIN:
      case SCULPT_UNDO_DYNTOPO_END:
      case SCULPT_UNDO_DYNTOPO_SYMMETRIZE:
        BLI_assert(!"Dynamic topology should've already been handled");
        break;
    }
  }

  if (use_multires_undo) {
    for (unode = lb->first; unode; unode = unode->next) {
      if (!STREQ(unode->idname, ob->id.name)) {
        continue;
      }
      if (unode->maxgrid == 0) {
        continue;
      }

      if (undo_modified_grids == NULL) {
        undo_modified_grids = MEM_callocN(sizeof(char) * unode->maxgrid, "undo_grids");
      }

      for (int i = 0; i < unode->totgrid; i++) {
        undo_modified_grids[unode->grids[i]] = 1;
      }
    }
  }

  if (subdiv_ccg != NULL && need_refine_subdiv) {
    sculpt_undo_refine_subdiv(depsgraph, ss, ob, subdiv_ccg->subdiv);
  }

  if (update || rebuild) {
    bool tag_update = false;
    /* We update all nodes still, should be more clever, but also
     * needs to work correct when exiting/entering sculpt mode and
     * the nodes get recreated, though in that case it could do all. */
    struct PartialUpdateData data = {
        .rebuild = rebuild,
        .pbvh = ss->pbvh,
        .modified_grids = undo_modified_grids,
    };
    BKE_pbvh_search_callback(ss->pbvh, NULL, NULL, update_cb_partial, &data);
    BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateBB | PBVH_UpdateOriginalBB | PBVH_UpdateRedraw);

    if (update_mask) {
      BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateMask);
    }

    if (update_visibility) {
      SCULPT_visibility_sync_all_vertex_to_face_sets(ss);
      BKE_pbvh_update_visibility(ss->pbvh);
    }

    if (BKE_sculpt_multires_active(scene, ob)) {
      if (rebuild) {
        multires_mark_as_modified(depsgraph, ob, MULTIRES_HIDDEN_MODIFIED);
      }
      else {
        multires_mark_as_modified(depsgraph, ob, MULTIRES_COORDS_MODIFIED);
      }
    }

    tag_update |= ID_REAL_USERS(ob->data) > 1 || !BKE_sculptsession_use_pbvh_draw(ob, v3d) ||
                  ss->shapekey_active || ss->deform_modifiers_active;

    if (tag_update) {
      Mesh *mesh = ob->data;
      BKE_mesh_calc_normals(mesh);

      BKE_sculptsession_free_deformMats(ss);
    }

    if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES && update_visibility) {
      Mesh *mesh = ob->data;
      BKE_mesh_flush_hidden_from_verts(mesh);
    }

    if (tag_update) {
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }
    else {
      SCULPT_update_object_bounding_box(ob);
    }
  }

  MEM_SAFE_FREE(undo_modified_grids);
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

    sculpt_undo_geometry_free_data(&unode->geometry_original);
    sculpt_undo_geometry_free_data(&unode->geometry_modified);
    sculpt_undo_geometry_free_data(&unode->geometry_bmesh_enter);

    if (unode->face_sets) {
      MEM_freeN(unode->face_sets);
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

SculptUndoNode *SCULPT_undo_get_node(PBVHNode *node)
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();

  if (usculpt == NULL) {
    return NULL;
  }

  return BLI_findptr(&usculpt->nodes, node, offsetof(SculptUndoNode, node));
}

SculptUndoNode *SCULPT_undo_get_first_node()
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();

  if (usculpt == NULL) {
    return NULL;
  }

  return usculpt->nodes.first;
}

static size_t sculpt_undo_alloc_and_store_hidden(PBVH *pbvh, SculptUndoNode *unode)
{
  PBVHNode *node = unode->node;
  BLI_bitmap **grid_hidden = BKE_pbvh_grid_hidden(pbvh);

  int *grid_indices, totgrid;
  BKE_pbvh_node_get_grids(pbvh, node, &grid_indices, &totgrid, NULL, NULL, NULL);

  size_t alloc_size = sizeof(*unode->grid_hidden) * (size_t)totgrid;
  unode->grid_hidden = MEM_callocN(alloc_size, "unode->grid_hidden");

  for (int i = 0; i < totgrid; i++) {
    if (grid_hidden[grid_indices[i]]) {
      unode->grid_hidden[i] = MEM_dupallocN(grid_hidden[grid_indices[i]]);
      alloc_size += MEM_allocN_len(unode->grid_hidden[i]);
    }
    else {
      unode->grid_hidden[i] = NULL;
    }
  }

  return alloc_size;
}

/* Allocate node and initialize its default fields specific for the given undo type.
 * Will also add the node to the list in the undo step. */
static SculptUndoNode *sculpt_undo_alloc_node_type(Object *object, SculptUndoType type)
{
  const size_t alloc_size = sizeof(SculptUndoNode);
  SculptUndoNode *unode = MEM_callocN(alloc_size, "SculptUndoNode");
  BLI_strncpy(unode->idname, object->id.name, sizeof(unode->idname));
  unode->type = type;

  UndoSculpt *usculpt = sculpt_undo_get_nodes();
  BLI_addtail(&usculpt->nodes, unode);
  usculpt->undo_size += alloc_size;

  return unode;
}

/* Will return first existing undo node of the given type.
 * If such node does not exist will allocate node of this type, register it in the undo step and
 * return it. */
static SculptUndoNode *sculpt_undo_find_or_alloc_node_type(Object *object, SculptUndoType type)
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();

  LISTBASE_FOREACH (SculptUndoNode *, unode, &usculpt->nodes) {
    if (unode->type == type) {
      return unode;
    }
  }

  return sculpt_undo_alloc_node_type(object, type);
}

static SculptUndoNode *sculpt_undo_alloc_node(Object *ob, PBVHNode *node, SculptUndoType type)
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();
  SculptSession *ss = ob->sculpt;
  int totvert = 0;
  int allvert = 0;
  int totgrid = 0;
  int maxgrid = 0;
  int gridsize = 0;
  int *grids = NULL;

  SculptUndoNode *unode = sculpt_undo_alloc_node_type(ob, type);
  unode->node = node;

  if (node) {
    BKE_pbvh_node_num_verts(ss->pbvh, node, &totvert, &allvert);
    BKE_pbvh_node_get_grids(ss->pbvh, node, &grids, &totgrid, &maxgrid, &gridsize, NULL);

    unode->totvert = totvert;
  }

  switch (type) {
    case SCULPT_UNDO_COORDS: {
      size_t alloc_size = sizeof(*unode->co) * (size_t)allvert;
      unode->co = MEM_callocN(alloc_size, "SculptUndoNode.co");
      usculpt->undo_size += alloc_size;

      /* FIXME: Should explain why this is allocated here, to be freed in
       * `SCULPT_undo_push_end_ex()`? */
      alloc_size = sizeof(*unode->no) * (size_t)allvert;
      unode->no = MEM_callocN(alloc_size, "SculptUndoNode.no");
      usculpt->undo_size += alloc_size;
      break;
    }
    case SCULPT_UNDO_HIDDEN: {
      if (maxgrid) {
        usculpt->undo_size += sculpt_undo_alloc_and_store_hidden(ss->pbvh, unode);
      }
      else {
        unode->vert_hidden = BLI_BITMAP_NEW(allvert, "SculptUndoNode.vert_hidden");
        usculpt->undo_size += BLI_BITMAP_SIZE(allvert);
      }

      break;
    }
    case SCULPT_UNDO_MASK: {
      const size_t alloc_size = sizeof(*unode->mask) * (size_t)allvert;
      unode->mask = MEM_callocN(alloc_size, "SculptUndoNode.mask");
      usculpt->undo_size += alloc_size;
      break;
    }
    case SCULPT_UNDO_COLOR: {
      const size_t alloc_size = sizeof(*unode->col) * (size_t)allvert;
      unode->col = MEM_callocN(alloc_size, "SculptUndoNode.col");
      usculpt->undo_size += alloc_size;
      break;
    }
    case SCULPT_UNDO_DYNTOPO_BEGIN:
    case SCULPT_UNDO_DYNTOPO_END:
    case SCULPT_UNDO_DYNTOPO_SYMMETRIZE:
      BLI_assert(!"Dynamic topology should've already been handled");
    case SCULPT_UNDO_GEOMETRY:
    case SCULPT_UNDO_FACE_SETS:
      break;
  }

  if (maxgrid) {
    /* Multires. */
    unode->maxgrid = maxgrid;
    unode->totgrid = totgrid;
    unode->gridsize = gridsize;

    const size_t alloc_size = sizeof(*unode->grids) * (size_t)totgrid;
    unode->grids = MEM_callocN(alloc_size, "SculptUndoNode.grids");
    usculpt->undo_size += alloc_size;
  }
  else {
    /* Regular mesh. */
    unode->maxvert = ss->totvert;

    const size_t alloc_size = sizeof(*unode->index) * (size_t)allvert;
    unode->index = MEM_callocN(alloc_size, "SculptUndoNode.index");
    usculpt->undo_size += alloc_size;
  }

  if (ss->deform_modifiers_active) {
    const size_t alloc_size = sizeof(*unode->orig_co) * (size_t)allvert;
    unode->orig_co = MEM_callocN(alloc_size, "undoSculpt orig_cos");
    usculpt->undo_size += alloc_size;
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

    if (ss->deform_modifiers_active) {
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
    /* Already stored during allocation. */
  }
  else {
    MVert *mvert;
    const int *vert_indices;
    int allvert;

    BKE_pbvh_node_num_verts(pbvh, node, NULL, &allvert);
    BKE_pbvh_node_get_verts(pbvh, node, &vert_indices, &mvert);
    for (int i = 0; i < allvert; i++) {
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

static void sculpt_undo_store_color(Object *ob, SculptUndoNode *unode)
{
  SculptSession *ss = ob->sculpt;
  PBVHVertexIter vd;

  BKE_pbvh_vertex_iter_begin(ss->pbvh, unode->node, vd, PBVH_ITER_ALL)
  {
    copy_v4_v4(unode->col[vd.i], vd.col);
  }
  BKE_pbvh_vertex_iter_end;
}

static SculptUndoNodeGeometry *sculpt_undo_geometry_get(SculptUndoNode *unode)
{
  if (!unode->geometry_original.is_initialized) {
    return &unode->geometry_original;
  }

  BLI_assert(!unode->geometry_modified.is_initialized);

  return &unode->geometry_modified;
}

static SculptUndoNode *sculpt_undo_geometry_push(Object *object, SculptUndoType type)
{
  SculptUndoNode *unode = sculpt_undo_find_or_alloc_node_type(object, type);
  unode->applied = false;
  unode->geometry_clear_pbvh = true;

  SculptUndoNodeGeometry *geometry = sculpt_undo_geometry_get(unode);
  sculpt_undo_geometry_store_data(geometry, object);

  return unode;
}

static SculptUndoNode *sculpt_undo_face_sets_push(Object *ob, SculptUndoType type)
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();
  SculptUndoNode *unode = usculpt->nodes.first;

  unode = MEM_callocN(sizeof(*unode), __func__);

  BLI_strncpy(unode->idname, ob->id.name, sizeof(unode->idname));
  unode->type = type;
  unode->applied = true;

  Mesh *me = BKE_object_get_original_mesh(ob);

  unode->face_sets = MEM_callocN(me->totpoly * sizeof(int), "sculpt face sets");

  int *face_sets = CustomData_get_layer(&me->pdata, CD_SCULPT_FACE_SETS);
  for (int i = 0; i < me->totpoly; i++) {
    unode->face_sets[i] = face_sets[i];
  }

  BLI_addtail(&usculpt->nodes, unode);

  return unode;
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
      /* Store a copy of the mesh's current vertices, loops, and
       * polys. A full copy like this is needed because entering
       * dynamic-topology immediately does topological edits
       * (converting polys to triangles) that the BMLog can't
       * fully restore from. */
      SculptUndoNodeGeometry *geometry = &unode->geometry_bmesh_enter;
      sculpt_undo_geometry_store_data(geometry, ob);

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
         * original positions are logged. */
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
      case SCULPT_UNDO_GEOMETRY:
      case SCULPT_UNDO_FACE_SETS:
      case SCULPT_UNDO_COLOR:
        break;
    }
  }

  return unode;
}

SculptUndoNode *SCULPT_undo_push_node(Object *ob, PBVHNode *node, SculptUndoType type)
{
  SculptSession *ss = ob->sculpt;
  SculptUndoNode *unode;

  /* List is manipulated by multiple threads, so we lock. */
  BLI_thread_lock(LOCK_CUSTOM1);

  ss->needs_flush_to_id = 1;

  if (ss->bm || ELEM(type, SCULPT_UNDO_DYNTOPO_BEGIN, SCULPT_UNDO_DYNTOPO_END)) {
    /* Dynamic topology stores only one undo node per stroke,
     * regardless of the number of PBVH nodes modified. */
    unode = sculpt_undo_bmesh_push(ob, node, type);
    BLI_thread_unlock(LOCK_CUSTOM1);
    return unode;
  }
  if (type == SCULPT_UNDO_GEOMETRY) {
    unode = sculpt_undo_geometry_push(ob, type);
    BLI_thread_unlock(LOCK_CUSTOM1);
    return unode;
  }
  if (type == SCULPT_UNDO_FACE_SETS) {
    unode = sculpt_undo_face_sets_push(ob, type);
    BLI_thread_unlock(LOCK_CUSTOM1);
    return unode;
  }
  if ((unode = SCULPT_undo_get_node(node))) {
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
    case SCULPT_UNDO_COLOR:
      sculpt_undo_store_color(ob, unode);
      break;
    case SCULPT_UNDO_DYNTOPO_BEGIN:
    case SCULPT_UNDO_DYNTOPO_END:
    case SCULPT_UNDO_DYNTOPO_SYMMETRIZE:
      BLI_assert(!"Dynamic topology should've already been handled");
    case SCULPT_UNDO_GEOMETRY:
    case SCULPT_UNDO_FACE_SETS:
      break;
  }

  /* Store sculpt pivot. */
  copy_v3_v3(unode->pivot_pos, ss->pivot_pos);
  copy_v3_v3(unode->pivot_rot, ss->pivot_rot);

  /* Store active shape key. */
  if (ss->shapekey_active) {
    BLI_strncpy(unode->shapeName, ss->shapekey_active->name, sizeof(ss->shapekey_active->name));
  }
  else {
    unode->shapeName[0] = '\0';
  }

  BLI_thread_unlock(LOCK_CUSTOM1);

  return unode;
}

void SCULPT_undo_push_begin(Object *ob, const char *name)
{
  UndoStack *ustack = ED_undo_stack_get();

  if (ob != NULL) {
    /* If possible, we need to tag the object and its geometry data as 'changed in the future' in
     * the previous undo step if it's a memfile one. */
    ED_undosys_stack_memfile_id_changed_tag(ustack, &ob->id);
    ED_undosys_stack_memfile_id_changed_tag(ustack, ob->data);
  }

  /* Special case, we never read from this. */
  bContext *C = NULL;

  BKE_undosys_step_push_init_with_type(ustack, C, name, BKE_UNDOSYS_TYPE_SCULPT);
}

void SCULPT_undo_push_end(void)
{
  SCULPT_undo_push_end_ex(false);
}

void SCULPT_undo_push_end_ex(const bool use_nested_undo)
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();
  SculptUndoNode *unode;

  /* We don't need normals in the undo stack. */
  for (unode = usculpt->nodes.first; unode; unode = unode->next) {
    if (unode->no) {
      usculpt->undo_size -= MEM_allocN_len(unode->no);
      MEM_freeN(unode->no);
      unode->no = NULL;
    }
  }

  /* We could remove this and enforce all callers run in an operator using 'OPTYPE_UNDO'. */
  wmWindowManager *wm = G_MAIN->wm.first;
  if (wm->op_undo_depth == 0 || use_nested_undo) {
    UndoStack *ustack = ED_undo_stack_get();
    BKE_undosys_step_push(ustack, NULL, NULL);
    if (wm->op_undo_depth == 0) {
      BKE_undosys_stack_limit_steps_and_memory_defaults(ustack);
    }
    WM_file_tag_modified();
  }
}

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

typedef struct SculptUndoStep {
  UndoStep step;
  /* Note: will split out into list for multi-object-sculpt-mode. */
  UndoSculpt data;
} SculptUndoStep;

static void sculpt_undosys_step_encode_init(struct bContext *UNUSED(C), UndoStep *us_p)
{
  SculptUndoStep *us = (SculptUndoStep *)us_p;
  /* Dummy, memory is cleared anyway. */
  BLI_listbase_clear(&us->data.nodes);
}

static bool sculpt_undosys_step_encode(struct bContext *UNUSED(C),
                                       struct Main *bmain,
                                       UndoStep *us_p)
{
  /* Dummy, encoding is done along the way by adding tiles
   * to the current 'SculptUndoStep' added by encode_init. */
  SculptUndoStep *us = (SculptUndoStep *)us_p;
  us->step.data_size = us->data.undo_size;

  SculptUndoNode *unode = us->data.nodes.last;
  if (unode && unode->type == SCULPT_UNDO_DYNTOPO_END) {
    us->step.use_memfile_step = true;
  }
  us->step.is_applied = true;

  if (!BLI_listbase_is_empty(&us->data.nodes)) {
    bmain->is_memfile_undo_flush_needed = true;
  }

  return true;
}

static void sculpt_undosys_step_decode_undo_impl(struct bContext *C,
                                                 Depsgraph *depsgraph,
                                                 SculptUndoStep *us)
{
  BLI_assert(us->step.is_applied == true);
  sculpt_undo_restore_list(C, depsgraph, &us->data.nodes);
  us->step.is_applied = false;
}

static void sculpt_undosys_step_decode_redo_impl(struct bContext *C,
                                                 Depsgraph *depsgraph,
                                                 SculptUndoStep *us)
{
  BLI_assert(us->step.is_applied == false);
  sculpt_undo_restore_list(C, depsgraph, &us->data.nodes);
  us->step.is_applied = true;
}

static void sculpt_undosys_step_decode_undo(struct bContext *C,
                                            Depsgraph *depsgraph,
                                            SculptUndoStep *us,
                                            const bool is_final)
{
  SculptUndoStep *us_iter = us;
  while (us_iter->step.next && (us_iter->step.next->type == us_iter->step.type)) {
    if (us_iter->step.next->is_applied == false) {
      break;
    }
    us_iter = (SculptUndoStep *)us_iter->step.next;
  }

  while ((us_iter != us) || (!is_final && us_iter == us)) {
    sculpt_undosys_step_decode_undo_impl(C, depsgraph, us_iter);
    if (us_iter == us) {
      break;
    }
    us_iter = (SculptUndoStep *)us_iter->step.prev;
  }
}

static void sculpt_undosys_step_decode_redo(struct bContext *C,
                                            Depsgraph *depsgraph,
                                            SculptUndoStep *us)
{
  SculptUndoStep *us_iter = us;
  while (us_iter->step.prev && (us_iter->step.prev->type == us_iter->step.type)) {
    if (us_iter->step.prev->is_applied == true) {
      break;
    }
    us_iter = (SculptUndoStep *)us_iter->step.prev;
  }
  while (us_iter && (us_iter->step.is_applied == false)) {
    sculpt_undosys_step_decode_redo_impl(C, depsgraph, us_iter);
    if (us_iter == us) {
      break;
    }
    us_iter = (SculptUndoStep *)us_iter->step.next;
  }
}

static void sculpt_undosys_step_decode(
    struct bContext *C, struct Main *bmain, UndoStep *us_p, const eUndoStepDir dir, bool is_final)
{
  BLI_assert(dir != STEP_INVALID);

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  /* Ensure sculpt mode. */
  {
    Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    Object *ob = OBACT(view_layer);
    if (ob && (ob->type == OB_MESH)) {
      if (ob->mode & OB_MODE_SCULPT) {
        /* Pass. */
      }
      else {
        ED_object_mode_generic_exit(bmain, depsgraph, scene, ob);

        /* Sculpt needs evaluated state.
         * Note: needs to be done here, as #ED_object_mode_generic_exit will usually invalidate
         * (some) evaluated data. */
        BKE_scene_graph_evaluated_ensure(depsgraph, bmain);

        Mesh *me = ob->data;
        /* Don't add sculpt topology undo steps when reading back undo state.
         * The undo steps must enter/exit for us. */
        me->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;
        ED_object_sculptmode_enter_ex(bmain, depsgraph, scene, ob, true, NULL);
      }

      if (ob->sculpt) {
        ob->sculpt->needs_flush_to_id = 1;
      }
      bmain->is_memfile_undo_flush_needed = true;
    }
    else {
      BLI_assert(0);
      return;
    }
  }

  SculptUndoStep *us = (SculptUndoStep *)us_p;
  if (dir == STEP_UNDO) {
    sculpt_undosys_step_decode_undo(C, depsgraph, us, is_final);
  }
  else if (dir == STEP_REDO) {
    sculpt_undosys_step_decode_redo(C, depsgraph, us);
  }
}

static void sculpt_undosys_step_free(UndoStep *us_p)
{
  SculptUndoStep *us = (SculptUndoStep *)us_p;
  sculpt_undo_free_list(&us->data.nodes);
}

void ED_sculpt_undo_geometry_begin(struct Object *ob, const char *name)
{
  SCULPT_undo_push_begin(ob, name);
  SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_GEOMETRY);
}

void ED_sculpt_undo_geometry_end(struct Object *ob)
{
  SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_GEOMETRY);
  SCULPT_undo_push_end();
}

/* Export for ED_undo_sys. */
void ED_sculpt_undosys_type(UndoType *ut)
{
  ut->name = "Sculpt";
  ut->poll = NULL; /* No poll from context for now. */
  ut->step_encode_init = sculpt_undosys_step_encode_init;
  ut->step_encode = sculpt_undosys_step_encode;
  ut->step_decode = sculpt_undosys_step_decode;
  ut->step_free = sculpt_undosys_step_free;

  ut->flags = 0;

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

/* -------------------------------------------------------------------- */
/** \name Undo for changes happening on a base mesh for multires sculpting.
 *
 * Use this for multires operators which changes base mesh and which are to be
 * possible. Example of such operators is Apply Base.
 *
 * Usage:
 *
 *   static int operator_exec((bContext *C, wmOperator *op) {
 *
 *      ED_sculpt_undo_push_mixed_begin(C, op->type->name);
 *      // Modify base mesh.
 *      ED_sculpt_undo_push_mixed_end(C, op->type->name);
 *
 *      return OPERATOR_FINISHED;
 *   }
 *
 * If object is not in sculpt mode or sculpt does not happen on multires then
 * regular ED_undo_push() is used.
 * *
 * \{ */

static bool sculpt_undo_use_multires_mesh(bContext *C)
{
  if (BKE_paintmode_get_active_from_context(C) != PAINT_MODE_SCULPT) {
    return false;
  }

  Object *object = CTX_data_active_object(C);
  SculptSession *sculpt_session = object->sculpt;

  return sculpt_session->multires.active;
}

static void sculpt_undo_push_all_grids(Object *object)
{
  SculptSession *ss = object->sculpt;

  /* It is possible that undo push is done from an object state where there is no PBVH. This
   * happens, for example, when an operation which tagged for geometry update was performed prior
   * to the current operation without making any stroke in between.
   *
   * Skip pushing nodes based on the following logic: on redo SCULPT_UNDO_COORDS will ensure
   * PBVH for the new base geometry, which will have same coordinates as if we create PBVH here. */
  if (ss->pbvh == NULL) {
    return;
  }

  PBVHNode **nodes;
  int totnodes;

  BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnodes);
  for (int i = 0; i < totnodes; i++) {
    SculptUndoNode *unode = SCULPT_undo_push_node(object, nodes[i], SCULPT_UNDO_COORDS);
    unode->node = NULL;
  }

  MEM_SAFE_FREE(nodes);
}

void ED_sculpt_undo_push_multires_mesh_begin(bContext *C, const char *str)
{
  if (!sculpt_undo_use_multires_mesh(C)) {
    return;
  }

  Object *object = CTX_data_active_object(C);

  SCULPT_undo_push_begin(object, str);

  SculptUndoNode *geometry_unode = SCULPT_undo_push_node(object, NULL, SCULPT_UNDO_GEOMETRY);
  geometry_unode->geometry_clear_pbvh = false;

  sculpt_undo_push_all_grids(object);
}

void ED_sculpt_undo_push_multires_mesh_end(bContext *C, const char *str)
{
  if (!sculpt_undo_use_multires_mesh(C)) {
    ED_undo_push(C, str);
    return;
  }

  Object *object = CTX_data_active_object(C);

  SculptUndoNode *geometry_unode = SCULPT_undo_push_node(object, NULL, SCULPT_UNDO_GEOMETRY);
  geometry_unode->geometry_clear_pbvh = false;

  SCULPT_undo_push_end();
}

/** \} */
