/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup GHOST
 */

#include <cassert>
#include <cstring>

#include "GHOST_Types.h"

#include "GHOST_XrException.h"
#include "GHOST_Xr_intern.h"

#include "GHOST_XrAction.h"

/* -------------------------------------------------------------------- */
/** \name GHOST_XrActionSpace
 *
 * \{ */

GHOST_XrActionSpace::GHOST_XrActionSpace(XrInstance instance,
                                         XrSession session,
                                         XrAction action,
                                         const GHOST_XrActionSpaceInfo &info,
                                         uint32_t subaction_idx)
{
  const char *subaction_path = info.subaction_paths[subaction_idx];
  CHECK_XR(xrStringToPath(instance, subaction_path, &m_subaction_path),
           (std::string("Failed to get user path \"") + subaction_path + "\".").data());

  XrActionSpaceCreateInfo action_space_info{XR_TYPE_ACTION_SPACE_CREATE_INFO};
  action_space_info.action = action;
  action_space_info.subactionPath = m_subaction_path;
  copy_ghost_pose_to_openxr_pose(info.poses[subaction_idx], action_space_info.poseInActionSpace);

  CHECK_XR(xrCreateActionSpace(session, &action_space_info, &m_space),
           (std::string("Failed to create space \"") + subaction_path + "\" for action \"" +
            info.action_name + "\".")
               .data());
}

GHOST_XrActionSpace::~GHOST_XrActionSpace()
{
  if (m_space != XR_NULL_HANDLE) {
    CHECK_XR_ASSERT(xrDestroySpace(m_space));
  }
}

XrSpace GHOST_XrActionSpace::getSpace() const
{
  return m_space;
}

const XrPath &GHOST_XrActionSpace::getSubactionPath() const
{
  return m_subaction_path;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GHOST_XrActionProfile
 *
 * \{ */

GHOST_XrActionProfile::GHOST_XrActionProfile(XrInstance instance,
                                             XrAction action,
                                             const char *profile_path,
                                             const GHOST_XrActionBindingInfo &info)
{
  CHECK_XR(
      xrStringToPath(instance, profile_path, &m_profile),
      (std::string("Failed to get interaction profile path \"") + profile_path + "\".").data());

  /* Create bindings. */
  XrInteractionProfileSuggestedBinding bindings_info{
      XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
  bindings_info.interactionProfile = m_profile;
  bindings_info.countSuggestedBindings = 1;

  for (uint32_t interaction_idx = 0; interaction_idx < info.count_interaction_paths;
       ++interaction_idx) {
    const char *interaction_path = info.interaction_paths[interaction_idx];
    if (m_bindings.find(interaction_path) != m_bindings.end()) {
      continue;
    }

    XrActionSuggestedBinding sbinding;
    sbinding.action = action;
    CHECK_XR(xrStringToPath(instance, interaction_path, &sbinding.binding),
             (std::string("Failed to get interaction path \"") + interaction_path + "\".").data());
    bindings_info.suggestedBindings = &sbinding;

    /* Although the bindings will be re-suggested in GHOST_XrSession::attachActionSets(), it
     * greatly improves error checking to suggest them here first. */
    CHECK_XR(xrSuggestInteractionProfileBindings(instance, &bindings_info),
             (std::string("Failed to create binding for profile \"") + profile_path +
              "\" and action \"" + info.action_name +
              "\". Are the profile and action paths correct?")
                 .data());

    m_bindings.insert({interaction_path, sbinding.binding});
  }
}

void GHOST_XrActionProfile::getBindings(
    XrAction action, std::map<XrPath, std::vector<XrActionSuggestedBinding>> &r_bindings) const
{
  std::map<XrPath, std::vector<XrActionSuggestedBinding>>::iterator it = r_bindings.find(
      m_profile);
  if (it == r_bindings.end()) {
    it = r_bindings
             .emplace(std::piecewise_construct, std::make_tuple(m_profile), std::make_tuple())
             .first;
  }

  std::vector<XrActionSuggestedBinding> &sbindings = it->second;

  for (auto &[path, binding] : m_bindings) {
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
    : m_type(info.type),
      m_states(info.states),
      m_custom_data_(
          std::make_unique<GHOST_C_CustomDataWrapper>(info.customdata, info.customdata_free_fn))
{
  m_subaction_paths.resize(info.count_subaction_paths);

  for (uint32_t i = 0; i < info.count_subaction_paths; ++i) {
    CHECK_XR(xrStringToPath(instance, info.subaction_paths[i], &m_subaction_paths[i]),
             (std::string("Failed to get user path \"") + info.subaction_paths[i] + "\".").data());
  }

  XrActionCreateInfo action_info{XR_TYPE_ACTION_CREATE_INFO};
  strcpy(action_info.actionName, info.name);
  strcpy(action_info.localizedActionName, info.name); /* Just use same name for localized. This can
                                                         be changed in the future if necessary. */

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
  action_info.subactionPaths = m_subaction_paths.data();

  CHECK_XR(xrCreateAction(action_set, &action_info, &m_action),
           (std::string("Failed to create action \"") + info.name +
            "\". Action name and/or paths are invalid. Name must not contain upper "
            "case letters or special characters other than '-', '_', or '.'.")
               .data());
}

GHOST_XrAction::~GHOST_XrAction()
{
  if (m_action != XR_NULL_HANDLE) {
    CHECK_XR_ASSERT(xrDestroyAction(m_action));
  }
}

bool GHOST_XrAction::createSpace(XrInstance instance,
                                 XrSession session,
                                 const GHOST_XrActionSpaceInfo &info)
{
  uint32_t subaction_idx = 0;
  for (; subaction_idx < info.count_subaction_paths; ++subaction_idx) {
    if (m_spaces.find(info.subaction_paths[subaction_idx]) != m_spaces.end()) {
      return false;
    }
  }

  for (subaction_idx = 0; subaction_idx < info.count_subaction_paths; ++subaction_idx) {
    m_spaces.emplace(std::piecewise_construct,
                     std::make_tuple(info.subaction_paths[subaction_idx]),
                     std::make_tuple(instance, session, m_action, info, subaction_idx));
  }

  return true;
}

void GHOST_XrAction::destroySpace(const char *subaction_path)
{
  if (m_spaces.find(subaction_path) != m_spaces.end()) {
    m_spaces.erase(subaction_path);
  }
}

bool GHOST_XrAction::createBinding(XrInstance instance,
                                   const char *profile_path,
                                   const GHOST_XrActionBindingInfo &info)
{
  if (m_profiles.find(profile_path) != m_profiles.end()) {
    return false;
  }

  m_profiles.emplace(std::piecewise_construct,
                     std::make_tuple(profile_path),
                     std::make_tuple(instance, m_action, profile_path, info));

  return true;
}

void GHOST_XrAction::destroyBinding(const char *interaction_profile_path)
{
  if (m_profiles.find(interaction_profile_path) != m_profiles.end()) {
    m_profiles.erase(interaction_profile_path);
  }
}

void GHOST_XrAction::updateState(XrSession session,
                                 const char *action_name,
                                 XrSpace reference_space,
                                 const XrTime &predicted_display_time)
{
  XrActionStateGetInfo state_info{XR_TYPE_ACTION_STATE_GET_INFO};
  state_info.action = m_action;

  const size_t count_subaction_paths = m_subaction_paths.size();
  for (size_t subaction_idx = 0; subaction_idx < count_subaction_paths; ++subaction_idx) {
    state_info.subactionPath = m_subaction_paths[subaction_idx];

    switch (m_type) {
      case GHOST_kXrActionTypeBooleanInput: {
        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        CHECK_XR(xrGetActionStateBoolean(session, &state_info, &state),
                 (std::string("Failed to get state for boolean action \"") + action_name + "\".")
                     .data());
        if (state.isActive) {
          ((bool *)m_states)[subaction_idx] = state.currentState;
        }
        break;
      }
      case GHOST_kXrActionTypeFloatInput: {
        XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
        CHECK_XR(
            xrGetActionStateFloat(session, &state_info, &state),
            (std::string("Failed to get state for float action \"") + action_name + "\".").data());
        if (state.isActive) {
          ((float *)m_states)[subaction_idx] = state.currentState;
        }
        break;
      }
      case GHOST_kXrActionTypeVector2fInput: {
        XrActionStateVector2f state{XR_TYPE_ACTION_STATE_VECTOR2F};
        CHECK_XR(xrGetActionStateVector2f(session, &state_info, &state),
                 (std::string("Failed to get state for vector2f action \"") + action_name + "\".")
                     .data());
        if (state.isActive) {
          memcpy(((float(*)[2])m_states)[subaction_idx], &state.currentState, sizeof(float[2]));
        }
        break;
      }
      case GHOST_kXrActionTypePoseInput: {
        XrActionStatePose state{XR_TYPE_ACTION_STATE_POSE};
        CHECK_XR(
            xrGetActionStatePose(session, &state_info, &state),
            (std::string("Failed to get state for pose action \"") + action_name + "\".").data());
        if (state.isActive) {
          XrSpace pose_space = XR_NULL_HANDLE;
          for (auto &[path, space] : m_spaces) {
            if (space.getSubactionPath() == state_info.subactionPath) {
              pose_space = space.getSpace();
              break;
            }
          }

          if (pose_space != XR_NULL_HANDLE) {
            XrSpaceLocation space_location{XR_TYPE_SPACE_LOCATION};
            CHECK_XR(
                xrLocateSpace(
                    pose_space, reference_space, predicted_display_time, &space_location),
                (std::string("Failed to query pose space for action \"") + action_name + "\".")
                    .data());
            copy_openxr_pose_to_ghost_pose(space_location.pose,
                                           ((GHOST_XrPose *)m_states)[subaction_idx]);
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
                                         const GHOST_TInt64 &duration,
                                         const float &frequency,
                                         const float &amplitude)
{
  XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
  vibration.duration = (duration == 0) ? XR_MIN_HAPTIC_DURATION :
                                         static_cast<XrDuration>(duration);
  vibration.frequency = frequency;
  vibration.amplitude = amplitude;

  XrHapticActionInfo haptic_info{XR_TYPE_HAPTIC_ACTION_INFO};
  haptic_info.action = m_action;

  for (std::vector<XrPath>::iterator it = m_subaction_paths.begin(); it != m_subaction_paths.end();
       ++it) {
    haptic_info.subactionPath = *it;
    CHECK_XR(xrApplyHapticFeedback(session, &haptic_info, (const XrHapticBaseHeader *)&vibration),
             (std::string("Failed to apply haptic action \"") + action_name + "\".").data());
  }
}

void GHOST_XrAction::stopHapticFeedback(XrSession session, const char *action_name)
{
  XrHapticActionInfo haptic_info{XR_TYPE_HAPTIC_ACTION_INFO};
  haptic_info.action = m_action;

  for (std::vector<XrPath>::iterator it = m_subaction_paths.begin(); it != m_subaction_paths.end();
       ++it) {
    haptic_info.subactionPath = *it;
    CHECK_XR(xrStopHapticFeedback(session, &haptic_info),
             (std::string("Failed to stop haptic action \"") + action_name + "\".").data());
  }
}

void *GHOST_XrAction::getCustomdata()
{
  if (m_custom_data_ == nullptr) {
    return nullptr;
  }
  return m_custom_data_->custom_data_;
}

void GHOST_XrAction::getBindings(
    std::map<XrPath, std::vector<XrActionSuggestedBinding>> &r_bindings) const
{
  for (auto &[path, profile] : m_profiles) {
    profile.getBindings(m_action, r_bindings);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GHOST_XrActionSet
 *
 * \{ */

GHOST_XrActionSet::GHOST_XrActionSet(XrInstance instance, const GHOST_XrActionSetInfo &info)
    : m_custom_data_(
          std::make_unique<GHOST_C_CustomDataWrapper>(info.customdata, info.customdata_free_fn))
{
  XrActionSetCreateInfo action_set_info{XR_TYPE_ACTION_SET_CREATE_INFO};
  strcpy(action_set_info.actionSetName, info.name);
  strcpy(action_set_info.localizedActionSetName,
         info.name); /* Just use same name for localized. This can be changed in the future if
                        necessary. */
  action_set_info.priority = 0; /* Use same (default) priority for all action sets. */

  CHECK_XR(xrCreateActionSet(instance, &action_set_info, &m_action_set),
           (std::string("Failed to create action set \"") + info.name +
            "\". Name must not contain upper case letters or special characters "
            "other than '-', '_', or '.'.")
               .data());
}

GHOST_XrActionSet::~GHOST_XrActionSet()
{
  /* This needs to be done before xrDestroyActionSet() to avoid an assertion in the GHOST_XrAction
   * destructor (which calls xrDestroyAction()). */
  m_actions.clear();

  if (m_action_set != XR_NULL_HANDLE) {
    CHECK_XR_ASSERT(xrDestroyActionSet(m_action_set));
  }
}

bool GHOST_XrActionSet::createAction(XrInstance instance, const GHOST_XrActionInfo &info)
{
  if (m_actions.find(info.name) != m_actions.end()) {
    return false;
  }

  m_actions.emplace(std::piecewise_construct,
                    std::make_tuple(info.name),
                    std::make_tuple(instance, m_action_set, info));

  return true;
}

void GHOST_XrActionSet::destroyAction(const char *action_name)
{
  if (m_actions.find(action_name) != m_actions.end()) {
    m_actions.erase(action_name);
  }
}

GHOST_XrAction *GHOST_XrActionSet::findAction(const char *action_name)
{
  std::map<std::string, GHOST_XrAction>::iterator it = m_actions.find(action_name);
  if (it == m_actions.end()) {
    return nullptr;
  }
  return &it->second;
}

void GHOST_XrActionSet::updateStates(XrSession session,
                                     XrSpace reference_space,
                                     const XrTime &predicted_display_time)
{
  for (auto &[name, action] : m_actions) {
    action.updateState(session, name.data(), reference_space, predicted_display_time);
  }
}

XrActionSet GHOST_XrActionSet::getActionSet() const
{
  return m_action_set;
}

void *GHOST_XrActionSet::getCustomdata()
{
  if (m_custom_data_ == nullptr) {
    return nullptr;
  }
  return m_custom_data_->custom_data_;
}

void GHOST_XrActionSet::getBindings(
    std::map<XrPath, std::vector<XrActionSuggestedBinding>> &r_bindings) const
{
  for (auto &[name, action] : m_actions) {
    action.getBindings(r_bindings);
  }
}

/** \} */
