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
                                 const float *float_threshold,
                                 wmOperatorType *ot,
                                 IDProperty *op_properties,
                                 eXrOpFlag op_flag)
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

  if (float_threshold) {
    BLI_assert(type == XR_FLOAT_INPUT || type == XR_VECTOR2F_INPUT);
    action->float_threshold = *float_threshold;
    CLAMP(action->float_threshold, 0.0f, 1.0f);
  }

  action->ot = ot;
  action->op_properties = op_properties;
  action->op_flag = op_flag;

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
    if (action_set->controller_pose_action) {
      wm_xr_session_controller_data_clear(session_state);
      action_set->controller_pose_action = NULL;
    }
    if (action_set->active_modal_action) {
      action_set->active_modal_action = NULL;
    }
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
                         const float *float_threshold,
                         wmOperatorType *ot,
                         IDProperty *op_properties,
                         eXrOpFlag op_flag)
{
  if (action_find(xr, action_set_name, action_name)) {
    return false;
  }

  wmXrAction *action = action_create(action_name,
                                     type,
                                     count_subaction_paths,
                                     subaction_paths,
                                     float_threshold,
                                     ot,
                                     op_properties,
                                     op_flag);

  GHOST_XrActionInfo info = {
      .name = action_name,
      .count_subaction_paths = count_subaction_paths,
      .subaction_paths = subaction_paths,
      .states = action->states,
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

  if (action_set->controller_pose_action &&
      STREQ(action_set->controller_pose_action->name, action_name)) {
    if (action_set == xr->runtime->session_state.active_action_set) {
      wm_xr_session_controller_data_clear(&xr->runtime->session_state);
    }
    action_set->controller_pose_action = NULL;
  }
  if (action_set->active_modal_action &&
      STREQ(action_set->active_modal_action->name, action_name)) {
    action_set->active_modal_action = NULL;
  }

  wmXrAction *action = action_find(xr, action_set_name, action_name);
  if (!action) {
    return;
  }
}

bool WM_xr_action_space_create(wmXrData *xr,
                               const char *action_set_name,
                               const char *action_name,
                               unsigned int count_subaction_paths,
                               const char **subaction_paths,
                               const wmXrPose *poses)
{
  GHOST_XrActionSpaceInfo info = {
      .action_name = action_name,
      .count_subaction_paths = count_subaction_paths,
      .subaction_paths = subaction_paths,
  };

  GHOST_XrPose *ghost_poses = MEM_malloc_arrayN(
      count_subaction_paths, sizeof(*ghost_poses), __func__);
  for (unsigned int i = 0; i < count_subaction_paths; ++i) {
    const wmXrPose *pose = &poses[i];
    GHOST_XrPose *ghost_pose = &ghost_poses[i];
    copy_v3_v3(ghost_pose->position, pose->position);
    copy_qt_qt(ghost_pose->orientation_quat, pose->orientation_quat);
  }
  info.poses = ghost_poses;

  bool ret = GHOST_XrCreateActionSpaces(xr->runtime->context, action_set_name, 1, &info) ? true :
                                                                                           false;
  MEM_freeN(ghost_poses);
  return ret;
}

void WM_xr_action_space_destroy(wmXrData *xr,
                                const char *action_set_name,
                                const char *action_name,
                                unsigned int count_subaction_paths,
                                const char **subaction_paths)
{
  GHOST_XrActionSpaceInfo info = {
      .action_name = action_name,
      .count_subaction_paths = count_subaction_paths,
      .subaction_paths = subaction_paths,
  };

  GHOST_XrDestroyActionSpaces(xr->runtime->context, action_set_name, 1, &info);
}

bool WM_xr_action_binding_create(wmXrData *xr,
                                 const char *action_set_name,
                                 const char *profile_path,
                                 const char *action_name,
                                 unsigned int count_interaction_paths,
                                 const char **interaction_paths)
{
  GHOST_XrActionBindingInfo binding_info = {
      .action_name = action_name,
      .count_interaction_paths = count_interaction_paths,
      .interaction_paths = interaction_paths,
  };

  GHOST_XrActionProfileInfo profile_info = {
      .profile_path = profile_path,
      .count_bindings = 1,
      .bindings = &binding_info,
  };

  return GHOST_XrCreateActionBindings(xr->runtime->context, action_set_name, 1, &profile_info);
}

void WM_xr_action_binding_destroy(wmXrData *xr,
                                  const char *action_set_name,
                                  const char *profile_path,
                                  const char *action_name,
                                  unsigned int count_interaction_paths,
                                  const char **interaction_paths)
{
  GHOST_XrActionBindingInfo binding_info = {
      .action_name = action_name,
      .count_interaction_paths = count_interaction_paths,
      .interaction_paths = interaction_paths,
  };

  GHOST_XrActionProfileInfo profile_info = {
      .profile_path = profile_path,
      .count_bindings = 1,
      .bindings = &binding_info,
  };

  GHOST_XrDestroyActionBindings(xr->runtime->context, action_set_name, 1, &profile_info);
}

bool WM_xr_active_action_set_set(wmXrData *xr, const char *action_set_name)
{
  wmXrActionSet *action_set = action_set_find(xr, action_set_name);
  if (!action_set) {
    return false;
  }

  {
    /* Unset active modal action (if any). */
    wmXrActionSet *active_action_set = xr->runtime->session_state.active_action_set;
    if (active_action_set) {
      wmXrAction *active_modal_action = active_action_set->active_modal_action;
      if (active_modal_action) {
        if (active_modal_action->active_modal_path) {
          active_modal_action->active_modal_path = NULL;
        }
        active_action_set->active_modal_action = NULL;
      }
    }
  }

  xr->runtime->session_state.active_action_set = action_set;

  if (action_set->controller_pose_action) {
    wm_xr_session_controller_data_populate(action_set->controller_pose_action, xr);
  }

  return true;
}

bool WM_xr_controller_pose_action_set(wmXrData *xr,
                                      const char *action_set_name,
                                      const char *action_name)
{
  wmXrActionSet *action_set = action_set_find(xr, action_set_name);
  if (!action_set) {
    return false;
  }

  wmXrAction *action = action_find(xr, action_set_name, action_name);
  if (!action) {
    return false;
  }

  action_set->controller_pose_action = action;

  if (action_set == xr->runtime->session_state.active_action_set) {
    wm_xr_session_controller_data_populate(action, xr);
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

  BLI_assert(action->type == (eXrActionType)r_state->type);

  /* Find the action state corresponding to the subaction path. */
  for (unsigned int i = 0; i < action->count_subaction_paths; ++i) {
    if (STREQ(subaction_path, action->subaction_paths[i])) {
      switch ((eXrActionType)r_state->type) {
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
                               const long long *duration,
                               const float *frequency,
                               const float *amplitude)
{
  return GHOST_XrApplyHapticAction(
             xr->runtime->context, action_set_name, action_name, duration, frequency, amplitude) ?
             true :
             false;
}

void WM_xr_haptic_action_stop(wmXrData *xr, const char *action_set_name, const char *action_name)
{
  GHOST_XrStopHapticAction(xr->runtime->context, action_set_name, action_name);
}

/** \} */ /* XR-Action API */
