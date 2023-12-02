/* SPDX-FileCopyrightText: 2006 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

#include <cstddef>

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.hh"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_attribute.hh"
#include "BKE_ccg.h"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_layer.h"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_multires.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_scene.h"
#include "BKE_subdiv_ccg.hh"
#include "BKE_subsurf.hh"
#include "BKE_undo_system.h"

/* TODO(sergey): Ideally should be no direct call to such low level things. */
#include "BKE_subdiv_eval.hh"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_geometry.hh"
#include "ED_object.hh"
#include "ED_sculpt.hh"
#include "ED_undo.hh"

#include "bmesh.h"
#include "sculpt_intern.hh"

using blender::Span;
using blender::Vector;

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

struct UndoSculpt {
  ListBase nodes;

  size_t undo_size;
};

struct SculptAttrRef {
  eAttrDomain domain;
  eCustomDataType type;
  char name[MAX_CUSTOMDATA_LAYER_NAME];
  bool was_set;
};

struct SculptUndoStep {
  UndoStep step;
  /* NOTE: will split out into list for multi-object-sculpt-mode. */
  UndoSculpt data;

  /* Active color attribute at the start of this undo step. */
  SculptAttrRef active_color_start;

  /* Active color attribute at the end of this undo step. */
  SculptAttrRef active_color_end;

  bContext *C;

#ifdef SCULPT_UNDO_DEBUG
  int id;
#endif
};

static UndoSculpt *sculpt_undo_get_nodes(void);
static bool sculpt_attribute_ref_equals(SculptAttrRef *a, SculptAttrRef *b);
static void sculpt_save_active_attribute(Object *ob, SculptAttrRef *attr);
static UndoSculpt *sculpt_undosys_step_get_nodes(UndoStep *us_p);

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
    BKE_pbvh_node_mark_update_visibility(node);
  }
  BKE_pbvh_node_fully_hidden_set(node, 0);
}

struct PartialUpdateData {
  PBVH *pbvh;
  bool rebuild;
  bool *modified_grids;
  bool *modified_hidden_verts;
  bool *modified_mask_verts;
  bool *modified_color_verts;
  bool *modified_face_set_faces;
};

/**
 * A version of #update_cb that tests for the update tag in #PBVH.vert_bitmap.
 */
static void update_cb_partial(PBVHNode *node, void *userdata)
{
  PartialUpdateData *data = static_cast<PartialUpdateData *>(userdata);
  if (BKE_pbvh_type(data->pbvh) == PBVH_GRIDS) {
    const Span<int> grid_indices = BKE_pbvh_node_get_grid_indices(*node);
    if (std::any_of(grid_indices.begin(), grid_indices.end(), [&](const int grid) {
          return data->modified_grids[grid];
        }))
    {
      update_cb(node, &data->rebuild);
    }
  }
  else {
    if (BKE_pbvh_node_has_vert_with_normal_update_tag(data->pbvh, node)) {
      BKE_pbvh_node_mark_update(node);
    }
    const blender::Span<int> verts = BKE_pbvh_node_get_vert_indices(node);
    if (data->modified_mask_verts != nullptr) {
      for (const int vert : verts) {
        if (data->modified_mask_verts[vert]) {
          BKE_pbvh_node_mark_update_mask(node);
          break;
        }
      }
    }
    if (data->modified_color_verts != nullptr) {
      for (const int vert : verts) {
        if (data->modified_color_verts[vert]) {
          BKE_pbvh_node_mark_update_color(node);
          break;
        }
      }
    }
    if (data->modified_hidden_verts != nullptr) {
      for (const int vert : verts) {
        if (data->modified_hidden_verts[vert]) {
          if (data->rebuild) {
            BKE_pbvh_node_mark_update_visibility(node);
          }
          BKE_pbvh_node_fully_hidden_set(node, 0);
          break;
        }
      }
    }
  }
  if (data->modified_face_set_faces) {
    for (const int face : BKE_pbvh_node_calc_face_indices(*data->pbvh, *node)) {
      if (data->modified_face_set_faces[face]) {
        BKE_pbvh_node_mark_update_face_sets(node);
        break;
      }
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
  using namespace blender;
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  SculptSession *ss = ob->sculpt;
  SubdivCCG *subdiv_ccg = ss->subdiv_ccg;

  if (unode->maxvert) {
    /* Regular mesh restore. */

    if (ss->shapekey_active && !STREQ(ss->shapekey_active->name, unode->shapeName)) {
      /* Shape key has been changed before calling undo operator. */

      Key *key = BKE_key_from_object(ob);
      KeyBlock *kb = key ? BKE_keyblock_find_name(key, unode->shapeName) : nullptr;

      if (kb) {
        ob->shapenr = BLI_findindex(&key->block, kb) + 1;

        BKE_sculpt_update_object_for_edit(depsgraph, ob, false);
        WM_event_add_notifier(C, NC_OBJECT | ND_DATA, ob);
      }
      else {
        /* Key has been removed -- skip this undo node. */
        return false;
      }
    }

    /* No need for float comparison here (memory is exactly equal or not). */
    const Span<int> index = unode->index;
    blender::MutableSpan<blender::float3> positions = ss->vert_positions;

    if (ss->shapekey_active) {
      blender::MutableSpan<blender::float3> vertCos(
          static_cast<blender::float3 *>(ss->shapekey_active->data), ss->shapekey_active->totelem);

      if (!unode->orig_co.is_empty()) {
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
      BKE_pbvh_vert_coords_apply(ss->pbvh, vertCos);
    }
    else {
      if (!unode->orig_co.is_empty()) {
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
  else if (!unode->grids.is_empty() && subdiv_ccg != nullptr) {
    const int gridsize = subdiv_ccg->grid_size;
    CCGKey key;
    BKE_subdiv_ccg_key_top_level(key, *subdiv_ccg);
    const Span<int> grid_indices = unode->grids;

    blender::MutableSpan<blender::float3> co = unode->co;
    MutableSpan<CCGElem *> grids = subdiv_ccg->grids;

    int index = 0;
    for (const int i : grid_indices.index_range()) {
      CCGElem *grid = grids[grid_indices[i]];
      for (const int j : IndexRange(gridsize * gridsize)) {
        swap_v3_v3(CCG_elem_offset_co(&key, grid, j), co[index]);
        index++;
      }
    }
  }

  return true;
}

static bool sculpt_undo_restore_hidden(bContext *C, SculptUndoNode *unode, bool *modified_vertices)
{
  using namespace blender;
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  SculptSession *ss = ob->sculpt;
  SubdivCCG *subdiv_ccg = ss->subdiv_ccg;

  if (unode->maxvert) {
    Mesh &mesh = *static_cast<Mesh *>(ob->data);
    bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
    bke::SpanAttributeWriter<bool> hide_vert = attributes.lookup_or_add_for_write_span<bool>(
        ".hide_vert", ATTR_DOMAIN_POINT);
    for (const int i : unode->index.index_range()) {
      const int vert = unode->index[i];
      if (unode->vert_hidden[i].test() != hide_vert.span[vert]) {
        unode->vert_hidden[i].set(!unode->vert_hidden[i].test());
        hide_vert.span[vert] = !hide_vert.span[vert];
        modified_vertices[vert] = true;
      }
    }
    hide_vert.finish();
  }
  else if (!unode->grids.is_empty() && subdiv_ccg != nullptr) {
    if (unode->grid_hidden.is_empty()) {
      BKE_subdiv_ccg_grid_hidden_free(*subdiv_ccg);
      return true;
    }

    blender::BitGroupVector<> &grid_hidden = BKE_subdiv_ccg_grid_hidden_ensure(*subdiv_ccg);
    const Span<int> grids = unode->grids;
    for (const int i : grids.index_range()) {
      const int grid_index = grids[i];
      /* Swap the two bit spans. */
      blender::BitVector<512> tmp(grid_hidden[grid_index]);
      grid_hidden[grid_index].copy_from(unode->grid_hidden[i].as_span());
      unode->grid_hidden[i].copy_from(tmp);
    }
  }

  return true;
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
  if (!unode->col.is_empty() && unode->loop_col.is_empty()) {
    BKE_pbvh_swap_colors(ss->pbvh, unode->index, unode->col);
    modified = true;
  }

  Mesh *me = BKE_object_get_original_mesh(ob);

  if (!unode->loop_col.is_empty() && unode->maxloop == me->totloop) {
    BKE_pbvh_swap_colors(ss->pbvh, unode->loop_index, unode->loop_col);

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
  using namespace blender;
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  Mesh *mesh = BKE_object_get_original_mesh(ob);
  SculptSession *ss = ob->sculpt;
  SubdivCCG *subdiv_ccg = ss->subdiv_ccg;

  if (unode->maxvert) {
    bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
    bke::SpanAttributeWriter<float> mask = attributes.lookup_or_add_for_write_span<float>(
        ".sculpt_mask", ATTR_DOMAIN_POINT);

    const Span<int> index = unode->index;

    for (int i = 0; i < unode->totvert; i++) {
      if (mask.span[index[i]] != unode->mask[i]) {
        std::swap(mask.span[index[i]], unode->mask[i]);
        modified_vertices[index[i]] = true;
      }
    }

    mask.finish();
  }
  else if (!unode->grids.is_empty() && subdiv_ccg != nullptr) {
    const int gridsize = subdiv_ccg->grid_size;
    CCGKey key;
    BKE_subdiv_ccg_key_top_level(key, *subdiv_ccg);
    const Span<int> grid_indices = unode->grids;

    BKE_subdiv_ccg_key_top_level(key, *subdiv_ccg);

    blender::MutableSpan<float> mask = unode->mask;
    MutableSpan<CCGElem *> grids = subdiv_ccg->grids;

    int index = 0;
    for (const int i : grid_indices.index_range()) {
      CCGElem *grid = grids[grid_indices[i]];
      for (const int j : IndexRange(gridsize * gridsize)) {
        std::swap(*CCG_elem_offset_mask(&key, grid, j), mask[index]);
        index++;
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

  bool modified = false;
  const Span<int> face_indices = unode->face_indices;

  for (const int i : face_indices.index_range()) {
    int face_index = face_indices[i];
    if (unode->face_sets[i] != ss->face_sets[face_index]) {
      modified_face_set_faces[face_index] = true;
      modified = true;
    }

    SWAP(int, unode->face_sets[i], ss->face_sets[face_index]);
  }
  return modified;
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
    Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, {});
    for (PBVHNode *node : nodes) {
      BKE_pbvh_node_mark_redraw(node);
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
  Mesh *me = static_cast<Mesh *>(ob->data);

  SCULPT_pbvh_clear(ob);

  /* Create empty BMesh and enable logging. */
  BMeshCreateParams bmesh_create_params{};
  bmesh_create_params.use_toolflags = false;

  ss->bm = BM_mesh_create(&bm_mesh_allocsize_default, &bmesh_create_params);
  BM_data_layer_add_named(ss->bm, &ss->bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");

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
    SCULPT_dynamic_topology_disable(C, nullptr);
    unode->applied = true;
  }
}

static void sculpt_undo_geometry_store_data(SculptUndoNodeGeometry *geometry, Object *object)
{
  Mesh *mesh = static_cast<Mesh *>(object->data);

  BLI_assert(!geometry->is_initialized);
  geometry->is_initialized = true;

  CustomData_copy(&mesh->vert_data, &geometry->vert_data, CD_MASK_MESH.vmask, mesh->totvert);
  CustomData_copy(&mesh->edge_data, &geometry->edge_data, CD_MASK_MESH.emask, mesh->totedge);
  CustomData_copy(&mesh->loop_data, &geometry->loop_data, CD_MASK_MESH.lmask, mesh->totloop);
  CustomData_copy(&mesh->face_data, &geometry->face_data, CD_MASK_MESH.pmask, mesh->faces_num);
  blender::implicit_sharing::copy_shared_pointer(mesh->face_offset_indices,
                                                 mesh->runtime->face_offsets_sharing_info,
                                                 &geometry->face_offset_indices,
                                                 &geometry->face_offsets_sharing_info);

  geometry->totvert = mesh->totvert;
  geometry->totedge = mesh->totedge;
  geometry->totloop = mesh->totloop;
  geometry->faces_num = mesh->faces_num;
}

static void sculpt_undo_geometry_restore_data(SculptUndoNodeGeometry *geometry, Object *object)
{
  Mesh *mesh = static_cast<Mesh *>(object->data);

  BLI_assert(geometry->is_initialized);

  BKE_mesh_clear_geometry(mesh);

  mesh->totvert = geometry->totvert;
  mesh->totedge = geometry->totedge;
  mesh->totloop = geometry->totloop;
  mesh->faces_num = geometry->faces_num;
  mesh->totface_legacy = 0;

  CustomData_copy(&geometry->vert_data, &mesh->vert_data, CD_MASK_MESH.vmask, geometry->totvert);
  CustomData_copy(&geometry->edge_data, &mesh->edge_data, CD_MASK_MESH.emask, geometry->totedge);
  CustomData_copy(&geometry->loop_data, &mesh->loop_data, CD_MASK_MESH.lmask, geometry->totloop);
  CustomData_copy(&geometry->face_data, &mesh->face_data, CD_MASK_MESH.pmask, geometry->faces_num);
  blender::implicit_sharing::copy_shared_pointer(geometry->face_offset_indices,
                                                 geometry->face_offsets_sharing_info,
                                                 &mesh->face_offset_indices,
                                                 &mesh->runtime->face_offsets_sharing_info);
}

static void sculpt_undo_geometry_free_data(SculptUndoNodeGeometry *geometry)
{
  if (geometry->totvert) {
    CustomData_free(&geometry->vert_data, geometry->totvert);
  }
  if (geometry->totedge) {
    CustomData_free(&geometry->edge_data, geometry->totedge);
  }
  if (geometry->totloop) {
    CustomData_free(&geometry->loop_data, geometry->totloop);
  }
  if (geometry->faces_num) {
    CustomData_free(&geometry->face_data, geometry->faces_num);
  }
  blender::implicit_sharing::free_shared_data(&geometry->face_offset_indices,
                                              &geometry->face_offsets_sharing_info);
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
                                      Subdiv *subdiv)
{
  blender::Array<blender::float3> deformed_verts =
      BKE_multires_create_deformed_base_mesh_vert_coords(depsgraph, object, ss->multires.modifier);

  BKE_subdiv_eval_refine_from_mesh(subdiv,
                                   static_cast<const Mesh *>(object->data),
                                   reinterpret_cast<float(*)[3]>(deformed_verts.data()));
}

static void sculpt_undo_restore_list(bContext *C, Depsgraph *depsgraph, ListBase *lb)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  SculptSession *ss = ob->sculpt;
  SubdivCCG *subdiv_ccg = ss->subdiv_ccg;
  bool update = false, rebuild = false, update_mask = false, update_visibility = false;
  bool update_face_sets = false;
  bool need_refine_subdiv = false;
  bool clear_automask_cache = false;

  LISTBASE_FOREACH (SculptUndoNode *, unode, lb) {
    if (!ELEM(unode->type, SCULPT_UNDO_COLOR, SCULPT_UNDO_MASK)) {
      clear_automask_cache = true;
    }

    /* Restore pivot. */
    copy_v3_v3(ss->pivot_pos, unode->pivot_pos);
    copy_v3_v3(ss->pivot_rot, unode->pivot_rot);
  }

  if (clear_automask_cache) {
    ss->last_automasking_settings_hash = 0;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);

  if (lb->first != nullptr) {
    /* Only do early object update for edits if first node needs this.
     * Undo steps like geometry does not need object to be updated before they run and will
     * ensure object is updated after the node is handled. */
    const SculptUndoNode *first_unode = (const SculptUndoNode *)lb->first;
    if (first_unode->type != SCULPT_UNDO_GEOMETRY) {
      BKE_sculpt_update_object_for_edit(depsgraph, ob, false);
    }

    if (sculpt_undo_bmesh_restore(C, static_cast<SculptUndoNode *>(lb->first), ob, ss)) {
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
  bool *undo_modified_grids = nullptr;
  bool use_multires_undo = false;

  LISTBASE_FOREACH (SculptUndoNode *, unode, lb) {

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
    else if (unode->maxgrid && subdiv_ccg != nullptr) {
      if ((subdiv_ccg->grids.size() != unode->maxgrid) ||
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
        BKE_sculpt_update_object_for_edit(depsgraph, ob, false);
        break;

      case SCULPT_UNDO_DYNTOPO_BEGIN:
      case SCULPT_UNDO_DYNTOPO_END:
      case SCULPT_UNDO_DYNTOPO_SYMMETRIZE:
        BLI_assert_msg(0, "Dynamic topology should've already been handled");
        break;
    }
  }

  if (use_multires_undo) {
    LISTBASE_FOREACH (SculptUndoNode *, unode, lb) {
      if (!STREQ(unode->idname, ob->id.name)) {
        continue;
      }
      if (unode->maxgrid == 0) {
        continue;
      }

      if (undo_modified_grids == nullptr) {
        undo_modified_grids = static_cast<bool *>(
            MEM_callocN(sizeof(bool) * unode->maxgrid, "undo_grids"));
      }

      for (const int grid : unode->grids) {
        undo_modified_grids[grid] = true;
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
    data.pbvh = ss->pbvh;
    data.modified_grids = undo_modified_grids;
    data.modified_hidden_verts = modified_hidden_verts;
    data.modified_mask_verts = modified_mask_verts;
    data.modified_color_verts = modified_color_verts;
    data.modified_face_set_faces = modified_face_set_faces;
    BKE_pbvh_search_callback(ss->pbvh, {}, update_cb_partial, &data);
    BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateBB | PBVH_UpdateOriginalBB | PBVH_UpdateRedraw);

    if (update_mask) {
      BKE_pbvh_update_mask(ss->pbvh);
    }
    if (update_face_sets) {
      DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);
      BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_RebuildDrawBuffers);
    }

    if (update_visibility) {
      if (ELEM(BKE_pbvh_type(ss->pbvh), PBVH_FACES, PBVH_GRIDS)) {
        Mesh *me = (Mesh *)ob->data;
        BKE_pbvh_sync_visibility_from_verts(ss->pbvh, me);
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
  }

  MEM_SAFE_FREE(modified_hidden_verts);
  MEM_SAFE_FREE(modified_mask_verts);
  MEM_SAFE_FREE(modified_color_verts);
  MEM_SAFE_FREE(modified_face_set_faces);
  MEM_SAFE_FREE(undo_modified_grids);
}

static void sculpt_undo_free_list(ListBase *lb)
{
  SculptUndoNode *unode = static_cast<SculptUndoNode *>(lb->first);
  while (unode != nullptr) {
    SculptUndoNode *unode_next = unode->next;

    if (unode->bm_entry) {
      BM_log_entry_drop(unode->bm_entry);
    }

    sculpt_undo_geometry_free_data(&unode->geometry_original);
    sculpt_undo_geometry_free_data(&unode->geometry_modified);
    sculpt_undo_geometry_free_data(&unode->geometry_bmesh_enter);

    MEM_delete(unode);

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

static size_t sculpt_undo_alloc_and_store_hidden(SculptSession *ss, SculptUndoNode *unode)
{
  PBVHNode *node = static_cast<PBVHNode *>(unode->node);
  if (!ss->subdiv_ccg) {
    return 0;
  }
  const blender::BitGroupVector<> grid_hidden = ss->subdiv_ccg->grid_hidden;
  if (grid_hidden.is_empty()) {
    return 0;
  }

  const Span<int> grid_indices = BKE_pbvh_node_get_grid_indices(*node);
  for (const int i : grid_indices.index_range()) {
    unode->grid_hidden[i].copy_from(grid_hidden[grid_indices[i]]);
  }

  return unode->grid_hidden.all_bits().full_ints_num() / blender::bits::BitsPerInt;
}

/* Allocate node and initialize its default fields specific for the given undo type.
 * Will also add the node to the list in the undo step. */
static SculptUndoNode *sculpt_undo_alloc_node_type(Object *object, SculptUndoType type)
{
  const size_t alloc_size = sizeof(SculptUndoNode);
  SculptUndoNode *unode = MEM_new<SculptUndoNode>(__func__);
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

static SculptUndoNode *sculpt_undo_alloc_node(Object *ob, PBVHNode *node, SculptUndoType type)
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();
  SculptSession *ss = ob->sculpt;

  SculptUndoNode *unode = sculpt_undo_alloc_node_type(ob, type);
  unode->node = node;

  int totvert = 0;
  int allvert = 0;
  BKE_pbvh_node_num_verts(ss->pbvh, node, &totvert, &allvert);

  Span<int> grids;
  if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
    grids = BKE_pbvh_node_get_grid_indices(*node);
  }

  unode->totvert = totvert;

  bool need_loops = type == SCULPT_UNDO_COLOR;
  const bool need_faces = type == SCULPT_UNDO_FACE_SETS;

  if (need_loops) {
    int totloop;

    BKE_pbvh_node_num_loops(ss->pbvh, node, &totloop);

    unode->loop_index.reinitialize(totloop);
    unode->maxloop = 0;
    unode->totloop = totloop;

    usculpt->undo_size += unode->loop_index.as_span().size_in_bytes();
  }

  if (need_faces) {
    unode->face_indices = BKE_pbvh_node_calc_face_indices(*ss->pbvh, *node);
    usculpt->undo_size += unode->face_indices.as_span().size_in_bytes();
  }

  switch (type) {
    case SCULPT_UNDO_COORDS: {
      unode->co.reinitialize(allvert);
      usculpt->undo_size += unode->co.as_span().size_in_bytes();

      /* Needed for original data lookup. */
      unode->no.reinitialize(allvert);
      usculpt->undo_size += unode->no.as_span().size_in_bytes();
      break;
    }
    case SCULPT_UNDO_HIDDEN: {
      if (grids.is_empty()) {
        unode->vert_hidden.resize(allvert);
        usculpt->undo_size += BLI_BITMAP_SIZE(allvert);
      }
      else {
        usculpt->undo_size += sculpt_undo_alloc_and_store_hidden(ss, unode);
      }

      break;
    }
    case SCULPT_UNDO_MASK: {
      unode->mask.reinitialize(allvert);
      usculpt->undo_size += unode->mask.as_span().size_in_bytes();
      break;
    }
    case SCULPT_UNDO_COLOR: {
      /* Allocate vertex colors, even for loop colors we still
       * need this for original data lookup. */
      unode->col.reinitialize(allvert);
      usculpt->undo_size += unode->col.as_span().size_in_bytes();

      /* Allocate loop colors separately too. */
      if (ss->vcol_domain == ATTR_DOMAIN_CORNER) {
        unode->loop_col.reinitialize(unode->totloop);
        unode->undo_size += unode->loop_col.as_span().size_in_bytes();
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
      unode->face_sets.reinitialize(unode->face_indices.size());
      usculpt->undo_size += unode->face_sets.as_span().size_in_bytes();
      break;
    }
  }

  if (!grids.is_empty()) {
    /* Multires. */
    unode->maxgrid = ss->subdiv_ccg->grids.size();
    unode->gridsize = ss->subdiv_ccg->grid_size;

    unode->grids.reinitialize(grids.size());
    usculpt->undo_size += unode->grids.as_span().size_in_bytes();
  }
  else {
    /* Regular mesh. */
    unode->maxvert = ss->totvert;
    unode->index.reinitialize(allvert);
    usculpt->undo_size += unode->index.as_span().size_in_bytes();
  }

  if (ss->deform_modifiers_active) {
    unode->orig_co.reinitialize(allvert);
    usculpt->undo_size += unode->orig_co.as_span().size_in_bytes();
  }

  return unode;
}

static void sculpt_undo_store_coords(Object *ob, SculptUndoNode *unode)
{
  SculptSession *ss = ob->sculpt;
  PBVHVertexIter vd;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, static_cast<PBVHNode *>(unode->node), vd, PBVH_ITER_ALL) {
    copy_v3_v3(unode->co[vd.i], vd.co);
    if (vd.no) {
      copy_v3_v3(unode->no[vd.i], vd.no);
    }
    else {
      copy_v3_v3(unode->no[vd.i], vd.fno);
    }

    if (ss->deform_modifiers_active) {
      copy_v3_v3(unode->orig_co[vd.i], ss->orig_cos[unode->index[vd.i]]);
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void sculpt_undo_store_hidden(Object *ob, SculptUndoNode *unode)
{
  using namespace blender;
  using namespace blender::bke;
  if (!unode->grids.is_empty()) {
    /* Already stored during allocation. */
  }

  const Mesh &mesh = *static_cast<const Mesh *>(ob->data);
  const AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<bool> hide_vert = *attributes.lookup<bool>(".hide_vert", ATTR_DOMAIN_POINT);
  if (hide_vert.is_empty()) {
    return;
  }

  PBVHNode *node = static_cast<PBVHNode *>(unode->node);
  const blender::Span<int> verts = BKE_pbvh_node_get_vert_indices(node);
  for (const int i : verts.index_range())
    unode->vert_hidden[i].set(hide_vert[verts[i]]);
}

static void sculpt_undo_store_mask(Object *ob, SculptUndoNode *unode)
{
  SculptSession *ss = ob->sculpt;
  PBVHVertexIter vd;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, static_cast<PBVHNode *>(unode->node), vd, PBVH_ITER_ALL) {
    unode->mask[vd.i] = vd.mask;
  }
  BKE_pbvh_vertex_iter_end;
}

static void sculpt_undo_store_color(Object *ob, SculptUndoNode *unode)
{
  SculptSession *ss = ob->sculpt;

  BLI_assert(BKE_pbvh_type(ss->pbvh) == PBVH_FACES);

  /* NOTE: even with loop colors we still store (derived)
   * vertex colors for original data lookup. */
  BKE_pbvh_store_colors_vertex(ss->pbvh, unode->index, unode->col);

  if (!unode->loop_col.is_empty() && unode->totloop) {
    BKE_pbvh_store_colors(ss->pbvh, unode->loop_index, unode->loop_col);
  }
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

static void sculpt_undo_store_face_sets(const Mesh &mesh, SculptUndoNode &unode)
{
  blender::array_utils::gather(
      *mesh.attributes().lookup_or_default<int>(".sculpt_face_set", ATTR_DOMAIN_FACE, 0),
      unode.face_indices.as_span(),
      unode.face_sets.as_mutable_span());
}

static SculptUndoNode *sculpt_undo_bmesh_push(Object *ob, PBVHNode *node, SculptUndoType type)
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();
  SculptSession *ss = ob->sculpt;
  PBVHVertexIter vd;

  SculptUndoNode *unode = static_cast<SculptUndoNode *>(usculpt->nodes.first);

  if (unode == nullptr) {
    unode = MEM_new<SculptUndoNode>(__func__);

    STRNCPY(unode->idname, ob->id.name);
    unode->type = type;
    unode->applied = true;

    if (type == SCULPT_UNDO_DYNTOPO_END) {
      unode->bm_entry = BM_log_entry_add(ss->bm_log);
      BM_log_before_all_removed(ss->bm, ss->bm_log);
    }
    else if (type == SCULPT_UNDO_DYNTOPO_BEGIN) {
      /* Store a copy of the mesh's current vertices, loops, and
       * faces. A full copy like this is needed because entering
       * dynamic-topology immediately does topological edits
       * (converting faces to triangles) that the BMLog can't
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
        BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_ALL) {
          BM_log_vert_before_modified(ss->bm_log, vd.bm_vert, vd.cd_vert_mask_offset);
        }
        BKE_pbvh_vertex_iter_end;
        break;

      case SCULPT_UNDO_HIDDEN: {
        BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_ALL) {
          BM_log_vert_before_modified(ss->bm_log, vd.bm_vert, vd.cd_vert_mask_offset);
        }
        BKE_pbvh_vertex_iter_end;

        for (BMFace *f : BKE_pbvh_bmesh_node_faces(node)) {
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
  if ((unode = SCULPT_undo_get_node(node, type))) {
    BLI_thread_unlock(LOCK_CUSTOM1);
    return unode;
  }

  unode = sculpt_undo_alloc_node(ob, node, type);

  /* NOTE: If this ever becomes a bottleneck, make a lock inside of the node.
   * so we release global lock sooner, but keep data locked for until it is
   * fully initialized.
   */

  if (!unode->grids.is_empty()) {
    unode->grids.as_mutable_span().copy_from(BKE_pbvh_node_get_grid_indices(*node));
  }
  else {
    unode->index.as_mutable_span().copy_from(BKE_pbvh_node_get_vert_indices(node));

    if (!unode->loop_index.is_empty()) {
      const int *loop_indices;
      int allloop;
      BKE_pbvh_node_num_loops(ss->pbvh, static_cast<PBVHNode *>(unode->node), &allloop);
      BKE_pbvh_node_get_loops(static_cast<PBVHNode *>(unode->node), &loop_indices);

      if (allloop) {
        unode->loop_index.as_mutable_span().copy_from({loop_indices, allloop});

        unode->maxloop = BKE_object_get_original_mesh(ob)->totloop;
      }
    }
  }

  switch (type) {
    case SCULPT_UNDO_COORDS:
      sculpt_undo_store_coords(ob, unode);
      break;
    case SCULPT_UNDO_HIDDEN:
      sculpt_undo_store_hidden(ob, unode);
      break;
    case SCULPT_UNDO_MASK:
      if (pbvh_has_mask(ss->pbvh)) {
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
      sculpt_undo_store_face_sets(*static_cast<const Mesh *>(ob->data), *unode);
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

  BLI_thread_unlock(LOCK_CUSTOM1);

  return unode;
}

static bool sculpt_attribute_ref_equals(SculptAttrRef *a, SculptAttrRef *b)
{
  return a->domain == b->domain && a->type == b->type && STREQ(a->name, b->name);
}

static void sculpt_save_active_attribute(Object *ob, SculptAttrRef *attr)
{
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

void SCULPT_undo_push_begin(Object *ob, const wmOperator *op)
{
  SCULPT_undo_push_begin_ex(ob, op->type->name);
}

void SCULPT_undo_push_begin_ex(Object *ob, const char *name)
{
  UndoStack *ustack = ED_undo_stack_get();

  if (ob != nullptr) {
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
    sculpt_save_active_attribute(ob, &us->active_color_start);
  }

  /* Set end attribute in case SCULPT_undo_push_end is not called,
   * so we don't end up with corrupted state.
   */
  if (!us->active_color_end.was_set) {
    sculpt_save_active_attribute(ob, &us->active_color_end);
    us->active_color_end.was_set = false;
  }
}

void SCULPT_undo_push_end(Object *ob)
{
  SCULPT_undo_push_end_ex(ob, false);
}

void SCULPT_undo_push_end_ex(Object *ob, const bool use_nested_undo)
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();

  /* We don't need normals in the undo stack. */
  LISTBASE_FOREACH (SculptUndoNode *, unode, &usculpt->nodes) {
    usculpt->undo_size -= unode->no.as_span().size_in_bytes();
    unode->no = {};
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

  sculpt_save_active_attribute(ob, &us->active_color_end);
  sculpt_undo_print_nodes(ob, nullptr);
}

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

static void sculpt_undo_set_active_layer(bContext *C, SculptAttrRef *attr)
{
  if (attr->domain == ATTR_DOMAIN_AUTO) {
    return;
  }

  Object *ob = CTX_data_active_object(C);
  Mesh *me = BKE_object_get_original_mesh(ob);

  SculptAttrRef existing;
  sculpt_save_active_attribute(ob, &existing);

  CustomDataLayer *layer = BKE_id_attribute_find(&me->id, attr->name, attr->type, attr->domain);

  /* Temporary fix for #97408. This is a fundamental
   * bug in the undo stack; the operator code needs to push
   * an extra undo step before running an operator if a
   * non-memfile undo system is active.
   *
   * For now, detect if the layer does exist but with a different
   * domain and just unconvert it.
   */
  if (!layer) {
    layer = BKE_id_attribute_search_for_write(
        &me->id, attr->name, CD_MASK_PROP_ALL, ATTR_DOMAIN_MASK_ALL);
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
    me->attributes_for_write().add(
        attr->name, attr->domain, attr->type, blender::bke::AttributeInitDefaultValue());
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
}

static void sculpt_undosys_step_encode_init(bContext * /*C*/, UndoStep *us_p)
{
  SculptUndoStep *us = (SculptUndoStep *)us_p;
  /* Dummy, memory is cleared anyway. */
  BLI_listbase_clear(&us->data.nodes);
}

static bool sculpt_undosys_step_encode(bContext * /*C*/, Main *bmain, UndoStep *us_p)
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

static void sculpt_undosys_step_decode_undo_impl(bContext *C,
                                                 Depsgraph *depsgraph,
                                                 SculptUndoStep *us)
{
  BLI_assert(us->step.is_applied == true);

  sculpt_undo_restore_list(C, depsgraph, &us->data.nodes);
  us->step.is_applied = false;

  sculpt_undo_print_nodes(CTX_data_active_object(C), nullptr);
}

static void sculpt_undosys_step_decode_redo_impl(bContext *C,
                                                 Depsgraph *depsgraph,
                                                 SculptUndoStep *us)
{
  BLI_assert(us->step.is_applied == false);

  sculpt_undo_restore_list(C, depsgraph, &us->data.nodes);
  us->step.is_applied = true;

  sculpt_undo_print_nodes(CTX_data_active_object(C), nullptr);
}

static void sculpt_undosys_step_decode_undo(bContext *C,
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

    sculpt_undo_set_active_layer(C, &((SculptUndoStep *)us_iter)->active_color_start);
    sculpt_undosys_step_decode_undo_impl(C, depsgraph, us_iter);

    if (us_iter == us) {
      if (us_iter->step.prev && us_iter->step.prev->type == BKE_UNDOSYS_TYPE_SCULPT) {
        sculpt_undo_set_active_layer(C, &((SculptUndoStep *)us_iter->step.prev)->active_color_end);
      }
      break;
    }

    us_iter = (SculptUndoStep *)us_iter->step.prev;
  }
}

static void sculpt_undosys_step_decode_redo(bContext *C, Depsgraph *depsgraph, SculptUndoStep *us)
{
  SculptUndoStep *us_iter = us;
  while (us_iter->step.prev && (us_iter->step.prev->type == us_iter->step.type)) {
    if (us_iter->step.prev->is_applied == true) {
      break;
    }
    us_iter = (SculptUndoStep *)us_iter->step.prev;
  }
  while (us_iter && (us_iter->step.is_applied == false)) {
    sculpt_undo_set_active_layer(C, &((SculptUndoStep *)us_iter)->active_color_start);
    sculpt_undosys_step_decode_redo_impl(C, depsgraph, us_iter);

    if (us_iter == us) {
      sculpt_undo_set_active_layer(C, &((SculptUndoStep *)us_iter)->active_color_end);
      break;
    }
    us_iter = (SculptUndoStep *)us_iter->step.next;
  }
}

static void sculpt_undosys_step_decode(
    bContext *C, Main *bmain, UndoStep *us_p, const eUndoStepDir dir, bool is_final)
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

        Mesh *me = static_cast<Mesh *>(ob->data);
        /* Don't add sculpt topology undo steps when reading back undo state.
         * The undo steps must enter/exit for us. */
        me->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;
        ED_object_sculptmode_enter_ex(bmain, depsgraph, scene, ob, true, nullptr);
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

void ED_sculpt_undo_geometry_begin(Object *ob, const wmOperator *op)
{
  SCULPT_undo_push_begin(ob, op);
  SCULPT_undo_push_node(ob, nullptr, SCULPT_UNDO_GEOMETRY);
}

void ED_sculpt_undo_geometry_begin_ex(Object *ob, const char *name)
{
  SCULPT_undo_push_begin_ex(ob, name);
  SCULPT_undo_push_node(ob, nullptr, SCULPT_UNDO_GEOMETRY);
}

void ED_sculpt_undo_geometry_end(Object *ob)
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

static UndoSculpt *sculpt_undo_get_nodes()
{
  UndoStack *ustack = ED_undo_stack_get();
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

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, {});
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
