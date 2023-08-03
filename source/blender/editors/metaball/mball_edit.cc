/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmeta
 */

#include <cmath>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_kdtree.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "DNA_defs.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_mball.h"
#include "BKE_object.h"

#include "DEG_depsgraph.h"

#include "GPU_select.h"

#include "ED_mball.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_view3d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "mball_intern.h"

/* -------------------------------------------------------------------- */
/** \name Edit Mode Functions
 * \{ */

void ED_mball_editmball_free(Object *obedit)
{
  MetaBall *mb = (MetaBall *)obedit->data;

  mb->editelems = nullptr;
  mb->lastelem = nullptr;
}

void ED_mball_editmball_make(Object *obedit)
{
  MetaBall *mb = (MetaBall *)obedit->data;
  MetaElem *ml; /*, *newml;*/

  ml = static_cast<MetaElem *>(mb->elems.first);

  while (ml) {
    if (ml->flag & SELECT) {
      mb->lastelem = ml;
    }
    ml = ml->next;
  }

  mb->editelems = &mb->elems;
}

void ED_mball_editmball_load(Object * /*obedit*/) {}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Selection
 * \{ */

bool ED_mball_deselect_all_multi(bContext *C)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc;
  ED_view3d_viewcontext_init(C, &vc, depsgraph);
  uint bases_len = 0;
  Base **bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
      vc.scene, vc.view_layer, vc.v3d, &bases_len);
  bool changed_multi = BKE_mball_deselect_all_multi_ex(bases, bases_len);
  MEM_freeN(bases);
  return changed_multi;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Meta Primitive Utility
 * \{ */

MetaElem *ED_mball_add_primitive(
    bContext * /*C*/, Object *obedit, bool obedit_is_new, float mat[4][4], float dia, int type)
{
  MetaBall *mball = (MetaBall *)obedit->data;
  MetaElem *ml;

  /* Deselect all existing metaelems */
  ml = static_cast<MetaElem *>(mball->editelems->first);
  while (ml) {
    ml->flag &= ~SELECT;
    ml = ml->next;
  }

  ml = BKE_mball_element_add(mball, type);
  ml->rad *= dia;

  if (obedit_is_new) {
    mball->wiresize *= dia;
    mball->rendersize *= dia;
  }
  copy_v3_v3(&ml->x, mat[3]);
  /* MB_ELIPSOID works differently (intentional?). Whatever the case,
   * on testing this needs to be skipped otherwise it doesn't behave like other types. */
  if (type != MB_ELIPSOID) {
    mul_v3_fl(&ml->expx, dia);
  }

  ml->flag |= SELECT;
  mball->lastelem = ml;
  return ml;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select/Deselect Operator
 * \{ */

/* Select or deselect all MetaElements */
static int mball_select_all_exec(bContext *C, wmOperator *op)
{
  int action = RNA_enum_get(op->ptr, "action");

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint bases_len = 0;
  Base **bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C), &bases_len);

  if (action == SEL_TOGGLE) {
    action = BKE_mball_is_any_selected_multi(bases, bases_len) ? SEL_DESELECT : SEL_SELECT;
  }

  switch (action) {
    case SEL_SELECT:
      BKE_mball_select_all_multi_ex(bases, bases_len);
      break;
    case SEL_DESELECT:
      BKE_mball_deselect_all_multi_ex(bases, bases_len);
      break;
    case SEL_INVERT:
      BKE_mball_select_swap_multi_ex(bases, bases_len);
      break;
  }

  for (uint base_index = 0; base_index < bases_len; base_index++) {
    Object *obedit = bases[base_index]->object;
    MetaBall *mb = (MetaBall *)obedit->data;
    DEG_id_tag_update(&mb->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, mb);
  }

  MEM_freeN(bases);

  return OPERATOR_FINISHED;
}

void MBALL_OT_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "(De)select All";
  ot->description = "Change selection of all metaball elements";
  ot->idname = "MBALL_OT_select_all";

  /* callback functions */
  ot->exec = mball_select_all_exec;
  ot->poll = ED_operator_editmball;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Similar Operator
 * \{ */

enum {
  SIMMBALL_TYPE = 1,
  SIMMBALL_RADIUS,
  SIMMBALL_STIFFNESS,
  SIMMBALL_ROTATION,
};

static const EnumPropertyItem prop_similar_types[] = {
    {SIMMBALL_TYPE, "TYPE", 0, "Type", ""},
    {SIMMBALL_RADIUS, "RADIUS", 0, "Radius", ""},
    {SIMMBALL_STIFFNESS, "STIFFNESS", 0, "Stiffness", ""},
    {SIMMBALL_ROTATION, "ROTATION", 0, "Rotation", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void mball_select_similar_type_get(
    Object *obedit, MetaBall *mb, int type, KDTree_1d *tree_1d, KDTree_3d *tree_3d)
{
  float tree_entry[3] = {0.0f, 0.0f, 0.0f};
  int tree_index = 0;
  LISTBASE_FOREACH (MetaElem *, ml, mb->editelems) {
    if (ml->flag & SELECT) {
      switch (type) {
        case SIMMBALL_RADIUS: {
          float radius = ml->rad;
          /* Radius in world space. */
          float smat[3][3];
          float radius_vec[3] = {radius, radius, radius};
          BKE_object_scale_to_mat3(obedit, smat);
          mul_m3_v3(smat, radius_vec);
          radius = (radius_vec[0] + radius_vec[1] + radius_vec[2]) / 3;
          tree_entry[0] = radius;
          break;
        }
        case SIMMBALL_STIFFNESS: {
          tree_entry[0] = ml->s;
          break;
        } break;
        case SIMMBALL_ROTATION: {
          float dir[3] = {1.0f, 0.0f, 0.0f};
          float rmat[3][3];
          mul_qt_v3(ml->quat, dir);
          BKE_object_rot_to_mat3(obedit, rmat, true);
          mul_m3_v3(rmat, dir);
          copy_v3_v3(tree_entry, dir);
          break;
        }
      }
      if (tree_1d) {
        BLI_kdtree_1d_insert(tree_1d, tree_index++, tree_entry);
      }
      else {
        BLI_kdtree_3d_insert(tree_3d, tree_index++, tree_entry);
      }
    }
  }
}

static bool mball_select_similar_type(Object *obedit,
                                      MetaBall *mb,
                                      int type,
                                      const KDTree_1d *tree_1d,
                                      const KDTree_3d *tree_3d,
                                      const float thresh)
{
  bool changed = false;
  LISTBASE_FOREACH (MetaElem *, ml, mb->editelems) {
    bool select = false;
    switch (type) {
      case SIMMBALL_RADIUS: {
        float radius = ml->rad;
        /* Radius in world space is the average of the
         * scaled radius in x, y and z directions. */
        float smat[3][3];
        float radius_vec[3] = {radius, radius, radius};
        BKE_object_scale_to_mat3(obedit, smat);
        mul_m3_v3(smat, radius_vec);
        radius = (radius_vec[0] + radius_vec[1] + radius_vec[2]) / 3;

        if (ED_select_similar_compare_float_tree(tree_1d, radius, thresh, SIM_CMP_EQ)) {
          select = true;
        }
        break;
      }
      case SIMMBALL_STIFFNESS: {
        float s = ml->s;
        if (ED_select_similar_compare_float_tree(tree_1d, s, thresh, SIM_CMP_EQ)) {
          select = true;
        }
        break;
      }
      case SIMMBALL_ROTATION: {
        float dir[3] = {1.0f, 0.0f, 0.0f};
        float rmat[3][3];
        mul_qt_v3(ml->quat, dir);
        BKE_object_rot_to_mat3(obedit, rmat, true);
        mul_m3_v3(rmat, dir);

        float thresh_cos = cosf(thresh * float(M_PI_2));

        KDTreeNearest_3d nearest;
        if (BLI_kdtree_3d_find_nearest(tree_3d, dir, &nearest) != -1) {
          float orient = angle_normalized_v3v3(dir, nearest.co);
          /* Map to 0-1 to compare orientation. */
          float delta = thresh_cos - fabsf(cosf(orient));
          if (ED_select_similar_compare_float(delta, thresh, SIM_CMP_EQ)) {
            select = true;
          }
        }
        break;
      }
    }

    if (select) {
      changed = true;
      ml->flag |= SELECT;
    }
  }
  return changed;
}

static int mball_select_similar_exec(bContext *C, wmOperator *op)
{
  const int type = RNA_enum_get(op->ptr, "type");
  const float thresh = RNA_float_get(op->ptr, "threshold");
  int tot_mball_selected_all = 0;

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint bases_len = 0;
  Base **bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C), &bases_len);

  tot_mball_selected_all = BKE_mball_select_count_multi(bases, bases_len);

  short type_ref = 0;
  KDTree_1d *tree_1d = nullptr;
  KDTree_3d *tree_3d = nullptr;

  switch (type) {
    case SIMMBALL_RADIUS:
    case SIMMBALL_STIFFNESS:
      tree_1d = BLI_kdtree_1d_new(tot_mball_selected_all);
      break;
    case SIMMBALL_ROTATION:
      tree_3d = BLI_kdtree_3d_new(tot_mball_selected_all);
      break;
  }

  /* Get type of selected MetaBall */
  for (uint base_index = 0; base_index < bases_len; base_index++) {
    Object *obedit = bases[base_index]->object;
    MetaBall *mb = (MetaBall *)obedit->data;

    switch (type) {
      case SIMMBALL_TYPE: {
        LISTBASE_FOREACH (MetaElem *, ml, mb->editelems) {
          if (ml->flag & SELECT) {
            short mball_type = 1 << (ml->type + 1);
            type_ref |= mball_type;
          }
        }
        break;
      }
      case SIMMBALL_RADIUS:
      case SIMMBALL_STIFFNESS:
      case SIMMBALL_ROTATION:
        mball_select_similar_type_get(obedit, mb, type, tree_1d, tree_3d);
        break;
      default:
        BLI_assert(0);
        break;
    }
  }

  if (tree_1d != nullptr) {
    BLI_kdtree_1d_deduplicate(tree_1d);
    BLI_kdtree_1d_balance(tree_1d);
  }
  if (tree_3d != nullptr) {
    BLI_kdtree_3d_deduplicate(tree_3d);
    BLI_kdtree_3d_balance(tree_3d);
  }
  /* Select MetaBalls with desired type. */
  for (uint base_index = 0; base_index < bases_len; base_index++) {
    Object *obedit = bases[base_index]->object;
    MetaBall *mb = (MetaBall *)obedit->data;
    bool changed = false;

    switch (type) {
      case SIMMBALL_TYPE: {
        LISTBASE_FOREACH (MetaElem *, ml, mb->editelems) {
          short mball_type = 1 << (ml->type + 1);
          if (mball_type & type_ref) {
            ml->flag |= SELECT;
            changed = true;
          }
        }
        break;
      }
      case SIMMBALL_RADIUS:
      case SIMMBALL_STIFFNESS:
      case SIMMBALL_ROTATION:
        changed = mball_select_similar_type(obedit, mb, type, tree_1d, tree_3d, thresh);
        break;
      default:
        BLI_assert(0);
        break;
    }

    if (changed) {
      DEG_id_tag_update(&mb->id, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_GEOM | ND_SELECT, mb);
    }
  }

  MEM_freeN(bases);
  if (tree_1d != nullptr) {
    BLI_kdtree_1d_free(tree_1d);
  }
  if (tree_3d != nullptr) {
    BLI_kdtree_3d_free(tree_3d);
  }
  return OPERATOR_FINISHED;
}

void MBALL_OT_select_similar(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Similar";
  ot->idname = "MBALL_OT_select_similar";

  /* callback functions */
  ot->invoke = WM_menu_invoke;
  ot->exec = mball_select_similar_exec;
  ot->poll = ED_operator_editmball;
  ot->description = "Select similar metaballs by property types";

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", prop_similar_types, 0, "Type", "");

  RNA_def_float(ot->srna, "threshold", 0.1, 0.0, FLT_MAX, "Threshold", "", 0.01, 1.0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Random Operator
 * \{ */

static int select_random_metaelems_exec(bContext *C, wmOperator *op)
{
  const bool select = (RNA_enum_get(op->ptr, "action") == SEL_SELECT);
  const float randfac = RNA_float_get(op->ptr, "ratio");
  const int seed = WM_operator_properties_select_random_seed_increment_get(op);

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    MetaBall *mb = (MetaBall *)obedit->data;
    if (!BKE_mball_is_any_unselected(mb)) {
      continue;
    }
    int seed_iter = seed;

    /* This gives a consistent result regardless of object order. */
    if (ob_index) {
      seed_iter += BLI_ghashutil_strhash_p(obedit->id.name);
    }

    RNG *rng = BLI_rng_new_srandom(seed_iter);

    LISTBASE_FOREACH (MetaElem *, ml, mb->editelems) {
      if (BLI_rng_get_float(rng) < randfac) {
        if (select) {
          ml->flag |= SELECT;
        }
        else {
          ml->flag &= ~SELECT;
        }
      }
    }

    BLI_rng_free(rng);

    DEG_id_tag_update(&mb->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, mb);
  }
  MEM_freeN(objects);
  return OPERATOR_FINISHED;
}

void MBALL_OT_select_random_metaelems(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Random";
  ot->description = "Randomly select metaball elements";
  ot->idname = "MBALL_OT_select_random_metaelems";

  /* callback functions */
  ot->exec = select_random_metaelems_exec;
  ot->poll = ED_operator_editmball;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_select_random(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Meta-Ball Operator
 * \{ */

/* Duplicate selected MetaElements */
static int duplicate_metaelems_exec(bContext *C, wmOperator * /*op*/)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    MetaBall *mb = (MetaBall *)obedit->data;
    MetaElem *ml, *newml;

    if (!BKE_mball_is_any_selected(mb)) {
      continue;
    }

    ml = static_cast<MetaElem *>(mb->editelems->last);
    if (ml) {
      while (ml) {
        if (ml->flag & SELECT) {
          newml = static_cast<MetaElem *>(MEM_dupallocN(ml));
          BLI_addtail(mb->editelems, newml);
          mb->lastelem = newml;
          ml->flag &= ~SELECT;
        }
        ml = ml->prev;
      }
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, mb);
      DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
    }
  }
  MEM_freeN(objects);
  return OPERATOR_FINISHED;
}

void MBALL_OT_duplicate_metaelems(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Duplicate Metaball Elements";
  ot->description = "Duplicate selected metaball element(s)";
  ot->idname = "MBALL_OT_duplicate_metaelems";

  /* callback functions */
  ot->exec = duplicate_metaelems_exec;
  ot->poll = ED_operator_editmball;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Meta-Ball Operator
 *
 * Delete all selected MetaElems (not MetaBall).
 * \{ */

static int delete_metaelems_exec(bContext *C, wmOperator * /*op*/)
{
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C), &objects_len);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    MetaBall *mb = (MetaBall *)obedit->data;
    MetaElem *ml, *next;

    if (!BKE_mball_is_any_selected(mb)) {
      continue;
    }

    ml = static_cast<MetaElem *>(mb->editelems->first);
    if (ml) {
      while (ml) {
        next = ml->next;
        if (ml->flag & SELECT) {
          if (mb->lastelem == ml) {
            mb->lastelem = nullptr;
          }
          BLI_remlink(mb->editelems, ml);
          MEM_freeN(ml);
        }
        ml = next;
      }
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, mb);
      DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
    }
  }
  MEM_freeN(objects);
  return OPERATOR_FINISHED;
}

void MBALL_OT_delete_metaelems(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete";
  ot->description = "Delete selected metaball element(s)";
  ot->idname = "MBALL_OT_delete_metaelems";

  /* callback functions */
  ot->invoke = WM_operator_confirm_or_exec;
  ot->exec = delete_metaelems_exec;
  ot->poll = ED_operator_editmball;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  WM_operator_properties_confirm_or_exec(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hide Meta-Elements Operator
 * \{ */

static int hide_metaelems_exec(bContext *C, wmOperator *op)
{
  Object *obedit = CTX_data_edit_object(C);
  MetaBall *mb = (MetaBall *)obedit->data;
  MetaElem *ml;
  const bool invert = RNA_boolean_get(op->ptr, "unselected") ? SELECT : false;

  ml = static_cast<MetaElem *>(mb->editelems->first);

  if (ml) {
    while (ml) {
      if ((ml->flag & SELECT) != invert) {
        ml->flag |= MB_HIDE;
      }
      ml = ml->next;
    }
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, mb);
    DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
  }

  return OPERATOR_FINISHED;
}

void MBALL_OT_hide_metaelems(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide Selected";
  ot->description = "Hide (un)selected metaball element(s)";
  ot->idname = "MBALL_OT_hide_metaelems";

  /* callback functions */
  ot->exec = hide_metaelems_exec;
  ot->poll = ED_operator_editmball;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_boolean(
      ot->srna, "unselected", false, "Unselected", "Hide unselected rather than selected");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Un-Hide Meta-Elements Operator
 * \{ */

static int reveal_metaelems_exec(bContext *C, wmOperator *op)
{
  Object *obedit = CTX_data_edit_object(C);
  MetaBall *mb = (MetaBall *)obedit->data;
  const bool select = RNA_boolean_get(op->ptr, "select");
  bool changed = false;

  LISTBASE_FOREACH (MetaElem *, ml, mb->editelems) {
    if (ml->flag & MB_HIDE) {
      SET_FLAG_FROM_TEST(ml->flag, select, SELECT);
      ml->flag &= ~MB_HIDE;
      changed = true;
    }
  }
  if (changed) {
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, mb);
    DEG_id_tag_update(static_cast<ID *>(obedit->data), 0);
  }

  return OPERATOR_FINISHED;
}

void MBALL_OT_reveal_metaelems(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reveal Hidden";
  ot->description = "Reveal all hidden metaball elements";
  ot->idname = "MBALL_OT_reveal_metaelems";

  /* callback functions */
  ot->exec = reveal_metaelems_exec;
  ot->poll = ED_operator_editmball;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_boolean(ot->srna, "select", true, "Select", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Pick Utility
 * \{ */

Base *ED_mball_base_and_elem_from_select_buffer(Base **bases,
                                                uint bases_len,
                                                const uint select_id,
                                                MetaElem **r_ml)
{
  const uint hit_object = select_id & 0xFFFF;
  Base *base = nullptr;
  MetaElem *ml = nullptr;
  /* TODO(@ideasman42): optimize, eg: sort & binary search. */
  for (uint base_index = 0; base_index < bases_len; base_index++) {
    if (bases[base_index]->object->runtime.select_id == hit_object) {
      base = bases[base_index];
      break;
    }
  }
  if (base != nullptr) {
    const uint hit_elem = (select_id & ~MBALLSEL_ANY) >> 16;
    MetaBall *mb = static_cast<MetaBall *>(base->object->data);
    ml = static_cast<MetaElem *>(BLI_findlink(mb->editelems, hit_elem));
  }
  *r_ml = ml;
  return base;
}

static bool ed_mball_findnearest_metaelem(bContext *C,
                                          const int mval[2],
                                          bool use_cycle,
                                          Base **r_base,
                                          MetaElem **r_ml,
                                          uint *r_selmask)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc;
  int a, hits;
  GPUSelectResult buffer[MAXPICKELEMS];
  rcti rect;
  bool found = false;

  ED_view3d_viewcontext_init(C, &vc, depsgraph);

  BLI_rcti_init_pt_radius(&rect, mval, 12);

  hits = view3d_opengl_select(&vc,
                              buffer,
                              ARRAY_SIZE(buffer),
                              &rect,
                              use_cycle ? VIEW3D_SELECT_PICK_ALL : VIEW3D_SELECT_PICK_NEAREST,
                              VIEW3D_SELECT_FILTER_NOP);

  if (hits == 0) {
    return false;
  }

  uint bases_len = 0;
  Base **bases = BKE_view_layer_array_from_bases_in_edit_mode(
      vc.scene, vc.view_layer, vc.v3d, &bases_len);

  int hit_cycle_offset = 0;
  if (use_cycle) {
    /* When cycling, use the hit directly after the current active meta-element (when set). */
    const int base_index = vc.obact->runtime.select_id;
    MetaBall *mb = (MetaBall *)vc.obact->data;
    MetaElem *ml = mb->lastelem;
    if (ml && (ml->flag & SELECT)) {
      const int ml_index = BLI_findindex(mb->editelems, ml);
      BLI_assert(ml_index != -1);

      /* Count backwards in case the active meta-element has multiple entries,
       * ensure this steps onto the next meta-element. */
      a = hits;
      while (a--) {
        const int select_id = buffer[a].id;
        if (select_id == -1) {
          continue;
        }

        if (((select_id & 0xFFFF) == base_index) &&
            ((select_id & ~MBALLSEL_ANY) >> 16 == ml_index)) {
          hit_cycle_offset = a + 1;
          break;
        }
      }
    }
  }

  for (a = 0; a < hits; a++) {
    const int index = (hit_cycle_offset == 0) ? a : ((a + hit_cycle_offset) % hits);
    const uint select_id = buffer[index].id;
    if (select_id == -1) {
      continue;
    }

    MetaElem *ml;
    Base *base = ED_mball_base_and_elem_from_select_buffer(bases, bases_len, select_id, &ml);
    if (ml == nullptr) {
      continue;
    }
    *r_base = base;
    *r_ml = ml;
    *r_selmask = select_id & MBALLSEL_ANY;
    found = true;
    break;
  }

  MEM_freeN(bases);

  return found;
}

bool ED_mball_select_pick(bContext *C, const int mval[2], const SelectPick_Params *params)
{
  Base *base = nullptr;
  MetaElem *ml = nullptr;
  uint selmask = 0;

  bool changed = false;

  bool found = ed_mball_findnearest_metaelem(C, mval, true, &base, &ml, &selmask);

  if (params->sel_op == SEL_OP_SET) {
    if ((found && params->select_passthrough) && (ml->flag & SELECT)) {
      found = false;
    }
    else if (found || params->deselect_all) {
      /* Deselect everything. */
      changed |= ED_mball_deselect_all_multi(C);
    }
  }

  if (found) {
    if (selmask & MBALLSEL_RADIUS) {
      ml->flag |= MB_SCALE_RAD;
    }
    else if (selmask & MBALLSEL_STIFF) {
      ml->flag &= ~MB_SCALE_RAD;
    }

    switch (params->sel_op) {
      case SEL_OP_ADD: {
        ml->flag |= SELECT;
        break;
      }
      case SEL_OP_SUB: {
        ml->flag &= ~SELECT;
        break;
      }
      case SEL_OP_XOR: {
        if (ml->flag & SELECT) {
          ml->flag &= ~SELECT;
        }
        else {
          ml->flag |= SELECT;
        }
        break;
      }
      case SEL_OP_SET: {
        /* Deselect has already been performed. */
        ml->flag |= SELECT;
        break;
      }
      case SEL_OP_AND: {
        BLI_assert_unreachable(); /* Doesn't make sense for picking. */
        break;
      }
    }
    const Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    MetaBall *mb = (MetaBall *)base->object->data;
    mb->lastelem = ml;

    DEG_id_tag_update(&mb->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_GEOM | ND_SELECT, mb);

    BKE_view_layer_synced_ensure(scene, view_layer);
    if (BKE_view_layer_active_base_get(view_layer) != base) {
      ED_object_base_activate(C, base);
    }

    changed = true;
  }

  return changed || found;
}

/** \} */
