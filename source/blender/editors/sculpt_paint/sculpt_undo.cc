/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 by Nicholas Bishop. All rights reserved. */

/** \file
 * \ingroup edsculpt
 * Implements the Sculpt Mode tools.
 *
 * Usage Guide
 * ===========
 *
 * The sculpt undo system is a delta-based system. Each undo step stores
 * the difference with the prior one.
 *
 * To use the sculpt undo system, you must call SCULPT_undo_push_begin
 * inside an operator exec or invoke callback (ED_sculpt_undo_geometry_begin
 * may be called if you wish to save a non-delta copy of the entire mesh).
 * This will initialize the sculpt undo stack and set up an undo step.
 *
 * At the end of the operator you should call SCULPT_undo_push_end.
 *
 * SCULPT_undo_push_end and ED_sculpt_undo_geometry_begin both take a
 * #wmOperatorType as an argument. There are _ex versions that allow a custom
 * name; try to avoid using them. These can break the redo panel since it requires
 * the undo push have the same name as the calling operator.
 *
 * NOTE: Sculpt undo steps are not appended to the global undo stack until
 * the operator finishes.  We use BKE_undosys_step_push_init_with_type to build
 * a tentative undo step with is appended later when the operator ends.
 * Operators must have the OPTYPE_UNDO flag set for this to work properly.
 */

#include <stddef.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_attribute.hh"
#include "BKE_ccg.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.h"
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

#include "ED_geometry.h"
#include "ED_object.h"
#include "ED_sculpt.h"
#include "ED_undo.h"

#include "../../bmesh/intern/bmesh_idmap.h"
#include "bmesh.h"
#include "bmesh_log.h"
#include "sculpt_intern.hh"

#define WHEN_GLOBAL_UNDO_WORKS
/* Uncomment to print the undo stack in the console on push/undo/redo. */
//#define SCULPT_UNDO_DEBUG

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

#define NO_ACTIVE_LAYER ATTR_DOMAIN_AUTO

typedef struct UndoSculpt {
  ListBase nodes;

  size_t undo_size;
  BMLog *bm_restore;
} UndoSculpt;

typedef struct SculptAttrRef {
  eAttrDomain domain;
  eCustomDataType type;
  char name[MAX_CUSTOMDATA_LAYER_NAME];
  bool was_set;
} SculptAttrRef;

typedef struct SculptUndoStep {
  UndoStep step;
  /* NOTE: will split out into list for multi-object-sculpt-mode. */
  UndoSculpt data;
  int id;

  bool auto_saved;

  // active vcol layer
  SculptAttrRef active_attr_start;
  SculptAttrRef active_attr_end;

  /* Active color attribute at the start of this undo step. */
  SculptAttrRef active_color_start;

  /* Active color attribute at the end of this undo step. */
  SculptAttrRef active_color_end;

  bContext *C;

#ifdef SCULPT_UNDO_DEBUG
  int id;
#endif
} SculptUndoStep;

extern "C" void BKE_pbvh_bmesh_check_nodes(PBVH *pbvh);

static UndoSculpt *sculpt_undo_get_nodes(void);
static bool sculpt_attribute_ref_equals(SculptAttrRef *a, SculptAttrRef *b);
static void sculpt_save_active_attribute(Object *ob, SculptAttrRef *attr);
static UndoSculpt *sculpt_undosys_step_get_nodes(UndoStep *us_p);

static void update_unode_bmesh_memsize(SculptUndoNode *unode);
static UndoSculpt *sculpt_undo_get_nodes(void);
void sculpt_undo_print_nodes(void *active);
static bool check_first_undo_entry_dyntopo(Object *ob);
static void sculpt_undo_push_begin_ex(Object *ob, const char *name, bool no_first_entry_check);

#ifdef SCULPT_UNDO_DEBUG
#  ifdef _
#    undef _
#  endif
#  define _(type) \
    case type: \
      return #type;
static char *undo_type_to_str(int type)
{
  switch (type) {
    _(SCULPT_UNDO_DYNTOPO_BEGIN)
    _(SCULPT_UNDO_DYNTOPO_END)
    _(SCULPT_UNDO_COORDS)
    _(SCULPT_UNDO_GEOMETRY)
    _(SCULPT_UNDO_DYNTOPO_SYMMETRIZE)
    _(SCULPT_UNDO_FACE_SETS)
    _(SCULPT_UNDO_HIDDEN)
    _(SCULPT_UNDO_MASK)
    _(SCULPT_UNDO_COLOR)
    default:
      return "unknown node type";
  }
}
#  undef _

static int nodeidgen = 1;

static void print_sculpt_node(Object *ob, SculptUndoNode *node)
{
  printf("    %s:%s {applied=%d}\n", undo_type_to_str(node->type), node->idname, node->applied);

  if (node->bm_entry) {
    BM_log_print_entry(ob->sculpt ? ob->sculpt->bm : nullptr, node->bm_entry);
  }
}

static void print_sculpt_undo_step(Object *ob, UndoStep *us, UndoStep *active, int i)
{
  SculptUndoNode *node;

  if (us->type != BKE_UNDOSYS_TYPE_SCULPT) {
    printf("%d %s (non-sculpt): '%s', type:%s, use_memfile_step:%s\n",
           i,
           us == active ? "->" : "  ",
           us->name,
           us->type->name,
           us->use_memfile_step ? "true" : "false");
    return;
  }

  int id = -1;

  SculptUndoStep *su = (SculptUndoStep *)us;
  if (!su->id) {
    su->id = nodeidgen++;
  }

  id = su->id;

  printf("id=%d %s %d %s (use_memfile_step=%s)\n",
         id,
         us == active ? "->" : "  ",
         i,
         us->name,
         us->use_memfile_step ? "true" : "false");

  if (us->type == BKE_UNDOSYS_TYPE_SCULPT) {
    UndoSculpt *usculpt = sculpt_undosys_step_get_nodes(us);

    for (node = usculpt->nodes.first; node; node = node->next) {
      print_sculpt_node(ob, node);
    }
  }
}
void sculpt_undo_print_nodes(Object *ob, void *active)
{

  printf("=================== Sculpt undo steps ==============\n");

  UndoStack *ustack = ED_undo_stack_get();
  UndoStep *us = ustack->steps.first;
  if (active == nullptr) {
    active = ustack->step_active;
  }

  if (!us) {
    return;
  }

  printf("\n");
  if (ustack->step_init) {
    printf("===Undo initialization stepB===\n");
    print_sculpt_undo_step(ob, ustack->step_init, active, -1);
    printf("===============\n");
  }

  int i = 0, act_i = -1;
  for (; us; us = us->next, i++) {
    if (active == us) {
      act_i = i;
    }

    print_sculpt_undo_step(ob, us, active, i);
  }

  if (ustack->step_active) {
    printf("\n\n==Active step:==\n");
    print_sculpt_undo_step(ob, ustack->step_active, active, act_i);
  }
}
#else
#  define sculpt_undo_print_nodes(ob, active) while (0)
#endif

static void update_cb(PBVHNode *node, void *rebuild)
{
  BKE_pbvh_node_mark_update(node);
  BKE_pbvh_node_mark_update_mask(node);
  if (*((bool *)rebuild)) {
    BKE_pbvh_vert_tag_update_normal_visibility(node);
  }
  BKE_pbvh_node_fully_hidden_set(node, 0);
}

struct PartialUpdateData {
  PBVH *pbvh;
  SculptSession *ss;
  bool rebuild;
  char *modified_grids;
  bool *modified_hidden_verts;
  bool *modified_mask_verts;
  bool *modified_color_verts;
  bool *modified_face_set_faces;
};

static UndoSculpt *sculpt_undosys_step_get_nodes(UndoStep *us_p);

/**
 * A version of #update_cb that tests for the update tag in #PBVH.vert_bitmap.
 */
static void update_cb_partial(PBVHNode *node, void *userdata)
{
  PartialUpdateData *data = static_cast<PartialUpdateData *>(userdata);
  if (BKE_pbvh_type(data->pbvh) == PBVH_GRIDS) {
    int *node_grid_indices;
    int totgrid;
    bool update = false;
    BKE_pbvh_node_get_grids(
        data->pbvh, node, &node_grid_indices, &totgrid, nullptr, nullptr, nullptr);
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
    if (BKE_pbvh_node_has_vert_with_normal_update_tag(data->pbvh, node)) {
      BKE_pbvh_node_mark_update(node);
    }
    int verts_num;
    BKE_pbvh_node_num_verts(data->pbvh, node, nullptr, &verts_num);
    const int *vert_indices = BKE_pbvh_node_get_vert_indices(node);
    if (data->modified_mask_verts != nullptr) {
      for (int i = 0; i < verts_num; i++) {
        if (data->modified_mask_verts[vert_indices[i]]) {
          BKE_pbvh_node_mark_update_mask(node);
          break;
        }
      }
    }
    if (data->modified_color_verts != nullptr) {
      for (int i = 0; i < verts_num; i++) {
        if (data->modified_color_verts[vert_indices[i]]) {
          BKE_pbvh_node_mark_update_color(node);
          break;
        }
      }
    }
    if (data->modified_hidden_verts != nullptr) {
      for (int i = 0; i < verts_num; i++) {
        if (data->modified_hidden_verts[vert_indices[i]]) {
          BKE_sculpt_boundary_flag_update(data->ss, BKE_pbvh_make_vref(vert_indices[i]));

          if (data->rebuild) {
            BKE_pbvh_vert_tag_update_normal_visibility(node);
          }
          BKE_pbvh_node_fully_hidden_set(node, 0);
          break;
        }
      }
    }
  }

  if (data->modified_face_set_faces) {
    PBVHFaceIter fd;
    bool updated = false;

    BKE_pbvh_face_iter_begin (data->pbvh, node, fd) {
      if (data->modified_face_set_faces[fd.index]) {
        SCULPT_face_mark_boundary_update(data->ss, fd.face);

        if (!updated) {
          BKE_pbvh_node_mark_update_face_sets(node);
          updated = true;
        }
      }
    }
    BKE_pbvh_face_iter_end(fd);
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

// void pbvh_bmesh_check_nodes(PBVH *pbvh);

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
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  SculptSession *ss = ob->sculpt;
  SubdivCCG *subdiv_ccg = ss->subdiv_ccg;
  int *index;

  if (unode->maxvert) {
    /* Regular mesh restore. */

    if (ss->shapekey_active && !STREQ(ss->shapekey_active->name, unode->shapeName)) {
      /* Shape key has been changed before calling undo operator. */

      Key *key = BKE_key_from_object(ob);
      KeyBlock *kb = key ? BKE_keyblock_find_name(key, unode->shapeName) : nullptr;

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
    float(*positions)[3] = ss->vert_positions;

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

      /* PBVH uses its own vertex array, so coords should be */
      /* propagated to PBVH here. */
      BKE_pbvh_vert_coords_apply(ss->pbvh, vertCos, ss->shapekey_active->totelem);

      MEM_freeN(vertCos);
    }
    else {
      if (unode->orig_co) {
        if (ss->deform_modifiers_active) {
          for (int i = 0; i < unode->totvert; i++) {
            sculpt_undo_restore_deformed(ss, unode, i, index[i], positions[index[i]]);
            BKE_pbvh_vert_tag_update_normal(ss->pbvh, BKE_pbvh_make_vref(index[i]));
          }
        }
        else {
          for (int i = 0; i < unode->totvert; i++) {
            swap_v3_v3(positions[index[i]], unode->orig_co[i]);
            BKE_pbvh_vert_tag_update_normal(ss->pbvh, BKE_pbvh_make_vref(index[i]));
          }
        }
      }
      else {
        for (int i = 0; i < unode->totvert; i++) {
          swap_v3_v3(positions[index[i]], unode->co[i]);
          BKE_pbvh_vert_tag_update_normal(ss->pbvh, BKE_pbvh_make_vref(index[i]));
        }
      }
    }
  }
  else if (unode->maxgrid && subdiv_ccg != nullptr) {
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

static bool sculpt_undo_restore_hidden(bContext *C, SculptUndoNode *unode, bool *modified_vertices)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  SculptSession *ss = ob->sculpt;
  SubdivCCG *subdiv_ccg = ss->subdiv_ccg;

  bool *hide_vert = BKE_pbvh_get_vert_hide_for_write(ss->pbvh);

  if (unode->maxvert) {
    for (int i = 0; i < unode->totvert; i++) {
      const int vert_index = unode->index[i];

      if ((BLI_BITMAP_TEST(unode->vert_hidden, i) != 0) != hide_vert[vert_index]) {
        BLI_BITMAP_FLIP(unode->vert_hidden, i);
        hide_vert[vert_index] = !hide_vert[vert_index];
        modified_vertices[vert_index] = true;
      }
    }
  }
  else if (unode->maxgrid && subdiv_ccg != nullptr) {
    BLI_bitmap **grid_hidden = subdiv_ccg->grid_hidden;

    for (int i = 0; i < unode->totgrid; i++) {
      SWAP(BLI_bitmap *, unode->grid_hidden[i], grid_hidden[unode->grids[i]]);
    }
  }

  return true;
}

static int *sculpt_undo_get_indices32(SculptUndoNode *unode, int allvert)
{
  int *indices = (int *)MEM_malloc_arrayN(allvert, sizeof(int), __func__);

  for (int i = 0; i < allvert; i++) {
    indices[i] = (int)unode->index[i];
  }

  return indices;
}

static bool sculpt_undo_restore_color(bContext *C, SculptUndoNode *unode, bool *modified_vertices)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  SculptSession *ss = ob->sculpt;

  bool modified = false;

  /* NOTE: even with loop colors we still store derived
   * vertex colors for original data lookup. */
  if (unode->col && !unode->loop_col) {
    int *indices = sculpt_undo_get_indices32(unode, unode->totvert);

    BKE_pbvh_swap_colors(ss->pbvh, indices, unode->totvert, unode->col);
    modified = true;

    MEM_SAFE_FREE(indices);
  }

  Mesh *me = BKE_object_get_original_mesh(ob);

  if (unode->loop_col && unode->maxloop == me->totloop) {
    BKE_pbvh_swap_colors(ss->pbvh, unode->loop_index, unode->totloop, unode->loop_col);

    modified = true;
  }

  if (modified) {
    for (int i = 0; i < unode->totvert; i++) {
      modified_vertices[unode->index[i]] = true;
    }
  }

  return modified;
}

static bool sculpt_undo_restore_mask(bContext *C, SculptUndoNode *unode, bool *modified_vertices)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  SculptSession *ss = ob->sculpt;
  SubdivCCG *subdiv_ccg = ss->subdiv_ccg;
  float *vmask;
  int *index;

  if (unode->maxvert) {
    /* Regular mesh restore. */

    index = unode->index;
    vmask = ss->vmask;

    for (int i = 0; i < unode->totvert; i++) {
      if (vmask[index[i]] != unode->mask[i]) {
        SWAP(float, vmask[index[i]], unode->mask[i]);
        modified_vertices[index[i]] = true;
      }
    }
  }
  else if (unode->maxgrid && subdiv_ccg != nullptr) {
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

static bool sculpt_undo_restore_face_sets(bContext *C,
                                          SculptUndoNode *unode,
                                          bool *modified_face_set_faces)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  SculptSession *ss = ob->sculpt;

  ss->face_sets = BKE_sculpt_face_sets_ensure(ob);
  BKE_pbvh_face_sets_set(ss->pbvh, ss->face_sets);

  bool modified = false;

  for (int i = 0; i < unode->faces_num; i++) {
    int face_index = unode->faces[i].i;

    SWAP(int, unode->face_sets[i], ss->face_sets[face_index]);

    modified_face_set_faces[face_index] |= unode->face_sets[i] != ss->face_sets[face_index];
    modified |= modified_face_set_faces[face_index];
  }

  return modified;
}

typedef struct BmeshUndoData {
  PBVH *pbvh;
  BMesh *bm;
  bool do_full_recalc;
  bool balance_pbvh;
  int cd_face_node_offset, cd_vert_node_offset;
  int cd_face_node_offset_old, cd_vert_node_offset_old;
  int cd_boundary_flag, cd_flags;
  bool regen_all_unique_verts;
  bool is_redo;
} BmeshUndoData;

static void bmesh_undo_on_vert_kill(BMVert *v, void *userdata)
{
  BmeshUndoData *data = (BmeshUndoData *)userdata;
  int ni = BM_ELEM_CD_GET_INT(v, data->cd_vert_node_offset);
  // data->do_full_recalc = true;

  if (ni < 0) {
#if 0  // not sure this is really an error
    // something went wrong
    printf("%s: error, vertex %d is not in pbvh; ni was: %d\n",
           __func__,
           BM_ELEM_GET_ID(data->bm, v),
           ni);
    // data->do_full_recalc = true;
#endif
    return;
  }

  BKE_pbvh_bmesh_remove_vertex(data->pbvh, v, false);
  data->balance_pbvh = true;
}

static void bmesh_undo_on_vert_add(BMVert *v, void *userdata)
{
  BmeshUndoData *data = (BmeshUndoData *)userdata;

  data->balance_pbvh = true;

  /* Flag vert as unassigned to a PBVH node; it'll be added to pbvh when
   * its owning faces are.
   */
  BM_ELEM_CD_SET_INT(v, data->cd_vert_node_offset, -1);

  *(int *)BM_ELEM_CD_GET_VOID_P(v, data->cd_boundary_flag) |= SCULPT_BOUNDARY_NEEDS_UPDATE;
  *BM_ELEM_CD_PTR<uint8_t *>(v, data->cd_flags) |= SCULPTFLAG_NEED_DISK_SORT |
                                                   SCULPTFLAG_NEED_VALENCE |
                                                   SCULPTFLAG_NEED_TRIANGULATE;
}

static void bmesh_undo_on_face_kill(BMFace *f, void *userdata)
{
  BmeshUndoData *data = (BmeshUndoData *)userdata;
  int ni = BM_ELEM_CD_GET_INT(f, data->cd_face_node_offset);

  BKE_pbvh_bmesh_remove_face(data->pbvh, f, false);

  if (ni >= 0) {
    PBVHNode *node = BKE_pbvh_get_node(data->pbvh, ni);
    BKE_pbvh_bmesh_mark_node_regen(data->pbvh, node);
  }

  BMLoop *l = f->l_first;
  do {
    *BM_ELEM_CD_PTR<uint8_t *>(l->v, data->cd_flags) |= SCULPTFLAG_NEED_DISK_SORT |
                                                        SCULPTFLAG_NEED_VALENCE;
    *BM_ELEM_CD_PTR<int *>(l->v, data->cd_boundary_flag) |= SCULPT_BOUNDARY_NEEDS_UPDATE;
  } while ((l = l->next) != f->l_first);

  // data->do_full_recalc = true;
  data->balance_pbvh = true;
}

static void bmesh_undo_on_face_add(BMFace *f, void *userdata)
{
  BmeshUndoData *data = (BmeshUndoData *)userdata;
  // data->do_full_recalc = true;

  BM_ELEM_CD_SET_INT(f, data->cd_face_node_offset, -1);
  BKE_pbvh_bmesh_add_face(data->pbvh, f, false, true);

  int ni = BM_ELEM_CD_GET_INT(f, data->cd_face_node_offset);
  PBVHNode *node = BKE_pbvh_get_node(data->pbvh, ni);

  BMLoop *l = f->l_first;
  do {
    *(int *)BM_ELEM_CD_GET_VOID_P(l->v, data->cd_boundary_flag) |= SCULPT_BOUNDARY_NEEDS_UPDATE;

    *BM_ELEM_CD_PTR<uint8_t *>(l->v, data->cd_flags) |= SCULPTFLAG_NEED_DISK_SORT |
                                                        SCULPTFLAG_NEED_VALENCE;

    if (f->len > 3) {
      *BM_ELEM_CD_PTR<uint8_t *>(l->v, data->cd_flags) |= SCULPTFLAG_NEED_TRIANGULATE;
    }

    int ni_l = BM_ELEM_CD_GET_INT(l->v, data->cd_vert_node_offset);

    if (ni_l < 0 && ni >= 0) {
      BM_ELEM_CD_SET_INT(l->v, ni_l, ni);
      TableGSet *bm_unique_verts = BKE_pbvh_bmesh_node_unique_verts(node);

      BLI_table_gset_add(bm_unique_verts, l->v);
    }
  } while ((l = l->next) != f->l_first);

  data->balance_pbvh = true;
}
static void bmesh_undo_full_mesh(void *userdata)
{
  BmeshUndoData *data = (BmeshUndoData *)userdata;

  if (data->pbvh) {
    BKE_pbvh_bmesh_update_all_valence(data->pbvh);
  }

  data->do_full_recalc = true;
}

static void bmesh_undo_on_edge_change(BMEdge * /*v*/,
                                      void * /*userdata*/,
                                      void * /*old_customdata*/)
{
}

static void bmesh_undo_on_edge_kill(BMEdge *e, void *userdata)
{
  BmeshUndoData *data = (BmeshUndoData *)userdata;

  *(int *)BM_ELEM_CD_GET_VOID_P(e->v1, data->cd_boundary_flag) |= SCULPT_BOUNDARY_NEEDS_UPDATE;
  *(int *)BM_ELEM_CD_GET_VOID_P(e->v2, data->cd_boundary_flag) |= SCULPT_BOUNDARY_NEEDS_UPDATE;

  *BM_ELEM_CD_PTR<uint8_t *>(e->v1, data->cd_flags) |= SCULPTFLAG_NEED_DISK_SORT |
                                                       SCULPTFLAG_NEED_VALENCE |
                                                       SCULPTFLAG_NEED_TRIANGULATE;
  *BM_ELEM_CD_PTR<uint8_t *>(e->v2, data->cd_flags) |= SCULPTFLAG_NEED_DISK_SORT |
                                                       SCULPTFLAG_NEED_VALENCE |
                                                       SCULPTFLAG_NEED_TRIANGULATE;
};
;
static void bmesh_undo_on_edge_add(BMEdge *e, void *userdata)
{
  BmeshUndoData *data = (BmeshUndoData *)userdata;

  *(int *)BM_ELEM_CD_GET_VOID_P(e->v1, data->cd_boundary_flag) |= SCULPT_BOUNDARY_NEEDS_UPDATE;
  *(int *)BM_ELEM_CD_GET_VOID_P(e->v2, data->cd_boundary_flag) |= SCULPT_BOUNDARY_NEEDS_UPDATE;

  *BM_ELEM_CD_PTR<uint8_t *>(e->v1, data->cd_flags) |= SCULPTFLAG_NEED_DISK_SORT |
                                                       SCULPTFLAG_NEED_VALENCE |
                                                       SCULPTFLAG_NEED_TRIANGULATE;
  *BM_ELEM_CD_PTR<uint8_t *>(e->v2, data->cd_flags) |= SCULPTFLAG_NEED_DISK_SORT |
                                                       SCULPTFLAG_NEED_VALENCE |
                                                       SCULPTFLAG_NEED_TRIANGULATE;
}

static void bmesh_undo_on_vert_change(BMVert *v, void *userdata, void *old_customdata)
{
  BmeshUndoData *data = (BmeshUndoData *)userdata;

  if (!old_customdata) {
    BM_ELEM_CD_SET_INT(v, data->cd_vert_node_offset, -1);
    data->regen_all_unique_verts = true;
    return;
  }

  BMElem h;
  h.head.data = old_customdata;

  int ni = BM_ELEM_CD_GET_INT(&h, data->cd_vert_node_offset);

  /* Attempt to find old node reference. */
  PBVHNode *node = BKE_pbvh_get_node_leaf_safe(data->pbvh, ni);
  if (node) {
    /* Make sure undo customdata didn't override node ref. */
    BKE_pbvh_node_mark_update(node);
    BM_ELEM_CD_SET_INT(v, data->cd_vert_node_offset, ni);
  }
  else {
    if (ni != DYNTOPO_NODE_NONE) {
      printf("%s: error: corrupted vertex. ni: %d, cd_node_offset: %d\n",
             __func__,
             ni,
             data->cd_vert_node_offset_old);
      BM_ELEM_CD_SET_INT(v, data->cd_vert_node_offset, DYNTOPO_NODE_NONE);
    }

    // data->regen_all_unique_verts = true;
  }

  return;
  // preserve pbvh node references

  int oldnode_i = BM_ELEM_CD_GET_INT(&h, data->cd_vert_node_offset);

  BM_ELEM_CD_SET_INT(v, data->cd_vert_node_offset, oldnode_i);

  if (oldnode_i >= 0) {
    PBVHNode *node = BKE_pbvh_node_from_index(data->pbvh, oldnode_i);
    BKE_pbvh_node_mark_update(node);
  }
}

static void bmesh_undo_on_face_change(BMFace *f,
                                      void *userdata,
                                      void *old_customdata,
                                      char old_hflag)
{
  BmeshUndoData *data = (BmeshUndoData *)userdata;

  if (!old_customdata) {
    data->do_full_recalc = true;  // can't recover?
    return;
  }

  BMElem h;
  h.head.data = old_customdata;

  int ni = BM_ELEM_CD_GET_INT(&h, data->cd_face_node_offset);

  BMLoop *l = f->l_first;
  do {
    int flag = BM_ELEM_CD_GET_INT(l->v, data->cd_boundary_flag);
    BM_ELEM_CD_SET_INT(l->v, data->cd_boundary_flag, flag | SCULPT_BOUNDARY_NEEDS_UPDATE);
  } while ((l = l->next) != f->l_first);

  // attempt to find old node in old_customdata
  PBVHNode *node = BKE_pbvh_get_node_leaf_safe(data->pbvh, ni);
  if (node) {
    BM_ELEM_CD_SET_INT(f, data->cd_face_node_offset, ni);
    BKE_pbvh_node_mark_update(node);

    if ((old_hflag & BM_ELEM_HIDDEN) != (f->head.hflag & BM_ELEM_HIDDEN)) {
      BKE_pbvh_node_mark_update_visibility(node);
    }
  }
  else {
    printf("pbvh face undo error\n");
    data->do_full_recalc = true;
    BM_ELEM_CD_SET_INT(f, data->cd_face_node_offset, -1);
  }
}

static void update_unode_bmesh_memsize(SculptUndoNode *unode)
{
  // update memory size
  UndoSculpt *usculpt = sculpt_undo_get_nodes();

  if (!usculpt) {
    return;
  }

  // subtract old size
  if (usculpt->undo_size >= unode->undo_size) {
    usculpt->undo_size -= unode->undo_size;
  }

  unode->undo_size = BM_log_entry_size(unode->bm_entry);
  //printf("unode->unode_size: size: %.4fmb\n", __func__, float(unode->undo_size) / 1024.0f / 1024.0f);

  // add new size
  usculpt->undo_size += unode->undo_size;
}

static void bmesh_undo_customdata_change(CustomData *domain, char htype, void *userdata)
{
  BmeshUndoData *data = (BmeshUndoData *)userdata;

  if (htype == BM_VERT) {
    data->cd_vert_node_offset_old = CustomData_get_offset_named(
        domain, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(dyntopo_node_id_vertex));
  }
  else if (htype == BM_FACE) {
    data->cd_face_node_offset_old = CustomData_get_offset_named(
        domain, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(dyntopo_node_id_face));
  }
}

static void sculpt_undo_bmesh_restore_generic(SculptUndoNode *unode, Object *ob, SculptSession *ss)
{
  BmeshUndoData data = {ss->pbvh,
                        ss->bm,
                        false,
                        false,
                        ss->cd_face_node_offset,
                        ss->cd_vert_node_offset,
                        -1,
                        -1,
                        ss->attrs.boundary_flags->bmesh_cd_offset,
                        ss->attrs.flags->bmesh_cd_offset,
                        false,
                        !unode->applied};

  BMLogCallbacks callbacks = {bmesh_undo_on_vert_add,
                              bmesh_undo_on_vert_kill,
                              bmesh_undo_on_vert_change,
                              bmesh_undo_on_edge_add,
                              bmesh_undo_on_edge_kill,
                              bmesh_undo_on_edge_change,
                              bmesh_undo_on_face_add,
                              bmesh_undo_on_face_kill,
                              bmesh_undo_on_face_change,
                              bmesh_undo_full_mesh,
                              bmesh_undo_customdata_change,
                              (void *)&data};

  BKE_sculptsession_update_attr_refs(ob);

  // pbvh_bmesh_check_nodes(ss->pbvh);

  if (unode->applied) {
    BM_log_undo(ss->bm, ss->bm_log, &callbacks);
    unode->applied = false;
  }
  else {
    BM_log_redo(ss->bm, ss->bm_log, &callbacks);
    unode->applied = true;
  }

  BKE_pbvh_bmesh_check_nodes(ss->pbvh);
  update_unode_bmesh_memsize(unode);

  if (!data.do_full_recalc) {
    Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, nullptr, nullptr);

    if (data.regen_all_unique_verts) {
      for (PBVHNode *node : nodes) {
        BKE_pbvh_bmesh_mark_node_regen(ss->pbvh, node);
      }
    }

    // pbvh_bmesh_check_nodes(ss->pbvh);
    BKE_pbvh_bmesh_regen_node_verts(ss->pbvh, false);
    // pbvh_bmesh_check_nodes(ss->pbvh);

    BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateBB | PBVH_UpdateOriginalBB | PBVH_UpdateRedraw);

    if (data.balance_pbvh) {
      BKE_pbvh_bmesh_after_stroke(ss->pbvh, true);
    }

    // pbvh_bmesh_check_nodes(ss->pbvh);
  }
  else {
    printf("undo triggered pbvh rebuild");
    SCULPT_pbvh_clear(ob);
  }
}

/* Create empty sculpt BMesh and enable logging. */
static void sculpt_undo_bmesh_enable(Object *ob, SculptUndoNode *unode, bool is_redo)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = static_cast<Mesh *>(ob->data);

  SCULPT_pbvh_clear(ob);

  ss->active_face.i = ss->active_vertex.i = 0;

  /* Create empty BMesh and enable logging. */
  ss->bm = SCULPT_dyntopo_empty_bmesh();
#if 0
  ss->bm = BM_mesh_create(&bm_mesh_allocsize_default,
                          &((struct BMeshCreateParams){.use_toolflags = false,
                                                       .create_unique_ids = true,
                                                       .id_elem_mask = BM_VERT | BM_EDGE | BM_FACE,
                                                       .id_map = true,
                                                       .temporary_ids = false,
                                                       .no_reuse_ids = false}));
#endif

  BMeshFromMeshParams params = {0};
  params.use_shapekey = true;
  params.create_shapekey_layers = true;
  params.active_shapekey = ob->shapenr;

  BM_mesh_bm_from_me(ss->bm, me, &params);

  /* Calculate normals. */
  BM_mesh_normals_update(ss->bm);

  BKE_sculptsession_update_attr_refs(ob);

  if (ss->pbvh) {
    blender::bke::paint::load_all_original(ob);
  }

  if (ss->bm_idmap) {
    BM_idmap_destroy(ss->bm_idmap);
  }

  ss->bm_idmap = BM_idmap_new(ss->bm, BM_VERT | BM_EDGE | BM_FACE);
  BM_idmap_check_ids(ss->bm_idmap);

  if (!ss->bm_log) {
    /* Restore the BMLog using saved entries. */
    ss->bm_log = BM_log_from_existing_entries_create(ss->bm, ss->bm_idmap, unode->bm_entry);
    BMLogEntry *entry = is_redo ? BM_log_entry_prev(unode->bm_entry) : unode->bm_entry;

    BM_log_set_current_entry(ss->bm_log, entry);
  }
  else {
    BM_log_set_idmap(ss->bm_log, ss->bm_idmap);
  }

  if (!CustomData_has_layer(&ss->bm->vdata, CD_PAINT_MASK)) {
    BM_data_layer_add(ss->bm, &ss->bm->vdata, CD_PAINT_MASK);
    BKE_sculptsession_update_attr_refs(ob);
  }

  SCULPT_update_all_valence_boundary(ob);

  me->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;
}

static void sculpt_undo_bmesh_restore_begin(
    bContext *C, SculptUndoNode *unode, Object *ob, SculptSession *ss, int dir)
{
  if (unode->applied) {
    if (ss->bm && ss->bm_log) {
      /*note that we can't log ids here.
        not entirely sure why, and in thoery it shouldn't be necassary.
        ids end up corrupted.
        */

#if 1
      // BM_log_all_ids(ss->bm, ss->bm_log, unode->bm_entry);

      // need to run bmlog undo on empty log,
      // getting a refcount error in the log
      // ref counting system otherwise

      if (dir == -1) {
        BM_log_undo_skip(ss->bm, ss->bm_log);
      }
      else {
        BM_log_redo_skip(ss->bm, ss->bm_log);
      }
#endif
    }

    BKE_pbvh_bmesh_check_nodes(ss->pbvh);
    SCULPT_dynamic_topology_disable(C, unode);
    unode->applied = false;
  }
  else {
    /*load bmesh from mesh data*/
    sculpt_undo_bmesh_enable(ob, unode, true);

#if 1
    // need to run bmlog undo on empty log,
    // getting a refcount error in the log
    // ref counting system otherwise

    if (dir == 1) {
      BM_log_redo(ss->bm, ss->bm_log, nullptr);
    }
    else {
      BM_log_undo(ss->bm, ss->bm_log, nullptr);
    }
#endif

    unode->applied = true;
  }

  if (ss->bm) {
    BM_mesh_elem_index_ensure(ss->bm, BM_VERT | BM_FACE);
  }
}

static void sculpt_undo_bmesh_restore_end(
    bContext *C, SculptUndoNode *unode, Object *ob, SculptSession *ss, int dir)
{

  if (unode->applied) {
    /*load bmesh from mesh data*/
    sculpt_undo_bmesh_enable(ob, unode, false);

#if 1
    // need to run bmlog undo on empty log,
    // getting a refcount error in the log
    // ref counting system otherwise

    if (dir == -1) {
      BM_log_undo(ss->bm, ss->bm_log, nullptr);
    }
    else {
      BM_log_redo(ss->bm, ss->bm_log, nullptr);
    }

    SCULPT_pbvh_clear(ob);
    BKE_sculptsession_update_attr_refs(ob);

    BMIter iter;
    BMVert *v;
    BMFace *f;

    BM_ITER_MESH (v, &iter, ss->bm, BM_VERTS_OF_MESH) {
      BM_ELEM_CD_SET_INT(v, ss->cd_vert_node_offset, DYNTOPO_NODE_NONE);
    }
    BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
      BM_ELEM_CD_SET_INT(f, ss->cd_face_node_offset, DYNTOPO_NODE_NONE);
    }
#endif

    unode->applied = false;
  }
  else {
#if 1
    if (ss->bm && ss->bm_log) {
      // need to run bmlog undo on empty log,
      // getting a refcount error in the log
      // ref counting system otherwise

      if (dir == -1) {
        BM_log_undo_skip(ss->bm, ss->bm_log);
      }
      else {
        BM_log_redo_skip(ss->bm, ss->bm_log);
      }
    }
#endif

    /* Disable dynamic topology sculpting. */
    SCULPT_dynamic_topology_disable(C, nullptr);
    unode->applied = true;
  }

  if (ss->bm) {
    BM_mesh_elem_index_ensure(ss->bm, BM_VERT | BM_FACE);
  }
}

static void sculpt_undo_geometry_store_data(SculptUndoNodeGeometry *geometry, Object *object)
{
  Mesh *mesh = static_cast<Mesh *>(object->data);

  BLI_assert(!geometry->is_initialized);
  geometry->is_initialized = true;

  CustomData_copy(&mesh->vdata, &geometry->vdata, CD_MASK_MESH.vmask, mesh->totvert);
  CustomData_copy(&mesh->edata, &geometry->edata, CD_MASK_MESH.emask, mesh->totedge);
  CustomData_copy(&mesh->ldata, &geometry->ldata, CD_MASK_MESH.lmask, mesh->totloop);
  CustomData_copy(&mesh->pdata, &geometry->pdata, CD_MASK_MESH.pmask, mesh->totpoly);
  blender::implicit_sharing::copy_shared_pointer(mesh->poly_offset_indices,
                                                 mesh->runtime->poly_offsets_sharing_info,
                                                 &geometry->poly_offset_indices,
                                                 &geometry->poly_offsets_sharing_info);

  geometry->totvert = mesh->totvert;
  geometry->totedge = mesh->totedge;
  geometry->totloop = mesh->totloop;
  geometry->totpoly = mesh->totpoly;
}

static void sculpt_undo_geometry_restore_data(SculptUndoNodeGeometry *geometry, Object *object)
{
  Mesh *mesh = static_cast<Mesh *>(object->data);

  BLI_assert(geometry->is_initialized);

  BKE_mesh_clear_geometry(mesh);

  mesh->totvert = geometry->totvert;
  mesh->totedge = geometry->totedge;
  mesh->totloop = geometry->totloop;
  mesh->totpoly = geometry->totpoly;
  mesh->totface = 0;

  CustomData_copy(&geometry->vdata, &mesh->vdata, CD_MASK_MESH.vmask, geometry->totvert);
  CustomData_copy(&geometry->edata, &mesh->edata, CD_MASK_MESH.emask, geometry->totedge);
  CustomData_copy(&geometry->ldata, &mesh->ldata, CD_MASK_MESH.lmask, geometry->totloop);
  CustomData_copy(&geometry->pdata, &mesh->pdata, CD_MASK_MESH.pmask, geometry->totpoly);
  blender::implicit_sharing::copy_shared_pointer(geometry->poly_offset_indices,
                                                 geometry->poly_offsets_sharing_info,
                                                 &mesh->poly_offset_indices,
                                                 &mesh->runtime->poly_offsets_sharing_info);
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
  blender::implicit_sharing::free_shared_data(&geometry->poly_offset_indices,
                                              &geometry->poly_offsets_sharing_info);
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
static int sculpt_undo_bmesh_restore(
    bContext *C, SculptUndoNode *unode, Object *ob, SculptSession *ss, int dir)
{
  // handle transition from another undo type

#ifdef WHEN_GLOBAL_UNDO_WORKS
  if (!ss->bm_log && ss->bm && unode->bm_entry) {  // && BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    ss->bm_log = BM_log_from_existing_entries_create(ss->bm, ss->bm_idmap, unode->bm_entry);
  }
#endif

  if (ss->bm_log && ss->bm &&
      !ELEM(unode->type, SCULPT_UNDO_DYNTOPO_BEGIN, SCULPT_UNDO_DYNTOPO_END)) {
    BKE_sculptsession_update_attr_refs(ob);

#if 0
    if (ss->active_face.i && ss->active_face.i != -1LL) {
      ss->active_face.i = (intptr_t)BM_log_face_id_get(ss->bm_log,
                                                             (BMFace *)ss->active_face.i);
    }
    else {
      ss->active_face.i = -1;
    }

    if (ss->active_vertex.i && ss->active_vertex.i != -1LL) {
      ss->active_vertex.i = (intptr_t)BM_log_vert_id_get(
          ss->bm_log, (BMVert *)ss->active_vertex.i);
    }
    else {
      ss->active_vertex.i = -1;
    }
#endif
    ss->active_face.i = ss->active_vertex.i = -1;
  }
  else {
    ss->active_face.i = ss->active_vertex.i = -1;
  }

  bool ret = false;
  bool set_active_vertex = true;

  switch (unode->type) {
    case SCULPT_UNDO_DYNTOPO_BEGIN:
      sculpt_undo_bmesh_restore_begin(C, unode, ob, ss, dir);
      SCULPT_vertex_random_access_ensure(ss);

      ss->active_face.i = ss->active_vertex.i = 0;
      set_active_vertex = false;

      ret = true;
      break;
    case SCULPT_UNDO_DYNTOPO_END:
      ss->active_face.i = ss->active_vertex.i = 0;
      set_active_vertex = false;

      sculpt_undo_bmesh_restore_end(C, unode, ob, ss, dir);
      SCULPT_vertex_random_access_ensure(ss);

      ret = true;
      break;
    default:
      if (ss->bm_log) {
        sculpt_undo_bmesh_restore_generic(unode, ob, ss);
        SCULPT_vertex_random_access_ensure(ss);

        if (unode->type == SCULPT_UNDO_HIDDEN) {
          BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateVisibility);
        }
        ret = true;
      }
      break;
  }

  if (ss->pbvh && BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    BKE_pbvh_flush_tri_areas(ss->pbvh);
  }

  if (set_active_vertex && ss->bm_log && ss->bm) {
    if (ss->active_face.i != -1) {
      BMFace *f = BM_log_id_face_get(ss->bm, ss->bm_log, (uint)ss->active_face.i);
      if (f && f->head.htype == BM_FACE) {
        ss->active_face.i = (intptr_t)f;
      }
      else {
        ss->active_face.i = 0LL;
      }
    }
    else {
      ss->active_face.i = 0LL;
    }

    if (ss->active_vertex.i != -1) {
      BMVert *v = BM_log_id_vert_get(ss->bm, ss->bm_log, (uint)ss->active_vertex.i);

      if (v && v->head.htype == BM_VERT) {
        ss->active_vertex.i = (intptr_t)v;
      }
      else {
        ss->active_vertex.i = 0LL;
      }
    }
    else {
      ss->active_vertex.i = 0LL;
    }
  }
  else {
    ss->active_face.i = ss->active_vertex.i = 0;
  }

  return ret;
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
      depsgraph, object, ss->multires.modifier, nullptr);

  BKE_subdiv_eval_refine_from_mesh(
      subdiv, static_cast<const Mesh *>(object->data), deformed_verts);

  MEM_freeN(deformed_verts);
}

static void sculpt_undo_restore_list(bContext *C, Depsgraph *depsgraph, ListBase *lb, int dir)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  SculptSession *ss = ob->sculpt;
  SubdivCCG *subdiv_ccg = ss->subdiv_ccg;
  SculptUndoNode *unode;
  bool update = false, rebuild = false, update_mask = false, update_visibility = false;
  bool update_face_sets = false;
  bool need_mask = false;
  bool need_refine_subdiv = false;
  //  bool did_first_hack = false;

  bool clear_automask_cache = false;

  for (unode = static_cast<SculptUndoNode *>(lb->first); unode; unode = unode->next) {
    if (!ELEM(unode->type, SCULPT_UNDO_COLOR, SCULPT_UNDO_MASK)) {
      clear_automask_cache = true;
    }

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

  if (clear_automask_cache) {
    ss->last_automasking_settings_hash = 0;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);

  sculpt_undo_print_nodes(ob, nullptr);

  if (lb->first != nullptr) {
    /* Only do early object update for edits if first node needs this.
     * Undo steps like geometry does not need object to be updated before they run and will
     * ensure object is updated after the node is handled. */
    const SculptUndoNode *first_unode = (const SculptUndoNode *)lb->first;
    if (first_unode->type != SCULPT_UNDO_GEOMETRY &&
        first_unode->type != SCULPT_UNDO_DYNTOPO_BEGIN) {
      BKE_sculpt_update_object_for_edit(depsgraph, ob, false, need_mask, false);
    }

    if (sculpt_undo_bmesh_restore(C, (SculptUndoNode *)lb->first, ob, ss, dir)) {
      return;
    }
  }

  /* The PBVH already keeps track of which vertices need updated normals, but it doesn't keep
   * track of other updated. In order to tell the corresponding PBVH nodes to update, keep track
   * of which elements were updated for specific layers. */
  bool *modified_hidden_verts = nullptr;
  bool *modified_mask_verts = nullptr;
  bool *modified_color_verts = nullptr;
  bool *modified_face_set_faces = nullptr;
  char *undo_modified_grids = nullptr;
  bool use_multires_undo = false;

  for (unode = static_cast<SculptUndoNode *>(lb->first); unode; unode = unode->next) {

    if (!STREQ(unode->idname, ob->id.name)) {
      continue;
    }

    /* Check if undo data matches current data well enough to
     * continue. */
    if (unode->maxvert) {
      if (ss->totvert != unode->maxvert) {
        printf("error! %s\n", __func__);
        continue;
      }
    }
    else if (unode->maxgrid && subdiv_ccg != nullptr) {
      if ((subdiv_ccg->num_grids != unode->maxgrid) || (subdiv_ccg->grid_size != unode->gridsize))
      {
        continue;
      }

      use_multires_undo = true;
    }

    switch (unode->type) {
      case SCULPT_UNDO_NO_TYPE:
        BLI_assert_unreachable();
        break;
      case SCULPT_UNDO_COORDS:
        if (sculpt_undo_restore_coords(C, depsgraph, unode)) {
          update = true;
        }
        break;
      case SCULPT_UNDO_HIDDEN:
        if (modified_hidden_verts == nullptr) {
          modified_hidden_verts = static_cast<bool *>(
              MEM_calloc_arrayN(ss->totvert, sizeof(bool), __func__));
        }
        if (sculpt_undo_restore_hidden(C, unode, modified_hidden_verts)) {
          rebuild = true;
          update_visibility = true;
        }
        break;
      case SCULPT_UNDO_MASK:
        if (modified_mask_verts == nullptr) {
          modified_mask_verts = static_cast<bool *>(
              MEM_calloc_arrayN(ss->totvert, sizeof(bool), __func__));
        }
        if (sculpt_undo_restore_mask(C, unode, modified_mask_verts)) {
          update = true;
          update_mask = true;
        }
        break;
      case SCULPT_UNDO_FACE_SETS:
        if (modified_face_set_faces == nullptr) {
          modified_face_set_faces = static_cast<bool *>(
              MEM_calloc_arrayN(BKE_pbvh_num_faces(ss->pbvh), sizeof(bool), __func__));
        }
        if (sculpt_undo_restore_face_sets(C, unode, modified_face_set_faces)) {
          update = true;
          update_face_sets = true;
        }
        break;
      case SCULPT_UNDO_COLOR:
        if (modified_color_verts == nullptr) {
          modified_color_verts = static_cast<bool *>(
              MEM_calloc_arrayN(ss->totvert, sizeof(bool), __func__));
        }
        if (sculpt_undo_restore_color(C, unode, modified_color_verts)) {
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
        BLI_assert_msg(0, "Dynamic topology should've already been handled");
        break;
    }
  }

  if (use_multires_undo) {
    for (unode = static_cast<SculptUndoNode *>(lb->first); unode; unode = unode->next) {
      if (!STREQ(unode->idname, ob->id.name)) {
        continue;
      }
      if (unode->maxgrid == 0) {
        continue;
      }

      if (undo_modified_grids == nullptr) {
        undo_modified_grids = static_cast<char *>(
            MEM_callocN(sizeof(char) * unode->maxgrid, "undo_grids"));
      }

      for (int i = 0; i < unode->totgrid; i++) {
        undo_modified_grids[unode->grids[i]] = 1;
      }
    }
  }

  if (subdiv_ccg != nullptr && need_refine_subdiv) {
    sculpt_undo_refine_subdiv(depsgraph, ss, ob, subdiv_ccg->subdiv);
  }

  if (update || rebuild) {
    bool tag_update = false;
    /* We update all nodes still, should be more clever, but also
     * needs to work correct when exiting/entering sculpt mode and
     * the nodes get recreated, though in that case it could do all. */
    PartialUpdateData data{};
    data.rebuild = rebuild;
    data.ss = ss;
    data.pbvh = ss->pbvh;
    data.modified_grids = undo_modified_grids;
    data.modified_hidden_verts = modified_hidden_verts;
    data.modified_mask_verts = modified_mask_verts;
    data.modified_color_verts = modified_color_verts;
    data.modified_face_set_faces = modified_face_set_faces;
    BKE_pbvh_search_callback(ss->pbvh, nullptr, nullptr, update_cb_partial, &data);
    BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateBB | PBVH_UpdateOriginalBB | PBVH_UpdateRedraw);

    if (update_mask) {
      BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateMask);
    }
    if (update_face_sets) {
      DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);
      BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_RebuildDrawBuffers);
    }

    if (update_visibility) {
      if (ELEM(BKE_pbvh_type(ss->pbvh), PBVH_FACES, PBVH_GRIDS)) {
        Mesh *me = (Mesh *)ob->data;
        BKE_pbvh_sync_visibility_from_verts(ss->pbvh, me);

        BKE_pbvh_update_hide_attributes_from_mesh(ss->pbvh);
      }

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

    tag_update |= ID_REAL_USERS(ob->data) > 1 || !BKE_sculptsession_use_pbvh_draw(ob, rv3d) ||
                  ss->shapekey_active || ss->deform_modifiers_active;

    if (tag_update) {
      Mesh *mesh = static_cast<Mesh *>(ob->data);
      BKE_mesh_tag_positions_changed(mesh);

      BKE_sculptsession_free_deformMats(ss);
    }

    if (tag_update) {
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }
    else {
      SCULPT_update_object_bounding_box(ob);
    }
  }

  MEM_SAFE_FREE(modified_hidden_verts);
  MEM_SAFE_FREE(modified_mask_verts);
  MEM_SAFE_FREE(modified_color_verts);
  MEM_SAFE_FREE(modified_face_set_faces);
  MEM_SAFE_FREE(undo_modified_grids);
}

static void sculpt_undo_free_list(ListBase *lb)
{
  SculptUndoNode *unode = (SculptUndoNode *)lb->first;

  while (unode != nullptr) {
    SculptUndoNode *unode_next = unode->next;
    if (unode->co) {
      MEM_freeN(unode->co);
    }

    if (unode->nodemap) {
      MEM_freeN(unode->nodemap);
    }
    if (unode->col) {
      MEM_freeN(unode->col);
    }
    if (unode->loop_col) {
      MEM_freeN(unode->loop_col);
    }
    if (unode->no) {
      MEM_freeN(unode->no);
    }
    if (unode->index) {
      MEM_freeN(unode->index);
    }
    if (unode->faces) {
      MEM_freeN(unode->faces);
    }
    if (unode->loop_index) {
      MEM_freeN(unode->loop_index);
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
      unode->bm_entry = nullptr;
      unode->bm_log = nullptr;
    }

    sculpt_undo_geometry_free_data(&unode->geometry_original);
    sculpt_undo_geometry_free_data(&unode->geometry_modified);

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
  Object *ob = BKE_view_layer_active_object_get(view_layer);
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

SculptUndoNode *SCULPT_undo_get_node(PBVHNode *node, SculptUndoType type)
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();

  if (usculpt == nullptr) {
    return nullptr;
  }

  if (type == SCULPT_UNDO_NO_TYPE) {
    return (SculptUndoNode *)BLI_findptr(&usculpt->nodes, node, offsetof(SculptUndoNode, node));
  }

  LISTBASE_FOREACH (SculptUndoNode *, unode, &usculpt->nodes) {
    if (unode->node == node && unode->type == type) {
      return unode;
    }
  }

  return nullptr;
}

SculptUndoNode *SCULPT_undo_get_first_node()
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();

  if (usculpt == nullptr) {
    return nullptr;
  }

  return static_cast<SculptUndoNode *>(usculpt->nodes.first);
}

static size_t sculpt_undo_alloc_and_store_hidden(PBVH *pbvh, SculptUndoNode *unode)
{
  PBVHNode *node = static_cast<PBVHNode *>(unode->node);
  BLI_bitmap **grid_hidden = BKE_pbvh_grid_hidden(pbvh);

  int *grid_indices, totgrid;
  BKE_pbvh_node_get_grids(pbvh, node, &grid_indices, &totgrid, nullptr, nullptr, nullptr);

  size_t alloc_size = sizeof(*unode->grid_hidden) * size_t(totgrid);
  unode->grid_hidden = static_cast<BLI_bitmap **>(MEM_callocN(alloc_size, "unode->grid_hidden"));

  for (int i = 0; i < totgrid; i++) {
    if (grid_hidden[grid_indices[i]]) {
      unode->grid_hidden[i] = static_cast<BLI_bitmap *>(
          MEM_dupallocN(grid_hidden[grid_indices[i]]));
      alloc_size += MEM_allocN_len(unode->grid_hidden[i]);
    }
    else {
      unode->grid_hidden[i] = nullptr;
    }
  }

  return alloc_size;
}

/* Allocate node and initialize its default fields specific for the given undo type.
 * Will also add the node to the list in the undo step. */
static SculptUndoNode *sculpt_undo_alloc_node_type(Object *object, SculptUndoType type)
{
  const size_t alloc_size = sizeof(SculptUndoNode);
  SculptUndoNode *unode = static_cast<SculptUndoNode *>(MEM_callocN(alloc_size, "SculptUndoNode"));
  STRNCPY(unode->idname, object->id.name);
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

static void sculpt_undo_store_faces(SculptSession *ss, SculptUndoNode *unode)
{
  unode->faces_num = 0;

  PBVHFaceIter fd;
  BKE_pbvh_face_iter_begin (ss->pbvh, static_cast<PBVHNode *>(unode->node), fd) {
    unode->faces_num++;
  }
  BKE_pbvh_face_iter_end(fd);

  unode->faces = static_cast<PBVHFaceRef *>(
      MEM_malloc_arrayN(sizeof(*unode->faces), unode->faces_num, __func__));
  BKE_pbvh_face_iter_begin (ss->pbvh, static_cast<PBVHNode *>(unode->node), fd) {
    unode->faces[fd.i] = fd.face;
  }
  BKE_pbvh_face_iter_end(fd);
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
  int *grids = nullptr;

  SculptUndoNode *unode = sculpt_undo_alloc_node_type(ob, type);
  unode->node = node;

  if (node) {
    BKE_pbvh_node_num_verts(ss->pbvh, node, &totvert, &allvert);
    BKE_pbvh_node_get_grids(ss->pbvh, node, &grids, &totgrid, &maxgrid, &gridsize, nullptr);

    unode->totvert = totvert;
  }

  bool need_loops = type == SCULPT_UNDO_COLOR;
  const bool need_faces = type == SCULPT_UNDO_FACE_SETS;

  if (need_loops) {
    int totloop;

    BKE_pbvh_node_num_loops(ss->pbvh, node, &totloop);

    unode->loop_index = static_cast<int *>(MEM_calloc_arrayN(totloop, sizeof(int), __func__));
    unode->maxloop = 0;
    unode->totloop = totloop;

    size_t alloc_size = sizeof(int) * size_t(totloop);
    usculpt->undo_size += alloc_size;
  }

  if (need_faces) {
    sculpt_undo_store_faces(ss, unode);
    const size_t alloc_size = sizeof(*unode->faces) * size_t(unode->faces_num);
    usculpt->undo_size += alloc_size;
  }

  switch (type) {
    case SCULPT_UNDO_NO_TYPE:
      BLI_assert_unreachable();
      break;
    case SCULPT_UNDO_COORDS: {
      size_t alloc_size = sizeof(*unode->co) * size_t(allvert);
      unode->co = static_cast<float(*)[3]>(MEM_callocN(alloc_size, "SculptUndoNode.co"));
      usculpt->undo_size += alloc_size;

      /* Needed for original data lookup. */
      alloc_size = sizeof(*unode->no) * size_t(allvert);
      unode->no = static_cast<float(*)[3]>(MEM_callocN(alloc_size, "SculptUndoNode.no"));
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
      const size_t alloc_size = sizeof(*unode->mask) * size_t(allvert);
      unode->mask = static_cast<float *>(MEM_callocN(alloc_size, "SculptUndoNode.mask"));
      usculpt->undo_size += alloc_size;
      break;
    }
    case SCULPT_UNDO_COLOR: {
      /* Allocate vertex colors, even for loop colors we still
       * need this for original data lookup. */
      const size_t alloc_size = sizeof(*unode->col) * size_t(allvert);
      unode->col = static_cast<float(*)[4]>(MEM_callocN(alloc_size, "SculptUndoNode.col"));
      usculpt->undo_size += alloc_size;

      /* Allocate loop colors separately too. */
      if (ss->vcol_domain == ATTR_DOMAIN_CORNER) {
        size_t alloc_size_loop = sizeof(float) * 4 * size_t(unode->totloop);

        unode->loop_col = static_cast<float(*)[4]>(
            MEM_calloc_arrayN(unode->totloop, sizeof(float) * 4, "SculptUndoNode.loop_col"));
        usculpt->undo_size += alloc_size_loop;
      }
      break;
    }
    case SCULPT_UNDO_DYNTOPO_BEGIN:
    case SCULPT_UNDO_DYNTOPO_END:
    case SCULPT_UNDO_DYNTOPO_SYMMETRIZE:
      BLI_assert_msg(0, "Dynamic topology should've already been handled");
      break;
    case SCULPT_UNDO_GEOMETRY:
      break;
    case SCULPT_UNDO_FACE_SETS: {
      const size_t alloc_size = sizeof(*unode->face_sets) * size_t(unode->faces_num);
      usculpt->undo_size += alloc_size;
      break;
    }
  }

  if (maxgrid) {
    /* Multires. */
    unode->maxgrid = maxgrid;
    unode->totgrid = totgrid;
    unode->gridsize = gridsize;

    const size_t alloc_size = sizeof(*unode->grids) * size_t(totgrid);
    unode->grids = static_cast<int *>(MEM_callocN(alloc_size, "SculptUndoNode.grids"));
    usculpt->undo_size += alloc_size;
  }
  else {
    /* Regular mesh. */
    unode->maxvert = ss->totvert;

    const size_t alloc_size = sizeof(*unode->index) * size_t(allvert);
    unode->index = static_cast<int *>(MEM_callocN(alloc_size, "SculptUndoNode.index"));
    usculpt->undo_size += alloc_size;
  }

  if (ss->deform_modifiers_active) {
    const size_t alloc_size = sizeof(*unode->orig_co) * size_t(allvert);
    unode->orig_co = static_cast<float(*)[3]>(MEM_callocN(alloc_size, "undoSculpt orig_cos"));
    usculpt->undo_size += alloc_size;
  }

  return unode;
}

static void sculpt_undo_store_coords(Object *ob, SculptUndoNode *unode)
{
  SculptSession *ss = ob->sculpt;
  PBVHVertexIter vd;

  int totvert, allvert;
  BKE_pbvh_node_num_verts(ss->pbvh, (PBVHNode *)unode->node, &totvert, &allvert);

  if (ss->deform_modifiers_active && !unode->orig_co) {
    unode->orig_co = (float(*)[3])MEM_malloc_arrayN(
        allvert, sizeof(float) * 3, "sculpt unode undo coords");
  }

  bool have_grids = BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, ((PBVHNode *)unode->node), vd, PBVH_ITER_ALL) {
    copy_v3_v3(unode->co[vd.i], vd.co);
    if (vd.no) {
      copy_v3_v3(unode->no[vd.i], vd.no);
    }
    else {
      copy_v3_v3(unode->no[vd.i], vd.fno);
    }

    if (ss->deform_modifiers_active) {
      if (!have_grids && ss->shapekey_active) {
        float(*cos)[3] = (float(*)[3])ss->shapekey_active->data;

        copy_v3_v3(unode->orig_co[vd.i], cos[vd.index]);
      }
      else {
        copy_v3_v3(unode->orig_co[vd.i],
                   blender::bke::paint::vertex_attr_ptr<float>(vd.vertex, ss->attrs.orig_co));
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void sculpt_undo_store_hidden(Object *ob, SculptUndoNode *unode)
{
  PBVH *pbvh = ob->sculpt->pbvh;
  PBVHNode *node = static_cast<PBVHNode *>(unode->node);

  const bool *hide_vert = BKE_pbvh_get_vert_hide(pbvh);
  if (hide_vert == nullptr) {
    return;
  }

  if (unode->grids) {
    /* Already stored during allocation. */
  }
  else {
    int allvert;

    BKE_pbvh_node_num_verts(pbvh, node, nullptr, &allvert);
    const int *vert_indices = BKE_pbvh_node_get_vert_indices(node);
    for (int i = 0; i < allvert; i++) {
      BLI_BITMAP_SET(unode->vert_hidden, i, hide_vert[vert_indices[i]]);
    }
  }
}

static void sculpt_undo_store_mask(Object *ob, SculptUndoNode *unode)
{
  SculptSession *ss = ob->sculpt;
  PBVHVertexIter vd;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, static_cast<PBVHNode *>(unode->node), vd, PBVH_ITER_ALL) {
    unode->mask[vd.i] = *vd.mask;
  }
  BKE_pbvh_vertex_iter_end;
}

static void sculpt_undo_store_color(Object *ob, SculptUndoNode *unode)
{
  SculptSession *ss = ob->sculpt;

  BLI_assert(BKE_pbvh_type(ss->pbvh) == PBVH_FACES);

  int allvert;
  BKE_pbvh_node_num_verts(ss->pbvh, static_cast<PBVHNode *>(unode->node), nullptr, &allvert);

  int *indices = sculpt_undo_get_indices32(unode, allvert);

  /* NOTE: even with loop colors we still store (derived)
   * vertex colors for original data lookup. */

  BKE_pbvh_store_colors_vertex(ss->pbvh, indices, allvert, unode->col);

  if (unode->loop_col && unode->totloop) {
    BKE_pbvh_store_colors(ss->pbvh, unode->loop_index, unode->totloop, unode->loop_col);
  }

  MEM_SAFE_FREE(indices);
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

static void sculpt_undo_store_face_sets(SculptSession *ss, SculptUndoNode *unode)
{
  unode->face_sets = static_cast<int *>(
      MEM_malloc_arrayN(sizeof(*unode->face_sets), unode->faces_num, __func__));

  PBVHFaceIter fd;
  BKE_pbvh_face_iter_begin (ss->pbvh, static_cast<PBVHNode *>(unode->node), fd) {
    unode->face_sets[fd.i] = fd.face_set ? *fd.face_set : SCULPT_FACE_SET_NONE;
  }
  BKE_pbvh_face_iter_end(fd);
}

void SCULPT_undo_ensure_bmlog(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = BKE_object_get_original_mesh(ob);

  /* Log exists or object is not in sculpt mode? */
  if (!ss || ss->bm_log) {
    return;
  }

  /* Try to find log from entries in the undo stack. */
  UndoStack *ustack = ED_undo_stack_get();

  if (!ustack) {
    return;
  }

  UndoStep *us = BKE_undosys_stack_active_with_type(ustack, BKE_UNDOSYS_TYPE_SCULPT);

  if (!us) {
    // check next step
    if (ustack->step_active && ustack->step_active->next &&
        ustack->step_active->next->type == BKE_UNDOSYS_TYPE_SCULPT)
    {
      us = ustack->step_active->next;
    }
  }

  if (!us) {
    return;
  }

  UndoSculpt *usculpt = sculpt_undosys_step_get_nodes(us);

  if (!ss->bm && !(me->flag & ME_SCULPT_DYNAMIC_TOPOLOGY)) {
    return;
  }

  if (!usculpt) {
    // happens during file load
    return;
  }

  SculptUndoNode *unode = (SculptUndoNode *)usculpt->nodes.first;

  BKE_sculpt_ensure_idmap(ob);

  /*when transition between undo step types the log might simply
  have been freed, look for entries to rebuild it with*/
  if (!ss->bm_log) {
    if (unode && unode->bm_entry) {
      ss->bm_log = BM_log_from_existing_entries_create(ss->bm, ss->bm_idmap, unode->bm_entry);
    }
    else {
      ss->bm_log = BM_log_create(ss->bm, ss->bm_idmap);
    }

    if (ss->pbvh) {
      BKE_pbvh_set_bm_log(ss->pbvh, ss->bm_log);
    }
  }
}

static SculptUndoNode *sculpt_undo_bmesh_push(Object *ob, PBVHNode *node, SculptUndoType type)
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();
  SculptSession *ss = ob->sculpt;
  PBVHVertexIter vd;

  SculptUndoNode *unode = static_cast<SculptUndoNode *>(usculpt->nodes.first);

  SCULPT_undo_ensure_bmlog(ob);

  if (!ss->bm_log) {
    ss->bm_log = BM_log_create(ss->bm, ss->bm_idmap);
  }

  bool new_node = false;

  if (unode == nullptr) {
    new_node = true;
    unode = (SculptUndoNode *)MEM_callocN(sizeof(*unode), __func__);

    STRNCPY(unode->idname, ob->id.name);
    unode->type = type;
    unode->applied = true;

    /* note that every undo type must push a bm_entry for
       so we can recreate the BMLog from chained entries
       when going to/from other undo system steps */

    if (type == SCULPT_UNDO_DYNTOPO_END) {
      unode->bm_log = ss->bm_log;
      unode->bm_entry = BM_log_entry_add(ss->bm, ss->bm_log);

      BM_log_full_mesh(ss->bm, ss->bm_log);
    }
    else if (type == SCULPT_UNDO_DYNTOPO_BEGIN) {
      unode->bm_log = ss->bm_log;
      unode->bm_entry = BM_log_entry_add(ss->bm, ss->bm_log);

      BM_log_full_mesh(ss->bm, ss->bm_log);
    }
    else {
      unode->bm_log = ss->bm_log;
      unode->bm_entry = BM_log_entry_add(ss->bm, ss->bm_log);
    }

    BLI_addtail(&usculpt->nodes, unode);
  }

  if (node) {
    if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
      unode->bm_log = ss->bm_log;
      unode->bm_entry = BM_log_entry_check_customdata(ss->bm, ss->bm_log);
    }

    switch (type) {
      case SCULPT_UNDO_COORDS:
      case SCULPT_UNDO_MASK:
        BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
          bm_logstack_push();
          BM_log_vert_before_modified(ss->bm, ss->bm_log, vd.bm_vert);
          bm_logstack_pop();
        }
        BKE_pbvh_vertex_iter_end;
        break;

      case SCULPT_UNDO_HIDDEN: {
        TableGSet *faces = BKE_pbvh_bmesh_node_faces(node);
        BMFace *f;

        BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
          bm_logstack_push();
          BM_log_vert_before_modified(ss->bm, ss->bm_log, vd.bm_vert);
          bm_logstack_pop();
        }
        BKE_pbvh_vertex_iter_end;

        TGSET_ITER (f, faces) {
          BM_log_face_modified(ss->bm, ss->bm_log, f);
        }
        TGSET_ITER_END
        break;
      }

      case SCULPT_UNDO_COLOR: {
        Mesh *mesh = BKE_object_get_original_mesh(ob);

        CustomDataLayer *color_layer = BKE_id_attribute_search(
            &mesh->id,
            BKE_id_attributes_active_color_name(&mesh->id),
            CD_MASK_COLOR_ALL,
            ATTR_DOMAIN_MASK_POINT | ATTR_DOMAIN_MASK_CORNER);

        if (!color_layer) {
          break;
        }
        eAttrDomain domain = BKE_id_attribute_domain(&mesh->id, color_layer);

        if (domain == ATTR_DOMAIN_POINT) {
          BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
            bm_logstack_push();
            BM_log_vert_before_modified(ss->bm, ss->bm_log, vd.bm_vert);
            bm_logstack_pop();
          }
          BKE_pbvh_vertex_iter_end;
        }
        else if (domain == ATTR_DOMAIN_CORNER) {
          TableGSet *faces = BKE_pbvh_bmesh_node_faces(node);
          BMFace *f;

          TGSET_ITER (f, faces) {
            BM_log_face_modified(ss->bm, ss->bm_log, f);
          }
          TGSET_ITER_END
        }

        break;
      }
      case SCULPT_UNDO_FACE_SETS: {
        TableGSet *faces = BKE_pbvh_bmesh_node_faces(node);
        BMFace *f;

        TGSET_ITER (f, faces) {
          BM_log_face_modified(ss->bm, ss->bm_log, f);
        }
        TGSET_ITER_END

        break;
      }
      case SCULPT_UNDO_DYNTOPO_BEGIN:
      case SCULPT_UNDO_DYNTOPO_END:
      case SCULPT_UNDO_DYNTOPO_SYMMETRIZE:
      case SCULPT_UNDO_GEOMETRY:
      case SCULPT_UNDO_NO_TYPE:
        break;
    }
  }
  else {
    switch (type) {
      case SCULPT_UNDO_DYNTOPO_SYMMETRIZE:
      case SCULPT_UNDO_GEOMETRY:
        BM_log_full_mesh(ss->bm, ss->bm_log);
        break;
      default:  // to avoid warnings
        break;
    }
  }

  if (new_node) {
    sculpt_undo_print_nodes(ob, nullptr);
  }

  return unode;
}

bool SCULPT_ensure_dyntopo_node_undo(Object *ob,
                                     PBVHNode *node,
                                     SculptUndoType type,
                                     int extraType)
{
  SculptSession *ss = ob->sculpt;

  UndoSculpt *usculpt = sculpt_undo_get_nodes();
  SculptUndoNode *unode = (SculptUndoNode *)usculpt->nodes.first;

  if (!unode) {
    unode = sculpt_undo_alloc_node_type(ob, type);

    BLI_strncpy(unode->idname, ob->id.name, sizeof(unode->idname));

    unode->type = type;
    unode->typemask = 1 << type;
    unode->applied = true;
    unode->bm_log = ss->bm_log;
    unode->bm_entry = BM_log_entry_add(ss->bm, ss->bm_log);

    return SCULPT_ensure_dyntopo_node_undo(ob, node, type, extraType);
  }
  else if (!(unode->typemask & (1 << type))) {
    unode->typemask |= 1 << type;

    /* add a log sub-entry */
    BM_log_entry_add_ex(ss->bm, ss->bm_log, true);
  }

  if (!node) {
    return false;
  }

  int n = BKE_pbvh_get_node_id(ss->pbvh, node);

  if (unode->nodemap_size <= n) {
    int newsize = (n + 1);
    newsize += newsize >> 1;

    if (!unode->nodemap) {
      unode->nodemap = (int *)MEM_callocN(sizeof(*unode->nodemap) * newsize, "unode->nodemap");
    }
    else {
      unode->nodemap = (int *)MEM_recallocN(unode->nodemap, sizeof(*unode->nodemap) * newsize);
    }

    unode->nodemap_size = newsize;
  }

  bool check = !((type | extraType) & (SCULPT_UNDO_COORDS | SCULPT_UNDO_COLOR | SCULPT_UNDO_MASK));
  if (check && unode->nodemap[n] & (1 << type)) {
    return false;
  }

  unode->nodemap[n] |= 1 << type;
  sculpt_undo_bmesh_push(ob, node, type);

  if (extraType >= 0) {
    sculpt_undo_bmesh_push(ob, node, (SculptUndoType)extraType);
  }

  return true;
}

static bool check_first_undo_entry_dyntopo(Object *ob)
{
  UndoStack *ustack = ED_undo_stack_get();
  if (!ustack || !ob->sculpt || !ob->sculpt->bm) {
    return false;
  }

  UndoStep *us = ustack->step_init ? ustack->step_init : ustack->step_active;
  bool bad = false;

  if (!us) {
    bad = true;
  }
  else if (us->type) {
    if (!STREQ(us->type->name, "Sculpt")) {
      bad = true;
    }
    else {
      SculptUndoStep *step = (SculptUndoStep *)us;
      SculptUndoNode *unode = (SculptUndoNode *)step->data.nodes.first;

      if (!unode) {
        bad = true;
      }
      else {
        UndoStep *act = ustack->step_active;

        if (!act->type || !STREQ(act->type->name, "Sculpt")) {
          bad = unode->type != SCULPT_UNDO_DYNTOPO_BEGIN;
        }
      }
    }
  }
  else {
    bad = true;
  }

  if (bad) {
    sculpt_undo_push_begin_ex(ob, "Dyntopo Begin", true);
    SCULPT_undo_push_node(ob, nullptr, SCULPT_UNDO_DYNTOPO_BEGIN);
    SCULPT_undo_push_end(ob);
  }

  return bad;
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
    sculpt_undo_print_nodes(ob, nullptr);
    BLI_thread_unlock(LOCK_CUSTOM1);
    return unode;
  }
  if (type == SCULPT_UNDO_GEOMETRY) {
    unode = sculpt_undo_geometry_push(ob, type);
    sculpt_undo_print_nodes(ob, nullptr);
    BLI_thread_unlock(LOCK_CUSTOM1);
    return unode;
  }
  if ((unode = SCULPT_undo_get_node(node, type))) {
    sculpt_undo_print_nodes(ob, nullptr);
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
    BKE_pbvh_node_get_grids(ss->pbvh, node, &grids, &totgrid, nullptr, nullptr, nullptr);
    memcpy(unode->grids, grids, sizeof(int) * totgrid);
  }
  else {
    const int *loop_indices;
    int allvert, allloop;

    BKE_pbvh_node_num_verts(ss->pbvh, static_cast<PBVHNode *>(unode->node), nullptr, &allvert);
    const int *vert_indices = BKE_pbvh_node_get_vert_indices(node);
    memcpy(unode->index, vert_indices, sizeof(int) * allvert);

    if (unode->loop_index) {
      BKE_pbvh_node_num_loops(ss->pbvh, static_cast<PBVHNode *>(unode->node), &allloop);
      BKE_pbvh_node_get_loops(
          ss->pbvh, static_cast<PBVHNode *>(unode->node), &loop_indices, nullptr);

      if (allloop) {
        memcpy(unode->loop_index, loop_indices, sizeof(int) * allloop);

        unode->maxloop = BKE_object_get_original_mesh(ob)->totloop;
      }
    }
  }

  switch (type) {
    case SCULPT_UNDO_NO_TYPE:
      BLI_assert_unreachable();
      break;
    case SCULPT_UNDO_COORDS:
      sculpt_undo_store_coords(ob, unode);
      break;
    case SCULPT_UNDO_HIDDEN:
      sculpt_undo_store_hidden(ob, unode);
      break;
    case SCULPT_UNDO_MASK:
      if (BKE_pbvh_has_mask(ss->pbvh)) {
        sculpt_undo_store_mask(ob, unode);
      }
      break;
    case SCULPT_UNDO_COLOR:
      sculpt_undo_store_color(ob, unode);
      break;
    case SCULPT_UNDO_DYNTOPO_BEGIN:
    case SCULPT_UNDO_DYNTOPO_END:
    case SCULPT_UNDO_DYNTOPO_SYMMETRIZE:
      BLI_assert_msg(0, "Dynamic topology should've already been handled");
    case SCULPT_UNDO_GEOMETRY:
      break;
    case SCULPT_UNDO_FACE_SETS:
      sculpt_undo_store_face_sets(ss, unode);
      break;
  }

  /* Store sculpt pivot. */
  copy_v3_v3(unode->pivot_pos, ss->pivot_pos);
  copy_v3_v3(unode->pivot_rot, ss->pivot_rot);

  /* Store active shape key. */
  if (ss->shapekey_active) {
    STRNCPY(unode->shapeName, ss->shapekey_active->name);
  }
  else {
    unode->shapeName[0] = '\0';
  }

  sculpt_undo_print_nodes(ob, nullptr);

  BLI_thread_unlock(LOCK_CUSTOM1);

  return unode;
}

static bool sculpt_attribute_ref_equals(SculptAttrRef *a, SculptAttrRef *b)
{
  return a->domain == b->domain && a->type == b->type && STREQ(a->name, b->name);
}

static void sculpt_save_active_attribute(Object *ob, SculptAttrRef *attr)
{
  Mesh *me = BKE_object_get_original_mesh(ob);
  const CustomDataLayer *layer;

  if (ob && me && (layer = BKE_id_attributes_active_get((ID *)me))) {
    attr->domain = BKE_id_attribute_domain((ID *)me, layer);
    BLI_strncpy(attr->name, layer->name, sizeof(attr->name));
    attr->type = eCustomDataType(layer->type);
  }
  else {
    attr->domain = NO_ACTIVE_LAYER;
    attr->name[0] = 0;
  }

  attr->was_set = true;
}

static void sculpt_save_active_attribute_color(Object *ob, SculptAttrRef *attr)
{
  Mesh *me = BKE_object_get_original_mesh(ob);
  CustomDataLayer *layer;

  if (ob && me &&
      (layer = BKE_id_attribute_search(&me->id,
                                       BKE_id_attributes_active_color_name(&me->id),
                                       CD_MASK_COLOR_ALL,
                                       ATTR_DOMAIN_MASK_POINT | ATTR_DOMAIN_MASK_CORNER)))
  {
    attr->domain = BKE_id_attribute_domain((ID *)me, layer);
    BLI_strncpy(attr->name, layer->name, sizeof(attr->name));
    attr->type = eCustomDataType(layer->type);
  }
  else {
    attr->domain = NO_ACTIVE_LAYER;
    attr->name[0] = 0;
  }

  using namespace blender;
  Mesh *mesh = BKE_object_get_original_mesh(ob);
  attr->was_set = true;
  attr->domain = NO_ACTIVE_LAYER;
  attr->name[0] = 0;
  if (!mesh) {
    return;
  }
  const char *name = mesh->active_color_attribute;
  const bke::AttributeAccessor attributes = mesh->attributes();
  const std::optional<bke::AttributeMetaData> meta_data = attributes.lookup_meta_data(name);
  if (!meta_data) {
    return;
  }
  if (!(ATTR_DOMAIN_AS_MASK(meta_data->domain) & ATTR_DOMAIN_MASK_COLOR) ||
      !(CD_TYPE_AS_MASK(meta_data->data_type) & CD_MASK_COLOR_ALL))
  {
    return;
  }
  attr->domain = meta_data->domain;
  STRNCPY(attr->name, name);
  attr->type = meta_data->data_type;
}

static void sculpt_undo_push_begin_ex(Object *ob, const char *name, bool no_first_entry_check)
{
  UndoStack *ustack = ED_undo_stack_get();

  SCULPT_undo_ensure_bmlog(ob);

  if (ob != nullptr) {
    if (!no_first_entry_check && ob->sculpt && ob->sculpt->bm) {
      check_first_undo_entry_dyntopo(ob);
    }

    /* If possible, we need to tag the object and its geometry data as 'changed in the future' in
     * the previous undo step if it's a memfile one. */
    ED_undosys_stack_memfile_id_changed_tag(ustack, &ob->id);
    ED_undosys_stack_memfile_id_changed_tag(ustack, static_cast<ID *>(ob->data));
  }

  /* Special case, we never read from this. */
  bContext *C = nullptr;

  SculptUndoStep *us = (SculptUndoStep *)BKE_undosys_step_push_init_with_type(
      ustack, C, name, BKE_UNDOSYS_TYPE_SCULPT);

  if (!us->active_color_start.was_set) {
    sculpt_save_active_attribute_color(ob, &us->active_color_start);
  }
  if (!us->active_attr_start.was_set) {
    sculpt_save_active_attribute(ob, &us->active_attr_start);
  }

  /* Set end attribute in case SCULPT_undo_push_end is not called,
   * so we don't end up with corrupted state.
   */
  if (!us->active_color_end.was_set) {
    sculpt_save_active_attribute_color(ob, &us->active_color_end);
    us->active_color_end.was_set = false;
  }

  SculptSession *ss = ob->sculpt;

  /* When pushing an undo node after undoing to the start of the stack
   * the log ref count hits zero; we must detect this and handle it.
   */

  if (ss && ss->bm && ss->bm_log && BM_log_is_dead(ss->bm_log)) {
    /* Forcibly destroy all entries (the 'true' parameter). */
    BM_log_free(ss->bm_log, true);

    BKE_sculpt_ensure_idmap(ob);

    ss->bm_log = BM_log_create(ss->bm, ss->bm_idmap);

    if (ss->pbvh) {
      BKE_pbvh_set_bm_log(ss->pbvh, ss->bm_log);
    }
  }
}

void SCULPT_undo_push_begin_ex(Object *ob, const char *name)
{
  return sculpt_undo_push_begin_ex(ob, name, false);
}

void SCULPT_undo_push_begin(Object *ob, const wmOperator *op)
{
  SCULPT_undo_push_begin_ex(ob, op->type->name);
}

void SCULPT_undo_push_end(Object *ob)
{
  SCULPT_undo_push_end_ex(ob, false);
}

void SCULPT_undo_push_end_ex(struct Object *ob, const bool use_nested_undo)
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();
  SculptUndoNode *unode;

  /* We don't need normals in the undo stack. */
  for (unode = (SculptUndoNode *)usculpt->nodes.first; unode; unode = unode->next) {
    if (unode->bm_entry) {
      update_unode_bmesh_memsize(unode);
    }

    if (unode->no) {
      usculpt->undo_size -= MEM_allocN_len(unode->no);
      MEM_freeN(unode->no);
      unode->no = nullptr;
    }
  }

  /* We could remove this and enforce all callers run in an operator using 'OPTYPE_UNDO'. */
  wmWindowManager *wm = static_cast<wmWindowManager *>(G_MAIN->wm.first);
  if (wm->op_undo_depth == 0 || use_nested_undo) {
    UndoStack *ustack = ED_undo_stack_get();
    BKE_undosys_step_push(ustack, nullptr, nullptr);
    if (wm->op_undo_depth == 0) {
      BKE_undosys_stack_limit_steps_and_memory_defaults(ustack);
    }
    WM_file_tag_modified();
  }

  UndoStack *ustack = ED_undo_stack_get();
  SculptUndoStep *us = (SculptUndoStep *)BKE_undosys_stack_init_or_active_with_type(
      ustack, BKE_UNDOSYS_TYPE_SCULPT);

  sculpt_save_active_attribute(ob, &us->active_attr_end);
  sculpt_save_active_attribute_color(ob, &us->active_color_end);
  sculpt_undo_print_nodes(ob, nullptr);
}

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

static void sculpt_undo_set_active_layer(struct bContext *C, SculptAttrRef *attr, bool is_color)
{
  if (attr->domain == ATTR_DOMAIN_AUTO) {
    return;
  }

  Object *ob = CTX_data_active_object(C);
  Mesh *me = BKE_object_get_original_mesh(ob);

  SculptAttrRef existing;
  if (is_color) {
    sculpt_save_active_attribute_color(ob, &existing);
  }
  else {
    sculpt_save_active_attribute(ob, &existing);
  }

  CustomDataLayer *layer;
  layer = BKE_id_attribute_find(&me->id, attr->name, attr->type, attr->domain);

  /* Temporary fix for #97408. This is a fundamental
   * bug in the undo stack; the operator code needs to push
   * an extra undo step before running an operator if a
   * non-memfile undo system is active.
   *
   * For now, detect if the layer does exist but with a different
   * domain and just unconvert it.
   */
  if (!layer) {
    layer = BKE_id_attribute_search(&me->id, attr->name, CD_MASK_PROP_ALL, ATTR_DOMAIN_MASK_ALL);
    if (layer) {
      if (ED_geometry_attribute_convert(
              me, attr->name, eCustomDataType(attr->type), attr->domain, nullptr))
      {
        layer = BKE_id_attribute_find(&me->id, attr->name, attr->type, attr->domain);
      }
    }
  }

  if (!layer) {
    /* Memfile undo killed the layer; re-create it. */
    CustomData *cdata = attr->domain == ATTR_DOMAIN_POINT ? &me->vdata : &me->ldata;
    int totelem = attr->domain == ATTR_DOMAIN_POINT ? me->totvert : me->totloop;

    CustomData_add_layer_named(
        cdata, eCustomDataType(attr->type), CD_SET_DEFAULT, totelem, attr->name);
    layer = BKE_id_attribute_find(&me->id, attr->name, attr->type, attr->domain);
  }

  if (layer) {
    BKE_id_attributes_active_color_set(&me->id, layer->name);

    if (ob->sculpt && ob->sculpt->pbvh) {
      BKE_pbvh_update_active_vcol(ob->sculpt->pbvh, me);

      if (!sculpt_attribute_ref_equals(&existing, attr)) {
        BKE_pbvh_update_vertex_data(ob->sculpt->pbvh, PBVH_UpdateColor);
      }
    }
  }
  else if (layer) {
    BKE_id_attributes_active_set(&me->id, layer->name);
  }
}

static void sculpt_undosys_step_encode_init(struct bContext * /*C*/, UndoStep *us_p)
{
  SculptUndoStep *us = (SculptUndoStep *)us_p;
  /* Dummy, memory is cleared anyway. */
  BLI_listbase_clear(&us->data.nodes);
}

static bool sculpt_undosys_step_encode(struct bContext * /*C*/, struct Main *bmain, UndoStep *us_p)
{
  /* Dummy, encoding is done along the way by adding tiles
   * to the current 'SculptUndoStep' added by encode_init. */
  SculptUndoStep *us = (SculptUndoStep *)us_p;
  us->step.data_size = us->data.undo_size;

  SculptUndoNode *unode = static_cast<SculptUndoNode *>(us->data.nodes.last);
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
  sculpt_undo_restore_list(C, depsgraph, &us->data.nodes, -1);
  us->step.is_applied = false;

  sculpt_undo_print_nodes(CTX_data_active_object(C), us);
}

static void sculpt_undosys_step_decode_redo_impl(struct bContext *C,
                                                 Depsgraph *depsgraph,
                                                 SculptUndoStep *us)
{
  BLI_assert(us->step.is_applied == false);
  sculpt_undo_restore_list(C, depsgraph, &us->data.nodes, 1);
  us->step.is_applied = true;

  sculpt_undo_print_nodes(CTX_data_active_object(C), us);
}

static void sculpt_undosys_step_decode_undo(struct bContext *C,
                                            Depsgraph *depsgraph,
                                            SculptUndoStep *us,
                                            const bool is_final)
{
  /* Walk forward over any applied steps of same type,
   * then walk back in the next loop, un-applying them. */
  SculptUndoStep *us_iter = us;
  while (us_iter->step.next && (us_iter->step.next->type == us_iter->step.type)) {
    if (us_iter->step.next->is_applied == false) {
      break;
    }
    us_iter = (SculptUndoStep *)us_iter->step.next;
  }

  while ((us_iter != us) || (!is_final && us_iter == us)) {
    BLI_assert(us_iter->step.type == us->step.type); /* Previous loop ensures this. */

    sculpt_undo_set_active_layer(C, &((SculptUndoStep *)us_iter)->active_attr_start, false);
    sculpt_undo_set_active_layer(C, &((SculptUndoStep *)us_iter)->active_color_start, true);

    sculpt_undosys_step_decode_undo_impl(C, depsgraph, us_iter);
    // sculpt_undo_set_active_layer(C, &((SculptUndoStep *)us_iter)->active_attr_start);

    if (us_iter == us) {
      if (us_iter->step.prev && us_iter->step.prev->type == BKE_UNDOSYS_TYPE_SCULPT) {
        sculpt_undo_set_active_layer(
            C, &((SculptUndoStep *)us_iter->step.prev)->active_attr_end, false);
        sculpt_undo_set_active_layer(
            C, &((SculptUndoStep *)us_iter->step.prev)->active_color_end, true);
      }
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
    sculpt_undo_set_active_layer(C, &((SculptUndoStep *)us_iter)->active_attr_start, false);
    sculpt_undo_set_active_layer(C, &((SculptUndoStep *)us_iter)->active_color_start, true);
    sculpt_undosys_step_decode_redo_impl(C, depsgraph, us_iter);

    if (us_iter == us) {
      sculpt_undo_set_active_layer(C, &((SculptUndoStep *)us_iter)->active_attr_end, false);
      sculpt_undo_set_active_layer(C, &((SculptUndoStep *)us_iter)->active_color_end, true);
      break;
    }
    us_iter = (SculptUndoStep *)us_iter->step.next;
  }
}

static void sculpt_undosys_step_decode(
    struct bContext *C, struct Main *bmain, UndoStep *us_p, const eUndoStepDir dir, bool is_final)
{
  /* NOTE: behavior for undo/redo closely matches image undo. */
  BLI_assert(dir != STEP_INVALID);

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  /* Ensure sculpt mode. */
  {
    Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    BKE_view_layer_synced_ensure(scene, view_layer);
    Object *ob = BKE_view_layer_active_object_get(view_layer);
    if (ob && (ob->type == OB_MESH)) {
      if (ob->mode & (OB_MODE_SCULPT | OB_MODE_VERTEX_PAINT)) {
        /* Pass. */
      }
      else {
        ED_object_mode_generic_exit(bmain, depsgraph, scene, ob);

        /* Sculpt needs evaluated state.
         * NOTE: needs to be done here, as #ED_object_mode_generic_exit will usually invalidate
         * (some) evaluated data. */
        BKE_scene_graph_evaluated_ensure(depsgraph, bmain);

#ifndef WHEN_GLOBAL_UNDO_WORKS
        /* Don't add sculpt topology undo steps when reading back undo state.
         * The undo steps must enter/exit for us. */
        me->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;
#endif
        ED_object_sculptmode_enter_ex(bmain, depsgraph, scene, ob, true, nullptr, false);
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

void ED_sculpt_undo_geometry_begin(struct Object *ob, const wmOperator *op)
{
  SCULPT_undo_push_begin(ob, op);
  SCULPT_undo_push_node(ob, nullptr, SCULPT_UNDO_GEOMETRY);
}

void ED_sculpt_undo_geometry_begin_ex(struct Object *ob, const char *name)
{
  SCULPT_undo_push_begin_ex(ob, name);
  SCULPT_undo_push_node(ob, nullptr, SCULPT_UNDO_GEOMETRY);
}

void ED_sculpt_undo_geometry_end(struct Object *ob)
{
  SCULPT_undo_push_node(ob, nullptr, SCULPT_UNDO_GEOMETRY);
  SCULPT_undo_push_end(ob);
}

void ED_sculpt_undosys_type(UndoType *ut)
{
  ut->name = "Sculpt";
  ut->poll = nullptr; /* No poll from context for now. */
  ut->step_encode_init = sculpt_undosys_step_encode_init;
  ut->step_encode = sculpt_undosys_step_encode;
  ut->step_decode = sculpt_undosys_step_decode;
  ut->step_free = sculpt_undosys_step_free;

  ut->flags = UNDOTYPE_FLAG_DECODE_ACTIVE_STEP;

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

  if (!ustack) {  // happens during file load
    return nullptr;
  }

  UndoStep *us = BKE_undosys_stack_init_or_active_with_type(ustack, BKE_UNDOSYS_TYPE_SCULPT);
  return us ? sculpt_undosys_step_get_nodes(us) : nullptr;
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
   * PBVH for the new base geometry, which will have same coordinates as if we create PBVH here.
   */
  if (ss->pbvh == nullptr) {
    return;
  }

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, nullptr, nullptr);
  for (PBVHNode *node : nodes) {
    SculptUndoNode *unode = SCULPT_undo_push_node(object, node, SCULPT_UNDO_COORDS);
    unode->node = nullptr;
  }
}

void ED_sculpt_undo_push_multires_mesh_begin(bContext *C, const char *str)
{
  if (!sculpt_undo_use_multires_mesh(C)) {
    return;
  }

  Object *object = CTX_data_active_object(C);

  SCULPT_undo_push_begin_ex(object, str);

  SculptUndoNode *geometry_unode = SCULPT_undo_push_node(object, nullptr, SCULPT_UNDO_GEOMETRY);
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

  SculptUndoNode *geometry_unode = SCULPT_undo_push_node(object, nullptr, SCULPT_UNDO_GEOMETRY);
  geometry_unode->geometry_clear_pbvh = false;

  SCULPT_undo_push_end(object);
}

/** \} */
extern "C" void SCULPT_substep_undo(bContext * /*C*/, int /*dir*/)
{
  printf("%s: not working!\n", __func__);
#if 0  // XXX
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  if (!scene || !ob || !ob->sculpt) {
    printf("not in sculpt mode\n");
    return;
  }

  SculptSession *ss = ob->sculpt;

  if (!ss->bm) {
    printf("not in dyntopo mode\n");
    return;
  }

  BmeshUndoData data = {ss->pbvh,
                        ss->bm,
                        false,
                        false,
                        ss->cd_face_node_offset,
                        ss->cd_vert_node_offset,
                        ss->attrs.boundary_flags->bmesh_cd_offset,
                        false,
                        false};

  BMLogCallbacks callbacks = {bmesh_undo_on_vert_add,
                              bmesh_undo_on_vert_kill,
                              bmesh_undo_on_vert_change,
                              bmesh_undo_on_edge_add,
                              bmesh_undo_on_edge_kill,
                              bmesh_undo_on_edge_change,
                              bmesh_undo_on_face_add,
                              bmesh_undo_on_face_kill,
                              bmesh_undo_on_face_change,
                              bmesh_undo_full_mesh,
                              nullptr,
                              (void *)&data};

  BM_log_undo_single(ss->bm, ss->bm_log, &callbacks, nullptr);

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false, false);
  DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);
#endif
}

void BM_log_get_changed(BMesh *bm, struct BMIdMap *idmap, BMLogEntry *_entry, SmallHash *sh);

void ED_sculpt_fast_save_bmesh(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  BMesh *bm = ss->bm;

  if (!bm || !ss) {
    return;
  }

#if 1
  struct BMeshToMeshParams params = {};

  void BM_mesh_bm_to_me_threaded(
      Main * bmain, Object * ob, BMesh * bm, Mesh * me, const struct BMeshToMeshParams *params);

  params.update_shapekey_indices = true;

  // BM_mesh_bm_to_me_threaded(nullptr, ob, bm, (Mesh *)ob->data, &params);
  BM_mesh_bm_to_me(nullptr, bm, (Mesh *)ob->data, &params);
#else
  SculptUndoStep *last_step = nullptr;

  UndoStack *ustack = ED_undo_stack_get();
  UndoStep *us = ustack->step_active;

  SmallHash elems;
  BLI_smallhash_init(&elems);

  bool bad = false;

  if (!us) {
    printf("no active undo step!");
    bad = true;
  }
  else {
    while (us) {
      us = us->prev;

      if (us->type == BKE_UNDOSYS_TYPE_SCULPT) {
        SculptUndoStep *usculpt = (SculptUndoStep *)us;

        LISTBASE_FOREACH (SculptUndoNode *, unode, &usculpt->data.nodes) {
          if (unode->bm_entry) {
            BM_log_get_changed(bm, ss->bm_idmap, unode->bm_entry, &elems);
          }
        }

        if (usculpt->auto_saved) {
          last_step = usculpt;
          break;
        }

        if (!last_step) {
          usculpt->auto_saved = true;
        }

        last_step = usculpt;
      }
    }
  }

  if (!last_step) {
    bad = true;
  }
  else {
    last_step->auto_saved = true;
  }

  if (bad) {
    printf("%s: Failed to find sculpt undo stack entries\n", __func__);

    /* Just save everything */
    struct BMeshToMeshParams params = {0};
    BM_mesh_bm_to_me(nullptr, bm, (Mesh *)ob->data, &params);
    return;
  }

  int totv = 0, tote = 0, totl = 0, totf = 0;

  BM_mesh_elem_table_ensure(bm, BM_VERT | BM_EDGE | BM_LOOP | BM_FACE);

  SmallHashIter iter;
  uintptr_t key;
  void *val = BLI_smallhash_iternew(&elems, &iter, &key);
  for (; val; val = BLI_smallhash_iternext(&iter, &key)) {
    BMElem *elem = (BMElem *)key;

    switch (elem->head.htype) {
      case BM_VERT:
        totv++;
        break;
      case BM_EDGE:
        tote++;
        break;
      case BM_LOOP:
        totl++;
        break;
      case BM_FACE:
        totf++;
        break;
    }
  }

  BLI_smallhash_release(&elems);
#endif
}
