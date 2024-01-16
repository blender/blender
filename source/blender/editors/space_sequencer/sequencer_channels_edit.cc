/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.hh"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "ED_screen.hh"

#include "UI_view2d.hh"

#include "SEQ_channels.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"

#include "WM_api.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

/* Own include. */
#include "sequencer_intern.hh"

static int sequencer_rename_channel_invoke(bContext *C, wmOperator * /*op*/, const wmEvent *event)
{
  SeqChannelDrawContext context;
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  channel_draw_context_init(C, CTX_wm_region(C), &context);
  float mouse_y = UI_view2d_region_to_view_y(context.timeline_region_v2d, event->mval[1]);

  sseq->runtime->rename_channel_index = mouse_y;
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, CTX_data_scene(C));
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_rename_channel(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Rename Channel";
  ot->idname = "SEQUENCER_OT_rename_channel";

  /* Api callbacks. */
  ot->invoke = sequencer_rename_channel_invoke;
  ot->poll = sequencer_edit_with_channel_region_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}
