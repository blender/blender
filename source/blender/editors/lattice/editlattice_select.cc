/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edlattice
 */

#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "BKE_context.hh"
#include "BKE_lattice.hh"
#include "BKE_layer.hh"
#include "BKE_report.hh"

#include "ED_lattice.hh"
#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"
#include "ED_view3d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "DEG_depsgraph.hh"

#include "lattice_intern.h"

using blender::Span;
using blender::Vector;

/* -------------------------------------------------------------------- */
/** \name Utility Functions
 * \{ */

static void bpoint_select_set(BPoint *bp, bool select)
{
  if (select) {
    if (!bp->hide) {
      bp->f1 |= SELECT;
    }
  }
  else {
    bp->f1 &= ~SELECT;
  }
}

static bool lattice_deselect_all_multi(const Span<Base *> bases)
{
  bool changed_multi = false;
  for (Base *base : bases) {
    Object *ob_iter = base->object;
    changed_multi |= ED_lattice_flags_set(ob_iter, 0);
    DEG_id_tag_update(static_cast<ID *>(ob_iter->data), ID_RECALC_SELECT);
  }
  return changed_multi;
}

bool ED_lattice_deselect_all_multi(bContext *C)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);
  Vector<Base *> bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
      vc.scene, vc.view_layer, vc.v3d);
  return lattice_deselect_all_multi(bases);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Random Operator
 * \{ */

static int lattice_select_random_exec(bContext *C, wmOperator *op)
{
  const bool select = (RNA_enum_get(op->ptr, "action") == SEL_SELECT);
  const float randfac = RNA_float_get(op->ptr, "ratio");
  const int seed = WM_operator_properties_select_random_seed_increment_get(op);

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (const int ob_index : objects.index_range()) {
    Object *obedit = objects[ob_index];
    Lattice *lt = ((Lattice *)obedit->data)->editlatt->latt;
    int seed_iter = seed;

    /* This gives a consistent result regardless of object order. */
    if (ob_index) {
      seed_iter += BLI_ghashutil_strhash_p(obedit->id.name);
    }

    int a = lt->pntsu * lt->pntsv * lt->pntsw;
    int elem_map_len = 0;
    BPoint **elem_map = static_cast<BPoint **>(MEM_mallocN(sizeof(*elem_map) * a, __func__));
    BPoint *bp = lt->def;

    while (a--) {
      if (!bp->hide) {
        elem_map[elem_map_len++] = bp;
      }
      bp++;
    }

    BLI_array_randomize(elem_map, sizeof(*elem_map), elem_map_len, seed_iter);
    const int count_select = elem_map_len * randfac;
    for (int i = 0; i < count_select; i++) {
      bpoint_select_set(elem_map[i], select);
    }
    MEM_freeN(elem_map);

    if (select == false) {
      lt->actbp = LT_ACTBP_NONE;
    }

    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }

  return OPERATOR_FINISHED;
}

void LATTICE_OT_select_random(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Random";
  ot->description = "Randomly select UVW control points";
  ot->idname = "LATTICE_OT_select_random";

  /* api callbacks */
  ot->exec = lattice_select_random_exec;
  ot->poll = ED_operator_editlattice;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  WM_operator_properties_select_random(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Mirror Operator
 * \{ */

static void ed_lattice_select_mirrored(Lattice *lt, const int axis, const bool extend)
{
  const int tot = lt->pntsu * lt->pntsv * lt->pntsw;

  bool flip_uvw[3] = {false};
  flip_uvw[axis] = true;

  /* we could flip this too */
  if (!extend) {
    lt->actbp = LT_ACTBP_NONE;
  }

  /* store "original" selection */
  BLI_bitmap *selpoints = BLI_BITMAP_NEW(tot, __func__);
  BKE_lattice_bitmap_from_flag(lt, selpoints, SELECT, false, false);

  /* actual (de)selection */
  for (int i = 0; i < tot; i++) {
    const int i_flip = BKE_lattice_index_flip(lt, i, flip_uvw[0], flip_uvw[1], flip_uvw[2]);
    BPoint *bp = &lt->def[i];
    if (!bp->hide) {
      if (BLI_BITMAP_TEST(selpoints, i_flip)) {
        bp->f1 |= SELECT;
      }
      else {
        if (!extend) {
          bp->f1 &= ~SELECT;
        }
      }
    }
  }

  MEM_freeN(selpoints);
}

static int lattice_select_mirror_exec(bContext *C, wmOperator *op)
{
  const int axis_flag = RNA_enum_get(op->ptr, "axis");
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  for (Object *obedit : objects) {
    Lattice *lt = ((Lattice *)obedit->data)->editlatt->latt;

    for (int axis = 0; axis < 3; axis++) {
      if ((1 << axis) & axis_flag) {
        ed_lattice_select_mirrored(lt, axis, extend);
      }
    }

    /* TODO: only notify changes. */
    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }

  return OPERATOR_FINISHED;
}

void LATTICE_OT_select_mirror(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Mirror";
  ot->description = "Select mirrored lattice points";
  ot->idname = "LATTICE_OT_select_mirror";

  /* api callbacks */
  ot->exec = lattice_select_mirror_exec;
  ot->poll = ED_operator_editlattice;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_enum_flag(ot->srna, "axis", rna_enum_axis_flag_xyz_items, (1 << 0), "Axis", "");

  RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select More/Less Operator
 * \{ */

static bool lattice_test_bitmap_uvw(
    Lattice *lt, const BLI_bitmap *selpoints, int u, int v, int w, const bool selected)
{
  if ((u < 0 || u >= lt->pntsu) || (v < 0 || v >= lt->pntsv) || (w < 0 || w >= lt->pntsw)) {
    return false;
  }

  int i = BKE_lattice_index_from_uvw(lt, u, v, w);
  if (lt->def[i].hide == 0) {
    return (BLI_BITMAP_TEST(selpoints, i) != 0) == selected;
  }
  return false;
}

static int lattice_select_more_less(bContext *C, const bool select)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  bool changed = false;

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    Lattice *lt = ((Lattice *)obedit->data)->editlatt->latt;
    BPoint *bp;
    const int tot = lt->pntsu * lt->pntsv * lt->pntsw;
    int u, v, w;
    BLI_bitmap *selpoints;

    lt->actbp = LT_ACTBP_NONE;

    selpoints = BLI_BITMAP_NEW(tot, __func__);
    BKE_lattice_bitmap_from_flag(lt, selpoints, SELECT, false, false);

    bp = lt->def;
    for (w = 0; w < lt->pntsw; w++) {
      for (v = 0; v < lt->pntsv; v++) {
        for (u = 0; u < lt->pntsu; u++) {
          if ((bp->hide == 0) && (((bp->f1 & SELECT) == 0) == select)) {
            if (lattice_test_bitmap_uvw(lt, selpoints, u + 1, v, w, select) ||
                lattice_test_bitmap_uvw(lt, selpoints, u - 1, v, w, select) ||
                lattice_test_bitmap_uvw(lt, selpoints, u, v + 1, w, select) ||
                lattice_test_bitmap_uvw(lt, selpoints, u, v - 1, w, select) ||
                lattice_test_bitmap_uvw(lt, selpoints, u, v, w + 1, select) ||
                lattice_test_bitmap_uvw(lt, selpoints, u, v, w - 1, select))
            {
              SET_FLAG_FROM_TEST(bp->f1, select, SELECT);
            }
          }
          bp++;
        }
      }
    }

    MEM_freeN(selpoints);

    changed = true;
    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }

  return changed ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static int lattice_select_more_exec(bContext *C, wmOperator * /*op*/)
{
  return lattice_select_more_less(C, true);
}

static int lattice_select_less_exec(bContext *C, wmOperator * /*op*/)
{
  return lattice_select_more_less(C, false);
}

void LATTICE_OT_select_more(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select More";
  ot->description = "Select vertex directly linked to already selected ones";
  ot->idname = "LATTICE_OT_select_more";

  /* api callbacks */
  ot->exec = lattice_select_more_exec;
  ot->poll = ED_operator_editlattice;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void LATTICE_OT_select_less(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Less";
  ot->description = "Deselect vertices at the boundary of each selection region";
  ot->idname = "LATTICE_OT_select_less";

  /* api callbacks */
  ot->exec = lattice_select_less_exec;
  ot->poll = ED_operator_editlattice;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select All Operator
 * \{ */

bool ED_lattice_flags_set(Object *obedit, int flag)
{
  Lattice *lt = static_cast<Lattice *>(obedit->data);
  BPoint *bp;
  int a;
  bool changed = false;

  bp = lt->editlatt->latt->def;

  a = lt->editlatt->latt->pntsu * lt->editlatt->latt->pntsv * lt->editlatt->latt->pntsw;

  if (lt->editlatt->latt->actbp != LT_ACTBP_NONE) {
    lt->editlatt->latt->actbp = LT_ACTBP_NONE;
    changed = true;
  }

  while (a--) {
    if (bp->hide == 0) {
      if (bp->f1 != flag) {
        bp->f1 = flag;
        changed = true;
      }
    }
    bp++;
  }
  return changed;
}

static int lattice_select_all_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  int action = RNA_enum_get(op->ptr, "action");

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));

  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;
    for (Object *obedit : objects) {
      Lattice *lt = static_cast<Lattice *>(obedit->data);
      if (BKE_lattice_is_any_selected(lt->editlatt->latt)) {
        action = SEL_DESELECT;
        break;
      }
    }
  }

  bool changed_multi = false;
  for (Object *obedit : objects) {
    Lattice *lt;
    BPoint *bp;
    int a;
    bool changed = false;

    switch (action) {
      case SEL_SELECT:
        changed = ED_lattice_flags_set(obedit, 1);
        break;
      case SEL_DESELECT:
        changed = ED_lattice_flags_set(obedit, 0);
        break;
      case SEL_INVERT:
        lt = static_cast<Lattice *>(obedit->data);
        bp = lt->editlatt->latt->def;
        a = lt->editlatt->latt->pntsu * lt->editlatt->latt->pntsv * lt->editlatt->latt->pntsw;
        lt->editlatt->latt->actbp = LT_ACTBP_NONE;

        while (a--) {
          if (bp->hide == 0) {
            bp->f1 ^= SELECT;
            changed = true;
          }
          bp++;
        }
        break;
    }
    if (changed) {
      changed_multi = true;
      DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
    }
  }

  if (changed_multi) {
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void LATTICE_OT_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "(De)select All";
  ot->description = "Change selection of all UVW control points";
  ot->idname = "LATTICE_OT_select_all";

  /* api callbacks */
  ot->exec = lattice_select_all_exec;
  ot->poll = ED_operator_editlattice;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Ungrouped Verts Operator
 * \{ */

static int lattice_select_ungrouped_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool is_extend = RNA_boolean_get(op->ptr, "extend");
  bool changed = false;

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C));
  for (Object *obedit : objects) {
    Lattice *lt = ((Lattice *)obedit->data)->editlatt->latt;
    MDeformVert *dv;
    BPoint *bp;
    int a, tot;

    if (BLI_listbase_is_empty(&lt->vertex_group_names) || lt->dvert == nullptr) {
      continue;
    }

    if (!is_extend) {
      ED_lattice_flags_set(obedit, 0);
    }

    dv = lt->dvert;
    tot = lt->pntsu * lt->pntsv * lt->pntsw;

    for (a = 0, bp = lt->def; a < tot; a++, bp++, dv++) {
      if (bp->hide == 0) {
        if (dv->dw == nullptr) {
          bp->f1 |= SELECT;
        }
      }
    }

    changed = true;
    DEG_id_tag_update(static_cast<ID *>(obedit->data), ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }

  if (!changed) {
    BKE_report(op->reports, RPT_ERROR, "No weights/vertex groups on object(s)");
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

void LATTICE_OT_select_ungrouped(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Ungrouped";
  ot->idname = "LATTICE_OT_select_ungrouped";
  ot->description = "Select vertices without a group";

  /* api callbacks */
  ot->exec = lattice_select_ungrouped_exec;
  ot->poll = ED_operator_editlattice;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Picking API
 *
 * Here actual select happens,
 * Gets called via generic mouse select operator.
 * \{ */

struct NearestLatticeVert_UserData {
  BPoint *bp;
  float dist;
  /** When true, the existing selection gets a disadvantage. */
  bool select;
  float mval_fl[2];
  bool is_changed;
};

static void findnearestLattvert__doClosest(void *user_data, BPoint *bp, const float screen_co[2])
{
  NearestLatticeVert_UserData *data = static_cast<NearestLatticeVert_UserData *>(user_data);
  float dist_test = len_manhattan_v2v2(data->mval_fl, screen_co);

  if ((bp->f1 & SELECT) && data->select) {
    dist_test += 5.0f;
  }

  if (dist_test < data->dist) {
    data->dist = dist_test;
    data->bp = bp;
    data->is_changed = true;
  }
}

static BPoint *findnearestLattvert(ViewContext *vc, bool select, Base **r_base)
{
  NearestLatticeVert_UserData data = {nullptr};

  data.dist = ED_view3d_select_dist_px();
  data.select = select;
  data.mval_fl[0] = vc->mval[0];
  data.mval_fl[1] = vc->mval[1];

  Vector<Base *> bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
      vc->scene, vc->view_layer, vc->v3d);
  for (Base *base : bases) {
    data.is_changed = false;

    ED_view3d_viewcontext_init_object(vc, base->object);
    ED_view3d_init_mats_rv3d(base->object, vc->rv3d);
    lattice_foreachScreenVert(
        vc, findnearestLattvert__doClosest, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

    if (data.is_changed) {
      *r_base = base;
    }
  }
  return data.bp;
}

bool ED_lattice_select_pick(bContext *C, const int mval[2], const SelectPick_Params *params)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  BPoint *bp = nullptr;
  Base *basact = nullptr;
  bool changed = false;

  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);
  vc.mval[0] = mval[0];
  vc.mval[1] = mval[1];

  bp = findnearestLattvert(&vc, true, &basact);
  bool found = (bp != nullptr);

  if (params->sel_op == SEL_OP_SET) {
    if ((found && params->select_passthrough) && (bp->f1 & SELECT)) {
      found = false;
    }
    else if (found || params->deselect_all) {
      /* Deselect everything. */
      Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
          vc.scene, vc.view_layer, vc.v3d);
      for (Object *ob : objects) {
        if (ED_lattice_flags_set(ob, 0)) {
          DEG_id_tag_update(static_cast<ID *>(ob->data), ID_RECALC_SELECT);
          WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);
        }
      }
      changed = true;
    }
  }

  if (found) {
    ED_view3d_viewcontext_init_object(&vc, basact->object);
    Lattice *lt = ((Lattice *)vc.obedit->data)->editlatt->latt;

    switch (params->sel_op) {
      case SEL_OP_ADD: {
        bp->f1 |= SELECT;
        break;
      }
      case SEL_OP_SUB: {
        bp->f1 &= ~SELECT;
        break;
      }
      case SEL_OP_XOR: {
        bp->f1 ^= SELECT; /* swap */
        break;
      }
      case SEL_OP_SET: {
        bp->f1 |= SELECT;
        break;
      }
      case SEL_OP_AND: {
        BLI_assert_unreachable(); /* Doesn't make sense for picking. */
        break;
      }
    }

    if (bp->f1 & SELECT) {
      lt->actbp = bp - lt->def;
    }
    else {
      lt->actbp = LT_ACTBP_NONE;
    }

    BKE_view_layer_synced_ensure(vc.scene, vc.view_layer);
    if (BKE_view_layer_active_base_get(vc.view_layer) != basact) {
      ED_object_base_activate(C, basact);
    }

    DEG_id_tag_update(static_cast<ID *>(vc.obedit->data), ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc.obedit->data);

    changed = true;
  }

  return changed || found;
}

/** \} */
