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
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "BLI_utildefines.h"
#include "BLI_array_utils.h"

#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_undo_system.h"

#include "DEG_depsgraph.h"

#include "ED_object.h"
#include "ED_lattice.h"
#include "ED_undo.h"
#include "ED_util.h"

#include "WM_types.h"
#include "WM_api.h"

#include "lattice_intern.h"

/** We only need this locally. */
static CLG_LogRef LOG = {"ed.undo.lattice"};

/* -------------------------------------------------------------------- */
/** \name Undo Conversion
 * \{ */

typedef struct UndoLattice {
  BPoint *def;
  int pntsu, pntsv, pntsw, actbp;
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

  editlatt->latt->pntsu = ult->pntsu;
  editlatt->latt->pntsv = ult->pntsv;
  editlatt->latt->pntsw = ult->pntsw;
  editlatt->latt->actbp = ult->actbp;
}

static void *undolatt_from_editlatt(UndoLattice *ult, EditLatt *editlatt)
{
  BLI_assert(BLI_array_is_zeroed(ult, 1));

  ult->def = MEM_dupallocN(editlatt->latt->def);
  ult->pntsu = editlatt->latt->pntsu;
  ult->pntsv = editlatt->latt->pntsv;
  ult->pntsw = editlatt->latt->pntsw;
  ult->actbp = editlatt->latt->actbp;

  ult->undo_size += sizeof(*ult->def) * ult->pntsu * ult->pntsv * ult->pntsw;

  return ult;
}

static void undolatt_free_data(UndoLattice *ult)
{
  if (ult->def) {
    MEM_freeN(ult->def);
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
  Object *obedit = CTX_data_edit_object(C);
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

static bool lattice_undosys_step_encode(struct bContext *C,
                                        struct Main *UNUSED(bmain),
                                        UndoStep *us_p)
{
  LatticeUndoStep *us = (LatticeUndoStep *)us_p;

  /* Important not to use the 3D view when getting objects because all objects
   * outside of this list will be moved out of edit-mode when reading back undo steps. */
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      view_layer, NULL, &objects_len);

  us->elems = MEM_callocN(sizeof(*us->elems) * objects_len, __func__);
  us->elems_len = objects_len;

  for (uint i = 0; i < objects_len; i++) {
    Object *ob = objects[i];
    LatticeUndoStep_Elem *elem = &us->elems[i];

    elem->obedit_ref.ptr = ob;
    Lattice *lt = ob->data;
    undolatt_from_editlatt(&elem->data, lt->editlatt);
    us->step.data_size += elem->data.undo_size;
  }
  MEM_freeN(objects);
  return true;
}

static void lattice_undosys_step_decode(struct bContext *C,
                                        struct Main *UNUSED(bmain),
                                        UndoStep *us_p,
                                        int UNUSED(dir),
                                        bool UNUSED(is_final))
{
  LatticeUndoStep *us = (LatticeUndoStep *)us_p;

  /* Load all our objects  into edit-mode, clear everything else. */
  ED_undo_object_editmode_restore_helper(
      C, &us->elems[0].obedit_ref.ptr, us->elems_len, sizeof(*us->elems));

  BLI_assert(lattice_undosys_poll(C));

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
    DEG_id_tag_update(&obedit->id, ID_RECALC_GEOMETRY);
  }

  /* The first element is always active */
  ED_undo_object_set_active_or_warn(
      CTX_data_view_layer(C), us->elems[0].obedit_ref.ptr, us_p->name, &LOG);

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

/* Export for ED_undo_sys. */
void ED_lattice_undosys_type(UndoType *ut)
{
  ut->name = "Edit Lattice";
  ut->poll = lattice_undosys_poll;
  ut->step_encode = lattice_undosys_step_encode;
  ut->step_decode = lattice_undosys_step_decode;
  ut->step_free = lattice_undosys_step_free;

  ut->step_foreach_ID_ref = lattice_undosys_foreach_ID_ref;

  ut->use_context = true;

  ut->step_size = sizeof(LatticeUndoStep);
}

/** \} */
