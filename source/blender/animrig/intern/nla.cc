/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "BLI_bit_vector.hh"
#include "BLI_dynstr.hh"

#include "BKE_animsys.h"
#include "BKE_fcurve.hh"

#include "RNA_access.hh"
#include "RNA_types.hh"

#include "BLT_translation.hh"

#include "ANIM_keyframing.hh"
#include "ANIM_nla.hh"

namespace blender::animrig::nla {

bool assign_action(NlaStrip &strip, Action &action, ID &animated_id)
{
  if (!generic_assign_action(
          animated_id, &action, strip.act, strip.action_slot_handle, strip.last_slot_identifier))
  {
    return false;
  }

  /* For the NLA, the auto slot selection gets one more fallback option (compared to the generic
   * code). This is to support the following scenario:
   *
   * - Python script creates an Action, and adds some F-Curves via the legacy API.
   * - This creates a slot 'XXSlot'.
   * - The script creates multiple NLA strips for that Action.
   * - The desired result is that these strips get the same Slot assigned as well.
   *
   * The generic code doesn't work for this. The first strip assignment would see the slot
   * `XXSlot`, and because it has never been used, just use it. This would change its name to, for
   * example, `OBSlot`. The second strip assignment would not see a 'virgin' slot, and thus not
   * auto-select `OBSlot`. This behavior makes sense when assigning Actions in the Action editor
   * (it shouldn't automatically pick the first slot of matching ID type), but for the NLA I
   * (Sybren) feel that it could be a bit more 'enthusiastic' in auto-picking a slot.
   */
  if (strip.action_slot_handle == Slot::unassigned && action.slots().size() == 1) {
    Slot *first_slot = action.slot(0);
    if (first_slot->is_suitable_for(animated_id)) {
      const ActionSlotAssignmentResult result = assign_action_slot(strip, first_slot, animated_id);
      BLI_assert_msg(result == ActionSlotAssignmentResult::OK,
                     "Assigning a slot that we know is suitable should work");
      UNUSED_VARS_NDEBUG(result);
    }
  }

  /* Regardless of slot auto-selection, the Action assignment worked just fine. */
  return true;
}

void unassign_action(NlaStrip &strip, ID &animated_id)
{
  const bool ok = generic_assign_action(
      animated_id, nullptr, strip.act, strip.action_slot_handle, strip.last_slot_identifier);
  BLI_assert_msg(ok, "Un-assigning an Action from an NLA strip should always work.");
  UNUSED_VARS_NDEBUG(ok);
}

ActionSlotAssignmentResult assign_action_slot(NlaStrip &strip,
                                              Slot *slot_to_assign,
                                              ID &animated_id)
{
  BLI_assert(strip.act);

  return generic_assign_action_slot(slot_to_assign,
                                    animated_id,
                                    strip.act,
                                    strip.action_slot_handle,
                                    strip.last_slot_identifier);
}

ActionSlotAssignmentResult assign_action_slot_handle(NlaStrip &strip,
                                                     const slot_handle_t slot_handle,
                                                     ID &animated_id)
{
  BLI_assert(strip.act);

  Action &action = strip.act->wrap();
  Slot *slot_to_assign = action.slot_for_handle(slot_handle);

  return assign_action_slot(strip, slot_to_assign, animated_id);
}

/* Check indices that were intended to be remapped and report any failed remaps. */
static void get_keyframe_values_create_reports(ReportList *reports,
                                               const PointerRNA &ptr,
                                               const PropertyRNA *prop,
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

  MEM_delete(str_failed_indices);
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

  if (!BKE_fcurve_is_keyframable(*fcu)) {
    BKE_report(reports, RPT_ERROR, "FCurve is not keyable. Cannot insert keyframes");
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
  update_autoflags_fcurve_direct(fcu, RNA_property_type(prop));

  const int index = fcu->array_index;
  const bool visual_keyframing = flag & INSERTKEY_MATRIX;
  Vector<float> values = get_property_values(&ptr, prop, visual_keyframing);

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

  KeyframeSettings settings = get_keyframe_settings((flag & INSERTKEY_NO_USERPREF) == 0);
  settings.keyframe_type = keytype;

  const SingleKeyingResult result = insert_vert_fcurve(
      fcu, {anim_eval_context->eval_time, current_value}, settings, flag);

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

}  // namespace blender::animrig::nla
