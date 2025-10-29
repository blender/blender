/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>
#include <cstring>
#include <limits>

#include "BLI_math_vector_types.hh"
#include "BLO_readfile.hh"
#include "MEM_guardedalloc.h"

#include "ED_outliner.hh"
#include "ED_sequencer.hh"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"
#include "BLI_path_utils.hh"

#include "BKE_anim_data.hh"
#include "BKE_appdir.hh"
#include "BKE_blendfile.hh"
#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "SEQ_animation.hh"
#include "SEQ_iterator.hh"
#include "SEQ_relations.hh"
#include "SEQ_select.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"
#include "SEQ_utils.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "ANIM_action.hh"
#include "ANIM_action_legacy.hh"
#include "ANIM_animdata.hh"

#include "UI_view2d.hh"
#include "WM_api.hh"
#include "WM_types.hh"

#ifdef WITH_AUDASPACE
#  include <AUD_Special.h>
#endif

/* Own include. */
#include "sequencer_intern.hh"

namespace blender::ed::vse {

using namespace bke::blendfile;

/* -------------------------------------------------------------------- */
/* Copy Operator Helper functions
 */

static void sequencer_copy_animation_listbase(Scene *scene_src,
                                              Strip *strip_dst,
                                              ListBase *clipboard_dst,
                                              ListBase *fcurve_base_src)
{
  /* Add curves for strips inside meta strip. */
  if (strip_dst->type == STRIP_TYPE_META) {
    LISTBASE_FOREACH (Strip *, meta_child, &strip_dst->seqbase) {
      sequencer_copy_animation_listbase(scene_src, meta_child, clipboard_dst, fcurve_base_src);
    }
  }

  Vector<FCurve *> fcurves_src = animrig::fcurves_in_listbase_filtered(
      *fcurve_base_src,
      [&](const FCurve &fcurve) { return seq::fcurve_matches(*strip_dst, fcurve); });

  for (FCurve *fcu_src : fcurves_src) {
    BLI_addtail(clipboard_dst, BKE_fcurve_copy(fcu_src));
  }
}

/* This is effectively just a copy of `sequencer_copy_animation_listbase()`
 * above, except that it copies from an action's animation to a vector rather
 * than between two listbases. */
static void sequencer_copy_animation_to_vector(Scene *scene_src,
                                               Strip *strip_dst,
                                               Vector<FCurve *> &clipboard_dst,
                                               bAction &fcurves_src_action,
                                               animrig::slot_handle_t fcurves_src_slot_handle)
{
  /* Add curves for strips inside meta strip. */
  if (strip_dst->type == STRIP_TYPE_META) {
    LISTBASE_FOREACH (Strip *, meta_child, &strip_dst->seqbase) {
      sequencer_copy_animation_to_vector(
          scene_src, meta_child, clipboard_dst, fcurves_src_action, fcurves_src_slot_handle);
    }
  }

  Vector<FCurve *> fcurves_src = animrig::fcurves_in_action_slot_filtered(
      &fcurves_src_action, fcurves_src_slot_handle, [&](const FCurve &fcurve) {
        return seq::fcurve_matches(*strip_dst, fcurve);
      });

  for (FCurve *fcu_src : fcurves_src) {
    FCurve *fcu_copy = BKE_fcurve_copy(fcu_src);

    /* Handling groups properly requires more work, so for now just ignore them. */
    fcu_copy->grp = nullptr;

    clipboard_dst.append(fcu_copy);
  }
}

static void sequencer_copy_animation(Scene *scene_src,
                                     Vector<FCurve *> &fcurves_dst,
                                     ListBase *drivers_dst,
                                     Strip *strip_dst)
{
  if (seq::animation_keyframes_exist(scene_src)) {
    sequencer_copy_animation_to_vector(
        scene_src, strip_dst, fcurves_dst, *scene_src->adt->action, scene_src->adt->slot_handle);
  }
  if (seq::animation_drivers_exist(scene_src)) {
    sequencer_copy_animation_listbase(scene_src, strip_dst, drivers_dst, &scene_src->adt->drivers);
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
  PartialWriteContext copy_buffer{*bmain_src};
  const char *scene_name = "copybuffer_vse_scene";

  /* Add a dummy empty scene to the temporary Main copy buffer. */
  Scene *scene_dst = reinterpret_cast<Scene *>(
      copy_buffer.id_create(ID_SCE,
                            scene_name,
                            nullptr,
                            {(PartialWriteContext::IDAddOperations::SET_FAKE_USER |
                              PartialWriteContext::IDAddOperations::SET_CLIPBOARD_MARK)}));

  /* Create an empty sequence editor data to store all copied strips. */
  scene_dst->ed = MEM_callocN<Editing>(__func__);
  seq::seqbase_duplicate_recursive(bmain_src,
                                   scene_src,
                                   scene_dst,
                                   &scene_dst->ed->seqbase,
                                   scene_src->ed->current_strips(),
                                   seq::StripDuplicate::Selected,
                                   0);

  BLI_duplicatelist(&scene_dst->ed->channels, &scene_src->ed->channels);

  /* Save current frame and active strip. */
  scene_dst->r.cfra = scene_src->r.cfra;
  Strip *active_seq_src = seq::select_active_get(scene_src);
  if (active_seq_src) {
    Strip *strip_dst = static_cast<Strip *>(
        BLI_findstring(&scene_dst->ed->seqbase, active_seq_src->name, offsetof(Strip, name)));
    if (strip_dst) {
      seq::select_active_set(scene_dst, strip_dst);
    }
  }

  Vector<FCurve *> fcurves_dst = {};
  ListBase drivers_dst = {nullptr, nullptr};
  LISTBASE_FOREACH (Strip *, strip_dst, &scene_dst->ed->seqbase) {
    /* Copy any fcurves/drivers from `scene_src` that are relevant to `strip_dst`. */
    sequencer_copy_animation(scene_src, fcurves_dst, &drivers_dst, strip_dst);
  }

  BLI_assert(scene_dst->adt == nullptr);

  /* Copy over the fcurves. */
  if (!fcurves_dst.is_empty()) {
    scene_dst->adt = BKE_animdata_ensure_id(&scene_dst->id);
    animrig::Action &action_dst =
        reinterpret_cast<bAction *>(
            copy_buffer.id_create(
                ID_AC, scene_name, nullptr, {PartialWriteContext::IDAddOperations::SET_FAKE_USER}))
            ->wrap();

    /* Assign the `dst_action` as either legacy or layered, depending on what
     * the source action we're copying from is. */
    if (animrig::legacy::action_treat_as_legacy(*scene_src->adt->action)) {
      const bool success = animrig::assign_action(&action_dst, scene_dst->id);
      if (!success) {
        return false;
      }
    }
    else {
      /* If we're copying from a layered action, also ensure a connected slot. */
      animrig::Slot *slot = animrig::assign_action_ensure_slot_for_keying(action_dst,
                                                                          scene_dst->id);
      if (slot == nullptr) {
        return false;
      }
    }

    for (FCurve *fcurve : fcurves_dst) {
      animrig::action_fcurve_attach(action_dst,
                                    scene_dst->adt->slot_handle,
                                    *fcurve,
                                    fcurve->grp ? std::optional(fcurve->grp->name) : std::nullopt);
    }
  }

  /* Copy over the drivers. */
  if (!BLI_listbase_is_empty(&drivers_dst)) {
    scene_dst->adt = BKE_animdata_ensure_id(&scene_dst->id);
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

wmOperatorStatus sequencer_clipboard_copy_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);

  VectorSet<Strip *> selected = seq::query_selected_strips(ed->current_strips());

  if (selected.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  VectorSet<Strip *> effect_chain;
  effect_chain.add_multiple(selected);
  seq::iterator_set_expand(
      scene, ed->current_strips(), effect_chain, seq::query_strip_effect_chain);

  VectorSet<Strip *> expanded;
  for (Strip *strip : effect_chain) {
    if (!(strip->flag & SELECT)) {
      strip->flag |= SELECT;
      expanded.add(strip);
    }
  }

  char filepath[FILE_MAX];
  sequencer_copybuffer_filepath_get(filepath, sizeof(filepath));
  bool success = sequencer_write_copy_paste_file(bmain, scene, filepath, *op->reports);
  if (!success) {
    BKE_report(op->reports, RPT_ERROR, "Could not create the copy paste file!");
    for (Strip *strip : expanded) {
      strip->flag &= ~SELECT;
    }
    return OPERATOR_CANCELLED;
  }

  /* We are all done! */
  if (effect_chain.size() > selected.size()) {
    BKE_report(op->reports,
               RPT_INFO,
               "Copied the selected Video Sequencer strips and associated effect chain to "
               "internal clipboard");
  }
  else {
    BKE_report(
        op->reports, RPT_INFO, "Copied the selected Video Sequencer strips to internal clipboard");
  }
  ED_outliner_select_sync_from_sequence_tag(C);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);
  return OPERATOR_FINISHED;
}

/* -------------------------------------------------------------------- */
/* Paste Operator Helper functions
 */

static bool sequencer_paste_animation(Main *bmain_dst, Scene *scene_dst, Scene *scene_src)
{
  if (!seq::animation_keyframes_exist(scene_src) && !seq::animation_drivers_exist(scene_src)) {
    return false;
  }

  bAction *act_dst = animrig::id_action_ensure(bmain_dst, &scene_dst->id);

  /* For layered actions ensure we have an attached slot. */
  if (!animrig::legacy::action_treat_as_legacy(*act_dst)) {
    const animrig::Slot *slot = animrig::assign_action_ensure_slot_for_keying(act_dst->wrap(),
                                                                              scene_dst->id);
    BLI_assert(slot != nullptr);
    if (slot == nullptr) {
      return false;
    }
  }

  for (FCurve *fcu : animrig::legacy::fcurves_for_assigned_action(scene_src->adt)) {
    animrig::action_fcurve_attach(act_dst->wrap(),
                                  scene_dst->adt->slot_handle,
                                  *BKE_fcurve_copy(fcu),
                                  fcu->grp ? std::optional(fcu->grp->name) : std::nullopt);
  }
  LISTBASE_FOREACH (FCurve *, fcu, &scene_src->adt->drivers) {
    BLI_addtail(&scene_dst->adt->drivers, BKE_fcurve_copy(fcu));
  }

  return true;
}

wmOperatorStatus sequencer_clipboard_paste_invoke(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent *event)
{
  RNA_int_set(op->ptr, "x", event->mval[0]);
  RNA_int_set(op->ptr, "y", event->mval[1]);
  return sequencer_clipboard_paste_exec(C, op);
}

wmOperatorStatus sequencer_clipboard_paste_exec(bContext *C, wmOperator *op)
{
  char filepath[FILE_MAX];
  sequencer_copybuffer_filepath_get(filepath, sizeof(filepath));
  const BlendFileReadParams params{};
  BlendFileReadReport bf_reports{};
  BlendFileData *bfd = BKE_blendfile_read(filepath, &params, &bf_reports);
  const int mval[2] = {RNA_int_get(op->ptr, "x"), RNA_int_get(op->ptr, "y")};
  float2 view_mval;
  View2D *v2d = UI_view2d_fromcontext(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  UI_view2d_region_to_view(v2d, mval[0], mval[1], &view_mval[0], &view_mval[1]);

  /* For checking if region type is Preview. */
  ARegion *region = CTX_wm_region(C);

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
    if (scene_iter->id.flag & ID_FLAG_CLIPBOARD_MARK) {
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

  Scene *scene_dst = CTX_data_sequencer_scene(C);
  Editing *ed_dst = seq::editing_ensure(scene_dst); /* Creates "ed" if it's missing. */
  int ofs;

  deselect_all_strips(scene_dst);
  if (RNA_boolean_get(op->ptr, "keep_offset") || (region->regiontype == RGN_TYPE_PREVIEW)) {
    ofs = scene_dst->r.cfra - scene_src->r.cfra;
  }
  else {
    int min_seq_startdisp = std::numeric_limits<int>::max();
    LISTBASE_FOREACH (Strip *, strip, &scene_src->ed->seqbase) {
      min_seq_startdisp = std::min(seq::time_left_handle_frame_get(scene_src, strip),
                                   min_seq_startdisp);
    }
    /* Paste strips relative to the current-frame. */
    ofs = scene_dst->r.cfra - min_seq_startdisp;
  }

  Strip *prev_active_seq = seq::select_active_get(scene_src);
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
  seq::AnimationBackup animation_backup = {{nullptr}};
  seq::animation_backup_original(scene_dst, &animation_backup);
  bool has_animation = sequencer_paste_animation(bmain_dst, scene_dst, scene_src);

  ListBase nseqbase = {nullptr, nullptr};
  /* NOTE: seq::seqbase_duplicate_recursive() takes care of generating
   * new UIDs for sequences in the new list. */
  seq::seqbase_duplicate_recursive(bmain_dst,
                                   scene_src,
                                   scene_dst,
                                   &nseqbase,
                                   &scene_src->ed->seqbase,
                                   seq::StripDuplicate::Selected,
                                   0);

  /* BKE_main_merge will copy the scene_src and its action into bmain_dst. Remove them as
   * we merge the data from these manually.
   */
  if (has_animation && scene_src->adt->action != nullptr) {
    BKE_id_delete(bmain_dst, scene_src->adt->action);
  }
  BKE_id_delete(bmain_dst, scene_src);

  Strip *iseq_first = static_cast<Strip *>(nseqbase.first);
  BLI_movelisttolist(ed_dst->current_strips(), &nseqbase);
  /* Restore "first" pointer as BLI_movelisttolist sets it to nullptr */
  nseqbase.first = iseq_first;

  int2 strip_mean_pos = {0, 0};
  int image_strip_count = 0;
  LISTBASE_FOREACH (Strip *, istrip, &nseqbase) {
    if (istrip->name == active_seq_name) {
      seq::select_active_set(scene_dst, istrip);
    }
    /* Make sure, that pasted strips have unique names. This has to be done after
     * adding strips to seqbase, for lookup cache to work correctly. */
    seq::ensure_unique_name(istrip, scene_dst);

    if (region->regiontype == RGN_TYPE_PREVIEW && istrip->type != STRIP_TYPE_SOUND_RAM &&
        seq::must_render_strip(seq::query_all_strips(&nseqbase), istrip))
    {
      strip_mean_pos += static_cast<int2>(
          seq::image_transform_origin_offset_pixelspace_get(scene, istrip));
      image_strip_count++;
    }
  }

  if (image_strip_count > 0) {
    strip_mean_pos /= image_strip_count;
  }

  LISTBASE_FOREACH (Strip *, istrip, &nseqbase) {
    /* Place strips that generate an image at the mouse cursor. */
    if (region->regiontype == RGN_TYPE_PREVIEW && !RNA_boolean_get(op->ptr, "keep_offset") &&
        istrip->type != STRIP_TYPE_SOUND_RAM &&
        seq::must_render_strip(seq::query_all_strips(&nseqbase), istrip))
    {
      StripTransform *transform = istrip->data->transform;
      const float2 mirror = seq::image_transform_mirror_factor_get(istrip);
      const float2 origin = seq::image_transform_origin_offset_pixelspace_get(scene, istrip);
      transform->xofs = (view_mval[0] - (strip_mean_pos[0] - origin[0])) * mirror[0];
      transform->yofs = (view_mval[1] - (strip_mean_pos[1] - origin[1])) * mirror[1];
      seq::relations_invalidate_cache(scene, istrip);
    }
    /* Translate after name has been changed, otherwise this will affect animdata of original
     * strip. */
    seq::transform_translate_strip(scene_dst, istrip, ofs);
    /* Ensure, that pasted strips don't overlap. */
    if (seq::transform_test_overlap(scene_dst, ed_dst->current_strips(), istrip)) {
      seq::transform_seqbase_shuffle(ed_dst->current_strips(), istrip, scene_dst);
    }
  }

  seq::animation_restore_original(scene_dst, &animation_backup);

  DEG_id_tag_update(&scene_dst->id, ID_RECALC_SEQUENCER_STRIPS);
  if (scene_dst->adt && scene_dst->adt->action) {
    DEG_id_tag_update(&scene_dst->adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
  }
  DEG_relations_tag_update(bmain_dst);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene_dst);
  WM_event_add_notifier(C, NC_SCENE | ND_ANIMCHAN, scene_dst);
  ED_outliner_select_sync_from_sequence_tag(C);

  BKE_reportf(op->reports, RPT_INFO, "%d strips pasted", num_strips_to_paste);

  return OPERATOR_FINISHED;
}
}  // namespace blender::ed::vse
