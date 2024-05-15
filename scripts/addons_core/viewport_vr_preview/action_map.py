# SPDX-FileCopyrightText: 2021-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

if "bpy" in locals():
    import importlib
    importlib.reload(defaults)
else:
    from . import action_map_io, defaults

import bpy
from bpy.app.handlers import persistent
from bpy_extras.io_utils import ExportHelper, ImportHelper
import importlib.util
import os.path


def vr_actionset_active_update(context):
    session_state = context.window_manager.xr_session_state
    if not session_state or len(session_state.actionmaps) < 1:
        return

    scene = context.scene

    if scene.vr_actions_use_gamepad and session_state.actionmaps.find(
            session_state, defaults.VRDefaultActionmaps.GAMEPAD.value):
        session_state.active_action_set_set(context, defaults.VRDefaultActionmaps.GAMEPAD.value)
    else:
        # Use first action map.
        session_state.active_action_set_set(context, session_state.actionmaps[0].name)


def vr_actions_use_gamepad_update(self, context):
    vr_actionset_active_update(context)


@persistent
def vr_create_actions(context: bpy.context):
    context = bpy.context
    session_state = context.window_manager.xr_session_state
    if not session_state:
        return

    # Check if actions are enabled.
    scene = context.scene
    if not scene.vr_actions_enable:
        return

    # Ensure default action maps.
    if not defaults.vr_ensure_default_actionmaps(session_state):
        return

    for am in session_state.actionmaps:
        if len(am.actionmap_items) < 1:
            continue

        ok = session_state.action_set_create(context, am)
        if not ok:
            return

        controller_grip_name = ""
        controller_aim_name = ""

        for ami in am.actionmap_items:
            if len(ami.bindings) < 1:
                continue

            ok = session_state.action_create(context, am, ami)
            if not ok:
                return

            if ami.type == 'POSE':
                if ami.pose_is_controller_grip:
                    controller_grip_name = ami.name
                if ami.pose_is_controller_aim:
                    controller_aim_name = ami.name

            for amb in ami.bindings:
                # Check for bindings that require OpenXR extensions.
                if amb.name == defaults.VRDefaultActionbindings.REVERB_G2.value:
                    if not scene.vr_actions_enable_reverb_g2:
                        continue
                elif amb.name == defaults.VRDefaultActionbindings.VIVE_COSMOS.value:
                    if not scene.vr_actions_enable_vive_cosmos:
                        continue
                elif amb.name == defaults.VRDefaultActionbindings.VIVE_FOCUS.value:
                    if not scene.vr_actions_enable_vive_focus:
                        continue
                elif amb.name == defaults.VRDefaultActionbindings.HUAWEI.value:
                    if not scene.vr_actions_enable_huawei:
                        continue

                ok = session_state.action_binding_create(context, am, ami, amb)
                if not ok:
                    return

        # Set controller pose actions.
        if controller_grip_name and controller_aim_name:
            session_state.controller_pose_actions_set(context, am.name, controller_grip_name, controller_aim_name)

    # Set active action set.
    vr_actionset_active_update(context)


def vr_load_actionmaps(session_state, filepath):
    if not os.path.exists(filepath):
        return False

    spec = importlib.util.spec_from_file_location(os.path.basename(filepath), filepath)
    file = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(file)

    action_map_io.actionconfig_init_from_data(session_state, file.actionconfig_data, file.actionconfig_version)

    return True


def vr_save_actionmaps(session_state, filepath, sort=False):
    action_map_io.actionconfig_export_as_data(session_state, filepath, sort=sort)

    print("Saved XR actionmaps: " + filepath)

    return True


def register():
    bpy.types.Scene.vr_actions_enable = bpy.props.BoolProperty(
        name="Use Controller Actions",
        description="Enable default VR controller actions, including controller poses and haptics",
        default=True,
    )
    bpy.types.Scene.vr_actions_use_gamepad = bpy.props.BoolProperty(
        description="Use input from gamepad (Microsoft Xbox Controller) instead of motion controllers",
        default=False,
        update=vr_actions_use_gamepad_update,
    )
    bpy.types.Scene.vr_actions_enable_huawei = bpy.props.BoolProperty(
        description="Enable bindings for the Huawei controllers. Note that this may not be supported by all OpenXR runtimes",
        default=False,
    )
    bpy.types.Scene.vr_actions_enable_reverb_g2 = bpy.props.BoolProperty(
        description="Enable bindings for the HP Reverb G2 controllers. Note that this may not be supported by all OpenXR runtimes",
        default=False,
    )
    bpy.types.Scene.vr_actions_enable_vive_cosmos = bpy.props.BoolProperty(
        description="Enable bindings for the HTC Vive Cosmos controllers. Note that this may not be supported by all OpenXR runtimes",
        default=False,
    )
    bpy.types.Scene.vr_actions_enable_vive_focus = bpy.props.BoolProperty(
        description="Enable bindings for the HTC Vive Focus 3 controllers. Note that this may not be supported by all OpenXR runtimes",
        default=False,
    )

    bpy.app.handlers.xr_session_start_pre.append(vr_create_actions)


def unregister():
    del bpy.types.Scene.vr_actions_enable
    del bpy.types.Scene.vr_actions_use_gamepad
    del bpy.types.Scene.vr_actions_enable_huawei
    del bpy.types.Scene.vr_actions_enable_reverb_g2
    del bpy.types.Scene.vr_actions_enable_vive_cosmos
    del bpy.types.Scene.vr_actions_enable_vive_focus

    bpy.app.handlers.xr_session_start_pre.remove(vr_create_actions)
