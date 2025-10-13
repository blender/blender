/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * \name Window-Manager XR Actions
 *
 * Uses the Ghost-XR API to manage OpenXR actions.
 * All functions are designed to be usable by RNA / the Python API.
 */

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "GHOST_C-api.h"

#include "MEM_guardedalloc.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "wm_xr_intern.hh"

#include <cstring>

/* -------------------------------------------------------------------- */
/** \name XR-Action API
 *
 * API functions for managing OpenXR actions.
 *
 * \{ */

static wmXrActionSet *action_set_create(const char *action_set_name)
{
  wmXrActionSet *action_set = MEM_callocN<wmXrActionSet>(__func__);
  action_set->name = BLI_strdup(action_set_name);
  return action_set;
}

static void action_set_destroy(void *val)
{
  wmXrActionSet *action_set = static_cast<wmXrActionSet *>(val);

  MEM_SAFE_FREE(action_set->name);

  BLI_freelistN(&action_set->active_modal_actions);
  BLI_freelistN(&action_set->active_haptic_actions);

  MEM_freeN(action_set);
}

static wmXrActionSet *action_set_find(wmXrData *xr, const char *action_set_name)
{
  return static_cast<wmXrActionSet *>(
      GHOST_XrGetActionSetCustomdata(xr->runtime->context, action_set_name));
}

static wmXrAction *action_create(const char *action_name,
                                 eXrActionType type,
                                 const ListBase *user_paths,
                                 wmOperatorType *ot,
                                 IDProperty *op_properties,
                                 const char *haptic_name,
                                 const int64_t *haptic_duration,
                                 const float *haptic_frequency,
                                 const float *haptic_amplitude,
                                 eXrOpFlag op_flag,
                                 eXrActionFlag action_flag,
                                 eXrHapticFlag haptic_flag)
{
  wmXrAction *action = MEM_callocN<wmXrAction>(__func__);
  action->name = BLI_strdup(action_name);
  action->type = type;

  const uint count = uint(BLI_listbase_count(user_paths));
  uint subaction_idx = 0;
  action->count_subaction_paths = count;

  action->subaction_paths = MEM_malloc_arrayN<char *>(count, "XrAction_SubactionPaths");
  LISTBASE_FOREACH_INDEX (XrUserPath *, user_path, user_paths, subaction_idx) {
    action->subaction_paths[subaction_idx] = BLI_strdup(user_path->path);
  }

  size_t size;
  switch (type) {
    case XR_BOOLEAN_INPUT:
      size = sizeof(bool);
      break;
    case XR_FLOAT_INPUT:
      size = sizeof(float);
      break;
    case XR_VECTOR2F_INPUT:
      size = sizeof(float) * 2;
      break;
    case XR_POSE_INPUT:
      size = sizeof(GHOST_XrPose);
      break;
    case XR_VIBRATION_OUTPUT:
      return action;
  }
  action->states = MEM_calloc_arrayN(count, size, "XrAction_States");
  action->states_prev = MEM_calloc_arrayN(count, size, "XrAction_StatesPrev");

  const bool is_float_action = ELEM(type, XR_FLOAT_INPUT, XR_VECTOR2F_INPUT);
  const bool is_button_action = (is_float_action || type == XR_BOOLEAN_INPUT);
  if (is_float_action) {
    action->float_thresholds = MEM_calloc_arrayN<float>(count, "XrAction_FloatThresholds");
  }
  if (is_button_action) {
    action->axis_flags = MEM_calloc_arrayN<eXrAxisFlag>(count, "XrAction_AxisFlags");
  }

  action->ot = ot;
  action->op_properties = op_properties;

  if (haptic_name) {
    BLI_assert(is_button_action);
    action->haptic_name = BLI_strdup(haptic_name);
    action->haptic_duration = *haptic_duration;
    action->haptic_frequency = *haptic_frequency;
    action->haptic_amplitude = *haptic_amplitude;
  }

  action->op_flag = op_flag;
  action->action_flag = action_flag;
  action->haptic_flag = haptic_flag;

  return action;
}

static void action_destroy(void *val)
{
  wmXrAction *action = static_cast<wmXrAction *>(val);

  MEM_SAFE_FREE(action->name);

  char **subaction_paths = action->subaction_paths;
  if (subaction_paths) {
    for (uint i = 0; i < action->count_subaction_paths; ++i) {
      MEM_SAFE_FREE(subaction_paths[i]);
    }
    MEM_freeN(subaction_paths);
  }

  MEM_SAFE_FREE(action->states);
  MEM_SAFE_FREE(action->states_prev);

  MEM_SAFE_FREE(action->float_thresholds);
  MEM_SAFE_FREE(action->axis_flags);

  MEM_SAFE_FREE(action->haptic_name);

  MEM_freeN(action);
}

static wmXrAction *action_find(wmXrData *xr, const char *action_set_name, const char *action_name)
{
  return static_cast<wmXrAction *>(
      GHOST_XrGetActionCustomdata(xr->runtime->context, action_set_name, action_name));
}

bool WM_xr_action_set_create(wmXrData *xr, const char *action_set_name)
{
  if (action_set_find(xr, action_set_name)) {
    return false;
  }

  wmXrActionSet *action_set = action_set_create(action_set_name);

  GHOST_XrActionSetInfo info{};
  info.name = action_set_name;
  info.customdata_free_fn = action_set_destroy;
  info.customdata = action_set;

  if (!GHOST_XrCreateActionSet(xr->runtime->context, &info)) {
    return false;
  }

  return true;
}

void WM_xr_action_set_destroy(wmXrData *xr, const char *action_set_name)
{
  wmXrActionSet *action_set = action_set_find(xr, action_set_name);
  if (!action_set) {
    return;
  }

  wmXrSessionState *session_state = &xr->runtime->session_state;

  if (action_set == session_state->active_action_set) {
    if (action_set->controller_grip_action || action_set->controller_aim_action) {
      wm_xr_session_controller_data_clear(session_state);
      action_set->controller_grip_action = action_set->controller_aim_action = nullptr;
    }

    BLI_freelistN(&action_set->active_modal_actions);
    BLI_freelistN(&action_set->active_haptic_actions);

    session_state->active_action_set = nullptr;
  }

  GHOST_XrDestroyActionSet(xr->runtime->context, action_set_name);
}

bool WM_xr_action_create(wmXrData *xr,
                         const char *action_set_name,
                         const char *action_name,
                         eXrActionType type,
                         const ListBase *user_paths,
                         wmOperatorType *ot,
                         IDProperty *op_properties,
                         const char *haptic_name,
                         const int64_t *haptic_duration,
                         const float *haptic_frequency,
                         const float *haptic_amplitude,
                         eXrOpFlag op_flag,
                         eXrActionFlag action_flag,
                         eXrHapticFlag haptic_flag)
{
  if (action_find(xr, action_set_name, action_name)) {
    return false;
  }

  wmXrAction *action = action_create(action_name,
                                     type,
                                     user_paths,
                                     ot,
                                     op_properties,
                                     haptic_name,
                                     haptic_duration,
                                     haptic_frequency,
                                     haptic_amplitude,
                                     op_flag,
                                     action_flag,
                                     haptic_flag);

  const uint count = uint(BLI_listbase_count(user_paths));
  uint subaction_idx = 0;

  char **subaction_paths = MEM_calloc_arrayN<char *>(count, "XrAction_SubactionPathPointers");

  LISTBASE_FOREACH_INDEX (XrUserPath *, user_path, user_paths, subaction_idx) {
    subaction_paths[subaction_idx] = (char *)user_path->path;
  }

  GHOST_XrActionInfo info{};
  info.name = action_name;
  info.count_subaction_paths = count;
  info.subaction_paths = (const char **)subaction_paths;
  info.states = action->states;
  info.float_thresholds = action->float_thresholds;
  info.axis_flags = (int16_t *)action->axis_flags;
  info.customdata_free_fn = action_destroy;
  info.customdata = action;

  switch (type) {
    case XR_BOOLEAN_INPUT:
      info.type = GHOST_kXrActionTypeBooleanInput;
      break;
    case XR_FLOAT_INPUT:
      info.type = GHOST_kXrActionTypeFloatInput;
      break;
    case XR_VECTOR2F_INPUT:
      info.type = GHOST_kXrActionTypeVector2fInput;
      break;
    case XR_POSE_INPUT:
      info.type = GHOST_kXrActionTypePoseInput;
      break;
    case XR_VIBRATION_OUTPUT:
      info.type = GHOST_kXrActionTypeVibrationOutput;
      break;
  }

  const bool success = GHOST_XrCreateActions(xr->runtime->context, action_set_name, 1, &info);

  MEM_freeN(subaction_paths);

  return success;
}

void WM_xr_action_destroy(wmXrData *xr, const char *action_set_name, const char *action_name)
{
  wmXrActionSet *action_set = action_set_find(xr, action_set_name);
  if (!action_set) {
    return;
  }

  wmXrAction *action = action_find(xr, action_set_name, action_name);
  if (!action) {
    return;
  }

  if ((action_set->controller_grip_action &&
       STREQ(action_set->controller_grip_action->name, action_name)) ||
      (action_set->controller_aim_action &&
       STREQ(action_set->controller_aim_action->name, action_name)))
  {
    if (action_set == xr->runtime->session_state.active_action_set) {
      wm_xr_session_controller_data_clear(&xr->runtime->session_state);
    }
    action_set->controller_grip_action = action_set->controller_aim_action = nullptr;
  }

  LISTBASE_FOREACH (LinkData *, ld, &action_set->active_modal_actions) {
    wmXrAction *active_modal_action = static_cast<wmXrAction *>(ld->data);
    if (STREQ(active_modal_action->name, action_name)) {
      BLI_freelinkN(&action_set->active_modal_actions, ld);
      break;
    }
  }

  LISTBASE_FOREACH_MUTABLE (wmXrHapticAction *, ha, &action_set->active_haptic_actions) {
    if (STREQ(ha->action->name, action_name)) {
      BLI_freelinkN(&action_set->active_haptic_actions, ha);
    }
  }

  GHOST_XrDestroyActions(xr->runtime->context, action_set_name, 1, &action_name);
}

bool WM_xr_action_binding_create(wmXrData *xr,
                                 const char *action_set_name,
                                 const char *action_name,
                                 const char *profile_path,
                                 const ListBase *user_paths,
                                 const ListBase *component_paths,
                                 const float *float_thresholds,
                                 const eXrAxisFlag *axis_flags,
                                 const wmXrPose *poses)
{
  const uint count = uint(BLI_listbase_count(user_paths));
  BLI_assert(count == uint(BLI_listbase_count(component_paths)));

  GHOST_XrActionBindingInfo *binding_infos = MEM_calloc_arrayN<GHOST_XrActionBindingInfo>(
      count, "XrActionBinding_Infos");

  char **subaction_paths = MEM_calloc_arrayN<char *>(count,
                                                     "XrActionBinding_SubactionPathPointers");

  for (uint i = 0; i < count; ++i) {
    GHOST_XrActionBindingInfo *binding_info = &binding_infos[i];
    const XrUserPath *user_path = static_cast<const XrUserPath *>(BLI_findlink(user_paths, i));
    const XrComponentPath *component_path = static_cast<const XrComponentPath *>(
        BLI_findlink(component_paths, i));

    subaction_paths[i] = (char *)user_path->path;

    binding_info->component_path = component_path->path;
    if (float_thresholds) {
      binding_info->float_threshold = float_thresholds[i];
    }
    if (axis_flags) {
      binding_info->axis_flag = axis_flags[i];
    }
    if (poses) {
      copy_v3_v3(binding_info->pose.position, poses[i].position);
      copy_qt_qt(binding_info->pose.orientation_quat, poses[i].orientation_quat);
    }
  }

  GHOST_XrActionProfileInfo profile_info{};
  profile_info.action_name = action_name;
  profile_info.profile_path = profile_path;
  profile_info.count_subaction_paths = count;
  profile_info.subaction_paths = (const char **)subaction_paths;
  profile_info.bindings = binding_infos;

  const bool success = GHOST_XrCreateActionBindings(
      xr->runtime->context, action_set_name, 1, &profile_info);

  MEM_freeN(subaction_paths);
  MEM_freeN(binding_infos);

  return success;
}

void WM_xr_action_binding_destroy(wmXrData *xr,
                                  const char *action_set_name,
                                  const char *action_name,
                                  const char *profile_path)
{
  GHOST_XrDestroyActionBindings(
      xr->runtime->context, action_set_name, 1, &action_name, &profile_path);
}

bool WM_xr_active_action_set_set(wmXrData *xr, const char *action_set_name, bool delayed)
{
  wmXrActionSet *action_set = action_set_find(xr, action_set_name);
  if (!action_set) {
    return false;
  }

  if (delayed) {
    /* Save name to activate action set later, before next actions sync
     * (see #wm_xr_session_actions_update()). */
    STRNCPY(xr->runtime->session_state.active_action_set_next, action_set_name);
    return true;
  }

  {
    /* Clear any active modal/haptic actions. */
    wmXrActionSet *active_action_set = xr->runtime->session_state.active_action_set;
    if (active_action_set) {
      BLI_freelistN(&active_action_set->active_modal_actions);
      BLI_freelistN(&active_action_set->active_haptic_actions);
    }
  }

  xr->runtime->session_state.active_action_set = action_set;

  if (action_set->controller_grip_action && action_set->controller_aim_action) {
    wm_xr_session_controller_data_populate(
        action_set->controller_grip_action, action_set->controller_aim_action, xr);
  }
  else {
    wm_xr_session_controller_data_clear(&xr->runtime->session_state);
  }

  return true;
}

bool WM_xr_controller_pose_actions_set(wmXrData *xr,
                                       const char *action_set_name,
                                       const char *grip_action_name,
                                       const char *aim_action_name)
{
  wmXrActionSet *action_set = action_set_find(xr, action_set_name);
  if (!action_set) {
    return false;
  }

  wmXrAction *grip_action = action_find(xr, action_set_name, grip_action_name);
  if (!grip_action) {
    return false;
  }

  wmXrAction *aim_action = action_find(xr, action_set_name, aim_action_name);
  if (!aim_action) {
    return false;
  }

  /* Ensure consistent subaction paths. */
  const uint count = grip_action->count_subaction_paths;
  if (count != aim_action->count_subaction_paths) {
    return false;
  }

  for (uint i = 0; i < count; ++i) {
    if (!STREQ(grip_action->subaction_paths[i], aim_action->subaction_paths[i])) {
      return false;
    }
  }

  action_set->controller_grip_action = grip_action;
  action_set->controller_aim_action = aim_action;

  if (action_set == xr->runtime->session_state.active_action_set) {
    wm_xr_session_controller_data_populate(grip_action, aim_action, xr);
  }

  return true;
}

bool WM_xr_action_state_get(const wmXrData *xr,
                            const char *action_set_name,
                            const char *action_name,
                            const char *subaction_path,
                            wmXrActionState *r_state)
{
  const wmXrAction *action = action_find((wmXrData *)xr, action_set_name, action_name);
  if (!action) {
    return false;
  }

  r_state->type = int(action->type);

  /* Find the action state corresponding to the subaction path. */
  for (uint i = 0; i < action->count_subaction_paths; ++i) {
    if (STREQ(subaction_path, action->subaction_paths[i])) {
      switch (action->type) {
        case XR_BOOLEAN_INPUT:
          r_state->state_boolean = ((bool *)action->states)[i];
          break;
        case XR_FLOAT_INPUT:
          r_state->state_float = ((float *)action->states)[i];
          break;
        case XR_VECTOR2F_INPUT:
          copy_v2_v2(r_state->state_vector2f, ((float (*)[2])action->states)[i]);
          break;
        case XR_POSE_INPUT: {
          const GHOST_XrPose *pose = &((GHOST_XrPose *)action->states)[i];
          copy_v3_v3(r_state->state_pose.position, pose->position);
          copy_qt_qt(r_state->state_pose.orientation_quat, pose->orientation_quat);
          break;
        }
        case XR_VIBRATION_OUTPUT:
          BLI_assert_unreachable();
          break;
      }
      return true;
    }
  }

  return false;
}

bool WM_xr_haptic_action_apply(wmXrData *xr,
                               const char *action_set_name,
                               const char *action_name,
                               const char *subaction_path,
                               const int64_t *duration,
                               const float *frequency,
                               const float *amplitude)
{
  return GHOST_XrApplyHapticAction(xr->runtime->context,
                                   action_set_name,
                                   action_name,
                                   subaction_path,
                                   duration,
                                   frequency,
                                   amplitude) ?
             true :
             false;
}

void WM_xr_haptic_action_stop(wmXrData *xr,
                              const char *action_set_name,
                              const char *action_name,
                              const char *subaction_path)
{
  GHOST_XrStopHapticAction(xr->runtime->context, action_set_name, action_name, subaction_path);
}

/** \} */ /* XR-Action API. */
