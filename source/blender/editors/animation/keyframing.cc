/* SPDX-FileCopyrightText: 2009 Blender Authors, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

#include <cstdio>

#include <fmt/format.h>

#include "MEM_guardedalloc.h"

#include "BLI_math_base.h"
#include "BLI_string.h"

#include "BLT_translation.hh"

#include "DNA_ID.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_animsys.h"
#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_nla.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "ED_anim_api.hh"
#include "ED_keyframing.hh"
#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_sequencer.hh"

#include "ANIM_action.hh"
#include "ANIM_action_iterators.hh"
#include "ANIM_animdata.hh"
#include "ANIM_armature.hh"
#include "ANIM_bone_collections.hh"
#include "ANIM_driver.hh"
#include "ANIM_fcurve.hh"
#include "ANIM_keyframing.hh"
#include "ANIM_keyingsets.hh"
#include "ANIM_rna.hh"

#include "SEQ_relations.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "anim_intern.hh"

static KeyingSet *keyingset_get_from_op_with_error(wmOperator *op,
                                                   PropertyRNA *prop,
                                                   Scene *scene);

static wmOperatorStatus delete_key_using_keying_set(bContext *C, wmOperator *op, KeyingSet *ks);

/* ******************************************* */
/* Animation Data Validation */

void update_autoflags_fcurve(FCurve *fcu, bContext *C, ReportList *reports, PointerRNA *ptr)
{
  PointerRNA tmp_ptr;
  PropertyRNA *prop;
  int old_flag = fcu->flag;

  if ((ptr->owner_id == nullptr) && (ptr->data == nullptr)) {
    BKE_report(reports, RPT_ERROR, "No RNA pointer available to retrieve values for this F-curve");
    return;
  }

  /* try to get property we should be affecting */
  if (RNA_path_resolve_property(ptr, fcu->rna_path, &tmp_ptr, &prop) == false) {
    /* property not found... */
    const char *idname = (ptr->owner_id) ? ptr->owner_id->name : RPT_("<No ID pointer>");

    BKE_reportf(reports,
                RPT_ERROR,
                "Could not update flags for this F-curve, as RNA path is invalid for the given ID "
                "(ID = %s, path = %s)",
                idname,
                fcu->rna_path);
    return;
  }

  /* update F-Curve flags */
  blender::animrig::update_autoflags_fcurve_direct(fcu, RNA_property_type(prop));

  if (old_flag != fcu->flag) {
    /* Same as if keyframes had been changed */
    WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
  }
}

/* ------------------------- Insert Key API ------------------------- */

void ED_keyframes_add(FCurve *fcu, int num_keys_to_add)
{
  BLI_assert_msg(num_keys_to_add >= 0, "cannot remove keyframes with this function");

  if (num_keys_to_add == 0) {
    return;
  }

  fcu->bezt = static_cast<BezTriple *>(
      MEM_recallocN(fcu->bezt, sizeof(BezTriple) * (fcu->totvert + num_keys_to_add)));
  BezTriple *bezt = fcu->bezt + fcu->totvert; /* Pointer to the first new one. */

  fcu->totvert += num_keys_to_add;

  /* Iterate over the new keys to update their settings. */
  while (num_keys_to_add--) {
    /* Defaults, ignoring user-preference gives predictable results for API. */
    bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
    bezt->ipo = BEZT_IPO_BEZ;
    bezt->h1 = bezt->h2 = HD_AUTO_ANIM;
    bezt++;
  }
}

/* ******************************************* */
/* KEYFRAME MODIFICATION */

/* mode for commonkey_modifykey */
enum {
  COMMONKEY_MODE_INSERT = 0,
  COMMONKEY_MODE_DELETE,
} /*eCommonModifyKey_Modes*/;

/**
 * Polling callback for use with `ANIM_*_keyframe()` operators
 * This is based on the standard ED_operator_areaactive callback,
 * except that it does special checks for a few space-types too.
 */
static bool modify_key_op_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  Scene *scene = CTX_data_scene(C);

  /* if no area or active scene */
  if (ELEM(nullptr, area, scene)) {
    return false;
  }

  /* should be fine */
  return true;
}

/* Insert Key Operator ------------------------ */

static wmOperatorStatus insert_key_with_keyingset(bContext *C, wmOperator *op, KeyingSet *ks)
{
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  Scene *scene = is_sequencer ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  if (!scene) {
    return OPERATOR_CANCELLED;
  }
  Object *obedit = CTX_data_edit_object(C);
  bool ob_edit_mode = false;

  const float cfra = BKE_scene_frame_get(scene);
  const bool confirm = op->flag & OP_IS_INVOKE;
  /* exit the edit mode to make sure that those object data properties that have been
   * updated since the last switching to the edit mode will be keyframed correctly
   */
  if (obedit && blender::animrig::keyingset_find_id(ks, static_cast<ID *>(obedit->data))) {
    blender::ed::object::mode_set(C, OB_MODE_OBJECT);
    ob_edit_mode = true;
  }

  /* try to insert keyframes for the channels specified by KeyingSet */
  const int num_channels = blender::animrig::apply_keyingset(
      C, nullptr, ks, blender::animrig::ModifyKeyMode::INSERT, cfra);
  if (G.debug & G_DEBUG) {
    BKE_reportf(op->reports,
                RPT_INFO,
                "Keying set '%s' - successfully added %d keyframes",
                ks->name,
                num_channels);
  }

  /* restore the edit mode if necessary */
  if (ob_edit_mode) {
    blender::ed::object::mode_set(C, OB_MODE_EDIT);
  }

  /* report failure or do updates? */
  if (num_channels < 0) {
    BKE_report(op->reports, RPT_ERROR, "No suitable context info for active keying set");
    return OPERATOR_CANCELLED;
  }

  if (num_channels > 0) {
    /* send notifiers that keyframes have been changed */
    WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_ADDED, nullptr);
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  }

  if (confirm) {
    /* if called by invoke (from the UI), make a note that we've inserted keyframes */
    if (num_channels > 0) {
      BKE_reportf(op->reports,
                  RPT_INFO,
                  "Successfully added %d keyframes for keying set '%s'",
                  num_channels,
                  ks->name);
    }
    else {
      BKE_report(op->reports, RPT_WARNING, "Keying set failed to insert any keyframes");
    }
  }

  return OPERATOR_FINISHED;
}

static blender::Vector<RNAPath> construct_rna_paths(PointerRNA *ptr)
{
  eRotationModes rotation_mode;
  blender::Vector<RNAPath> paths;

  if (ptr->type == &RNA_Strip || RNA_struct_is_a(ptr->type, &RNA_Strip)) {
    eKeyInsertChannels insert_channel_flags = eKeyInsertChannels(U.key_insert_channels);
    if (insert_channel_flags & USER_ANIM_KEY_CHANNEL_LOCATION) {
      paths.append({"transform.offset_x"});
      paths.append({"transform.offset_y"});
    }
    if (insert_channel_flags & USER_ANIM_KEY_CHANNEL_ROTATION) {
      paths.append({"transform.rotation"});
    }
    if (insert_channel_flags & USER_ANIM_KEY_CHANNEL_SCALE) {
      paths.append({"transform.scale_x"});
      paths.append({"transform.scale_y"});
    }
    if (insert_channel_flags & USER_ANIM_KEY_CHANNEL_CUSTOM_PROPERTIES) {
      paths.extend(blender::animrig::get_keyable_id_property_paths(*ptr));
    }
    return paths;
  }

  if (ptr->type == &RNA_PoseBone) {
    bPoseChannel *pchan = static_cast<bPoseChannel *>(ptr->data);
    rotation_mode = eRotationModes(pchan->rotmode);
  }
  else if (ptr->type == &RNA_Object) {
    Object *ob = static_cast<Object *>(ptr->data);
    rotation_mode = eRotationModes(ob->rotmode);
  }
  else {
    /* Pointer type not supported. */
    return paths;
  }

  eKeyInsertChannels insert_channel_flags = eKeyInsertChannels(U.key_insert_channels);
  if (insert_channel_flags & USER_ANIM_KEY_CHANNEL_LOCATION) {
    paths.append({"location"});
  }
  if (insert_channel_flags & USER_ANIM_KEY_CHANNEL_ROTATION) {
    switch (rotation_mode) {
      case ROT_MODE_QUAT:
        paths.append({"rotation_quaternion"});
        break;
      case ROT_MODE_AXISANGLE:
        paths.append({"rotation_axis_angle"});
        break;
      case ROT_MODE_XYZ:
      case ROT_MODE_XZY:
      case ROT_MODE_YXZ:
      case ROT_MODE_YZX:
      case ROT_MODE_ZXY:
      case ROT_MODE_ZYX:
        paths.append({"rotation_euler"});
      default:
        break;
    }
  }
  if (insert_channel_flags & USER_ANIM_KEY_CHANNEL_SCALE) {
    paths.append({"scale"});
  }
  if (insert_channel_flags & USER_ANIM_KEY_CHANNEL_ROTATION_MODE) {
    paths.append({"rotation_mode"});
  }

  if (insert_channel_flags & USER_ANIM_KEY_CHANNEL_CUSTOM_PROPERTIES) {
    paths.extend(blender::animrig::get_keyable_id_property_paths(*ptr));
  }
  return paths;
}

/* Fill the list with items depending on the mode of the context. */
static bool get_selection(bContext *C, blender::Vector<PointerRNA> *r_selection)
{
  const eContextObjectMode context_mode = CTX_data_mode_enum(C);
  ScrArea *area = CTX_wm_area(C);

  if (area && area->spacetype == SPACE_SEQ) {
    blender::VectorSet<Strip *> strips = blender::ed::vse::selected_strips_from_context(C);
    for (Strip *strip : strips) {
      PointerRNA ptr;
      ptr = RNA_pointer_create_discrete(&CTX_data_scene(C)->id, &RNA_Strip, strip);
      r_selection->append(ptr);
    }
    return true;
  }

  switch (context_mode) {
    case CTX_MODE_OBJECT: {
      CTX_data_selected_objects(C, r_selection);
      break;
    }
    case CTX_MODE_POSE: {
      CTX_data_selected_pose_bones(C, r_selection);
      break;
    }
    default:
      return false;
  }

  return true;
}

static wmOperatorStatus insert_key(bContext *C, wmOperator *op)
{
  using namespace blender;

  blender::Vector<PointerRNA> selection;
  const bool found_selection = get_selection(C, &selection);
  if (!found_selection) {
    BKE_reportf(op->reports, RPT_ERROR, "Unsupported context mode");
    return OPERATOR_CANCELLED;
  }

  if (selection.is_empty()) {
    BKE_reportf(op->reports, RPT_WARNING, "Nothing selected to key");
    return OPERATOR_CANCELLED;
  }

  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  const float scene_frame = BKE_scene_frame_get(scene);

  const eInsertKeyFlags insert_key_flags = animrig::get_keyframing_flags(scene);
  const eBezTriple_KeyframeType key_type = eBezTriple_KeyframeType(
      scene->toolsettings->keyframe_type);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(
      depsgraph, BKE_scene_frame_get(scene));

  animrig::CombinedKeyingResult combined_result;
  blender::Set<ID *> ids;
  for (PointerRNA &id_ptr : selection) {
    ID *selected_id = id_ptr.owner_id;
    ids.add(selected_id);
    if (!id_can_have_animdata(selected_id)) {
      BKE_reportf(op->reports,
                  RPT_ERROR,
                  "Could not insert keyframe, as this type does not support animation data (ID = "
                  "%s)",
                  selected_id->name);
      continue;
    }
    if (!BKE_id_is_editable(bmain, selected_id)) {
      BKE_reportf(op->reports, RPT_ERROR, "'%s' is not editable", selected_id->name + 2);
      continue;
    }
    Vector<RNAPath> rna_paths = construct_rna_paths(&id_ptr);

    combined_result.merge(animrig::insert_keyframes(bmain,
                                                    &id_ptr,
                                                    std::nullopt,
                                                    rna_paths.as_span(),
                                                    scene_frame,
                                                    anim_eval_context,
                                                    key_type,
                                                    insert_key_flags));
  }

  if (combined_result.get_count(animrig::SingleKeyingResult::SUCCESS) == 0) {
    combined_result.generate_reports(op->reports);
  }

  for (ID *id : ids) {
    DEG_id_tag_update(id, ID_RECALC_ANIMATION_NO_FLUSH);
  }

  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_ADDED, nullptr);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus insert_key_exec(bContext *C, wmOperator *op)
{
  ANIM_deselect_keys_in_animation_editors(C);

  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  Scene *scene = is_sequencer ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  if (!scene) {
    return OPERATOR_CANCELLED;
  }
  /* Use the active keying set if there is one. */
  const int type = RNA_enum_get(op->ptr, "type");
  KeyingSet *ks = ANIM_keyingset_get_from_enum_type(scene, type);
  if (ks) {
    return insert_key_with_keyingset(C, op, ks);
  }
  return insert_key(C, op);
}

static wmOperatorStatus insert_key_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  /* The depsgraph needs to be in an evaluated state to ensure the values we get from the
   * properties are actually the values of the current frame. However we cannot do that in the exec
   * function, as that would mean every call to the operator via python has to re-evaluate the
   * depsgraph, causing performance regressions. */
  CTX_data_ensure_evaluated_depsgraph(C);
  return insert_key_exec(C, op);
}

void ANIM_OT_keyframe_insert(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Insert Keyframe";
  ot->idname = "ANIM_OT_keyframe_insert";
  ot->description =
      "Insert keyframes on the current frame using either the active keying set, or the user "
      "preferences if no keying set is active";

  /* callbacks */
  ot->invoke = insert_key_invoke;
  ot->exec = insert_key_exec;
  ot->poll = modify_key_op_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Allows passing in a keying set when using the Python operator. */
  PropertyRNA *prop = RNA_def_enum(
      ot->srna, "type", rna_enum_dummy_DEFAULT_items, 0, "Keying Set", "The Keying Set to use");
  RNA_def_enum_funcs(prop, ANIM_keying_sets_enum_itemf);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  ot->prop = prop;
}

static wmOperatorStatus keyframe_insert_with_keyingset_exec(bContext *C, wmOperator *op)
{
  ANIM_deselect_keys_in_animation_editors(C);

  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  Scene *scene = is_sequencer ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  if (!scene) {
    return OPERATOR_CANCELLED;
  }
  KeyingSet *ks = keyingset_get_from_op_with_error(op, op->type->prop, scene);
  if (ks == nullptr) {
    return OPERATOR_CANCELLED;
  }
  return insert_key_with_keyingset(C, op, ks);
}

void ANIM_OT_keyframe_insert_by_name(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Insert Keyframe (by name)";
  ot->idname = "ANIM_OT_keyframe_insert_by_name";
  ot->description = "Alternate access to 'Insert Keyframe' for keymaps to use";

  /* callbacks */
  ot->exec = keyframe_insert_with_keyingset_exec;
  ot->poll = modify_key_op_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* keyingset to use (idname) */
  prop = RNA_def_string(
      ot->srna, "type", nullptr, MAX_ID_NAME - 2, "Keying Set", "The Keying Set to use");
  RNA_def_property_string_search_func_runtime(
      prop, ANIM_keyingset_visit_for_search_no_poll, PROP_STRING_SEARCH_SUGGESTION);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  ot->prop = prop;
}

/* Insert Key Operator (With Menu) ------------------------ */
/* This operator checks if a menu should be shown for choosing the KeyingSet to use,
 * then calls the menu if necessary before
 */

static wmOperatorStatus insert_key_menu_invoke(bContext *C,
                                               wmOperator *op,
                                               const wmEvent * /*event*/)
{
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  Scene *scene = is_sequencer ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  if (!scene) {
    return OPERATOR_CANCELLED;
  }

  /* When there is an active keying set and no request to prompt, keyframe immediately. */
  if ((scene->active_keyingset != 0) && !RNA_boolean_get(op->ptr, "always_prompt")) {
    /* Just call the exec() on the active keying-set. */
    RNA_enum_set(op->ptr, "type", 0);
    return op->type->exec(C, op);
  }

  /* Show a menu listing all keying-sets, the enum is expanded here to make use of the
   * operator that accesses the keying-set by name. This is important for the ability
   * to assign shortcuts to arbitrarily named keying sets. See #89560.
   * These menu items perform the key-frame insertion (not this operator)
   * hence the #OPERATOR_INTERFACE return. */
  uiPopupMenu *pup = UI_popup_menu_begin(
      C, WM_operatortype_name(op->type, op->ptr).c_str(), ICON_NONE);
  uiLayout *layout = UI_popup_menu_layout(pup);

  /* Even though `ANIM_OT_keyframe_insert_menu` can show a menu in one line,
   * prefer `ANIM_OT_keyframe_insert_by_name` so users can bind keys to specific
   * keying sets by name in the key-map instead of the index which isn't stable. */
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "type");
  const EnumPropertyItem *item_array = nullptr;
  int totitem;
  bool free;

  RNA_property_enum_items_gettexted(C, op->ptr, prop, &item_array, &totitem, &free);

  for (int i = 0; i < totitem; i++) {
    const EnumPropertyItem *item = &item_array[i];
    if (item->identifier[0] != '\0') {
      PointerRNA op_ptr = layout->op("ANIM_OT_keyframe_insert_by_name", item->name, item->icon);
      RNA_string_set(&op_ptr, "type", item->identifier);
    }
    else {
      /* This enum shouldn't contain headings, assert there are none.
       * NOTE: If in the future the enum includes them, additional layout code can be
       * added to show them - although that doesn't seem likely. */
      BLI_assert(item->name == nullptr);
      layout->separator();
    }
  }

  if (free) {
    MEM_freeN(item_array);
  }

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

void ANIM_OT_keyframe_insert_menu(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Insert Keyframe Menu";
  ot->idname = "ANIM_OT_keyframe_insert_menu";
  ot->description =
      "Insert Keyframes for specified Keying Set, with menu of available Keying Sets if undefined";

  /* callbacks */
  ot->invoke = insert_key_menu_invoke;
  ot->exec = keyframe_insert_with_keyingset_exec;
  ot->poll = ED_operator_areaactive;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* keyingset to use (dynamic enum) */
  prop = RNA_def_enum(
      ot->srna, "type", rna_enum_dummy_DEFAULT_items, 0, "Keying Set", "The Keying Set to use");
  RNA_def_enum_funcs(prop, ANIM_keying_sets_enum_itemf);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  ot->prop = prop;

  /* whether the menu should always be shown
   * - by default, the menu should only be shown when there is no active Keying Set (2.5 behavior),
   *   although in some cases it might be useful to always shown (pre 2.5 behavior)
   */
  prop = RNA_def_boolean(ot->srna, "always_prompt", false, "Always Show Menu", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/* Delete Key Operator ------------------------ */

static wmOperatorStatus delete_key_exec(bContext *C, wmOperator *op)
{
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  Scene *scene = is_sequencer ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  if (!scene) {
    return OPERATOR_CANCELLED;
  }
  KeyingSet *ks = keyingset_get_from_op_with_error(op, op->type->prop, scene);
  if (ks == nullptr) {
    return OPERATOR_CANCELLED;
  }

  return delete_key_using_keying_set(C, op, ks);
}

static wmOperatorStatus delete_key_using_keying_set(bContext *C, wmOperator *op, KeyingSet *ks)
{
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  Scene *scene = is_sequencer ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  if (!scene) {
    return OPERATOR_CANCELLED;
  }
  float cfra = BKE_scene_frame_get(scene);
  int num_channels;
  const bool confirm = op->flag & OP_IS_INVOKE;

  /* Try to delete keyframes for the channels specified by KeyingSet. */
  num_channels = blender::animrig::apply_keyingset(
      C, nullptr, ks, blender::animrig::ModifyKeyMode::DELETE_KEY, cfra);
  if (G.debug & G_DEBUG) {
    printf("KeyingSet '%s' - Successfully removed %d Keyframes\n", ks->name, num_channels);
  }

  /* Report failure or do updates? */
  if (num_channels < 0) {
    BKE_report(op->reports, RPT_ERROR, "No suitable context info for active keying set");
    return OPERATOR_CANCELLED;
  }

  if (num_channels > 0) {
    WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_REMOVED, nullptr);

    /* VSE notifiers. */
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
    WM_event_add_notifier(C, NC_ANIMATION, nullptr);
  }

  if (confirm) {
    /* If called by invoke (from the UI), make a note that we've removed keyframes. */
    if (num_channels > 0) {
      BKE_reportf(op->reports,
                  RPT_INFO,
                  "Successfully removed %d keyframes for keying set '%s'",
                  num_channels,
                  ks->name);
    }
    else {
      BKE_report(op->reports, RPT_WARNING, "Keying set failed to remove any keyframes");
    }
  }
  return OPERATOR_FINISHED;
}

void ANIM_OT_keyframe_delete(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Delete Keying-Set Keyframe";
  ot->idname = "ANIM_OT_keyframe_delete";
  ot->description =
      "Delete keyframes on the current frame for all properties in the specified Keying Set";

  /* callbacks */
  ot->exec = delete_key_exec;
  ot->poll = modify_key_op_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* keyingset to use (dynamic enum) */
  prop = RNA_def_enum(
      ot->srna, "type", rna_enum_dummy_DEFAULT_items, 0, "Keying Set", "The Keying Set to use");
  RNA_def_enum_funcs(prop, ANIM_keying_sets_enum_itemf);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  ot->prop = prop;
}

void ANIM_OT_keyframe_delete_by_name(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Delete Keying-Set Keyframe (by name)";
  ot->idname = "ANIM_OT_keyframe_delete_by_name";
  ot->description = "Alternate access to 'Delete Keyframe' for keymaps to use";

  /* callbacks */
  ot->exec = delete_key_exec;
  ot->poll = modify_key_op_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* keyingset to use (idname) */
  prop = RNA_def_string(
      ot->srna, "type", nullptr, MAX_ID_NAME - 2, "Keying Set", "The Keying Set to use");
  RNA_def_property_string_search_func_runtime(
      prop, ANIM_keyingset_visit_for_search_no_poll, PROP_STRING_SEARCH_SUGGESTION);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  ot->prop = prop;
}

/* Delete Key Operator ------------------------ */
/* NOTE: Although this version is simpler than the more generic version for KeyingSets,
 * it is more useful for animators working in the 3D view.
 */

/* While in pose mode, the selection of bones has to be considered. */
static bool can_delete_fcurve(FCurve *fcu, Object *ob)
{
  bool can_delete = false;
  /* in pose mode, only delete the F-Curve if it belongs to a selected bone */
  if (ob->mode & OB_MODE_POSE) {
    if (fcu->rna_path) {
      /* Get bone-name, and check if this bone is selected. */
      bPoseChannel *pchan = nullptr;
      char bone_name[sizeof(pchan->name)];
      if (BLI_str_quoted_substr(fcu->rna_path, "pose.bones[", bone_name, sizeof(bone_name))) {
        pchan = BKE_pose_channel_find_name(ob->pose, bone_name);
        /* Delete if bone is selected. */
        if ((pchan) && (pchan->bone)) {
          if (pchan->flag & POSE_SELECTED) {
            can_delete = true;
          }
        }
      }
    }
  }
  else {
    /* object mode - all of Object's F-Curves are affected */
    /* TODO: this logic isn't solid. Only delete FCurves of the object, not of bones in this case.
     */
    can_delete = true;
  }

  return can_delete;
}

static wmOperatorStatus clear_anim_v3d_exec(bContext *C, wmOperator * /*op*/)
{
  using namespace blender::animrig;
  bool changed = false;

  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    /* just those in active action... */
    if ((ob->adt) && (ob->adt->action)) {
      AnimData *adt = ob->adt;
      bAction *dna_action = adt->action;
      FCurve *fcu, *fcn;

      Action &action = dna_action->wrap();
      if (action.is_action_layered()) {
        blender::Vector<FCurve *> fcurves_to_delete;
        foreach_fcurve_in_action_slot(action, adt->slot_handle, [&](FCurve &fcurve) {
          if (can_delete_fcurve(&fcurve, ob)) {
            fcurves_to_delete.append(&fcurve);
          }
        });
        for (FCurve *fcurve : fcurves_to_delete) {
          action_fcurve_remove(action, *fcurve);
          DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
          changed = true;
        }
        DEG_id_tag_update(&ob->adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
      }
      else {
        for (fcu = static_cast<FCurve *>(dna_action->curves.first); fcu; fcu = fcn) {
          fcn = fcu->next;
          /* delete F-Curve completely */
          if (can_delete_fcurve(fcu, ob)) {
            blender::animrig::animdata_fcurve_delete(adt, fcu);
            DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
            changed = true;
          }
        }
      }

      /* Delete the action itself if it is empty. */
      if (action.is_action_legacy() && blender::animrig::animdata_remove_empty_action(adt)) {
        changed = true;
      }
    }
  }
  CTX_DATA_END;

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  /* send updates */
  WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus clear_anim_v3d_invoke(bContext *C,
                                              wmOperator *op,
                                              const wmEvent * /*event*/)
{
  if (RNA_boolean_get(op->ptr, "confirm")) {
    return WM_operator_confirm_ex(C,
                                  op,
                                  IFACE_("Remove animation from selected objects?"),
                                  nullptr,
                                  CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Remove"),
                                  ALERT_ICON_NONE,
                                  false);
  }
  return clear_anim_v3d_exec(C, op);
}

void ANIM_OT_keyframe_clear_v3d(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Animation";
  ot->description = "Remove all keyframe animation for selected objects";
  ot->idname = "ANIM_OT_keyframe_clear_v3d";

  /* callbacks */
  ot->invoke = clear_anim_v3d_invoke;
  ot->exec = clear_anim_v3d_exec;

  ot->poll = ED_operator_areaactive;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  WM_operator_properties_confirm_or_exec(ot);
}

static blender::Vector<std::string> get_selected_strips_rna_paths(
    blender::Vector<PointerRNA> &selection)
{
  blender::Vector<std::string> selected_strips_rna_paths;
  for (PointerRNA &id_ptr : selection) {
    if (RNA_struct_is_a(id_ptr.type, &RNA_Strip)) {
      std::optional<std::string> rna_path = RNA_path_from_ID_to_struct(&id_ptr);
      selected_strips_rna_paths.append(*rna_path);
    }
  }
  return selected_strips_rna_paths;
}

static void invalidate_strip_caches(blender::Vector<PointerRNA> selection, Scene *scene)
{
  for (PointerRNA &id_ptr : selection) {
    if (RNA_struct_is_a(id_ptr.type, &RNA_Strip)) {
      ::Strip *strip = static_cast<::Strip *>(id_ptr.data);
      blender::seq::relations_invalidate_cache(scene, strip);
    }
  }
}

static bool fcurve_belongs_to_strip(const FCurve &fcurve, const std::string &strip_path)
{
  return fcurve.rna_path &&
         std::strncmp(fcurve.rna_path, strip_path.c_str(), strip_path.length()) == 0;
}

static wmOperatorStatus clear_anim_vse_exec(bContext *C, wmOperator *op)
{
  using namespace blender::animrig;
  bool changed = false;

  Scene *scene = CTX_data_sequencer_scene(C);

  blender::Vector<PointerRNA> selection;
  blender::Vector<std::string> selected_strips_rna_paths;
  get_selection(C, &selection);
  selected_strips_rna_paths = get_selected_strips_rna_paths(selection);

  if (selected_strips_rna_paths.is_empty()) {
    BKE_reportf(op->reports, RPT_WARNING, "No strips selected");
    return OPERATOR_CANCELLED;
  }

  if (!scene->adt || !scene->adt->action || (scene->adt->slot_handle == Slot::unassigned)) {
    BKE_reportf(op->reports, RPT_ERROR, "Scene has no animation data or active action");
    return OPERATOR_CANCELLED;
  }

  AnimData *adt = scene->adt;
  bAction *dna_action = adt->action;

  Action &action = dna_action->wrap();
  blender::Vector<FCurve *> fcurves_to_delete;
  foreach_fcurve_in_action_slot(action, adt->slot_handle, [&](FCurve &fcurve) {
    for (const std::string &strip_path : selected_strips_rna_paths) {
      if (fcurve_belongs_to_strip(fcurve, strip_path)) {
        fcurves_to_delete.append(&fcurve);
        break;
      }
    }
  });
  for (FCurve *fcurve : fcurves_to_delete) {
    action_fcurve_remove(action, *fcurve);
    changed = true;
  }

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  if (scene->adt->action) {
    DEG_id_tag_update(&scene->adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
  }
  invalidate_strip_caches(selection, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  WM_event_add_notifier(C, NC_ANIMATION, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus clear_anim_vse_invoke(bContext *C,
                                              wmOperator *op,
                                              const wmEvent * /*event*/)
{
  if (RNA_boolean_get(op->ptr, "confirm")) {
    return WM_operator_confirm_ex(C,
                                  op,
                                  IFACE_("Remove animation from selected strips?"),
                                  nullptr,
                                  CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Remove"),
                                  ALERT_ICON_NONE,
                                  false);
  }
  return clear_anim_vse_exec(C, op);
}
void ANIM_OT_keyframe_clear_vse(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Animation";
  ot->description = "Remove all keyframe animation for selected strips";
  ot->idname = "ANIM_OT_keyframe_clear_vse";

  /* callbacks */
  ot->invoke = clear_anim_vse_invoke;
  ot->exec = clear_anim_vse_exec;

  ot->poll = ED_operator_sequencer_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  WM_operator_properties_confirm_or_exec(ot);
}

static bool can_delete_key(FCurve *fcu, Object *ob, ReportList *reports)
{
  /* don't touch protected F-Curves */
  if (BKE_fcurve_is_protected(fcu)) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Not deleting keyframe for locked F-Curve '%s', object '%s'",
                fcu->rna_path,
                ob->id.name + 2);
    return false;
  }

  /* Special exception for bones, as this makes this operator more convenient to use
   * NOTE: This is only done in pose mode.
   * In object mode, we're dealing with the entire object.
   * TODO: While this means bone animation is not deleted of all bones while in pose mode. Running
   * the code on the armature object WILL delete keys of all bones.
   */
  if (ob->mode & OB_MODE_POSE) {
    bPoseChannel *pchan = nullptr;

    /* Get bone-name, and check if this bone is selected. */
    char bone_name[sizeof(pchan->name)];
    if (!BLI_str_quoted_substr(fcu->rna_path, "pose.bones[", bone_name, sizeof(bone_name))) {
      return false;
    }
    pchan = BKE_pose_channel_find_name(ob->pose, bone_name);

    /* skip if bone is not selected */
    if ((pchan) && (pchan->bone)) {
      bArmature *arm = static_cast<bArmature *>(ob->data);

      /* Only selected bones should be affected. */
      if (!blender::animrig::bone_is_selected(arm, pchan)) {
        return false;
      }
    }
  }

  return true;
}

static bool can_delete_scene_key(FCurve *fcu, Scene *scene, wmOperator *op)
{
  /* Don't touch protected F-Curves. */
  if (BKE_fcurve_is_protected(fcu)) {
    BKE_reportf(op->reports,
                RPT_WARNING,
                "Not deleting keyframe for locked F-Curve '%s', scene '%s'",
                fcu->rna_path,
                scene->id.name + 2);
    return false;
  }
  return true;
}

static wmOperatorStatus delete_key_vse_without_keying_set(bContext *C, wmOperator *op)
{
  using namespace blender::animrig;
  Scene *scene = CTX_data_sequencer_scene(C);
  const float cfra = BKE_scene_frame_get(scene);

  blender::Vector<PointerRNA> selection;
  blender::Vector<std::string> selected_strips_rna_paths;
  get_selection(C, &selection);
  selected_strips_rna_paths = get_selected_strips_rna_paths(selection);

  if (selected_strips_rna_paths.is_empty()) {
    BKE_reportf(op->reports, RPT_WARNING, "No strips selected");
    return OPERATOR_CANCELLED;
  }

  const bool confirm = op->flag & OP_IS_INVOKE;
  if (!scene->adt || !scene->adt->action || (scene->adt->slot_handle == Slot::unassigned)) {
    BKE_reportf(op->reports, RPT_ERROR, "Scene has no animation data or active action");
    return OPERATOR_CANCELLED;
  }

  AnimData *adt = scene->adt;
  bAction *act = adt->action;
  Action &action = act->wrap();

  const float cfra_unmap = BKE_nla_tweakedit_remap(adt, cfra, NLATIME_CONVERT_UNMAP);

  blender::VectorSet<std::string> modified_strips;
  blender::Vector<FCurve *> modified_fcurves;

  foreach_fcurve_in_action_slot(action, adt->slot_handle, [&](FCurve &fcurve) {
    std::string changed_strip;
    for (const std::string &strip_path : selected_strips_rna_paths) {
      if (fcurve_belongs_to_strip(fcurve, strip_path)) {
        changed_strip = strip_path;
        break;
      }
    }
    if (!can_delete_scene_key(&fcurve, scene, op) || changed_strip.empty()) {
      return;
    }
    if (blender::animrig::fcurve_delete_keyframe_at_time(&fcurve, cfra_unmap)) {
      modified_fcurves.append(&fcurve);
      modified_strips.add(changed_strip);
    }
  });

  for (FCurve *fcurve : modified_fcurves) {
    if (BKE_fcurve_is_empty(fcurve)) {
      action_fcurve_remove(action, *fcurve);
    }
  }

  if (scene->adt->action) {
    /* The Action might have been unassigned, if it is legacy and the last
     * F-Curve was removed. */
    DEG_id_tag_update(&scene->adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
  }

  if (!modified_strips.is_empty()) {
    /* Key-frames on strips has been moved, so make sure related editors are informed. */
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
    WM_event_add_notifier(C, NC_ANIMATION, nullptr);
  }

  invalidate_strip_caches(selection, scene);

  if (confirm) {
    /* If called by invoke (from the UI), make a note that we've removed keyframes. */
    if (modified_strips.is_empty()) {
      const std::string msg = fmt::format(
          fmt::runtime(RPT_("No keyframes removed from {} strip(s)")),
          selected_strips_rna_paths.size());
      BKE_report(op->reports, RPT_WARNING, msg.c_str());
      return OPERATOR_CANCELLED;
    }

    const std::string msg = fmt::format(
        fmt::runtime(RPT_("{} strip(s) successfully had {} keyframes removed")),
        modified_strips.size(),
        modified_fcurves.size());
    BKE_report(op->reports, RPT_INFO, msg.c_str());
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus delete_key_vse_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  KeyingSet *ks = blender::animrig::scene_get_active_keyingset(scene);

  if (ks == nullptr) {
    return delete_key_vse_without_keying_set(C, op);
  }

  return delete_key_using_keying_set(C, op, ks);
}

static wmOperatorStatus delete_key_vse_invoke(bContext *C,
                                              wmOperator *op,
                                              const wmEvent * /*event*/)
{
  if (RNA_boolean_get(op->ptr, "confirm")) {
    return WM_operator_confirm_ex(C,
                                  op,
                                  IFACE_("Delete keyframes from selected strips?"),
                                  nullptr,
                                  IFACE_("Delete"),
                                  ALERT_ICON_NONE,
                                  false);
  }
  return delete_key_vse_exec(C, op);
}

void ANIM_OT_keyframe_delete_vse(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Keyframe";
  ot->description = "Remove keyframes on current frame for selected strips";
  ot->idname = "ANIM_OT_keyframe_delete_vse";

  /* callbacks */
  ot->invoke = delete_key_vse_invoke;
  ot->exec = delete_key_vse_exec;

  ot->poll = ED_operator_sequencer_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  WM_operator_properties_confirm_or_exec(ot);
}

static wmOperatorStatus delete_key_v3d_without_keying_set(bContext *C, wmOperator *op)
{
  using namespace blender::animrig;
  Scene *scene = CTX_data_scene(C);
  const float cfra = BKE_scene_frame_get(scene);

  int selected_objects_len = 0;
  int selected_objects_success_len = 0;
  int success_multi = 0;

  const bool confirm = op->flag & OP_IS_INVOKE;

  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    int success = 0;

    selected_objects_len += 1;

    /* just those in active action... */
    if ((ob->adt) && (ob->adt->action)) {
      AnimData *adt = ob->adt;
      bAction *act = adt->action;
      const float cfra_unmap = BKE_nla_tweakedit_remap(adt, cfra, NLATIME_CONVERT_UNMAP);

      Action &action = act->wrap();
      if (action.is_action_layered()) {
        blender::Vector<FCurve *> modified_fcurves;
        foreach_fcurve_in_action_slot(action, adt->slot_handle, [&](FCurve &fcurve) {
          if (!can_delete_key(&fcurve, ob, op->reports)) {
            return;
          }
          if (blender::animrig::fcurve_delete_keyframe_at_time(&fcurve, cfra_unmap)) {
            modified_fcurves.append(&fcurve);
          }
        });

        success += modified_fcurves.size();
        for (FCurve *fcurve : modified_fcurves) {
          if (BKE_fcurve_is_empty(fcurve)) {
            action_fcurve_remove(action, *fcurve);
          }
        }
      }
      else {
        FCurve *fcn;
        for (FCurve *fcu = static_cast<FCurve *>(act->curves.first); fcu; fcu = fcn) {
          fcn = fcu->next;
          if (!can_delete_key(fcu, ob, op->reports)) {
            continue;
          }
          /* Delete keyframes on current frame
           * WARNING: this can delete the next F-Curve, hence the "fcn" copying.
           */
          success += delete_keyframe_fcurve_legacy(adt, fcu, cfra_unmap);
        }
      }

      if (ob->adt->action) {
        /* The Action might have been unassigned, if it is legacy and the last
         * F-Curve was removed. */
        DEG_id_tag_update(&ob->adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
      }
    }

    /* Only for reporting. */
    if (success) {
      selected_objects_success_len += 1;
      success_multi += success;
    }

    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
  }
  CTX_DATA_END;

  if (selected_objects_success_len) {
    /* send updates */
    WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, nullptr);
  }

  if (confirm) {
    /* if called by invoke (from the UI), make a note that we've removed keyframes */
    if (selected_objects_success_len) {
      BKE_reportf(op->reports,
                  RPT_INFO,
                  "%d object(s) successfully had %d keyframes removed",
                  selected_objects_success_len,
                  success_multi);
    }
    else {
      BKE_reportf(
          op->reports, RPT_ERROR, "No keyframes removed from %d object(s)", selected_objects_len);
    }
  }
  return OPERATOR_FINISHED;
}

static wmOperatorStatus delete_key_v3d_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  KeyingSet *ks = blender::animrig::scene_get_active_keyingset(scene);

  if (ks == nullptr) {
    return delete_key_v3d_without_keying_set(C, op);
  }

  return delete_key_using_keying_set(C, op, ks);
}

static wmOperatorStatus delete_key_v3d_invoke(bContext *C,
                                              wmOperator *op,
                                              const wmEvent * /*event*/)
{
  if (RNA_boolean_get(op->ptr, "confirm")) {
    return WM_operator_confirm_ex(C,
                                  op,
                                  IFACE_("Delete keyframes from selected objects?"),
                                  nullptr,
                                  IFACE_("Delete"),
                                  ALERT_ICON_NONE,
                                  false);
  }
  return delete_key_v3d_exec(C, op);
}

void ANIM_OT_keyframe_delete_v3d(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Keyframe";
  ot->description = "Remove keyframes on current frame for selected objects and bones";
  ot->idname = "ANIM_OT_keyframe_delete_v3d";

  /* callbacks */
  ot->invoke = delete_key_v3d_invoke;
  ot->exec = delete_key_v3d_exec;

  ot->poll = ED_operator_areaactive;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  WM_operator_properties_confirm_or_exec(ot);
}

/* Insert Key Button Operator ------------------------ */

static wmOperatorStatus insert_key_button_exec(bContext *C, wmOperator *op)
{
  using namespace blender::animrig;
  Main *bmain = CTX_data_main(C);
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  Scene *scene = is_sequencer ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  if (!scene) {
    return OPERATOR_CANCELLED;
  }
  ToolSettings *ts = scene->toolsettings;
  PointerRNA ptr = {};
  PropertyRNA *prop = nullptr;
  uiBut *but;
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(
      CTX_data_depsgraph_pointer(C), BKE_scene_frame_get(scene));
  bool changed = false;
  int index;
  const bool all = RNA_boolean_get(op->ptr, "all");
  eInsertKeyFlags flag = INSERTKEY_NOFLAGS;

  flag = get_keyframing_flags(scene);

  if (!(but = UI_context_active_but_prop_get(C, &ptr, &prop, &index))) {
    /* pass event on if no active button found */
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }

  if ((ptr.owner_id && ptr.data && prop) && RNA_property_anim_editable(&ptr, prop)) {
    if (ptr.type == &RNA_NlaStrip) {
      /* Handle special properties for NLA Strips, whose F-Curves are stored on the
       * strips themselves. These are stored separately or else the properties will
       * not have any effect.
       */
      NlaStrip *strip = static_cast<NlaStrip *>(ptr.data);
      FCurve *fcu = BKE_fcurve_find(&strip->fcurves, RNA_property_identifier(prop), index);

      if (fcu) {
        changed = insert_keyframe_direct(op->reports,
                                         ptr,
                                         prop,
                                         fcu,
                                         &anim_eval_context,
                                         eBezTriple_KeyframeType(ts->keyframe_type),
                                         nullptr,
                                         eInsertKeyFlags(0));
      }
      else {
        BKE_report(op->reports,
                   RPT_ERROR,
                   "This property cannot be animated as it will not get updated correctly");
      }
    }
    else if (UI_but_flag_is_set(but, UI_BUT_DRIVEN)) {
      /* Driven property - Find driver */
      FCurve *fcu;
      bool driven, special;

      fcu = BKE_fcurve_find_by_rna_context_ui(
          C, &ptr, prop, index, nullptr, nullptr, &driven, &special);

      if (fcu && driven) {
        const float driver_frame = evaluate_driver_from_rna_pointer(
            &anim_eval_context, &ptr, prop, fcu);
        AnimationEvalContext remapped_context = BKE_animsys_eval_context_construct(
            CTX_data_depsgraph_pointer(C), driver_frame);
        changed = insert_keyframe_direct(op->reports,
                                         ptr,
                                         prop,
                                         fcu,
                                         &remapped_context,
                                         eBezTriple_KeyframeType(ts->keyframe_type),
                                         nullptr,
                                         INSERTKEY_NOFLAGS);
      }
    }
    else {
      /* standard properties */
      if (const std::optional<std::string> path = RNA_path_from_ID_to_property(&ptr, prop)) {
        const char *identifier = RNA_property_identifier(prop);
        const std::optional<blender::StringRefNull> group = default_channel_group_for_path(
            &ptr, identifier);

        ANIM_deselect_keys_in_animation_editors(C);

        /* NOTE: `index == -1` is a magic number, meaning either "operate on all
         * elements" or "not an array property". */
        const std::optional<int> array_index = (all || index < 0) ? std::nullopt :
                                                                    std::optional(index);
        PointerRNA owner_ptr = RNA_id_pointer_create(ptr.owner_id);
        CombinedKeyingResult result = insert_keyframes(bmain,
                                                       &owner_ptr,
                                                       group,
                                                       {{*path, {}, array_index}},
                                                       std::nullopt,
                                                       anim_eval_context,
                                                       eBezTriple_KeyframeType(ts->keyframe_type),
                                                       flag);
        changed = result.get_count(SingleKeyingResult::SUCCESS) != 0;
      }
      else {
        BKE_report(op->reports,
                   RPT_WARNING,
                   "Failed to resolve path to property, "
                   "try manually specifying this using a Keying Set instead");
      }
    }
  }
  else {
    if (prop && !RNA_property_anim_editable(&ptr, prop)) {
      BKE_reportf(op->reports,
                  RPT_WARNING,
                  "\"%s\" property cannot be animated",
                  RNA_property_identifier(prop));
    }
    else {
      BKE_reportf(op->reports,
                  RPT_WARNING,
                  "Button does not appear to have any property information attached (ptr.data = "
                  "%p, prop = %p)",
                  ptr.data,
                  (void *)prop);
    }
  }

  if (changed) {
    ID *id = ptr.owner_id;
    AnimData *adt = BKE_animdata_from_id(id);
    if (adt->action != nullptr) {
      DEG_id_tag_update(&adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
    }
    DEG_id_tag_update(id, ID_RECALC_ANIMATION_NO_FLUSH);

    /* send updates */
    UI_context_update_anim_flag(C);

    /* send notifiers that keyframes have been changed */
    WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_ADDED, nullptr);
  }

  return (changed) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void ANIM_OT_keyframe_insert_button(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Insert Keyframe (Buttons)";
  ot->idname = "ANIM_OT_keyframe_insert_button";
  ot->description = "Insert a keyframe for current UI-active property";

  /* callbacks */
  ot->exec = insert_key_button_exec;
  ot->poll = modify_key_op_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* properties */
  RNA_def_boolean(ot->srna, "all", true, "All", "Insert a keyframe for all element of the array");
}

/* Delete Key Button Operator ------------------------ */

static wmOperatorStatus delete_key_button_exec(bContext *C, wmOperator *op)
{
  const bool is_sequencer = CTX_wm_space_seq(C) != nullptr;
  Scene *scene = is_sequencer ? CTX_data_sequencer_scene(C) : CTX_data_scene(C);
  if (!scene) {
    return OPERATOR_CANCELLED;
  }
  PointerRNA ptr = {};
  PropertyRNA *prop = nullptr;
  Main *bmain = CTX_data_main(C);
  const float cfra = BKE_scene_frame_get(scene);
  bool changed = false;
  int index;
  const bool all = RNA_boolean_get(op->ptr, "all");

  if (!UI_context_active_but_prop_get(C, &ptr, &prop, &index)) {
    /* pass event on if no active button found */
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }

  if (ptr.owner_id && ptr.data && prop) {
    if (BKE_nlastrip_has_curves_for_property(&ptr, prop)) {
      /* Handle special properties for NLA Strips, whose F-Curves are stored on the
       * strips themselves. These are stored separately or else the properties will
       * not have any effect.
       */
      ID *id = ptr.owner_id;
      NlaStrip *strip = static_cast<NlaStrip *>(ptr.data);
      FCurve *fcu = BKE_fcurve_find(&strip->fcurves, RNA_property_identifier(prop), 0);

      if (fcu) {
        if (BKE_fcurve_is_protected(fcu)) {
          BKE_reportf(
              op->reports,
              RPT_WARNING,
              "Not deleting keyframe for locked F-Curve for NLA Strip influence on %s - %s '%s'",
              strip->name,
              BKE_idtype_idcode_to_name(GS(id->name)),
              id->name + 2);
        }
        else {
          /* remove the keyframe directly
           * NOTE: cannot use delete_keyframe_fcurve(), as that will free the curve,
           *       and delete_keyframe() expects the FCurve to be part of an action
           */
          bool found = false;
          int i;

          /* try to find index of beztriple to get rid of */
          i = BKE_fcurve_bezt_binarysearch_index(fcu->bezt, cfra, fcu->totvert, &found);
          if (found) {
            /* delete the key at the index (will sanity check + do recalc afterwards) */
            BKE_fcurve_delete_key(fcu, i);
            BKE_fcurve_handles_recalc(fcu);
            changed = true;
          }
        }
      }
    }
    else {
      /* standard properties */
      if (const std::optional<std::string> path = RNA_path_from_ID_to_property(&ptr, prop)) {
        RNAPath rna_path = {*path, std::nullopt, index};
        if (all) {
          /* nullopt indicates operating on the entire array (or the property itself otherwise). */
          rna_path.index = std::nullopt;
        }

        changed = blender::animrig::delete_keyframe(
                      bmain, op->reports, ptr.owner_id, rna_path, cfra) != 0;
      }
      else if (G.debug & G_DEBUG) {
        printf("Button Delete-Key: no path to property\n");
      }
    }
  }
  else if (G.debug & G_DEBUG) {
    printf("ptr.data = %p, prop = %p\n", ptr.data, (void *)prop);
  }

  if (changed) {
    /* send updates */
    UI_context_update_anim_flag(C);

    /* send notifiers that keyframes have been changed */
    WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_REMOVED, nullptr);
  }

  return (changed) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void ANIM_OT_keyframe_delete_button(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Keyframe (Buttons)";
  ot->idname = "ANIM_OT_keyframe_delete_button";
  ot->description = "Delete current keyframe of current UI-active property";

  /* callbacks */
  ot->exec = delete_key_button_exec;
  ot->poll = modify_key_op_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* properties */
  RNA_def_boolean(ot->srna, "all", true, "All", "Delete keyframes from all elements of the array");
}

/* Clear Key Button Operator ------------------------ */

static wmOperatorStatus clear_key_button_exec(bContext *C, wmOperator *op)
{
  PointerRNA ptr = {};
  PropertyRNA *prop = nullptr;
  Main *bmain = CTX_data_main(C);
  bool changed = false;
  int index;
  const bool all = RNA_boolean_get(op->ptr, "all");

  if (!UI_context_active_but_prop_get(C, &ptr, &prop, &index)) {
    /* pass event on if no active button found */
    return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
  }

  if (ptr.owner_id && ptr.data && prop) {
    if (const std::optional<std::string> path = RNA_path_from_ID_to_property(&ptr, prop)) {
      RNAPath rna_path = {*path, std::nullopt, index};
      if (all) {
        /* nullopt indicates operating on the entire array (or the property itself otherwise). */
        rna_path.index = std::nullopt;
      }

      changed |= (blender::animrig::clear_keyframe(bmain, op->reports, ptr.owner_id, rna_path) !=
                  0);
    }
    else if (G.debug & G_DEBUG) {
      printf("Button Clear-Key: no path to property\n");
    }
  }
  else if (G.debug & G_DEBUG) {
    printf("ptr.data = %p, prop = %p\n", ptr.data, (void *)prop);
  }

  if (changed) {
    /* send updates */
    UI_context_update_anim_flag(C);

    /* send notifiers that keyframes have been changed */
    WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_REMOVED, nullptr);
  }

  return (changed) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void ANIM_OT_keyframe_clear_button(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Keyframe (Buttons)";
  ot->idname = "ANIM_OT_keyframe_clear_button";
  ot->description = "Clear all keyframes on the currently active property";

  /* callbacks */
  ot->exec = clear_key_button_exec;
  ot->poll = modify_key_op_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* properties */
  RNA_def_boolean(ot->srna, "all", true, "All", "Clear keyframes from all elements of the array");
}

/* ******************************************* */
/* KEYFRAME DETECTION */

/* --------------- API/Per-Datablock Handling ------------------- */

bool fcurve_is_changed(PointerRNA ptr,
                       PropertyRNA *prop,
                       FCurve *fcu,
                       const AnimationEvalContext *anim_eval_context)
{
  PathResolvedRNA anim_rna;
  anim_rna.ptr = ptr;
  anim_rna.prop = prop;
  anim_rna.prop_index = fcu->array_index;

  int index = fcu->array_index;
  blender::Vector<float> values = blender::animrig::get_rna_values(&ptr, prop);

  float fcurve_val = calculate_fcurve(&anim_rna, fcu, anim_eval_context);
  float cur_val = (index >= 0 && index < values.size()) ? values[index] : 0.0f;

  return !compare_ff_relative(fcurve_val, cur_val, FLT_EPSILON, 64);
}

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

/** Use for insert/delete key-frame. */
static KeyingSet *keyingset_get_from_op_with_error(wmOperator *op, PropertyRNA *prop, Scene *scene)
{
  KeyingSet *ks = nullptr;
  const int prop_type = RNA_property_type(prop);
  if (prop_type == PROP_ENUM) {
    int type = RNA_property_enum_get(op->ptr, prop);
    ks = ANIM_keyingset_get_from_enum_type(scene, type);
    if (ks == nullptr) {
      BKE_report(op->reports, RPT_ERROR, "No active Keying Set");
    }
  }
  else if (prop_type == PROP_STRING) {
    char type_id[MAX_ID_NAME - 2];
    RNA_property_string_get(op->ptr, prop, type_id);

    if (STREQ(type_id, "__ACTIVE__")) {
      ks = ANIM_keyingset_get_from_enum_type(scene, scene->active_keyingset);
    }
    else {
      ks = ANIM_keyingset_get_from_idname(scene, type_id);
    }

    if (ks == nullptr) {
      BKE_reportf(op->reports, RPT_ERROR, "Keying set '%s' not found", type_id);
    }
  }
  else {
    BLI_assert(0);
  }
  return ks;
}

/** \} */
