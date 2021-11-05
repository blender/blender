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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup edlattice
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BKE_context.h"
#include "BKE_lattice.h"
#include "BKE_layer.h"
#include "BKE_report.h"

#include "ED_lattice.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_view3d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "DEG_depsgraph.h"

#include "lattice_intern.h"

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

bool ED_lattice_deselect_all_multi_ex(struct Base **bases, const uint bases_len)
{
  bool changed_multi = false;
  for (uint base_index = 0; base_index < bases_len; base_index++) {
    Base *base_iter = bases[base_index];
    Object *ob_iter = base_iter->object;
    changed_multi |= ED_lattice_flags_set(ob_iter, 0);
    DEG_id_tag_update(ob_iter->data, ID_RECALC_SELECT);
  }
  return changed_multi;
}

bool ED_lattice_deselect_all_multi(struct bContext *C)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc;
  ED_view3d_viewcontext_init(C, &vc, depsgraph);
  uint bases_len = 0;
  Base **bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
      vc.view_layer, vc.v3d, &bases_len);
  bool changed_multi = ED_lattice_deselect_all_multi_ex(bases, bases_len);
  MEM_freeN(bases);
  return changed_multi;
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

  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    Lattice *lt = ((Lattice *)obedit->data)->editlatt->latt;
    int seed_iter = seed;

    /* This gives a consistent result regardless of object order. */
    if (ob_index) {
      seed_iter += BLI_ghashutil_strhash_p(obedit->id.name);
    }

    int a = lt->pntsu * lt->pntsv * lt->pntsw;
    int elem_map_len = 0;
    BPoint **elem_map = MEM_mallocN(sizeof(*elem_map) * a, __func__);
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

    DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }
  MEM_freeN(objects);

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

  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    Lattice *lt = ((Lattice *)obedit->data)->editlatt->latt;

    for (int axis = 0; axis < 3; axis++) {
      if ((1 << axis) & axis_flag) {
        ed_lattice_select_mirrored(lt, axis, extend);
      }
    }

    /* TODO: only notify changes. */
    DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }
  MEM_freeN(objects);

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
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len;
  bool changed = false;

  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
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
                lattice_test_bitmap_uvw(lt, selpoints, u, v, w - 1, select)) {
              SET_FLAG_FROM_TEST(bp->f1, select, SELECT);
            }
          }
          bp++;
        }
      }
    }

    MEM_freeN(selpoints);

    changed = true;
    DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }
  MEM_freeN(objects);

  return changed ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static int lattice_select_more_exec(bContext *C, wmOperator *UNUSED(op))
{
  return lattice_select_more_less(C, true);
}

static int lattice_select_less_exec(bContext *C, wmOperator *UNUSED(op))
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
  Lattice *lt = obedit->data;
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
  ViewLayer *view_layer = CTX_data_view_layer(C);
  int action = RNA_enum_get(op->ptr, "action");

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);

  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;
    for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
      Object *obedit = objects[ob_index];
      Lattice *lt = obedit->data;
      if (BKE_lattice_is_any_selected(lt->editlatt->latt)) {
        action = SEL_DESELECT;
        break;
      }
    }
  }

  bool changed_multi = false;
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
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
        lt = obedit->data;
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
      DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
    }
  }
  MEM_freeN(objects);

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
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len;
  const bool is_extend = RNA_boolean_get(op->ptr, "extend");
  bool changed = false;

  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    Lattice *lt = ((Lattice *)obedit->data)->editlatt->latt;
    MDeformVert *dv;
    BPoint *bp;
    int a, tot;

    if (BLI_listbase_is_empty(&lt->vertex_group_names) || lt->dvert == NULL) {
      continue;
    }

    if (!is_extend) {
      ED_lattice_flags_set(obedit, 0);
    }

    dv = lt->dvert;
    tot = lt->pntsu * lt->pntsv * lt->pntsw;

    for (a = 0, bp = lt->def; a < tot; a++, bp++, dv++) {
      if (bp->hide == 0) {
        if (dv->dw == NULL) {
          bp->f1 |= SELECT;
        }
      }
    }

    changed = true;
    DEG_id_tag_update(obedit->data, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, obedit->data);
  }
  MEM_freeN(objects);

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

static void findnearestLattvert__doClosest(void *userData, BPoint *bp, const float screen_co[2])
{
  struct {
    BPoint *bp;
    float dist;
    int select;
    float mval_fl[2];
    bool is_changed;
  } *data = userData;
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

static BPoint *findnearestLattvert(ViewContext *vc, int sel, Base **r_base)
{
  /* (sel == 1): selected gets a disadvantage */
  /* in nurb and bezt or bp the nearest is written */
  /* return 0 1 2: handlepunt */
  struct {
    BPoint *bp;
    float dist;
    int select;
    float mval_fl[2];
    bool is_changed;
  } data = {NULL};

  data.dist = ED_view3d_select_dist_px();
  data.select = sel;
  data.mval_fl[0] = vc->mval[0];
  data.mval_fl[1] = vc->mval[1];

  uint bases_len;
  Base **bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
      vc->view_layer, vc->v3d, &bases_len);
  for (uint base_index = 0; base_index < bases_len; base_index++) {
    Base *base = bases[base_index];
    data.is_changed = false;

    ED_view3d_viewcontext_init_object(vc, base->object);
    ED_view3d_init_mats_rv3d(base->object, vc->rv3d);
    lattice_foreachScreenVert(
        vc, findnearestLattvert__doClosest, &data, V3D_PROJ_TEST_CLIP_DEFAULT);

    if (data.is_changed) {
      *r_base = base;
    }
  }
  MEM_freeN(bases);
  return data.bp;
}

bool ED_lattice_select_pick(
    bContext *C, const int mval[2], bool extend, bool deselect, bool toggle)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc;
  BPoint *bp = NULL;
  Base *basact = NULL;

  ED_view3d_viewcontext_init(C, &vc, depsgraph);
  vc.mval[0] = mval[0];
  vc.mval[1] = mval[1];

  bp = findnearestLattvert(&vc, true, &basact);
  if (bp) {
    ED_view3d_viewcontext_init_object(&vc, basact->object);
    Lattice *lt = ((Lattice *)vc.obedit->data)->editlatt->latt;

    if (!extend && !deselect && !toggle) {
      uint objects_len = 0;
      Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
          vc.view_layer, vc.v3d, &objects_len);
      for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
        Object *ob = objects[ob_index];
        if (ED_lattice_flags_set(ob, 0)) {
          DEG_id_tag_update(ob->data, ID_RECALC_SELECT);
          WM_event_add_notifier(C, NC_GEOM | ND_SELECT, ob->data);
        }
      }
      MEM_freeN(objects);
    }

    if (extend) {
      bp->f1 |= SELECT;
    }
    else if (deselect) {
      bp->f1 &= ~SELECT;
    }
    else if (toggle) {
      bp->f1 ^= SELECT; /* swap */
    }
    else {
      ED_lattice_flags_set(vc.obedit, 0);
      bp->f1 |= SELECT;
    }

    if (bp->f1 & SELECT) {
      lt->actbp = bp - lt->def;
    }
    else {
      lt->actbp = LT_ACTBP_NONE;
    }

    if (vc.view_layer->basact != basact) {
      ED_object_base_activate(C, basact);
    }

    DEG_id_tag_update(vc.obedit->data, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, vc.obedit->data);

    return true;
  }

  return false;
}

/** \} */
