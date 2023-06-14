/* SPDX-FileCopyrightText: 2022 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"

#include "BLI_span.hh"

#include "RNA_access.h"

#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_gizmo_library.h"
#include "ED_gizmo_utils.h"

#include "SEQ_iterator.h"
#include "SEQ_retiming.h"
#include "SEQ_retiming.hh"
#include "SEQ_sequencer.h"

/* Own include. */
#include "sequencer_intern.h"

typedef struct GizmoGroup_retime {
  wmGizmo *add_handle_gizmo;
  wmGizmo *move_handle_gizmo;
  wmGizmo *remove_handle_gizmo;
  wmGizmo *speed_set_gizmo;
} GizmoGroup_retime;

static bool gizmogroup_retime_poll(const bContext *C, wmGizmoGroupType *gzgt)
{
  /* Needed to prevent drawing gizmos when retiming tool is not activated. */
  if (!ED_gizmo_poll_or_unlink_delayed_from_tool(C, gzgt)) {
    return false;
  }

  if ((U.gizmo_flag & USER_GIZMO_DRAW) == 0) {
    return false;
  }

  ScrArea *area = CTX_wm_area(C);
  if (area == nullptr && area->spacetype != SPACE_SEQ) {
    return false;
  }

  const SpaceSeq *sseq = (SpaceSeq *)area->spacedata.first;
  if (sseq->gizmo_flag & (SEQ_GIZMO_HIDE | SEQ_GIZMO_HIDE_TOOL)) {
    return false;
  }

  Editing *ed = SEQ_editing_get(CTX_data_scene(C));
  Sequence *seq = ed->act_seq;

  if (ed == nullptr || seq == nullptr || !SEQ_retiming_is_allowed(seq)) {
    return false;
  }

  return true;
}

static void gizmogroup_retime_setup(const bContext * /* C */, wmGizmoGroup *gzgroup)
{
  GizmoGroup_retime *ggd = (GizmoGroup_retime *)MEM_callocN(sizeof(GizmoGroup_retime), __func__);

  /* Assign gizmos. */
  const wmGizmoType *gzt_add_handle = WM_gizmotype_find("GIZMO_GT_retime_handle_add", true);
  ggd->add_handle_gizmo = WM_gizmo_new_ptr(gzt_add_handle, gzgroup, nullptr);
  const wmGizmoType *gzt_remove_handle = WM_gizmotype_find("GIZMO_GT_retime_handle_remove", true);
  ggd->remove_handle_gizmo = WM_gizmo_new_ptr(gzt_remove_handle, gzgroup, nullptr);
  const wmGizmoType *gzt_move_handle = WM_gizmotype_find("GIZMO_GT_retime_handle_move", true);
  ggd->move_handle_gizmo = WM_gizmo_new_ptr(gzt_move_handle, gzgroup, nullptr);
  const wmGizmoType *gzt_speed_set = WM_gizmotype_find("GIZMO_GT_retime_speed_set", true);
  ggd->speed_set_gizmo = WM_gizmo_new_ptr(gzt_speed_set, gzgroup, nullptr);
  gzgroup->customdata = ggd;

  /* Assign operators. */
  wmOperatorType *ot = WM_operatortype_find("SEQUENCER_OT_retiming_handle_move", true);
  WM_gizmo_operator_set(ggd->move_handle_gizmo, 0, ot, nullptr);
  ot = WM_operatortype_find("SEQUENCER_OT_retiming_handle_add", true);
  WM_gizmo_operator_set(ggd->add_handle_gizmo, 0, ot, nullptr);
  ot = WM_operatortype_find("SEQUENCER_OT_retiming_handle_remove", true);
  WM_gizmo_operator_set(ggd->remove_handle_gizmo, 0, ot, nullptr);
  ot = WM_operatortype_find("SEQUENCER_OT_retiming_segment_speed_set", true);
  WM_gizmo_operator_set(ggd->speed_set_gizmo, 0, ot, nullptr);
}

void SEQUENCER_GGT_gizmo_retime(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Sequencer Transform Gizmo Retime";
  gzgt->idname = "SEQUENCER_GGT_gizmo_retime";

  gzgt->flag = WM_GIZMOGROUPTYPE_DRAW_MODAL_ALL;

  gzgt->gzmap_params.spaceid = SPACE_SEQ;
  gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

  gzgt->poll = gizmogroup_retime_poll;
  gzgt->setup = gizmogroup_retime_setup;
}
