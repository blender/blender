/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include <cmath>
#include <string>

#include <fmt/format.h>

#include "ANIM_action.hh"
#include "ANIM_action_iterators.hh"
#include "ANIM_action_legacy.hh"
#include "ANIM_animdata.hh"
#include "ANIM_fcurve.hh"
#include "ANIM_keyframing.hh"
#include "ANIM_rna.hh"
#include "ANIM_visualkey.hh"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_fcurve.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_nla.hh"
#include "BKE_report.hh"

#include "DNA_scene_types.h"

#include "BLI_bit_vector.hh"
#include "BLI_dynstr.h"
#include "BLI_math_base.h"
#include "BLI_utildefines.h"
#include "BLT_translation.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"
#include "DNA_anim_types.h"
#include "MEM_guardedalloc.h"
#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "WM_types.hh"

namespace blender::animrig {

CombinedKeyingResult::CombinedKeyingResult()
{
  result_counter.fill(0);
}

void CombinedKeyingResult::add(const SingleKeyingResult result, const int count)
{
  result_counter[int(result)] += count;
}

void CombinedKeyingResult::merge(const CombinedKeyingResult &other)
{
  for (int i = 0; i < result_counter.size(); i++) {
    result_counter[i] += other.result_counter[i];
  }
}

int CombinedKeyingResult::get_count(const SingleKeyingResult result) const
{
  return result_counter[int(result)];
}

bool CombinedKeyingResult::has_errors() const
{
  /* For loop starts at 1 to skip the SUCCESS flag. Assumes that SUCCESS is 0 and the rest of the
   * enum are sequential values. */
  static_assert(int(SingleKeyingResult::SUCCESS) == 0);
  for (int i = 1; i < result_counter.size(); i++) {
    if (result_counter[i] > 0) {
      return true;
    }
  }
  return false;
}

void CombinedKeyingResult::generate_reports(ReportList *reports, const eReportType report_level)
{
  if (!this->has_errors() && this->get_count(SingleKeyingResult::SUCCESS) == 0) {
    BKE_reportf(
        reports, RPT_WARNING, "No keys have been inserted and no errors have been reported.");
    return;
  }

  Vector<std::string> errors;
  if (this->get_count(SingleKeyingResult::UNKNOWN_FAILURE) > 0) {
    const int error_count = this->get_count(SingleKeyingResult::UNKNOWN_FAILURE);
    errors.append(fmt::format(
        fmt::runtime(RPT_("There were {:d} keying failures for unknown reasons.")), error_count));
  }

  if (this->get_count(SingleKeyingResult::CANNOT_CREATE_FCURVE) > 0) {
    const int error_count = this->get_count(SingleKeyingResult::CANNOT_CREATE_FCURVE);
    errors.append(fmt::format(
        fmt::runtime(RPT_("Could not create {:d} F-Curve(s). This can happen when only "
                          "inserting to available F-Curves.")),
        error_count));
  }

  if (this->get_count(SingleKeyingResult::FCURVE_NOT_KEYFRAMEABLE) > 0) {
    const int error_count = this->get_count(SingleKeyingResult::FCURVE_NOT_KEYFRAMEABLE);
    errors.append(
        fmt::format(fmt::runtime(RPT_(
                        "{:d} F-Curve(s) are not keyframeable. They might be locked or sampled.")),
                    error_count));
  }

  if (this->get_count(SingleKeyingResult::NO_KEY_NEEDED) > 0) {
    const int error_count = this->get_count(SingleKeyingResult::NO_KEY_NEEDED);
    errors.append(fmt::format(
        fmt::runtime(RPT_(
            "Due to the setting 'Only Insert Needed', {:d} keyframe(s) have not been inserted.")),
        error_count));
  }

  if (this->get_count(SingleKeyingResult::UNABLE_TO_INSERT_TO_NLA_STACK) > 0) {
    const int error_count = this->get_count(SingleKeyingResult::UNABLE_TO_INSERT_TO_NLA_STACK);
    errors.append(fmt::format(
        fmt::runtime(RPT_("Due to the NLA stack setup, {:d} keyframe(s) have not been inserted.")),
        error_count));
  }

  if (this->get_count(SingleKeyingResult::ID_NOT_EDITABLE) > 0) {
    const int error_count = this->get_count(SingleKeyingResult::ID_NOT_EDITABLE);
    errors.append(fmt::format(
        fmt::runtime(RPT_("Inserting keys on {:d} data-block(s) has been skipped because "
                          "they are not editable.")),
        error_count));
  }

  if (this->get_count(SingleKeyingResult::ID_NOT_ANIMATABLE) > 0) {
    const int error_count = this->get_count(SingleKeyingResult::ID_NOT_ANIMATABLE);
    errors.append(fmt::format(
        fmt::runtime(RPT_("Inserting keys on {:d} data-block(s) has been skipped because "
                          "they cannot be animated.")),
        error_count));
  }

  if (this->get_count(SingleKeyingResult::CANNOT_RESOLVE_PATH) > 0) {
    const int error_count = this->get_count(SingleKeyingResult::CANNOT_RESOLVE_PATH);
    errors.append(fmt::format(
        fmt::runtime(RPT_("Inserting keys on {:d} data-block(s) has been skipped because "
                          "the RNA path wasn't valid for them.")),
        error_count));
  }

  if (this->get_count(SingleKeyingResult::NO_VALID_LAYER) > 0) {
    const int error_count = this->get_count(SingleKeyingResult::NO_VALID_LAYER);
    errors.append(fmt::format(
        fmt::runtime(RPT_("Inserting keys on {:d} data-block(s) has been skipped because "
                          "there were no layers that could accept the keys.")),
        error_count));
  }

  if (this->get_count(SingleKeyingResult::NO_VALID_STRIP) > 0) {
    const int error_count = this->get_count(SingleKeyingResult::NO_VALID_STRIP);
    errors.append(fmt::format(
        fmt::runtime(RPT_("Inserting keys on {:d} data-block(s) has been skipped because "
                          "there were no strips that could accept the keys.")),
        error_count));
  }

  if (this->get_count(SingleKeyingResult::NO_VALID_SLOT) > 0) {
    const int error_count = this->get_count(SingleKeyingResult::NO_VALID_SLOT);
    errors.append(fmt::format(
        fmt::runtime(RPT_("Inserting keys on {:d} data-block(s) has been skipped because "
                          "of missing action slots.")),
        error_count));
  }

  if (errors.is_empty()) {
    BKE_report(reports, RPT_WARNING, "Encountered unhandled error during keyframing");
    return;
  }

  if (errors.size() == 1) {
    BKE_report(reports, report_level, errors[0].c_str());
    return;
  }

  std::string error_message = RPT_("Inserting keyframes failed:");
  for (const std::string &error : errors) {
    error_message.append(fmt::format("\n- {}", error));
  }
  BKE_report(reports, report_level, error_message.c_str());
}

const std::optional<StringRefNull> default_channel_group_for_path(
    const PointerRNA *animated_struct, const StringRef prop_rna_path)
{
  if (animated_struct->type == &RNA_PoseBone) {
    bPoseChannel *pose_channel = static_cast<bPoseChannel *>(animated_struct->data);
    BLI_assert(pose_channel->name != nullptr);
    return pose_channel->name;
  }

  if (animated_struct->type == &RNA_Object) {
    if (prop_rna_path.find("location") != StringRef::not_found ||
        prop_rna_path.find("rotation") != StringRef::not_found ||
        prop_rna_path.find("scale") != StringRef::not_found)
    {
      /* NOTE: Keep this label in sync with the "ID" case in
       * keyingsets_utils.py :: get_transform_generators_base_info()
       */
      return "Object Transforms";
    }
  }

  return std::nullopt;
}

void update_autoflags_fcurve_direct(FCurve *fcu, PropertyRNA *prop)
{
  /* Set additional flags for the F-Curve (i.e. only integer values). */
  fcu->flag &= ~(FCURVE_INT_VALUES | FCURVE_DISCRETE_VALUES);
  switch (RNA_property_type(prop)) {
    case PROP_FLOAT:
      /* Do nothing. */
      break;
    case PROP_INT:
      /* Do integer (only 'whole' numbers) interpolation between all points. */
      fcu->flag |= FCURVE_INT_VALUES;
      break;
    default:
      /* Do 'discrete' (i.e. enum, boolean values which cannot take any intermediate
       * values at all) interpolation between all points.
       *    - however, we must also ensure that evaluated values are only integers still.
       */
      fcu->flag |= (FCURVE_DISCRETE_VALUES | FCURVE_INT_VALUES);
      break;
  }
}

bool is_keying_flag(const Scene *scene, const eKeying_Flag flag)
{
  if (scene) {
    return (scene->toolsettings->keying_flag & flag) || (U.keying_flag & flag);
  }
  return U.keying_flag & flag;
}

eInsertKeyFlags get_keyframing_flags(Scene *scene)
{
  eInsertKeyFlags flag = INSERTKEY_NOFLAGS;

  /* Visual keying. */
  if (is_keying_flag(scene, KEYING_FLAG_VISUALKEY)) {
    flag |= INSERTKEY_MATRIX;
  }

  /* Cycle-aware keyframe insertion - preserve cycle period and flow. */
  if (is_keying_flag(scene, KEYING_FLAG_CYCLEAWARE)) {
    flag |= INSERTKEY_CYCLE_AWARE;
  }

  if (is_keying_flag(scene, MANUALKEY_FLAG_INSERTNEEDED)) {
    flag |= INSERTKEY_NEEDED;
  }

  return flag;
}

/**
 * Checks whether the Action assigned to `adt` (if any) has any keyframes at the
 * given frame. Since we're only concerned whether a keyframe exists, we can
 * simply loop until a match is found.
 *
 * For layered actions, this only checks for keyframes in the assigned slot.
 */
static bool assigned_action_has_keyframe_at(AnimData &adt, const float frame)
{
  if (adt.action == nullptr) {
    return false;
  }

  if (adt.action->flag & ACT_MUTED) {
    return false;
  }

  for (FCurve *fcu : blender::animrig::legacy::fcurves_for_assigned_action(&adt)) {
    if (fcurve_frame_has_keyframe(fcu, frame)) {
      return true;
    }
  }

  return false;
}

/* Checks whether an Object has a keyframe for a given frame. */
static bool object_frame_has_keyframe(Object *ob, const float frame)
{
  if (ob == nullptr) {
    return false;
  }

  /* Check its own animation data - specifically, the action it contains. */
  if ((ob->adt) && (ob->adt->action)) {
    /* #41525 - When the active action is a NLA strip being edited,
     * we need to correct the frame number to "look inside" the
     * remapped action
     */
    const float ob_frame = BKE_nla_tweakedit_remap(ob->adt, frame, NLATIME_CONVERT_UNMAP);

    if (assigned_action_has_keyframe_at(*ob->adt, ob_frame)) {
      return true;
    }
  }

  /* nothing found */
  return false;
}

bool id_frame_has_keyframe(ID *id, float frame)
{
  if (id == nullptr) {
    return false;
  }

  /* Perform special checks for 'macro' types. */
  switch (GS(id->name)) {
    case ID_OB:
      return object_frame_has_keyframe((Object *)id, frame);

    default: {
      AnimData *adt = BKE_animdata_from_id(id);

      /* only check keyframes in active action */
      if (adt) {
        return assigned_action_has_keyframe_at(*adt, frame);
      }
      break;
    }
  }

  return false;
}

bool key_insertion_may_create_fcurve(const eInsertKeyFlags insert_key_flags)
{
  return (insert_key_flags & (INSERTKEY_REPLACE | INSERTKEY_AVAILABLE)) == 0;
}

/** Used to make curves newly added to a cyclic Action cycle with the correct period. */
static void make_new_fcurve_cyclic(FCurve *fcu, const blender::float2 &action_range)
{
  /* The curve must contain one (newly-added) keyframe. */
  if (fcu->totvert != 1 || !fcu->bezt) {
    return;
  }

  const float period = action_range[1] - action_range[0];

  if (period < 0.1f) {
    return;
  }

  /* Move the keyframe into the range. */
  const float frame_offset = fcu->bezt[0].vec[1][0] - action_range[0];
  const float fix = floorf(frame_offset / period) * period;

  fcu->bezt[0].vec[0][0] -= fix;
  fcu->bezt[0].vec[1][0] -= fix;
  fcu->bezt[0].vec[2][0] -= fix;

  /* Duplicate and offset the keyframe. */
  fcu->bezt = static_cast<BezTriple *>(MEM_reallocN(fcu->bezt, sizeof(BezTriple) * 2));
  fcu->totvert = 2;

  fcu->bezt[1] = fcu->bezt[0];
  fcu->bezt[1].vec[0][0] += period;
  fcu->bezt[1].vec[1][0] += period;
  fcu->bezt[1].vec[2][0] += period;

  if (!fcu->modifiers.first) {
    add_fmodifier(&fcu->modifiers, FMODIFIER_TYPE_CYCLES, fcu);
  }
}

/* Check indices that were intended to be remapped and report any failed remaps. */
static void get_keyframe_values_create_reports(ReportList *reports,
                                               PointerRNA ptr,
                                               PropertyRNA *prop,
                                               const int index,
                                               const int count,
                                               const bool force_all,
                                               const BitSpan successful_remaps)
{

  DynStr *ds_failed_indices = BLI_dynstr_new();

  int total_failed = 0;
  for (int i = 0; i < count; i++) {
    const bool cur_index_evaluated = ELEM(index, i, -1) || force_all;
    if (!cur_index_evaluated) {
      /* `values[i]` was never intended to be remapped. */
      continue;
    }

    if (successful_remaps[i]) {
      /* `values[i]` successfully remapped. */
      continue;
    }

    total_failed++;
    /* Report that `values[i]` were intended to be remapped but failed remapping process. */
    BLI_dynstr_appendf(ds_failed_indices, "%d, ", i);
  }

  if (total_failed == 0) {
    BLI_dynstr_free(ds_failed_indices);
    return;
  }

  char *str_failed_indices = BLI_dynstr_get_cstring(ds_failed_indices);
  BLI_dynstr_free(ds_failed_indices);

  BKE_reportf(reports,
              RPT_WARNING,
              "Could not insert %i keyframe(s) due to zero NLA influence, base value, or value "
              "remapping failed: %s.%s for indices [%s]",
              total_failed,
              ptr.owner_id->name,
              RNA_property_ui_name(prop),
              str_failed_indices);

  MEM_freeN(str_failed_indices);
}

static Vector<float> get_keyframe_values(PointerRNA *ptr, PropertyRNA *prop, const bool visual_key)
{
  Vector<float> values;

  if (visual_key && visualkey_can_use(ptr, prop)) {
    /* Visual-keying is only available for object and pchan datablocks, as
     * it works by keyframing using a value extracted from the final matrix
     * instead of using the kt system to extract a value.
     */
    values = visualkey_get_values(ptr, prop);
  }
  else {
    values = get_rna_values(ptr, prop);
  }
  return values;
}

static BitVector<> nla_map_keyframe_values_and_generate_reports(
    const MutableSpan<float> values,
    const int index,
    PointerRNA &ptr,
    PropertyRNA &prop,
    NlaKeyframingContext *nla_context,
    const AnimationEvalContext *anim_eval_context,
    ReportList *reports,
    bool *force_all)
{
  BitVector<> successful_remaps(values.size(), false);
  BKE_animsys_nla_remap_keyframe_values(
      nla_context, &ptr, &prop, values, index, anim_eval_context, force_all, successful_remaps);
  get_keyframe_values_create_reports(
      reports, ptr, &prop, index, values.size(), false, successful_remaps);
  return successful_remaps;
}

static float nla_time_remap(float time,
                            const AnimationEvalContext *anim_eval_context,
                            PointerRNA *id_ptr,
                            AnimData *adt,
                            bAction *act,
                            ListBase *nla_cache,
                            NlaKeyframingContext **r_nla_context)
{
  if (adt && adt->action == act) {
    *r_nla_context = BKE_animsys_get_nla_keyframing_context(
        nla_cache, id_ptr, adt, anim_eval_context);

    const float remapped_frame = BKE_nla_tweakedit_remap(adt, time, NLATIME_CONVERT_UNMAP);
    return remapped_frame;
  }

  *r_nla_context = nullptr;
  return time;
}

/* Insert the specified keyframe value into a single F-Curve. */
static SingleKeyingResult insert_keyframe_value(
    FCurve *fcu, float cfra, float curval, eBezTriple_KeyframeType keytype, eInsertKeyFlags flag)
{
  if (!BKE_fcurve_is_keyframable(fcu)) {
    return SingleKeyingResult::FCURVE_NOT_KEYFRAMEABLE;
  }

  KeyframeSettings settings = get_keyframe_settings((flag & INSERTKEY_NO_USERPREF) == 0);
  settings.keyframe_type = keytype;

  return insert_vert_fcurve(fcu, {cfra, curval}, settings, flag);
}

bool insert_keyframe_direct(ReportList *reports,
                            PointerRNA ptr,
                            PropertyRNA *prop,
                            FCurve *fcu,
                            const AnimationEvalContext *anim_eval_context,
                            eBezTriple_KeyframeType keytype,
                            NlaKeyframingContext *nla_context,
                            eInsertKeyFlags flag)
{

  if (fcu == nullptr) {
    BKE_report(reports, RPT_ERROR, "No F-Curve to add keyframes to");
    return false;
  }

  if ((ptr.owner_id == nullptr) && (ptr.data == nullptr)) {
    BKE_report(
        reports, RPT_ERROR, "No RNA pointer available to retrieve values for keyframing from");
    return false;
  }

  if (prop == nullptr) {
    PointerRNA tmp_ptr;

    if (RNA_path_resolve_property(&ptr, fcu->rna_path, &tmp_ptr, &prop) == false) {
      const char *idname = (ptr.owner_id) ? ptr.owner_id->name : RPT_("<No ID pointer>");

      BKE_reportf(reports,
                  RPT_ERROR,
                  "Could not insert keyframe, as RNA path is invalid for the given ID (ID = %s, "
                  "path = %s)",
                  idname,
                  fcu->rna_path);
      return false;
    }

    /* Property found, so overwrite 'ptr' to make later code easier. */
    ptr = tmp_ptr;
  }

  /* Update F-Curve flags to ensure proper behavior for property type. */
  update_autoflags_fcurve_direct(fcu, prop);

  const int index = fcu->array_index;
  const bool visual_keyframing = flag & INSERTKEY_MATRIX;
  Vector<float> values = get_keyframe_values(&ptr, prop, visual_keyframing);

  BitVector<> successful_remaps = nla_map_keyframe_values_and_generate_reports(
      values.as_mutable_span(),
      index,
      ptr,
      *prop,
      nla_context,
      anim_eval_context,
      reports,
      nullptr);

  float current_value = 0.0f;
  if (index >= 0 && index < values.size()) {
    current_value = values[index];
  }

  /* This happens if NLA rejects this insertion. */
  if (!successful_remaps[index]) {
    return false;
  }

  const float cfra = anim_eval_context->eval_time;
  const SingleKeyingResult result = insert_keyframe_value(fcu, cfra, current_value, keytype, flag);

  if (result != SingleKeyingResult::SUCCESS) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Failed to insert keys on F-Curve with path '%s[%d]', ensure that it is not "
                "locked or sampled, and try removing F-Modifiers",
                fcu->rna_path,
                fcu->array_index);
  }
  return result == SingleKeyingResult::SUCCESS;
}

/** Find or create the FCurve based on the given path, and insert the specified value into it. */
static SingleKeyingResult insert_keyframe_fcurve_value(Main *bmain,
                                                       PointerRNA *ptr,
                                                       PropertyRNA *prop,
                                                       bAction *act,
                                                       const char group[],
                                                       const char rna_path[],
                                                       int array_index,
                                                       const float fcurve_frame,
                                                       float curval,
                                                       eBezTriple_KeyframeType keytype,
                                                       eInsertKeyFlags flag)
{
  BLI_assert(rna_path != nullptr);

  /* Make sure the F-Curve exists.
   * - if we're replacing keyframes only, DO NOT create new F-Curves if they do not exist yet
   *   but still try to get the F-Curve if it exists...
   */

  FCurve *fcu = key_insertion_may_create_fcurve(flag) ?
                    action_fcurve_ensure(bmain, act, group, ptr, {rna_path, array_index}) :
                    fcurve_find_in_action(act, {rna_path, array_index});

  /* We may not have a F-Curve when we're replacing only. */
  if (!fcu) {
    return SingleKeyingResult::CANNOT_CREATE_FCURVE;
  }

  const bool is_new_curve = (fcu->totvert == 0);

  /* If the curve has only one key, make it cyclic if appropriate. */
  const bool is_cyclic_action = (flag & INSERTKEY_CYCLE_AWARE) && act->wrap().is_cyclic();

  if (is_cyclic_action && fcu->totvert == 1) {
    make_new_fcurve_cyclic(fcu, {act->frame_start, act->frame_end});
  }

  /* Update F-Curve flags to ensure proper behavior for property type. */
  update_autoflags_fcurve_direct(fcu, prop);

  const SingleKeyingResult result = insert_keyframe_value(
      fcu, fcurve_frame, curval, keytype, flag);

  /* If the curve is new, make it cyclic if appropriate. */
  if (is_cyclic_action && is_new_curve) {
    make_new_fcurve_cyclic(fcu, {act->frame_start, act->frame_end});
  }

  return result;
}

/* ************************************************** */
/* KEYFRAME DELETION */

/* Main Keyframing API call:
 * Use this when validation of necessary animation data isn't necessary as it
 * already exists. It will delete a keyframe at the current frame.
 *
 * The flag argument is used for special settings that alter the behavior of
 * the keyframe deletion. These include the quick refresh options.
 */

static void deg_tag_after_keyframe_delete(Main *bmain, ID *id, AnimData *adt)
{
  if (adt->action == nullptr) {
    /* In the case last f-curve was removed need to inform dependency graph
     * about relations update, since it needs to get rid of animation operation
     * for this data-block. */
    DEG_id_tag_update_ex(bmain, id, ID_RECALC_ANIMATION_NO_FLUSH);
    DEG_relations_tag_update(bmain);
  }
  else {
    DEG_id_tag_update_ex(bmain, &adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
  }
}

int delete_keyframe(Main *bmain, ReportList *reports, ID *id, const RNAPath &rna_path, float cfra)
{
  AnimData *adt = BKE_animdata_from_id(id);

  if (ELEM(nullptr, id, adt)) {
    BKE_report(reports, RPT_ERROR, "No ID block and/or AnimData to delete keyframe from");
    return 0;
  }

  PointerRNA ptr;
  PropertyRNA *prop;
  PointerRNA id_ptr = RNA_id_pointer_create(id);
  if (RNA_path_resolve_property(&id_ptr, rna_path.path.c_str(), &ptr, &prop) == false) {
    BKE_reportf(
        reports,
        RPT_ERROR,
        "Could not delete keyframe, as RNA path is invalid for the given ID (ID = %s, path = %s)",
        id->name,
        rna_path.path.c_str());
    return 0;
  }

  if (!adt->action) {
    BKE_reportf(reports, RPT_ERROR, "No action to delete keyframes from for ID = %s", id->name);
    return 0;
  }
  bAction *act = adt->action;
  cfra = BKE_nla_tweakedit_remap(adt, cfra, NLATIME_CONVERT_UNMAP);
  int array_index = rna_path.index.value_or(0);
  int array_index_max = array_index + 1;

  if (!rna_path.index.has_value()) {
    array_index_max = RNA_property_array_length(&ptr, prop);
    /* For single properties, increase max_index so that the property itself gets included,
     * but don't do this for standard arrays since that can cause corruption issues
     * (extra unused curves).
     */
    if (array_index_max == array_index) {
      array_index_max++;
    }
  }

  Action &action = act->wrap();
  Vector<FCurve *> modified_fcurves;
  if (action.is_action_layered()) {
    /* Just being defensive in the face of the NLA shenanigans above. This
     * probably isn't necessary, but it doesn't hurt. */
    BLI_assert(adt->action == act && action.slot_for_handle(adt->slot_handle) != nullptr);

    Span<FCurve *> fcurves = fcurves_for_action_slot(action, adt->slot_handle);
    /* This loop's clause is copied from the pre-existing code for legacy
     * actions below, to ensure behavioral consistency between the two code
     * paths. In the future when legacy actions are removed, we can restructure
     * it to be clearer. */
    for (; array_index < array_index_max; array_index++) {
      FCurve *fcurve = fcurve_find(fcurves, {rna_path.path, array_index});
      if (fcurve == nullptr) {
        continue;
      }
      if (fcurve_delete_keyframe_at_time(fcurve, cfra)) {
        modified_fcurves.append(fcurve);
      }
    }
  }
  else {
    /* Will only loop once unless the array index was -1. */
    for (; array_index < array_index_max; array_index++) {
      FCurve *fcu = fcurve_find_in_action(act, {rna_path.path, array_index});

      if (fcu == nullptr) {
        continue;
      }

      if (BKE_fcurve_is_protected(fcu)) {
        BKE_reportf(reports,
                    RPT_WARNING,
                    "Not deleting keyframe for locked F-Curve '%s' for %s '%s'",
                    fcu->rna_path,
                    BKE_idtype_idcode_to_name(GS(id->name)),
                    id->name + 2);
        continue;
      }

      if (fcurve_delete_keyframe_at_time(fcu, cfra)) {
        modified_fcurves.append(fcu);
      }
    }
  }

  if (!modified_fcurves.is_empty()) {
    for (FCurve *fcurve : modified_fcurves) {
      if (BKE_fcurve_is_empty(fcurve)) {
        animdata_fcurve_delete(nullptr, adt, fcurve);
      }
    }
    deg_tag_after_keyframe_delete(bmain, id, adt);
  }

  return modified_fcurves.size();
}

/* ************************************************** */
/* KEYFRAME CLEAR */

int clear_keyframe(Main *bmain, ReportList *reports, ID *id, const RNAPath &rna_path)
{
  AnimData *adt = BKE_animdata_from_id(id);

  if (ELEM(nullptr, id, adt)) {
    BKE_report(reports, RPT_ERROR, "No ID block and/or AnimData to delete keyframe from");
    return 0;
  }

  PointerRNA ptr;
  PropertyRNA *prop;
  PointerRNA id_ptr = RNA_id_pointer_create(id);
  if (RNA_path_resolve_property(&id_ptr, rna_path.path.c_str(), &ptr, &prop) == false) {
    BKE_reportf(
        reports,
        RPT_ERROR,
        "Could not clear keyframe, as RNA path is invalid for the given ID (ID = %s, path = %s)",
        id->name,
        rna_path.path.c_str());
    return 0;
  }

  if (!adt->action) {
    BKE_reportf(reports, RPT_ERROR, "No action to delete keyframes from for ID = %s", id->name);
    return 0;
  }
  bAction *act = adt->action;

  Action &action = act->wrap();
  int key_count = 0;

  if (action.is_action_layered()) {
    if (adt->slot_handle) {
      Vector<FCurve *> fcurves;
      foreach_fcurve_in_action_slot(action, adt->slot_handle, [&](FCurve &fcurve) {
        if (rna_path.index.has_value() && rna_path.index.value() != fcurve.array_index) {
          return;
        }
        if (rna_path.path != fcurve.rna_path) {
          return;
        }
        fcurves.append(&fcurve);
      });

      for (FCurve *fcu : fcurves) {
        if (action_fcurve_remove(action, *fcu)) {
          key_count++;
        }
      }
    }
  }
  else {
    int array_index = rna_path.index.value_or(0);
    int array_index_max = array_index + 1;
    if (!rna_path.index.has_value()) {
      array_index_max = RNA_property_array_length(&ptr, prop);

      /* For single properties, increase max_index so that the property itself gets included,
       * but don't do this for standard arrays since that can cause corruption issues
       * (extra unused curves).
       */
      if (array_index_max == array_index) {
        array_index_max++;
      }
    }
    /* Will only loop once unless the array index was -1. */
    for (; array_index < array_index_max; array_index++) {
      FCurve *fcu = fcurve_find_in_action(act, {rna_path.path, array_index});

      if (fcu == nullptr) {
        continue;
      }

      if (BKE_fcurve_is_protected(fcu)) {
        BKE_reportf(reports,
                    RPT_WARNING,
                    "Not clearing all keyframes from locked F-Curve '%s' for %s '%s'",
                    fcu->rna_path,
                    BKE_idtype_idcode_to_name(GS(id->name)),
                    id->name + 2);
        continue;
      }

      animdata_fcurve_delete(nullptr, adt, fcu);

      key_count++;
    }
  }

  if (key_count) {
    deg_tag_after_keyframe_delete(bmain, id, adt);
  }

  return key_count;
}

static CombinedKeyingResult insert_key_legacy_action(
    Main *bmain,
    bAction *action,
    PointerRNA *ptr,
    PropertyRNA *prop,
    const std::optional<StringRefNull> channel_group,
    const std::string &rna_path,
    const float frame,
    const Span<float> values,
    eInsertKeyFlags insert_key_flag,
    eBezTriple_KeyframeType key_type,
    const BitSpan keying_mask)
{
  BLI_assert(bmain != nullptr);
  BLI_assert(action != nullptr);
  BLI_assert(action->wrap().is_action_legacy());

  const char *group;
  if (channel_group.has_value()) {
    group = channel_group->c_str();
  }
  else {
    const std::optional<StringRefNull> default_group = default_channel_group_for_path(ptr,
                                                                                      rna_path);
    group = default_group.has_value() ? default_group->c_str() : nullptr;
  }

  int property_array_index = 0;
  CombinedKeyingResult combined_result;
  for (float value : values) {
    if (!keying_mask[property_array_index]) {
      combined_result.add(SingleKeyingResult::UNABLE_TO_INSERT_TO_NLA_STACK);
      property_array_index++;
      continue;
    }
    const SingleKeyingResult keying_result = insert_keyframe_fcurve_value(bmain,
                                                                          ptr,
                                                                          prop,
                                                                          action,
                                                                          group,
                                                                          rna_path.c_str(),
                                                                          property_array_index,
                                                                          frame,
                                                                          value,
                                                                          key_type,
                                                                          insert_key_flag);
    combined_result.add(keying_result);
    property_array_index++;
  }
  return combined_result;
}

struct KeyInsertData {
  float2 position;
  int array_index;
};

static SingleKeyingResult insert_key_layer(
    Main *bmain,
    Action &action,
    Layer &layer,
    const Slot &slot,
    const std::string &rna_path,
    const std::optional<PropertySubType> prop_subtype,
    const std::optional<blender::StringRefNull> channel_group,
    const KeyInsertData &key_data,
    const KeyframeSettings &key_settings,
    const eInsertKeyFlags insert_key_flags)
{
  assert_baklava_phase_1_invariants(layer);
  BLI_assert(layer.strips().size() == 1);

  const bool do_cyclic = (insert_key_flags & INSERTKEY_CYCLE_AWARE) && action.is_cyclic();

  Strip *strip = layer.strip(0);
  return strip->data<StripKeyframeData>(action).keyframe_insert(
      bmain,
      slot,
      {rna_path, key_data.array_index, prop_subtype, channel_group},
      key_data.position,
      key_settings,
      insert_key_flags,
      do_cyclic ? std::optional(action.get_frame_range()) : std::nullopt);
}

static std::pair<Layer *, Slot *> prep_action_layer_for_keying(Action &action, ID &animated_id)
{
  BLI_assert(action.is_action_layered());
  BLI_assert_msg(
      ELEM(get_action(animated_id), &action, nullptr),
      "The animated ID should not be using another Action than the one passed to this function");

  Slot *slot = assign_action_ensure_slot_for_keying(action, animated_id);
  BLI_assert_msg(
      slot,
      "The conditions that would cause this Slot assignment to fail (such as the ID not being "
      "animatible) should have been caught and handled by higher-level functions.");

  action.layer_keystrip_ensure();

  /* TODO: we currently assume this will always successfully find a layer.
   * However, that may not be true in the future when we implement features like
   * layer locking: if layers already exist, but they are all locked, then the
   * default layer won't be added by the line above, but there also won't be any
   * layers we can insert keys into. */
  Layer *layer = action.get_layer_for_keyframing();
  BLI_assert(layer != nullptr);

  return std::make_pair(layer, slot);
}

static CombinedKeyingResult insert_key_layered_action(
    Main *bmain,
    Action &action,
    Layer &layer,
    const Slot &slot,
    PropertyRNA *prop,
    const std::optional<StringRefNull> channel_group,
    const std::string &rna_path,
    const float frame,
    const Span<float> values,
    const eInsertKeyFlags insert_key_flags,
    const KeyframeSettings &key_settings,
    const BitSpan keying_mask)
{
  BLI_assert(bmain != nullptr);
  BLI_assert(action.is_action_layered());

  const PropertySubType prop_subtype = RNA_property_subtype(prop);

  int property_array_index = 0;
  CombinedKeyingResult combined_result;
  for (float value : values) {
    if (!keying_mask[property_array_index]) {
      combined_result.add(SingleKeyingResult::UNABLE_TO_INSERT_TO_NLA_STACK);
      property_array_index++;
      continue;
    }
    const KeyInsertData key_data = {{frame, value}, property_array_index};
    const SingleKeyingResult result = insert_key_layer(bmain,
                                                       action,
                                                       layer,
                                                       slot,
                                                       rna_path,
                                                       prop_subtype,
                                                       channel_group,
                                                       key_data,
                                                       key_settings,
                                                       insert_key_flags);

    combined_result.add(result);
    property_array_index++;
  }
  return combined_result;
}

CombinedKeyingResult insert_keyframes(Main *bmain,
                                      PointerRNA *struct_pointer,
                                      const std::optional<StringRefNull> channel_group,
                                      const blender::Span<RNAPath> rna_paths,
                                      const std::optional<float> scene_frame,
                                      const AnimationEvalContext &anim_eval_context,
                                      const eBezTriple_KeyframeType key_type,
                                      const eInsertKeyFlags insert_key_flags)

{
  ID *id = struct_pointer->owner_id;
  PointerRNA id_pointer = RNA_id_pointer_create(id);
  CombinedKeyingResult combined_result;

  /* Init animdata if none available yet. */
  AnimData *adt = BKE_animdata_ensure_id(id);
  if (adt == nullptr) {
    combined_result.add(SingleKeyingResult::ID_NOT_ANIMATABLE);
    return combined_result;
  }

  if ((adt->action == nullptr) && (insert_key_flags & INSERTKEY_AVAILABLE)) {
    combined_result.add(SingleKeyingResult::CANNOT_CREATE_FCURVE, rna_paths.size());
    return combined_result;
  }

  bAction *dna_action = id_action_ensure(bmain, id);
  BLI_assert(dna_action != nullptr);
  Action &action = dna_action->wrap();
  const bool is_action_legacy = animrig::legacy::action_treat_as_legacy(action);

  KeyframeSettings key_settings = get_keyframe_settings(
      (insert_key_flags & INSERTKEY_NO_USERPREF) == 0);
  key_settings.keyframe_type = key_type;

  /* NOTE: keyframing functions can deal with the nla_context being a nullptr. */
  ListBase nla_cache = {nullptr, nullptr};
  NlaKeyframingContext *nla_context = nullptr;
  const float nla_frame = nla_time_remap(scene_frame.value_or(anim_eval_context.eval_time),
                                         &anim_eval_context,
                                         &id_pointer,
                                         adt,
                                         dna_action,
                                         &nla_cache,
                                         &nla_context);
  const bool visual_keyframing = insert_key_flags & INSERTKEY_MATRIX;

  for (const RNAPath &rna_path : rna_paths) {
    PointerRNA ptr;
    PropertyRNA *prop = nullptr;
    const bool path_resolved = RNA_path_resolve_property(
        struct_pointer, rna_path.path.c_str(), &ptr, &prop);
    if (!path_resolved) {
      combined_result.add(SingleKeyingResult::CANNOT_RESOLVE_PATH);
      continue;
    }

    Vector<float> rna_values = get_keyframe_values(&ptr, prop, visual_keyframing);
    BitVector<> rna_values_mask(rna_values.size(), false);
    bool force_all;

    /* NOTE: this function call is complex with interesting/non-obvious effects.
     * Please see its documentation for details. */
    BKE_animsys_nla_remap_keyframe_values(nla_context,
                                          struct_pointer,
                                          prop,
                                          rna_values.as_mutable_span(),
                                          rna_path.index.value_or(-1),
                                          &anim_eval_context,
                                          &force_all,
                                          rna_values_mask);

    const std::optional<std::string> rna_path_id_to_prop = RNA_path_from_ID_to_property(&ptr,
                                                                                        prop);
    if (!rna_path_id_to_prop.has_value()) {
      continue;
    }

    /* Handle the `force_all` condition mentioned above, ensuring the
     * "all-or-nothing" behavior if needed.
     *
     * TODO: this currently doesn't account for the "Only Insert Available"
     * flag, which also needs to be accounted for to actually ensure
     * all-or-nothing behavior. This is because the function this part of the
     * code originally came from (see #122053) also didn't account for it.
     * Presumably that was an oversight, and should be addressed. But for now
     * we're faithfully reproducing the original behavior.
     */
    eInsertKeyFlags insert_key_flags_adjusted = insert_key_flags;
    if (force_all && (insert_key_flags & (INSERTKEY_REPLACE | INSERTKEY_AVAILABLE))) {
      /* Determine if at least one element would succeed getting keyed. */
      bool at_least_one_would_succeed = false;
      for (int i = 0; i < rna_values.size(); i++) {
        const FCurve *fcu = fcurve_find_in_action(dna_action, {*rna_path_id_to_prop, i});
        if (!fcu) {
          continue;
        }

        /* We found an fcurve, and "Only Replace" is not on, so a key insertion
         * would succeed according to the two flags we're accounting for. */
        if (!(insert_key_flags & INSERTKEY_REPLACE)) {
          at_least_one_would_succeed = true;
          break;
        }

        /* "Only Replace" *is* on, so a key insertion would succeed only if we
         * actually replace an existing keyframe. */
        bool replace;
        BKE_fcurve_bezt_binarysearch_index(fcu->bezt, nla_frame, fcu->totvert, &replace);
        if (replace) {
          at_least_one_would_succeed = true;
          break;
        }
      }

      /* If at least one would succeed, then we disable all keying flags that
       * would prevent the other elements from getting keyed as well. */
      if (at_least_one_would_succeed) {
        insert_key_flags_adjusted &= ~(INSERTKEY_REPLACE | INSERTKEY_AVAILABLE);
      }
    }

    CombinedKeyingResult result;
    if (is_action_legacy) {
      result = insert_key_legacy_action(bmain,
                                        dna_action,
                                        struct_pointer,
                                        prop,
                                        channel_group,
                                        rna_path_id_to_prop->c_str(),
                                        nla_frame,
                                        rna_values.as_span(),
                                        insert_key_flags_adjusted,
                                        key_type,
                                        rna_values_mask);
    }
    else {
      /* When getting rid of legacy code & WITH_ANIM_BAKLAVA, this line can be
       * moved out of the for-loop. */
      auto [layer, slot] = prep_action_layer_for_keying(action, *struct_pointer->owner_id);

      const std::optional<blender::StringRefNull> this_rna_path_channel_group =
          channel_group.has_value() ? *channel_group :
                                      default_channel_group_for_path(&ptr, *rna_path_id_to_prop);

      result = insert_key_layered_action(bmain,
                                         action,
                                         *layer,
                                         *slot,
                                         prop,
                                         this_rna_path_channel_group,
                                         *rna_path_id_to_prop,
                                         nla_frame,
                                         rna_values,
                                         insert_key_flags,
                                         key_settings,
                                         rna_values_mask);
    }

    combined_result.merge(result);
  }

  BKE_animsys_free_nla_keyframing_context_cache(&nla_cache);

  if (combined_result.get_count(SingleKeyingResult::SUCCESS) > 0) {
    DEG_id_tag_update(&dna_action->id, ID_RECALC_ANIMATION_NO_FLUSH);

    /* TODO: it's not entirely clear why the action we got wouldn't be the same
     * as the action in AnimData. Further, it's not clear why it would need to
     * be tagged for a depsgraph update regardless. This code is here because it
     * was part of the function this one was refactored from, but at some point
     * this should be investigated and either documented or removed. */
    if (!ELEM(adt->action, nullptr, dna_action)) {
      DEG_id_tag_update(&adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
    }
  }

  return combined_result;
}

}  // namespace blender::animrig
