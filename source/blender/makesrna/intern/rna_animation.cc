/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "DNA_anim_types.h"

#include "BLT_translation.hh"

#include "BKE_lib_override.hh"
#include "BKE_nla.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_keyframing.hh"

using namespace blender;

/* exported for use in API */
const EnumPropertyItem rna_enum_keyingset_path_grouping_items[] = {
    {KSP_GROUP_NAMED, "NAMED", 0, "Named Group", ""},
    {KSP_GROUP_NONE, "NONE", 0, "None", ""},
    {KSP_GROUP_KSNAME, "KEYINGSET", 0, "Keying Set Name", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

/* It would be cool to get rid of this 'INSERTKEY_' prefix in 'py strings' values,
 * but it would break existing
 * exported keyingset... :/
 */
const EnumPropertyItem rna_enum_keying_flag_items[] = {
    {INSERTKEY_NEEDED,
     "INSERTKEY_NEEDED",
     0,
     "Only Needed",
     "Only insert keyframes where they're needed in the relevant F-Curves"},
    {INSERTKEY_MATRIX,
     "INSERTKEY_VISUAL",
     0,
     "Visual Keying",
     "Insert keyframes based on 'visual transforms'"},
    {0, nullptr, 0, nullptr, nullptr},
};

/* Contains additional flags suitable for use in Python API functions. */
const EnumPropertyItem rna_enum_keying_flag_api_items[] = {
    {INSERTKEY_NEEDED,
     "INSERTKEY_NEEDED",
     0,
     "Only Needed",
     "Only insert keyframes where they're needed in the relevant F-Curves"},
    {INSERTKEY_MATRIX,
     "INSERTKEY_VISUAL",
     0,
     "Visual Keying",
     "Insert keyframes based on 'visual transforms'"},
    {INSERTKEY_REPLACE,
     "INSERTKEY_REPLACE",
     0,
     "Replace Existing",
     "Only replace existing keyframes"},
    {INSERTKEY_AVAILABLE,
     "INSERTKEY_AVAILABLE",
     0,
     "Only Available",
     "Don't create F-Curves when they don't already exist"},
    {INSERTKEY_CYCLE_AWARE,
     "INSERTKEY_CYCLE_AWARE",
     0,
     "Cycle Aware Keying",
     "When inserting into a curve with cyclic extrapolation, remap the keyframe inside "
     "the cycle time range, and if changing an end key, also update the other one"},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include <algorithm>

#  include "BLI_math_base.h"

#  include "BKE_anim_data.hh"
#  include "BKE_animsys.h"
#  include "BKE_fcurve.hh"
#  include "BKE_nla.hh"

#  include "ANIM_action.hh"
#  include "ANIM_action_legacy.hh"
#  include "ANIM_keyingsets.hh"

#  include "DEG_depsgraph.hh"
#  include "DEG_depsgraph_build.hh"

#  include "DNA_object_types.h"

#  include "ED_anim_api.hh"

#  include "WM_api.hh"

#  include "UI_interface_icons.hh"

static void rna_AnimData_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  ID *id = ptr->owner_id;

  ANIM_id_update(bmain, id);
}

static void rna_AnimData_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  DEG_relations_tag_update(bmain);

  rna_AnimData_update(bmain, scene, ptr);
}

void rna_generic_action_slot_handle_override_diff(Main *bmain,
                                                  RNAPropertyOverrideDiffContext &rnadiff_ctx,
                                                  const bAction *action_a,
                                                  const bAction *action_b)
{
  rna_property_override_diff_default(bmain, rnadiff_ctx);

  if (rnadiff_ctx.comparison || (rnadiff_ctx.report_flag & RNA_OVERRIDE_MATCH_RESULT_CREATED)) {
    /* Default diffing found a difference, no need to go further. */
    return;
  }

  if (action_a == action_b) {
    /* Action is unchanged, it's fine to mark the slot handle as unchanged as well. */
    return;
  }

  /* Sign doesn't make sense here, as the numerical values are the same. */
  rnadiff_ctx.comparison = 1;

  /* The remainder of this function was taken from rna_property_override_diff_default(). It's just
   * formatted a little differently to allow for early returns. */

  const bool do_create = rnadiff_ctx.liboverride != nullptr &&
                         (rnadiff_ctx.liboverride_flags & RNA_OVERRIDE_COMPARE_CREATE) != 0 &&
                         rnadiff_ctx.rna_path != nullptr;
  if (!do_create) {
    /* Not enough info to create an override operation, so bail out. */
    return;
  }

  /* Create the override operation. */
  bool created = false;
  IDOverrideLibraryProperty *op = BKE_lib_override_library_property_get(
      rnadiff_ctx.liboverride, rnadiff_ctx.rna_path, &created);

  if (op && created) {
    BKE_lib_override_library_property_operation_get(
        op, LIBOVERRIDE_OP_REPLACE, nullptr, nullptr, {}, {}, -1, -1, true, nullptr, nullptr);
    rnadiff_ctx.report_flag |= RNA_OVERRIDE_MATCH_RESULT_CREATED;
  }
}

/**
 * Emit a 'diff' for the .slot_handle property whenever the .action property differs.
 *
 * \see rna_generic_action_slot_handle_override_diff()
 */
static void rna_AnimData_slot_handle_override_diff(Main *bmain,
                                                   RNAPropertyOverrideDiffContext &rnadiff_ctx)
{
  const AnimData *adt_a = static_cast<AnimData *>(rnadiff_ctx.prop_a->ptr->data);
  const AnimData *adt_b = static_cast<AnimData *>(rnadiff_ctx.prop_b->ptr->data);

  rna_generic_action_slot_handle_override_diff(bmain, rnadiff_ctx, adt_a->action, adt_b->action);
}

static int rna_AnimData_action_editable(const PointerRNA *ptr, const char ** /*r_info*/)
{
  BLI_assert(ptr->type == &RNA_AnimData);
  AnimData *adt = static_cast<AnimData *>(ptr->data);
  if (!adt) {
    return PROP_EDITABLE;
  }
  return BKE_animdata_action_editable(adt) ? PROP_EDITABLE : PropertyFlag(0);
}

static PointerRNA rna_AnimData_action_get(PointerRNA *ptr)
{
  ID &animated_id = *ptr->owner_id;
  animrig::Action *action = animrig::get_action(animated_id);
  if (!action) {
    return PointerRNA_NULL;
  };
  return RNA_id_pointer_create(&action->id);
}

static void rna_AnimData_action_set(PointerRNA *ptr, PointerRNA value, ReportList *reports)
{
  using namespace blender::animrig;
  BLI_assert(ptr->owner_id);
  ID &animated_id = *ptr->owner_id;

  Action *action = static_cast<Action *>(value.data);
  if (!assign_action(action, animated_id)) {
    BKE_report(reports, RPT_ERROR, "Could not change action");
  }
}

static void rna_AnimData_tmpact_set(PointerRNA *ptr, PointerRNA value, ReportList *reports)
{
  ID *owner_id = ptr->owner_id;
  AnimData *adt = (AnimData *)ptr->data;
  BLI_assert(adt != nullptr);

  bAction *action = static_cast<bAction *>(value.data);
  if (!blender::animrig::assign_tmpaction(action, {*owner_id, *adt})) {
    BKE_report(reports, RPT_WARNING, "Failed to set temporary action");
  }
}

static void rna_AnimData_tweakmode_set(PointerRNA *ptr, const bool value)
{
  ID *animated_id = ptr->owner_id;
  AnimData *adt = (AnimData *)ptr->data;

  /* NOTE: technically we should also set/unset SCE_NLA_EDIT_ON flag on the
   * scene which is used to make polling tests faster, but this flag is weak
   * and can easily break e.g. by changing layer visibility. This needs to be
   * dealt with at some point. */

  if (value) {
    BKE_nla_tweakmode_enter({*animated_id, *adt});
  }
  else {
    BKE_nla_tweakmode_exit({*animated_id, *adt});
  }
}

/**
 * This is used to avoid the check for NLA tracks when enabling tweak
 * mode while loading overrides. This is necessary because the normal
 * RNA tweak-mode setter refuses to enable tweak mode if there are no
 * NLA tracks since that's normally an invalid state... but the
 * overridden NLA tracks are only added *after* setting the tweak mode
 * override.
 */
bool rna_AnimData_tweakmode_override_apply(Main * /*bmain*/,
                                           RNAPropertyOverrideApplyContext &rnaapply_ctx)
{
  PointerRNA *ptr_dst = &rnaapply_ctx.ptr_dst;
  PointerRNA *ptr_src = &rnaapply_ctx.ptr_src;

  AnimData *anim_data_dst = (AnimData *)ptr_dst->data;
  AnimData *anim_data_src = (AnimData *)ptr_src->data;

  anim_data_dst->flag = (anim_data_dst->flag & ~ADT_NLA_EDIT_ON) |
                        (anim_data_src->flag & ADT_NLA_EDIT_ON);

  /* There are many more flags & pointers to deal with when switching NLA tweak mode. This has to
   * be handled once all the NLA tracks & strips are available, though. It's done in a post-process
   * step, see BKE_nla_liboverride_post_process(). */
  return true;
}

void rna_generic_action_slot_handle_set(blender::animrig::slot_handle_t slot_handle_to_assign,
                                        ID &animated_id,
                                        bAction *&action_ptr_ref,
                                        blender::animrig::slot_handle_t &slot_handle_ref,
                                        char *slot_name)
{
  using namespace blender::animrig;

  const ActionSlotAssignmentResult result = generic_assign_action_slot_handle(
      slot_handle_to_assign, animated_id, action_ptr_ref, slot_handle_ref, slot_name);

  /* Unfortunately setters for PROP_INT do not receive a `reports` parameter, so
   * report to the Window Manager report list instead. */
  switch (result) {
    case ActionSlotAssignmentResult::OK:
      break;
    case ActionSlotAssignmentResult::SlotNotFromAction:
      BLI_assert_unreachable();
      break;
    case ActionSlotAssignmentResult::SlotNotSuitable:
      WM_global_reportf(RPT_ERROR,
                        "This slot is not suitable for this data-block type (%c%c)",
                        animated_id.name[0],
                        animated_id.name[1]);
      break;
    case ActionSlotAssignmentResult::MissingAction:
      WM_global_report(RPT_ERROR, "Cannot set slot without an assigned Action.");
      break;
  }
}

static void rna_AnimData_action_slot_handle_set(
    PointerRNA *ptr, const blender::animrig::slot_handle_t new_slot_handle)
{
  ID &animated_id = *ptr->owner_id;
  AnimData *adt = BKE_animdata_from_id(&animated_id);

  rna_generic_action_slot_handle_set(
      new_slot_handle, animated_id, adt->action, adt->slot_handle, adt->last_slot_identifier);
}

static AnimData &rna_animdata(const PointerRNA *ptr)
{
  return *reinterpret_cast<AnimData *>(ptr->data);
}

PointerRNA rna_generic_action_slot_get(bAction *dna_action,
                                       const animrig::slot_handle_t slot_handle)
{
  using namespace blender::animrig;

  if (!dna_action || slot_handle == Slot::unassigned) {
    return PointerRNA_NULL;
  }

  Action &action = dna_action->wrap();
  Slot *slot = action.slot_for_handle(slot_handle);
  if (!slot) {
    return PointerRNA_NULL;
  }
  return RNA_pointer_create_discrete(&action.id, &RNA_ActionSlot, slot);
}

static PointerRNA rna_AnimData_action_slot_get(PointerRNA *ptr)
{
  AnimData &adt = rna_animdata(ptr);
  return rna_generic_action_slot_get(adt.action, adt.slot_handle);
}

void rna_generic_action_slot_set(PointerRNA rna_slot_to_assign,
                                 ID &animated_id,
                                 bAction *&action_ptr_ref,
                                 blender::animrig::slot_handle_t &slot_handle_ref,
                                 char *slot_name,
                                 ReportList *reports)
{
  using namespace blender::animrig;

  ActionSlot *dna_slot = static_cast<ActionSlot *>(rna_slot_to_assign.data);
  Slot *slot = dna_slot ? &dna_slot->wrap() : nullptr;

  const ActionSlotAssignmentResult result = generic_assign_action_slot(
      slot, animated_id, action_ptr_ref, slot_handle_ref, slot_name);

  switch (result) {
    case ActionSlotAssignmentResult::OK:
      break;
    case ActionSlotAssignmentResult::SlotNotFromAction:
      BKE_reportf(reports,
                  RPT_ERROR,
                  "This slot (%s) does not belong to the assigned Action",
                  slot->identifier);
      break;
    case ActionSlotAssignmentResult::SlotNotSuitable:
      BKE_reportf(reports,
                  RPT_ERROR,
                  "This slot (%s) is not suitable for this data-block type (%c%c)",
                  slot->identifier,
                  animated_id.name[0],
                  animated_id.name[1]);
      break;
    case ActionSlotAssignmentResult::MissingAction:
      BKE_report(reports, RPT_ERROR, "Cannot set slot without an assigned Action.");
      break;
  }
}

static void rna_AnimData_action_slot_set(PointerRNA *ptr, PointerRNA value, ReportList *reports)
{
  ID *animated_id = ptr->owner_id;
  AnimData *adt = BKE_animdata_from_id(animated_id);
  if (!adt) {
    BKE_report(reports, RPT_ERROR, "Cannot set slot without an assigned Action.");
    return;
  }

  rna_generic_action_slot_set(
      value, *animated_id, adt->action, adt->slot_handle, adt->last_slot_identifier, reports);
}

static void rna_AnimData_action_slot_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  /* TODO: see if this is still necessary. */
  blender::animrig::Slot::users_invalidate(*bmain);
  rna_AnimData_dependency_update(bmain, scene, ptr);
}

/**
 * Skip any slot that is not suitable for the ID owning the 'slots' pointer.
 *
 * This function belongs to the 'generic action slot' family of functions. It's not (yet) necessary
 * to refer to this from any other file, though, which is why it's not declared in
 * rna_action_tools.hh.
 */
bool rna_iterator_generic_action_suitable_slots_skip(CollectionPropertyIterator *iter, void *data)
{
  using animrig::Slot;

  /* Get the current Slot being iterated over. */
  const Slot **slot_ptr_ptr = static_cast<const Slot **>(data);
  BLI_assert(slot_ptr_ptr);
  BLI_assert(*slot_ptr_ptr);
  const Slot &slot = **slot_ptr_ptr;

  /* Get the animated ID. */
  const ID *animated_id = iter->parent.owner_id;
  BLI_assert(animated_id);

  /* Skip this Slot if it's not suitable for the animated ID. */
  return !slot.is_suitable_for(*animated_id);
}

void rna_iterator_generic_action_suitable_slots_begin(CollectionPropertyIterator *iter,
                                                      PointerRNA *owner_ptr,
                                                      bAction *assigned_action)
{
  if (!assigned_action) {
    /* No action means no slots. */
    rna_iterator_array_begin(iter, owner_ptr, nullptr, 0, 0, 0, nullptr);
    return;
  }

  animrig::Action &action = assigned_action->wrap();
  Span<animrig::Slot *> slots = action.slots();
  rna_iterator_array_begin(iter,
                           owner_ptr,
                           (void *)slots.data(),
                           sizeof(animrig::Slot *),
                           slots.size(),
                           0,
                           rna_iterator_generic_action_suitable_slots_skip);
}

static void rna_iterator_animdata_action_suitable_slots_begin(CollectionPropertyIterator *iter,
                                                              PointerRNA *ptr)
{
  rna_iterator_generic_action_suitable_slots_begin(iter, ptr, rna_animdata(ptr).action);
}

/* ****************************** */

/* wrapper for poll callback */
static bool RKS_POLL_rna_internal(KeyingSetInfo *ksi, bContext *C)
{
  extern FunctionRNA rna_KeyingSetInfo_poll_func;

  ParameterList list;
  FunctionRNA *func;
  void *ret;
  int ok;

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, ksi->rna_ext.srna, ksi);
  func = &rna_KeyingSetInfo_poll_func; /* RNA_struct_find_function(&ptr, "poll"); */

  RNA_parameter_list_create(&list, &ptr, func);
  {
    /* hook up arguments */
    RNA_parameter_set_lookup(&list, "ksi", &ksi);
    RNA_parameter_set_lookup(&list, "context", &C);

    /* execute the function */
    ksi->rna_ext.call(C, &ptr, func, &list);

    /* read the result */
    RNA_parameter_get_lookup(&list, "ok", &ret);
    ok = *(bool *)ret;
  }
  RNA_parameter_list_free(&list);

  return ok;
}

/* wrapper for iterator callback */
static void RKS_ITER_rna_internal(KeyingSetInfo *ksi, bContext *C, KeyingSet *ks)
{
  extern FunctionRNA rna_KeyingSetInfo_iterator_func;

  ParameterList list;
  FunctionRNA *func;

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, ksi->rna_ext.srna, ksi);
  func = &rna_KeyingSetInfo_iterator_func; /* RNA_struct_find_function(&ptr, "poll"); */

  RNA_parameter_list_create(&list, &ptr, func);
  {
    /* hook up arguments */
    RNA_parameter_set_lookup(&list, "ksi", &ksi);
    RNA_parameter_set_lookup(&list, "context", &C);
    RNA_parameter_set_lookup(&list, "ks", &ks);

    /* execute the function */
    ksi->rna_ext.call(C, &ptr, func, &list);
  }
  RNA_parameter_list_free(&list);
}

/* wrapper for generator callback */
static void RKS_GEN_rna_internal(KeyingSetInfo *ksi, bContext *C, KeyingSet *ks, PointerRNA *data)
{
  extern FunctionRNA rna_KeyingSetInfo_generate_func;

  ParameterList list;
  FunctionRNA *func;

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, ksi->rna_ext.srna, ksi);
  func = &rna_KeyingSetInfo_generate_func; /* RNA_struct_find_generate(&ptr, "poll"); */

  RNA_parameter_list_create(&list, &ptr, func);
  {
    /* hook up arguments */
    RNA_parameter_set_lookup(&list, "ksi", &ksi);
    RNA_parameter_set_lookup(&list, "context", &C);
    RNA_parameter_set_lookup(&list, "ks", &ks);
    RNA_parameter_set_lookup(&list, "data", data);

    /* execute the function */
    ksi->rna_ext.call(C, &ptr, func, &list);
  }
  RNA_parameter_list_free(&list);
}

/* ------ */

/* XXX: the exact purpose of this is not too clear...
 * maybe we want to revise this at some point? */
static StructRNA *rna_KeyingSetInfo_refine(PointerRNA *ptr)
{
  KeyingSetInfo *ksi = (KeyingSetInfo *)ptr->data;
  return (ksi->rna_ext.srna) ? ksi->rna_ext.srna : &RNA_KeyingSetInfo;
}

static bool rna_KeyingSetInfo_unregister(Main *bmain, StructRNA *type)
{
  KeyingSetInfo *ksi = static_cast<KeyingSetInfo *>(RNA_struct_blender_type_get(type));

  if (ksi == nullptr) {
    return false;
  }

  /* free RNA data referencing this */
  RNA_struct_free_extension(type, &ksi->rna_ext);
  RNA_struct_free(&BLENDER_RNA, type);

  WM_main_add_notifier(NC_WINDOW, nullptr);

  /* unlink Blender-side data */
  blender::animrig::keyingset_info_unregister(bmain, ksi);
  return true;
}

static StructRNA *rna_KeyingSetInfo_register(Main *bmain,
                                             ReportList *reports,
                                             void *data,
                                             const char *identifier,
                                             StructValidateFunc validate,
                                             StructCallbackFunc call,
                                             StructFreeFunc free)
{
  const char *error_prefix = "Registering keying set info class:";
  KeyingSetInfo dummy_ksi = {nullptr};
  KeyingSetInfo *ksi;
  bool have_function[3];

  /* setup dummy type info to store static properties in */
  /* TODO: perhaps we want to get users to register
   * as if they're using 'KeyingSet' directly instead? */
  PointerRNA dummy_ksi_ptr = RNA_pointer_create_discrete(nullptr, &RNA_KeyingSetInfo, &dummy_ksi);

  /* validate the python class */
  if (validate(&dummy_ksi_ptr, data, have_function) != 0) {
    return nullptr;
  }

  if (strlen(identifier) >= sizeof(dummy_ksi.idname)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s '%s' is too long, maximum length is %d",
                error_prefix,
                identifier,
                int(sizeof(dummy_ksi.idname)));
    return nullptr;
  }

  /* check if we have registered this info before, and remove it */
  ksi = blender::animrig::keyingset_info_find_name(dummy_ksi.idname);
  if (ksi) {
    BKE_reportf(reports,
                RPT_INFO,
                "%s '%s', bl_idname '%s' has been registered before, unregistering previous",
                error_prefix,
                identifier,
                dummy_ksi.idname);

    StructRNA *srna = ksi->rna_ext.srna;
    if (!(srna && rna_KeyingSetInfo_unregister(bmain, srna))) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "%s '%s', bl_idname '%s' %s",
                  error_prefix,
                  identifier,
                  dummy_ksi.idname,
                  srna ? "is built-in" : "could not be unregistered");
      return nullptr;
    }
  }

  /* create a new KeyingSetInfo type */
  ksi = MEM_mallocN<KeyingSetInfo>("python keying set info");
  memcpy(ksi, &dummy_ksi, sizeof(KeyingSetInfo));

  /* set RNA-extensions info */
  ksi->rna_ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, ksi->idname, &RNA_KeyingSetInfo);
  ksi->rna_ext.data = data;
  ksi->rna_ext.call = call;
  ksi->rna_ext.free = free;
  RNA_struct_blender_type_set(ksi->rna_ext.srna, ksi);

  /* set callbacks */
  /* NOTE: we really should have all of these... */
  ksi->poll = (have_function[0]) ? RKS_POLL_rna_internal : nullptr;
  ksi->iter = (have_function[1]) ? RKS_ITER_rna_internal : nullptr;
  ksi->generate = (have_function[2]) ? RKS_GEN_rna_internal : nullptr;

  /* add and register with other info as needed */
  blender::animrig::keyingset_info_register(ksi);

  WM_main_add_notifier(NC_WINDOW, nullptr);

  /* return the struct-rna added */
  return ksi->rna_ext.srna;
}

/* ****************************** */

static StructRNA *rna_ksPath_id_typef(PointerRNA *ptr)
{
  KS_Path *ksp = (KS_Path *)ptr->data;
  return ID_code_to_RNA_type(ksp->idtype);
}

static int rna_ksPath_id_editable(const PointerRNA *ptr, const char ** /*r_info*/)
{
  KS_Path *ksp = (KS_Path *)ptr->data;
  return (ksp->idtype) ? PROP_EDITABLE : PropertyFlag(0);
}

static void rna_ksPath_id_type_set(PointerRNA *ptr, int value)
{
  KS_Path *data = (KS_Path *)(ptr->data);

  /* set the driver type, then clear the id-block if the type is invalid */
  data->idtype = value;
  if ((data->id) && (GS(data->id->name) != data->idtype)) {
    data->id = nullptr;
  }
}

static void rna_ksPath_RnaPath_get(PointerRNA *ptr, char *value)
{
  KS_Path *ksp = (KS_Path *)ptr->data;

  if (ksp->rna_path) {
    strcpy(value, ksp->rna_path);
  }
  else {
    value[0] = '\0';
  }
}

static int rna_ksPath_RnaPath_length(PointerRNA *ptr)
{
  KS_Path *ksp = (KS_Path *)ptr->data;

  if (ksp->rna_path) {
    return strlen(ksp->rna_path);
  }
  else {
    return 0;
  }
}

static void rna_ksPath_RnaPath_set(PointerRNA *ptr, const char *value)
{
  KS_Path *ksp = (KS_Path *)ptr->data;

  if (ksp->rna_path) {
    MEM_freeN(ksp->rna_path);
  }

  if (value[0]) {
    ksp->rna_path = BLI_strdup(value);
  }
  else {
    ksp->rna_path = nullptr;
  }
}

/* ****************************** */

static void rna_KeyingSet_name_set(PointerRNA *ptr, const char *value)
{
  KeyingSet *ks = (KeyingSet *)ptr->data;

  /* update names of corresponding groups if name changes */
  if (!STREQ(ks->name, value)) {
    KS_Path *ksp;

    for (ksp = static_cast<KS_Path *>(ks->paths.first); ksp; ksp = ksp->next) {
      if ((ksp->groupmode == KSP_GROUP_KSNAME) && (ksp->id)) {
        AnimData *adt = BKE_animdata_from_id(ksp->id);

        /* TODO: NLA strips? */
        /* lazy check - should really find the F-Curve for the affected path and check its
         * group but this way should be faster and work well for most cases, as long as there
         * are no conflicts
         */
        for (bActionGroup *agrp : animrig::legacy::channel_groups_for_assigned_slot(adt)) {
          if (STREQ(ks->name, agrp->name)) {
            /* there should only be one of these in the action, so can stop... */
            STRNCPY_UTF8(agrp->name, value);
            break;
          }
        }
      }
    }
  }

  /* finally, update name to new value */
  STRNCPY(ks->name, value);
}

static int rna_KeyingSet_active_ksPath_editable(const PointerRNA *ptr, const char ** /*r_info*/)
{
  KeyingSet *ks = (KeyingSet *)ptr->data;

  /* only editable if there are some paths to change to */
  return (BLI_listbase_is_empty(&ks->paths) == false) ? PROP_EDITABLE : PropertyFlag(0);
}

static PointerRNA rna_KeyingSet_active_ksPath_get(PointerRNA *ptr)
{
  KeyingSet *ks = (KeyingSet *)ptr->data;
  return RNA_pointer_create_with_parent(
      *ptr, &RNA_KeyingSetPath, BLI_findlink(&ks->paths, ks->active_path - 1));
}

static void rna_KeyingSet_active_ksPath_set(PointerRNA *ptr,
                                            PointerRNA value,
                                            ReportList * /*reports*/)
{
  KeyingSet *ks = (KeyingSet *)ptr->data;
  KS_Path *ksp = (KS_Path *)value.data;
  ks->active_path = BLI_findindex(&ks->paths, ksp) + 1;
}

static int rna_KeyingSet_active_ksPath_index_get(PointerRNA *ptr)
{
  KeyingSet *ks = (KeyingSet *)ptr->data;
  return std::max(ks->active_path - 1, 0);
}

static void rna_KeyingSet_active_ksPath_index_set(PointerRNA *ptr, int value)
{
  KeyingSet *ks = (KeyingSet *)ptr->data;
  ks->active_path = value + 1;
}

static void rna_KeyingSet_active_ksPath_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  KeyingSet *ks = (KeyingSet *)ptr->data;

  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&ks->paths) - 1);
}

static PointerRNA rna_KeyingSet_typeinfo_get(PointerRNA *ptr)
{
  KeyingSet *ks = (KeyingSet *)ptr->data;
  KeyingSetInfo *ksi = nullptr;

  /* keying set info is only for builtin Keying Sets */
  if ((ks->flag & KEYINGSET_ABSOLUTE) == 0) {
    ksi = blender::animrig::keyingset_info_find_name(ks->typeinfo);
  }
  return RNA_pointer_create_with_parent(*ptr, &RNA_KeyingSetInfo, ksi);
}

static KS_Path *rna_KeyingSet_paths_add(KeyingSet *keyingset,
                                        ReportList *reports,
                                        ID *id,
                                        const char rna_path[],
                                        int index,
                                        int group_method,
                                        const char group_name[])
{
  KS_Path *ksp = nullptr;
  short flag = 0;

  /* Special case when index = -1, we key the whole array
   * (as with other places where index is used). */
  if (index == -1) {
    flag |= KSP_FLAG_WHOLE_ARRAY;
    index = 0;
  }

  /* if data is valid, call the API function for this */
  if (keyingset) {
    ksp = BKE_keyingset_add_path(keyingset, id, group_name, rna_path, index, flag, group_method);
    keyingset->active_path = BLI_listbase_count(&keyingset->paths);
  }
  else {
    BKE_report(reports, RPT_ERROR, "Keying set path could not be added");
  }

  /* return added path */
  return ksp;
}

static void rna_KeyingSet_paths_remove(KeyingSet *keyingset,
                                       ReportList *reports,
                                       PointerRNA *ksp_ptr)
{
  KS_Path *ksp = static_cast<KS_Path *>(ksp_ptr->data);

  /* if data is valid, call the API function for this */
  if ((keyingset && ksp) == false) {
    BKE_report(reports, RPT_ERROR, "Keying set path could not be removed");
    return;
  }

  /* remove the active path from the KeyingSet */
  BKE_keyingset_free_path(keyingset, ksp);
  ksp_ptr->invalidate();

  /* the active path number will most likely have changed */
  /* TODO: we should get more fancy and actually check if it was removed,
   * but this will do for now */
  keyingset->active_path = 0;
}

static void rna_KeyingSet_paths_clear(KeyingSet *keyingset, ReportList *reports)
{
  /* if data is valid, call the API function for this */
  if (keyingset) {
    KS_Path *ksp, *kspn;

    /* free each path as we go to avoid looping twice */
    for (ksp = static_cast<KS_Path *>(keyingset->paths.first); ksp; ksp = kspn) {
      kspn = ksp->next;
      BKE_keyingset_free_path(keyingset, ksp);
    }

    /* reset the active path, since there aren't any left */
    keyingset->active_path = 0;
  }
  else {
    BKE_report(reports, RPT_ERROR, "Keying set paths could not be removed");
  }
}

/* needs wrapper function to push notifier */
static NlaTrack *rna_NlaTrack_new(ID *id, AnimData *adt, Main *bmain, bContext *C, NlaTrack *track)
{
  NlaTrack *new_track;

  if (track == nullptr) {
    new_track = BKE_nlatrack_new_tail(&adt->nla_tracks, ID_IS_OVERRIDE_LIBRARY(id));
  }
  else {
    new_track = BKE_nlatrack_new_after(&adt->nla_tracks, track, ID_IS_OVERRIDE_LIBRARY(id));
  }

  BKE_nlatrack_set_active(&adt->nla_tracks, new_track);

  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_ADDED, nullptr);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update_ex(bmain, id, ID_RECALC_ANIMATION | ID_RECALC_SYNC_TO_EVAL);

  return new_track;
}

static void rna_NlaTrack_remove(
    ID *id, AnimData *adt, Main *bmain, bContext *C, ReportList *reports, PointerRNA *track_ptr)
{
  NlaTrack *track = static_cast<NlaTrack *>(track_ptr->data);

  if (BLI_findindex(&adt->nla_tracks, track) == -1) {
    BKE_reportf(reports, RPT_ERROR, "NlaTrack '%s' cannot be removed", track->name);
    return;
  }

  BKE_nlatrack_remove_and_free(&adt->nla_tracks, track, true);
  track_ptr->invalidate();

  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_REMOVED, nullptr);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update_ex(bmain, id, ID_RECALC_ANIMATION | ID_RECALC_SYNC_TO_EVAL);
}

static PointerRNA rna_NlaTrack_active_get(PointerRNA *ptr)
{
  AnimData *adt = (AnimData *)ptr->data;
  NlaTrack *track = BKE_nlatrack_find_active(&adt->nla_tracks);
  return RNA_pointer_create_with_parent(*ptr, &RNA_NlaTrack, track);
}

static void rna_NlaTrack_active_set(PointerRNA *ptr, PointerRNA value, ReportList * /*reports*/)
{
  AnimData *adt = (AnimData *)ptr->data;
  NlaTrack *track = (NlaTrack *)value.data;
  BKE_nlatrack_set_active(&adt->nla_tracks, track);
}

static FCurve *rna_Driver_from_existing(AnimData *adt, bContext *C, FCurve *src_driver)
{
  /* verify that we've got a driver to duplicate */
  if (ELEM(nullptr, src_driver, src_driver->driver)) {
    BKE_report(CTX_wm_reports(C), RPT_ERROR, "No valid driver data to create copy of");
    return nullptr;
  }
  else {
    /* just make a copy of the existing one and add to self */
    FCurve *new_fcu = BKE_fcurve_copy(src_driver);

    /* XXX: if we impose any ordering on these someday, this will be problematic */
    BLI_addtail(&adt->drivers, new_fcu);
    return new_fcu;
  }
}

static FCurve *rna_Driver_new(
    ID *id, AnimData *adt, Main *bmain, ReportList *reports, const char *rna_path, int array_index)
{
  if (rna_path[0] == '\0') {
    BKE_report(reports, RPT_ERROR, "F-Curve data path empty, invalid argument");
    return nullptr;
  }

  if (BKE_fcurve_find(&adt->drivers, rna_path, array_index)) {
    BKE_reportf(reports, RPT_ERROR, "Driver '%s[%d]' already exists", rna_path, array_index);
    return nullptr;
  }

  FCurve *fcu = verify_driver_fcurve(id, rna_path, array_index, DRIVER_FCURVE_KEYFRAMES);
  BLI_assert(fcu != nullptr);

  DEG_relations_tag_update(bmain);

  return fcu;
}

static void rna_Driver_remove(AnimData *adt, Main *bmain, ReportList *reports, FCurve *fcu)
{
  if (!BLI_remlink_safe(&adt->drivers, fcu)) {
    BKE_report(reports, RPT_ERROR, "Driver not found in this animation data");
    return;
  }
  BKE_fcurve_free(fcu);
  DEG_relations_tag_update(bmain);
}

static FCurve *rna_Driver_find(AnimData *adt,
                               ReportList *reports,
                               const char *data_path,
                               int index)
{
  if (data_path[0] == '\0') {
    BKE_report(reports, RPT_ERROR, "F-Curve data path empty, invalid argument");
    return nullptr;
  }

  /* Returns nullptr if not found. */
  return BKE_fcurve_find(&adt->drivers, data_path, index);
}

std::optional<std::string> rna_AnimData_path(const PointerRNA * /*ptr*/)
{
  return std::string{"animation_data"};
}

bool rna_AnimaData_override_apply(Main *bmain, RNAPropertyOverrideApplyContext &rnaapply_ctx)
{
  PointerRNA *ptr_dst = &rnaapply_ctx.ptr_dst;
  PointerRNA *ptr_src = &rnaapply_ctx.ptr_src;
  PointerRNA *ptr_storage = &rnaapply_ctx.ptr_storage;
  PropertyRNA *prop_dst = rnaapply_ctx.prop_dst;
  PropertyRNA *prop_src = rnaapply_ctx.prop_src;
  const int len_dst = rnaapply_ctx.len_src;
  const int len_src = rnaapply_ctx.len_src;
  const int len_storage = rnaapply_ctx.len_storage;
  IDOverrideLibraryPropertyOperation *opop = rnaapply_ctx.liboverride_operation;

  BLI_assert(len_dst == len_src && (!ptr_storage || len_dst == len_storage) && len_dst == 0);
  BLI_assert_msg(opop->operation == LIBOVERRIDE_OP_REPLACE,
                 "Unsupported RNA override operation on animdata pointer");
  UNUSED_VARS_NDEBUG(ptr_storage, len_dst, len_src, len_storage, opop);

  /* AnimData is a special case, since you cannot edit/replace it, it's either existent or not.
   * Further more, when an animdata is added to the linked reference later on, the one created
   * for the liboverride needs to be 'merged', such that its overridable data is kept. */
  AnimData *adt_dst = static_cast<AnimData *>(RNA_property_pointer_get(ptr_dst, prop_dst).data);
  AnimData *adt_src = static_cast<AnimData *>(RNA_property_pointer_get(ptr_src, prop_src).data);

  if (adt_dst == nullptr && adt_src != nullptr) {
    /* Copy anim data from reference into final local ID. */
    BKE_animdata_copy_id(nullptr, ptr_dst->owner_id, ptr_src->owner_id, 0);
    RNA_property_update_main(bmain, nullptr, ptr_dst, prop_dst);
    return true;
  }
  else if (adt_dst != nullptr && adt_src == nullptr) {
    /* Override has cleared/removed anim data from its reference. */
    BKE_animdata_free(ptr_dst->owner_id, true);
    RNA_property_update_main(bmain, nullptr, ptr_dst, prop_dst);
    return true;
  }
  else if (adt_dst != nullptr && adt_src != nullptr) {
    /* Override had to create an anim data, but now its reference also has one, need to merge
     * them by keeping the few overridable data from the liboverride, while using the animdata of
     * the reference.
     *
     * Note that this case will not be encountered when the linked reference data already had
     * anim data, since there will be no operation for the animdata pointer itself then, only
     * potentially for its internal overridable data (NLA, action...). */
    id_us_min(reinterpret_cast<ID *>(adt_dst->action));
    adt_dst->action = adt_src->action;
    id_us_plus(reinterpret_cast<ID *>(adt_dst->action));
    id_us_min(reinterpret_cast<ID *>(adt_dst->tmpact));

    adt_dst->slot_handle = adt_src->slot_handle;
    adt_dst->tmp_slot_handle = adt_src->tmp_slot_handle;
    STRNCPY(adt_dst->last_slot_identifier, adt_src->last_slot_identifier);
    STRNCPY(adt_dst->tmp_last_slot_identifier, adt_src->tmp_last_slot_identifier);
    adt_dst->tmpact = adt_src->tmpact;
    id_us_plus(reinterpret_cast<ID *>(adt_dst->tmpact));
    adt_dst->act_blendmode = adt_src->act_blendmode;
    adt_dst->act_extendmode = adt_src->act_extendmode;
    adt_dst->act_influence = adt_src->act_influence;
    adt_dst->flag = adt_src->flag;

    /* NLA tracks: since overrides are always after tracks from linked reference, we can 'just'
     * move the whole list from `src` to the end of the list of `dst` (which currently contains
     * tracks from linked reference). then active track and strip pointers can be kept as-is. */
    BLI_movelisttolist(&adt_dst->nla_tracks, &adt_src->nla_tracks);
    adt_dst->act_track = adt_src->act_track;
    adt_dst->actstrip = adt_src->actstrip;

    DEG_relations_tag_update(bmain);
    ANIM_id_update(bmain, ptr_dst->owner_id);
  }

  return false;
}

bool rna_NLA_tracks_override_apply(Main *bmain, RNAPropertyOverrideApplyContext &rnaapply_ctx)
{
  PointerRNA *ptr_dst = &rnaapply_ctx.ptr_dst;
  PointerRNA *ptr_src = &rnaapply_ctx.ptr_src;
  PropertyRNA *prop_dst = rnaapply_ctx.prop_dst;
  IDOverrideLibraryPropertyOperation *opop = rnaapply_ctx.liboverride_operation;

  BLI_assert_msg(opop->operation == LIBOVERRIDE_OP_INSERT_AFTER,
                 "Unsupported RNA override operation on constraints collection");

  AnimData *anim_data_dst = (AnimData *)ptr_dst->data;
  AnimData *anim_data_src = (AnimData *)ptr_src->data;

  /* Remember that insertion operations are defined and stored in correct order, which means that
   * even if we insert several items in a row, we always insert first one, then second one, etc.
   * So we should always find 'anchor' track in both _src *and* _dst.
   *
   * This is only true however is NLA tracks do not get removed from linked data. Otherwise, an
   * index-based reference may lead to lost data. */
  NlaTrack *nla_track_anchor = nullptr;
#  if 0
  /* This is not working so well with index-based insertion, especially in case some tracks get
   * added to lib linked data. So we simply add locale tracks at the end of the list always, order
   * of override operations should ensure order of local tracks is preserved properly. */
  if (opop->subitem_reference_index >= 0) {
    nla_track_anchor = BLI_findlink(&anim_data_dst->nla_tracks, opop->subitem_reference_index);
  }
  /* Otherwise we just insert in first position. */
#  else
  nla_track_anchor = static_cast<NlaTrack *>(anim_data_dst->nla_tracks.last);
#  endif

  NlaTrack *nla_track_src = nullptr;
  if (opop->subitem_local_index >= 0) {
    nla_track_src = static_cast<NlaTrack *>(
        BLI_findlink(&anim_data_src->nla_tracks, opop->subitem_local_index));
  }

  if (nla_track_src == nullptr) {
    /* Can happen if tracks were removed from linked data. */
    return false;
  }

  NlaTrack *nla_track_dst = BKE_nlatrack_copy(bmain, nla_track_src, true, 0);

  /* This handles nullptr anchor as expected by adding at head of list. */
  BLI_insertlinkafter(&anim_data_dst->nla_tracks, nla_track_anchor, nla_track_dst);

  // printf("%s: We inserted a NLA Track...\n", __func__);

  RNA_property_update_main(bmain, nullptr, ptr_dst, prop_dst);
  return true;
}

#else

/* helper function for Keying Set -> keying settings */
static void rna_def_common_keying_flags(StructRNA *srna, short reg)
{
  PropertyRNA *prop;

  /* override scene/userpref defaults? */
  prop = RNA_def_property(srna, "use_insertkey_override_needed", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "keyingoverride", INSERTKEY_NEEDED);
  RNA_def_property_ui_text(prop,
                           "Override Insert Keyframes Default- Only Needed",
                           "Override default setting to only insert keyframes where they're "
                           "needed in the relevant F-Curves");
  if (reg) {
    RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  }

  prop = RNA_def_property(srna, "use_insertkey_override_visual", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "keyingoverride", INSERTKEY_MATRIX);
  RNA_def_property_ui_text(
      prop,
      "Override Insert Keyframes Default - Visual",
      "Override default setting to insert keyframes based on 'visual transforms'");
  if (reg) {
    RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  }

  /* value to override defaults with */
  prop = RNA_def_property(srna, "use_insertkey_needed", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "keyingflag", INSERTKEY_NEEDED);
  RNA_def_property_ui_text(prop,
                           "Insert Keyframes - Only Needed",
                           "Only insert keyframes where they're needed in the relevant F-Curves");
  if (reg) {
    RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  }

  prop = RNA_def_property(srna, "use_insertkey_visual", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "keyingflag", INSERTKEY_MATRIX);
  RNA_def_property_ui_text(
      prop, "Insert Keyframes - Visual", "Insert keyframes based on 'visual transforms'");
  if (reg) {
    RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  }
}

/* --- */

/* To avoid repeating it twice! */
#  define KEYINGSET_IDNAME_DOC \
    "If this is set, the Keying Set gets a custom ID, otherwise it takes " \
    "the name of the class used to define the Keying Set (for example, " \
    "if the class name is \"BUILTIN_KSI_location\", and bl_idname is not " \
    "set by the script, then bl_idname = \"BUILTIN_KSI_location\")"

static void rna_def_keyingset_info(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "KeyingSetInfo", nullptr);
  RNA_def_struct_sdna(srna, "KeyingSetInfo");
  RNA_def_struct_ui_text(
      srna, "Keying Set Info", "Callback function defines for builtin Keying Sets");
  RNA_def_struct_refine_func(srna, "rna_KeyingSetInfo_refine");
  RNA_def_struct_register_funcs(
      srna, "rna_KeyingSetInfo_register", "rna_KeyingSetInfo_unregister", nullptr);

  /* Properties --------------------- */

  RNA_define_verify_sdna(false); /* not in sdna */

  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "idname");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "ID Name", KEYINGSET_IDNAME_DOC);

  prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "name");
  RNA_def_property_ui_text(prop, "UI Name", "");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_flag(prop, PROP_REGISTER);

  prop = RNA_def_property(srna, "bl_description", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "description");
  RNA_def_property_string_maxlength(prop, RNA_DYN_DESCR_MAX); /* else it uses the pointer size! */
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
  RNA_def_property_ui_text(prop, "Description", "A short description of the keying set");

  /* Regarding why we don't use rna_def_common_keying_flags() here:
   * - Using it would keep this case in sync with the other places
   *   where these options are exposed (which are optimized for being
   *   used in the UI).
   * - Unlike all the other places, this case is used for defining
   *   new "built in" Keying Sets via the Python API. In that case,
   *   it makes more sense to expose these in a way more similar to
   *   other places featuring bl_idname/label/description (i.e. operators)
   */
  prop = RNA_def_property(srna, "bl_options", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL | PROP_ENUM_FLAG);
  RNA_def_property_enum_sdna(prop, nullptr, "keyingflag");
  RNA_def_property_enum_items(prop, rna_enum_keying_flag_items);
  RNA_def_property_ui_text(prop, "Options", "Keying Set options to use when inserting keyframes");

  RNA_define_verify_sdna(true);

  /* Function Callbacks ------------- */
  /* poll */
  func = RNA_def_function(srna, "poll", nullptr);
  RNA_def_function_ui_description(func, "Test if Keying Set can be used or not");
  RNA_def_function_flag(func, FUNC_REGISTER);
  RNA_def_function_return(func, RNA_def_boolean(func, "ok", true, "", ""));
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* iterator */
  func = RNA_def_function(srna, "iterator", nullptr);
  RNA_def_function_ui_description(
      func, "Call generate() on the structs which have properties to be keyframed");
  RNA_def_function_flag(func, FUNC_REGISTER);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "ks", "KeyingSet", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* generate */
  func = RNA_def_function(srna, "generate", nullptr);
  RNA_def_function_ui_description(
      func, "Add Paths to the Keying Set to keyframe the properties of the given data");
  RNA_def_function_flag(func, FUNC_REGISTER);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "ks", "KeyingSet", "", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "data", "AnyType", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
}

static void rna_def_keyingset_path(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "KeyingSetPath", nullptr);
  RNA_def_struct_sdna(srna, "KS_Path");
  RNA_def_struct_ui_text(srna, "Keying Set Path", "Path to a setting for use in a Keying Set");

  /* ID */
  prop = RNA_def_property(srna, "id", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ID");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_editable_func(prop, "rna_ksPath_id_editable");
  RNA_def_property_pointer_funcs(prop, nullptr, nullptr, "rna_ksPath_id_typef", nullptr);
  RNA_def_property_ui_text(prop,
                           "ID-Block",
                           "ID-Block that keyframes for Keying Set should be added to "
                           "(for Absolute Keying Sets only)");
  RNA_def_property_update(
      prop, NC_SCENE | ND_KEYINGSET | NA_EDITED, nullptr); /* XXX: maybe a bit too noisy */

  prop = RNA_def_property(srna, "id_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "idtype");
  RNA_def_property_enum_items(prop, rna_enum_id_type_items);
  RNA_def_property_enum_default(prop, ID_OB);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_ksPath_id_type_set", nullptr);
  RNA_def_property_ui_text(prop, "ID Type", "Type of ID-block that can be used");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ID);
  RNA_def_property_update(
      prop, NC_SCENE | ND_KEYINGSET | NA_EDITED, nullptr); /* XXX: maybe a bit too noisy */

  /* Group */
  prop = RNA_def_property(srna, "group", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Group Name", "Name of Action Group to assign setting(s) for this path to");
  RNA_def_property_update(
      prop, NC_SCENE | ND_KEYINGSET | NA_EDITED, nullptr); /* XXX: maybe a bit too noisy */

  /* Grouping */
  prop = RNA_def_property(srna, "group_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "groupmode");
  RNA_def_property_enum_items(prop, rna_enum_keyingset_path_grouping_items);
  RNA_def_property_ui_text(
      prop, "Grouping Method", "Method used to define which Group-name to use");
  RNA_def_property_update(
      prop, NC_SCENE | ND_KEYINGSET | NA_EDITED, nullptr); /* XXX: maybe a bit too noisy */

  /* Path + Array Index */
  prop = RNA_def_property(srna, "data_path", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_ksPath_RnaPath_get", "rna_ksPath_RnaPath_length", "rna_ksPath_RnaPath_set");
  RNA_def_property_ui_text(prop, "Data Path", "Path to property setting");
  RNA_def_struct_name_property(srna, prop); /* XXX this is the best indicator for now... */
  RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET | NA_EDITED, nullptr);

  /* called 'index' when given as function arg */
  prop = RNA_def_property(srna, "array_index", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "RNA Array Index", "Index to the specific setting if applicable");
  RNA_def_property_update(
      prop, NC_SCENE | ND_KEYINGSET | NA_EDITED, nullptr); /* XXX: maybe a bit too noisy */

  /* Flags */
  prop = RNA_def_property(srna, "use_entire_array", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", KSP_FLAG_WHOLE_ARRAY);
  RNA_def_property_ui_text(
      prop,
      "Entire Array",
      "When an 'array/vector' type is chosen (Location, Rotation, Color, etc.), "
      "entire array is to be used");
  RNA_def_property_update(
      prop, NC_SCENE | ND_KEYINGSET | NA_EDITED, nullptr); /* XXX: maybe a bit too noisy */

  /* Keyframing Settings */
  rna_def_common_keying_flags(srna, 0);
}

/* keyingset.paths */
static void rna_def_keyingset_paths(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  PropertyRNA *prop;

  RNA_def_property_srna(cprop, "KeyingSetPaths");
  srna = RNA_def_struct(brna, "KeyingSetPaths", nullptr);
  RNA_def_struct_sdna(srna, "KeyingSet");
  RNA_def_struct_ui_text(srna, "Keying set paths", "Collection of keying set paths");

  /* Add Path */
  func = RNA_def_function(srna, "add", "rna_KeyingSet_paths_add");
  RNA_def_function_ui_description(func, "Add a new path for the Keying Set");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  /* return arg */
  parm = RNA_def_pointer(
      func, "ksp", "KeyingSetPath", "New Path", "Path created and added to the Keying Set");
  RNA_def_function_return(func, parm);
  /* ID-block for target */
  parm = RNA_def_pointer(
      func, "target_id", "ID", "Target ID", "ID data-block for the destination");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* rna-path */
  /* XXX hopefully this is long enough */
  parm = RNA_def_string(
      func, "data_path", nullptr, 256, "Data-Path", "RNA-Path to destination property");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* index (defaults to -1 for entire array) */
  RNA_def_int(func,
              "index",
              -1,
              -1,
              INT_MAX,
              "Index",
              "The index of the destination property (i.e. axis of Location/Rotation/etc.), "
              "or -1 for the entire array",
              0,
              INT_MAX);
  /* grouping */
  RNA_def_enum(func,
               "group_method",
               rna_enum_keyingset_path_grouping_items,
               KSP_GROUP_KSNAME,
               "Grouping Method",
               "Method used to define which Group-name to use");
  RNA_def_string(
      func,
      "group_name",
      nullptr,
      64,
      "Group Name",
      "Name of Action Group to assign destination to (only if grouping mode is to use this name)");

  /* Remove Path */
  func = RNA_def_function(srna, "remove", "rna_KeyingSet_paths_remove");
  RNA_def_function_ui_description(func, "Remove the given path from the Keying Set");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  /* path to remove */
  parm = RNA_def_pointer(func, "path", "KeyingSetPath", "Path", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  /* Remove All Paths */
  func = RNA_def_function(srna, "clear", "rna_KeyingSet_paths_clear");
  RNA_def_function_ui_description(func, "Remove all the paths from the Keying Set");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "KeyingSetPath");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_editable_func(prop, "rna_KeyingSet_active_ksPath_editable");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_KeyingSet_active_ksPath_get",
                                 "rna_KeyingSet_active_ksPath_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_ui_text(
      prop, "Active Keying Set", "Active Keying Set used to insert/delete keyframes");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "active_path");
  RNA_def_property_int_funcs(prop,
                             "rna_KeyingSet_active_ksPath_index_get",
                             "rna_KeyingSet_active_ksPath_index_set",
                             "rna_KeyingSet_active_ksPath_index_range");
  RNA_def_property_ui_text(prop, "Active Path Index", "Current Keying Set index");
}

static void rna_def_keyingset(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "KeyingSet", nullptr);
  RNA_def_struct_ui_text(srna, "Keying Set", "Settings that should be keyframed together");

  /* Id/Label */
  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "idname");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "ID Name", KEYINGSET_IDNAME_DOC);
  /* NOTE: disabled, as ID name shouldn't be editable */
#  if 0
  RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET | NA_RENAME, nullptr);
#  endif

  prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "name");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_KeyingSet_name_set");
  RNA_def_property_ui_text(prop, "UI Name", "");
  RNA_def_struct_ui_icon(srna, ICON_KEYINGSET);
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_SCENE | ND_KEYINGSET | NA_RENAME, nullptr);

  prop = RNA_def_property(srna, "bl_description", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "description");
  RNA_def_property_string_maxlength(prop, RNA_DYN_DESCR_MAX); /* else it uses the pointer size! */
  RNA_def_property_ui_text(prop, "Description", "A short description of the keying set");

  /* KeyingSetInfo (Type Info) for Builtin Sets only. */
  prop = RNA_def_property(srna, "type_info", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "KeyingSetInfo");
  RNA_def_property_pointer_funcs(prop, "rna_KeyingSet_typeinfo_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Type Info", "Callback function defines for built-in Keying Sets");

  /* Paths */
  prop = RNA_def_property(srna, "paths", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "paths", nullptr);
  RNA_def_property_struct_type(prop, "KeyingSetPath");
  RNA_def_property_ui_text(
      prop, "Paths", "Keying Set Paths to define settings that get keyframed together");
  rna_def_keyingset_paths(brna, prop);

  /* Flags */
  prop = RNA_def_property(srna, "is_path_absolute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", KEYINGSET_ABSOLUTE);
  RNA_def_property_ui_text(prop,
                           "Absolute",
                           "Keying Set defines specific paths/settings to be keyframed "
                           "(i.e. is not reliant on context info)");

  /* Keyframing Flags */
  rna_def_common_keying_flags(srna, 0);

  /* Keying Set API */
  RNA_api_keyingset(srna);
}

#  undef KEYINGSET_IDNAME_DOC
/* --- */

static void rna_api_animdata_nla_tracks(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *parm;
  FunctionRNA *func;

  PropertyRNA *prop;

  RNA_def_property_srna(cprop, "NlaTracks");
  srna = RNA_def_struct(brna, "NlaTracks", nullptr);
  RNA_def_struct_sdna(srna, "AnimData");
  RNA_def_struct_ui_text(srna, "NLA Tracks", "Collection of NLA Tracks");

  func = RNA_def_function(srna, "new", "rna_NlaTrack_new");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN | FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Add a new NLA Track");
  RNA_def_pointer(func, "prev", "NlaTrack", "", "NLA Track to add the new one after");
  /* return type */
  parm = RNA_def_pointer(func, "track", "NlaTrack", "", "New NLA Track");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_NlaTrack_remove");
  RNA_def_function_flag(func,
                        FUNC_USE_SELF_ID | FUNC_USE_REPORTS | FUNC_USE_MAIN | FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Remove a NLA Track");
  parm = RNA_def_pointer(func, "track", "NlaTrack", "", "NLA Track to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "NlaTrack");
  RNA_def_property_pointer_funcs(
      prop, "rna_NlaTrack_active_get", "rna_NlaTrack_active_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active Track", "Active NLA Track");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ACTION);
  /* XXX: should (but doesn't) update the active track in the NLA window */
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA | NA_SELECTED, nullptr);
}

static void rna_api_animdata_drivers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *parm;
  FunctionRNA *func;

  // PropertyRNA *prop;

  RNA_def_property_srna(cprop, "AnimDataDrivers");
  srna = RNA_def_struct(brna, "AnimDataDrivers", nullptr);
  RNA_def_struct_sdna(srna, "AnimData");
  RNA_def_struct_ui_text(srna, "Drivers", "Collection of Driver F-Curves");

  /* Match: ActionFCurves.new/remove */

  /* AnimData.drivers.new(...) */
  func = RNA_def_function(srna, "new", "rna_Driver_new");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS | FUNC_USE_MAIN);
  parm = RNA_def_string(func, "data_path", nullptr, 0, "Data Path", "F-Curve data path to use");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "Array index", 0, INT_MAX);
  /* return type */
  parm = RNA_def_pointer(func, "driver", "FCurve", "", "Newly Driver F-Curve");
  RNA_def_function_return(func, parm);

  /* AnimData.drivers.remove(...) */
  func = RNA_def_function(srna, "remove", "rna_Driver_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_MAIN);
  parm = RNA_def_pointer(func, "driver", "FCurve", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  /* AnimData.drivers.from_existing(...) */
  func = RNA_def_function(srna, "from_existing", "rna_Driver_from_existing");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Add a new driver given an existing one");
  RNA_def_pointer(func,
                  "src_driver",
                  "FCurve",
                  "",
                  "Existing Driver F-Curve to use as template for a new one");
  /* return type */
  parm = RNA_def_pointer(func, "driver", "FCurve", "", "New Driver F-Curve");
  RNA_def_function_return(func, parm);

  /* AnimData.drivers.find(...) */
  func = RNA_def_function(srna, "find", "rna_Driver_find");
  RNA_def_function_ui_description(
      func,
      "Find a driver F-Curve. Note that this function performs a linear scan "
      "of all driver F-Curves.");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(func, "data_path", nullptr, 0, "Data Path", "F-Curve data path");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "Array index", 0, INT_MAX);
  /* return type */
  parm = RNA_def_pointer(
      func, "fcurve", "FCurve", "", "The found F-Curve, or None if it doesn't exist");
  RNA_def_function_return(func, parm);
}

void rna_def_animdata_common(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "animation_data", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "adt");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_override_funcs(prop, nullptr, nullptr, "rna_AnimaData_override_apply");
  RNA_def_property_ui_text(prop, "Animation Data", "Animation data for this data-block");
}

static void rna_def_animdata(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "AnimData", nullptr);
  RNA_def_struct_ui_text(srna, "Animation Data", "Animation data for data-block");
  RNA_def_struct_ui_icon(srna, ICON_ANIM_DATA);
  RNA_def_struct_path_func(srna, "rna_AnimData_path");

  /* NLA */
  prop = RNA_def_property(srna, "nla_tracks", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "nla_tracks", nullptr);
  RNA_def_property_struct_type(prop, "NlaTrack");
  RNA_def_property_ui_text(prop, "NLA Tracks", "NLA Tracks (i.e. Animation Layers)");
  RNA_def_property_override_flag(prop,
                                 PROPOVERRIDE_OVERRIDABLE_LIBRARY |
                                     PROPOVERRIDE_LIBRARY_INSERTION | PROPOVERRIDE_NO_PROP_NAME);
  RNA_def_property_override_funcs(prop, nullptr, nullptr, "rna_NLA_tracks_override_apply");

  rna_api_animdata_nla_tracks(brna, prop);

  RNA_define_lib_overridable(true);

  /* Active Action */
  prop = RNA_def_property(srna, "action", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Action");
  /* this flag as well as the dynamic test must be defined for this to be editable... */
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_pointer_funcs(
      prop,
      /* Define a getter that is NULL-safe, so that an RNA_AnimData prop with `ptr->data = nullptr`
       * can still be used to get the property. In that case it will always return nullptr, of
       * course, but it won't crash Blender. */
      "rna_AnimData_action_get",
      /* Similarly, for the setter, the NULL-safety allows constructing the AnimData struct on
       * assignment of this "action" property. This is possible because RNA has typed NULL
       * pointers, and thus it knows which setter to call even when `ptr->data` is NULL. */
      "rna_AnimData_action_set",
      nullptr,
      "rna_Action_id_poll");
  RNA_def_property_editable_func(prop, "rna_AnimData_action_editable");
  RNA_def_property_ui_text(prop, "Action", "Active Action for this data-block");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA_ACTCHANGE, "rna_AnimData_dependency_update");

  /* Active Action Settings */
  prop = RNA_def_property(srna, "action_extrapolation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "act_extendmode");
  RNA_def_property_enum_items(prop, rna_enum_nla_mode_extend_items);
  RNA_def_property_ui_text(
      prop,
      "Action Extrapolation",
      "Action to take for gaps past the Active Action's range (when evaluating with NLA)");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA, "rna_AnimData_update");

  prop = RNA_def_property(srna, "action_blend_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "act_blendmode");
  RNA_def_property_enum_items(prop, rna_enum_nla_mode_blend_items);
  RNA_def_property_ui_text(
      prop,
      "Action Blending",
      "Method used for combining Active Action's result with result of NLA stack");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA, "rna_AnimData_update"); /* this will do? */

  prop = RNA_def_property(srna, "action_influence", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "act_influence");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop,
                           "Action Influence",
                           "Amount the Active Action contributes to the result of the NLA stack");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA, "rna_AnimData_update"); /* this will do? */

  /* Temporary action slot for tweak mode. */
  prop = RNA_def_property(srna, "action_tweak_storage", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "tmpact");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_AnimData_tmpact_set", nullptr, "rna_Action_id_poll");
  RNA_def_property_ui_text(prop,
                           "Tweak Mode Action Storage",
                           "Storage to temporarily hold the main action while in tweak mode");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA_ACTCHANGE, "rna_AnimData_dependency_update");

  /* Temporary action slot for tweak mode. Just like `action_slot_handle` this is needed for
   * library overrides to work. */
  prop = RNA_def_property(srna, "action_slot_handle_tweak_storage", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "tmp_slot_handle");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Tweak Mode Action Slot Storage",
                           "Storage to temporarily hold the main action slot while in tweak mode");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA_ACTCHANGE, "rna_AnimData_dependency_update");

  /* Drivers */
  prop = RNA_def_property(srna, "drivers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "drivers", nullptr);
  RNA_def_property_struct_type(prop, "FCurve");
  RNA_def_property_ui_text(prop, "Drivers", "The Drivers/Expressions for this data-block");

  RNA_define_lib_overridable(false);

  rna_api_animdata_drivers(brna, prop);

  RNA_define_lib_overridable(true);

  /* General Settings */
  prop = RNA_def_property(srna, "use_nla", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", ADT_NLA_EVAL_OFF);
  RNA_def_property_ui_text(
      prop, "NLA Evaluation Enabled", "NLA stack is evaluated when evaluating this block");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA, "rna_AnimData_update"); /* this will do? */

  prop = RNA_def_property(srna, "use_tweak_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ADT_NLA_EDIT_ON);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_AnimData_tweakmode_set");
  RNA_def_property_ui_text(
      prop, "Use NLA Tweak Mode", "Whether to enable or disable tweak mode in NLA");
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA, "rna_AnimData_update");
  RNA_def_property_override_funcs(prop, nullptr, nullptr, "rna_AnimData_tweakmode_override_apply");

  prop = RNA_def_property(srna, "use_pin", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", ADT_CURVES_ALWAYS_VISIBLE);
  RNA_def_property_ui_text(prop, "Pin in Graph Editor", "");
  RNA_def_property_update(prop, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, nullptr);

  /* This property is not necessary for the Python API (that is better off using
   * slot references/pointers directly), but it is needed for library overrides
   * to work. */
  prop = RNA_def_property(srna, "action_slot_handle", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "slot_handle");
  RNA_def_property_int_funcs(prop, nullptr, "rna_AnimData_action_slot_handle_set", nullptr);
  RNA_def_property_ui_text(prop,
                           "Action Slot Handle",
                           "A number that identifies which sub-set of the Action is considered "
                           "to be for this data-block");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_override_funcs(
      prop, "rna_AnimData_slot_handle_override_diff", nullptr, nullptr);
  RNA_def_property_update(prop, NC_ANIMATION | ND_NLA_ACTCHANGE, "rna_AnimData_dependency_update");

  prop = RNA_def_property(srna, "last_slot_identifier", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "last_slot_identifier");
  RNA_def_property_ui_text(
      prop,
      "Last Action Slot Identifier",
      "The identifier of the most recently assigned action slot. The slot identifies which "
      "sub-set of the Action is considered to be for this data-block, and its identifier is used "
      "to find the right slot when assigning an Action.");

  prop = RNA_def_property(srna, "action_slot", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ActionSlot");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop,
      "Action Slot",
      "The slot identifies which sub-set of the Action is considered to be for this "
      "data-block, and its name is used to find the right slot when assigning an Action");
  RNA_def_property_pointer_funcs(
      prop, "rna_AnimData_action_slot_get", "rna_AnimData_action_slot_set", nullptr, nullptr);
  RNA_def_property_update(
      prop, NC_ANIMATION | ND_NLA_ACTCHANGE, "rna_AnimData_action_slot_update");
  /* `adt.action_slot` is exposed to RNA as a pointer for things like the action slot selector in
   * the GUI. The ground truth of the assigned slot, however, is `action_slot_handle` declared
   * above. That property is used for library override operations, and this pointer property should
   * just be ignored.
   *
   * This needs PROPOVERRIDE_IGNORE; PROPOVERRIDE_NO_COMPARISON is not suitable here. This property
   * should act as if it is an overridable property (as from the user's perspective, it is), but an
   * override operation should not be created for it. It will be created for `action_slot_handle`,
   * and that's enough. */
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);

  prop = RNA_def_property(srna, "action_suitable_slots", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "ActionSlot");
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_animdata_action_suitable_slots_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Slots", "The list of slots in this animation data-block");

  RNA_define_lib_overridable(false);

  /* Animation Data API */
  RNA_api_animdata(srna);
}

/* --- */

void RNA_def_animation(BlenderRNA *brna)
{
  rna_def_animdata(brna);

  rna_def_keyingset(brna);
  rna_def_keyingset_path(brna);
  rna_def_keyingset_info(brna);
}

#endif
