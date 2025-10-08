/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "DNA_object_types.h"

#include "BLI_linklist_stack.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_memarena.h"
#include "BLI_stack.h"
#include "BLI_vector.hh"

#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_editmesh_bvh.hh"
#include "BKE_layer.hh"
#include "BKE_report.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_types.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "ED_mesh.hh"
#include "ED_screen.hh"

#include "mesh_intern.hh" /* own include */

#include "tools/bmesh_boolean.hh"
#include "tools/bmesh_intersect.hh"
#include "tools/bmesh_separate.hh"

/* detect isolated holes and fill them */
#define USE_NET_ISLAND_CONNECT

using blender::Vector;

/**
 * Compare selected with itself.
 */
static int bm_face_isect_self(BMFace *f, void * /*user_data*/)
{
  if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
    return 0;
  }
  return -1;
}

/**
 * Compare selected/unselected.
 */
static int bm_face_isect_pair(BMFace *f, void * /*user_data*/)
{
  if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
    return -1;
  }
  if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
    return 1;
  }
  return 0;
}

/**
 * A flipped version of #bm_face_isect_pair
 * use for boolean 'difference', which depends on order.
 */
static int bm_face_isect_pair_swap(BMFace *f, void * /*user_data*/)
{
  if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
    return -1;
  }
  if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
    return 0;
  }
  return 1;
}

/**
 * Use for intersect and boolean.
 */
static void edbm_intersect_select(BMEditMesh *em, Mesh *mesh, bool do_select)
{
  if (do_select) {
    BM_mesh_elem_hflag_disable_all(em->bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_SELECT, false);

    if (em->bm->selectmode & (SCE_SELECT_VERTEX | SCE_SELECT_EDGE)) {
      BMIter iter;
      BMEdge *e;

      BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
        if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
          BM_edge_select_set(em->bm, e, true);
        }
      }

      EDBM_selectmode_flush(em);
      EDBM_uvselect_clear(em);
    }
  }

  EDBMUpdate_Params params{};
  params.calc_looptris = true;
  params.calc_normals = true;
  params.is_destructive = true;
  EDBM_update(mesh, &params);
}

/* -------------------------------------------------------------------- */
/** \name Simple Intersect (self-intersect)
 *
 * Cut intersections into geometry.
 * \{ */

enum {
  ISECT_SEL = 0,
  ISECT_SEL_UNSEL = 1,
};

enum {
  ISECT_SEPARATE_ALL = 0,
  ISECT_SEPARATE_CUT = 1,
  ISECT_SEPARATE_NONE = 2,
};

enum {
  ISECT_SOLVER_FAST = 0,
  ISECT_SOLVER_EXACT = 1,
};

static wmOperatorStatus edbm_intersect_exec(bContext *C, wmOperator *op)
{
  const int mode = RNA_enum_get(op->ptr, "mode");
  int (*test_fn)(BMFace *, void *);
  bool use_separate_all = false;
  bool use_separate_cut = false;
  const int separate_mode = RNA_enum_get(op->ptr, "separate_mode");
  const float eps = RNA_float_get(op->ptr, "threshold");
#ifdef WITH_GMP
  const bool exact = RNA_enum_get(op->ptr, "solver") == ISECT_SOLVER_EXACT;
#else
  if (RNA_enum_get(op->ptr, "solver") == ISECT_SOLVER_EXACT) {
    BKE_report(op->reports, RPT_WARNING, "Compiled without GMP, using fast solver");
  }
  const bool exact = false;
#endif
  bool use_self;
  bool has_isect;

  switch (mode) {
    case ISECT_SEL:
      test_fn = bm_face_isect_self;
      use_self = true;
      break;
    default: /* ISECT_SEL_UNSEL */
      test_fn = bm_face_isect_pair;
      use_self = false;
      break;
  }

  switch (separate_mode) {
    case ISECT_SEPARATE_ALL:
      use_separate_all = true;
      break;
    case ISECT_SEPARATE_CUT:
      if (use_self == false) {
        use_separate_cut = true;
      }
      else {
        /* we could support this but would require more advanced logic inside 'BM_mesh_intersect'
         * for now just separate all */
        use_separate_all = true;
      }
      break;
    default: /* ISECT_SEPARATE_NONE */
      break;
  }
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint isect_len = 0;
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (em->bm->totfacesel == 0) {
      continue;
    }

    if (exact) {
      int nshapes = use_self ? 1 : 2;
      has_isect = BM_mesh_boolean_knife(em->bm,
                                        em->looptris,
                                        test_fn,
                                        nullptr,
                                        nshapes,
                                        use_self,
                                        use_separate_all,
                                        false,
                                        true);
    }
    else {
      has_isect = BM_mesh_intersect(em->bm,
                                    em->looptris,
                                    test_fn,
                                    nullptr,
                                    use_self,
                                    use_separate_all,
                                    true,
                                    true,
                                    true,
                                    true,
                                    -1,
                                    eps);
    }

    if (use_separate_cut) {
      /* detach selected/un-selected faces */
      BM_mesh_separate_faces(
          em->bm, BM_elem_cb_check_hflag_enabled_simple(const BMFace *, BM_ELEM_SELECT));
    }

    edbm_intersect_select(em, static_cast<Mesh *>(obedit->data), has_isect);

    if (!has_isect) {
      isect_len++;
    }
  }

  if (isect_len == objects.size()) {
    BKE_report(op->reports, RPT_WARNING, "No intersections found");
  }
  return OPERATOR_FINISHED;
}

static void edbm_intersect_ui(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;
  uiLayout *row;

  bool use_exact = RNA_enum_get(op->ptr, "solver") == ISECT_SOLVER_EXACT;

  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);
  row = &layout->row(false);
  row->prop(op->ptr, "mode", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  layout->separator();
  row = &layout->row(false);
  row->prop(op->ptr, "separate_mode", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  layout->separator();

  row = &layout->row(false);
  row->prop(op->ptr, "solver", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  layout->separator();

  if (!use_exact) {
    layout->prop(op->ptr, "threshold", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
}

void MESH_OT_intersect(wmOperatorType *ot)
{
  static const EnumPropertyItem isect_mode_items[] = {
      {ISECT_SEL, "SELECT", 0, "Self Intersect", "Self intersect selected faces"},
      {ISECT_SEL_UNSEL,
       "SELECT_UNSELECT",
       0,
       "Selected/Unselected",
       "Intersect selected with unselected faces"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem isect_separate_items[] = {
      {ISECT_SEPARATE_ALL, "ALL", 0, "All", "Separate all geometry from intersections"},
      {ISECT_SEPARATE_CUT,
       "CUT",
       0,
       "Cut",
       "Cut into geometry keeping each side separate (Selected/Unselected only)"},
      {ISECT_SEPARATE_NONE, "NONE", 0, "Merge", "Merge all geometry from the intersection"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem isect_intersect_solver_items[] = {
      {ISECT_SOLVER_FAST,
       "FLOAT",
       0,
       "Float",
       "Simple solver with good performance, without support for overlapping geometry"},
      {ISECT_SOLVER_EXACT,
       "EXACT",
       0,
       "Exact",
       "Slower solver with the best results for coplanar faces"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Intersect (Knife)";
  ot->description = "Cut an intersection into faces";
  ot->idname = "MESH_OT_intersect";

  /* API callbacks. */
  ot->exec = edbm_intersect_exec;
  ot->poll = ED_operator_editmesh;
  ot->ui = edbm_intersect_ui;

  /* props */
  RNA_def_enum(ot->srna, "mode", isect_mode_items, ISECT_SEL_UNSEL, "Source", "");
  RNA_def_enum(
      ot->srna, "separate_mode", isect_separate_items, ISECT_SEPARATE_CUT, "Separate Mode", "");
  RNA_def_float_distance(
      ot->srna, "threshold", 0.000001f, 0.0, 0.01, "Merge Threshold", "", 0.0, 0.001);
  RNA_def_enum(ot->srna,
               "solver",
               isect_intersect_solver_items,
               ISECT_SOLVER_EXACT,
               "Solver",
               "Which Intersect solver to use");

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Boolean Intersect
 *
 * \note internally this is nearly exactly the same as `MESH_OT_intersect`,
 * however from a user perspective they are quite different, so expose as different tools.
 * \{ */

static wmOperatorStatus edbm_intersect_boolean_exec(bContext *C, wmOperator *op)
{
  const int boolean_operation = RNA_enum_get(op->ptr, "operation");
  bool use_swap = RNA_boolean_get(op->ptr, "use_swap");
  bool use_self = RNA_boolean_get(op->ptr, "use_self");
#ifdef WITH_GMP
  const bool use_exact = RNA_enum_get(op->ptr, "solver") == ISECT_SOLVER_EXACT;
#else
  if (RNA_enum_get(op->ptr, "solver") == ISECT_SOLVER_EXACT) {
    BKE_report(op->reports, RPT_WARNING, "Compiled without GMP, using fast solver");
  }
  const bool use_exact = false;
#endif
  const float eps = RNA_float_get(op->ptr, "threshold");
  int (*test_fn)(BMFace *, void *);
  bool has_isect;

  test_fn = use_swap ? bm_face_isect_pair_swap : bm_face_isect_pair;
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint isect_len = 0;
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    if (em->bm->totfacesel == 0) {
      continue;
    }

    if (use_exact) {
      has_isect = BM_mesh_boolean(
          em->bm, em->looptris, test_fn, nullptr, 2, use_self, true, false, boolean_operation);
    }
    else {
      has_isect = BM_mesh_intersect(em->bm,
                                    em->looptris,
                                    test_fn,
                                    nullptr,
                                    false,
                                    false,
                                    true,
                                    true,
                                    false,
                                    true,
                                    boolean_operation,
                                    eps);
    }

    edbm_intersect_select(em, static_cast<Mesh *>(obedit->data), has_isect);

    if (!has_isect) {
      isect_len++;
    }
  }

  if (isect_len == objects.size()) {
    BKE_report(op->reports, RPT_WARNING, "No intersections found");
  }
  return OPERATOR_FINISHED;
}

static void edbm_intersect_boolean_ui(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;
  uiLayout *row;

  bool use_exact = RNA_enum_get(op->ptr, "solver") == ISECT_SOLVER_EXACT;

  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  row = &layout->row(false);
  row->prop(op->ptr, "operation", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  layout->separator();

  row = &layout->row(false);
  row->prop(op->ptr, "solver", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  layout->separator();

  layout->prop(op->ptr, "use_swap", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(op->ptr, "use_self", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  if (!use_exact) {
    layout->prop(op->ptr, "threshold", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
}

void MESH_OT_intersect_boolean(wmOperatorType *ot)
{
  static const EnumPropertyItem isect_boolean_operation_items[] = {
      {BMESH_ISECT_BOOLEAN_ISECT, "INTERSECT", 0, "Intersect", ""},
      {BMESH_ISECT_BOOLEAN_UNION, "UNION", 0, "Union", ""},
      {BMESH_ISECT_BOOLEAN_DIFFERENCE, "DIFFERENCE", 0, "Difference", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem isect_boolean_solver_items[] = {
      {ISECT_SOLVER_FAST, "FLOAT", 0, "Float", "Faster solver, some limitations"},
      {ISECT_SOLVER_EXACT, "EXACT", 0, "Exact", "Exact solver, slower, handles more cases"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Intersect (Boolean)";
  ot->description = "Cut solid geometry from selected to unselected";
  ot->idname = "MESH_OT_intersect_boolean";

  /* API callbacks. */
  ot->exec = edbm_intersect_boolean_exec;
  ot->poll = ED_operator_editmesh;
  ot->ui = edbm_intersect_boolean_ui;

  /* props */
  RNA_def_enum(ot->srna,
               "operation",
               isect_boolean_operation_items,
               BMESH_ISECT_BOOLEAN_DIFFERENCE,
               "Boolean Operation",
               "Which boolean operation to apply");
  RNA_def_boolean(ot->srna,
                  "use_swap",
                  false,
                  "Swap",
                  "Use with difference intersection to swap which side is kept");
  RNA_def_boolean(
      ot->srna, "use_self", false, "Self Intersection", "Do self-union or self-intersection");
  RNA_def_float_distance(
      ot->srna, "threshold", 0.000001f, 0.0, 0.01, "Merge Threshold", "", 0.0, 0.001);
  RNA_def_enum(ot->srna,
               "solver",
               isect_boolean_solver_items,
               ISECT_SOLVER_EXACT,
               "Solver",
               "Which Boolean solver to use");

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Face Split by Edges
 * \{ */

static void bm_face_split_by_edges(BMesh *bm,
                                   BMFace *f,
                                   const char hflag, /* reusable memory buffer */
                                   Vector<BMEdge *, 128> *edge_net_temp_buf)
{
  const int f_index = BM_elem_index_get(f);

  BMLoop *l_iter;
  BMLoop *l_first;
  BMVert *v;

  /* likely this will stay very small
   * all verts pushed into this stack _must_ have their previous edges set! */
  BLI_SMALLSTACK_DECLARE(vert_stack, BMVert *);
  BLI_SMALLSTACK_DECLARE(vert_stack_next, BMVert *);

  BLI_assert(edge_net_temp_buf->is_empty());

  /* collect all edges */
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    BMIter iter;
    BMEdge *e;

    BM_ITER_ELEM (e, &iter, l_iter->v, BM_EDGES_OF_VERT) {
      if (BM_elem_flag_test(e, hflag) && (BM_elem_index_get(e) == f_index)) {
        v = BM_edge_other_vert(e, l_iter->v);
        v->e = e;

        BLI_SMALLSTACK_PUSH(vert_stack, v);
        edge_net_temp_buf->append(e);
      }
    }
  } while ((l_iter = l_iter->next) != l_first);

  /* now assign all */
  /* pop free values into the next stack */
  while ((v = static_cast<BMVert *>(BLI_SMALLSTACK_POP_EX(vert_stack, vert_stack_next)))) {
    BMIter eiter;
    BMEdge *e_next;

    BM_ITER_ELEM (e_next, &eiter, v, BM_EDGES_OF_VERT) {
      if (BM_elem_flag_test(e_next, hflag) && (BM_elem_index_get(e_next) == -1)) {
        BMVert *v_next;
        v_next = BM_edge_other_vert(e_next, v);
        BM_elem_index_set(e_next, f_index);
        BLI_SMALLSTACK_PUSH(vert_stack_next, v_next);
        edge_net_temp_buf->append(e_next);
      }
    }

    if (BLI_SMALLSTACK_IS_EMPTY(vert_stack)) {
      BLI_SMALLSTACK_SWAP(vert_stack, vert_stack_next);
    }
  }

  Vector<BMFace *> face_arr;
  BM_face_split_edgenet(bm, f, edge_net_temp_buf->data(), edge_net_temp_buf->size(), &face_arr);

  edge_net_temp_buf->clear();

  for (BMFace *face : face_arr) {
    BM_face_select_set(bm, face, true);
    BM_elem_flag_disable(face, hflag);
  }
}

/**
 * Check if a vert is in any of the faces connected to the edge,
 * \a f_ignore is a face we happen to know isn't shared by the vertex.
 */
static bool bm_vert_in_faces_radial(BMVert *v, BMEdge *e_radial, BMFace *f_ignore)
{
  BLI_assert(BM_vert_in_face(v, f_ignore) == false);
  if (e_radial->l) {
    BMLoop *l_iter = e_radial->l;
    do {
      if (l_iter->f != f_ignore) {
        if (BM_vert_in_face(v, l_iter->f)) {
          return true;
        }
      }
    } while ((l_iter = l_iter->radial_next) != e_radial->l);
  }
  return false;
}

#ifdef USE_NET_ISLAND_CONNECT

struct LinkBase {
  LinkNode *list;
  uint list_len;
};

static void ghash_insert_face_edge_link(GHash *gh,
                                        BMFace *f_key,
                                        BMEdge *e_val,
                                        MemArena *mem_arena)
{
  void **ls_base_p;
  LinkBase *ls_base;
  LinkNode *ls;

  if (!BLI_ghash_ensure_p(gh, f_key, &ls_base_p)) {
    ls_base = static_cast<LinkBase *>(*ls_base_p = BLI_memarena_alloc(mem_arena,
                                                                      sizeof(*ls_base)));
    ls_base->list = nullptr;
    ls_base->list_len = 0;
  }
  else {
    ls_base = static_cast<LinkBase *>(*ls_base_p);
  }

  ls = static_cast<LinkNode *>(BLI_memarena_alloc(mem_arena, sizeof(*ls)));
  ls->next = ls_base->list;
  ls->link = e_val;
  ls_base->list = ls;
  ls_base->list_len += 1;
}

static int bm_edge_sort_length_cb(const void *e_a_v, const void *e_b_v)
{
  const float val_a = -BM_edge_calc_length_squared(*((BMEdge **)e_a_v));
  const float val_b = -BM_edge_calc_length_squared(*((BMEdge **)e_b_v));

  if (val_a > val_b) {
    return 1;
  }
  if (val_a < val_b) {
    return -1;
  }
  return 0;
}

static void bm_face_split_by_edges_island_connect(
    BMesh *bm, BMFace *f, LinkNode *e_link, const int e_link_len, MemArena *mem_arena_edgenet)
{
  BMEdge **edge_arr = static_cast<BMEdge **>(
      BLI_memarena_alloc(mem_arena_edgenet, sizeof(*edge_arr) * e_link_len));
  int edge_arr_len = 0;

  while (e_link) {
    edge_arr[edge_arr_len++] = static_cast<BMEdge *>(e_link->link);
    e_link = e_link->next;
  }

  {
    uint edge_arr_holes_len;
    BMEdge **edge_arr_holes;
    if (BM_face_split_edgenet_connect_islands(bm,
                                              f,
                                              edge_arr,
                                              e_link_len,
                                              true,
                                              mem_arena_edgenet,
                                              &edge_arr_holes,
                                              &edge_arr_holes_len))
    {
      edge_arr_len = edge_arr_holes_len;
      edge_arr = edge_arr_holes; /* owned by the arena */
    }
  }

  BM_face_split_edgenet(bm, f, edge_arr, edge_arr_len, nullptr);

  for (int i = e_link_len; i < edge_arr_len; i++) {
    BM_edge_select_set(bm, edge_arr[i], true);
  }

  if (e_link_len != edge_arr_len) {
    /* connecting partial islands can add redundant edges
     * sort before removal to give deterministic outcome */
    qsort(edge_arr, edge_arr_len - e_link_len, sizeof(*edge_arr), bm_edge_sort_length_cb);
    for (int i = e_link_len; i < edge_arr_len; i++) {
      BMFace *f_pair[2];
      if (BM_edge_face_pair(edge_arr[i], &f_pair[0], &f_pair[1])) {
        if (BM_face_share_vert_count(f_pair[0], f_pair[1]) == 2) {
          BMFace *f_double;
          BMFace *f_new = BM_faces_join(bm, f_pair, 2, true, &f_double);
          /* See #BM_faces_join note on callers asserting when `r_double` is non-null. */
          BLI_assert_msg(f_double == nullptr,
                         "Doubled face detected at " AT ". Resulting mesh may be corrupt.");

          if (f_new) {
            BM_face_select_set(bm, f_new, true);
          }
        }
      }
    }
  }
}

/**
 * Check if \a v_pivot should be spliced into an existing edge.
 *
 * Detect one of 3 cases:
 *
 * - \a v_pivot is shared by 2+ edges from different faces.
 *   in this case return the closest edge shared by all faces.
 *
 * - \a v_pivot is an end-point of an edge which has no other edges connected.
 *   in this case return the closest edge in \a f_a to the \a v_pivot.
 *
 * - \a v_pivot has only edges from the same face connected,
 *   in this case return nullptr. This is the most common case - no action is needed.
 *
 * \return the edge to be split.
 *
 * \note Currently we don't snap to verts or split chains by verts on-edges.
 */
static BMEdge *bm_face_split_edge_find(BMEdge *e_a,
                                       BMFace *f_a,
                                       BMVert *v_pivot,
                                       BMFace **ftable,
                                       const int ftable_len,
                                       float r_v_pivot_co[3],
                                       float *r_v_pivot_fac)
{
  const int f_a_index = BM_elem_index_get(e_a);
  bool found_other_self = false;
  int found_other_face = 0;
  BLI_SMALLSTACK_DECLARE(face_stack, BMFace *);

  /* loop over surrounding edges to check if we're part of a chain or a delimiter vertex */
  BMEdge *e_b = v_pivot->e;
  do {
    if (e_b != e_a) {
      const int f_b_index = BM_elem_index_get(e_b);
      if (f_b_index == f_a_index) {
        /* not an endpoint */
        found_other_self = true;
      }
      else if (f_b_index != -1) {
        BLI_assert(f_b_index < ftable_len);
        UNUSED_VARS_NDEBUG(ftable_len);

        /* 'v_pivot' spans 2+ faces,
         * tag to ensure we pick an edge that includes this face */
        BMFace *f_b = ftable[f_b_index];
        if (!BM_elem_flag_test(f_b, BM_ELEM_INTERNAL_TAG)) {
          BM_elem_flag_enable(f_b, BM_ELEM_INTERNAL_TAG);
          BLI_SMALLSTACK_PUSH(face_stack, f_b);
          found_other_face++;
        }
      }
    }
  } while ((e_b = BM_DISK_EDGE_NEXT(e_b, v_pivot)) != v_pivot->e);

  BMEdge *e_split = nullptr;

  /* if we have no others or the other edge is outside this face,
   * we're an endpoint to connect to a boundary */
  if ((found_other_self == false) || found_other_face) {

    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f_a);
    float dist_best_sq = FLT_MAX;

    do {
      float v_pivot_co_test[3];
      float v_pivot_fac = line_point_factor_v3(v_pivot->co, l_iter->e->v1->co, l_iter->e->v2->co);
      CLAMP(v_pivot_fac, 0.0f, 1.0f);
      interp_v3_v3v3(v_pivot_co_test, l_iter->e->v1->co, l_iter->e->v2->co, v_pivot_fac);

      float dist_test_sq = len_squared_v3v3(v_pivot_co_test, v_pivot->co);
      if ((dist_test_sq < dist_best_sq) || (e_split == nullptr)) {
        bool ok = true;

        if (UNLIKELY(BM_edge_exists(v_pivot, l_iter->e->v1) ||
                     BM_edge_exists(v_pivot, l_iter->e->v2)))
        {
          /* very unlikely but will cause complications splicing the verts together,
           * so just skip this case */
          ok = false;
        }
        else if (found_other_face) {
          /* double check that _all_ the faces used by v_pivot's edges are attached
           * to this edge otherwise don't attempt the split since it will give
           * non-deterministic results */
          BMLoop *l_radial_iter = l_iter->radial_next;
          int other_face_shared = 0;
          if (l_radial_iter != l_iter) {
            do {
              if (BM_elem_flag_test(l_radial_iter->f, BM_ELEM_INTERNAL_TAG)) {
                other_face_shared++;
              }
            } while ((l_radial_iter = l_radial_iter->radial_next) != l_iter);
          }
          if (other_face_shared != found_other_face) {
            ok = false;
          }
        }

        if (ok) {
          e_split = l_iter->e;
          dist_best_sq = dist_test_sq;
          copy_v3_v3(r_v_pivot_co, v_pivot_co_test);
          *r_v_pivot_fac = v_pivot_fac;
        }
      }
    } while ((l_iter = l_iter->next) != l_first);
  }

  {
    /* reset the flag, for future use */
    BMFace *f;
    while ((f = static_cast<BMFace *>(BLI_SMALLSTACK_POP(face_stack)))) {
      BM_elem_flag_disable(f, BM_ELEM_INTERNAL_TAG);
    }
  }

  return e_split;
}

#endif /* USE_NET_ISLAND_CONNECT */

static wmOperatorStatus edbm_face_split_by_edges_exec(bContext *C, wmOperator * /*op*/)
{
  const char hflag = BM_ELEM_TAG;

  BMEdge *e;
  BMIter iter;

  BLI_SMALLSTACK_DECLARE(loop_stack, BMLoop *);

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;

    if ((bm->totedgesel == 0) || (bm->totfacesel == 0)) {
      continue;
    }

    {
      BMVert *v;
      BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
        BM_elem_flag_disable(v, hflag);
      }
    }

    /* edge index is set to -1 then used to associate them with faces */
    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      if (BM_elem_flag_test(e, BM_ELEM_SELECT) && BM_edge_is_wire(e)) {
        BM_elem_flag_enable(e, hflag);

        BM_elem_flag_enable(e->v1, hflag);
        BM_elem_flag_enable(e->v2, hflag);
      }
      else {
        BM_elem_flag_disable(e, hflag);
      }
      BM_elem_index_set(e, -1); /* set_dirty */
    }
    bm->elem_index_dirty |= BM_EDGE;

    {
      BMFace *f;
      int i;
      BM_ITER_MESH_INDEX (f, &iter, bm, BM_FACES_OF_MESH, i) {
        if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
          BM_elem_flag_enable(f, hflag);
        }
        else {
          BM_elem_flag_disable(f, hflag);
        }
        BM_elem_flag_disable(f, BM_ELEM_INTERNAL_TAG);
        BM_elem_index_set(f, i); /* set_ok */
      }
    }
    bm->elem_index_dirty &= ~BM_FACE;

    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      if (BM_elem_flag_test(e, hflag)) {
        BMIter viter;
        BMVert *v;
        BM_ITER_ELEM (v, &viter, e, BM_VERTS_OF_EDGE) {
          BMIter liter;
          BMLoop *l;

          uint loop_stack_len;
          BMLoop *l_best = nullptr;

          BLI_assert(BLI_SMALLSTACK_IS_EMPTY(loop_stack));
          loop_stack_len = 0;

          BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
            if (BM_elem_flag_test(l->f, hflag)) {
              BLI_SMALLSTACK_PUSH(loop_stack, l);
              loop_stack_len++;
            }
          }

          if (loop_stack_len == 0) {
            /* pass */
          }
          else if (loop_stack_len == 1) {
            l_best = static_cast<BMLoop *>(BLI_SMALLSTACK_POP(loop_stack));
          }
          else {
            /* complicated case, match the edge with a face-loop */

            BMVert *v_other = BM_edge_other_vert(e, v);
            float e_dir[3];

            /* we want closest to zero */
            float dot_best = FLT_MAX;

            sub_v3_v3v3(e_dir, v_other->co, v->co);
            normalize_v3(e_dir);

            while ((l = static_cast<BMLoop *>(BLI_SMALLSTACK_POP(loop_stack)))) {
              float dot_test;

              /* Check dot first to save on expensive angle-comparison.
               * ideal case is 90d difference == 0.0 dot */
              dot_test = fabsf(dot_v3v3(e_dir, l->f->no));
              if (dot_test < dot_best) {

                /* check we're in the correct corner
                 * (works with convex loops too) */
                if (angle_signed_on_axis_v3v3v3_v3(
                        l->prev->v->co, l->v->co, v_other->co, l->f->no) <
                    angle_signed_on_axis_v3v3v3_v3(
                        l->prev->v->co, l->v->co, l->next->v->co, l->f->no))
                {
                  dot_best = dot_test;
                  l_best = l;
                }
              }
            }
          }

          if (l_best) {
            BM_elem_index_set(e, BM_elem_index_get(l_best->f)); /* set_dirty */
          }
        }
      }
    }

    {
      BMFace *f;
      Vector<BMEdge *, 128> edge_net_temp_buf;

      BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
        if (BM_elem_flag_test(f, hflag)) {
          bm_face_split_by_edges(bm, f, hflag, &edge_net_temp_buf);
        }
      }
    }

#ifdef USE_NET_ISLAND_CONNECT
    /* before overwriting edge index values, collect edges left untouched */
    BLI_Stack *edges_loose = BLI_stack_new(sizeof(BMEdge *), __func__);
    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      if (BM_elem_flag_test(e, BM_ELEM_SELECT) && BM_edge_is_wire(e)) {
        BLI_stack_push(edges_loose, &e);
      }
    }
#endif

    EDBMUpdate_Params params{};
    params.calc_looptris = true;
    params.calc_normals = true;
    params.is_destructive = true;
    EDBM_update(static_cast<Mesh *>(obedit->data), &params);

#ifdef USE_NET_ISLAND_CONNECT
    /* we may have remaining isolated regions remaining,
     * these will need to have connecting edges created */
    if (!BLI_stack_is_empty(edges_loose)) {
      GHash *face_edge_map = BLI_ghash_ptr_new(__func__);

      MemArena *mem_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

      BM_mesh_elem_index_ensure(bm, BM_FACE);

      {
        BMBVHTree *bmbvh = BKE_bmbvh_new(bm, em->looptris, BMBVH_RESPECT_SELECT, nullptr, false);

        BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
          BM_elem_index_set(e, -1); /* set_dirty */
        }

        while (!BLI_stack_is_empty(edges_loose)) {
          BLI_stack_pop(edges_loose, &e);
          float e_center[3];
          mid_v3_v3v3(e_center, e->v1->co, e->v2->co);

          BMFace *f = BKE_bmbvh_find_face_closest(bmbvh, e_center, FLT_MAX);
          if (f) {
            ghash_insert_face_edge_link(face_edge_map, f, e, mem_arena);
            BM_elem_index_set(e, BM_elem_index_get(f)); /* set_dirty */
          }
        }

        BKE_bmbvh_free(bmbvh);
      }

      bm->elem_index_dirty |= BM_EDGE;

      BM_mesh_elem_table_ensure(bm, BM_FACE);

      /* detect edges chains that span faces
       * and splice vertices into the closest edges */
      {
        GHashIterator gh_iter;

        GHASH_ITER (gh_iter, face_edge_map) {
          BMFace *f = static_cast<BMFace *>(BLI_ghashIterator_getKey(&gh_iter));
          LinkBase *e_ls_base = static_cast<LinkBase *>(BLI_ghashIterator_getValue(&gh_iter));
          LinkNode *e_link = e_ls_base->list;

          do {
            e = static_cast<BMEdge *>(e_link->link);

            for (int j = 0; j < 2; j++) {
              BMVert *v_pivot = (&e->v1)[j];
              /* checking that \a v_pivot isn't in the face prevents attempting
               * to splice the same vertex into an edge from multiple faces */
              if (!BM_vert_in_face(v_pivot, f)) {
                float v_pivot_co[3];
                float v_pivot_fac;
                BMEdge *e_split = bm_face_split_edge_find(
                    e, f, v_pivot, bm->ftable, bm->totface, v_pivot_co, &v_pivot_fac);

                if (e_split) {
                  /* for degenerate cases this vertex may be in one
                   * of this edges radial faces */
                  if (!bm_vert_in_faces_radial(v_pivot, e_split, f)) {
                    BMEdge *e_new;
                    BMVert *v_new = BM_edge_split(bm, e_split, e_split->v1, &e_new, v_pivot_fac);
                    if (v_new) {
                      /* we _know_ these don't share an edge */
                      BM_vert_splice(bm, v_pivot, v_new);
                      BM_elem_index_set(e_new, BM_elem_index_get(e_split));
                    }
                  }
                }
              }
            }

          } while ((e_link = e_link->next));
        }
      }

      {
        MemArena *mem_arena_edgenet = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

        GHashIterator gh_iter;

        GHASH_ITER (gh_iter, face_edge_map) {
          BMFace *f = static_cast<BMFace *>(BLI_ghashIterator_getKey(&gh_iter));
          LinkBase *e_ls_base = static_cast<LinkBase *>(BLI_ghashIterator_getValue(&gh_iter));

          bm_face_split_by_edges_island_connect(
              bm, f, e_ls_base->list, e_ls_base->list_len, mem_arena_edgenet);

          BLI_memarena_clear(mem_arena_edgenet);
        }

        BLI_memarena_free(mem_arena_edgenet);
      }

      BLI_memarena_free(mem_arena);

      BLI_ghash_free(face_edge_map, nullptr, nullptr);

      EDBMUpdate_Params params{};
      params.calc_looptris = true;
      params.calc_normals = true;
      params.is_destructive = true;
      EDBM_update(static_cast<Mesh *>(obedit->data), &params);
    }

    BLI_stack_free(edges_loose);
#endif /* USE_NET_ISLAND_CONNECT */
  }
  return OPERATOR_FINISHED;
}

void MESH_OT_face_split_by_edges(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Weld Edges into Faces";
  ot->description = "Weld loose edges into faces (splitting them into new faces)";
  ot->idname = "MESH_OT_face_split_by_edges";

  /* API callbacks. */
  ot->exec = edbm_face_split_by_edges_exec;
  ot->poll = ED_operator_editmesh;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */
