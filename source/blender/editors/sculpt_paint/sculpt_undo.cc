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
 * inside an operator exec or invoke callback (ED_Type::Geometry_begin
 * may be called if you wish to save a non-delta copy of the entire mesh).
 * This will initialize the sculpt undo stack and set up an undo step.
 *
 * At the end of the operator you should call push_end.
 *
 * push_end and ED_Type::Geometry_begin both take a
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
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_array_utils.hh"
#include "BLI_listbase.h"
#include "BLI_string.h"
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
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_global.hh"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.hh"
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
#include "bmesh_idmap.hh"
#include "bmesh_log.hh"
#include "paint_intern.hh"
#include "sculpt_intern.hh"

using blender::bke::dyntopo::DyntopoSet;

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

#define NO_ACTIVE_LAYER AttrDomain::Auto

struct UndoSculpt {
  ListBase nodes;

  size_t undo_size;
  BMLog *bm_restore;
};

struct SculptAttrRef {
  AttrDomain domain;
  eCustomDataType type;
  char name[MAX_CUSTOMDATA_LAYER_NAME];
  bool was_set;
};

struct SculptUndoStep {
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
};

static UndoSculpt *sculpt_undo_get_nodes(void);
static bool sculpt_attribute_ref_equals(SculptAttrRef *a, SculptAttrRef *b);
static void sculpt_save_active_attribute(Object *ob, SculptAttrRef *attr);
static UndoSculpt *sculpt_undosys_step_get_nodes(UndoStep *us_p);

static void update_unode_bmesh_memsize(Node *unode);
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
    _(Type::DyntopoBegin)
    _(Type::DyntopoEnd)
    _(Type::Position)
    _(Type::Geometry)
    _(Type::DyntopoSymmetrize)
    _(Type::FaceSet)
    _(SCULPT_UNDO_HIDDEN)
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

static void print_sculpt_undo_step(Object *ob, UndoStep *us, UndoStep *active, int i)
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

static UndoSculpt *sculpt_undosys_step_get_nodes(UndoStep *us_p);

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

static int *sculpt_undo_get_indices32(Node *unode, int allvert)
{
  int *indices = (int *)MEM_malloc_arrayN(allvert, sizeof(int), __func__);

  for (int i = 0; i < allvert; i++) {
    indices[i] = (int)unode->vert_indices[i];
  }

  return indices;
}

typedef struct BmeshUndoData {
  Object *ob;
  PBVH *pbvh;
  BMesh *bm;
  bool do_full_recalc;
  bool balance_pbvh;
  int cd_face_node_offset, cd_vert_node_offset;
  int cd_face_node_offset_old, cd_vert_node_offset_old;
  int cd_boundary_flag, cd_flags, cd_edge_boundary;
  bool regen_all_unique_verts;
  bool is_redo;
} BmeshUndoData;

static void bmesh_undo_vert_update(BmeshUndoData *data, BMVert *v, bool triangulate = false)
{
  *BM_ELEM_CD_PTR<uint8_t *>(v, data->cd_flags) |= SCULPTFLAG_NEED_VALENCE;
  if (triangulate) {
    *BM_ELEM_CD_PTR<uint8_t *>(v, data->cd_flags) |= SCULPTFLAG_NEED_TRIANGULATE;
  }

  BKE_sculpt_boundary_flag_update(data->ob->sculpt, {reinterpret_cast<intptr_t>(v)});
}

static void bmesh_undo_on_vert_kill(BMVert *v, void *userdata)
{
  BmeshUndoData *data = (BmeshUndoData *)userdata;
  int ni = BM_ELEM_CD_GET_INT(v, data->cd_vert_node_offset);
  // data->do_full_recalc = true;

  bool bad = ni == -1 || !BKE_pbvh_get_node_leaf_safe(data->pbvh, ni);

  if (bad) {
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

  bmesh_undo_vert_update(data, v, true);

  /* Flag vert as unassigned to a PBVH node; it'll be added to pbvh when
   * its owning faces are.
   */
  BM_ELEM_CD_SET_INT(v, data->cd_vert_node_offset, -1);
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
    bmesh_undo_vert_update(data, l->v, true);
  } while ((l = l->next) != f->l_first);

  // data->do_full_recalc = true;
  data->balance_pbvh = true;
}

static void bmesh_undo_on_face_add(BMFace *f, void *userdata)
{
  BmeshUndoData *data = (BmeshUndoData *)userdata;

  BM_ELEM_CD_SET_INT(f, data->cd_face_node_offset, -1);
  BKE_pbvh_bmesh_add_face(data->pbvh, f, false, true);

  int ni = BM_ELEM_CD_GET_INT(f, data->cd_face_node_offset);
  PBVHNode *node = BKE_pbvh_get_node(data->pbvh, ni);

  BMLoop *l = f->l_first;
  do {
    bmesh_undo_vert_update(data, l->v, f->len > 3);
    *BM_ELEM_CD_PTR<int *>(l->e, data->cd_edge_boundary) |= SCULPT_BOUNDARY_NEEDS_UPDATE |
                                                            SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE;

    int ni_l = BM_ELEM_CD_GET_INT(l->v, data->cd_vert_node_offset);
    if (ni_l < 0 && ni >= 0) {
      BM_ELEM_CD_SET_INT(l->v, ni_l, ni);
      DyntopoSet<BMVert> &bm_unique_verts = BKE_pbvh_bmesh_node_unique_verts(node);

      bm_unique_verts.add(l->v);
    }
  } while ((l = l->next) != f->l_first);

  data->balance_pbvh = true;
}

static void bmesh_undo_full_mesh(void *userdata)
{
  BmeshUndoData *data = (BmeshUndoData *)userdata;

  BKE_sculptsession_update_attr_refs(data->ob);

  if (data->pbvh) {
    BMIter iter;
    BMVert *v;

    BM_ITER_MESH (v, &iter, data->bm, BM_VERTS_OF_MESH) {
      bmesh_undo_vert_update(data, v, true);
    }

    data->pbvh = nullptr;
  }

  /* Recalculate face normals to prevent tessellation errors.*/
  BM_mesh_normals_update(data->bm);
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

  bmesh_undo_vert_update(data, e->v1, true);
  bmesh_undo_vert_update(data, e->v2, true);
};
;
static void bmesh_undo_on_edge_add(BMEdge *e, void *userdata)
{
  BmeshUndoData *data = (BmeshUndoData *)userdata;

  *BM_ELEM_CD_PTR<int *>(e, data->cd_edge_boundary) |= SCULPT_BOUNDARY_NEEDS_UPDATE |
                                                       SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE;

  bmesh_undo_vert_update(data, e->v1, true);
  bmesh_undo_vert_update(data, e->v2, true);
}

static void bmesh_undo_on_vert_change(BMVert *v, void *userdata, void *old_customdata)
{
  BmeshUndoData *data = (BmeshUndoData *)userdata;

  bmesh_undo_vert_update(data, v, true);

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
    *BM_ELEM_CD_PTR<int *>(l->e, data->cd_edge_boundary) |= SCULPT_BOUNDARY_NEEDS_UPDATE |
                                                            SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE;
    *BM_ELEM_CD_PTR<int *>(l->v, data->cd_boundary_flag) |= SCULPT_BOUNDARY_NEEDS_UPDATE |
                                                            SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE;
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

static void update_unode_bmesh_memsize(Node *unode)
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
  // printf("unode->unode_size: size: %.4fmb\n", __func__, float(unode->undo_size) / 1024.0f /
  // 1024.0f);

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

static void sculpt_undo_bmesh_restore_generic(Node *unode, Object *ob, SculptSession *ss)
{
  BmeshUndoData data = {};
  data.ob = ob;
  data.bm = ss->bm;
  data.pbvh = ss->pbvh;
  data.cd_face_node_offset = ss->cd_face_node_offset;
  data.cd_vert_node_offset = ss->cd_vert_node_offset;
  data.cd_face_node_offset_old = data.cd_vert_node_offset_old = -1;
  data.cd_boundary_flag = ss->attrs.boundary_flags->bmesh_cd_offset;
  data.cd_edge_boundary = ss->attrs.edge_boundary_flags->bmesh_cd_offset;
  data.cd_flags = ss->attrs.flags->bmesh_cd_offset;
  data.is_redo = !unode->applied;

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
  BKE_sculpt_ensure_idmap(ob);

  if (unode->applied) {
    BM_log_undo(ss->bm, ss->bm_log, &callbacks);
    unode->applied = false;
  }
  else {
    BM_log_redo(ss->bm, ss->bm_log, &callbacks);
    unode->applied = true;
  }

  update_unode_bmesh_memsize(unode);

  if (!data.do_full_recalc) {
    BKE_pbvh_bmesh_check_nodes(ss->pbvh);

    Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, {});

    if (data.regen_all_unique_verts) {
      for (PBVHNode *node : nodes) {
        BKE_pbvh_bmesh_mark_node_regen(ss->pbvh, node);
      }
    }

    BKE_pbvh_bmesh_regen_node_verts(ss->pbvh, false);
    bke::pbvh::update_bounds(*ss->pbvh, PBVH_UpdateBB | PBVH_UpdateOriginalBB | PBVH_UpdateRedraw);

    if (data.balance_pbvh) {
      blender::bke::dyntopo::after_stroke(ss->pbvh, true);
    }
  }
  else {
    printf("undo triggered pbvh rebuild");
    SCULPT_pbvh_clear(ob);
  }
}

static void sculpt_unode_bmlog_ensure(SculptSession *ss, Node *unode)
{
  if (!ss->bm_log && ss->bm && unode->bm_entry) {
    ss->bm_log = BM_log_from_existing_entries_create(ss->bm, ss->bm_idmap, unode->bm_entry);

    if (!unode->applied) {
      BM_log_undo_skip(ss->bm, ss->bm_log);
    }

    if (ss->pbvh) {
      BKE_pbvh_set_bm_log(ss->pbvh, ss->bm_log);
    }
  }
}

/* Create empty sculpt BMesh and enable logging. */
static void sculpt_undo_bmesh_enable(Object *ob, Node *unode, bool /*is_redo*/)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = static_cast<Mesh *>(ob->data);

  SCULPT_pbvh_clear(ob);

  if (ss->bm_idmap) {
    BM_idmap_destroy(ss->bm_idmap);
    ss->bm_idmap = nullptr;
  }

  ss->active_face.i = ss->active_vertex.i = PBVH_REF_NONE;

  /* Create empty BMesh and enable logging. */
  ss->bm = SCULPT_dyntopo_empty_bmesh();

  BMeshFromMeshParams params = {0};
  params.use_shapekey = true;
  params.active_shapekey = ob->shapenr;

  BM_mesh_bm_from_me(ss->bm, me, &params);

  /* Calculate normals. */
  BM_mesh_normals_update(ss->bm);

  /* Ensure mask. */
  if (!CustomData_has_layer_named(&ss->bm->vdata, CD_PROP_FLOAT, ".sculpt_mask")) {
    BM_data_layer_add_named(ss->bm, &ss->bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");
  }

  BKE_sculptsession_update_attr_refs(ob);

  if (ss->pbvh) {
    blender::bke::paint::load_all_original(ob);
  }

  BKE_sculpt_ensure_idmap(ob);

  if (!ss->bm_log) {
    sculpt_unode_bmlog_ensure(ss, unode);
  }
  else {
    BM_log_set_idmap(ss->bm_log, ss->bm_idmap);
  }

  SCULPT_update_all_valence_boundary(ob);

  me->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;
}

static void sculpt_undo_bmesh_restore_begin(
    bContext *C, Node *unode, Object *ob, SculptSession *ss, int dir)
{
  if (unode->applied) {
    if (ss->bm && ss->bm_log) {
#if 0
    if (dir == 1) {
      BM_log_redo_skip(ss->bm, ss->bm_log);
    }
    else {
      BM_log_undo_skip(ss->bm, ss->bm_log);
    }
#else
      if (dir == 1) {
        BM_log_redo(ss->bm, ss->bm_log, nullptr);
      }
      else {
        BM_log_undo(ss->bm, ss->bm_log, nullptr);
      }
#endif
    }

    BKE_pbvh_bmesh_check_nodes(ss->pbvh);
    dyntopo::disable(C, unode);
    unode->applied = false;
  }
  else {
    /* Load bmesh from mesh data. */
    sculpt_undo_bmesh_enable(ob, unode, true);

    if (dir == 1) {
      BM_log_redo(ss->bm, ss->bm_log, nullptr);
    }
    else {
      BM_log_undo(ss->bm, ss->bm_log, nullptr);
    }

    unode->applied = true;
  }

  if (ss->bm) {
    BM_mesh_elem_index_ensure(ss->bm, BM_VERT | BM_FACE);
  }
}

static void sculpt_undo_bmesh_restore_end(
    bContext *C, Node *unode, Object *ob, SculptSession *ss, int dir)
{

  if (unode->applied) {
    /*load bmesh from mesh data*/
    sculpt_undo_bmesh_enable(ob, unode, false);

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

    unode->applied = false;
  }
  else {
    if (ss->bm && ss->bm_log) {

      if (dir == -1) {
        BM_log_undo_skip(ss->bm, ss->bm_log);
      }
      else {
        BM_log_redo_skip(ss->bm, ss->bm_log);
      }
    }

    /* Disable dynamic topology sculpting. */
    dyntopo::disable(C, nullptr);
    unode->applied = true;
  }

  if (ss->bm) {
    BM_mesh_elem_index_ensure(ss->bm, BM_VERT | BM_FACE);
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
static int sculpt_undo_bmesh_restore(
    bContext *C, Node *unode, Object *ob, SculptSession *ss, int dir)
{
  // handle transition from another undo type

  ss->needs_flush_to_id = 1;
  sculpt_unode_bmlog_ensure(ss, unode);

  bool ret = false;

  switch (unode->type) {
    case Type::DyntopoBegin:
      sculpt_undo_bmesh_restore_begin(C, unode, ob, ss, dir);
      SCULPT_vertex_random_access_ensure(ss);

      ss->active_face.i = ss->active_vertex.i = PBVH_REF_NONE;

      ret = true;
      break;
    case Type::DyntopoEnd:
      ss->active_face.i = ss->active_vertex.i = PBVH_REF_NONE;

      sculpt_undo_bmesh_restore_end(C, unode, ob, ss, dir);
      SCULPT_vertex_random_access_ensure(ss);

      ret = true;
      break;
    default:
      if (ss->bm_log) {
        sculpt_undo_bmesh_restore_generic(unode, ob, ss);
        SCULPT_vertex_random_access_ensure(ss);

        if (unode->type == Type::HideVert || unode->type == Type::HideFace) {
          bke::pbvh::update_vertex_data(*ss->pbvh, PBVH_UpdateVisibility);
        }
        ret = true;
      }
      break;
  }

  if (ss->pbvh && BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    BKE_pbvh_flush_tri_areas(ob, ss->pbvh);
  }

  ss->active_face.i = ss->active_vertex.i = PBVH_REF_NONE;

  /* Load attribute layout from bmesh to ob. */
  BKE_sculptsession_sync_attributes(ob, static_cast<Mesh *>(ob->data), true);

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
                                      Subdiv *subdiv)
{
  Array<float3> deformed_verts = BKE_multires_create_deformed_base_mesh_vert_coords(
      depsgraph, object, ss->multires.modifier);

  BKE_subdiv_eval_refine_from_mesh(subdiv,
                                   static_cast<const Mesh *>(object->data),
                                   reinterpret_cast<float(*)[3]>(deformed_verts.data()));
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
  bool need_mask = false;
  //  bool did_first_hack = false;

  bool clear_automask_cache = false;

  LISTBASE_FOREACH (Node *, unode, lb) {
    if (!ELEM(unode->type, Type::Color, Type::Mask)) {
      clear_automask_cache = true;
    }

    /* Restore pivot. */
    copy_v3_v3(ss->pivot_pos, unode->pivot_pos);
    copy_v3_v3(ss->pivot_rot, unode->pivot_rot);
    if (STREQ(unode->idname, ob->id.name)) {
      if (unode->type == Type::Mask) {
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
    const Node *first_unode = (const Node *)lb->first;
    if (!ELEM(first_unode->type, Type::Geometry, Type::DyntopoBegin, Type::DyntopoSymmetrize)) {
      BKE_sculpt_update_object_for_edit(depsgraph, ob, false);
    }

    if (sculpt_undo_bmesh_restore(C, (Node *)lb->first, ob, ss, dir)) {
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

  LISTBASE_FOREACH (Node *, unode, lb) {
    if (!STREQ(unode->idname, ob->id.name)) {
      continue;
    }

    /* Check if undo data matches current data well enough to
     * continue. */
    if (unode->mesh_verts_num) {
      if (ss->totvert != unode->mesh_verts_num) {
        printf("error! %s\n", __func__);
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

    LISTBASE_FOREACH (Node *, unode, lb) {
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
        case Type::None:
          BLI_assert_unreachable();
          break;
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
  }

  if (use_multires_undo) {
    LISTBASE_FOREACH (Node *, unode, lb) {
      if (!STREQ(unode->idname, ob->id.name)) {
        continue;
      }
      modified_grids.resize(unode->maxgrid, false);
      modified_grids.as_mutable_span().fill_indices(unode->grids.as_span(), true);
    }
  }

  if (subdiv_ccg != nullptr && changed_all_geometry) {
    sculpt_undo_refine_subdiv(depsgraph, ss, ob, subdiv_ccg->subdiv);
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

    if (tag_update) {
      Mesh *mesh = static_cast<Mesh *>(ob->data);
      mesh->tag_positions_changed();

      BKE_sculptsession_free_deformMats(ss);
    }
    else {
      SCULPT_update_object_bounding_box(ob);
    }
  }
}

static void sculpt_undo_free_list(ListBase *lb)
{
  Node *unode = (Node *)lb->first;

  while (unode != nullptr) {
    Node *unode_next = unode->next;

    if (unode->bm_entry) {
      BM_log_entry_drop(unode->bm_entry);
      unode->bm_entry = nullptr;
      unode->bm_log = nullptr;
    }

    geometry_free_data(&unode->geometry_original);
    geometry_free_data(&unode->geometry_modified);

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
  Node *unode;

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

Node *get_node(PBVHNode *node, Type type)
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();

  if (usculpt == nullptr) {
    return nullptr;
  }

  if (type == Type::None) {
    return (Node *)BLI_findptr(&usculpt->nodes, node, offsetof(Node, node));
  }

  LISTBASE_FOREACH (Node *, unode, &usculpt->nodes) {
    if (unode->node == node && unode->type == type) {
      return unode;
    }
  }

  return nullptr;
}

Node *get_first_node()
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();

  if (usculpt == nullptr) {
    return nullptr;
  }

  return static_cast<Node *>(usculpt->nodes.first);
}

static size_t sculpt_undo_alloc_and_store_hidden(SculptSession *ss, Node *unode)
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
static Node *sculpt_undo_alloc_node_type(Object *object, Type type)
{
  const size_t alloc_size = sizeof(Node);
  Node *unode = MEM_new<Node>(__func__);
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
static Node *find_or_alloc_node_type(Object *object, Type type)
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();

  LISTBASE_FOREACH (Node *, unode, &usculpt->nodes) {
    if (unode->type == type) {
      return unode;
    }
  }

  return sculpt_undo_alloc_node_type(object, type);
}

static Node *sculpt_undo_alloc_node(Object *ob, PBVHNode *node, Type type)
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();
  SculptSession *ss = ob->sculpt;
  int totvert = 0;
  int allvert = 0;
  int totgrid = 0;
  int maxgrid = 0;
  int gridsize = 0;
  const int *grids = nullptr;

  Node *unode = sculpt_undo_alloc_node_type(ob, type);
  unode->node = node;

  if (node) {
    totvert = BKE_pbvh_node_num_unique_verts(*ss->pbvh, *node);
    allvert = BKE_pbvh_node_get_vert_indices(node).size();
    BKE_pbvh_node_get_grids(ss->pbvh, node, &grids, &totgrid, &maxgrid, &gridsize, nullptr);

    unode->verts_num = totvert;
  }

  bool need_loops = type == Type::Color;
  const bool need_faces = type == Type::FaceSet;

  if (need_loops) {
    unode->corner_indices = BKE_pbvh_node_get_corner_indices(node);
    unode->mesh_corners_num = static_cast<Mesh *>(ob->data)->corners_num;

    usculpt->undo_size += unode->corner_indices.as_span().size_in_bytes();
  }

  if (need_faces) {
    if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES) {
      bke::pbvh::node_face_indices_calc_mesh(*ss->pbvh, *node, unode->face_indices);
    }
    else {
      bke::pbvh::node_face_indices_calc_grids(*ss->pbvh, *node, unode->face_indices);
    }
    usculpt->undo_size += unode->face_indices.as_span().size_in_bytes();
  }

  switch (type) {
    case Type::None:
      BLI_assert_unreachable();
      break;
    case Type::Position: {
      unode->position.reinitialize(allvert);
      usculpt->undo_size += unode->position.as_span().size_in_bytes();

      /* Needed for original data lookup. */
      unode->normal.reinitialize(allvert);
      usculpt->undo_size += unode->normal.as_span().size_in_bytes();
      break;
    }
    case Type::HideFace: {
      unode->face_hidden.resize(unode->face_indices.size());
      usculpt->undo_size += unode->face_hidden.size() / 8;
      break;
    }
    case Type::HideVert: {
      if (maxgrid) {
        usculpt->undo_size += sculpt_undo_alloc_and_store_hidden(ss, unode);
      }
      else {
        unode->vert_hidden.resize(allvert);
      }

      break;
    }
    case Type::Mask: {
      unode->mask.reinitialize(allvert);
      usculpt->undo_size += unode->mask.as_span().size_in_bytes();
      break;
    }
    case Type::Color: {
      /* Allocate vertex colors, even for loop colors we still
       * need this for original data lookup. */
      unode->col.reinitialize(allvert);
      usculpt->undo_size += unode->col.as_span().size_in_bytes();

      /* Allocate loop colors separately too. */
      if (ss->vcol_domain == AttrDomain::Corner) {
        unode->loop_col.reinitialize(unode->corners_num);
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

  if (maxgrid) {
    /* Multires. */
    unode->maxgrid = maxgrid;
    unode->grids_num = totgrid;
    unode->gridsize = gridsize;

    unode->grids.reinitialize(totgrid);
    usculpt->undo_size += unode->grids.as_span().size_in_bytes();
  }
  else {
    /* Regular mesh. */
    unode->mesh_verts_num = ss->totvert;
    unode->vert_indices.reinitialize(allvert);
    usculpt->undo_size += unode->vert_indices.as_span().size_in_bytes();
  }

  if (ss->deform_modifiers_active) {
    unode->orig_position.reinitialize(allvert);
    usculpt->undo_size += unode->orig_position.as_span().size_in_bytes();
  }

  return unode;
}

static void sculpt_undo_store_coords(Object *ob, Node *unode)
{
  SculptSession *ss = ob->sculpt;
  PBVHVertexIter vd;
  const PBVHNode *node = static_cast<PBVHNode *>(unode->node);

  int totvert, allvert;
  totvert = BKE_pbvh_node_num_unique_verts(*ss->pbvh, *node);
  allvert = BKE_pbvh_node_get_vert_indices(node).size();

  if (ss->deform_modifiers_active && unode->orig_position.is_empty()) {
    unode->orig_position.reinitialize(allvert);
  }

  bool have_grids = BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, ((PBVHNode *)unode->node), vd, PBVH_ITER_ALL) {
    copy_v3_v3(unode->position[vd.i], vd.co);
    if (vd.no) {
      copy_v3_v3(unode->normal[vd.i], vd.no);
    }
    else {
      copy_v3_v3(unode->normal[vd.i], vd.fno);
    }

    if (ss->deform_modifiers_active) {
      if (!have_grids && ss->shapekey_active) {
        float(*cos)[3] = (float(*)[3])ss->shapekey_active->data;

        copy_v3_v3(unode->orig_position[vd.i], cos[vd.index]);
      }
      else {
        copy_v3_v3(unode->orig_position[vd.i],
                   blender::bke::paint::vertex_attr_ptr<float>(vd.vertex, ss->attrs.orig_co));
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void sculpt_undo_store_hidden(Object *ob, Node *unode)
{
  PBVH *pbvh = ob->sculpt->pbvh;
  PBVHNode *node = static_cast<PBVHNode *>(unode->node);

  const bool *hide_vert = BKE_pbvh_get_vert_hide(pbvh);
  if (hide_vert == nullptr) {
    return;
  }

  if (!unode->grids.is_empty()) {
    /* Already stored during allocation. */
  }
  else {
    const blender::Span<int> verts = BKE_pbvh_node_get_vert_indices(node);
    for (const int i : verts.index_range())
      unode->vert_hidden[i].set(hide_vert[verts[i]]);
  }
}

static void sculpt_undo_store_face_hidden(Object &object, Node &unode)
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

static void sculpt_undo_store_mask(Object *ob, Node *unode)
{
  SculptSession *ss = ob->sculpt;
  PBVHVertexIter vd;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, static_cast<PBVHNode *>(unode->node), vd, PBVH_ITER_ALL) {
    unode->mask[vd.i] = vd.mask;
  }
  BKE_pbvh_vertex_iter_end;
}

static void sculpt_undo_store_color(Object *ob, Node *unode)
{
  SculptSession *ss = ob->sculpt;

  BLI_assert(BKE_pbvh_type(ss->pbvh) == PBVH_FACES);

  /* NOTE: even with loop colors we still store (derived)
   * vertex colors for original data lookup. */
  BKE_pbvh_store_colors_vertex(
      ss->pbvh, unode->vert_indices.as_span().take_front(unode->unique_verts_num), unode->col);

  if (!unode->loop_col.is_empty() && unode->corners_num) {
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

static void sculpt_undo_store_face_sets(const Mesh &mesh, Node &unode)
{
  blender::array_utils::gather(
      *mesh.attributes().lookup_or_default<int>(".sculpt_face_set", bke::AttrDomain::Face, 0),
      unode.face_indices.as_span(),
      unode.face_sets.as_mutable_span());
}

void ensure_bmlog(Object *ob)
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

  if (!ss->bm && (!(me->flag & ME_SCULPT_DYNAMIC_TOPOLOGY) || ss->mode_type != OB_MODE_SCULPT)) {
    return;
  }

  if (!usculpt) {
    // happens during file load
    return;
  }

  Node *unode = (Node *)usculpt->nodes.first;

  BKE_sculpt_ensure_idmap(ob);

  /*when transition between undo step types the log might simply
  have been freed, look for entries to rebuild it with*/
  sculpt_unode_bmlog_ensure(ss, unode);
}

static Node *sculpt_undo_bmesh_push(Object *ob, PBVHNode *node, Type type)
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();
  SculptSession *ss = ob->sculpt;
  PBVHVertexIter vd;

  Node *unode = static_cast<Node *>(usculpt->nodes.first);

  ensure_bmlog(ob);

  if (!ss->bm_log) {
    ss->bm_log = BM_log_create(ss->bm, ss->bm_idmap);
  }

  bool new_node = false;

  if (unode == nullptr) {
    new_node = true;
    unode = MEM_new<Node>(__func__);

    STRNCPY(unode->idname, ob->id.name);
    unode->type = type;
    unode->applied = true;

    /* note that every undo type must push a bm_entry for
       so we can recreate the BMLog from chained entries
       when going to/from other undo system steps */

    if (type == Type::DyntopoEnd) {
      unode->bm_log = ss->bm_log;
      unode->bm_entry = BM_log_entry_add(ss->bm, ss->bm_log);

      BM_log_full_mesh(ss->bm, ss->bm_log);
    }
    else if (type == Type::DyntopoBegin) {
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
      case Type::HideVert:
      case Type::Position:
      case Type::Mask:
        BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
          BM_log_vert_modified(ss->bm, ss->bm_log, vd.bm_vert);
        }
        BKE_pbvh_vertex_iter_end;
        break;

      case Type::HideFace: {
        DyntopoSet<BMFace> &faces = BKE_pbvh_bmesh_node_faces(node);

        BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
          BM_log_vert_modified(ss->bm, ss->bm_log, vd.bm_vert);
        }
        BKE_pbvh_vertex_iter_end;

        for (BMFace *f : faces) {
          BM_log_face_modified(ss->bm, ss->bm_log, f);
        }
        break;
      }

      case Type::Color: {
        Mesh *mesh = BKE_object_get_original_mesh(ob);

        const CustomDataLayer *color_layer = BKE_id_attribute_search(
            &mesh->id,
            BKE_id_attributes_active_color_name(&mesh->id),
            CD_MASK_COLOR_ALL,
            ATTR_DOMAIN_MASK_POINT | ATTR_DOMAIN_MASK_CORNER);

        if (!color_layer) {
          break;
        }
        AttrDomain domain = BKE_id_attribute_domain(&mesh->id, color_layer);

        if (domain == AttrDomain::Point) {
          BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
            BM_log_vert_modified(ss->bm, ss->bm_log, vd.bm_vert);
          }
          BKE_pbvh_vertex_iter_end;
        }
        else if (domain == AttrDomain::Corner) {
          DyntopoSet<BMFace> &faces = BKE_pbvh_bmesh_node_faces(node);

          for (BMFace *f : faces) {
            BM_log_face_modified(ss->bm, ss->bm_log, f);
          }
        }

        break;
      }
      case Type::FaceSet: {
        DyntopoSet<BMFace> &faces = BKE_pbvh_bmesh_node_faces(node);

        for (BMFace *f : faces) {
          BM_log_face_if_modified(ss->bm, ss->bm_log, f);
        }

        break;
      }
      case Type::DyntopoBegin:
      case Type::DyntopoEnd:
      case Type::DyntopoSymmetrize:
      case Type::Geometry:
      case Type::None:
        break;
    }
  }
  else {
    switch (type) {
      case Type::DyntopoSymmetrize:
      case Type::Geometry:
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

bool ensure_dyntopo_node_undo(
    Object *ob, PBVHNode *node, Type type, Type extraType, Type force_push_mask)
{
  SculptSession *ss = ob->sculpt;

  UndoSculpt *usculpt = sculpt_undo_get_nodes();

  if (!usculpt) {
    printf("%s: possible undo error.\n", __func__);
    return false;
  }

  Node *unode = (Node *)usculpt->nodes.first;

  if (!unode) {
    unode = sculpt_undo_alloc_node_type(ob, type);

    BLI_strncpy(unode->idname, ob->id.name, sizeof(unode->idname));

    unode->type = type;
    unode->applied = true;
    unode->bm_log = ss->bm_log;
    unode->bm_entry = BM_log_entry_add(ss->bm, ss->bm_log);

    return undo::ensure_dyntopo_node_undo(ob, node, type, extraType, force_push_mask);
  }
  else if (!(unode->typemask & (1 << int(type)))) {
    /* Add a log sub-entry. */
    BM_log_entry_add_delta_set(ss->bm, ss->bm_log);
  }

  if (!node) {
    return false;
  }

  int node_id = BKE_pbvh_get_node_id(ss->pbvh, node);
  bool pushed = false;

  if (((type & force_push_mask) != Type::None) || unode->dyntopo_undo_set.add({node_id, type})) {
    sculpt_undo_bmesh_push(ob, node, type);
    pushed = true;
  }

  if (extraType != Type::None && (((extraType & force_push_mask) != Type::None) ||
                                  unode->dyntopo_undo_set.add({node_id, extraType})))
  {
    sculpt_undo_bmesh_push(ob, node, type);
    pushed = true;
  }

  return pushed;
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
      Node *unode = (Node *)step->data.nodes.first;

      if (!unode) {
        bad = true;
      }
      else {
        UndoStep *act = ustack->step_active;

        if (!act->type || !STREQ(act->type->name, "Sculpt")) {
          bad = unode->type != Type::DyntopoBegin;
        }
      }
    }
  }
  else {
    bad = true;
  }

  if (bad) {
    sculpt_undo_push_begin_ex(ob, "Dyntopo Begin", true);
    push_node(ob, nullptr, Type::DyntopoBegin);
    push_end(ob);
  }

  return bad;
}

Node *push_node(Object *ob, PBVHNode *node, Type type)
{
  SculptSession *ss = ob->sculpt;
  Node *unode;

  /* List is manipulated by multiple threads, so we lock. */
  BLI_thread_lock(LOCK_CUSTOM1);

  ss->needs_flush_to_id = 1;

  if (ss->bm || ELEM(type, Type::DyntopoBegin, Type::DyntopoEnd)) {
    /* Dynamic topology stores only one undo node per stroke,
     * regardless of the number of PBVH nodes modified. */
    unode = sculpt_undo_bmesh_push(ob, node, type);
    sculpt_undo_print_nodes(ob, nullptr);
    BLI_thread_unlock(LOCK_CUSTOM1);
    return unode;
  }
  if (type == Type::Geometry) {
    unode = geometry_push(ob, type);
    sculpt_undo_print_nodes(ob, nullptr);
    BLI_thread_unlock(LOCK_CUSTOM1);
    return unode;
  }
  if ((unode = get_node(node, type))) {
    sculpt_undo_print_nodes(ob, nullptr);
    BLI_thread_unlock(LOCK_CUSTOM1);
    return unode;
  }

  unode = sculpt_undo_alloc_node(ob, node, type);

  /* NOTE: If this ever becomes a bottleneck, make a lock inside of the node.
   * so we release global lock sooner, but keep data locked for until it is
   * fully initialized.
   */

  if (!unode->grids.is_empty()) {
    int totgrid;
    const int *grids;
    BKE_pbvh_node_get_grids(ss->pbvh, node, &grids, &totgrid, nullptr, nullptr, nullptr);
    unode->grids.as_mutable_span().copy_from({grids, totgrid});
  }
  else {
    unode->vert_indices.as_mutable_span().copy_from(BKE_pbvh_node_get_vert_indices(node));

    if (!unode->corner_indices.is_empty()) {
      Span<int> corner_indices = BKE_pbvh_node_get_corner_indices(
          static_cast<const PBVHNode *>(unode->node));

      if (!corner_indices.is_empty()) {
        unode->corner_indices.as_mutable_span().copy_from(corner_indices);
        unode->mesh_corners_num = BKE_object_get_original_mesh(ob)->corners_num;
      }
    }
  }

  switch (type) {
    case Type::None:
      BLI_assert_unreachable();
      break;
    case Type::Position:
      sculpt_undo_store_coords(ob, unode);
      break;
    case Type::HideVert:
      sculpt_undo_store_hidden(ob, unode);
      break;
    case Type::HideFace:
      sculpt_undo_store_face_hidden(*ob, *unode);
      break;
    case Type::Mask:
      if (pbvh_has_mask(ss->pbvh)) {
        sculpt_undo_store_mask(ob, unode);
      }
      break;
    case Type::Color:
      sculpt_undo_store_color(ob, unode);
      break;
    case Type::DyntopoBegin:
    case Type::DyntopoEnd:
    case Type::DyntopoSymmetrize:
      BLI_assert_msg(0, "Dynamic topology should've already been handled");
    case Type::Geometry:
      break;
    case Type::FaceSet:
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
  const CustomDataLayer *layer;

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

  ensure_bmlog(ob);

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

  /* Set end attribute in case push_end is not called,
   * so we don't end up with corrupted state.
   */
  if (!us->active_color_end.was_set) {
    sculpt_save_active_attribute_color(ob, &us->active_color_end);
    us->active_color_end.was_set = false;
  }
}

void push_begin_ex(Object *ob, const char *name)
{
  return sculpt_undo_push_begin_ex(ob, name, false);
}

void push_begin(Object *ob, const wmOperator *op)
{
  push_begin_ex(ob, op->type->name);
}

void push_end(Object *ob)
{
  push_end_ex(ob, false);
}

void push_end_ex(Object *ob, const bool use_nested_undo)
{
  UndoSculpt *usculpt = sculpt_undo_get_nodes();

  /* We don't need normals in the undo stack. */
  LISTBASE_FOREACH (Node *, unode, &usculpt->nodes) {
    if (unode->bm_entry) {
      update_unode_bmesh_memsize(unode);
    }

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

  sculpt_save_active_attribute(ob, &us->active_attr_end);
  sculpt_save_active_attribute_color(ob, &us->active_color_end);
  sculpt_undo_print_nodes(ob, nullptr);
}

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 * \{ */

static void sculpt_undo_set_active_layer(bContext *C, SculptAttrRef *attr, bool is_color)
{
  if (attr->domain == AttrDomain::Auto) {
    return;
  }

  Object *ob = CTX_data_active_object(C);
  Mesh *mesh = BKE_object_get_original_mesh(ob);

  SculptAttrRef existing;
  if (is_color) {
    sculpt_save_active_attribute_color(ob, &existing);
  }
  else {
    sculpt_save_active_attribute(ob, &existing);
  }

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

      if (!sculpt_attribute_ref_equals(&existing, attr)) {
        bke::pbvh::update_vertex_data(*ob->sculpt->pbvh, PBVH_UpdateColor);
      }
    }
  }
  else if (layer) {
    BKE_id_attributes_active_set(&mesh->id, layer->name);
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

  Node *unode = static_cast<Node *>(us->data.nodes.last);
  if (unode && unode->type == Type::DyntopoEnd) {
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
  sculpt_undo_restore_list(C, depsgraph, &us->data.nodes, -1);
  us->step.is_applied = false;

  sculpt_undo_print_nodes(CTX_data_active_object(C), us);
}

static void sculpt_undosys_step_decode_redo_impl(bContext *C,
                                                 Depsgraph *depsgraph,
                                                 SculptUndoStep *us)
{
  BLI_assert(us->step.is_applied == false);
  sculpt_undo_restore_list(C, depsgraph, &us->data.nodes, 1);
  us->step.is_applied = true;

  sculpt_undo_print_nodes(CTX_data_active_object(C), us);
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
  /* Ensure sculpt attribute references are up to date. */
  BKE_sculptsession_update_attr_refs(ob);

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

static UndoSculpt *sculpt_undo_get_nodes()
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
  if (BKE_paintmode_get_active_from_context(C) != PaintMode::Sculpt) {
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
   * Skip pushing nodes based on the following logic: on redo Type::Position will ensure
   * PBVH for the new base geometry, which will have same coordinates as if we create PBVH here.
   */
  if (ss->pbvh == nullptr) {
    return;
  }

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, {});
  for (PBVHNode *node : nodes) {
    Node *unode = push_node(object, node, Type::Position);
    unode->node = nullptr;
  }
}

void push_multires_mesh_begin(bContext *C, const char *str)
{
  if (!sculpt_undo_use_multires_mesh(C)) {
    return;
  }

  Object *object = CTX_data_active_object(C);

  push_begin_ex(object, str);

  Node *geometry_unode = push_node(object, nullptr, Type::Geometry);
  geometry_unode->geometry_clear_pbvh = false;

  sculpt_undo_push_all_grids(object);
}

void push_multires_mesh_end(bContext *C, const char *str)
{
  if (!sculpt_undo_use_multires_mesh(C)) {
    ED_undo_push(C, str);
    return;
  }

  Object *object = CTX_data_active_object(C);

  Node *geometry_unode = push_node(object, nullptr, Type::Geometry);
  geometry_unode->geometry_clear_pbvh = false;

  push_end(object);
}

/** \} */

void fast_save_bmesh(Object *ob)
{
  if (!ob->sculpt || !ob->sculpt->bm) {
    return;
  }

  SculptSession *ss = ob->sculpt;
  BMesh *bm = ss->bm;

  struct BMeshToMeshParams params = {};
  params.update_shapekey_indices = true;
  BM_mesh_bm_to_me(nullptr, bm, (Mesh *)ob->data, &params);
}
}  // namespace blender::ed::sculpt_paint::undo
