/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_screen.hh"

#include "ED_screen.hh"
#include "ED_sequencer.hh"

#include "IMB_imbuf.hh"

#include "sequencer_intern.hh"

namespace blender::ed::vse {

/* **************************** buttons ********************************* */

#if 0
static bool sequencer_grease_pencil_panel_poll(const bContext *C, PanelType * /*pt*/)
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);

  /* Don't show the gpencil if we are not showing the image. */
  return check_show_imbuf(sseq);
}
#endif

static bool metadata_panel_context_poll(const bContext *C, PanelType * /*pt*/)
{
  SpaceSeq *space_sequencer = CTX_wm_space_seq(C);
  if (space_sequencer == nullptr) {
    return false;
  }
  return check_show_imbuf(*space_sequencer);
}

static void metadata_panel_context_draw(const bContext *C, Panel *panel)
{
  /* Image buffer can not be acquired during render, similar to
   * draw_image_seq(). */
  if (G.is_rendering) {
    return;
  }

  Scene *scene = CTX_data_sequencer_scene(C);
  SpaceSeq *space_sequencer = CTX_wm_space_seq(C);
  /* NOTE: We can only reliably show metadata for the original (current)
   * frame when split view is used. */
  const bool show_split = (scene->ed &&
                           (scene->ed->overlay_frame_flag & SEQ_EDIT_OVERLAY_FRAME_SHOW) &&
                           (space_sequencer->mainb == SEQ_DRAW_IMG_IMBUF));
  if (show_split && (space_sequencer->overlay_frame_type == SEQ_OVERLAY_FRAME_TYPE_REFERENCE)) {
    return;
  }
  /* NOTE: We disable multiview for drawing, since we don't know what is the
   * from the panel (is kind of all the views?). */
  ImBuf *ibuf = sequencer_ibuf_get(C, scene->r.cfra, "");
  if (ibuf != nullptr) {
    ED_region_image_metadata_panel_draw(ibuf, panel->layout);
    IMB_freeImBuf(ibuf);
  }
}

void sequencer_buttons_register(ARegionType *art)
{
  PanelType *pt;

#if 0
  pt = MEM_callocN(sizeof(PanelType), "spacetype sequencer panel gpencil");
  STRNCPY_UTF8(pt->idname, "SEQUENCER_PT_gpencil");
  STRNCPY_UTF8(pt->label, N_("Grease Pencil"));
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->draw_header = ED_gpencil_panel_standard_header;
  pt->draw = ED_gpencil_panel_standard;
  pt->poll = sequencer_grease_pencil_panel_poll;
  BLI_addtail(&art->paneltypes, pt);
#endif

  pt = MEM_callocN<PanelType>("spacetype sequencer panel metadata");
  STRNCPY_UTF8(pt->idname, "SEQUENCER_PT_metadata");
  STRNCPY_UTF8(pt->label, N_("Metadata"));
  STRNCPY_UTF8(pt->category, "Metadata");
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->poll = metadata_panel_context_poll;
  pt->draw = metadata_panel_context_draw;
  pt->order = 10;
  BLI_addtail(&art->paneltypes, pt);
}

}  // namespace blender::ed::vse
