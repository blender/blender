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
 * The Original Code is Copyright (C) 2009 by Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup spseq
 */

#include <string.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_listbase.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_screen.h"

#include "ED_screen.h"
#include "ED_gpencil.h"
#include "ED_sequencer.h"

#include "WM_api.h"
#include "WM_types.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "sequencer_intern.h"

/* **************************** buttons ********************************* */

#if 0
static bool sequencer_grease_pencil_panel_poll(const bContext *C, PanelType *UNUSED(pt))
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);

  /* don't show the gpencil if we are not showing the image */
  return ED_space_sequencer_check_show_imbuf(sseq);
}
#endif

static bool metadata_panel_context_poll(const bContext *C, PanelType *UNUSED(pt))
{
  SpaceSeq *space_sequencer = CTX_wm_space_seq(C);
  if (space_sequencer == NULL) {
    return false;
  }
  return ED_space_sequencer_check_show_imbuf(space_sequencer);
}

static void metadata_panel_context_draw(const bContext *C, Panel *panel)
{
  /* Image buffer can not be acquired during render, similar to
   * draw_image_seq(). */
  if (G.is_rendering) {
    return;
  }
  struct Main *bmain = CTX_data_main(C);
  struct Depsgraph *depsgraph = CTX_data_expect_evaluated_depsgraph(C);
  struct Scene *scene = CTX_data_scene(C);
  SpaceSeq *space_sequencer = CTX_wm_space_seq(C);
  /* NOTE: We can only reliably show metadata for the original (current)
   * frame when split view is used. */
  const bool show_split = (scene->ed && (scene->ed->over_flag & SEQ_EDIT_OVERLAY_SHOW) &&
                           (space_sequencer->mainb == SEQ_DRAW_IMG_IMBUF));
  if (show_split && space_sequencer->overlay_type == SEQ_DRAW_OVERLAY_REFERENCE) {
    return;
  }
  /* NOTE: We disable multiview for drawing, since we don't know what is the
   * from the panel (is kind of all the views?). */
  ImBuf *ibuf = sequencer_ibuf_get(bmain, depsgraph, scene, space_sequencer, scene->r.cfra, 0, "");
  if (ibuf != NULL) {
    ED_region_image_metadata_panel_draw(ibuf, panel->layout);
    IMB_freeImBuf(ibuf);
  }
}

void sequencer_buttons_register(ARegionType *art)
{
  PanelType *pt;

#if 0
  pt = MEM_callocN(sizeof(PanelType), "spacetype sequencer panel gpencil");
  strcpy(pt->idname, "SEQUENCER_PT_gpencil");
  strcpy(pt->label, N_("Grease Pencil"));
  strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw_header = ED_gpencil_panel_standard_header;
  pt->draw = ED_gpencil_panel_standard;
  pt->poll = sequencer_grease_pencil_panel_poll;
  BLI_addtail(&art->paneltypes, pt);
#endif

  pt = MEM_callocN(sizeof(PanelType), "spacetype sequencer panel metadata");
  strcpy(pt->idname, "SEQUENCER_PT_metadata");
  strcpy(pt->label, N_("Metadata"));
  strcpy(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->poll = metadata_panel_context_poll;
  pt->draw = metadata_panel_context_draw;
  pt->flag |= PNL_DEFAULT_CLOSED;
  BLI_addtail(&art->paneltypes, pt);
}
