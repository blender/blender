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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spseq
 */

#include <math.h>
#include <stdlib.h>

#include "DNA_space_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_markers.h"
#include "ED_select_utils.h"
#include "ED_sequencer.h"
#include "ED_transform.h" /* Transform keymap. */

#include "BKE_sequencer.h"

#include "sequencer_intern.h"

/* ************************** registration **********************************/

void sequencer_operatortypes(void)
{
  /* sequencer_edit.c */
  WM_operatortype_append(SEQUENCER_OT_split);
  WM_operatortype_append(SEQUENCER_OT_slip);
  WM_operatortype_append(SEQUENCER_OT_mute);
  WM_operatortype_append(SEQUENCER_OT_unmute);
  WM_operatortype_append(SEQUENCER_OT_lock);
  WM_operatortype_append(SEQUENCER_OT_unlock);
  WM_operatortype_append(SEQUENCER_OT_reload);
  WM_operatortype_append(SEQUENCER_OT_refresh_all);
  WM_operatortype_append(SEQUENCER_OT_reassign_inputs);
  WM_operatortype_append(SEQUENCER_OT_swap_inputs);
  WM_operatortype_append(SEQUENCER_OT_duplicate);
  WM_operatortype_append(SEQUENCER_OT_delete);
  WM_operatortype_append(SEQUENCER_OT_offset_clear);
  WM_operatortype_append(SEQUENCER_OT_images_separate);
  WM_operatortype_append(SEQUENCER_OT_meta_toggle);
  WM_operatortype_append(SEQUENCER_OT_meta_make);
  WM_operatortype_append(SEQUENCER_OT_meta_separate);

  WM_operatortype_append(SEQUENCER_OT_gap_remove);
  WM_operatortype_append(SEQUENCER_OT_gap_insert);
  WM_operatortype_append(SEQUENCER_OT_snap);
  WM_operatortype_append(SEQUENCER_OT_strip_jump);
  WM_operatortype_append(SEQUENCER_OT_swap);
  WM_operatortype_append(SEQUENCER_OT_swap_data);
  WM_operatortype_append(SEQUENCER_OT_rendersize);

  WM_operatortype_append(SEQUENCER_OT_export_subtitles);

  WM_operatortype_append(SEQUENCER_OT_copy);
  WM_operatortype_append(SEQUENCER_OT_paste);

  WM_operatortype_append(SEQUENCER_OT_view_all);
  WM_operatortype_append(SEQUENCER_OT_view_selected);
  WM_operatortype_append(SEQUENCER_OT_view_frame);
  WM_operatortype_append(SEQUENCER_OT_view_all_preview);
  WM_operatortype_append(SEQUENCER_OT_view_toggle);
  WM_operatortype_append(SEQUENCER_OT_view_zoom_ratio);
  WM_operatortype_append(SEQUENCER_OT_view_ghost_border);

  WM_operatortype_append(SEQUENCER_OT_rebuild_proxy);
  WM_operatortype_append(SEQUENCER_OT_enable_proxies);
  WM_operatortype_append(SEQUENCER_OT_change_effect_input);
  WM_operatortype_append(SEQUENCER_OT_change_effect_type);
  WM_operatortype_append(SEQUENCER_OT_change_path);

  WM_operatortype_append(SEQUENCER_OT_set_range_to_strips);

  /* sequencer_select.c */
  WM_operatortype_append(SEQUENCER_OT_select_all);
  WM_operatortype_append(SEQUENCER_OT_select);
  WM_operatortype_append(SEQUENCER_OT_select_more);
  WM_operatortype_append(SEQUENCER_OT_select_less);
  WM_operatortype_append(SEQUENCER_OT_select_linked_pick);
  WM_operatortype_append(SEQUENCER_OT_select_linked);
  WM_operatortype_append(SEQUENCER_OT_select_handles);
  WM_operatortype_append(SEQUENCER_OT_select_side);
  WM_operatortype_append(SEQUENCER_OT_select_side_of_frame);
  WM_operatortype_append(SEQUENCER_OT_select_box);
  WM_operatortype_append(SEQUENCER_OT_select_grouped);

  /* sequencer_add.c */
  WM_operatortype_append(SEQUENCER_OT_scene_strip_add);
  WM_operatortype_append(SEQUENCER_OT_movieclip_strip_add);
  WM_operatortype_append(SEQUENCER_OT_mask_strip_add);
  WM_operatortype_append(SEQUENCER_OT_movie_strip_add);
  WM_operatortype_append(SEQUENCER_OT_sound_strip_add);
  WM_operatortype_append(SEQUENCER_OT_image_strip_add);
  WM_operatortype_append(SEQUENCER_OT_effect_strip_add);

  /* sequencer_modifiers.c */
  WM_operatortype_append(SEQUENCER_OT_strip_modifier_add);
  WM_operatortype_append(SEQUENCER_OT_strip_modifier_remove);
  WM_operatortype_append(SEQUENCER_OT_strip_modifier_move);
  WM_operatortype_append(SEQUENCER_OT_strip_modifier_copy);

  /* sequencer_view.h */
  WM_operatortype_append(SEQUENCER_OT_sample);
}

void sequencer_keymap(wmKeyConfig *keyconf)
{
  /* Common items ------------------------------------------------------------------ */
  WM_keymap_ensure(keyconf, "SequencerCommon", SPACE_SEQ, 0);

  /* Strips Region --------------------------------------------------------------- */
  WM_keymap_ensure(keyconf, "Sequencer", SPACE_SEQ, 0);

  /* Preview Region ----------------------------------------------------------- */
  WM_keymap_ensure(keyconf, "SequencerPreview", SPACE_SEQ, 0);
}

void ED_operatormacros_sequencer(void)
{
  wmOperatorType *ot;

  ot = WM_operatortype_append_macro("SEQUENCER_OT_duplicate_move",
                                    "Duplicate Strips",
                                    "Duplicate selected strips and move them",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);

  WM_operatortype_macro_define(ot, "SEQUENCER_OT_duplicate");
  WM_operatortype_macro_define(ot, "TRANSFORM_OT_seq_slide");
}
