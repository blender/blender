# SPDX-FileCopyrightText: 2021-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

if "bpy" in locals():
    import importlib
    importlib.reload(action_map)
else:
    from . import action_map

import bpy
from bpy.app.handlers import persistent
from enum import Enum
import math
import os.path


# Default action maps.
class VRDefaultActionmaps(Enum):
    DEFAULT = "blender_default"
    GAMEPAD = "blender_default_gamepad"


# Default actions.
class VRDefaultActions(Enum):
    CONTROLLER_GRIP = "controller_grip"
    CONTROLLER_AIM = "controller_aim"
    TELEPORT = "teleport"
    NAV_GRAB = "nav_grab"
    FLY = "fly"
    FLY_FORWARD = "fly_forward"
    FLY_BACK = "fly_back"
    FLY_LEFT = "fly_left"
    FLY_RIGHT = "fly_right"
    FLY_UP = "fly_up"
    FLY_DOWN = "fly_down"
    FLY_TURNLEFT = "fly_turnleft"
    FLY_TURNRIGHT = "fly_turnright"
    NAV_RESET = "nav_reset"
    SWAP_HANDS = "swap_hands"
    HAPTIC = "haptic"
    HAPTIC_LEFT = "haptic_left"
    HAPTIC_RIGHT = "haptic_right"
    HAPTIC_LEFTTRIGGER = "haptic_lefttrigger"
    HAPTIC_RIGHTTRIGGER = "haptic_righttrigger"


# Default action bindings.
class VRDefaultActionbindings(Enum):
    GAMEPAD = "gamepad"
    HUAWEI = "huawei"
    INDEX = "index"
    OCULUS = "oculus"
    REVERB_G2 = "reverb_g2"
    SIMPLE = "simple"
    VIVE = "vive"
    VIVE_COSMOS = "vive_cosmos"
    VIVE_FOCUS = "vive_focus"
    WMR = "wmr"


class VRDefaultActionprofiles(Enum):
    GAMEPAD = "/interaction_profiles/microsoft/xbox_controller"
    HUAWEI = "/interaction_profiles/huawei/controller"
    INDEX = "/interaction_profiles/valve/index_controller"
    OCULUS = "/interaction_profiles/oculus/touch_controller"
    REVERB_G2 = "/interaction_profiles/hp/mixed_reality_controller"
    SIMPLE = "/interaction_profiles/khr/simple_controller"
    VIVE = "/interaction_profiles/htc/vive_controller"
    VIVE_COSMOS = "/interaction_profiles/htc/vive_cosmos_controller"
    VIVE_FOCUS = "/interaction_profiles/htc/vive_focus3_controller"
    WMR = "/interaction_profiles/microsoft/motion_controller"


def vr_defaults_actionmap_add(session_state, name):
    am = session_state.actionmaps.new(session_state, name, True)

    return am


def vr_defaults_action_add(am,
                           name,
                           user_paths,
                           op,
                           op_mode,
                           bimanual,
                           haptic_name,
                           haptic_match_user_paths,
                           haptic_duration,
                           haptic_frequency,
                           haptic_amplitude,
                           haptic_mode):

    ami = am.actionmap_items.new(name, True)
    if ami:
        ami.type = 'FLOAT'
        for path in user_paths:
            ami.user_paths.new(path)
        ami.op = op
        ami.op_mode = op_mode
        ami.bimanual = bimanual
        ami.haptic_name = haptic_name
        ami.haptic_match_user_paths = haptic_match_user_paths
        ami.haptic_duration = haptic_duration
        ami.haptic_frequency = haptic_frequency
        ami.haptic_amplitude = haptic_amplitude
        ami.haptic_mode = haptic_mode

    return ami


def vr_defaults_pose_action_add(am,
                                name,
                                user_paths,
                                is_controller_grip,
                                is_controller_aim):
    ami = am.actionmap_items.new(name, True)
    if ami:
        ami.type = 'POSE'
        for path in user_paths:
            ami.user_paths.new(path)
        ami.pose_is_controller_grip = is_controller_grip
        ami.pose_is_controller_aim = is_controller_aim

    return ami


def vr_defaults_haptic_action_add(am,
                                  name,
                                  user_paths):
    ami = am.actionmap_items.new(name, True)
    if ami:
        ami.type = 'VIBRATION'
        for path in user_paths:
            ami.user_paths.new(path)

    return ami


def vr_defaults_actionbinding_add(ami,
                                  name,
                                  profile,
                                  component_paths,
                                  threshold,
                                  axis0_region,
                                  axis1_region):
    amb = ami.bindings.new(name, True)
    if amb:
        amb.profile = profile
        for path in component_paths:
            amb.component_paths.new(path)
        amb.threshold = threshold
        amb.axis0_region = axis0_region
        amb.axis1_region = axis1_region

    return amb


def vr_defaults_pose_actionbinding_add(ami,
                                       name,
                                       profile,
                                       component_paths,
                                       location,
                                       rotation):
    amb = ami.bindings.new(name, True)
    if amb:
        amb.profile = profile
        for path in component_paths:
            amb.component_paths.new(path)
        amb.pose_location = location
        amb.pose_rotation = rotation

    return amb


def vr_defaults_haptic_actionbinding_add(ami,
                                         name,
                                         profile,
                                         component_paths):
    amb = ami.bindings.new(name, True)
    if amb:
        amb.profile = profile
        for path in component_paths:
            amb.component_paths.new(path)

    return amb


def vr_defaults_create_default(session_state):
    am = vr_defaults_actionmap_add(session_state,
                                   VRDefaultActionmaps.DEFAULT.value)
    if not am:
        return

    ami = vr_defaults_pose_action_add(am,
                                      VRDefaultActions.CONTROLLER_GRIP.value,
                                      ["/user/hand/left",
                                       "/user/hand/right"],
                                      True,
                                      False)
    if ami:
        vr_defaults_pose_actionbinding_add(ami,
                                           VRDefaultActionbindings.HUAWEI.value,
                                           VRDefaultActionprofiles.HUAWEI.value,
                                           ["/input/grip/pose",
                                            "/input/grip/pose"],
                                           (0, 0, 0),
                                           (0, 0, 0))
        vr_defaults_pose_actionbinding_add(ami,
                                           VRDefaultActionbindings.INDEX.value,
                                           VRDefaultActionprofiles.INDEX.value,
                                           ["/input/grip/pose",
                                            "/input/grip/pose"],
                                           (0, 0, 0),
                                           (0, 0, 0))
        vr_defaults_pose_actionbinding_add(ami,
                                           VRDefaultActionbindings.OCULUS.value,
                                           VRDefaultActionprofiles.OCULUS.value,
                                           ["/input/grip/pose",
                                            "/input/grip/pose"],
                                           (0, 0, 0),
                                           (0, 0, 0))
        vr_defaults_pose_actionbinding_add(ami,
                                           VRDefaultActionbindings.REVERB_G2.value,
                                           VRDefaultActionprofiles.REVERB_G2.value,
                                           ["/input/grip/pose",
                                            "/input/grip/pose"],
                                           (0, 0, 0),
                                           (0, 0, 0))
        vr_defaults_pose_actionbinding_add(ami,
                                           VRDefaultActionbindings.SIMPLE.value,
                                           VRDefaultActionprofiles.SIMPLE.value,
                                           ["/input/grip/pose",
                                            "/input/grip/pose"],
                                           (0, 0, 0),
                                           (0, 0, 0))
        vr_defaults_pose_actionbinding_add(ami,
                                           VRDefaultActionbindings.VIVE.value,
                                           VRDefaultActionprofiles.VIVE.value,
                                           ["/input/grip/pose",
                                            "/input/grip/pose"],
                                           (0, 0, 0),
                                           (0, 0, 0))
        vr_defaults_pose_actionbinding_add(ami,
                                           VRDefaultActionbindings.VIVE_COSMOS.value,
                                           VRDefaultActionprofiles.VIVE_COSMOS.value,
                                           ["/input/grip/pose",
                                            "/input/grip/pose"],
                                           (0, 0, 0),
                                           (0, 0, 0))
        vr_defaults_pose_actionbinding_add(ami,
                                           VRDefaultActionbindings.VIVE_FOCUS.value,
                                           VRDefaultActionprofiles.VIVE_FOCUS.value,
                                           ["/input/grip/pose",
                                            "/input/grip/pose"],
                                           (0, 0, 0),
                                           (0, 0, 0))
        vr_defaults_pose_actionbinding_add(ami,
                                           VRDefaultActionbindings.WMR.value,
                                           VRDefaultActionprofiles.WMR.value,
                                           ["/input/grip/pose",
                                            "/input/grip/pose"],
                                           (0, 0, 0),
                                           (0, 0, 0))

    ami = vr_defaults_pose_action_add(am,
                                      VRDefaultActions.CONTROLLER_AIM.value,
                                      ["/user/hand/left",
                                       "/user/hand/right"],
                                      False,
                                      True)
    if ami:
        vr_defaults_pose_actionbinding_add(ami,
                                           VRDefaultActionbindings.HUAWEI.value,
                                           VRDefaultActionprofiles.HUAWEI.value,
                                           ["/input/aim/pose",
                                            "/input/aim/pose"],
                                           (0, 0, 0),
                                           (0, 0, 0))
        vr_defaults_pose_actionbinding_add(ami,
                                           VRDefaultActionbindings.INDEX.value,
                                           VRDefaultActionprofiles.INDEX.value,
                                           ["/input/aim/pose",
                                            "/input/aim/pose"],
                                           (0, 0, 0),
                                           (0, 0, 0))
        vr_defaults_pose_actionbinding_add(ami,
                                           VRDefaultActionbindings.OCULUS.value,
                                           VRDefaultActionprofiles.OCULUS.value,
                                           ["/input/aim/pose",
                                            "/input/aim/pose"],
                                           (0, 0, 0),
                                           (0, 0, 0))
        vr_defaults_pose_actionbinding_add(ami,
                                           VRDefaultActionbindings.REVERB_G2.value,
                                           VRDefaultActionprofiles.REVERB_G2.value,
                                           ["/input/aim/pose",
                                            "/input/aim/pose"],
                                           (0, 0, 0),
                                           (0, 0, 0))
        vr_defaults_pose_actionbinding_add(ami,
                                           VRDefaultActionbindings.SIMPLE.value,
                                           VRDefaultActionprofiles.SIMPLE.value,
                                           ["/input/aim/pose",
                                            "/input/aim/pose"],
                                           (0, 0, 0),
                                           (0, 0, 0))
        vr_defaults_pose_actionbinding_add(ami,
                                           VRDefaultActionbindings.VIVE.value,
                                           VRDefaultActionprofiles.VIVE.value,
                                           ["/input/aim/pose",
                                            "/input/aim/pose"],
                                           (0, 0, 0),
                                           (0, 0, 0))
        vr_defaults_pose_actionbinding_add(ami,
                                           VRDefaultActionbindings.VIVE_COSMOS.value,
                                           VRDefaultActionprofiles.VIVE_COSMOS.value,
                                           ["/input/aim/pose",
                                            "/input/aim/pose"],
                                           (0, 0, 0),
                                           (0, 0, 0))
        vr_defaults_pose_actionbinding_add(ami,
                                           VRDefaultActionbindings.VIVE_FOCUS.value,
                                           VRDefaultActionprofiles.VIVE_FOCUS.value,
                                           ["/input/aim/pose",
                                            "/input/aim/pose"],
                                           (0, 0, 0),
                                           (0, 0, 0))
        vr_defaults_pose_actionbinding_add(ami,
                                           VRDefaultActionbindings.WMR.value,
                                           VRDefaultActionprofiles.WMR.value,
                                           ["/input/aim/pose",
                                            "/input/aim/pose"],
                                           (0, 0, 0),
                                           (0, 0, 0))

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.TELEPORT.value,
                                 ["/user/hand/left",
                                  "/user/hand/right"],
                                 "wm.xr_navigation_teleport",
                                 'MODAL',
                                 False,
                                 "",
                                 False,
                                 0.0,
                                 0.0,
                                 0.0,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.HUAWEI.value,
                                      VRDefaultActionprofiles.HUAWEI.value,
                                      ["/input/trigger/value",
                                       "/input/trigger/value"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.INDEX.value,
                                      VRDefaultActionprofiles.INDEX.value,
                                      ["/input/trigger/value",
                                       "/input/trigger/value"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.OCULUS.value,
                                      VRDefaultActionprofiles.OCULUS.value,
                                      ["/input/trigger/value",
                                       "/input/trigger/value"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.REVERB_G2.value,
                                      VRDefaultActionprofiles.REVERB_G2.value,
                                      ["/input/trigger/value",
                                       "/input/trigger/value"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.SIMPLE.value,
                                      VRDefaultActionprofiles.SIMPLE.value,
                                      ["/input/select/click",
                                       "/input/select/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE.value,
                                      VRDefaultActionprofiles.VIVE.value,
                                      ["/input/trigger/value",
                                       "/input/trigger/value"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_COSMOS.value,
                                      VRDefaultActionprofiles.VIVE_COSMOS.value,
                                      ["/input/trigger/value",
                                       "/input/trigger/value"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_FOCUS.value,
                                      VRDefaultActionprofiles.VIVE_FOCUS.value,
                                      ["/input/trigger/value",
                                       "/input/trigger/value"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.WMR.value,
                                      VRDefaultActionprofiles.WMR.value,
                                      ["/input/trigger/value",
                                       "/input/trigger/value"],
                                      0.3,
                                      'ANY',
                                      'ANY')

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.NAV_GRAB.value,
                                 ["/user/hand/left",
                                  "/user/hand/right"],
                                 "wm.xr_navigation_grab",
                                 'MODAL',
                                 True,
                                 "",
                                 False,
                                 0.0,
                                 0.0,
                                 0.0,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.HUAWEI.value,
                                      VRDefaultActionprofiles.HUAWEI.value,
                                      ["/input/trackpad/click",
                                       "/input/trackpad/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.INDEX.value,
                                      VRDefaultActionprofiles.INDEX.value,
                                      ["/input/squeeze/force",
                                       "/input/squeeze/force"],
                                      0.5,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.OCULUS.value,
                                      VRDefaultActionprofiles.OCULUS.value,
                                      ["/input/squeeze/value",
                                       "/input/squeeze/value"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.REVERB_G2.value,
                                      VRDefaultActionprofiles.REVERB_G2.value,
                                      ["/input/squeeze/value",
                                       "/input/squeeze/value"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.SIMPLE.value,
                                      VRDefaultActionprofiles.SIMPLE.value,
                                      ["/input/menu/click",
                                       "/input/menu/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE.value,
                                      VRDefaultActionprofiles.VIVE.value,
                                      ["/input/squeeze/click",
                                       "/input/squeeze/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_COSMOS.value,
                                      VRDefaultActionprofiles.VIVE_COSMOS.value,
                                      ["/input/squeeze/click",
                                       "/input/squeeze/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_FOCUS.value,
                                      VRDefaultActionprofiles.VIVE_FOCUS.value,
                                      ["/input/squeeze/click",
                                       "/input/squeeze/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.WMR.value,
                                      VRDefaultActionprofiles.WMR.value,
                                      ["/input/squeeze/click",
                                       "/input/squeeze/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.FLY_FORWARD.value,
                                 ["/user/hand/left"],
                                 "wm.xr_navigation_fly",
                                 'MODAL',
                                 False,
                                 "",
                                 False,
                                 0.0,
                                 0.0,
                                 0.0,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.HUAWEI.value,
                                      VRDefaultActionprofiles.HUAWEI.value,
                                      ["/input/trackpad/y"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.INDEX.value,
                                      VRDefaultActionprofiles.INDEX.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.OCULUS.value,
                                      VRDefaultActionprofiles.OCULUS.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.REVERB_G2.value,
                                      VRDefaultActionprofiles.REVERB_G2.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE.value,
                                      VRDefaultActionprofiles.VIVE.value,
                                      ["/input/trackpad/y"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_COSMOS.value,
                                      VRDefaultActionprofiles.VIVE_COSMOS.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_FOCUS.value,
                                      VRDefaultActionprofiles.VIVE_FOCUS.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.WMR.value,
                                      VRDefaultActionprofiles.WMR.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.FLY_BACK.value,
                                 ["/user/hand/left"],
                                 "wm.xr_navigation_fly",
                                 'MODAL',
                                 False,
                                 "",
                                 False,
                                 0.0,
                                 0.0,
                                 0.0,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.HUAWEI.value,
                                      VRDefaultActionprofiles.HUAWEI.value,
                                      ["/input/trackpad/y"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.INDEX.value,
                                      VRDefaultActionprofiles.INDEX.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.OCULUS.value,
                                      VRDefaultActionprofiles.OCULUS.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.REVERB_G2.value,
                                      VRDefaultActionprofiles.REVERB_G2.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE.value,
                                      VRDefaultActionprofiles.VIVE.value,
                                      ["/input/trackpad/y"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_COSMOS.value,
                                      VRDefaultActionprofiles.VIVE_COSMOS.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_FOCUS.value,
                                      VRDefaultActionprofiles.VIVE_FOCUS.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.WMR.value,
                                      VRDefaultActionprofiles.WMR.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.FLY_LEFT.value,
                                 ["/user/hand/left"],
                                 "wm.xr_navigation_fly",
                                 'MODAL',
                                 False,
                                 "",
                                 False,
                                 0.0,
                                 0.0,
                                 0.0,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.HUAWEI.value,
                                      VRDefaultActionprofiles.HUAWEI.value,
                                      ["/input/trackpad/x"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.INDEX.value,
                                      VRDefaultActionprofiles.INDEX.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.OCULUS.value,
                                      VRDefaultActionprofiles.OCULUS.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.REVERB_G2.value,
                                      VRDefaultActionprofiles.REVERB_G2.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE.value,
                                      VRDefaultActionprofiles.VIVE.value,
                                      ["/input/trackpad/x"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_COSMOS.value,
                                      VRDefaultActionprofiles.VIVE_COSMOS.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_FOCUS.value,
                                      VRDefaultActionprofiles.VIVE_FOCUS.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.WMR.value,
                                      VRDefaultActionprofiles.WMR.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.FLY_RIGHT.value,
                                 ["/user/hand/left"],
                                 "wm.xr_navigation_fly",
                                 'MODAL',
                                 False,
                                 "",
                                 False,
                                 0.0,
                                 0.0,
                                 0.0,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.HUAWEI.value,
                                      VRDefaultActionprofiles.HUAWEI.value,
                                      ["/input/trackpad/x"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.INDEX.value,
                                      VRDefaultActionprofiles.INDEX.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.OCULUS.value,
                                      VRDefaultActionprofiles.OCULUS.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.REVERB_G2.value,
                                      VRDefaultActionprofiles.REVERB_G2.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE.value,
                                      VRDefaultActionprofiles.VIVE.value,
                                      ["/input/trackpad/x"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_COSMOS.value,
                                      VRDefaultActionprofiles.VIVE_COSMOS.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_FOCUS.value,
                                      VRDefaultActionprofiles.VIVE_FOCUS.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.WMR.value,
                                      VRDefaultActionprofiles.WMR.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.FLY_UP.value,
                                 ["/user/hand/right"],
                                 "wm.xr_navigation_fly",
                                 'MODAL',
                                 False,
                                 "",
                                 False,
                                 0.0,
                                 0.0,
                                 0.0,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.HUAWEI.value,
                                      VRDefaultActionprofiles.HUAWEI.value,
                                      ["/input/trackpad/y"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.INDEX.value,
                                      VRDefaultActionprofiles.INDEX.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.OCULUS.value,
                                      VRDefaultActionprofiles.OCULUS.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.REVERB_G2.value,
                                      VRDefaultActionprofiles.REVERB_G2.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE.value,
                                      VRDefaultActionprofiles.VIVE.value,
                                      ["/input/trackpad/y"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_COSMOS.value,
                                      VRDefaultActionprofiles.VIVE_COSMOS.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_FOCUS.value,
                                      VRDefaultActionprofiles.VIVE_FOCUS.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.WMR.value,
                                      VRDefaultActionprofiles.WMR.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.FLY_DOWN.value,
                                 ["/user/hand/right"],
                                 "wm.xr_navigation_fly",
                                 'MODAL',
                                 False,
                                 "",
                                 False,
                                 0.0,
                                 0.0,
                                 0.0,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.HUAWEI.value,
                                      VRDefaultActionprofiles.HUAWEI.value,
                                      ["/input/trackpad/y"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.INDEX.value,
                                      VRDefaultActionprofiles.INDEX.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.OCULUS.value,
                                      VRDefaultActionprofiles.OCULUS.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.REVERB_G2.value,
                                      VRDefaultActionprofiles.REVERB_G2.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE.value,
                                      VRDefaultActionprofiles.VIVE.value,
                                      ["/input/trackpad/y"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_COSMOS.value,
                                      VRDefaultActionprofiles.VIVE_COSMOS.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_FOCUS.value,
                                      VRDefaultActionprofiles.VIVE_FOCUS.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.WMR.value,
                                      VRDefaultActionprofiles.WMR.value,
                                      ["/input/thumbstick/y"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.FLY_TURNLEFT.value,
                                 ["/user/hand/right"],
                                 "wm.xr_navigation_fly",
                                 'MODAL',
                                 False,
                                 "",
                                 False,
                                 0.0,
                                 0.0,
                                 0.0,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.HUAWEI.value,
                                      VRDefaultActionprofiles.HUAWEI.value,
                                      ["/input/trackpad/x"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.INDEX.value,
                                      VRDefaultActionprofiles.INDEX.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.OCULUS.value,
                                      VRDefaultActionprofiles.OCULUS.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.REVERB_G2.value,
                                      VRDefaultActionprofiles.REVERB_G2.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE.value,
                                      VRDefaultActionprofiles.VIVE.value,
                                      ["/input/trackpad/x"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_COSMOS.value,
                                      VRDefaultActionprofiles.VIVE_COSMOS.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_FOCUS.value,
                                      VRDefaultActionprofiles.VIVE_FOCUS.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.WMR.value,
                                      VRDefaultActionprofiles.WMR.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.FLY_TURNRIGHT.value,
                                 ["/user/hand/right"],
                                 "wm.xr_navigation_fly",
                                 'MODAL',
                                 False,
                                 "",
                                 False,
                                 0.0,
                                 0.0,
                                 0.0,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.HUAWEI.value,
                                      VRDefaultActionprofiles.HUAWEI.value,
                                      ["/input/trackpad/x"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.INDEX.value,
                                      VRDefaultActionprofiles.INDEX.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.OCULUS.value,
                                      VRDefaultActionprofiles.OCULUS.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.REVERB_G2.value,
                                      VRDefaultActionprofiles.REVERB_G2.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE.value,
                                      VRDefaultActionprofiles.VIVE.value,
                                      ["/input/trackpad/x"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_COSMOS.value,
                                      VRDefaultActionprofiles.VIVE_COSMOS.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_FOCUS.value,
                                      VRDefaultActionprofiles.VIVE_FOCUS.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.WMR.value,
                                      VRDefaultActionprofiles.WMR.value,
                                      ["/input/thumbstick/x"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.NAV_RESET.value,
                                 ["/user/hand/left",
                                  "/user/hand/right"],
                                 "wm.xr_navigation_reset",
                                 'PRESS',
                                 False,
                                 "haptic",
                                 True,
                                 0.3,
                                 3000.0,
                                 0.5,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.HUAWEI.value,
                                      VRDefaultActionprofiles.HUAWEI.value,
                                      ["/input/back/click",
                                       "/input/back/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.INDEX.value,
                                      VRDefaultActionprofiles.INDEX.value,
                                      ["/input/a/click",
                                       "/input/a/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.OCULUS.value,
                                      VRDefaultActionprofiles.OCULUS.value,
                                      ["/input/x/click",
                                       "/input/a/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.REVERB_G2.value,
                                      VRDefaultActionprofiles.REVERB_G2.value,
                                      ["/input/x/click",
                                       "/input/a/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE.value,
                                      VRDefaultActionprofiles.VIVE.value,
                                      ["/input/menu/click",
                                       "/input/menu/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_COSMOS.value,
                                      VRDefaultActionprofiles.VIVE_COSMOS.value,
                                      ["/input/x/click",
                                       "/input/a/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_FOCUS.value,
                                      VRDefaultActionprofiles.VIVE_FOCUS.value,
                                      ["/input/x/click",
                                       "/input/a/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.WMR.value,
                                      VRDefaultActionprofiles.WMR.value,
                                      ["/input/menu/click",
                                       "/input/menu/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.SWAP_HANDS.value,
                                 ["/user/hand/left",
                                  "/user/hand/right"],
                                 "wm.xr_navigation_swap_hands",
                                 'PRESS',
                                 False,
                                 "haptic",
                                 True,
                                 0.3,
                                 3000.0,
                                 0.5,
                                 'PRESS')
    if ami:
        """
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.HUAWEI.value,
                                      VRDefaultActionprofiles.HUAWEI.value,
                                      ["/input/back/click",
                                       "/input/back/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        """
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.INDEX.value,
                                      VRDefaultActionprofiles.INDEX.value,
                                      ["/input/b/click",
                                       "/input/b/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.OCULUS.value,
                                      VRDefaultActionprofiles.OCULUS.value,
                                      ["/input/y/click",
                                       "/input/b/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.REVERB_G2.value,
                                      VRDefaultActionprofiles.REVERB_G2.value,
                                      ["/input/y/click",
                                       "/input/b/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        """
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE.value,
                                      VRDefaultActionprofiles.VIVE.value,
                                      ["/input/menu/click",
                                       "/input/menu/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        """
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_COSMOS.value,
                                      VRDefaultActionprofiles.VIVE_COSMOS.value,
                                      ["/input/y/click",
                                       "/input/b/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.VIVE_FOCUS.value,
                                      VRDefaultActionprofiles.VIVE_FOCUS.value,
                                      ["/input/y/click",
                                       "/input/b/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        """
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.WMR.value,
                                      VRDefaultActionprofiles.WMR.value,
                                      ["/input/menu/click",
                                       "/input/menu/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')
        """

    ami = vr_defaults_haptic_action_add(am,
                                        VRDefaultActions.HAPTIC.value,
                                        ["/user/hand/left",
                                         "/user/hand/right"])
    if ami:
        vr_defaults_haptic_actionbinding_add(ami,
                                             VRDefaultActionbindings.HUAWEI.value,
                                             VRDefaultActionprofiles.HUAWEI.value,
                                             ["/output/haptic",
                                              "/output/haptic"])
        vr_defaults_haptic_actionbinding_add(ami,
                                             VRDefaultActionbindings.INDEX.value,
                                             VRDefaultActionprofiles.INDEX.value,
                                             ["/output/haptic",
                                              "/output/haptic"])
        vr_defaults_haptic_actionbinding_add(ami,
                                             VRDefaultActionbindings.OCULUS.value,
                                             VRDefaultActionprofiles.OCULUS.value,
                                             ["/output/haptic",
                                              "/output/haptic"])
        vr_defaults_haptic_actionbinding_add(ami,
                                             VRDefaultActionbindings.REVERB_G2.value,
                                             VRDefaultActionprofiles.REVERB_G2.value,
                                             ["/output/haptic",
                                              "/output/haptic"])
        vr_defaults_haptic_actionbinding_add(ami,
                                             VRDefaultActionbindings.SIMPLE.value,
                                             VRDefaultActionprofiles.SIMPLE.value,
                                             ["/output/haptic",
                                              "/output/haptic"])
        vr_defaults_haptic_actionbinding_add(ami,
                                             VRDefaultActionbindings.VIVE.value,
                                             VRDefaultActionprofiles.VIVE.value,
                                             ["/output/haptic",
                                              "/output/haptic"])
        vr_defaults_haptic_actionbinding_add(ami,
                                             VRDefaultActionbindings.VIVE_COSMOS.value,
                                             VRDefaultActionprofiles.VIVE_COSMOS.value,
                                             ["/output/haptic",
                                              "/output/haptic"])
        vr_defaults_haptic_actionbinding_add(ami,
                                             VRDefaultActionbindings.VIVE_FOCUS.value,
                                             VRDefaultActionprofiles.VIVE_FOCUS.value,
                                             ["/output/haptic",
                                              "/output/haptic"])
        vr_defaults_haptic_actionbinding_add(ami,
                                             VRDefaultActionbindings.WMR.value,
                                             VRDefaultActionprofiles.WMR.value,
                                             ["/output/haptic",
                                              "/output/haptic"])


def vr_defaults_create_default_gamepad(session_state):
    am = vr_defaults_actionmap_add(session_state,
                                   VRDefaultActionmaps.GAMEPAD.value)

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.TELEPORT.value,
                                 ["/user/gamepad"],
                                 "wm.xr_navigation_teleport",
                                 'MODAL',
                                 False,
                                 "",
                                 False,
                                 0.0,
                                 0.0,
                                 0.0,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.GAMEPAD.value,
                                      VRDefaultActionprofiles.GAMEPAD.value,
                                      ["/input/trigger_right/value"],
                                      0.3,
                                      'ANY',
                                      'ANY')

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.FLY.value,
                                 ["/user/gamepad"],
                                 "wm.xr_navigation_fly",
                                 'MODAL',
                                 False,
                                 "",
                                 False,
                                 0.0,
                                 0.0,
                                 0.0,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.GAMEPAD.value,
                                      VRDefaultActionprofiles.GAMEPAD.value,
                                      ["/input/trigger_left/value"],
                                      0.3,
                                      'ANY',
                                      'ANY')

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.FLY_FORWARD.value,
                                 ["/user/gamepad"],
                                 "wm.xr_navigation_fly",
                                 'MODAL',
                                 False,
                                 "",
                                 False,
                                 0.0,
                                 0.0,
                                 0.0,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.GAMEPAD.value,
                                      VRDefaultActionprofiles.GAMEPAD.value,
                                      ["/input/thumbstick_left/y"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.FLY_BACK.value,
                                 ["/user/gamepad"],
                                 "wm.xr_navigation_fly",
                                 'MODAL',
                                 False,
                                 "",
                                 False,
                                 0.0,
                                 0.0,
                                 0.0,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.GAMEPAD.value,
                                      VRDefaultActionprofiles.GAMEPAD.value,
                                      ["/input/thumbstick_left/y"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.FLY_LEFT.value,
                                 ["/user/gamepad"],
                                 "wm.xr_navigation_fly",
                                 'MODAL',
                                 False,
                                 "",
                                 False,
                                 0.0,
                                 0.0,
                                 0.0,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.GAMEPAD.value,
                                      VRDefaultActionprofiles.GAMEPAD.value,
                                      ["/input/thumbstick_left/x"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.FLY_RIGHT.value,
                                 ["/user/gamepad"],
                                 "wm.xr_navigation_fly",
                                 'MODAL',
                                 False,
                                 "",
                                 False,
                                 0.0,
                                 0.0,
                                 0.0,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.GAMEPAD.value,
                                      VRDefaultActionprofiles.GAMEPAD.value,
                                      ["/input/thumbstick_left/x"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.FLY_UP.value,
                                 ["/user/gamepad"],
                                 "wm.xr_navigation_fly",
                                 'MODAL',
                                 False,
                                 "",
                                 False,
                                 0.0,
                                 0.0,
                                 0.0,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.GAMEPAD.value,
                                      VRDefaultActionprofiles.GAMEPAD.value,
                                      ["/input/thumbstick_right/y"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.FLY_DOWN.value,
                                 ["/user/gamepad"],
                                 "wm.xr_navigation_fly",
                                 'MODAL',
                                 False,
                                 "",
                                 False,
                                 0.0,
                                 0.0,
                                 0.0,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.GAMEPAD.value,
                                      VRDefaultActionprofiles.GAMEPAD.value,
                                      ["/input/thumbstick_right/y"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.FLY_TURNLEFT.value,
                                 ["/user/gamepad"],
                                 "wm.xr_navigation_fly",
                                 'MODAL',
                                 False,
                                 "",
                                 False,
                                 0.0,
                                 0.0,
                                 0.0,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.GAMEPAD.value,
                                      VRDefaultActionprofiles.GAMEPAD.value,
                                      ["/input/thumbstick_right/x"],
                                      0.3,
                                      'NEGATIVE',
                                      'ANY')

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.FLY_TURNRIGHT.value,
                                 ["/user/gamepad"],
                                 "wm.xr_navigation_fly",
                                 'MODAL',
                                 False,
                                 "",
                                 False,
                                 0.0,
                                 0.0,
                                 0.0,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.GAMEPAD.value,
                                      VRDefaultActionprofiles.GAMEPAD.value,
                                      ["/input/thumbstick_right/x"],
                                      0.3,
                                      'POSITIVE',
                                      'ANY')

    ami = vr_defaults_action_add(am,
                                 VRDefaultActions.NAV_RESET.value,
                                 ["/user/gamepad"],
                                 "wm.xr_navigation_reset",
                                 'PRESS',
                                 False,
                                 "haptic_right",
                                 True,
                                 0.3,
                                 3000.0,
                                 0.5,
                                 'PRESS')
    if ami:
        vr_defaults_actionbinding_add(ami,
                                      VRDefaultActionbindings.GAMEPAD.value,
                                      VRDefaultActionprofiles.GAMEPAD.value,
                                      ["/input/a/click"],
                                      0.3,
                                      'ANY',
                                      'ANY')

    ami = vr_defaults_haptic_action_add(am,
                                        VRDefaultActions.HAPTIC_LEFT.value,
                                        ["/user/gamepad"])
    if ami:
        vr_defaults_haptic_actionbinding_add(ami,
                                             VRDefaultActionbindings.GAMEPAD.value,
                                             VRDefaultActionprofiles.GAMEPAD.value,
                                             ["/output/haptic_left"])

    ami = vr_defaults_haptic_action_add(am,
                                        VRDefaultActions.HAPTIC_RIGHT.value,
                                        ["/user/gamepad"])
    if ami:
        vr_defaults_haptic_actionbinding_add(ami,
                                             VRDefaultActionbindings.GAMEPAD.value,
                                             VRDefaultActionprofiles.GAMEPAD.value,
                                             ["/output/haptic_right"])

    ami = vr_defaults_haptic_action_add(am,
                                        VRDefaultActions.HAPTIC_LEFTTRIGGER.value,
                                        ["/user/gamepad"])
    if ami:
        vr_defaults_haptic_actionbinding_add(ami,
                                             VRDefaultActionbindings.GAMEPAD.value,
                                             VRDefaultActionprofiles.GAMEPAD.value,
                                             ["/output/haptic_left_trigger"])

    ami = vr_defaults_haptic_action_add(am,
                                        VRDefaultActions.HAPTIC_RIGHTTRIGGER.value,
                                        ["/user/gamepad"])
    if ami:
        vr_defaults_haptic_actionbinding_add(ami,
                                             VRDefaultActionbindings.GAMEPAD.value,
                                             VRDefaultActionprofiles.GAMEPAD.value,
                                             ["/output/haptic_right_trigger"])


def vr_get_default_config_path():
    filepath = os.path.join(os.path.dirname(os.path.abspath(__file__)), "configs")
    return os.path.join(filepath, "default.py")


def vr_ensure_default_actionmaps(session_state):
    loaded = True

    for name in VRDefaultActionmaps:
        if not session_state.actionmaps.find(session_state, name.value):
            loaded = False
            break

    if loaded:
        return loaded

    # Load default action maps.
    filepath = vr_get_default_config_path()

    if not os.path.exists(filepath):
        # Create and save default action maps.
        vr_defaults_create_default(session_state)
        vr_defaults_create_default_gamepad(session_state)

        action_map.vr_save_actionmaps(session_state, filepath, sort=False)

    loaded = action_map.vr_load_actionmaps(session_state, filepath)

    return loaded
