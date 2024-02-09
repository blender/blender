/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 */

#include "DNA_curve_types.h"
#include "DNA_gpencil_legacy_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_rect.h"

#include "BLT_translation.hh"

#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_layer.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_scene.h"
#include "BKE_screen.hh"
#include "BKE_vfont.hh"

#include "DEG_depsgraph_query.hh"

#include "ED_mesh.hh"
#include "ED_particle.hh"
#include "ED_screen.hh"
#include "ED_transform.hh"

#include "WM_api.hh"
#include "WM_message.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_resources.hh"

#include "view3d_intern.h"

#include "view3d_navigate.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name View Pan Operator
 *
 * Move (pan) in incremental steps. For interactive pan see #VIEW3D_OT_move.
 * \{ */

enum {
  V3D_VIEW_PANLEFT = 1,
  V3D_VIEW_PANRIGHT,
  V3D_VIEW_PANDOWN,
  V3D_VIEW_PANUP,
};

static const EnumPropertyItem prop_view_pan_items[] = {
    {V3D_VIEW_PANLEFT, "PANLEFT", 0, "Pan Left", "Pan the view to the left"},
    {V3D_VIEW_PANRIGHT, "PANRIGHT", 0, "Pan Right", "Pan the view to the right"},
    {V3D_VIEW_PANUP, "PANUP", 0, "Pan Up", "Pan the view up"},
    {V3D_VIEW_PANDOWN, "PANDOWN", 0, "Pan Down", "Pan the view down"},
    {0, nullptr, 0, nullptr, nullptr},
};

static int viewpan_invoke_impl(bContext * /*C*/,
                               ViewOpsData *vod,
                               const wmEvent * /*event*/,
                               PointerRNA *ptr)
{
  int x = 0, y = 0;
  int pandir = RNA_enum_get(ptr, "type");

  if (pandir == V3D_VIEW_PANRIGHT) {
    x = -32;
  }
  else if (pandir == V3D_VIEW_PANLEFT) {
    x = 32;
  }
  else if (pandir == V3D_VIEW_PANUP) {
    y = -25;
  }
  else if (pandir == V3D_VIEW_PANDOWN) {
    y = 25;
  }

  viewmove_apply(vod, vod->prev.event_xy[0] + x, vod->prev.event_xy[1] + y);

  return OPERATOR_FINISHED;
}

static int viewpan_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  return view3d_navigate_invoke_impl(C, op, event, &ViewOpsType_pan);
}

void VIEW3D_OT_view_pan(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Pan View Direction";
  ot->description = "Pan the view in a given direction";
  ot->idname = ViewOpsType_pan.idname;

  /* api callbacks */
  ot->invoke = viewpan_invoke;
  ot->poll = view3d_location_poll;

  /* flags */
  ot->flag = 0;

  /* Properties */
  ot->prop = RNA_def_enum(
      ot->srna, "type", prop_view_pan_items, 0, "Pan", "Direction of View Pan");
}

/** \} */

const ViewOpsType ViewOpsType_pan = {
    /*flag*/ (VIEWOPS_FLAG_DEPTH_NAVIGATE | VIEWOPS_FLAG_INIT_ZFAC),
    /*idname*/ "VIEW3D_OT_view_pan",
    /*poll_fn*/ view3d_location_poll,
    /*init_fn*/ viewpan_invoke_impl,
    /*apply_fn*/ nullptr,
};
