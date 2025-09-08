/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include <cstdlib>

#include "DNA_space_types.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_sequencer.hh"

#include "sequencer_intern.hh"

namespace blender::ed::vse {

/* ************************** registration **********************************/

void sequencer_operatortypes()
{
  /* `sequencer_edit.cc` */
  WM_operatortype_append(SEQUENCER_OT_split);
  WM_operatortype_append(SEQUENCER_OT_slip);
  WM_operatortype_append(SEQUENCER_OT_mute);
  WM_operatortype_append(SEQUENCER_OT_unmute);
  WM_operatortype_append(SEQUENCER_OT_lock);
  WM_operatortype_append(SEQUENCER_OT_unlock);
  WM_operatortype_append(SEQUENCER_OT_connect);
  WM_operatortype_append(SEQUENCER_OT_disconnect);
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

  WM_operatortype_append(SEQUENCER_OT_rebuild_proxy);
  WM_operatortype_append(SEQUENCER_OT_enable_proxies);
  WM_operatortype_append(SEQUENCER_OT_change_effect_type);
  WM_operatortype_append(SEQUENCER_OT_change_path);
  WM_operatortype_append(SEQUENCER_OT_change_scene);

  WM_operatortype_append(SEQUENCER_OT_set_range_to_strips);
  WM_operatortype_append(SEQUENCER_OT_strip_transform_clear);
  WM_operatortype_append(SEQUENCER_OT_strip_transform_fit);

  WM_operatortype_append(SEQUENCER_OT_strip_color_tag_set);
  WM_operatortype_append(SEQUENCER_OT_cursor_set);
  WM_operatortype_append(SEQUENCER_OT_scene_frame_range_update);

  /* `sequencer_retiming.cc` */
  WM_operatortype_append(SEQUENCER_OT_retiming_reset);
  WM_operatortype_append(SEQUENCER_OT_retiming_show);
  WM_operatortype_append(SEQUENCER_OT_retiming_key_add);
  WM_operatortype_append(SEQUENCER_OT_retiming_freeze_frame_add);
  WM_operatortype_append(SEQUENCER_OT_retiming_transition_add);
  WM_operatortype_append(SEQUENCER_OT_retiming_segment_speed_set);
  WM_operatortype_append(SEQUENCER_OT_retiming_key_delete);

  /* `sequencer_text_edit.cc` */
  WM_operatortype_append(SEQUENCER_OT_text_cursor_move);
  WM_operatortype_append(SEQUENCER_OT_text_insert);
  WM_operatortype_append(SEQUENCER_OT_text_delete);
  WM_operatortype_append(SEQUENCER_OT_text_line_break);
  WM_operatortype_append(SEQUENCER_OT_text_select_all);
  WM_operatortype_append(SEQUENCER_OT_text_deselect_all);
  WM_operatortype_append(SEQUENCER_OT_text_edit_mode_toggle);
  WM_operatortype_append(SEQUENCER_OT_text_cursor_set);
  WM_operatortype_append(SEQUENCER_OT_text_edit_copy);
  WM_operatortype_append(SEQUENCER_OT_text_edit_paste);
  WM_operatortype_append(SEQUENCER_OT_text_edit_cut);

  /* `sequencer_select.cc` */
  WM_operatortype_append(SEQUENCER_OT_select_all);
  WM_operatortype_append(SEQUENCER_OT_select);
  WM_operatortype_append(SEQUENCER_OT_select_handle);
  WM_operatortype_append(SEQUENCER_OT_select_more);
  WM_operatortype_append(SEQUENCER_OT_select_less);
  WM_operatortype_append(SEQUENCER_OT_select_linked_pick);
  WM_operatortype_append(SEQUENCER_OT_select_linked);
  WM_operatortype_append(SEQUENCER_OT_select_handles);
  WM_operatortype_append(SEQUENCER_OT_select_side);
  WM_operatortype_append(SEQUENCER_OT_select_side_of_frame);
  WM_operatortype_append(SEQUENCER_OT_select_box);
  WM_operatortype_append(SEQUENCER_OT_select_lasso);
  WM_operatortype_append(SEQUENCER_OT_select_circle);
  WM_operatortype_append(SEQUENCER_OT_select_grouped);

  /* `sequencer_add.cc` */
  WM_operatortype_append(SEQUENCER_OT_scene_strip_add);
  WM_operatortype_append(SEQUENCER_OT_scene_strip_add_new);
  WM_operatortype_append(SEQUENCER_OT_movieclip_strip_add);
  WM_operatortype_append(SEQUENCER_OT_mask_strip_add);
  WM_operatortype_append(SEQUENCER_OT_movie_strip_add);
  WM_operatortype_append(SEQUENCER_OT_sound_strip_add);
  WM_operatortype_append(SEQUENCER_OT_image_strip_add);
  WM_operatortype_append(SEQUENCER_OT_effect_strip_add);
  WM_operatortype_append(SEQUENCER_OT_add_scene_strip_from_scene_asset);

  /* sequencer_modifiers.c */
  WM_operatortype_append(SEQUENCER_OT_strip_modifier_add);
  WM_operatortype_append(SEQUENCER_OT_strip_modifier_remove);
  WM_operatortype_append(SEQUENCER_OT_strip_modifier_move);
  WM_operatortype_append(SEQUENCER_OT_strip_modifier_copy);
  WM_operatortype_append(SEQUENCER_OT_strip_modifier_move_to_index);
  WM_operatortype_append(SEQUENCER_OT_strip_modifier_set_active);
  WM_operatortype_append(SEQUENCER_OT_strip_modifier_equalizer_redefine);

  /* sequencer_view.h */
  WM_operatortype_append(SEQUENCER_OT_sample);
  WM_operatortype_append(SEQUENCER_OT_view_all);
  WM_operatortype_append(SEQUENCER_OT_view_frame);
  WM_operatortype_append(SEQUENCER_OT_view_all_preview);
  WM_operatortype_append(SEQUENCER_OT_view_zoom_ratio);
  WM_operatortype_append(SEQUENCER_OT_view_selected);
  WM_operatortype_append(SEQUENCER_OT_view_ghost_border);

  /* `sequencer_channels_edit.cc` */
  WM_operatortype_append(SEQUENCER_OT_rename_channel);
}

void sequencer_keymap(wmKeyConfig *keyconf)
{
  /* Common items ------------------------------------------------------------------ */
  WM_keymap_ensure(keyconf, "Video Sequence Editor", SPACE_SEQ, RGN_TYPE_WINDOW);

  /* Strips Region --------------------------------------------------------------- */
  WM_keymap_ensure(keyconf, "Sequencer", SPACE_SEQ, RGN_TYPE_WINDOW);

  /* Preview Region ----------------------------------------------------------- */
  WM_keymap_ensure(keyconf, "Preview", SPACE_SEQ, RGN_TYPE_WINDOW);

  /* Channels Region ----------------------------------------------------------- */
  WM_keymap_ensure(keyconf, "Sequencer Channels", SPACE_SEQ, RGN_TYPE_WINDOW);

  slip_modal_keymap(keyconf);
}

void ED_operatormacros_sequencer()
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *otmacro;

  ot = WM_operatortype_append_macro("SEQUENCER_OT_duplicate_move",
                                    "Duplicate Strips",
                                    "Duplicate selected strips and move them",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "SEQUENCER_OT_duplicate");
  WM_operatortype_macro_define(ot, "TRANSFORM_OT_seq_slide");

  ot = WM_operatortype_append_macro("SEQUENCER_OT_duplicate_move_linked",
                                    "Duplicate Strips",
                                    "Duplicate selected strips, but not their data, and move them",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  otmacro = WM_operatortype_macro_define(ot, "SEQUENCER_OT_duplicate");
  RNA_boolean_set(otmacro->ptr, "linked", true);
  WM_operatortype_macro_define(ot, "TRANSFORM_OT_seq_slide");

  ot = WM_operatortype_append_macro("SEQUENCER_OT_preview_duplicate_move",
                                    "Duplicate Strips",
                                    "Duplicate selected strips and move them",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "SEQUENCER_OT_duplicate");
  WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");

  ot = WM_operatortype_append_macro("SEQUENCER_OT_preview_duplicate_move_linked",
                                    "Duplicate Strips",
                                    "Duplicate selected strips, but not their data, and move them",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  otmacro = WM_operatortype_macro_define(ot, "SEQUENCER_OT_duplicate");
  RNA_boolean_set(otmacro->ptr, "linked", true);
  WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");

  ot = WM_operatortype_append_macro("SEQUENCER_OT_retiming_add_freeze_frame_slide",
                                    "Add Freeze Frame And Slide",
                                    "Add freeze frame and move it",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "SEQUENCER_OT_retiming_freeze_frame_add");
  WM_operatortype_macro_define(ot, "TRANSFORM_OT_seq_slide");

  ot = WM_operatortype_append_macro(
      "SEQUENCER_OT_retiming_add_transition_slide",
      "Add Speed Transition And Slide",
      "Add smooth transition between 2 retimed segments and change its duration",
      OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "SEQUENCER_OT_retiming_transition_add");
  WM_operatortype_macro_define(ot, "TRANSFORM_OT_seq_slide");
}

}  // namespace blender::ed::vse
