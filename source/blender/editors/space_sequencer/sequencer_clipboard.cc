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
#include "BLO_writefile.hh"
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

#include "BKE_anim_data.hh"
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

using namespace blender::bke::blendfile;

/* -------------------------------------------------------------------- */
/* Copy Operator Helper functions
 */

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
                                            ReportList &reports)

{
  /* NOTE: Setting the same current file path as G_MAIN is necessary for now to get correct
   * external filepaths when writing the partial write context on disk. otherwise, filepaths from
   * the scene's sequencer strips (e.g. image ones) would also need to be remapped in this code. */
  PartialWriteContext copy_buffer{bmain_src->filepath};
  const char *scene_name = "copybuffer_vse_scene";

  /* Add a dummy empty scene to the temporary Main copy buffer. */
  Scene *scene_dst = reinterpret_cast<Scene *>(
      copy_buffer.id_create(ID_SCE,
                            scene_name,
                            nullptr,
                            {PartialWriteContext::IDAddOperations(
                                PartialWriteContext::IDAddOperations::SET_FAKE_USER |
                                PartialWriteContext::IDAddOperations::SET_CLIPBOARD_MARK)}));

  /* Create an empty sequence editor data to store all copied strips. */
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
    scene_dst->adt = BKE_animdata_ensure_id(&scene_dst->id);
    scene_dst->adt->action = reinterpret_cast<bAction *>(copy_buffer.id_create(
        ID_AC, scene_name, nullptr, {PartialWriteContext::IDAddOperations::SET_FAKE_USER}));
    BLI_movelisttolist(&scene_dst->adt->action->curves, &fcurves_dst);
    BLI_movelisttolist(&scene_dst->adt->drivers, &drivers_dst);
  }

  /* Only add to the paste buffer some dependency ID types. For example, scenes are ignored/cleared
   * (how to copy and paste scene strips is not clear currently).
   */
  /* NOTE: since a special Scene root ID needs to be forged for the VSE copy/paste (instead of
   * directly using the current scene and adding it to the paste buffer), the first level of
   * dependencies (IDs directly used by the scene) need to be processed manually here.
   *
   * All other indirect dependencies will then be handled automatically by the partial write
   * context code.
   */
#define VSE_COPYBUFFER_IDTYPES ID_SO, ID_MC, ID_IM, ID_TXT, ID_VF, ID_AC
  auto add_scene_ids_dependencies_cb = [&copy_buffer,
                                        scene_dst](LibraryIDLinkCallbackData *cb_data) -> int {
    ID *id_src = *cb_data->id_pointer;

    /* Embedded or null IDs usages can be ignored here. */
    if (cb_data->cb_flag & (IDWALK_CB_EMBEDDED | IDWALK_CB_EMBEDDED_NOT_OWNING)) {
      return IDWALK_RET_NOP;
    }
    if (!id_src) {
      return IDWALK_RET_NOP;
    }

    /* The Action ID of the destination scene has already been added (created actually) in the copy
     * buffer. This is necessary to ensure that only the relevant sequencer-related animation data
     * is copied into the destination paste buffer, and not the whole scene's animation. See the
     * code around the call to #sequencer_copy_animation above.
     *
     * So trying to add it again here would lead to serious issues. */
    if (scene_dst->adt && scene_dst->adt->action == reinterpret_cast<bAction *>(id_src)) {
      BLI_assert(GS(id_src->name) == ID_AC);
      return IDWALK_RET_NOP;
    }

    ID *id_dst = nullptr;
    const ID_Type id_type = GS((id_src)->name);
    /* Only add (and follow) IDs which usage is marked as 'never null', or are from following
     * types: #bSound, #MovieClip, #Image, #Text, #VFont, #bAction. */
    if (ELEM(id_type, VSE_COPYBUFFER_IDTYPES) || (cb_data->cb_flag & IDWALK_CB_NEVER_NULL)) {
      /* The partial write context handle dependencies of ID added to it. This callback will tell
       * it whether a given dependency ID should be skipped/cleared, or also added in the context.
       */
      auto partial_write_dependencies_filter_cb = [](LibraryIDLinkCallbackData *cb_deps_data,
                                                     PartialWriteContext::IDAddOptions /*options*/)
          -> PartialWriteContext::IDAddOperations {
        ID *id_deps_src = *cb_deps_data->id_pointer;
        const ID_Type id_type = GS((id_deps_src)->name);
        if (ELEM(id_type, VSE_COPYBUFFER_IDTYPES) ||
            (cb_deps_data->cb_flag & IDWALK_CB_NEVER_NULL))
        {
          return PartialWriteContext::IDAddOperations::ADD_DEPENDENCIES;
        }
        return PartialWriteContext::IDAddOperations::CLEAR_DEPENDENCIES;
      };
      id_dst = copy_buffer.id_add(id_src,
                                  {PartialWriteContext::IDAddOperations::NOP},
                                  partial_write_dependencies_filter_cb);
    }
    *cb_data->id_pointer = id_dst;
    return IDWALK_RET_NOP;
  };
  BKE_library_foreach_ID_link(
      nullptr, &scene_dst->id, add_scene_ids_dependencies_cb, nullptr, IDWALK_NOP);
#undef VSE_COPYBUFFER_IDTYPES

  BLI_assert(copy_buffer.is_valid());

  const bool retval = copy_buffer.write(filepath, reports);

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
  bool success = sequencer_write_copy_paste_file(bmain, scene, filepath, *op->reports);
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
