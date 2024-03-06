/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstring>

#include "BLO_readfile.hh"
#include "MEM_guardedalloc.h"

#include "ED_outliner.hh"
#include "ED_sequencer.hh"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"

#include "BKE_appdir.hh"
#include "BKE_blender_copybuffer.hh"
#include "BKE_blendfile.hh"
#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "SEQ_animation.hh"
#include "SEQ_select.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"
#include "SEQ_utils.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "ANIM_animdata.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#ifdef WITH_AUDASPACE
#  include <AUD_Special.h>
#endif

/* Own include. */
#include "sequencer_intern.hh"

/* -------------------------------------------------------------------- */
/* Copy Operator Helper functions
 */

static int gather_strip_data_ids_to_null(LibraryIDLinkCallbackData *cb_data)
{
  blender::bke::id::IDRemapper &id_remapper = *static_cast<blender::bke::id::IDRemapper *>(
      cb_data->user_data);
  ID *id = *cb_data->id_pointer;

  /* We don't care about embedded, loop-back, or internal IDs. */
  if (cb_data->cb_flag & (IDWALK_CB_EMBEDDED | IDWALK_CB_EMBEDDED_NOT_OWNING)) {
    return IDWALK_RET_NOP;
  }
  if (cb_data->cb_flag & (IDWALK_CB_LOOPBACK | IDWALK_CB_INTERNAL)) {
    return IDWALK_RET_STOP_RECURSION;
  }

  if (id) {
    ID_Type id_type = GS((id)->name);
    /* Nullify everything that is not:
     * #bSound, #MovieClip, #Image, #Text, #VFont, #bAction, or #Collection IDs. */
    if (!ELEM(id_type, ID_SO, ID_MC, ID_IM, ID_TXT, ID_VF, ID_AC)) {
      id_remapper.add(id, nullptr);
      return IDWALK_RET_STOP_RECURSION;
    }
  }
  return IDWALK_RET_NOP;
}

static void sequencer_copy_animation_listbase(Scene *scene_src,
                                              Sequence *seq_dst,
                                              ListBase *clipboard_dst,
                                              ListBase *fcurve_base_src)
{
  /* Add curves for strips inside meta strip. */
  if (seq_dst->type == SEQ_TYPE_META) {
    LISTBASE_FOREACH (Sequence *, meta_child, &seq_dst->seqbase) {
      sequencer_copy_animation_listbase(scene_src, meta_child, clipboard_dst, fcurve_base_src);
    }
  }

  GSet *fcurves_src = SEQ_fcurves_by_strip_get(seq_dst, fcurve_base_src);
  if (fcurves_src == nullptr) {
    return;
  }

  GSET_FOREACH_BEGIN (FCurve *, fcu_src, fcurves_src) {
    BLI_addtail(clipboard_dst, BKE_fcurve_copy(fcu_src));
  }
  GSET_FOREACH_END();

  BLI_gset_free(fcurves_src, nullptr);
}

static void sequencer_copy_animation(Scene *scene_src,
                                     ListBase *fcurves_dst,
                                     ListBase *drivers_dst,
                                     Sequence *seq_dst)
{
  if (SEQ_animation_curves_exist(scene_src)) {
    sequencer_copy_animation_listbase(
        scene_src, seq_dst, fcurves_dst, &scene_src->adt->action->curves);
  }
  if (SEQ_animation_drivers_exist(scene_src)) {
    sequencer_copy_animation_listbase(scene_src, seq_dst, drivers_dst, &scene_src->adt->drivers);
  }
}

static void sequencer_copybuffer_filepath_get(char filepath[FILE_MAX], size_t filepath_maxncpy)
{
  BLI_path_join(filepath, filepath_maxncpy, BKE_tempdir_base(), "copybuffer_vse.blend");
}

static bool sequencer_write_copy_paste_file(Main *bmain_src,
                                            Scene *scene_src,
                                            const char *filepath,
                                            ReportList *reports)

{
  /* Ideally, scene should not be added to the global Main. There currently is no good
   * solution to avoid it if we want to properly pull in all strip dependencies. */
  Scene *scene_dst = BKE_scene_add(bmain_src, "copybuffer_vse_scene");

  /* Create a temporary scene that we will copy from.
   * This is needed as it is the scene that contains all the VSE strip data.
   */
  scene_dst->ed = MEM_cnew<Editing>(__func__);
  scene_dst->ed->seqbasep = &scene_dst->ed->seqbase;
  SEQ_sequence_base_dupli_recursive(
      scene_src, scene_dst, &scene_dst->ed->seqbase, scene_src->ed->seqbasep, 0, 0);

  BLI_duplicatelist(&scene_dst->ed->channels, &scene_src->ed->channels);
  scene_dst->ed->displayed_channels = &scene_dst->ed->channels;

  /* Save current frame and active strip. */
  scene_dst->r.cfra = scene_src->r.cfra;
  Sequence *active_seq_src = SEQ_select_active_get(scene_src);
  if (active_seq_src) {
    Sequence *seq_dst = static_cast<Sequence *>(
        BLI_findstring(&scene_dst->ed->seqbase, active_seq_src->name, offsetof(Sequence, name)));
    if (seq_dst) {
      SEQ_select_active_set(scene_dst, seq_dst);
    }
  }

  ListBase fcurves_dst = {nullptr, nullptr};
  ListBase drivers_dst = {nullptr, nullptr};
  LISTBASE_FOREACH (Sequence *, seq_dst, &scene_dst->ed->seqbase) {
    /* Copy animation curves from seq_dst (if any). */
    sequencer_copy_animation(scene_src, &fcurves_dst, &drivers_dst, seq_dst);
  }

  if (!BLI_listbase_is_empty(&fcurves_dst) || !BLI_listbase_is_empty(&drivers_dst)) {
    BLI_assert(scene_dst->adt == nullptr);
    bAction *act_dst = blender::animrig::id_action_ensure(bmain_src, &scene_dst->id);
    BLI_movelisttolist(&act_dst->curves, &fcurves_dst);
    BLI_movelisttolist(&scene_dst->adt->drivers, &drivers_dst);
  }

  /* Nullify all ID pointers that we don't want to copy. For example, we don't want
   * to copy whole scenes. We have to come up with a proper idea of how to copy and
   * paste scene strips.
   */
  blender::bke::id::IDRemapper id_remapper;
  BKE_library_foreach_ID_link(
      bmain_src, &scene_dst->id, gather_strip_data_ids_to_null, &id_remapper, IDWALK_RECURSE);

  BKE_libblock_relink_multiple(bmain_src,
                               {&scene_dst->id},
                               ID_REMAP_TYPE_REMAP,
                               id_remapper,
                               (ID_REMAP_SKIP_USER_CLEAR | ID_REMAP_SKIP_USER_REFCOUNT));

  /* Ensure that there are no old copy tags around */
  BKE_blendfile_write_partial_begin(bmain_src);
  /* Tag the scene copy so we can pull in all scrip dependencies. */
  BKE_copybuffer_copy_tag_ID(&scene_dst->id);
  /* Create the copy/paste temp file */
  bool retval = BKE_copybuffer_copy_end(bmain_src, filepath, reports);

  /* Clean up the action ID if we created any. */
  if (scene_dst->adt != nullptr && scene_dst->adt->action != nullptr) {
    BKE_id_delete(bmain_src, scene_dst->adt->action);
  }

  /* Cleanup the dummy scene file */
  BKE_id_delete(bmain_src, scene_dst);

  return retval;
}

int sequencer_clipboard_copy_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);

  if (SEQ_transform_seqbase_isolated_sel_check(ed->seqbasep) == false) {
    BKE_report(op->reports, RPT_ERROR, "Please select all related strips");
    return OPERATOR_CANCELLED;
  }

  char filepath[FILE_MAX];
  sequencer_copybuffer_filepath_get(filepath, sizeof(filepath));
  bool success = sequencer_write_copy_paste_file(bmain, scene, filepath, op->reports);
  if (!success) {
    BKE_report(op->reports, RPT_ERROR, "Could not create the copy paste file!");
    return OPERATOR_CANCELLED;
  }

  /* We are all done! */
  BKE_report(
      op->reports, RPT_INFO, "Copied the selected Video Sequencer strips to internal clipboard");
  return OPERATOR_FINISHED;
}

/* -------------------------------------------------------------------- */
/* Paste Operator Helper functions
 */

static bool sequencer_paste_animation(Main *bmain_dst, Scene *scene_dst, Scene *scene_src)
{
  if (!SEQ_animation_curves_exist(scene_src) && !SEQ_animation_drivers_exist(scene_src)) {
    return false;
  }

  bAction *act_dst;

  if (scene_dst->adt != nullptr && scene_dst->adt->action != nullptr) {
    act_dst = scene_dst->adt->action;
  }
  else {
    /* get action to add F-Curve+keyframe to */
    act_dst = blender::animrig::id_action_ensure(bmain_dst, &scene_dst->id);
  }

  LISTBASE_FOREACH (FCurve *, fcu, &scene_src->adt->action->curves) {
    BLI_addtail(&act_dst->curves, BKE_fcurve_copy(fcu));
  }
  LISTBASE_FOREACH (FCurve *, fcu, &scene_src->adt->drivers) {
    BLI_addtail(&scene_dst->adt->drivers, BKE_fcurve_copy(fcu));
  }

  return true;
}

int sequencer_clipboard_paste_exec(bContext *C, wmOperator *op)
{
  char filepath[FILE_MAX];
  sequencer_copybuffer_filepath_get(filepath, sizeof(filepath));
  const BlendFileReadParams params{};
  BlendFileReadReport bf_reports{};
  BlendFileData *bfd = BKE_blendfile_read(filepath, &params, &bf_reports);

  if (bfd == nullptr) {
    BKE_report(op->reports, RPT_INFO, "No data to paste");
    return OPERATOR_CANCELLED;
  }

  Main *bmain_src = bfd->main;
  bfd->main = nullptr;
  BLO_blendfiledata_free(bfd);

  Scene *scene_src = nullptr;
  /* Find the scene we pasted that contains the strips. It should be tagged. */
  LISTBASE_FOREACH (Scene *, scene_iter, &bmain_src->scenes) {
    if (scene_iter->id.flag & LIB_CLIPBOARD_MARK) {
      scene_src = scene_iter;
      break;
    }
  }

  if (!scene_src || !scene_src->ed) {
    BKE_report(op->reports, RPT_ERROR, "No clipboard scene to paste Video Sequencer data from");
    BKE_main_free(bmain_src);
    return OPERATOR_CANCELLED;
  }

  const int num_strips_to_paste = BLI_listbase_count(&scene_src->ed->seqbase);
  if (num_strips_to_paste == 0) {
    BKE_report(op->reports, RPT_INFO, "No strips to paste");
    BKE_main_free(bmain_src);
    return OPERATOR_CANCELLED;
  }

  Scene *scene_dst = CTX_data_scene(C);
  Editing *ed_dst = SEQ_editing_ensure(scene_dst); /* Creates "ed" if it's missing. */
  int ofs;

  ED_sequencer_deselect_all(scene_dst);
  if (RNA_boolean_get(op->ptr, "keep_offset")) {
    ofs = scene_dst->r.cfra - scene_src->r.cfra;
  }
  else {
    int min_seq_startdisp = INT_MAX;
    LISTBASE_FOREACH (Sequence *, seq, &scene_src->ed->seqbase) {
      if (SEQ_time_left_handle_frame_get(scene_src, seq) < min_seq_startdisp) {
        min_seq_startdisp = SEQ_time_left_handle_frame_get(scene_src, seq);
      }
    }
    /* Paste strips relative to the current-frame. */
    ofs = scene_dst->r.cfra - min_seq_startdisp;
  }

  Sequence *prev_active_seq = SEQ_select_active_get(scene_src);
  std::string active_seq_name;
  if (prev_active_seq) {
    active_seq_name.assign(prev_active_seq->name);
  }

  /* Make sure we have all data IDs we need in bmain_dst. Remap the IDs if we already have them.
   * This has to happen BEFORE we move the strip over to scene_dst. their ID mapping will not be
   * correct otherwise. */
  Main *bmain_dst = CTX_data_main(C);
  MainMergeReport merge_reports = {};
  /* NOTE: BKE_main_merge will free bmain_src! */
  BKE_main_merge(bmain_dst, &bmain_src, merge_reports);

  /* Paste animation.
   * NOTE: Only fcurves and drivers are copied. NLA action strips are not copied.
   * First backup original curves from scene and move curves from clipboard into scene. This way,
   * when pasted strips are renamed, pasted fcurves are renamed with them. Finally restore original
   * curves from backup.
   */
  SeqAnimationBackup animation_backup = {{nullptr}};
  SEQ_animation_backup_original(scene_dst, &animation_backup);
  bool has_animation = sequencer_paste_animation(bmain_dst, scene_dst, scene_src);

  ListBase nseqbase = {nullptr, nullptr};
  /* NOTE: SEQ_sequence_base_dupli_recursive() takes care of generating
   * new UIDs for sequences in the new list. */
  SEQ_sequence_base_dupli_recursive(
      scene_src, scene_dst, &nseqbase, &scene_src->ed->seqbase, 0, 0);

  /* BKE_main_merge will copy the scene_src and its action into bmain_dst. Remove them as
   * we merge the data from these manually.
   */
  if (has_animation) {
    BKE_id_delete(bmain_dst, scene_src->adt->action);
  }
  BKE_id_delete(bmain_dst, scene_src);

  Sequence *iseq_first = static_cast<Sequence *>(nseqbase.first);
  BLI_movelisttolist(ed_dst->seqbasep, &nseqbase);
  /* Restore "first" pointer as BLI_movelisttolist sets it to nullptr */
  nseqbase.first = iseq_first;

  LISTBASE_FOREACH (Sequence *, iseq, &nseqbase) {
    if (iseq->name == active_seq_name) {
      SEQ_select_active_set(scene_dst, iseq);
    }
    /* Make sure, that pasted strips have unique names. This has to be done after
     * adding strips to seqbase, for lookup cache to work correctly. */
    SEQ_ensure_unique_name(iseq, scene_dst);
  }

  LISTBASE_FOREACH (Sequence *, iseq, &nseqbase) {
    /* Translate after name has been changed, otherwise this will affect animdata of original
     * strip. */
    SEQ_transform_translate_sequence(scene_dst, iseq, ofs);
    /* Ensure, that pasted strips don't overlap. */
    if (SEQ_transform_test_overlap(scene_dst, ed_dst->seqbasep, iseq)) {
      SEQ_transform_seqbase_shuffle(ed_dst->seqbasep, iseq, scene_dst);
    }
  }

  SEQ_animation_restore_original(scene_dst, &animation_backup);

  DEG_id_tag_update(&scene_dst->id, ID_RECALC_SEQUENCER_STRIPS);
  DEG_relations_tag_update(bmain_dst);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene_dst);
  ED_outliner_select_sync_from_sequence_tag(C);

  BKE_reportf(op->reports, RPT_INFO, "%d strips pasted", num_strips_to_paste);

  return OPERATOR_FINISHED;
}
