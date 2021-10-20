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
 * \ingroup wm
 *
 * \name Window-Manager XR Actions
 *
 * Uses the Ghost-XR API to manage OpenXR actions.
 * All functions are designed to be usable by RNA / the Python API.
 */

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "GHOST_C-api.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"
#include "WM_types.h"

#include "wm_xr_intern.h"

/* -------------------------------------------------------------------- */
/** \name XR-Action API
 *
 * API functions for managing OpenXR actions.
 *
 * \{ */

static wmXrActionSet *action_set_create(const char *action_set_name)
{
  wmXrActionSet *action_set = MEM_callocN(sizeof(*action_set), __func__);
  action_set->name = MEM_mallocN(strlen(action_set_name) + 1, "XrActionSet_Name");
  strcpy(action_set->name, action_set_name);

  return action_set;
}

static void action_set_destroy(void *val)
{
  wmXrActionSet *action_set = val;

  MEM_SAFE_FREE(action_set->name);

  BLI_freelistN(&action_set->active_modal_actions);
  BLI_freelistN(&action_set->active_haptic_actions);

  MEM_freeN(action_set);
}

static wmXrActionSet *action_set_find(wmXrData *xr, const char *action_set_name)
{
  return GHOST_XrGetActionSetCustomdata(xr->runtime->context, action_set_name);
}

static wmXrAction *action_create(const char *action_name,
                                 eXrActionType type,
                                 unsigned int count_subaction_paths,
                                 const char **subaction_paths,
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
  wmXrAction *action = MEM_callocN(sizeof(*action), __func__);
  action->name = MEM_mallocN(strlen(action_name) + 1, "XrAction_Name");
  strcpy(action->name, action_name);
  action->type = type;

  const unsigned int count = count_subaction_paths;
  action->count_subaction_paths = count;

  action->subaction_paths = MEM_mallocN(sizeof(*action->subaction_paths) * count,
                                        "XrAction_SubactionPaths");
  for (unsigned int i = 0; i < count; ++i) {
    action->subaction_paths[i] = MEM_mallocN(strlen(subaction_paths[i]) + 1,
                                             "XrAction_SubactionPath");
    strcpy(action->subaction_paths[i], subaction_paths[i]);
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
    action->float_thresholds = MEM_calloc_arrayN(
        count, sizeof(*action->float_thresholds), "XrAction_FloatThresholds");
  }
  if (is_button_action) {
    action->axis_flags = MEM_calloc_arrayN(
        count, sizeof(*action->axis_flags), "XrAction_AxisFlags");
  }

  action->ot = ot;
  action->op_properties = op_properties;

  if (haptic_name) {
    BLI_assert(is_button_action);
    action->haptic_name = MEM_mallocN(strlen(haptic_name) + 1, "XrAction_HapticName");
    strcpy(action->haptic_name, haptic_name);
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
  wmXrAction *action = val;

  MEM_SAFE_FREE(action->name);

  const unsigned int count = action->count_subaction_paths;
  char **subaction_paths = action->subaction_paths;
  if (subaction_paths) {
    for (unsigned int i = 0; i < count; ++i) {
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
  return GHOST_XrGetActionCustomdata(xr->runtime->context, action_set_name, action_name);
}

bool WM_xr_action_set_create(wmXrData *xr, const char *action_set_name)
{
  if (action_set_find(xr, action_set_name)) {
    return false;
  }

  wmXrActionSet *action_set = action_set_create(action_set_name);

  GHOST_XrActionSetInfo info = {
      .name = action_set_name,
      .customdata_free_fn = action_set_destroy,
      .customdata = action_set,
  };

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
      action_set->controller_grip_action = action_set->controller_aim_action = NULL;
    }

    BLI_freelistN(&action_set->active_modal_actions);
    BLI_freelistN(&action_set->active_haptic_actions);

    session_state->active_action_set = NULL;
  }

  GHOST_XrDestroyActionSet(xr->runtime->context, action_set_name);
}

bool WM_xr_action_create(wmXrData *xr,
                         const char *action_set_name,
                         const char *action_name,
                         eXrActionType type,
                         unsigned int count_subaction_paths,
                         const char **subaction_paths,
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
                                     count_subaction_paths,
                                     subaction_paths,
                                     ot,
                                     op_properties,
                                     haptic_name,
                                     haptic_duration,
                                     haptic_frequency,
                                     haptic_amplitude,
                                     op_flag,
                                     action_flag,
                                     haptic_flag);

  GHOST_XrActionInfo info = {
      .name = action_name,
      .count_subaction_paths = count_subaction_paths,
      .subaction_paths = subaction_paths,
      .states = action->states,
      .float_thresholds = action->float_thresholds,
      .axis_flags = (int16_t *)action->axis_flags,
      .customdata_free_fn = action_destroy,
      .customdata = action,
  };

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

  if (!GHOST_XrCreateActions(xr->runtime->context, action_set_name, 1, &info)) {
    return false;
  }

  return true;
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
       STREQ(action_set->controller_aim_action->name, action_name))) {
    if (action_set == xr->runtime->session_state.active_action_set) {
      wm_xr_session_controller_data_clear(&xr->runtime->session_state);
    }
    action_set->controller_grip_action = action_set->controller_aim_action = NULL;
  }

  LISTBASE_FOREACH (LinkData *, ld, &action_set->active_modal_actions) {
    wmXrAction *active_modal_action = ld->data;
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
                                 unsigned int count_subaction_paths,
                                 const char **subaction_paths,
                                 const char **component_paths,
                                 const float *float_thresholds,
                                 const eXrAxisFlag *axis_flags,
                                 const struct wmXrPose *poses)
{
  GHOST_XrActionBindingInfo *binding_infos = MEM_calloc_arrayN(
      count_subaction_paths, sizeof(*binding_infos), __func__);

  for (unsigned int i = 0; i < count_subaction_paths; ++i) {
    GHOST_XrActionBindingInfo *binding_info = &binding_infos[i];
    binding_info->component_path = component_paths[i];
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

  GHOST_XrActionProfileInfo profile_info = {
      .action_name = action_name,
      .profile_path = profile_path,
      .count_subaction_paths = count_subaction_paths,
      .subaction_paths = subaction_paths,
      .bindings = binding_infos,
  };

  bool ret = GHOST_XrCreateActionBindings(xr->runtime->context, action_set_name, 1, &profile_info);

  MEM_freeN(binding_infos);
  return ret;
}

void WM_xr_action_binding_destroy(wmXrData *xr,
                                  const char *action_set_name,
                                  const char *action_name,
                                  const char *profile_path)
{
  GHOST_XrDestroyActionBindings(
      xr->runtime->context, action_set_name, 1, &action_name, &profile_path);
}

bool WM_xr_active_action_set_set(wmXrData *xr, const char *action_set_name)
{
  wmXrActionSet *action_set = action_set_find(xr, action_set_name);
  if (!action_set) {
    return false;
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
  const unsigned int count = grip_action->count_subaction_paths;
  if (count != aim_action->count_subaction_paths) {
    return false;
  }

  for (unsigned int i = 0; i < count; ++i) {
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

  r_state->type = (int)action->type;

  /* Find the action state corresponding to the subaction path. */
  for (unsigned int i = 0; i < action->count_subaction_paths; ++i) {
    if (STREQ(subaction_path, action->subaction_paths[i])) {
      switch (action->type) {
        case XR_BOOLEAN_INPUT:
          r_state->state_boolean = ((bool *)action->states)[i];
          break;
        case XR_FLOAT_INPUT:
          r_state->state_float = ((float *)action->states)[i];
          break;
        case XR_VECTOR2F_INPUT:
          copy_v2_v2(r_state->state_vector2f, ((float(*)[2])action->states)[i]);
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

/** \} */ /* XR-Action API */
