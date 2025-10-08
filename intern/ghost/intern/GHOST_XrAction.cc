/* SPDX-FileCopyrightText: 2021-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include <cassert>
#include <cstring>

#include "GHOST_Types.h"

#include "GHOST_XrException.hh"
#include "GHOST_Xr_intern.hh"

#include "GHOST_XrAction.hh"

/* -------------------------------------------------------------------- */
/** \name GHOST_XrActionSpace
 *
 * \{ */

GHOST_XrActionSpace::GHOST_XrActionSpace(XrSession session,
                                         XrAction action,
                                         const char *action_name,
                                         const char *profile_path,
                                         XrPath subaction_path,
                                         const char *subaction_path_str,
                                         const GHOST_XrPose &pose)
{
  XrActionSpaceCreateInfo action_space_info{XR_TYPE_ACTION_SPACE_CREATE_INFO};
  action_space_info.action = action;
  action_space_info.subactionPath = subaction_path;
  copy_ghost_pose_to_openxr_pose(pose, action_space_info.poseInActionSpace);

  CHECK_XR(xrCreateActionSpace(session, &action_space_info, &space_),
           (std::string("Failed to create space \"") + subaction_path_str + "\" for action \"" +
            action_name + "\" and profile \"" + profile_path + "\".")
               .data());
}

GHOST_XrActionSpace::~GHOST_XrActionSpace()
{
  if (space_ != XR_NULL_HANDLE) {
    CHECK_XR_ASSERT(xrDestroySpace(space_));
  }
}

XrSpace GHOST_XrActionSpace::getSpace() const
{
  return space_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GHOST_XrActionProfile
 *
 * \{ */

GHOST_XrActionProfile::GHOST_XrActionProfile(XrInstance instance,
                                             XrSession session,
                                             XrAction action,
                                             GHOST_XrActionType type,
                                             const GHOST_XrActionProfileInfo &info)
{
  CHECK_XR(xrStringToPath(instance, info.profile_path, &profile_),
           (std::string("Failed to get interaction profile path \"") + info.profile_path + "\".")
               .data());

  const bool is_float_action = (type == GHOST_kXrActionTypeFloatInput ||
                                type == GHOST_kXrActionTypeVector2fInput);
  const bool is_button_action = (is_float_action || type == GHOST_kXrActionTypeBooleanInput);
  const bool is_pose_action = (type == GHOST_kXrActionTypePoseInput);

  /* Create bindings. */
  XrInteractionProfileSuggestedBinding bindings_info{
      XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
  bindings_info.interactionProfile = profile_;
  bindings_info.countSuggestedBindings = 1;

  for (uint32_t subaction_idx = 0; subaction_idx < info.count_subaction_paths; ++subaction_idx) {
    const char *subaction_path_str = info.subaction_paths[subaction_idx];
    const GHOST_XrActionBindingInfo &binding_info = info.bindings[subaction_idx];

    const std::string interaction_path = std::string(subaction_path_str) +
                                         binding_info.component_path;
    if (bindings_.find(interaction_path) != bindings_.end()) {
      continue;
    }

    XrActionSuggestedBinding sbinding;
    sbinding.action = action;
    CHECK_XR(xrStringToPath(instance, interaction_path.data(), &sbinding.binding),
             (std::string("Failed to get interaction path \"") + interaction_path + "\".").data());
    bindings_info.suggestedBindings = &sbinding;

    /* Although the bindings will be re-suggested in GHOST_XrSession::attachActionSets(), it
     * greatly improves error checking to suggest them here first. */
    CHECK_XR(xrSuggestInteractionProfileBindings(instance, &bindings_info),
             (std::string("Failed to create binding for action \"") + info.action_name +
              "\" and profile \"" + info.profile_path +
              "\". Are the action and profile paths correct?")
                 .data());

    bindings_.insert({interaction_path, sbinding.binding});

    if (subaction_data_.find(subaction_path_str) == subaction_data_.end()) {
      std::map<std::string, GHOST_XrSubactionData>::iterator it =
          subaction_data_
              .emplace(
                  std::piecewise_construct, std::make_tuple(subaction_path_str), std::make_tuple())
              .first;
      GHOST_XrSubactionData &subaction = it->second;

      CHECK_XR(xrStringToPath(instance, subaction_path_str, &subaction.subaction_path),
               (std::string("Failed to get user path \"") + subaction_path_str + "\".").data());

      if (is_float_action || is_button_action) {
        if (is_float_action) {
          subaction.float_threshold = binding_info.float_threshold;
        }
        if (is_button_action) {
          subaction.axis_flag = binding_info.axis_flag;
        }
      }
      else if (is_pose_action) {
        /* Create action space for pose bindings. */
        subaction.space = std::make_unique<GHOST_XrActionSpace>(session,
                                                                action,
                                                                info.action_name,
                                                                info.profile_path,
                                                                subaction.subaction_path,
                                                                subaction_path_str,
                                                                binding_info.pose);
      }
    }
  }
}

XrPath GHOST_XrActionProfile::getProfile() const
{
  return profile_;
}

const GHOST_XrSubactionData *GHOST_XrActionProfile::getSubaction(XrPath subaction_path) const
{
  for (auto &[subaction_path_str, subaction] : subaction_data_) {
    if (subaction.subaction_path == subaction_path) {
      return &subaction;
    }
  }
  return nullptr;
}

void GHOST_XrActionProfile::getBindings(
    XrAction action, std::map<XrPath, std::vector<XrActionSuggestedBinding>> &r_bindings) const
{
  std::map<XrPath, std::vector<XrActionSuggestedBinding>>::iterator it = r_bindings.find(profile_);
  if (it == r_bindings.end()) {
    it = r_bindings.emplace(std::piecewise_construct, std::make_tuple(profile_), std::make_tuple())
             .first;
  }

  std::vector<XrActionSuggestedBinding> &sbindings = it->second;

  for (auto &[path, binding] : bindings_) {
    XrActionSuggestedBinding sbinding;
    sbinding.action = action;
    sbinding.binding = binding;

    sbindings.push_back(std::move(sbinding));
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GHOST_XrAction
 *
 * \{ */

GHOST_XrAction::GHOST_XrAction(XrInstance instance,
                               XrActionSet action_set,
                               const GHOST_XrActionInfo &info)
    : type_(info.type),
      states_(info.states),
      float_thresholds_(info.float_thresholds),
      axis_flags_(info.axis_flags),
      custom_data_(
          std::make_unique<GHOST_C_CustomDataWrapper>(info.customdata, info.customdata_free_fn))
{
  subaction_paths_.resize(info.count_subaction_paths);

  for (uint32_t i = 0; i < info.count_subaction_paths; ++i) {
    const char *subaction_path_str = info.subaction_paths[i];
    CHECK_XR(xrStringToPath(instance, subaction_path_str, &subaction_paths_[i]),
             (std::string("Failed to get user path \"") + subaction_path_str + "\".").data());
    subaction_indices_.insert({subaction_path_str, i});
  }

  XrActionCreateInfo action_info{XR_TYPE_ACTION_CREATE_INFO};
  strcpy(action_info.actionName, info.name);

  /* Just use same name for localized. This can be changed in the future if necessary. */
  strcpy(action_info.localizedActionName, info.name);

  switch (info.type) {
    case GHOST_kXrActionTypeBooleanInput:
      action_info.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
      break;
    case GHOST_kXrActionTypeFloatInput:
      action_info.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
      break;
    case GHOST_kXrActionTypeVector2fInput:
      action_info.actionType = XR_ACTION_TYPE_VECTOR2F_INPUT;
      break;
    case GHOST_kXrActionTypePoseInput:
      action_info.actionType = XR_ACTION_TYPE_POSE_INPUT;
      break;
    case GHOST_kXrActionTypeVibrationOutput:
      action_info.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
      break;
  }
  action_info.countSubactionPaths = info.count_subaction_paths;
  action_info.subactionPaths = subaction_paths_.data();

  CHECK_XR(xrCreateAction(action_set, &action_info, &action_),
           (std::string("Failed to create action \"") + info.name +
            "\". Action name and/or paths are invalid. Name must not contain upper "
            "case letters or special characters other than '-', '_', or '.'.")
               .data());
}

GHOST_XrAction::~GHOST_XrAction()
{
  if (action_ != XR_NULL_HANDLE) {
    CHECK_XR_ASSERT(xrDestroyAction(action_));
  }
}

bool GHOST_XrAction::createBinding(XrInstance instance,
                                   XrSession session,
                                   const GHOST_XrActionProfileInfo &info)
{
  if (profiles_.find(info.profile_path) != profiles_.end()) {
    return false;
  }

  profiles_.emplace(std::piecewise_construct,
                    std::make_tuple(info.profile_path),
                    std::make_tuple(instance, session, action_, type_, info));

  return true;
}

void GHOST_XrAction::destroyBinding(const char *profile_path)
{
  /* It's possible nothing is removed. */
  profiles_.erase(profile_path);
}

void GHOST_XrAction::updateState(XrSession session,
                                 const char *action_name,
                                 XrSpace reference_space,
                                 const XrTime &predicted_display_time)
{
  const bool is_float_action = (type_ == GHOST_kXrActionTypeFloatInput ||
                                type_ == GHOST_kXrActionTypeVector2fInput);
  const bool is_button_action = (is_float_action || type_ == GHOST_kXrActionTypeBooleanInput);

  XrActionStateGetInfo state_info{XR_TYPE_ACTION_STATE_GET_INFO};
  state_info.action = action_;

  const size_t count_subaction_paths = subaction_paths_.size();
  for (size_t subaction_idx = 0; subaction_idx < count_subaction_paths; ++subaction_idx) {
    state_info.subactionPath = subaction_paths_[subaction_idx];

    /* Set subaction data based on current interaction profile. */
    XrInteractionProfileState profile_state{XR_TYPE_INTERACTION_PROFILE_STATE};
    CHECK_XR(xrGetCurrentInteractionProfile(session, state_info.subactionPath, &profile_state),
             "Failed to get current interaction profile.");

    const GHOST_XrSubactionData *subaction = nullptr;
    for (auto &[profile_path, profile] : profiles_) {
      if (profile.getProfile() == profile_state.interactionProfile) {
        subaction = profile.getSubaction(state_info.subactionPath);
        break;
      }
    }

    if (subaction != nullptr) {
      if (is_float_action) {
        float_thresholds_[subaction_idx] = subaction->float_threshold;
      }
      if (is_button_action) {
        axis_flags_[subaction_idx] = subaction->axis_flag;
      }
    }

    switch (type_) {
      case GHOST_kXrActionTypeBooleanInput: {
        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        CHECK_XR(xrGetActionStateBoolean(session, &state_info, &state),
                 (std::string("Failed to get state for boolean action \"") + action_name + "\".")
                     .data());
        if (state.isActive) {
          ((bool *)states_)[subaction_idx] = state.currentState;
        }
        break;
      }
      case GHOST_kXrActionTypeFloatInput: {
        XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
        CHECK_XR(
            xrGetActionStateFloat(session, &state_info, &state),
            (std::string("Failed to get state for float action \"") + action_name + "\".").data());
        if (state.isActive) {
          ((float *)states_)[subaction_idx] = state.currentState;
        }
        break;
      }
      case GHOST_kXrActionTypeVector2fInput: {
        XrActionStateVector2f state{XR_TYPE_ACTION_STATE_VECTOR2F};
        CHECK_XR(xrGetActionStateVector2f(session, &state_info, &state),
                 (std::string("Failed to get state for vector2f action \"") + action_name + "\".")
                     .data());
        if (state.isActive) {
          memcpy(((float (*)[2])states_)[subaction_idx], &state.currentState, sizeof(float[2]));
        }
        break;
      }
      case GHOST_kXrActionTypePoseInput: {
        /* Check for valid display time to avoid an error in #xrLocateSpace(). */
        if (predicted_display_time > 0) {
          XrActionStatePose state{XR_TYPE_ACTION_STATE_POSE};
          CHECK_XR(xrGetActionStatePose(session, &state_info, &state),
                   (std::string("Failed to get state for pose action \"") + action_name + "\".")
                       .data());
          ((GHOST_XrPose *)states_)[subaction_idx].is_active = state.isActive;
          if (state.isActive) {
            XrSpace pose_space = ((subaction != nullptr) && (subaction->space != nullptr)) ?
                                     subaction->space->getSpace() :
                                     XR_NULL_HANDLE;
            if (pose_space != XR_NULL_HANDLE) {
              XrSpaceLocation space_location{XR_TYPE_SPACE_LOCATION};
              CHECK_XR(
                  xrLocateSpace(
                      pose_space, reference_space, predicted_display_time, &space_location),
                  (std::string("Failed to query pose space for action \"") + action_name + "\".")
                      .data());
              copy_openxr_pose_to_ghost_pose(space_location.pose,
                                             ((GHOST_XrPose *)states_)[subaction_idx]);
            }
          }
        }
        break;
      }
      case GHOST_kXrActionTypeVibrationOutput: {
        break;
      }
    }
  }
}

void GHOST_XrAction::applyHapticFeedback(XrSession session,
                                         const char *action_name,
                                         const char *subaction_path_str,
                                         const int64_t &duration,
                                         const float &frequency,
                                         const float &amplitude)
{
  XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
  vibration.duration = (duration == 0) ? XR_MIN_HAPTIC_DURATION :
                                         static_cast<XrDuration>(duration);
  vibration.frequency = frequency;
  vibration.amplitude = amplitude;

  XrHapticActionInfo haptic_info{XR_TYPE_HAPTIC_ACTION_INFO};
  haptic_info.action = action_;

  if (subaction_path_str != nullptr) {
    SubactionIndexMap::iterator it = subaction_indices_.find(subaction_path_str);
    if (it != subaction_indices_.end()) {
      haptic_info.subactionPath = subaction_paths_[it->second];
      CHECK_XR(
          xrApplyHapticFeedback(session, &haptic_info, (const XrHapticBaseHeader *)&vibration),
          (std::string("Failed to apply haptic action \"") + action_name + "\".").data());
    }
  }
  else {
    for (const XrPath &subaction_path : subaction_paths_) {
      haptic_info.subactionPath = subaction_path;
      CHECK_XR(
          xrApplyHapticFeedback(session, &haptic_info, (const XrHapticBaseHeader *)&vibration),
          (std::string("Failed to apply haptic action \"") + action_name + "\".").data());
    }
  }
}

void GHOST_XrAction::stopHapticFeedback(XrSession session,
                                        const char *action_name,
                                        const char *subaction_path_str)
{
  XrHapticActionInfo haptic_info{XR_TYPE_HAPTIC_ACTION_INFO};
  haptic_info.action = action_;

  if (subaction_path_str != nullptr) {
    SubactionIndexMap::iterator it = subaction_indices_.find(subaction_path_str);
    if (it != subaction_indices_.end()) {
      haptic_info.subactionPath = subaction_paths_[it->second];
      CHECK_XR(xrStopHapticFeedback(session, &haptic_info),
               (std::string("Failed to stop haptic action \"") + action_name + "\".").data());
    }
  }
  else {
    for (const XrPath &subaction_path : subaction_paths_) {
      haptic_info.subactionPath = subaction_path;
      CHECK_XR(xrStopHapticFeedback(session, &haptic_info),
               (std::string("Failed to stop haptic action \"") + action_name + "\".").data());
    }
  }
}

void *GHOST_XrAction::getCustomdata()
{
  if (custom_data_ == nullptr) {
    return nullptr;
  }
  return custom_data_->custom_data_;
}

void GHOST_XrAction::getBindings(
    std::map<XrPath, std::vector<XrActionSuggestedBinding>> &r_bindings) const
{
  for (auto &[path, profile] : profiles_) {
    profile.getBindings(action_, r_bindings);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GHOST_XrActionSet
 *
 * \{ */

GHOST_XrActionSet::GHOST_XrActionSet(XrInstance instance, const GHOST_XrActionSetInfo &info)
    : custom_data_(
          std::make_unique<GHOST_C_CustomDataWrapper>(info.customdata, info.customdata_free_fn))
{
  XrActionSetCreateInfo action_set_info{XR_TYPE_ACTION_SET_CREATE_INFO};
  strcpy(action_set_info.actionSetName, info.name);

  /* Just use same name for localized. This can be changed in the future if necessary. */
  strcpy(action_set_info.localizedActionSetName, info.name);

  action_set_info.priority = 0; /* Use same (default) priority for all action sets. */

  CHECK_XR(xrCreateActionSet(instance, &action_set_info, &action_set_),
           (std::string("Failed to create action set \"") + info.name +
            "\". Name must not contain upper case letters or special characters "
            "other than '-', '_', or '.'.")
               .data());
}

GHOST_XrActionSet::~GHOST_XrActionSet()
{
  /* This needs to be done before xrDestroyActionSet() to avoid an assertion in the GHOST_XrAction
   * destructor (which calls xrDestroyAction()). */
  actions_.clear();

  if (action_set_ != XR_NULL_HANDLE) {
    CHECK_XR_ASSERT(xrDestroyActionSet(action_set_));
  }
}

bool GHOST_XrActionSet::createAction(XrInstance instance, const GHOST_XrActionInfo &info)
{
  if (actions_.find(info.name) != actions_.end()) {
    return false;
  }

  actions_.emplace(std::piecewise_construct,
                   std::make_tuple(info.name),
                   std::make_tuple(instance, action_set_, info));

  return true;
}

void GHOST_XrActionSet::destroyAction(const char *action_name)
{
  /* It's possible nothing is removed. */
  actions_.erase(action_name);
}

GHOST_XrAction *GHOST_XrActionSet::findAction(const char *action_name)
{
  std::map<std::string, GHOST_XrAction>::iterator it = actions_.find(action_name);
  if (it == actions_.end()) {
    return nullptr;
  }
  return &it->second;
}

void GHOST_XrActionSet::updateStates(XrSession session,
                                     XrSpace reference_space,
                                     const XrTime &predicted_display_time)
{
  for (auto &[name, action] : actions_) {
    action.updateState(session, name.data(), reference_space, predicted_display_time);
  }
}

XrActionSet GHOST_XrActionSet::getActionSet() const
{
  return action_set_;
}

void *GHOST_XrActionSet::getCustomdata()
{
  if (custom_data_ == nullptr) {
    return nullptr;
  }
  return custom_data_->custom_data_;
}

uint32_t GHOST_XrActionSet::getActionCount() const
{
  return uint32_t(actions_.size());
}

void GHOST_XrActionSet::getActionCustomdataArray(void **r_customdata_array)
{
  uint32_t i = 0;
  for (auto &[name, action] : actions_) {
    r_customdata_array[i++] = action.getCustomdata();
  }
}

void GHOST_XrActionSet::getBindings(
    std::map<XrPath, std::vector<XrActionSuggestedBinding>> &r_bindings) const
{
  for (auto &[name, action] : actions_) {
    action.getBindings(r_bindings);
  }
}

/** \} */
