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
#include "BLI_utildefines.h"

#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_attribute.hh"
#include "BKE_ccg.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_global.hh"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_multires.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_scene.hh"
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

/* Storage of geometry for the undo node.
 * Is used as a storage for either original or modified geometry. */
struct NodeGeometry {
  /* Is used for sanity check, helping with ensuring that two and only two
   * geometry pushes happened in the undo stack. */
  bool is_initialized;

  CustomData vert_data;
  CustomData edge_data;
  CustomData corner_data;
  CustomData face_data;
  int *face_offset_indices;
  const ImplicitSharingInfo *face_offsets_sharing_info;
  int totvert;
  int totedge;
  int totloop;
  int faces_num;
};

struct StepData {
  /**
   * The type of data stored in this undo step. For historical reasons this is often set when the
   * first undo node is pushed.
   */
  Type type = Type::None;

  /** Name of the object associated with this undo data (`object.id.name`). */
  std::string object_name;

  /** Name of the object's active shape key when the undo step was created. */
  std::string active_shape_key_name;

  /* The number of vertices in the entire mesh. */
  int mesh_verts_num;
  /* The number of face corners in the entire mesh. */
  int mesh_corners_num;

  /** The number of grids in the entire mesh. */
  int mesh_grids_num;
  /** A copy of #SubdivCCG::grid_size. */
  int grid_size;

  float3 pivot_pos;
  float4 pivot_rot;

  /* Geometry modification operations.
   *
   * Original geometry is stored before some modification is run and is used to restore state of
   * the object when undoing the operation
   *
   * Modified geometry is stored after the modification and is used to redo the modification. */
  bool geometry_clear_pbvh;
  NodeGeometry geometry_original;
  NodeGeometry geometry_modified;

  /* bmesh */
  BMLogEntry *bm_entry;

  /* Geometry at the bmesh enter moment. */
  NodeGeometry geometry_bmesh_enter;

  bool applied;

  std::mutex nodes_mutex;

  /**
   * #undo::Node is stored per pbvh::Node to reduce data storage needed for changes only impacting
   * small portions of the mesh. During undo step creation and brush evaluation we often need to
   * look up the undo state for a specific node. That lookup must be protected by a lock since
   * nodes are pushed from multiple threads. This map speeds up undo node access to reduce the
   * amount of time we wait for the lock.
   *
   * This is only accessible when building the undo step, in between #push_begin and #push_end.
   *
   * \todo All nodes in a single step have the same type, so using the type as part of the map key
   * should be unnecessary. However, to remove it, first the storage of the undo type should be
   * moved to #StepData from #Node.
   */
  Map<const bke::pbvh::Node *, std::unique_ptr<Node>> undo_nodes_by_pbvh_node;

  /** Storage of per-node undo data after creation of the undo step is finished. */
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
  StepData data;

  /* Active color attribute at the start of this undo step. */
  SculptAttrRef active_color_start;

  /* Active color attribute at the end of this undo step. */
  SculptAttrRef active_color_end;

#ifdef SCULPT_UNDO_DEBUG
  int id;
#endif
};

static SculptUndoStep *get_active_step()
{
  UndoStack *ustack = ED_undo_stack_get();
  UndoStep *us = BKE_undosys_stack_init_or_active_with_type(ustack, BKE_UNDOSYS_TYPE_SCULPT);
  return reinterpret_cast<SculptUndoStep *>(us);
}

static StepData *get_step_data()
{
  if (SculptUndoStep *us = get_active_step()) {
    return &us->data;
  }
  return nullptr;
}

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

static void print_sculpt_node(Object &ob, Node *node)
{
  printf("    %s:%s {applied=%d}\n", undo_type_to_str(node->type), node->idname, node->applied);

  if (node->bm_entry) {
    BM_log_print_entry(object.sculpt ? object.sculpt->bm : nullptr, node->bm_entry);
  }
}

static void print_step(Object &ob, UndoStep *us, UndoStep *active, int i)
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
    StepData *step_data = reinterpret_cast<SculptUndoStep *>(us)->data;

    for (node = step_data->nodes.first; node; node = node->next) {
      print_sculpt_node(ob, node);
    }
  }
}
static void print_nodes(Object &ob, void *active)
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
static void print_nodes(Object & /*ob*/, void * /*active*/) {}
#endif

static bool use_multires_undo(const StepData &step_data, const SculptSession &ss)
{
  return step_data.mesh_grids_num != 0 && ss.subdiv_ccg != nullptr;
}

static bool topology_matches(const StepData &step_data, const Object &object)
{
  const SculptSession &ss = *object.sculpt;
  if (use_multires_undo(step_data, ss)) {
    const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
    return subdiv_ccg.grids.size() == step_data.mesh_grids_num &&
           subdiv_ccg.grid_size == step_data.grid_size;
  }
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  return mesh.verts_num == step_data.mesh_verts_num;
}

static bool indices_contain_true(const Span<bool> data, const Span<int> indices)
{
  return std::any_of(indices.begin(), indices.end(), [&](const int i) { return data[i]; });
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
    const SculptSession &ss, Node &unode, int uindex, int oindex, float3 &coord)
{
  if (test_swap_v3_v3(coord, unode.orig_position[uindex])) {
    copy_v3_v3(unode.position[uindex], ss.deform_cos[oindex]);
    return true;
  }
  return false;
}

static bool restore_active_shape_key(bContext &C,
                                     Depsgraph &depsgraph,
                                     const StepData &step_data,
                                     Object &object)
{
  SculptSession &ss = *object.sculpt;
  if (ss.shapekey_active && ss.shapekey_active->name != step_data.active_shape_key_name) {
    /* Shape key has been changed before calling undo operator. */

    Key *key = BKE_key_from_object(&object);
    KeyBlock *kb = key ? BKE_keyblock_find_name(key, step_data.active_shape_key_name.c_str()) :
                         nullptr;

    if (kb) {
      object.shapenr = BLI_findindex(&key->block, kb) + 1;

      BKE_sculpt_update_object_for_edit(&depsgraph, &object, false);
      WM_event_add_notifier(&C, NC_OBJECT | ND_DATA, &object);
    }
    else {
      /* Key has been removed -- skip this undo node. */
      return false;
    }
  }
  return true;
}

static void restore_position_mesh(Object &object,
                                  const Span<std::unique_ptr<Node>> unodes,
                                  MutableSpan<bool> modified_verts)
{
  SculptSession &ss = *object.sculpt;

  if (ss.shapekey_active) {
    float(*vertCos)[3] = BKE_keyblock_convert_to_vertcos(&object, ss.shapekey_active);
    MutableSpan key_positions(reinterpret_cast<float3 *>(vertCos), ss.shapekey_active->totelem);

    for (const std::unique_ptr<Node> &unode : unodes) {
      const Span<int> verts = unode->vert_indices.as_span().take_front(unode->unique_verts_num);
      if (!unode->orig_position.is_empty()) {
        if (ss.deform_modifiers_active) {
          for (const int i : verts.index_range()) {
            restore_deformed(ss, *unode, i, verts[i], key_positions[verts[i]]);
          }
        }
        else {
          for (const int i : verts.index_range()) {
            std::swap(key_positions[verts[i]], unode->orig_position[i]);
          }
        }
      }
      else {
        for (const int i : verts.index_range()) {
          std::swap(key_positions[verts[i]], unode->position[i]);
        }
      }
    }

    /* Propagate new coords to keyblock. */
    SCULPT_vertcos_to_key(object, ss.shapekey_active, key_positions);

    /* bke::pbvh::Tree uses its own vertex array, so coords should be */
    /* propagated to bke::pbvh::Tree here. */
    BKE_pbvh_vert_coords_apply(*ss.pbvh, key_positions);

    MEM_freeN(vertCos);
  }
  else {
    Mesh &mesh = *static_cast<Mesh *>(object.data);
    MutableSpan<float3> positions = mesh.vert_positions_for_write();

    for (const std::unique_ptr<Node> &unode : unodes) {
      const Span<int> verts = unode->vert_indices.as_span().take_front(unode->unique_verts_num);
      if (!unode->orig_position.is_empty()) {
        if (ss.deform_modifiers_active) {
          for (const int i : verts.index_range()) {
            restore_deformed(ss, *unode, i, verts[i], positions[verts[i]]);
            modified_verts[verts[i]] = true;
          }
        }
        else {
          for (const int i : verts.index_range()) {
            std::swap(positions[verts[i]], unode->orig_position[i]);
            modified_verts[verts[i]] = true;
          }
        }
      }
      else {
        for (const int i : verts.index_range()) {
          std::swap(positions[verts[i]], unode->position[i]);
          modified_verts[verts[i]] = true;
        }
      }
    }
  }
}

static void restore_position_grids(MutableSpan<CCGElem *> grids,
                                   const CCGKey &key,
                                   Node &unode,
                                   MutableSpan<bool> modified_grids)
{
  const Span<int> grid_indices = unode.grids;
  MutableSpan<float3> position = unode.position;

  int index = 0;
  for (const int i : grid_indices.index_range()) {
    CCGElem *grid = grids[grid_indices[i]];
    for (const int j : IndexRange(key.grid_area)) {
      std::swap(CCG_elem_offset_co(key, grid, j), position[index]);
      index++;
    }
  }

  modified_grids.fill_indices(grid_indices, true);
}

static void restore_vert_visibility_mesh(Object &object,
                                         Node &unode,
                                         MutableSpan<bool> modified_vertices)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
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

static void restore_vert_visibility_grids(SubdivCCG &subdiv_ccg,
                                          Node &unode,
                                          MutableSpan<bool> modified_grids)
{
  if (unode.grid_hidden.is_empty()) {
    BKE_subdiv_ccg_grid_hidden_free(subdiv_ccg);
    return;
  }

  BitGroupVector<> &grid_hidden = BKE_subdiv_ccg_grid_hidden_ensure(subdiv_ccg);
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

  modified_grids.fill_indices(grids, true);
}

static void restore_hidden_face(Object &object, Node &unode, MutableSpan<bool> modified_faces)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  bke::SpanAttributeWriter hide_poly = attributes.lookup_or_add_for_write_span<bool>(
      ".hide_poly", bke::AttrDomain::Face);

  const Span<int> face_indices = unode.face_indices;

  for (const int i : face_indices.index_range()) {
    const int face = face_indices[i];
    if (unode.face_hidden[i].test() != hide_poly.span[face]) {
      unode.face_hidden[i].set(!unode.face_hidden[i].test());
      hide_poly.span[face] = !hide_poly.span[face];
      modified_faces[face] = true;
    }
  }
  hide_poly.finish();
  BKE_sculpt_hide_poly_pointer_update(object);
}

static void restore_color(Object &object, StepData &step_data, MutableSpan<bool> modified_vertices)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  bke::GSpanAttributeWriter color_attribute = color::active_color_attribute_for_write(mesh);

  for (std::unique_ptr<Node> &unode : step_data.nodes) {
    if (color_attribute.domain == bke::AttrDomain::Point && !unode->col.is_empty()) {
      color::swap_gathered_colors(
          unode->vert_indices.as_span().take_front(unode->unique_verts_num),
          color_attribute.span,
          unode->col);
    }
    else if (color_attribute.domain == bke::AttrDomain::Corner && !unode->loop_col.is_empty()) {
      color::swap_gathered_colors(unode->corner_indices, color_attribute.span, unode->loop_col);
    }

    modified_vertices.fill_indices(unode->vert_indices.as_span(), true);
  }

  color_attribute.finish();
}

static void restore_mask_mesh(Object &object, Node &unode, MutableSpan<bool> modified_vertices)
{
  Mesh *mesh = BKE_object_get_original_mesh(&object);

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

static void restore_mask_grids(Object &object, Node &unode, MutableSpan<bool> modified_grids)
{
  SculptSession &ss = *object.sculpt;
  SubdivCCG *subdiv_ccg = ss.subdiv_ccg;

  const CCGKey key = BKE_subdiv_ccg_key_top_level(*subdiv_ccg);

  MutableSpan<float> mask = unode.mask;
  MutableSpan<CCGElem *> grids = subdiv_ccg->grids;

  int index = 0;
  for (const int grid : unode.grids) {
    CCGElem *elem = grids[grid];
    for (const int j : IndexRange(key.grid_area)) {
      std::swap(CCG_elem_offset_mask(key, elem, j), mask[index]);
      index++;
    }
  }

  modified_grids.fill_indices(unode.grids.as_span(), true);
}

static bool restore_face_sets(Object &object,
                              Node &unode,
                              MutableSpan<bool> modified_face_set_faces)
{
  const Span<int> face_indices = unode.face_indices;

  bke::SpanAttributeWriter<int> face_sets = face_set::ensure_face_sets_mesh(object);
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

static void bmesh_restore_generic(StepData &step_data, Object &object, SculptSession &ss)
{
  if (step_data.applied) {
    BM_log_undo(ss.bm, ss.bm_log);
    step_data.applied = false;
  }
  else {
    BM_log_redo(ss.bm, ss.bm_log);
    step_data.applied = true;
  }

  if (step_data.type == Type::Mask) {
    Vector<bke::pbvh::Node *> nodes = bke::pbvh::search_gather(*ss.pbvh, {});
    for (bke::pbvh::Node *node : nodes) {
      BKE_pbvh_node_mark_redraw(node);
    }
  }
  else {
    SCULPT_pbvh_clear(object);
  }
}

/* Create empty sculpt BMesh and enable logging. */
static void bmesh_enable(Object &object, StepData &step_data)
{
  SculptSession &ss = *object.sculpt;
  Mesh *mesh = static_cast<Mesh *>(object.data);

  SCULPT_pbvh_clear(object);

  /* Create empty BMesh and enable logging. */
  BMeshCreateParams bmesh_create_params{};
  bmesh_create_params.use_toolflags = false;

  ss.bm = BM_mesh_create(&bm_mesh_allocsize_default, &bmesh_create_params);
  BM_data_layer_add_named(ss.bm, &ss.bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");

  mesh->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;

  /* Restore the BMLog using saved entries. */
  ss.bm_log = BM_log_from_existing_entries_create(ss.bm, step_data.bm_entry);
}

static void bmesh_restore_begin(bContext *C,
                                StepData &step_data,
                                Object &object,
                                SculptSession &ss)
{
  if (step_data.applied) {
    dyntopo::disable(C, &step_data);
    step_data.applied = false;
  }
  else {
    bmesh_enable(object, step_data);

    /* Restore the mesh from the first log entry. */
    BM_log_redo(ss.bm, ss.bm_log);

    step_data.applied = true;
  }
}

static void bmesh_restore_end(bContext *C, StepData &step_data, Object &object, SculptSession &ss)
{
  if (step_data.applied) {
    bmesh_enable(object, step_data);

    /* Restore the mesh from the last log entry. */
    BM_log_undo(ss.bm, ss.bm_log);

    step_data.applied = false;
  }
  else {
    /* Disable dynamic topology sculpting. */
    dyntopo::disable(C, nullptr);
    step_data.applied = true;
  }
}

static void store_geometry_data(NodeGeometry *geometry, const Object &object)
{
  const Mesh *mesh = static_cast<const Mesh *>(object.data);

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

static void restore_geometry_data(const NodeGeometry *geometry, Mesh *mesh)
{
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

static void restore_geometry(StepData &step_data, Object &object)
{
  if (step_data.geometry_clear_pbvh) {
    SCULPT_pbvh_clear(object);
  }

  Mesh *mesh = static_cast<Mesh *>(object.data);

  if (step_data.applied) {
    restore_geometry_data(&step_data.geometry_modified, mesh);
    step_data.applied = false;
  }
  else {
    restore_geometry_data(&step_data.geometry_original, mesh);
    step_data.applied = true;
  }
}

/* Handle all dynamic-topology updates
 *
 * Returns true if this was a dynamic-topology undo step, otherwise
 * returns false to indicate the non-dyntopo code should run. */
static int bmesh_restore(
    bContext *C, Depsgraph &depsgraph, StepData &step_data, Object &object, SculptSession &ss)
{
  switch (step_data.type) {
    case Type::DyntopoBegin:
      BKE_sculpt_update_object_for_edit(&depsgraph, &object, false);
      bmesh_restore_begin(C, step_data, object, ss);
      return true;

    case Type::DyntopoEnd:
      BKE_sculpt_update_object_for_edit(&depsgraph, &object, false);
      bmesh_restore_end(C, step_data, object, ss);
      return true;
    default:
      if (ss.bm_log) {
        BKE_sculpt_update_object_for_edit(&depsgraph, &object, false);
        bmesh_restore_generic(step_data, object, ss);
        return true;
      }
      break;
  }

  return false;
}

void restore_from_bmesh_enter_geometry(const StepData &step_data, Mesh &mesh)
{
  restore_geometry_data(&step_data.geometry_bmesh_enter, &mesh);
}

BMLogEntry *get_bmesh_log_entry()
{
  return get_step_data()->bm_entry;
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
static void refine_subdiv(Depsgraph *depsgraph,
                          SculptSession &ss,
                          Object &object,
                          bke::subdiv::Subdiv *subdiv)
{
  Array<float3> deformed_verts = BKE_multires_create_deformed_base_mesh_vert_coords(
      depsgraph, &object, ss.multires.modifier);

  bke::subdiv::eval_refine_from_mesh(subdiv,
                                     static_cast<const Mesh *>(object.data),
                                     reinterpret_cast<float(*)[3]>(deformed_verts.data()));
}

static void restore_list(bContext *C, Depsgraph *depsgraph, StepData &step_data)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object &object = *BKE_view_layer_active_object_get(view_layer);
  if (step_data.object_name != object.id.name) {
    return;
  }
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  SculptSession &ss = *object.sculpt;

  /* Restore pivot. */
  ss.pivot_pos = step_data.pivot_pos;
  ss.pivot_rot = step_data.pivot_rot;

  const bool clear_automask_cache = !ELEM(step_data.type, Type::Color, Type::Mask);
  if (clear_automask_cache) {
    ss.last_automasking_settings_hash = 0;
  }

  if (bmesh_restore(C, *depsgraph, step_data, object, ss)) {
    return;
  }

  /* Switching to sculpt mode does not push a particular type.
   * See #124484. */
  if (step_data.type == Type::None && step_data.nodes.is_empty()) {
    return;
  }

  const bool tag_update = ID_REAL_USERS(object.data) > 1 ||
                          !BKE_sculptsession_use_pbvh_draw(&object, rv3d) || ss.shapekey_active ||
                          ss.deform_modifiers_active;

  switch (step_data.type) {
    case Type::None: {
      BLI_assert_unreachable();
      break;
    }
    case Type::Position: {
      BKE_sculpt_update_object_for_edit(depsgraph, &object, false);
      if (!topology_matches(step_data, object)) {
        return;
      }

      if (use_multires_undo(step_data, ss)) {
        SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
        MutableSpan<CCGElem *> grids = subdiv_ccg.grids;
        const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);

        Array<bool> modified_grids(grids.size(), false);
        for (std::unique_ptr<Node> &unode : step_data.nodes) {
          restore_position_grids(grids, key, *unode, modified_grids);
        }
        bke::pbvh::search_callback(*ss.pbvh, {}, [&](bke::pbvh::Node &node) {
          const Span<int> grids = bke::pbvh::node_grid_indices(node);
          if (indices_contain_true(modified_grids, grids)) {
            BKE_pbvh_node_mark_positions_update(&node);
          }
        });
        multires_mark_as_modified(depsgraph, &object, MULTIRES_COORDS_MODIFIED);
      }
      else {
        if (!restore_active_shape_key(*C, *depsgraph, step_data, object)) {
          return;
        }
        Array<bool> modified_verts(ss.totvert, false);
        restore_position_mesh(object, step_data.nodes, modified_verts);
        bke::pbvh::search_callback(*ss.pbvh, {}, [&](bke::pbvh::Node &node) {
          if (indices_contain_true(modified_verts, bke::pbvh::node_verts(node))) {
            BKE_pbvh_node_mark_positions_update(&node);
          }
        });
      }

      if (tag_update) {
        Mesh &mesh = *static_cast<Mesh *>(object.data);
        mesh.tag_positions_changed();
        BKE_sculptsession_free_deformMats(&ss);
      }
      bke::pbvh::update_bounds(*ss.pbvh);
      bke::pbvh::store_bounds_orig(*ss.pbvh);
      break;
    }
    case Type::HideVert: {
      BKE_sculpt_update_object_for_edit(depsgraph, &object, false);
      if (!topology_matches(step_data, object)) {
        return;
      }

      if (use_multires_undo(step_data, ss)) {
        SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
        Array<bool> modified_grids(subdiv_ccg.grids.size(), false);
        for (std::unique_ptr<Node> &unode : step_data.nodes) {
          restore_vert_visibility_grids(subdiv_ccg, *unode, modified_grids);
        }
        bke::pbvh::search_callback(*ss.pbvh, {}, [&](bke::pbvh::Node &node) {
          if (indices_contain_true(modified_grids, bke::pbvh::node_grid_indices(node))) {
            BKE_pbvh_node_mark_update_visibility(&node);
          }
        });
      }
      else {
        Array<bool> modified_verts(ss.totvert, false);
        for (std::unique_ptr<Node> &unode : step_data.nodes) {
          restore_vert_visibility_mesh(object, *unode, modified_verts);
        }
        bke::pbvh::search_callback(*ss.pbvh, {}, [&](bke::pbvh::Node &node) {
          if (indices_contain_true(modified_verts, bke::pbvh::node_verts(node))) {
            BKE_pbvh_node_mark_update_visibility(&node);
          }
        });
      }

      Mesh &mesh = *static_cast<Mesh *>(object.data);
      BKE_pbvh_sync_visibility_from_verts(*ss.pbvh, &mesh);
      bke::pbvh::update_visibility(*ss.pbvh);
      if (BKE_sculpt_multires_active(scene, &object)) {
        multires_mark_as_modified(depsgraph, &object, MULTIRES_HIDDEN_MODIFIED);
      }
      break;
    }
    case Type::HideFace: {
      BKE_sculpt_update_object_for_edit(depsgraph, &object, false);
      if (!topology_matches(step_data, object)) {
        return;
      }

      Array<bool> modified_faces(ss.totfaces, false);
      for (std::unique_ptr<Node> &unode : step_data.nodes) {
        restore_hidden_face(object, *unode, modified_faces);
      }

      if (use_multires_undo(step_data, ss)) {
        Vector<int> faces_vector;
        bke::pbvh::search_callback(*ss.pbvh, {}, [&](bke::pbvh::Node &node) {
          faces_vector.clear();
          const Span<int> faces = bke::pbvh::node_face_indices_calc_grids(
              *ss.pbvh, node, faces_vector);
          if (indices_contain_true(modified_faces, faces)) {
            BKE_pbvh_node_mark_update_visibility(&node);
          }
        });
      }
      else {
        const Span<int> tri_faces = mesh.corner_tri_faces();
        Vector<int> faces_vector;
        bke::pbvh::search_callback(*ss.pbvh, {}, [&](bke::pbvh::Node &node) {
          faces_vector.clear();
          const Span<int> faces = bke::pbvh::node_face_indices_calc_mesh(
              tri_faces, node, faces_vector);
          if (indices_contain_true(modified_faces, faces)) {
            BKE_pbvh_node_mark_update_visibility(&node);
          }
        });
      }

      hide::sync_all_from_faces(object);
      bke::pbvh::update_visibility(*ss.pbvh);
      break;
    }
    case Type::Mask: {
      BKE_sculpt_update_object_for_edit(depsgraph, &object, false);
      if (!topology_matches(step_data, object)) {
        return;
      }

      if (use_multires_undo(step_data, ss)) {
        Array<bool> modified_grids(ss.subdiv_ccg->grids.size(), false);
        for (std::unique_ptr<Node> &unode : step_data.nodes) {
          restore_mask_grids(object, *unode, modified_grids);
        }
        bke::pbvh::search_callback(*ss.pbvh, {}, [&](bke::pbvh::Node &node) {
          if (indices_contain_true(modified_grids, bke::pbvh::node_grid_indices(node))) {
            BKE_pbvh_node_mark_update_mask(&node);
          }
        });
      }
      else {
        Array<bool> modified_verts(ss.totvert, false);
        for (std::unique_ptr<Node> &unode : step_data.nodes) {
          restore_mask_mesh(object, *unode, modified_verts);
        }
        bke::pbvh::search_callback(*ss.pbvh, {}, [&](bke::pbvh::Node &node) {
          if (indices_contain_true(modified_verts, bke::pbvh::node_verts(node))) {
            BKE_pbvh_node_mark_update_mask(&node);
          }
        });
      }

      bke::pbvh::update_mask(*ss.pbvh);
      break;
    }
    case Type::FaceSet: {
      BKE_sculpt_update_object_for_edit(depsgraph, &object, false);
      if (!topology_matches(step_data, object)) {
        return;
      }

      Array<bool> modified_faces(ss.totfaces, false);
      for (std::unique_ptr<Node> &unode : step_data.nodes) {
        restore_face_sets(object, *unode, modified_faces);
      }
      if (use_multires_undo(step_data, ss)) {
        Vector<int> faces_vector;
        bke::pbvh::search_callback(*ss.pbvh, {}, [&](bke::pbvh::Node &node) {
          faces_vector.clear();
          const Span<int> faces = bke::pbvh::node_face_indices_calc_grids(
              *ss.pbvh, node, faces_vector);
          if (indices_contain_true(modified_faces, faces)) {
            BKE_pbvh_node_mark_update_face_sets(&node);
          }
        });
      }
      else {
        const Span<int> tri_faces = mesh.corner_tri_faces();
        Vector<int> faces_vector;
        bke::pbvh::search_callback(*ss.pbvh, {}, [&](bke::pbvh::Node &node) {
          faces_vector.clear();
          const Span<int> faces = bke::pbvh::node_face_indices_calc_mesh(
              tri_faces, node, faces_vector);
          if (indices_contain_true(modified_faces, faces)) {
            BKE_pbvh_node_mark_update_face_sets(&node);
          }
        });
      }
      break;
    }
    case Type::Color: {
      BKE_sculpt_update_object_for_edit(depsgraph, &object, false);
      if (!topology_matches(step_data, object)) {
        return;
      }

      Array<bool> modified_verts(ss.totvert, false);
      restore_color(object, step_data, modified_verts);
      bke::pbvh::search_callback(*ss.pbvh, {}, [&](bke::pbvh::Node &node) {
        if (indices_contain_true(modified_verts, bke::pbvh::node_verts(node))) {
          BKE_pbvh_node_mark_update_color(&node);
        }
      });
      break;
    }
    case Type::Geometry: {
      restore_geometry(step_data, object);
      BKE_sculpt_update_object_for_edit(depsgraph, &object, false);
      if (SubdivCCG *subdiv_ccg = ss.subdiv_ccg) {
        refine_subdiv(depsgraph, ss, object, subdiv_ccg->subdiv);
      }
      break;
    }
    case Type::DyntopoBegin:
    case Type::DyntopoEnd:
    case Type::DyntopoSymmetrize:
      /* Handled elsewhere. */
      BLI_assert_unreachable();
      break;
  }

  DEG_id_tag_update(&object.id, ID_RECALC_SHADING);
  if (tag_update) {
    DEG_id_tag_update(&object.id, ID_RECALC_GEOMETRY);
  }
}

static void free_step_data(StepData &step_data)
{
  geometry_free_data(&step_data.geometry_original);
  geometry_free_data(&step_data.geometry_modified);
  geometry_free_data(&step_data.geometry_bmesh_enter);
  if (step_data.bm_entry) {
    BM_log_entry_drop(step_data.bm_entry);
  }
  step_data.~StepData();
}

const Node *get_node(const bke::pbvh::Node *node, const Type type)
{
  StepData *step_data = get_step_data();
  if (!step_data) {
    return nullptr;
  }
  if (step_data->type != type) {
    return nullptr;
  }
  /* This access does not need to be locked because this function is not expected to be called
   * while the per-node undo data is being pushed. In other words, this must not be called
   * concurrently with #push_node.*/
  std::unique_ptr<Node> *node_ptr = step_data->undo_nodes_by_pbvh_node.lookup_ptr(node);
  if (!node_ptr) {
    return nullptr;
  }
  return node_ptr->get();
}

static void alloc_and_store_hidden(const SculptSession &ss,
                                   const bke::pbvh::Node &node,
                                   Node &unode)
{
  if (!ss.subdiv_ccg) {
    return;
  }
  const BitGroupVector<> grid_hidden = ss.subdiv_ccg->grid_hidden;
  if (grid_hidden.is_empty()) {
    return;
  }

  const Span<int> grid_indices = bke::pbvh::node_grid_indices(node);
  unode.grid_hidden = BitGroupVector<>(grid_indices.size(), grid_hidden.group_size());
  for (const int i : grid_indices.index_range()) {
    unode.grid_hidden[i].copy_from(grid_hidden[grid_indices[i]]);
  }
}

static void store_coords(const Object &object, Node &unode)
{
  SculptSession &ss = *object.sculpt;

  if (!unode.grids.is_empty()) {
    const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
    const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
    const Span<CCGElem *> grids = subdiv_ccg.grids;
    {
      int index = 0;
      for (const int grid : unode.grids) {
        CCGElem *elem = grids[grid];
        for (const int i : IndexRange(key.grid_area)) {
          unode.position[index] = CCG_elem_offset_co(key, elem, i);
          index++;
        }
      }
    }
    if (key.has_normals) {
      int index = 0;
      for (const int grid : unode.grids) {
        CCGElem *elem = grids[grid];
        for (const int i : IndexRange(key.grid_area)) {
          unode.normal[index] = CCG_elem_offset_no(key, elem, i);
          index++;
        }
      }
    }
  }
  else {
    array_utils::gather(BKE_pbvh_get_vert_positions(*ss.pbvh).as_span(),
                        unode.vert_indices.as_span(),
                        unode.position.as_mutable_span());
    array_utils::gather(BKE_pbvh_get_vert_normals(*ss.pbvh),
                        unode.vert_indices.as_span(),
                        unode.normal.as_mutable_span());
    if (ss.deform_modifiers_active) {
      const Mesh &mesh = *static_cast<const Mesh *>(object.data);
      const Span<float3> orig_positions = ss.shapekey_active ? Span(static_cast<const float3 *>(
                                                                        ss.shapekey_active->data),
                                                                    mesh.verts_num) :
                                                               mesh.vert_positions();
      array_utils::gather(
          orig_positions, unode.vert_indices.as_span(), unode.orig_position.as_mutable_span());
    }
  }
}

static void store_hidden(const Object &object, const bke::pbvh::Node &node, Node &unode)
{
  const Mesh &mesh = *static_cast<const Mesh *>(object.data);
  const bke::AttributeAccessor attributes = mesh.attributes();
  const VArraySpan<bool> hide_vert = *attributes.lookup<bool>(".hide_vert",
                                                              bke::AttrDomain::Point);
  if (hide_vert.is_empty()) {
    return;
  }

  const Span<int> verts = bke::pbvh::node_verts(node);
  for (const int i : verts.index_range()) {
    unode.vert_hidden[i].set(hide_vert[verts[i]]);
  }
}

static void store_face_hidden(const Object &object, Node &unode)
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

static void store_mask(const Object &object, Node &unode)
{
  const SculptSession &ss = *object.sculpt;

  if (!unode.grids.is_empty()) {
    const SubdivCCG &subdiv_ccg = *ss.subdiv_ccg;
    const CCGKey key = BKE_subdiv_ccg_key_top_level(subdiv_ccg);
    if (key.has_mask) {
      const Span<CCGElem *> grids = subdiv_ccg.grids;
      int index = 0;
      for (const int grid : unode.grids) {
        CCGElem *elem = grids[grid];
        for (const int i : IndexRange(key.grid_area)) {
          unode.mask[index] = CCG_elem_offset_mask(key, elem, i);
          index++;
        }
      }
    }
    else {
      unode.mask.fill(0.0f);
    }
  }
  else {
    const Mesh &mesh = *static_cast<const Mesh *>(object.data);
    const bke::AttributeAccessor attributes = mesh.attributes();
    if (const VArray mask = *attributes.lookup<float>(".sculpt_mask", bke::AttrDomain::Point)) {
      array_utils::gather(mask, unode.vert_indices.as_span(), unode.mask.as_mutable_span());
    }
    else {
      unode.mask.fill(0.0f);
    }
  }
}

static void store_color(const Object &object, const bke::pbvh::Node &node, Node &unode)
{
  const Mesh &mesh = *static_cast<const Mesh *>(object.data);

  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const bke::GAttributeReader color_attribute = color::active_color_attribute(mesh);
  const GVArraySpan colors(*color_attribute);

  /* NOTE: even with loop colors we still store (derived)
   * vertex colors for original data lookup. */
  const Span<int> verts = bke::pbvh::node_unique_verts(node);
  unode.col.reinitialize(verts.size());
  color::gather_colors_vert(
      faces, corner_verts, vert_to_face_map, colors, color_attribute.domain, verts, unode.col);

  if (color_attribute.domain == bke::AttrDomain::Corner) {
    unode.corner_indices = bke::pbvh::node_corners(node);
    unode.loop_col.reinitialize(unode.corner_indices.size());
    color::gather_colors(colors, unode.corner_indices, unode.loop_col);
  }
}

static NodeGeometry *geometry_get(StepData &step_data)
{
  if (!step_data.geometry_original.is_initialized) {
    return &step_data.geometry_original;
  }

  BLI_assert(!step_data.geometry_modified.is_initialized);

  return &step_data.geometry_modified;
}

static void geometry_push(const Object &object)
{
  StepData *step_data = get_step_data();

  step_data->type = Type::Geometry;

  step_data->applied = false;
  step_data->geometry_clear_pbvh = true;

  NodeGeometry *geometry = geometry_get(*step_data);
  store_geometry_data(geometry, object);
}

static void store_face_sets(const Mesh &mesh, Node &unode)
{
  array_utils::gather(
      *mesh.attributes().lookup_or_default<int>(".sculpt_face_set", bke::AttrDomain::Face, 1),
      unode.face_indices.as_span(),
      unode.face_sets.as_mutable_span());
}

static void fill_node_data(const Object &object,
                           const bke::pbvh::Node *node,
                           const Type type,
                           Node &unode)
{
  const SculptSession &ss = *object.sculpt;

  const Mesh &mesh = *static_cast<Mesh *>(object.data);

  int verts_num;
  if (ss.pbvh->type() == bke::pbvh::Type::Grids) {
    unode.grids = bke::pbvh::node_grid_indices(*node);

    const int grid_area = ss.subdiv_ccg->grid_size * ss.subdiv_ccg->grid_size;
    verts_num = unode.grids.size() * grid_area;
  }
  else {
    unode.vert_indices = bke::pbvh::node_verts(*node);
    unode.unique_verts_num = bke::pbvh::node_unique_verts(*node).size();

    verts_num = unode.vert_indices.size();
  }

  bool need_loops = type == Type::Color;
  const bool need_faces = ELEM(type, Type::FaceSet, Type::HideFace);

  if (need_loops) {
    unode.corner_indices = bke::pbvh::node_corners(*node);
  }

  if (need_faces) {
    if (ss.pbvh->type() == bke::pbvh::Type::Mesh) {
      bke::pbvh::node_face_indices_calc_mesh(mesh.corner_tri_faces(), *node, unode.face_indices);
    }
    else {
      bke::pbvh::node_face_indices_calc_grids(*ss.pbvh, *node, unode.face_indices);
    }
  }

  switch (type) {
    case Type::None:
      BLI_assert_unreachable();
      break;
    case Type::Position: {
      unode.position.reinitialize(verts_num);
      /* Needed for original data lookup. */
      unode.normal.reinitialize(verts_num);
      if (ss.deform_modifiers_active) {
        unode.orig_position.reinitialize(unode.vert_indices.size());
      }
      store_coords(object, unode);
      break;
    }
    case Type::HideVert: {
      if (ss.pbvh->type() == bke::pbvh::Type::Grids) {
        alloc_and_store_hidden(ss, *node, unode);
      }
      else {
        unode.vert_hidden.resize(unode.vert_indices.size());
        store_hidden(object, *node, unode);
      }
      break;
    }
    case Type::HideFace: {
      unode.face_hidden.resize(unode.face_indices.size());
      store_face_hidden(object, unode);
      break;
    }
    case Type::Mask: {
      unode.mask.reinitialize(verts_num);
      store_mask(object, unode);
      break;
    }
    case Type::Color: {
      store_color(object, *node, unode);
      break;
    }
    case Type::DyntopoBegin:
    case Type::DyntopoEnd:
    case Type::DyntopoSymmetrize:
      /* Dyntopo should be handled elsewhere. */
      BLI_assert_unreachable();
      break;
    case Type::Geometry:
      /* See #geometry_push. */
      BLI_assert_unreachable();
      break;
    case Type::FaceSet: {
      unode.face_sets.reinitialize(unode.face_indices.size());
      store_face_sets(*static_cast<const Mesh *>(object.data), unode);
      break;
    }
  }
}

/**
 * Dynamic topology stores only one undo node per stroke, regardless of the number of
 * bke::pbvh::Tree nodes modified.
 */
BLI_NOINLINE static void bmesh_push(const Object &object, const bke::pbvh::Node *node, Type type)
{
  StepData *step_data = get_step_data();
  const SculptSession &ss = *object.sculpt;

  std::scoped_lock lock(step_data->nodes_mutex);

  Node *unode = step_data->nodes.is_empty() ? nullptr : step_data->nodes.first().get();

  if (unode == nullptr) {
    step_data->nodes.append(std::make_unique<Node>());
    unode = step_data->nodes.last().get();

    step_data->type = type;
    step_data->applied = true;

    if (type == Type::DyntopoEnd) {
      step_data->bm_entry = BM_log_entry_add(ss.bm_log);
      BM_log_before_all_removed(ss.bm, ss.bm_log);
    }
    else if (type == Type::DyntopoBegin) {
      /* Store a copy of the mesh's current vertices, loops, and
       * faces. A full copy like this is needed because entering
       * dynamic-topology immediately does topological edits
       * (converting faces to triangles) that the BMLog can't
       * fully restore from. */
      NodeGeometry *geometry = &step_data->geometry_bmesh_enter;
      store_geometry_data(geometry, object);

      step_data->bm_entry = BM_log_entry_add(ss.bm_log);
      BM_log_all_added(ss.bm, ss.bm_log);
    }
    else {
      step_data->bm_entry = BM_log_entry_add(ss.bm_log);
    }
  }

  if (node) {
    const int cd_vert_mask_offset = CustomData_get_offset_named(
        &ss.bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");

    /* The vertices and node aren't changed, though pointers to them are stored in the log. */
    bke::pbvh::Node *node_mut = const_cast<bke::pbvh::Node *>(node);

    switch (type) {
      case Type::None:
        BLI_assert_unreachable();
        break;
      case Type::Position:
      case Type::Mask:
        /* Before any vertex values get modified, ensure their
         * original positions are logged. */
        for (BMVert *vert : BKE_pbvh_bmesh_node_unique_verts(node_mut)) {
          BM_log_vert_before_modified(ss.bm_log, vert, cd_vert_mask_offset);
        }
        for (BMVert *vert : BKE_pbvh_bmesh_node_other_verts(node_mut)) {
          BM_log_vert_before_modified(ss.bm_log, vert, cd_vert_mask_offset);
        }
        break;

      case Type::HideFace:
      case Type::HideVert: {
        for (BMVert *vert : BKE_pbvh_bmesh_node_unique_verts(node_mut)) {
          BM_log_vert_before_modified(ss.bm_log, vert, cd_vert_mask_offset);
        }
        for (BMVert *vert : BKE_pbvh_bmesh_node_other_verts(node_mut)) {
          BM_log_vert_before_modified(ss.bm_log, vert, cd_vert_mask_offset);
        }

        for (BMFace *f : BKE_pbvh_bmesh_node_faces(node_mut)) {
          BM_log_face_modified(ss.bm_log, f);
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
}

/**
 * Add an undo node for the bke::pbvh::Tree node to the step's storage. If the node was
 * newly created and needs to be filled with data, set \a r_new to true.
 */
static Node *ensure_node(StepData &step_data, const bke::pbvh::Node &node, bool &r_new)
{
  std::scoped_lock lock(step_data.nodes_mutex);
  r_new = false;
  std::unique_ptr<Node> &unode = step_data.undo_nodes_by_pbvh_node.lookup_or_add_cb(&node, [&]() {
    std::unique_ptr<Node> unode = std::make_unique<Node>();
    r_new = true;
    return unode;
  });
  return unode.get();
}

void push_node(const Object &object, const bke::pbvh::Node *node, Type type)
{
  SculptSession &ss = *object.sculpt;
  if (ss.bm || ELEM(type, Type::DyntopoBegin, Type::DyntopoEnd)) {
    bmesh_push(object, node, type);
    return;
  }

  StepData *step_data = get_step_data();
  BLI_assert(ELEM(step_data->type, Type::None, type));
  step_data->type = type;

  bool newly_added;
  Node *unode = ensure_node(*step_data, *node, newly_added);
  if (!newly_added) {
    /* The node was already filled with data for this undo step. */
    return;
  }

  ss.needs_flush_to_id = 1;

  fill_node_data(object, node, type, *unode);
}

void push_nodes(Object &object, const Span<const bke::pbvh::Node *> nodes, const Type type)
{
  SculptSession &ss = *object.sculpt;
  if (ss.bm || ELEM(type, Type::DyntopoBegin, Type::DyntopoEnd)) {
    for (const bke::pbvh::Node *node : nodes) {
      bmesh_push(object, node, type);
    }
    return;
  }

  StepData *step_data = get_step_data();
  BLI_assert(ELEM(step_data->type, Type::None, type));
  step_data->type = type;

  Vector<std::pair<const bke::pbvh::Node *, Node *>, 32> nodes_to_fill;
  for (const bke::pbvh::Node *node : nodes) {
    bool newly_added;
    Node *unode = ensure_node(*step_data, *node, newly_added);
    if (newly_added) {
      nodes_to_fill.append({node, unode});
    }
  }

  ss.needs_flush_to_id = 1;

  threading::parallel_for(nodes_to_fill.index_range(), 1, [&](const IndexRange range) {
    for (const auto &[node, unode] : nodes_to_fill.as_span().slice(range)) {
      fill_node_data(object, node, type, *unode);
    }
  });
}

static void save_active_attribute(Object &object, SculptAttrRef *attr)
{
  Mesh *mesh = BKE_object_get_original_mesh(&object);
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

void push_begin(Object &ob, const wmOperator *op)
{
  push_begin_ex(ob, op->type->name);
}

void push_begin_ex(Object &ob, const char *name)
{
  UndoStack *ustack = ED_undo_stack_get();

  /* If possible, we need to tag the object and its geometry data as 'changed in the future' in
   * the previous undo step if it's a memfile one. */
  ED_undosys_stack_memfile_id_changed_tag(ustack, &ob.id);
  ED_undosys_stack_memfile_id_changed_tag(ustack, static_cast<ID *>(ob.data));

  /* Special case, we never read from this. */
  bContext *C = nullptr;

  SculptUndoStep *us = (SculptUndoStep *)BKE_undosys_step_push_init_with_type(
      ustack, C, name, BKE_UNDOSYS_TYPE_SCULPT);
  us->data.object_name = ob.id.name;

  if (!us->active_color_start.was_set) {
    save_active_attribute(ob, &us->active_color_start);
  }

  /* Set end attribute in case push_end is not called,
   * so we don't end up with corrupted state.
   */
  if (!us->active_color_end.was_set) {
    save_active_attribute(ob, &us->active_color_end);
    us->active_color_end.was_set = false;
  }

  const SculptSession &ss = *ob.sculpt;

  switch (ss.pbvh->type()) {
    case bke::pbvh::Type::Mesh: {
      const Mesh &mesh = *static_cast<const Mesh *>(ob.data);
      us->data.mesh_verts_num = ss.totvert;
      us->data.mesh_corners_num = mesh.corners_num;
      break;
    }
    case bke::pbvh::Type::Grids: {
      us->data.mesh_grids_num = ss.subdiv_ccg->grids.size();
      us->data.grid_size = ss.subdiv_ccg->grid_size;
      break;
    }
    case bke::pbvh::Type::BMesh: {
      break;
    }
  }

  /* Store sculpt pivot. */
  us->data.pivot_pos = ss.pivot_pos;
  us->data.pivot_rot = ss.pivot_rot;

  if (const KeyBlock *key = BKE_keyblock_from_object(&ob)) {
    us->data.active_shape_key_name = key->name;
  }
}

void push_end(Object &ob)
{
  push_end_ex(ob, false);
}

static size_t node_size_in_bytes(const Node &node)
{
  size_t size = sizeof(Node);
  size += node.position.as_span().size_in_bytes();
  size += node.orig_position.as_span().size_in_bytes();
  size += node.normal.as_span().size_in_bytes();
  size += node.col.as_span().size_in_bytes();
  size += node.mask.as_span().size_in_bytes();
  size += node.loop_col.as_span().size_in_bytes();
  size += node.orig_loop_col.as_span().size_in_bytes();
  size += node.vert_indices.as_span().size_in_bytes();
  size += node.corner_indices.as_span().size_in_bytes();
  size += node.vert_hidden.size() / 8;
  size += node.face_hidden.size() / 8;
  size += node.grids.as_span().size_in_bytes();
  size += node.grid_hidden.all_bits().size() / 8;
  size += node.face_sets.as_span().size_in_bytes();
  size += node.face_indices.as_span().size_in_bytes();
  return size;
}

void push_end_ex(Object &ob, const bool use_nested_undo)
{
  StepData *step_data = get_step_data();

  /* Move undo node storage from map to vector. */
  step_data->nodes.reserve(step_data->undo_nodes_by_pbvh_node.size());
  for (std::unique_ptr<Node> &node : step_data->undo_nodes_by_pbvh_node.values()) {
    step_data->nodes.append(std::move(node));
  }
  step_data->undo_nodes_by_pbvh_node.clear_and_shrink();

  /* We don't need normals in the undo stack. */
  for (std::unique_ptr<Node> &unode : step_data->nodes) {
    unode->normal = {};
  }

  step_data->undo_size = threading::parallel_reduce(
      step_data->nodes.index_range(),
      16,
      0,
      [&](const IndexRange range, size_t size) {
        for (const int i : range) {
          size += node_size_in_bytes(*step_data->nodes[i]);
        }
        return size;
      },
      std::plus<size_t>());

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

  save_active_attribute(ob, &us->active_color_end);
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
  save_active_attribute(*ob, &existing);

  AttributeOwner owner = AttributeOwner::from_id(&mesh->id);
  CustomDataLayer *layer = BKE_attribute_find(owner, attr->name, attr->type, attr->domain);

  /* Temporary fix for #97408. This is a fundamental
   * bug in the undo stack; the operator code needs to push
   * an extra undo step before running an operator if a
   * non-memfile undo system is active.
   *
   * For now, detect if the layer does exist but with a different
   * domain and just unconvert it.
   */
  if (!layer) {
    layer = BKE_attribute_search_for_write(
        owner, attr->name, CD_MASK_PROP_ALL, ATTR_DOMAIN_MASK_ALL);
    if (layer) {
      if (ED_geometry_attribute_convert(
              mesh, attr->name, eCustomDataType(attr->type), attr->domain, nullptr))
      {
        layer = BKE_attribute_find(owner, attr->name, attr->type, attr->domain);
      }
    }
  }

  if (!layer) {
    /* Memfile undo killed the layer; re-create it. */
    mesh->attributes_for_write().add(
        attr->name, attr->domain, attr->type, bke::AttributeInitDefaultValue());
    layer = BKE_attribute_find(owner, attr->name, attr->type, attr->domain);
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }

  if (layer) {
    BKE_id_attributes_active_color_set(&mesh->id, layer->name);
  }
}

static void step_encode_init(bContext * /*C*/, UndoStep *us_p)
{
  SculptUndoStep *us = (SculptUndoStep *)us_p;
  new (&us->data) StepData();
}

static bool step_encode(bContext * /*C*/, Main *bmain, UndoStep *us_p)
{
  /* Dummy, encoding is done along the way by adding tiles
   * to the current 'SculptUndoStep' added by encode_init. */
  SculptUndoStep *us = (SculptUndoStep *)us_p;
  us->step.data_size = us->data.undo_size;

  Node *unode = us->data.nodes.is_empty() ? nullptr : us->data.nodes.last().get();
  if (unode && us->data.type == Type::DyntopoEnd) {
    us->step.use_memfile_step = true;
  }
  us->step.is_applied = true;

  if (!us->data.nodes.is_empty()) {
    bmain->is_memfile_undo_flush_needed = true;
  }

  return true;
}

static void step_decode_undo_impl(bContext *C, Depsgraph *depsgraph, SculptUndoStep *us)
{
  BLI_assert(us->step.is_applied == true);

  restore_list(C, depsgraph, us->data);
  us->step.is_applied = false;

  print_nodes(*CTX_data_active_object(C), nullptr);
}

static void step_decode_redo_impl(bContext *C, Depsgraph *depsgraph, SculptUndoStep *us)
{
  BLI_assert(us->step.is_applied == false);

  restore_list(C, depsgraph, us->data);
  us->step.is_applied = true;

  print_nodes(*CTX_data_active_object(C), nullptr);
}

static void step_decode_undo(bContext *C,
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
    step_decode_undo_impl(C, depsgraph, us_iter);

    if (us_iter == us) {
      if (us_iter->step.prev && us_iter->step.prev->type == BKE_UNDOSYS_TYPE_SCULPT) {
        set_active_layer(C, &((SculptUndoStep *)us_iter->step.prev)->active_color_end);
      }
      break;
    }

    us_iter = (SculptUndoStep *)us_iter->step.prev;
  }
}

static void step_decode_redo(bContext *C, Depsgraph *depsgraph, SculptUndoStep *us)
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
    step_decode_redo_impl(C, depsgraph, us_iter);

    if (us_iter == us) {
      set_active_layer(C, &((SculptUndoStep *)us_iter)->active_color_start);
      break;
    }
    us_iter = (SculptUndoStep *)us_iter->step.next;
  }
}

static void step_decode(
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
        object::mode_generic_exit(bmain, depsgraph, scene, ob);

        /* Sculpt needs evaluated state.
         * NOTE: needs to be done here, as #object::mode_generic_exit will usually invalidate
         * (some) evaluated data. */
        BKE_scene_graph_evaluated_ensure(depsgraph, bmain);

        Mesh *mesh = static_cast<Mesh *>(ob->data);
        /* Don't add sculpt topology undo steps when reading back undo state.
         * The undo steps must enter/exit for us. */
        mesh->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;
        object_sculpt_mode_enter(*bmain, *depsgraph, *scene, *ob, true, nullptr);
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
    step_decode_undo(C, depsgraph, us, is_final);
  }
  else if (dir == STEP_REDO) {
    step_decode_redo(C, depsgraph, us);
  }
}

static void step_free(UndoStep *us_p)
{
  SculptUndoStep *us = (SculptUndoStep *)us_p;
  free_step_data(us->data);
}

void geometry_begin(Object &ob, const wmOperator *op)
{
  push_begin(ob, op);
  geometry_push(ob);
}

void geometry_begin_ex(Object &ob, const char *name)
{
  push_begin_ex(ob, name);
  geometry_push(ob);
}

void geometry_end(Object &ob)
{
  geometry_push(ob);
  push_end(ob);
}

void register_type(UndoType *ut)
{
  ut->name = "Sculpt";
  ut->poll = nullptr; /* No poll from context for now. */
  ut->step_encode_init = step_encode_init;
  ut->step_encode = step_encode;
  ut->step_decode = step_decode;
  ut->step_free = step_free;

  ut->flags = UNDOTYPE_FLAG_DECODE_ACTIVE_STEP;

  ut->step_size = sizeof(SculptUndoStep);
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
  SculptSession &ss = *object->sculpt;

  /* It is possible that undo push is done from an object state where there is no tree. This
   * happens, for example, when an operation which tagged for geometry update was performed prior
   * to the current operation without making any stroke in between.
   *
   * Skip pushing nodes based on the following logic: on redo Type::Position will
   * ensure pbvh::Tree for the new base geometry, which will have same coordinates as if we create
   * pbvh::Tree here.
   */
  if (ss.pbvh == nullptr) {
    return;
  }

  Vector<bke::pbvh::Node *> nodes = bke::pbvh::search_gather(*ss.pbvh, {});
  for (bke::pbvh::Node *node : nodes) {
    push_node(*object, node, Type::Position);
  }
}

void push_multires_mesh_begin(bContext *C, const char *str)
{
  if (!use_multires_mesh(C)) {
    return;
  }

  Object *object = CTX_data_active_object(C);

  push_begin_ex(*object, str);

  geometry_push(*object);
  get_step_data()->geometry_clear_pbvh = false;

  push_all_grids(object);
}

void push_multires_mesh_end(bContext *C, const char *str)
{
  if (!use_multires_mesh(C)) {
    ED_undo_push(C, str);
    return;
  }

  Object *object = CTX_data_active_object(C);

  geometry_push(*object);
  get_step_data()->geometry_clear_pbvh = false;

  push_end(*object);
}

/** \} */

}  // namespace blender::ed::sculpt_paint::undo

namespace blender::ed::sculpt_paint {

OrigPositionData orig_position_data_get_mesh(const Object & /*object*/,
                                             const bke::pbvh::Node &node)
{
  const undo::Node *unode = undo::get_node(&node, undo::Type::Position);
  return {unode->position.as_span().take_front(unode->unique_verts_num),
          unode->normal.as_span().take_front(unode->unique_verts_num)};
}

OrigPositionData orig_position_data_get_grids(const Object & /*object*/,
                                              const bke::pbvh::Node &node)
{
  const undo::Node *unode = undo::get_node(&node, undo::Type::Position);
  return {unode->position.as_span(), unode->normal.as_span()};
}

void orig_position_data_gather_bmesh(const BMLog &bm_log,
                                     const Set<BMVert *, 0> &verts,
                                     const MutableSpan<float3> positions,
                                     const MutableSpan<float3> normals)
{
  int i = 0;
  for (const BMVert *vert : verts) {
    const float *co;
    const float *no;
    BM_log_original_vert_data(&const_cast<BMLog &>(bm_log), const_cast<BMVert *>(vert), &co, &no);
    if (!positions.is_empty()) {
      positions[i] = co;
    }
    if (!normals.is_empty()) {
      normals[i] = no;
    }
    i++;
  }
}

Span<float4> orig_color_data_get_mesh(const Object & /*object*/, const bke::pbvh::Node &node)
{
  const undo::Node *unode = undo::get_node(&node, undo::Type::Color);
  return unode->col.as_span();
}

}  // namespace blender::ed::sculpt_paint
