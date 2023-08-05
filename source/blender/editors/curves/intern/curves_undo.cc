/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurves
 */

#include "BLI_task.hh"

#include "BKE_context.h"
#include "BKE_curves.hh"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_undo_system.h"

#include "CLG_log.h"

#include "DEG_depsgraph.h"

#include "ED_curves.hh"
#include "ED_undo.hh"

#include "MEM_guardedalloc.h"

#include "WM_api.hh"
#include "WM_types.hh"

static CLG_LogRef LOG = {"ed.undo.curves"};

namespace blender::ed::curves::undo {

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 *
 * \note This is similar for all edit-mode types.
 * \{ */

struct StepObject {
  UndoRefID_Object obedit_ref = {};
  bke::CurvesGeometry geometry = {};
};

struct CurvesUndoStep {
  UndoStep step;
  Array<StepObject> objects;
};

static bool step_encode(bContext *C, Main *bmain, UndoStep *us_p)
{
  CurvesUndoStep *us = reinterpret_cast<CurvesUndoStep *>(us_p);

  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uint objects_num = 0;
  Object **objects = ED_undo_editmode_objects_from_view_layer(scene, view_layer, &objects_num);

  new (&us->objects) Array<StepObject>(objects_num);

  threading::parallel_for(us->objects.index_range(), 8, [&](const IndexRange range) {
    for (const int i : range) {
      Object *ob = objects[i];
      const Curves &curves_id = *static_cast<Curves *>(ob->data);
      StepObject &object = us->objects[i];

      object.obedit_ref.ptr = ob;
      object.geometry = curves_id.geometry.wrap();
    }
  });
  MEM_SAFE_FREE(objects);

  bmain->is_memfile_undo_flush_needed = true;

  return true;
}

static void step_decode(
    bContext *C, Main *bmain, UndoStep *us_p, const eUndoStepDir /*dir*/, bool /*is_final*/)
{
  CurvesUndoStep *us = reinterpret_cast<CurvesUndoStep *>(us_p);

  ED_undo_object_editmode_restore_helper(C,
                                         &us->objects.first().obedit_ref.ptr,
                                         us->objects.size(),
                                         sizeof(decltype(us->objects)::value_type));

  BLI_assert(BKE_object_is_in_editmode(us->objects.first().obedit_ref.ptr));

  for (const StepObject &object : us->objects) {
    Curves &curves_id = *static_cast<Curves *>(object.obedit_ref.ptr->data);

    /* Overwrite the curves geometry. */
    curves_id.geometry.wrap() = object.geometry;

    DEG_id_tag_update(&curves_id.id, ID_RECALC_GEOMETRY);
  }

  ED_undo_object_set_active_or_warn(CTX_data_scene(C),
                                    CTX_data_view_layer(C),
                                    us->objects.first().obedit_ref.ptr,
                                    us_p->name,
                                    &LOG);

  bmain->is_memfile_undo_flush_needed = true;

  WM_event_add_notifier(C, NC_GEOM | ND_DATA, nullptr);
}

static void step_free(UndoStep *us_p)
{
  CurvesUndoStep *us = reinterpret_cast<CurvesUndoStep *>(us_p);
  us->objects.~Array();
}

static void foreach_ID_ref(UndoStep *us_p,
                           UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                           void *user_data)
{
  CurvesUndoStep *us = reinterpret_cast<CurvesUndoStep *>(us_p);

  for (const StepObject &object : us->objects) {
    foreach_ID_ref_fn(user_data, ((UndoRefID *)&object.obedit_ref));
  }
}

/** \} */

}  // namespace blender::ed::curves::undo

void ED_curves_undosys_type(UndoType *ut)
{
  using namespace blender::ed;

  ut->name = "Edit Curves";
  ut->poll = curves::editable_curves_in_edit_mode_poll;
  ut->step_encode = curves::undo::step_encode;
  ut->step_decode = curves::undo::step_decode;
  ut->step_free = curves::undo::step_free;

  ut->step_foreach_ID_ref = curves::undo::foreach_ID_ref;

  ut->flags = UNDOTYPE_FLAG_NEED_CONTEXT_FOR_ENCODE;

  ut->step_size = sizeof(curves::undo::CurvesUndoStep);
}
