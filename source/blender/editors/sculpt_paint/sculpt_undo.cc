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
 * To use the sculpt undo system, you must call push_begin
 * inside an operator exec or invoke callback (geometry_begin
 * may be called if you wish to save a non-delta copy of the entire mesh).
 * This will initialize the sculpt undo stack and set up an undo step.
 *
 * At the end of the operator you should call push_end.
 *
 * push_end and geometry_begin both take a
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

#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_attribute.hh"
#include "BKE_ccg.h"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_global.h"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_multires.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_scene.h"
#include "BKE_subdiv_ccg.hh"
#include "BKE_subsurf.hh"
#include "BKE_undo_system.hh"

/* TODO(sergey): Ideally should be no direct call to such low level things. */
#include "BKE_subdiv_eval.hh"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_geometry.hh"
#include "ED_object.hh"
#include "ED_sculpt.hh"
#include "ED_undo.hh"

#include "bmesh.hh"
#include "paint_intern.hh"
#include "sculpt_intern.hh"

namespace blender::ed::sculpt_paint::undo {

/* Uncomment to print the undo stack in the console on push/undo/redo. */
// #define SCULPT_UNDO_DEBUG

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

#define NO_ACTIVE_LAYER bke::AttrDomain::Auto

struct UndoSculpt {
  Vector<std::unique_ptr<Node>> nodes;

  size_t undo_size;
};

struct SculptAttrRef {
  bke::AttrDomain domain;
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

static UndoSculpt *get_nodes();
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
    _(Type::DyntopoBegin)
    _(Type::DyntopoEnd)
    _(Type::Position)
    _(Type::Geometry)
    _(Type::DyntopoSymmetrize)
    _(Type::FaceSet)
    _(Type::HideVert)
    _(Type::HideFace)
    _(Type::Mask)
    _(Type::Color)
    default:
      return "unknown node type";
  }
}
#  undef _

static int nodeidgen = 1;

static void print_sculpt_node(Object *ob, Node *node)
{
  printf("    %s:%s {applied=%d}\n", undo_type_to_str(node->type), node->idname, node->applied);

  if (node->bm_entry) {
    BM_log_print_entry(ob->sculpt ? ob->sculpt->bm : nullptr, node->bm_entry);
  }
}

static void print_step(Object *ob, UndoStep *us, UndoStep *active, int i)
{
  Node *node;

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
static void print_nodes(Object *ob, void *active)
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
    print_step(ob, ustack->step_init, active, -1);
    printf("===============\n");
  }

  int i = 0, act_i = -1;
  for (; us; us = us->next, i++) {
    if (active == us) {
      act_i = i;
    }

    print_step(ob, us, active, i);
  }

  if (ustack->step_active) {
    printf("\n\n==Active step:==\n");
    print_step(ob, ustack->step_active, active, act_i);
  }
}
#else
static void print_nodes(Object * /*ob*/, void * /*active*/) {}
#endif

struct PartialUpdateData {
  PBVH *pbvh;
  bool changed_position;
  bool changed_hide_vert;
  bool changed_mask;
  Span<bool> modified_grids;
  Span<bool> modified_position_verts;
  Span<bool> modified_hidden_verts;
  Span<bool> modified_hidden_faces;
  Span<bool> modified_mask_verts;
  Span<bool> modified_color_verts;
  Span<bool> modified_face_set_faces;
};

static void update_modified_node_mesh(PBVHNode &node, PartialUpdateData &data)
{
  const Span<int> verts = BKE_pbvh_node_get_vert_indices(&node);
  if (!data.modified_position_verts.is_empty()) {
    for (const int vert : verts) {
      if (data.modified_position_verts[vert]) {
        BKE_pbvh_node_mark_positions_update(&node);
        break;
      }
    }
  }
  if (!data.modified_mask_verts.is_empty()) {
    for (const int vert : verts) {
      if (data.modified_mask_verts[vert]) {
        BKE_pbvh_node_mark_update_mask(&node);
        break;
      }
    }
  }
  if (!data.modified_color_verts.is_empty()) {
    for (const int vert : verts) {
      if (data.modified_color_verts[vert]) {
        BKE_pbvh_node_mark_update_color(&node);
        break;
      }
    }
  }
  if (!data.modified_hidden_verts.is_empty()) {
    for (const int vert : verts) {
      if (data.modified_hidden_verts[vert]) {
        BKE_pbvh_node_mark_update_visibility(&node);
        break;
      }
    }
  }

  Vector<int> faces;
  if (!data.modified_face_set_faces.is_empty()) {
    if (faces.is_empty()) {
      bke::pbvh::node_face_indices_calc_mesh(*data.pbvh, node, faces);
    }
    for (const int face : faces) {
      if (data.modified_face_set_faces[face]) {
        BKE_pbvh_node_mark_update_face_sets(&node);
        break;
      }
    }
  }
  if (!data.modified_hidden_faces.is_empty()) {
    if (faces.is_empty()) {
      bke::pbvh::node_face_indices_calc_mesh(*data.pbvh, node, faces);
    }
    for (const int face : faces) {
      if (data.modified_hidden_faces[face]) {
        BKE_pbvh_node_mark_update_visibility(&node);
        break;
      }
    }
  }
}

static void update_modified_node_grids(PBVHNode &node, PartialUpdateData &data)
{
  const Span<int> grid_indices = BKE_pbvh_node_get_grid_indices(node);
  if (std::any_of(grid_indices.begin(), grid_indices.end(), [&](const int grid) {
        return data.modified_grids[grid];
      }))
  {
    if (data.changed_position) {
      BKE_pbvh_node_mark_update(&node);
    }
    if (data.changed_mask) {
      BKE_pbvh_node_mark_update_mask(&node);
    }
    if (data.changed_hide_vert) {
      BKE_pbvh_node_mark_update_visibility(&node);
    }
  }

  Vector<int> faces;
  if (!data.modified_face_set_faces.is_empty()) {
    if (faces.is_empty()) {
      bke::pbvh::node_face_indices_calc_grids(*data.pbvh, node, faces);
    }
    for (const int face : faces) {
      if (data.modified_face_set_faces[face]) {
        BKE_pbvh_node_mark_update_face_sets(&node);
        break;
      }
    }
  }
  if (!data.modified_hidden_faces.is_empty()) {
    if (faces.is_empty()) {
      bke::pbvh::node_face_indices_calc_grids(*data.pbvh, node, faces);
    }
    for (const int face : faces) {
      if (data.modified_hidden_faces[face]) {
        BKE_pbvh_node_mark_update_visibility(&node);
        break;
      }
    }
  }
}

static bool test_swap_v3_v3(float3 &a, float3 &b)
{
  /* No need for float comparison here (memory is exactly equal or not). */
  if (memcmp(a, b, sizeof(float[3])) != 0) {
    std::swap(a, b);
    return true;
  }
  return false;
}

static bool restore_deformed(
    const SculptSession *ss, Node &unode, int uindex, int oindex, float3 &coord)
{
  if (test_swap_v3_v3(coord, unode.orig_position[uindex])) {
    copy_v3_v3(unode.position[uindex], ss->deform_cos[oindex]);
    return true;
  }
  return false;
}

static bool restore_coords(
    bContext *C, Object *ob, Depsgraph *depsgraph, Node &unode, MutableSpan<bool> modified_verts)
{
  SculptSession *ss = ob->sculpt;
  SubdivCCG *subdiv_ccg = ss->subdiv_ccg;

  if (unode.mesh_verts_num) {
    /* Regular mesh restore. */

    if (ss->shapekey_active && !STREQ(ss->shapekey_active->name, unode.shapeName)) {
      /* Shape key has been changed before calling undo operator. */

      Key *key = BKE_key_from_object(ob);
      KeyBlock *kb = key ? BKE_keyblock_find_name(key, unode.shapeName) : nullptr;

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
    const Span<int> index = unode.vert_indices.as_span().take_front(unode.unique_verts_num);
    MutableSpan<float3> positions = ss->vert_positions;

    if (ss->shapekey_active) {
      float(*vertCos)[3] = BKE_keyblock_convert_to_vertcos(ob, ss->shapekey_active);
      MutableSpan key_positions(reinterpret_cast<float3 *>(vertCos), ss->shapekey_active->totelem);

      if (!unode.orig_position.is_empty()) {
        if (ss->deform_modifiers_active) {
          for (const int i : index.index_range()) {
            restore_deformed(ss, unode, i, index[i], key_positions[index[i]]);
          }
        }
        else {
          for (const int i : index.index_range()) {
            std::swap(key_positions[index[i]], unode.orig_position[i]);
          }
        }
      }
      else {
        for (const int i : index.index_range()) {
          std::swap(key_positions[index[i]], unode.position[i]);
        }
      }

      /* Propagate new coords to keyblock. */
      SCULPT_vertcos_to_key(ob, ss->shapekey_active, key_positions);

      /* PBVH uses its own vertex array, so coords should be */
      /* propagated to PBVH here. */
      BKE_pbvh_vert_coords_apply(ss->pbvh, key_positions);

      MEM_freeN(vertCos);
    }
    else {
      if (!unode.orig_position.is_empty()) {
        if (ss->deform_modifiers_active) {
          for (const int i : index.index_range()) {
            restore_deformed(ss, unode, i, index[i], positions[index[i]]);
            modified_verts[index[i]] = true;
          }
        }
        else {
          for (const int i : index.index_range()) {
            std::swap(positions[index[i]], unode.orig_position[i]);
            modified_verts[index[i]] = true;
          }
        }
      }
      else {
        for (const int i : index.index_range()) {
          std::swap(positions[index[i]], unode.position[i]);
          modified_verts[index[i]] = true;
        }
      }
    }
  }
  else if (!unode.grids.is_empty() && subdiv_ccg != nullptr) {
    const CCGKey key = BKE_subdiv_ccg_key_top_level(*subdiv_ccg);
    const Span<int> grid_indices = unode.grids;

    MutableSpan<float3> position = unode.position;
    MutableSpan<CCGElem *> grids = subdiv_ccg->grids;

    int index = 0;
    for (const int i : grid_indices.index_range()) {
      CCGElem *grid = grids[grid_indices[i]];
      for (const int j : IndexRange(key.grid_area)) {
        swap_v3_v3(CCG_elem_offset_co(&key, grid, j), position[index]);
        index++;
      }
    }
  }

  return true;
}

static bool restore_hidden(Object *ob, Node &unode, MutableSpan<bool> modified_vertices)
{
  SculptSession *ss = ob->sculpt;
  SubdivCCG *subdiv_ccg = ss->subdiv_ccg;

  if (unode.mesh_verts_num) {
    Mesh &mesh = *static_cast<Mesh *>(ob->data);
    bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
    bke::SpanAttributeWriter<bool> hide_vert = attributes.lookup_or_add_for_write_span<bool>(
        ".hide_vert", bke::AttrDomain::Point);
    for (const int i : unode.vert_indices.index_range().take_front(unode.unique_verts_num)) {
      const int vert = unode.vert_indices[i];
      if (unode.vert_hidden[i].test() != hide_vert.span[vert]) {
        unode.vert_hidden[i].set(!unode.vert_hidden[i].test());
        hide_vert.span[vert] = !hide_vert.span[vert];
        modified_vertices[vert] = true;
      }
    }
    hide_vert.finish();
  }
  else if (!unode.grids.is_empty() && subdiv_ccg != nullptr) {
    if (unode.grid_hidden.is_empty()) {
      BKE_subdiv_ccg_grid_hidden_free(*subdiv_ccg);
      return true;
    }

    BitGroupVector<> &grid_hidden = BKE_subdiv_ccg_grid_hidden_ensure(*subdiv_ccg);
    const Span<int> grids = unode.grids;
    for (const int i : grids.index_range()) {
      /* Swap the two bit spans. */
      MutableBoundedBitSpan a = unode.grid_hidden[i];
      MutableBoundedBitSpan b = grid_hidden[grids[i]];
      for (const int j : a.index_range()) {
        const bool value_a = a[j];
        const bool value_b = b[j];
        a[j].set(value_b);
        b[j].set(value_a);
      }
    }
  }

  return true;
}

static bool restore_hidden_face(Object &object, Node &unode, MutableSpan<bool> modified_faces)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  bke::SpanAttributeWriter hide_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".hide_poly", bke::AttrDomain::Face);

  const Span<int> face_indices = unode.face_indices;

  bool modified = false;
  for (const int i : face_indices.index_range()) {
    const int face = face_indices[i];
    if (unode.face_hidden[i].test() != hide_poly.span[face]) {
      unode.face_hidden[i].set(!unode.face_hidden[i].test());
      hide_poly.span[face] = !hide_poly.span[face];
      modified_faces[face] = true;
      modified = true;
    }
  }
  hide_poly.finish();
  BKE_sculpt_hide_poly_pointer_update(object);
  return modified;
}

static bool restore_color(Object *ob, Node &unode, MutableSpan<bool> modified_vertices)
{
  SculptSession *ss = ob->sculpt;

  bool modified = false;

  /* NOTE: even with loop colors we still store derived
   * vertex colors for original data lookup. */
  if (!unode.col.is_empty() && unode.loop_col.is_empty()) {
    BKE_pbvh_swap_colors(
        ss->pbvh, unode.vert_indices.as_span().take_front(unode.unique_verts_num), unode.col);
    modified = true;
  }

  Mesh *mesh = BKE_object_get_original_mesh(ob);

  if (!unode.loop_col.is_empty() && unode.mesh_corners_num == mesh->corners_num) {
    BKE_pbvh_swap_colors(ss->pbvh, unode.corner_indices, unode.loop_col);
    modified = true;
  }

  if (modified) {
    modified_vertices.fill_indices(unode.vert_indices.as_span(), true);
  }

  return modified;
}

static bool restore_mask(Object *ob, Node &unode, MutableSpan<bool> modified_vertices)
{
  Mesh *mesh = BKE_object_get_original_mesh(ob);
  SculptSession *ss = ob->sculpt;
  SubdivCCG *subdiv_ccg = ss->subdiv_ccg;

  if (unode.mesh_verts_num) {
    bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
    bke::SpanAttributeWriter<float> mask = attributes.lookup_or_add_for_write_span<float>(
        ".sculpt_mask", bke::AttrDomain::Point);

    const Span<int> index = unode.vert_indices.as_span().take_front(unode.unique_verts_num);

    for (const int i : index.index_range()) {
      const int vert = index[i];
      if (mask.span[vert] != unode.mask[i]) {
        std::swap(mask.span[vert], unode.mask[i]);
        modified_vertices[vert] = true;
      }
    }

    mask.finish();
  }
  else if (!unode.grids.is_empty() && subdiv_ccg != nullptr) {
    const CCGKey key = BKE_subdiv_ccg_key_top_level(*subdiv_ccg);

    MutableSpan<float> mask = unode.mask;
    MutableSpan<CCGElem *> grids = subdiv_ccg->grids;

    int index = 0;
    for (const int grid : unode.grids) {
      CCGElem *elem = grids[grid];
      for (const int j : IndexRange(key.grid_area)) {
        std::swap(*CCG_elem_offset_mask(&key, elem, j), mask[index]);
        index++;
      }
    }
  }

  return true;
}

static bool restore_face_sets(Object *ob, Node &unode, MutableSpan<bool> modified_face_set_faces)
{
  const Span<int> face_indices = unode.face_indices;

  bke::SpanAttributeWriter<int> face_sets = face_set::ensure_face_sets_mesh(*ob);
  bool modified = false;
  for (const int i : face_indices.index_range()) {
    const int face = face_indices[i];
    if (unode.face_sets[i] == face_sets.span[face]) {
      continue;
    }
    std::swap(unode.face_sets[i], face_sets.span[face]);
    modified_face_set_faces[face] = true;
    modified = true;
  }
  face_sets.finish();
  return modified;
}

static void bmesh_restore_generic(Node &unode, Object *ob, SculptSession *ss)
{
  if (unode.applied) {
    BM_log_undo(ss->bm, ss->bm_log);
    unode.applied = false;
  }
  else {
    BM_log_redo(ss->bm, ss->bm_log);
    unode.applied = true;
  }

  if (unode.type == Type::Mask) {
    Vector<PBVHNode *> nodes = bke::pbvh::search_gather(ss->pbvh, {});
    for (PBVHNode *node : nodes) {
      BKE_pbvh_node_mark_redraw(node);
    }
  }
  else {
    SCULPT_pbvh_clear(ob);
  }
}

/* Create empty sculpt BMesh and enable logging. */
static void bmesh_enable(Object *ob, Node &unode)
{
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = static_cast<Mesh *>(ob->data);

  SCULPT_pbvh_clear(ob);

  /* Create empty BMesh and enable logging. */
  BMeshCreateParams bmesh_create_params{};
  bmesh_create_params.use_toolflags = false;

  ss->bm = BM_mesh_create(&bm_mesh_allocsize_default, &bmesh_create_params);
  BM_data_layer_add_named(ss->bm, &ss->bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");

  mesh->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;

  /* Restore the BMLog using saved entries. */
  ss->bm_log = BM_log_from_existing_entries_create(ss->bm, unode.bm_entry);
}

static void bmesh_restore_begin(bContext *C, Node &unode, Object *ob, SculptSession *ss)
{
  if (unode.applied) {
    dyntopo::disable(C, &unode);
    unode.applied = false;
  }
  else {
    bmesh_enable(ob, unode);

    /* Restore the mesh from the first log entry. */
    BM_log_redo(ss->bm, ss->bm_log);

    unode.applied = true;
  }
}

static void bmesh_restore_end(bContext *C, Node &unode, Object *ob, SculptSession *ss)
{
  if (unode.applied) {
    bmesh_enable(ob, unode);

    /* Restore the mesh from the last log entry. */
    BM_log_undo(ss->bm, ss->bm_log);

    unode.applied = false;
  }
  else {
    /* Disable dynamic topology sculpting. */
    dyntopo::disable(C, nullptr);
    unode.applied = true;
  }
}

static void store_geometry_data(NodeGeometry *geometry, Object *object)
{
  Mesh *mesh = static_cast<Mesh *>(object->data);

  BLI_assert(!geometry->is_initialized);
  geometry->is_initialized = true;

  CustomData_copy(&mesh->vert_data, &geometry->vert_data, CD_MASK_MESH.vmask, mesh->verts_num);
  CustomData_copy(&mesh->edge_data, &geometry->edge_data, CD_MASK_MESH.emask, mesh->edges_num);
  CustomData_copy(
      &mesh->corner_data, &geometry->corner_data, CD_MASK_MESH.lmask, mesh->corners_num);
  CustomData_copy(&mesh->face_data, &geometry->face_data, CD_MASK_MESH.pmask, mesh->faces_num);
  implicit_sharing::copy_shared_pointer(mesh->face_offset_indices,
                                        mesh->runtime->face_offsets_sharing_info,
                                        &geometry->face_offset_indices,
                                        &geometry->face_offsets_sharing_info);

  geometry->totvert = mesh->verts_num;
  geometry->totedge = mesh->edges_num;
  geometry->totloop = mesh->corners_num;
  geometry->faces_num = mesh->faces_num;
}

static void restore_geometry_data(NodeGeometry *geometry, Object *object)
{
  Mesh *mesh = static_cast<Mesh *>(object->data);

  BLI_assert(geometry->is_initialized);

  BKE_mesh_clear_geometry(mesh);

  mesh->verts_num = geometry->totvert;
  mesh->edges_num = geometry->totedge;
  mesh->corners_num = geometry->totloop;
  mesh->faces_num = geometry->faces_num;
  mesh->totface_legacy = 0;

  CustomData_copy(&geometry->vert_data, &mesh->vert_data, CD_MASK_MESH.vmask, geometry->totvert);
  CustomData_copy(&geometry->edge_data, &mesh->edge_data, CD_MASK_MESH.emask, geometry->totedge);
  CustomData_copy(
      &geometry->corner_data, &mesh->corner_data, CD_MASK_MESH.lmask, geometry->totloop);
  CustomData_copy(&geometry->face_data, &mesh->face_data, CD_MASK_MESH.pmask, geometry->faces_num);
  implicit_sharing::copy_shared_pointer(geometry->face_offset_indices,
                                        geometry->face_offsets_sharing_info,
                                        &mesh->face_offset_indices,
                                        &mesh->runtime->face_offsets_sharing_info);
}

static void geometry_free_data(NodeGeometry *geometry)
{
  CustomData_free(&geometry->vert_data, geometry->totvert);
  CustomData_free(&geometry->edge_data, geometry->totedge);
  CustomData_free(&geometry->corner_data, geometry->totloop);
  CustomData_free(&geometry->face_data, geometry->faces_num);
  implicit_sharing::free_shared_data(&geometry->face_offset_indices,
                                     &geometry->face_offsets_sharing_info);
}

static void restore_geometry(Node &unode, Object *object)
{
  if (unode.geometry_clear_pbvh) {
    SCULPT_pbvh_clear(object);
  }

  if (unode.applied) {
    restore_geometry_data(&unode.geometry_modified, object);
    unode.applied = false;
  }
  else {
    restore_geometry_data(&unode.geometry_original, object);
    unode.applied = true;
  }
}

/* Handle all dynamic-topology updates
 *
 * Returns true if this was a dynamic-topology undo step, otherwise
 * returns false to indicate the non-dyntopo code should run. */
static int bmesh_restore(bContext *C, Node &unode, Object *ob, SculptSession *ss)
{
  switch (unode.type) {
    case Type::DyntopoBegin:
      bmesh_restore_begin(C, unode, ob, ss);
      return true;

    case Type::DyntopoEnd:
      bmesh_restore_end(C, unode, ob, ss);
      return true;
    default:
      if (ss->bm_log) {
        bmesh_restore_generic(unode, ob, ss);
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
static void refine_subdiv(Depsgraph *depsgraph, SculptSession *ss, Object *object, Subdiv *subdiv)
{
  Array<float3> deformed_verts = BKE_multires_create_deformed_base_mesh_vert_coords(
      depsgraph, object, ss->multires.modifier);

  BKE_subdiv_eval_refine_from_mesh(subdiv,
                                   static_cast<const Mesh *>(object->data),
                                   reinterpret_cast<float(*)[3]>(deformed_verts.data()));
}

static void restore_list(bContext *C, Depsgraph *depsgraph, UndoSculpt &usculpt)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  SculptSession *ss = ob->sculpt;
  SubdivCCG *subdiv_ccg = ss->subdiv_ccg;

  bool clear_automask_cache = false;
  for (const std::unique_ptr<Node> &unode : usculpt.nodes) {
    if (!ELEM(unode->type, Type::Color, Type::Mask)) {
      clear_automask_cache = true;
    }

    /* Restore pivot. */
    copy_v3_v3(ss->pivot_pos, unode->pivot_pos);
    copy_v3_v3(ss->pivot_rot, unode->pivot_rot);
  }

  if (clear_automask_cache) {
    ss->last_automasking_settings_hash = 0;
  }

  if (!usculpt.nodes.is_empty()) {
    /* Only do early object update for edits if first node needs this.
     * Undo steps like geometry does not need object to be updated before they run and will
     * ensure object is updated after the node is handled. */
    const Node *first_unode = usculpt.nodes.first().get();
    if (first_unode->type != Type::Geometry) {
      BKE_sculpt_update_object_for_edit(depsgraph, ob, false);
    }

    if (bmesh_restore(C, *usculpt.nodes.first(), ob, ss)) {
      return;
    }
  }

  bool use_multires_undo = false;

  bool changed_all_geometry = false;
  bool changed_position = false;
  bool changed_hide_vert = false;
  bool changed_hide_face = false;
  bool changed_mask = false;
  bool changed_face_sets = false;
  bool changed_color = false;

  /* The PBVH already keeps track of which vertices need updated normals, but it doesn't keep
   * track of other updates. In order to tell the corresponding PBVH nodes to update, keep track
   * of which elements were updated for specific layers. */
  Vector<bool> modified_verts_position;
  Vector<bool> modified_verts_hide;
  Vector<bool> modified_faces_hide;
  Vector<bool> modified_verts_mask;
  Vector<bool> modified_verts_color;
  Vector<bool> modified_faces_face_set;
  Vector<bool> modified_grids;
  for (std::unique_ptr<Node> &unode : usculpt.nodes) {
    if (!STREQ(unode->idname, ob->id.name)) {
      continue;
    }

    /* Check if undo data matches current data well enough to continue. */
    if (unode->mesh_verts_num) {
      if (ss->totvert != unode->mesh_verts_num) {
        continue;
      }
    }
    else if (unode->maxgrid && subdiv_ccg != nullptr) {
      if ((subdiv_ccg->grids.size() != unode->maxgrid) ||
          (subdiv_ccg->grid_size != unode->gridsize))
      {
        continue;
      }

      use_multires_undo = true;
    }

    switch (unode->type) {
      case Type::Position:
        modified_verts_position.resize(ss->totvert, false);
        if (restore_coords(C, ob, depsgraph, *unode, modified_verts_position)) {
          changed_position = true;
        }
        break;
      case Type::HideVert:
        modified_verts_hide.resize(ss->totvert, false);
        if (restore_hidden(ob, *unode, modified_verts_hide)) {
          changed_hide_vert = true;
        }
        break;
      case Type::HideFace:
        modified_faces_hide.resize(ss->totfaces, false);
        if (restore_hidden_face(*ob, *unode, modified_faces_hide)) {
          changed_hide_face = true;
        }
        break;
      case Type::Mask:
        modified_verts_mask.resize(ss->totvert, false);
        if (restore_mask(ob, *unode, modified_verts_mask)) {
          changed_mask = true;
        }
        break;
      case Type::FaceSet:
        modified_faces_face_set.resize(ss->totfaces, false);
        if (restore_face_sets(ob, *unode, modified_faces_face_set)) {
          changed_face_sets = true;
        }
        break;
      case Type::Color:
        modified_verts_color.resize(ss->totvert, false);
        if (restore_color(ob, *unode, modified_verts_color)) {
          changed_color = true;
        }
        break;
      case Type::Geometry:
        restore_geometry(*unode, ob);
        changed_all_geometry = true;
        BKE_sculpt_update_object_for_edit(depsgraph, ob, false);
        break;

      case Type::DyntopoBegin:
      case Type::DyntopoEnd:
      case Type::DyntopoSymmetrize:
        BLI_assert_msg(0, "Dynamic topology should've already been handled");
        break;
    }
  }

  if (use_multires_undo) {
    for (std::unique_ptr<Node> &unode : usculpt.nodes) {
      if (!STREQ(unode->idname, ob->id.name)) {
        continue;
      }
      modified_grids.resize(unode->maxgrid, false);
      modified_grids.as_mutable_span().fill_indices(unode->grids.as_span(), true);
    }
  }

  if (subdiv_ccg != nullptr && changed_all_geometry) {
    refine_subdiv(depsgraph, ss, ob, subdiv_ccg->subdiv);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);

  if (!changed_position && !changed_hide_vert && !changed_hide_face && !changed_mask &&
      !changed_face_sets && !changed_color)
  {
    return;
  }

  /* We update all nodes still, should be more clever, but also
   * needs to work correct when exiting/entering sculpt mode and
   * the nodes get recreated, though in that case it could do all. */
  PartialUpdateData data{};
  data.changed_position = changed_position;
  data.changed_hide_vert = changed_hide_vert;
  data.changed_mask = changed_mask;
  data.pbvh = ss->pbvh;
  data.modified_grids = modified_grids;
  data.modified_position_verts = modified_verts_position;
  data.modified_hidden_verts = modified_verts_hide;
  data.modified_hidden_faces = modified_faces_hide;
  data.modified_mask_verts = modified_verts_mask;
  data.modified_color_verts = modified_verts_color;
  data.modified_face_set_faces = modified_faces_face_set;
  if (use_multires_undo) {
    bke::pbvh::search_callback(
        *ss->pbvh, {}, [&](PBVHNode &node) { update_modified_node_grids(node, data); });
  }
  else {
    bke::pbvh::search_callback(
        *ss->pbvh, {}, [&](PBVHNode &node) { update_modified_node_mesh(node, data); });
  }

  if (changed_position) {
    bke::pbvh::update_bounds(*ss->pbvh, PBVH_UpdateBB | PBVH_UpdateOriginalBB | PBVH_UpdateRedraw);
  }
  if (changed_mask) {
    bke::pbvh::update_mask(*ss->pbvh);
  }
  if (changed_hide_face) {
    hide::sync_all_from_faces(*ob);
    bke::pbvh::update_visibility(*ss->pbvh);
  }
  if (changed_hide_vert) {
    if (ELEM(BKE_pbvh_type(ss->pbvh), PBVH_FACES, PBVH_GRIDS)) {
      Mesh &mesh = *static_cast<Mesh *>(ob->data);
      BKE_pbvh_sync_visibility_from_verts(ss->pbvh, &mesh);
    }
    bke::pbvh::update_visibility(*ss->pbvh);
  }

  if (BKE_sculpt_multires_active(scene, ob)) {
    if (changed_hide_vert) {
      multires_mark_as_modified(depsgraph, ob, MULTIRES_HIDDEN_MODIFIED);
    }
    else if (changed_position) {
      multires_mark_as_modified(depsgraph, ob, MULTIRES_COORDS_MODIFIED);
    }
  }

  const bool tag_update = ID_REAL_USERS(ob->data) > 1 ||
                          !BKE_sculptsession_use_pbvh_draw(ob, rv3d) || ss->shapekey_active ||
                          ss->deform_modifiers_active;

  if (tag_update) {
    Mesh *mesh = static_cast<Mesh *>(ob->data);
    if (changed_position) {
      mesh->tag_positions_changed();
      BKE_sculptsession_free_deformMats(ss);
    }
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }
}

static void free_list(UndoSculpt &usculpt)
{
  for (std::unique_ptr<Node> &unode : usculpt.nodes) {
    geometry_free_data(&unode->geometry_original);
    geometry_free_data(&unode->geometry_modified);
    geometry_free_data(&unode->geometry_bmesh_enter);
    if (unode->bm_entry) {
      BM_log_entry_drop(unode->bm_entry);
    }
  }
  usculpt.nodes.~Vector();
}

Node *get_node(PBVHNode *node, Type type)
{
  UndoSculpt *usculpt = get_nodes();

  if (usculpt == nullptr) {
    return nullptr;
  }

  for (std::unique_ptr<Node> &unode : usculpt->nodes) {
    if (unode->node == node && unode->type == type) {
      return unode.get();
    }
  }

  return nullptr;
}

static size_t alloc_and_store_hidden(SculptSession *ss, Node *unode)
{
  PBVHNode *node = static_cast<PBVHNode *>(unode->node);
  if (!ss->subdiv_ccg) {
    return 0;
  }
  const BitGroupVector<> grid_hidden = ss->subdiv_ccg->grid_hidden;
  if (grid_hidden.is_empty()) {
    return 0;
  }

  const Span<int> grid_indices = BKE_pbvh_node_get_grid_indices(*node);
  unode->grid_hidden = BitGroupVector<>(grid_indices.size(), grid_hidden.group_size());
  for (const int i : grid_indices.index_range()) {
    unode->grid_hidden[i].copy_from(grid_hidden[grid_indices[i]]);
  }

  return unode->grid_hidden.all_bits().full_ints_num() / bits::BitsPerInt;
}

/* Allocate node and initialize its default fields specific for the given undo type.
 * Will also add the node to the list in the undo step. */
static Node *alloc_node_type(Object *object, Type type)
{
  UndoSculpt *usculpt = get_nodes();
  std::unique_ptr<Node> unode = std::make_unique<Node>();
  usculpt->nodes.append(std::move(unode));
  usculpt->undo_size += sizeof(Node);

  Node *node_ptr = usculpt->nodes.last().get();
  STRNCPY(node_ptr->idname, object->id.name);
  node_ptr->type = type;

  return node_ptr;
}

/* Will return first existing undo node of the given type.
 * If such node does not exist will allocate node of this type, register it in the undo step and
 * return it. */
static Node *find_or_alloc_node_type(Object *object, Type type)
{
  UndoSculpt *usculpt = get_nodes();

  for (std::unique_ptr<Node> &unode : usculpt->nodes) {
    if (unode->type == type) {
      return unode.get();
    }
  }

  return alloc_node_type(object, type);
}

static Node *alloc_node(Object *ob, PBVHNode *node, Type type)
{
  UndoSculpt *usculpt = get_nodes();
  SculptSession *ss = ob->sculpt;

  Node *unode = alloc_node_type(ob, type);
  unode->node = node;

  int verts_num;
  if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
    unode->maxgrid = ss->subdiv_ccg->grids.size();
    unode->gridsize = ss->subdiv_ccg->grid_size;

    verts_num = unode->maxgrid * unode->gridsize * unode->gridsize;

    unode->grids = BKE_pbvh_node_get_grid_indices(*node);
    usculpt->undo_size += unode->grids.as_span().size_in_bytes();
  }
  else {
    unode->mesh_verts_num = ss->totvert;

    unode->vert_indices = BKE_pbvh_node_get_vert_indices(node);
    unode->unique_verts_num = BKE_pbvh_node_get_unique_vert_indices(node).size();

    verts_num = unode->vert_indices.size();

    usculpt->undo_size += unode->vert_indices.as_span().size_in_bytes();
  }

  bool need_loops = type == Type::Color;
  const bool need_faces = ELEM(type, Type::FaceSet, Type::HideFace);

  if (need_loops) {
    unode->corner_indices = BKE_pbvh_node_get_loops(node);
    unode->mesh_corners_num = static_cast<Mesh *>(ob->data)->corners_num;

    usculpt->undo_size += unode->corner_indices.as_span().size_in_bytes();
  }

  if (need_faces) {
    unode->face_indices = BKE_pbvh_node_calc_face_indices(*ss->pbvh, *node);
    usculpt->undo_size += unode->face_indices.as_span().size_in_bytes();
  }

  switch (type) {
    case Type::Position: {
      unode->position.reinitialize(verts_num);
      usculpt->undo_size += unode->position.as_span().size_in_bytes();

      /* Needed for original data lookup. */
      unode->normal.reinitialize(verts_num);
      usculpt->undo_size += unode->normal.as_span().size_in_bytes();
      break;
    }
    case Type::HideVert: {
      if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
        usculpt->undo_size += alloc_and_store_hidden(ss, unode);
      }
      else {
        unode->vert_hidden.resize(unode->vert_indices.size());
        usculpt->undo_size += unode->vert_hidden.size() / 8;
      }

      break;
    }
    case Type::HideFace: {
      unode->face_hidden.resize(unode->face_indices.size());
      usculpt->undo_size += unode->face_hidden.size() / 8;
      break;
    }
    case Type::Mask: {
      unode->mask.reinitialize(verts_num);
      usculpt->undo_size += unode->mask.as_span().size_in_bytes();
      break;
    }
    case Type::Color: {
      /* Allocate vertex colors, even for loop colors we still
       * need this for original data lookup. */
      unode->col.reinitialize(verts_num);
      usculpt->undo_size += unode->col.as_span().size_in_bytes();

      /* Allocate loop colors separately too. */
      if (ss->vcol_domain == bke::AttrDomain::Corner) {
        unode->loop_col.reinitialize(unode->corner_indices.size());
        unode->undo_size += unode->loop_col.as_span().size_in_bytes();
      }
      break;
    }
    case Type::DyntopoBegin:
    case Type::DyntopoEnd:
    case Type::DyntopoSymmetrize:
      BLI_assert_msg(0, "Dynamic topology should've already been handled");
      break;
    case Type::Geometry:
      break;
    case Type::FaceSet: {
      unode->face_sets.reinitialize(unode->face_indices.size());
      usculpt->undo_size += unode->face_sets.as_span().size_in_bytes();
      break;
    }
  }

  if (ss->deform_modifiers_active) {
    unode->orig_position.reinitialize(unode->vert_indices.size());
    usculpt->undo_size += unode->orig_position.as_span().size_in_bytes();
  }

  return unode;
}

static void store_coords(Object *ob, Node *unode)
{
  SculptSession *ss = ob->sculpt;

  if (!unode->grids.is_empty()) {
    const SubdivCCG &subdiv_ccg = *ss->subdiv_ccg;
    const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
    const Span<CCGElem *> grids = subdiv_ccg.grids;
    {
      int index = 0;
      for (const int grid : unode->grids) {
        CCGElem *elem = grids[grid];
        for (const int i : IndexRange(key.grid_area)) {
          unode->position[index] = float3(CCG_elem_offset_co(&key, elem, i));
          index++;
        }
      }
    }
    if (key.has_normals) {
      int index = 0;
      for (const int grid : unode->grids) {
        CCGElem *elem = grids[grid];
        for (const int i : IndexRange(key.grid_area)) {
          unode->normal[index] = float3(CCG_elem_offset_no(&key, elem, i));
          index++;
        }
      }
    }
  }
  else {
    array_utils::gather(BKE_pbvh_get_vert_positions(ss->pbvh).as_span(),
                        unode->vert_indices.as_span(),
                        unode->position.as_mutable_span());
    array_utils::gather(BKE_pbvh_get_vert_normals(ss->pbvh),
                        unode->vert_indices.as_span(),
                        unode->normal.as_mutable_span());
    if (ss->deform_modifiers_active) {
      array_utils::gather(ss->orig_cos.as_span(),
                          unode->vert_indices.as_span(),
                          unode->orig_position.as_mutable_span());
    }
  }
}

static void store_hidden(Object *ob, Node *unode)
{
  if (!unode->grids.is_empty()) {
    /* Already stored during allocation. */
  }

  const Mesh &mesh = *static_cast<const Mesh *>(ob->data);
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<bool> hide_vert = *attributes.lookup<bool>(".hide_vert",
                                                              bke::AttrDomain::Point);
  if (hide_vert.is_empty()) {
    return;
  }

  PBVHNode *node = static_cast<PBVHNode *>(unode->node);
  const Span<int> verts = BKE_pbvh_node_get_vert_indices(node);
  for (const int i : verts.index_range()) {
    unode->vert_hidden[i].set(hide_vert[verts[i]]);
  }
}

static void store_face_hidden(Object &object, Node &unode)
{
  const Mesh &mesh = *static_cast<const Mesh *>(object.data);
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<bool> hide_poly = *attributes.lookup<bool>(".hide_poly", bke::AttrDomain::Face);
  if (hide_poly.is_empty()) {
    unode.face_hidden.fill(false);
    return;
  }
  const Span<int> faces = unode.face_indices;
  for (const int i : faces.index_range()) {
    unode.face_hidden[i].set(hide_poly[faces[i]]);
  }
}

static void store_mask(Object *ob, Node *unode)
{
  const SculptSession *ss = ob->sculpt;

  if (!unode->grids.is_empty()) {
    const SubdivCCG &subdiv_ccg = *ss->subdiv_ccg;
    const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
    if (key.has_mask) {
      const Span<CCGElem *> grids = subdiv_ccg.grids;
      int index = 0;
      for (const int grid : unode->grids) {
        CCGElem *elem = grids[grid];
        for (const int i : IndexRange(key.grid_area)) {
          unode->mask[index] = *CCG_elem_offset_mask(&key, elem, i);
          index++;
        }
      }
    }
    else {
      unode->mask.fill(0.0f);
    }
  }
  else {
    const Mesh &mesh = *static_cast<const Mesh *>(ob->data);
    const bke::AttributeAccessor attributes = mesh.attributes();
    if (const VArray mask = *attributes.lookup<float>(".sculpt_mask", bke::AttrDomain::Point)) {
      array_utils::gather(mask, unode->vert_indices.as_span(), unode->mask.as_mutable_span());
    }
    else {
      unode->mask.fill(0.0f);
    }
  }
}

static void store_color(Object *ob, Node *unode)
{
  SculptSession *ss = ob->sculpt;

  BLI_assert(BKE_pbvh_type(ss->pbvh) == PBVH_FACES);

  /* NOTE: even with loop colors we still store (derived)
   * vertex colors for original data lookup. */
  BKE_pbvh_store_colors_vertex(
      ss->pbvh, unode->vert_indices.as_span().take_front(unode->unique_verts_num), unode->col);

  if (!unode->loop_col.is_empty() && !unode->corner_indices.is_empty()) {
    BKE_pbvh_store_colors(ss->pbvh, unode->corner_indices, unode->loop_col);
  }
}

static NodeGeometry *geometry_get(Node *unode)
{
  if (!unode->geometry_original.is_initialized) {
    return &unode->geometry_original;
  }

  BLI_assert(!unode->geometry_modified.is_initialized);

  return &unode->geometry_modified;
}

static Node *geometry_push(Object *object, Type type)
{
  Node *unode = find_or_alloc_node_type(object, type);
  unode->applied = false;
  unode->geometry_clear_pbvh = true;

  NodeGeometry *geometry = geometry_get(unode);
  store_geometry_data(geometry, object);

  return unode;
}

static void store_face_sets(const Mesh &mesh, Node &unode)
{
  array_utils::gather(
      *mesh.attributes().lookup_or_default<int>(".sculpt_face_set", bke::AttrDomain::Face, 1),
      unode.face_indices.as_span(),
      unode.face_sets.as_mutable_span());
}

static Node *bmesh_push(Object *ob, PBVHNode *node, Type type)
{
  UndoSculpt *usculpt = get_nodes();
  SculptSession *ss = ob->sculpt;

  Node *unode = usculpt->nodes.is_empty() ? nullptr : usculpt->nodes.first().get();

  if (unode == nullptr) {
    usculpt->nodes.append(std::make_unique<Node>());
    unode = usculpt->nodes.last().get();

    STRNCPY(unode->idname, ob->id.name);
    unode->type = type;
    unode->applied = true;

    if (type == Type::DyntopoEnd) {
      unode->bm_entry = BM_log_entry_add(ss->bm_log);
      BM_log_before_all_removed(ss->bm, ss->bm_log);
    }
    else if (type == Type::DyntopoBegin) {
      /* Store a copy of the mesh's current vertices, loops, and
       * faces. A full copy like this is needed because entering
       * dynamic-topology immediately does topological edits
       * (converting faces to triangles) that the BMLog can't
       * fully restore from. */
      NodeGeometry *geometry = &unode->geometry_bmesh_enter;
      store_geometry_data(geometry, ob);

      unode->bm_entry = BM_log_entry_add(ss->bm_log);
      BM_log_all_added(ss->bm, ss->bm_log);
    }
    else {
      unode->bm_entry = BM_log_entry_add(ss->bm_log);
    }
  }

  if (node) {
    const int cd_vert_mask_offset = CustomData_get_offset_named(
        &ss->bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");

    switch (type) {
      case Type::Position:
      case Type::Mask:
        /* Before any vertex values get modified, ensure their
         * original positions are logged. */
        for (BMVert *vert : BKE_pbvh_bmesh_node_unique_verts(node)) {
          BM_log_vert_before_modified(ss->bm_log, vert, cd_vert_mask_offset);
        }
        for (BMVert *vert : BKE_pbvh_bmesh_node_other_verts(node)) {
          BM_log_vert_before_modified(ss->bm_log, vert, cd_vert_mask_offset);
        }
        break;

      case Type::HideFace:
      case Type::HideVert: {
        for (BMVert *vert : BKE_pbvh_bmesh_node_unique_verts(node)) {
          BM_log_vert_before_modified(ss->bm_log, vert, cd_vert_mask_offset);
        }
        for (BMVert *vert : BKE_pbvh_bmesh_node_other_verts(node)) {
          BM_log_vert_before_modified(ss->bm_log, vert, cd_vert_mask_offset);
        }

        for (BMFace *f : BKE_pbvh_bmesh_node_faces(node)) {
          BM_log_face_modified(ss->bm_log, f);
        }
        break;
      }

      case Type::DyntopoBegin:
      case Type::DyntopoEnd:
      case Type::DyntopoSymmetrize:
      case Type::Geometry:
      case Type::FaceSet:
      case Type::Color:
        break;
    }
  }

  return unode;
}

Node *push_node(Object *ob, PBVHNode *node, Type type)
{
  SculptSession *ss = ob->sculpt;

  Node *unode;

  /* List is manipulated by multiple threads, so we lock. */
  BLI_thread_lock(LOCK_CUSTOM1);

  ss->needs_flush_to_id = 1;

  threading::isolate_task([&]() {
    if (ss->bm || ELEM(type, Type::DyntopoBegin, Type::DyntopoEnd)) {
      /* Dynamic topology stores only one undo node per stroke,
       * regardless of the number of PBVH nodes modified. */
      unode = bmesh_push(ob, node, type);
      BLI_thread_unlock(LOCK_CUSTOM1);
      // return unode;
      return;
    }
    if (type == Type::Geometry) {
      unode = geometry_push(ob, type);
      BLI_thread_unlock(LOCK_CUSTOM1);
      // return unode;
      return;
    }
    if ((unode = get_node(node, type))) {
      BLI_thread_unlock(LOCK_CUSTOM1);
      // return unode;
      return;
    }

    unode = alloc_node(ob, node, type);

    /* NOTE: If this ever becomes a bottleneck, make a lock inside of the node.
     * so we release global lock sooner, but keep data locked for until it is
     * fully initialized. */
    switch (type) {
      case Type::Position:
        store_coords(ob, unode);
        break;
      case Type::HideVert:
        store_hidden(ob, unode);
        break;
      case Type::HideFace:
        store_face_hidden(*ob, *unode);
        break;
      case Type::Mask:
        store_mask(ob, unode);
        break;
      case Type::Color:
        store_color(ob, unode);
        break;
      case Type::DyntopoBegin:
      case Type::DyntopoEnd:
      case Type::DyntopoSymmetrize:
        BLI_assert_msg(0, "Dynamic topology should've already been handled");
      case Type::Geometry:
        break;
      case Type::FaceSet:
        store_face_sets(*static_cast<const Mesh *>(ob->data), *unode);
        break;
    }

    BLI_thread_unlock(LOCK_CUSTOM1);
  });

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

  return unode;
}

static void sculpt_save_active_attribute(Object *ob, SculptAttrRef *attr)
{
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

void push_begin(Object *ob, const wmOperator *op)
{
  push_begin_ex(ob, op->type->name);
}

void push_begin_ex(Object *ob, const char *name)
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

  /* Set end attribute in case push_end is not called,
   * so we don't end up with corrupted state.
   */
  if (!us->active_color_end.was_set) {
    sculpt_save_active_attribute(ob, &us->active_color_end);
    us->active_color_end.was_set = false;
  }
}

void push_end(Object *ob)
{
  push_end_ex(ob, false);
}

void push_end_ex(Object *ob, const bool use_nested_undo)
{
  UndoSculpt *usculpt = get_nodes();

  /* We don't need normals in the undo stack. */
  for (std::unique_ptr<Node> &unode : usculpt->nodes) {
    usculpt->undo_size -= unode->normal.as_span().size_in_bytes();
    unode->normal = {};
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
  print_nodes(ob, nullptr);
}

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

static void set_active_layer(bContext *C, SculptAttrRef *attr)
{
  if (attr->domain == bke::AttrDomain::Auto) {
    return;
  }

  Object *ob = CTX_data_active_object(C);
  Mesh *mesh = BKE_object_get_original_mesh(ob);

  SculptAttrRef existing;
  sculpt_save_active_attribute(ob, &existing);

  CustomDataLayer *layer = BKE_id_attribute_find(&mesh->id, attr->name, attr->type, attr->domain);

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
        &mesh->id, attr->name, CD_MASK_PROP_ALL, ATTR_DOMAIN_MASK_ALL);
    if (layer) {
      if (ED_geometry_attribute_convert(
              mesh, attr->name, eCustomDataType(attr->type), attr->domain, nullptr))
      {
        layer = BKE_id_attribute_find(&mesh->id, attr->name, attr->type, attr->domain);
      }
    }
  }

  if (!layer) {
    /* Memfile undo killed the layer; re-create it. */
    mesh->attributes_for_write().add(
        attr->name, attr->domain, attr->type, bke::AttributeInitDefaultValue());
    layer = BKE_id_attribute_find(&mesh->id, attr->name, attr->type, attr->domain);
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }

  if (layer) {
    BKE_id_attributes_active_color_set(&mesh->id, layer->name);

    if (ob->sculpt && ob->sculpt->pbvh) {
      BKE_pbvh_update_active_vcol(ob->sculpt->pbvh, mesh);
    }
  }
}

static void sculpt_undosys_step_encode_init(bContext * /*C*/, UndoStep *us_p)
{
  SculptUndoStep *us = (SculptUndoStep *)us_p;
  new (&us->data.nodes) Vector<std::unique_ptr<Node>>();
}

static bool sculpt_undosys_step_encode(bContext * /*C*/, Main *bmain, UndoStep *us_p)
{
  /* Dummy, encoding is done along the way by adding tiles
   * to the current 'SculptUndoStep' added by encode_init. */
  SculptUndoStep *us = (SculptUndoStep *)us_p;
  us->step.data_size = us->data.undo_size;

  Node *unode = us->data.nodes.is_empty() ? nullptr : us->data.nodes.last().get();
  if (unode && unode->type == Type::DyntopoEnd) {
    us->step.use_memfile_step = true;
  }
  us->step.is_applied = true;

  if (!us->data.nodes.is_empty()) {
    bmain->is_memfile_undo_flush_needed = true;
  }

  return true;
}

static void sculpt_undosys_step_decode_undo_impl(bContext *C,
                                                 Depsgraph *depsgraph,
                                                 SculptUndoStep *us)
{
  BLI_assert(us->step.is_applied == true);

  restore_list(C, depsgraph, us->data);
  us->step.is_applied = false;

  print_nodes(CTX_data_active_object(C), nullptr);
}

static void sculpt_undosys_step_decode_redo_impl(bContext *C,
                                                 Depsgraph *depsgraph,
                                                 SculptUndoStep *us)
{
  BLI_assert(us->step.is_applied == false);

  restore_list(C, depsgraph, us->data);
  us->step.is_applied = true;

  print_nodes(CTX_data_active_object(C), nullptr);
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

    set_active_layer(C, &((SculptUndoStep *)us_iter)->active_color_start);
    sculpt_undosys_step_decode_undo_impl(C, depsgraph, us_iter);

    if (us_iter == us) {
      if (us_iter->step.prev && us_iter->step.prev->type == BKE_UNDOSYS_TYPE_SCULPT) {
        set_active_layer(C, &((SculptUndoStep *)us_iter->step.prev)->active_color_end);
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
    set_active_layer(C, &((SculptUndoStep *)us_iter)->active_color_end);
    sculpt_undosys_step_decode_redo_impl(C, depsgraph, us_iter);

    if (us_iter == us) {
      set_active_layer(C, &((SculptUndoStep *)us_iter)->active_color_start);
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

        Mesh *mesh = static_cast<Mesh *>(ob->data);
        /* Don't add sculpt topology undo steps when reading back undo state.
         * The undo steps must enter/exit for us. */
        mesh->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;
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
  free_list(us->data);
}

void geometry_begin(Object *ob, const wmOperator *op)
{
  push_begin(ob, op);
  push_node(ob, nullptr, Type::Geometry);
}

void geometry_begin_ex(Object *ob, const char *name)
{
  push_begin_ex(ob, name);
  push_node(ob, nullptr, Type::Geometry);
}

void geometry_end(Object *ob)
{
  push_node(ob, nullptr, Type::Geometry);
  push_end(ob);
}

void register_type(UndoType *ut)
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

static UndoSculpt *get_nodes()
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

static bool use_multires_mesh(bContext *C)
{
  if (BKE_paintmode_get_active_from_context(C) != PaintMode::Sculpt) {
    return false;
  }

  Object *object = CTX_data_active_object(C);
  SculptSession *sculpt_session = object->sculpt;

  return sculpt_session->multires.active;
}

static void push_all_grids(Object *object)
{
  SculptSession *ss = object->sculpt;

  /* It is possible that undo push is done from an object state where there is no PBVH. This
   * happens, for example, when an operation which tagged for geometry update was performed prior
   * to the current operation without making any stroke in between.
   *
   * Skip pushing nodes based on the following logic: on redo Type::Position will
   * ensure PBVH for the new base geometry, which will have same coordinates as if we create PBVH
   * here.
   */
  if (ss->pbvh == nullptr) {
    return;
  }

  Vector<PBVHNode *> nodes = bke::pbvh::search_gather(ss->pbvh, {});
  for (PBVHNode *node : nodes) {
    Node *unode = push_node(object, node, Type::Position);
    unode->node = nullptr;
  }
}

void push_multires_mesh_begin(bContext *C, const char *str)
{
  if (!use_multires_mesh(C)) {
    return;
  }

  Object *object = CTX_data_active_object(C);

  push_begin_ex(object, str);

  Node *geometry_unode = push_node(object, nullptr, Type::Geometry);
  geometry_unode->geometry_clear_pbvh = false;

  push_all_grids(object);
}

void push_multires_mesh_end(bContext *C, const char *str)
{
  if (!use_multires_mesh(C)) {
    ED_undo_push(C, str);
    return;
  }

  Object *object = CTX_data_active_object(C);

  Node *geometry_unode = push_node(object, nullptr, Type::Geometry);
  geometry_unode->geometry_clear_pbvh = false;

  push_end(object);
}

/** \} */

}  // namespace blender::ed::sculpt_paint::undo
