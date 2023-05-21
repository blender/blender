/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup edlattice
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "BLI_array_utils.h"
#include "BLI_utildefines.h"

#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_undo_system.h"

#include "DEG_depsgraph.h"

#include "ED_lattice.h"
#include "ED_object.h"
#include "ED_undo.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

#include "lattice_intern.h"

/** We only need this locally. */
static CLG_LogRef LOG = {"ed.undo.lattice"};

/* -------------------------------------------------------------------- */
/** \name Undo Conversion
 * \{ */

/* TODO(@ideasman42): this could contain an entire 'Lattice' struct. */
typedef struct UndoLattice {
  BPoint *def;
  int pntsu, pntsv, pntsw, actbp;
  char typeu, typev, typew;
  float fu, fv, fw;
  float du, dv, dw;
  MDeformVert *dvert;
  size_t undo_size;
} UndoLattice;

static void undolatt_to_editlatt(UndoLattice *ult, EditLatt *editlatt)
{
  const int len_src = ult->pntsu * ult->pntsv * ult->pntsw;
  const int len_dst = editlatt->latt->pntsu * editlatt->latt->pntsv * editlatt->latt->pntsw;
  if (len_src != len_dst) {
    MEM_freeN(editlatt->latt->def);
    editlatt->latt->def = MEM_dupallocN(ult->def);
  }
  else {
    memcpy(editlatt->latt->def, ult->def, sizeof(BPoint) * len_src);
  }

  /* Even for the same amount of points we don't just copy memory for MDeformVert,
   * relations to #MDeformWeight might have changed. */
  if (editlatt->latt->dvert && ult->dvert) {
    BKE_defvert_array_free(editlatt->latt->dvert, len_dst);
    editlatt->latt->dvert = MEM_mallocN(sizeof(MDeformVert) * len_src, "Lattice MDeformVert");
    BKE_defvert_array_copy(editlatt->latt->dvert, ult->dvert, len_src);
  }

  editlatt->latt->pntsu = ult->pntsu;
  editlatt->latt->pntsv = ult->pntsv;
  editlatt->latt->pntsw = ult->pntsw;
  editlatt->latt->actbp = ult->actbp;

  editlatt->latt->typeu = ult->typeu;
  editlatt->latt->typev = ult->typev;
  editlatt->latt->typew = ult->typew;

  editlatt->latt->fu = ult->fu;
  editlatt->latt->fv = ult->fv;
  editlatt->latt->fw = ult->fw;
  editlatt->latt->du = ult->du;
  editlatt->latt->dv = ult->dv;
  editlatt->latt->dw = ult->dw;
}

static void *undolatt_from_editlatt(UndoLattice *ult, EditLatt *editlatt)
{
  BLI_assert(BLI_array_is_zeroed(ult, 1));

  ult->def = MEM_dupallocN(editlatt->latt->def);
  ult->pntsu = editlatt->latt->pntsu;
  ult->pntsv = editlatt->latt->pntsv;
  ult->pntsw = editlatt->latt->pntsw;
  ult->actbp = editlatt->latt->actbp;

  ult->typeu = editlatt->latt->typeu;
  ult->typev = editlatt->latt->typev;
  ult->typew = editlatt->latt->typew;

  ult->fu = editlatt->latt->fu;
  ult->fv = editlatt->latt->fv;
  ult->fw = editlatt->latt->fw;
  ult->du = editlatt->latt->du;
  ult->dv = editlatt->latt->dv;
  ult->dw = editlatt->latt->dw;

  if (editlatt->latt->dvert) {
    const int tot = ult->pntsu * ult->pntsv * ult->pntsw;
    ult->dvert = MEM_mallocN(sizeof(MDeformVert) * tot, "Undo Lattice MDeformVert");
    BKE_defvert_array_copy(ult->dvert, editlatt->latt->dvert, tot);
    ult->undo_size += sizeof(*ult->dvert) * tot;
  }

  ult->undo_size += sizeof(*ult->def) * ult->pntsu * ult->pntsv * ult->pntsw;

  return ult;
}

static void undolatt_free_data(UndoLattice *ult)
{
  if (ult->def) {
    MEM_freeN(ult->def);
  }
  if (ult->dvert) {
    BKE_defvert_array_free(ult->dvert, ult->pntsu * ult->pntsv * ult->pntsw);
    ult->dvert = NULL;
  }
}

#if 0
static int validate_undoLatt(void *data, void *edata)
{
  UndoLattice *ult = (UndoLattice *)data;
  EditLatt *editlatt = (EditLatt *)edata;

  return (ult->pntsu == editlatt->latt->pntsu && ult->pntsv == editlatt->latt->pntsv &&
          ult->pntsw == editlatt->latt->pntsw);
}
#endif

static Object *editlatt_object_from_context(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obedit = BKE_view_layer_edit_object_get(view_layer);
  if (obedit && obedit->type == OB_LATTICE) {
    Lattice *lt = obedit->data;
    if (lt->editlatt != NULL) {
      return obedit;
    }
  }

  return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 *
 * \note This is similar for all edit-mode types.
 * \{ */

typedef struct LatticeUndoStep_Elem {
  UndoRefID_Object obedit_ref;
  UndoLattice data;
} LatticeUndoStep_Elem;

typedef struct LatticeUndoStep {
  UndoStep step;
  LatticeUndoStep_Elem *elems;
  uint elems_len;
} LatticeUndoStep;

static bool lattice_undosys_poll(bContext *C)
{
  return editlatt_object_from_context(C) != NULL;
}

static bool lattice_undosys_step_encode(struct bContext *C, Main *bmain, UndoStep *us_p)
{
  LatticeUndoStep *us = (LatticeUndoStep *)us_p;

  /* Important not to use the 3D view when getting objects because all objects
   * outside of this list will be moved out of edit-mode when reading back undo steps. */
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = ED_undo_editmode_objects_from_view_layer(scene, view_layer, &objects_len);

  us->elems = MEM_callocN(sizeof(*us->elems) * objects_len, __func__);
  us->elems_len = objects_len;

  for (uint i = 0; i < objects_len; i++) {
    Object *ob = objects[i];
    LatticeUndoStep_Elem *elem = &us->elems[i];

    elem->obedit_ref.ptr = ob;
    Lattice *lt = ob->data;
    undolatt_from_editlatt(&elem->data, lt->editlatt);
    lt->editlatt->needs_flush_to_id = 1;
    us->step.data_size += elem->data.undo_size;
  }
  MEM_freeN(objects);

  bmain->is_memfile_undo_flush_needed = true;

  return true;
}

static void lattice_undosys_step_decode(struct bContext *C,
                                        struct Main *bmain,
                                        UndoStep *us_p,
                                        const eUndoStepDir UNUSED(dir),
                                        bool UNUSED(is_final))
{
  LatticeUndoStep *us = (LatticeUndoStep *)us_p;

  ED_undo_object_editmode_restore_helper(
      C, &us->elems[0].obedit_ref.ptr, us->elems_len, sizeof(*us->elems));

  BLI_assert(BKE_object_is_in_editmode(us->elems[0].obedit_ref.ptr));

  for (uint i = 0; i < us->elems_len; i++) {
    LatticeUndoStep_Elem *elem = &us->elems[i];
    Object *obedit = elem->obedit_ref.ptr;
    Lattice *lt = obedit->data;
    if (lt->editlatt == NULL) {
      /* Should never fail, may not crash but can give odd behavior. */
      CLOG_ERROR(&LOG,
                 "name='%s', failed to enter edit-mode for object '%s', undo state invalid",
                 us_p->name,
                 obedit->id.name);
      continue;
    }
    undolatt_to_editlatt(&elem->data, lt->editlatt);
    lt->editlatt->needs_flush_to_id = 1;
    DEG_id_tag_update(&lt->id, ID_RECALC_GEOMETRY);
  }

  /* The first element is always active */
  ED_undo_object_set_active_or_warn(
      CTX_data_scene(C), CTX_data_view_layer(C), us->elems[0].obedit_ref.ptr, us_p->name, &LOG);

  /* Check after setting active. */
  BLI_assert(lattice_undosys_poll(C));

  bmain->is_memfile_undo_flush_needed = true;

  WM_event_add_notifier(C, NC_GEOM | ND_DATA, NULL);
}

static void lattice_undosys_step_free(UndoStep *us_p)
{
  LatticeUndoStep *us = (LatticeUndoStep *)us_p;

  for (uint i = 0; i < us->elems_len; i++) {
    LatticeUndoStep_Elem *elem = &us->elems[i];
    undolatt_free_data(&elem->data);
  }
  MEM_freeN(us->elems);
}

static void lattice_undosys_foreach_ID_ref(UndoStep *us_p,
                                           UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                                           void *user_data)
{
  LatticeUndoStep *us = (LatticeUndoStep *)us_p;

  for (uint i = 0; i < us->elems_len; i++) {
    LatticeUndoStep_Elem *elem = &us->elems[i];
    foreach_ID_ref_fn(user_data, ((UndoRefID *)&elem->obedit_ref));
  }
}

void ED_lattice_undosys_type(UndoType *ut)
{
  ut->name = "Edit Lattice";
  ut->poll = lattice_undosys_poll;
  ut->step_encode = lattice_undosys_step_encode;
  ut->step_decode = lattice_undosys_step_decode;
  ut->step_free = lattice_undosys_step_free;

  ut->step_foreach_ID_ref = lattice_undosys_foreach_ID_ref;

  ut->flags = UNDOTYPE_FLAG_NEED_CONTEXT_FOR_ENCODE;

  ut->step_size = sizeof(LatticeUndoStep);
}

/** \} */
