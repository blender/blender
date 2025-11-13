/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.h"

#include "BKE_asset.hh"
#include "BKE_asset_edit.hh"
#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_global.hh"
#include "BKE_icons.hh"
#include "BKE_lib_id.hh"
#include "BKE_preferences.h"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "WM_api.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "ED_asset.hh"
#include "ED_asset_library.hh"
#include "ED_asset_list.hh"
#include "ED_asset_mark_clear.hh"
#include "ED_asset_menu_utils.hh"
#include "ED_asset_shelf.hh"
#include "ED_fileselect.hh"
#include "ED_screen.hh"
#include "ED_undo.hh"

#include "UI_interface_icons.hh"
#include "UI_resources.hh"

#include "BLT_translation.hh"

#include "ANIM_action.hh"
#include "ANIM_action_iterators.hh"
#include "ANIM_armature.hh"
#include "ANIM_keyframing.hh"
#include "ANIM_pose.hh"
#include "ANIM_rna.hh"

#include "AS_asset_catalog.hh"
#include "AS_asset_catalog_tree.hh"
#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "anim_intern.hh"

namespace blender::ed::animrig {

static const EnumPropertyItem *rna_asset_library_reference_itemf(bContext * /*C*/,
                                                                 PointerRNA * /*ptr*/,
                                                                 PropertyRNA * /*prop*/,
                                                                 bool *r_free)
{
  const EnumPropertyItem *items = blender::ed::asset::library_reference_to_rna_enum_itemf(false,
                                                                                          true);
  *r_free = true;
  BLI_assert(items != nullptr);
  return items;
}

static Vector<RNAPath> construct_pose_rna_paths(const PointerRNA &bone_pointer)
{
  BLI_assert(bone_pointer.type == &RNA_PoseBone);

  blender::Vector<RNAPath> paths;
  paths.append({"location"});
  paths.append({"scale"});
  bPoseChannel *pose_bone = static_cast<bPoseChannel *>(bone_pointer.data);
  switch (pose_bone->rotmode) {
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

  paths.extend({{"bbone_curveinx"},
                {"bbone_curveoutx"},
                {"bbone_curveinz"},
                {"bbone_curveoutz"},
                {"bbone_rollin"},
                {"bbone_rollout"},
                {"bbone_scalein"},
                {"bbone_scaleout"},
                {"bbone_easein"},
                {"bbone_easeout"}});

  paths.extend(blender::animrig::get_keyable_id_property_paths(bone_pointer));

  return paths;
}

static blender::animrig::Action &extract_pose(Main &bmain,
                                              const blender::Span<Object *> pose_objects)
{
  /* This currently only looks at the pose and not other things that could go onto different
   * slots on the same action. */

  using namespace blender::animrig;
  Action &action = action_add(bmain, "pose_create");
  Layer &layer = action.layer_add("pose");
  Strip &strip = layer.strip_add(action, Strip::Type::Keyframe);
  StripKeyframeData &strip_data = strip.data<StripKeyframeData>(action);
  const KeyframeSettings key_settings = {BEZT_KEYTYPE_KEYFRAME, HD_AUTO, BEZT_IPO_BEZ};

  for (Object *pose_object : pose_objects) {
    BLI_assert(pose_object->pose);
    Slot &slot = action.slot_add_for_id(pose_object->id);
    const bArmature *armature = static_cast<bArmature *>(pose_object->data);

    Set<RNAPath> existing_paths;
    if (pose_object->adt && pose_object->adt->action &&
        pose_object->adt->slot_handle != Slot::unassigned)
    {
      Action &pose_object_action = pose_object->adt->action->wrap();
      const slot_handle_t pose_object_slot = pose_object->adt->slot_handle;
      foreach_fcurve_in_action_slot(pose_object_action, pose_object_slot, [&](FCurve &fcurve) {
        RNAPath existing_path = {fcurve.rna_path, std::nullopt, fcurve.array_index};
        existing_paths.add(existing_path);
      });
    }

    LISTBASE_FOREACH (bPoseChannel *, pose_bone, &pose_object->pose->chanbase) {
      if (!blender::animrig::bone_is_selected(armature, pose_bone)) {
        continue;
      }
      PointerRNA bone_pointer = RNA_pointer_create_discrete(
          &pose_object->id, &RNA_PoseBone, pose_bone);
      Vector<RNAPath> rna_paths = construct_pose_rna_paths(bone_pointer);
      for (const RNAPath &rna_path : rna_paths) {
        PointerRNA resolved_pointer;
        PropertyRNA *resolved_property;
        if (!RNA_path_resolve(
                &bone_pointer, rna_path.path.c_str(), &resolved_pointer, &resolved_property))
        {
          continue;
        }
        const Vector<float> values = blender::animrig::get_rna_values(&resolved_pointer,
                                                                      resolved_property);
        const std::optional<std::string> rna_path_id_to_prop = RNA_path_from_ID_to_property(
            &resolved_pointer, resolved_property);
        if (!rna_path_id_to_prop.has_value()) {
          continue;
        }
        for (const int i : values.index_range()) {
          if (RNA_property_is_idprop(resolved_property) &&
              !existing_paths.contains({rna_path_id_to_prop.value(), std::nullopt, i}))
          {
            /* Skipping custom properties without animation. */
            continue;
          }
          strip_data.keyframe_insert(
              &bmain, slot, {rna_path_id_to_prop.value(), i}, {1, values[i]}, key_settings);
        }
      }
    }
  }
  return action;
}

/**
 * Check that the newly created asset is visible SOMEWHERE in Blender. If not already visible,
 * open the asset shelf on the current 3D view. The reason for not always doing that is that it
 * might be annoying in case you have 2 3D viewports open, but you want the asset shelf on only one
 * of them, or you work out of the asset browser.
 */
static void ensure_asset_ui_visible(bContext &C)
{
  ScrArea *current_area = CTX_wm_area(&C);
  if (!current_area || current_area->type->spaceid != SPACE_VIEW3D) {
    /* Opening the asset shelf will only work from the 3D viewport. */
    return;
  }

  wmWindowManager *wm = CTX_wm_manager(&C);
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    const bScreen *screen = WM_window_get_active_screen(win);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (area->type->spaceid == SPACE_FILE) {
        SpaceFile *sfile = reinterpret_cast<SpaceFile *>(area->spacedata.first);
        if (sfile->browse_mode == FILE_BROWSE_MODE_ASSETS) {
          /* Asset Browser is open. */
          return;
        }
        continue;
      }
      const ARegion *shelf_region = BKE_area_find_region_type(area, RGN_TYPE_ASSET_SHELF);
      if (!shelf_region) {
        continue;
      }
      if (shelf_region->runtime->visible) {
        /* A visible asset shelf was found. */
        return;
      }
    }
  }

  /* At this point, no asset shelf or asset browser was visible anywhere. */
  ARegion *shelf_region = BKE_area_find_region_type(current_area, RGN_TYPE_ASSET_SHELF);
  if (!shelf_region) {
    return;
  }
  shelf_region->flag &= ~RGN_FLAG_HIDDEN;
  ED_region_visibility_change_update(&C, CTX_wm_area(&C), shelf_region);
}

static blender::Vector<Object *> get_selected_pose_objects(bContext *C)
{
  blender::Vector<PointerRNA> selected_objects;
  CTX_data_selected_objects(C, &selected_objects);

  blender::Vector<Object *> selected_pose_objects;
  for (const PointerRNA &ptr : selected_objects) {
    Object *object = reinterpret_cast<Object *>(ptr.owner_id);
    if (!object->pose) {
      continue;
    }
    selected_pose_objects.append(object);
  }

  Object *active_object = CTX_data_active_object(C);
  /* The active object may not be selected, it should be added because you can still switch to pose
   * mode. */
  if (active_object && active_object->pose && !selected_pose_objects.contains(active_object)) {
    selected_pose_objects.append(active_object);
  }
  return selected_pose_objects;
}

static wmOperatorStatus create_pose_asset_local(bContext *C,
                                                wmOperator *op,
                                                const StringRefNull name,
                                                const AssetLibraryReference lib_ref)
{
  blender::Vector<Object *> selected_pose_objects = get_selected_pose_objects(C);

  if (selected_pose_objects.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  Main *bmain = CTX_data_main(C);
  /* Extract the pose into a new action. */
  blender::animrig::Action &pose_action = extract_pose(*bmain, selected_pose_objects);
  asset::mark_id(&pose_action.id);
  if (!G.background) {
    asset::generate_preview(C, &pose_action.id);
  }
  BKE_id_rename(*bmain, pose_action.id, name);

  /* Add asset to catalog. */
  char catalog_path_c[MAX_NAME];
  RNA_string_get(op->ptr, "catalog_path", catalog_path_c);

  AssetMetaData &meta_data = *pose_action.id.asset_data;
  asset_system::AssetLibrary *library = AS_asset_library_load(bmain, lib_ref);
  /* NOTE(@ChrisLend): I don't know if a local library can fail to load.
   * Just being defensive here. */
  BLI_assert(library);
  if (catalog_path_c[0] && library) {
    const asset_system::AssetCatalogPath catalog_path(catalog_path_c);
    asset_system::AssetCatalog &catalog = asset::library_ensure_catalogs_in_path(*library,
                                                                                 catalog_path);
    BKE_asset_metadata_catalog_id_set(&meta_data, catalog.catalog_id, catalog.simple_name.c_str());
  }

  ensure_asset_ui_visible(*C);
  asset::shelf::show_catalog_in_visible_shelves(*C, catalog_path_c);

  asset::refresh_asset_library(C, lib_ref);

  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST | NA_ADDED, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus create_pose_asset_user_library(bContext *C,
                                                       wmOperator *op,
                                                       const char name[MAX_NAME],
                                                       const AssetLibraryReference lib_ref)
{
  BLI_assert(lib_ref.type == ASSET_LIBRARY_CUSTOM);
  Main *bmain = CTX_data_main(C);

  const bUserAssetLibrary *user_library = BKE_preferences_asset_library_find_index(
      &U, lib_ref.custom_library_index);
  BLI_assert_msg(user_library, "The passed lib_ref is expected to be a user library");
  if (!user_library) {
    return OPERATOR_CANCELLED;
  }

  asset_system::AssetLibrary *library = AS_asset_library_load(bmain, lib_ref);
  if (!library) {
    BKE_report(op->reports, RPT_ERROR, "Failed to load asset library");
    return OPERATOR_CANCELLED;
  }

  blender::Vector<Object *> selected_pose_objects = get_selected_pose_objects(C);

  if (selected_pose_objects.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  /* Temporary action in current main that will be exported and later deleted. */
  blender::animrig::Action &pose_action = extract_pose(*bmain, selected_pose_objects);
  asset::mark_id(&pose_action.id);
  if (!G.background) {
    asset::generate_preview(C, &pose_action.id);
  }

  /* Add asset to catalog. */
  char catalog_path_c[MAX_NAME];
  RNA_string_get(op->ptr, "catalog_path", catalog_path_c);

  AssetMetaData &meta_data = *pose_action.id.asset_data;
  if (catalog_path_c[0]) {
    const asset_system::AssetCatalogPath catalog_path(catalog_path_c);
    const asset_system::AssetCatalog &catalog = asset::library_ensure_catalogs_in_path(
        *library, catalog_path);
    BKE_asset_metadata_catalog_id_set(&meta_data, catalog.catalog_id, catalog.simple_name.c_str());
  }

  AssetWeakReference pose_asset_reference;
  const std::optional<std::string> final_full_asset_filepath = bke::asset_edit_id_save_as(
      *bmain, pose_action.id, name, *user_library, pose_asset_reference, *op->reports);

  library->catalog_service().write_to_disk(*final_full_asset_filepath);
  ensure_asset_ui_visible(*C);
  asset::shelf::show_catalog_in_visible_shelves(*C, catalog_path_c);

  BKE_id_free(bmain, &pose_action.id);

  asset::refresh_asset_library(C, lib_ref);

  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST | NA_ADDED, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus pose_asset_create_exec(bContext *C, wmOperator *op)
{
  char name[MAX_NAME] = "";
  PropertyRNA *name_prop = RNA_struct_find_property(op->ptr, "pose_name");
  if (RNA_property_is_set(op->ptr, name_prop)) {
    RNA_property_string_get(op->ptr, name_prop, name);
  }
  if (name[0] == '\0') {
    BKE_report(op->reports, RPT_ERROR, "No name set");
    return OPERATOR_CANCELLED;
  }

  const int enum_value = RNA_enum_get(op->ptr, "asset_library_reference");
  const AssetLibraryReference lib_ref = asset::library_reference_from_enum_value(enum_value);

  switch (lib_ref.type) {
    case ASSET_LIBRARY_LOCAL:
      return create_pose_asset_local(C, op, name, lib_ref);

    case ASSET_LIBRARY_CUSTOM:
      return create_pose_asset_user_library(C, op, name, lib_ref);

    default:
      /* Only local and custom libraries should be exposed in the enum. */
      BLI_assert_unreachable();
      break;
  }

  BKE_report(op->reports, RPT_ERROR, "Unexpected library type. Failed to create pose asset");

  return OPERATOR_FINISHED;
}

static wmOperatorStatus pose_asset_create_invoke(bContext *C,
                                                 wmOperator *op,
                                                 const wmEvent * /*event*/)
{
  /* If the library isn't saved from the operator's last execution, use the first library. */
  if (!RNA_struct_property_is_set_ex(op->ptr, "asset_library_reference", false)) {
    const AssetLibraryReference first_library = asset::user_library_to_library_ref(
        *static_cast<const bUserAssetLibrary *>(U.asset_libraries.first));
    RNA_enum_set(op->ptr,
                 "asset_library_reference",
                 asset::library_reference_to_enum_value(&first_library));
  }

  return WM_operator_props_dialog_popup(C, op, 400, std::nullopt, IFACE_("Create"));
}

static bool pose_asset_create_poll(bContext *C)
{
  if (!ED_operator_posemode_context(C)) {
    return false;
  }
  return true;
}

static void visit_library_prop_catalogs_catalog_for_search_fn(
    const bContext *C,
    PointerRNA *ptr,
    PropertyRNA * /*prop*/,
    const char *edit_text,
    FunctionRef<void(StringPropertySearchVisitParams)> visit_fn)
{
  const int enum_value = RNA_enum_get(ptr, "asset_library_reference");
  const AssetLibraryReference lib_ref = asset::library_reference_from_enum_value(enum_value);

  asset::visit_library_catalogs_catalog_for_search(
      *CTX_data_main(C), lib_ref, edit_text, visit_fn);
}

void POSELIB_OT_create_pose_asset(wmOperatorType *ot)
{
  ot->name = "Create Pose Asset...";
  ot->description = "Create a new asset from the selected bones in the scene";
  ot->idname = "POSELIB_OT_create_pose_asset";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->exec = pose_asset_create_exec;
  ot->invoke = pose_asset_create_invoke;
  ot->poll = pose_asset_create_poll;

  ot->prop = RNA_def_string(
      ot->srna, "pose_name", nullptr, MAX_NAME, "Pose Name", "Name for the new pose asset");

  PropertyRNA *prop = RNA_def_property(ot->srna, "asset_library_reference", PROP_ENUM, PROP_NONE);
  RNA_def_enum_funcs(prop, rna_asset_library_reference_itemf);
  RNA_def_property_ui_text(prop, "Library", "Asset library used to store the new pose");

  prop = RNA_def_string(
      ot->srna, "catalog_path", nullptr, MAX_NAME, "Catalog", "Catalog to use for the new asset");
  RNA_def_property_string_search_func_runtime(
      prop, visit_library_prop_catalogs_catalog_for_search_fn, PROP_STRING_SEARCH_SUGGESTION);
}

enum AssetModifyMode {
  MODIFY_ADJUST = 0,
  MODIFY_REPLACE,
  MODIFY_ADD,
  MODIFY_REMOVE,
};

static const EnumPropertyItem prop_asset_overwrite_modes[] = {
    {MODIFY_ADJUST,
     "ADJUST",
     0,
     "Adjust",
     "Update existing channels in the pose asset but don't remove or add any channels"},
    {MODIFY_REPLACE,
     "REPLACE",
     0,
     "Replace with Selection",
     "Completely replace all channels in the pose asset with the current selection"},
    {MODIFY_ADD,
     "ADD",
     0,
     "Add Selected Bones",
     "Add channels of the selection to the pose asset. Existing channels will be updated"},
    {MODIFY_REMOVE,
     "REMOVE",
     0,
     "Remove Selected Bones",
     "Remove channels of the selection from the pose asset"},
    {0, nullptr, 0, nullptr, nullptr},
};

/* Gets the selected asset from the given `bContext`. If the asset is an action, returns a pointer
 * to that action, else returns a nullptr. */
static bAction *get_action_of_selected_asset(bContext *C)
{
  const blender::asset_system::AssetRepresentation *asset = CTX_wm_asset(C);
  if (!asset) {
    return nullptr;
  }

  if (asset->get_id_type() != ID_AC) {
    return nullptr;
  }

  AssetWeakReference asset_reference = asset->make_weak_reference();
  Main *bmain = CTX_data_main(C);
  return reinterpret_cast<bAction *>(
      bke::asset_edit_id_from_weak_reference(*bmain, ID_AC, asset_reference));
}

struct PathValue {
  RNAPath rna_path;
  float value;
};

static Vector<PathValue> generate_path_values(Object &pose_object)
{
  Vector<PathValue> path_values;
  const bArmature *armature = static_cast<bArmature *>(pose_object.data);
  LISTBASE_FOREACH (bPoseChannel *, pose_bone, &pose_object.pose->chanbase) {
    if (!blender::animrig::bone_is_selected(armature, pose_bone)) {
      continue;
    }
    PointerRNA bone_pointer = RNA_pointer_create_discrete(
        &pose_object.id, &RNA_PoseBone, pose_bone);
    Vector<RNAPath> rna_paths = construct_pose_rna_paths(bone_pointer);

    for (RNAPath &rna_path : rna_paths) {
      PointerRNA resolved_pointer;
      PropertyRNA *resolved_property;
      if (!RNA_path_resolve(
              &bone_pointer, rna_path.path.c_str(), &resolved_pointer, &resolved_property))
      {
        continue;
      }
      const std::optional<std::string> rna_path_id_to_prop = RNA_path_from_ID_to_property(
          &resolved_pointer, resolved_property);
      if (!rna_path_id_to_prop.has_value()) {
        continue;
      }
      Vector<float> values = blender::animrig::get_rna_values(&resolved_pointer,
                                                              resolved_property);
      int i = 0;
      for (const float value : values) {
        RNAPath path = {rna_path_id_to_prop.value(), std::nullopt, i};
        path_values.append({path, value});
        i++;
      }
    }
  }
  return path_values;
}

static inline void replace_pose_key(Main &bmain,
                                    blender::animrig::StripKeyframeData &strip_data,
                                    const blender::animrig::Slot &slot,
                                    const float2 time_value,
                                    const blender::animrig::FCurveDescriptor &fcurve_descriptor)
{
  using namespace blender::animrig;
  Channelbag &channelbag = strip_data.channelbag_for_slot_ensure(slot);
  FCurve &fcurve = channelbag.fcurve_ensure(&bmain, fcurve_descriptor);

  /* Clearing all keys beforehand in case the pose was not defined on frame defined in
   * `time_value`. */
  BKE_fcurve_delete_keys_all(&fcurve);
  const KeyframeSettings key_settings = {BEZT_KEYTYPE_KEYFRAME, HD_AUTO, BEZT_IPO_BEZ};
  insert_vert_fcurve(&fcurve, time_value, key_settings, INSERTKEY_NOFLAGS);
}

static void update_pose_action_from_scene(Main *bmain,
                                          blender::animrig::Action &pose_action,
                                          Object &pose_object,
                                          const AssetModifyMode mode)
{
  using namespace blender::animrig;
  /* The frame on which an FCurve has a key to define a pose. */
  constexpr int pose_frame = 1;
  if (pose_action.slot_array_num < 1) {
    /* All actions should have slots at this point. */
    BLI_assert_unreachable();
    return;
  }

  Slot &slot = blender::animrig::get_best_pose_slot_for_id(pose_object.id, pose_action);
  BLI_assert(pose_action.strip_keyframe_data().size() == 1);
  BLI_assert(pose_action.layers().size() == 1);
  StripKeyframeData *strip_data = pose_action.strip_keyframe_data()[0];
  Vector<PathValue> path_values = generate_path_values(pose_object);

  Set<RNAPath> existing_paths;
  foreach_fcurve_in_action_slot(pose_action, slot.handle, [&](FCurve &fcurve) {
    existing_paths.add({fcurve.rna_path, std::nullopt, fcurve.array_index});
  });

  switch (mode) {
    case MODIFY_ADJUST: {
      for (const PathValue &path_value : path_values) {
        /* Only updating existing channels. */
        if (existing_paths.contains(path_value.rna_path)) {
          replace_pose_key(*bmain,
                           *strip_data,
                           slot,
                           {pose_frame, path_value.value},
                           {path_value.rna_path.path, path_value.rna_path.index.value()});
        }
      }
      break;
    }
    case MODIFY_ADD: {
      for (const PathValue &path_value : path_values) {
        replace_pose_key(*bmain,
                         *strip_data,
                         slot,
                         {pose_frame, path_value.value},
                         {path_value.rna_path.path, path_value.rna_path.index.value()});
      }
      break;
    }
    case MODIFY_REPLACE: {
      Channelbag *channelbag = strip_data->channelbag_for_slot(slot.handle);
      if (!channelbag) {
        /* No channels to remove. */
        return;
      }
      channelbag->fcurves_clear();
      for (const PathValue &path_value : path_values) {
        replace_pose_key(*bmain,
                         *strip_data,
                         slot,
                         {pose_frame, path_value.value},
                         {path_value.rna_path.path, path_value.rna_path.index.value()});
      }
      break;
    }
    case MODIFY_REMOVE: {
      Channelbag *channelbag = strip_data->channelbag_for_slot(slot.handle);
      if (!channelbag) {
        /* No channels to remove. */
        return;
      }
      Map<RNAPath, FCurve *> fcurve_map;
      foreach_fcurve_in_action_slot(
          pose_action, pose_action.slot_array[0]->handle, [&](FCurve &fcurve) {
            fcurve_map.add({fcurve.rna_path, std::nullopt, fcurve.array_index}, &fcurve);
          });
      for (const PathValue &path_value : path_values) {
        if (existing_paths.contains(path_value.rna_path)) {
          FCurve *fcurve = fcurve_map.lookup(path_value.rna_path);
          channelbag->fcurve_remove(*fcurve);
        }
      }
      break;
    }
  }
}

static wmOperatorStatus pose_asset_modify_exec(bContext *C, wmOperator *op)
{
  bAction *action = get_action_of_selected_asset(C);
  BLI_assert_msg(action, "Poll should have checked action exists");
  /* Get asset now. Asset browser might get tagged for refreshing through operations below, and not
   * allow querying items from context until refreshed, see #140781. */
  const asset_system::AssetRepresentation *asset = CTX_wm_asset(C);

  Main *bmain = CTX_data_main(C);
  Object *pose_object = CTX_data_active_object(C);
  if (!pose_object || !pose_object->pose) {
    return OPERATOR_CANCELLED;
  }

  AssetModifyMode mode = AssetModifyMode(RNA_enum_get(op->ptr, "mode"));
  update_pose_action_from_scene(bmain, action->wrap(), *pose_object, mode);
  if (!G.background) {
    asset::generate_preview(C, &action->id);
  }
  if (ID_IS_LINKED(action)) {
    /* Not needed for local assets. */
    bke::asset_edit_id_save(*bmain, action->id, *op->reports);
  }
  else {
    /* Only create undo-step for local actions. Undoing external files isn't supported. */
    ED_undo_push_op(C, op);
  }

  asset::refresh_asset_library_from_asset(C, *asset);
  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static bool pose_asset_modify_poll(bContext *C)
{
  if (!ED_operator_posemode_context(C)) {
    CTX_wm_operator_poll_msg_set(C, "Pose assets can only be modified from Pose Mode");
    return false;
  }

  bAction *action = get_action_of_selected_asset(C);

  if (!action) {
    return false;
  }

  if (!ID_IS_LINKED(action)) {
    return true;
  }

  if (!bke::asset_edit_id_is_editable(action->id)) {
    CTX_wm_operator_poll_msg_set(C, "Action is not editable");
    return false;
  }

  if (!bke::asset_edit_id_is_writable(action->id)) {
    CTX_wm_operator_poll_msg_set(C, "Asset blend file is not editable");
    return false;
  }

  return true;
}

static std::string pose_asset_modify_description(bContext * /* C */,
                                                 wmOperatorType * /* ot */,
                                                 PointerRNA *ptr)
{
  const int mode = RNA_enum_get(ptr, "mode");
  return TIP_(std::string(prop_asset_overwrite_modes[mode].description));
}

/* Calling it overwrite instead of save because we aren't actually saving an opened asset. */
void POSELIB_OT_asset_modify(wmOperatorType *ot)
{
  ot->name = "Modify Pose Asset";
  ot->description =
      "Update the selected pose asset in the asset library from the currently selected bones. The "
      "mode defines how the asset is updated";
  ot->idname = "POSELIB_OT_asset_modify";

  ot->exec = pose_asset_modify_exec;
  ot->poll = pose_asset_modify_poll;
  ot->get_description = pose_asset_modify_description;

  RNA_def_enum(ot->srna,
               "mode",
               prop_asset_overwrite_modes,
               MODIFY_ADJUST,
               "Overwrite Mode",
               "Specify which parts of the pose asset are overwritten");
}

static bool pose_asset_delete_poll(bContext *C)
{
  bAction *action = get_action_of_selected_asset(C);

  if (!action) {
    return false;
  }

  if (!ID_IS_LINKED(action)) {
    return true;
  }

  if (!bke::asset_edit_id_is_editable(action->id)) {
    CTX_wm_operator_poll_msg_set(C, "Action is not editable");
    return false;
  }

  if (!bke::asset_edit_id_is_writable(action->id)) {
    CTX_wm_operator_poll_msg_set(C, "Asset blend file is not editable");
    return false;
  }

  return true;
}

static wmOperatorStatus pose_asset_delete_exec(bContext *C, wmOperator *op)
{
  bAction *action = get_action_of_selected_asset(C);
  if (!action) {
    return OPERATOR_CANCELLED;
  }

  const blender::asset_system::AssetRepresentation *asset = CTX_wm_asset(C);
  std::optional<AssetLibraryReference> library_ref =
      asset->owner_asset_library().library_reference();

  if (ID_IS_LINKED(action)) {
    bke::asset_edit_id_delete(*CTX_data_main(C), action->id, *op->reports);
  }
  else {
    asset::clear_id(&action->id);
    /* Only create undo-step for local actions. Undoing external files isn't supported. */
    ED_undo_push_op(C, op);
  }

  asset::refresh_asset_library(C, library_ref.value());

  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST | NA_REMOVED, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus pose_asset_delete_invoke(bContext *C,
                                                 wmOperator *op,
                                                 const wmEvent * /*event*/)
{
  bAction *action = get_action_of_selected_asset(C);

  return WM_operator_confirm_ex(
      C,
      op,
      IFACE_("Delete Pose Asset"),
      ID_IS_LINKED(action) ?
          IFACE_("Permanently delete pose asset blend file? This cannot be undone.") :
          IFACE_("The asset is local to the file. Deleting it will just clear the asset status."),
      IFACE_("Delete"),
      ALERT_ICON_WARNING,
      false);
}

void POSELIB_OT_asset_delete(wmOperatorType *ot)
{
  ot->name = "Delete Pose Asset";
  ot->description = "Delete the selected Pose Asset";
  ot->idname = "POSELIB_OT_asset_delete";

  ot->poll = pose_asset_delete_poll;
  ot->invoke = pose_asset_delete_invoke;
  ot->exec = pose_asset_delete_exec;
}

}  // namespace blender::ed::animrig
