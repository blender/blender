# SPDX-FileCopyrightText: 2021-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

if "bpy" in locals():
    import importlib
    importlib.reload(properties)
else:
    from . import properties

import bpy
import gpu
from bpy.app.translations import pgettext_data as data_
from bpy.types import (
    Gizmo,
    GizmoGroup,
    Operator,
)
import math
from math import radians
from mathutils import Color, Euler, Matrix, Quaternion, Vector


# Landmarks.
class VIEW3D_OT_vr_landmark_add(Operator):
    bl_idname = "view3d.vr_landmark_add"
    bl_label = "Add VR Landmark"
    bl_description = "Add a new VR landmark to the list and select it"
    bl_options = {'UNDO', 'REGISTER'}

    def execute(self, context):
        scene = context.scene
        landmarks = scene.vr_landmarks

        landmarks.add()

        # select newly created set
        scene.vr_landmarks_selected = len(landmarks) - 1

        return {'FINISHED'}


class VIEW3D_OT_vr_landmark_from_camera(Operator):
    bl_idname = "view3d.vr_landmark_from_camera"
    bl_label = "Add VR Landmark from Camera"
    bl_description = "Add a new VR landmark from the active camera object to the list and select it"
    bl_options = {'UNDO', 'REGISTER'}

    @classmethod
    def poll(cls, context):
        cam_selected = False

        vl_objects = bpy.context.view_layer.objects
        if vl_objects.active and vl_objects.active.type == 'CAMERA':
            cam_selected = True
        return cam_selected

    def execute(self, context):
        scene = context.scene
        landmarks = scene.vr_landmarks
        cam = context.view_layer.objects.active
        lm = landmarks.add()
        lm.type = 'OBJECT'
        lm.base_pose_object = cam
        lm.name = "LM_" + cam.name

        # select newly created set
        scene.vr_landmarks_selected = len(landmarks) - 1

        return {'FINISHED'}


class VIEW3D_OT_vr_landmark_from_session(Operator):
    bl_idname = "view3d.vr_landmark_from_session"
    bl_label = "Add VR Landmark from Session"
    bl_description = "Add VR landmark from the viewer pose of the running VR session to the list and select it"
    bl_options = {'UNDO', 'REGISTER'}

    @classmethod
    def poll(cls, context):
        return bpy.types.XrSessionState.is_running(context)

    def execute(self, context):
        scene = context.scene
        landmarks = scene.vr_landmarks
        wm = context.window_manager

        lm = landmarks.add()
        lm.type = "CUSTOM"
        scene.vr_landmarks_selected = len(landmarks) - 1

        loc = wm.xr_session_state.viewer_pose_location
        rot = wm.xr_session_state.viewer_pose_rotation.to_euler()

        lm.base_pose_location = loc
        lm.base_pose_angle = rot[2]

        return {'FINISHED'}


class VIEW3D_OT_vr_camera_landmark_from_session(Operator):
    bl_idname = "view3d.vr_camera_landmark_from_session"
    bl_label = "Add Camera and VR Landmark from Session"
    bl_description = "Create a new Camera and VR Landmark from the viewer pose of the running VR session and select it"
    bl_options = {'UNDO', 'REGISTER'}

    @classmethod
    def poll(cls, context):
        return bpy.types.XrSessionState.is_running(context)

    def execute(self, context):
        scene = context.scene
        landmarks = scene.vr_landmarks
        wm = context.window_manager

        lm = landmarks.add()
        lm.type = 'OBJECT'
        scene.vr_landmarks_selected = len(landmarks) - 1

        loc = wm.xr_session_state.viewer_pose_location
        rot = wm.xr_session_state.viewer_pose_rotation.to_euler()

        cam = bpy.data.cameras.new(data_("Camera") + "_" + lm.name)
        new_cam = bpy.data.objects.new(data_("Camera") + "_" + lm.name, cam)
        scene.collection.objects.link(new_cam)
        new_cam.location = loc
        new_cam.rotation_euler = rot

        lm.base_pose_object = new_cam

        return {'FINISHED'}


class VIEW3D_OT_update_vr_landmark(Operator):
    bl_idname = "view3d.update_vr_landmark"
    bl_label = "Update Custom VR Landmark"
    bl_description = "Update the selected landmark from the current viewer pose in the VR session"
    bl_options = {'UNDO', 'REGISTER'}

    @classmethod
    def poll(cls, context):
        selected_landmark = properties.VRLandmark.get_selected_landmark(context)
        return bpy.types.XrSessionState.is_running(context) and selected_landmark.type == 'CUSTOM'

    def execute(self, context):
        wm = context.window_manager

        lm = properties.VRLandmark.get_selected_landmark(context)

        loc = wm.xr_session_state.viewer_pose_location
        rot = wm.xr_session_state.viewer_pose_rotation.to_euler()

        lm.base_pose_location = loc
        lm.base_pose_angle = rot

        # Re-activate the landmark to trigger viewer reset and flush landmark settings to the session settings.
        properties.vr_landmark_active_update(None, context)

        return {'FINISHED'}


class VIEW3D_OT_vr_landmark_remove(Operator):
    bl_idname = "view3d.vr_landmark_remove"
    bl_label = "Remove VR Landmark"
    bl_description = "Delete the selected VR landmark from the list"
    bl_options = {'UNDO', 'REGISTER'}

    def execute(self, context):
        scene = context.scene
        landmarks = scene.vr_landmarks

        if len(landmarks) > 1:
            landmark_selected_idx = scene.vr_landmarks_selected
            landmarks.remove(landmark_selected_idx)

            scene.vr_landmarks_selected -= 1

        return {'FINISHED'}


class VIEW3D_OT_cursor_to_vr_landmark(Operator):
    bl_idname = "view3d.cursor_to_vr_landmark"
    bl_label = "Cursor to VR Landmark"
    bl_description = "Move the 3D Cursor to the selected VR Landmark"
    bl_options = {'UNDO', 'REGISTER'}

    @classmethod
    def poll(cls, context):
        lm = properties.VRLandmark.get_selected_landmark(context)
        if lm.type == 'SCENE_CAMERA':
            return context.scene.camera is not None
        elif lm.type == 'OBJECT':
            return lm.base_pose_object is not None

        return True

    def execute(self, context):
        scene = context.scene
        lm = properties.VRLandmark.get_selected_landmark(context)
        if lm.type == 'SCENE_CAMERA':
            lm_pos = scene.camera.location
        elif lm.type == 'OBJECT':
            lm_pos = lm.base_pose_object.location
        else:
            lm_pos = lm.base_pose_location
        scene.cursor.location = lm_pos

        return {'FINISHED'}


class VIEW3D_OT_add_camera_from_vr_landmark(Operator):
    bl_idname = "view3d.add_camera_from_vr_landmark"
    bl_label = "New Camera from VR Landmark"
    bl_description = "Create a new Camera from the selected VR Landmark"
    bl_options = {'UNDO', 'REGISTER'}

    def execute(self, context):
        scene = context.scene
        lm = properties.VRLandmark.get_selected_landmark(context)

        cam = bpy.data.cameras.new(data_("Camera") + "_" + lm.name)
        new_cam = bpy.data.objects.new(data_("Camera") + "_" + lm.name, cam)
        scene.collection.objects.link(new_cam)
        angle = lm.base_pose_angle
        new_cam.location = lm.base_pose_location
        new_cam.rotation_euler = (math.pi / 2, 0, angle)

        return {'FINISHED'}


class VIEW3D_OT_camera_to_vr_landmark(Operator):
    bl_idname = "view3d.camera_to_vr_landmark"
    bl_label = "Scene Camera to VR Landmark"
    bl_description = "Position the scene camera at the selected landmark"
    bl_options = {'UNDO', 'REGISTER'}

    @classmethod
    def poll(cls, context):
        return context.scene.camera is not None

    def execute(self, context):
        scene = context.scene
        lm = properties.VRLandmark.get_selected_landmark(context)

        cam = scene.camera
        angle = lm.base_pose_angle
        cam.location = lm.base_pose_location
        cam.rotation_euler = (math.pi / 2, 0, angle)

        return {'FINISHED'}


class VIEW3D_OT_vr_landmark_activate(Operator):
    bl_idname = "view3d.vr_landmark_activate"
    bl_label = "Activate VR Landmark"
    bl_description = "Change to the selected VR landmark from the list"
    bl_options = {'UNDO', 'REGISTER'}

    index: bpy.props.IntProperty(
        name="Index",
        options={'HIDDEN'},
    )

    def execute(self, context):
        scene = context.scene

        if self.index >= len(scene.vr_landmarks):
            return {'CANCELLED'}

        scene.vr_landmarks_active = (
            self.index if self.properties.is_property_set(
                "index") else scene.vr_landmarks_selected
        )

        return {'FINISHED'}


# Location Scouting Viewfinder
def viewfinder_camera_gizmo_view3d_redraw_workaround():
    # Workaround: After capturing or deleting a shot from the VR viewfinder, tag all View3D areas in the current context
    #             window (parent XR window) for redraw to display the newly created/deleted captures.
    #             The alternative to this is to give the capture camera Gizmo (VIEW3D_GGT_vr_captures) the VR_REDRAWS
    #             option, however this makes it constantly redraw, causing performances to drop.

    window = bpy.context.window

    areas = [area for area in window.screen.areas if area.type == 'VIEW_3D']
    for area in areas:
        view3d_region = [region for region in area.regions if region.type == 'WINDOW'][0]
        with bpy.context.temp_override(
                window=window,
                area=area,
                region=view3d_region,
                screen=window.screen
        ):
            bpy.context.region.tag_redraw()


def xr_event_match_viewfinder_hand(xr_event, xr_settings):
    # Check if the current XR event matches with the Viewfinder hand. Equivalent to the internal
    # wm_xr_operator_event_match_viewfinder_hand C++ function.
    hand_user_path_map = {
        'LEFT': "/user/hand/left",
        'RIGHT': "/user/hand/right"
    }

    return xr_event.user_path == hand_user_path_map[xr_settings.viewfinder_hand]


class VIEW3D_OT_vr_location_scouting_viewfinder_capture(Operator):
    bl_idname = "view3d.vr_location_scouting_viewfinder_capture"
    bl_label = "Viewfinder Capture"
    bl_description = "Create a VR Capture from the Viewfinder pose and mark it as selected"
    bl_options = {'UNDO', 'INTERNAL'}

    @classmethod
    def poll(cls, context):
        session_is_running = bpy.types.XrSessionState.is_running(context)

        xr_settings = context.window_manager.xr_session_settings
        xr_viewfinder = context.window_manager.xr_session_state.viewfinder

        viewfinder_enabled = xr_settings.viewfinder_enabled
        viewfinder_in_live_mode = xr_viewfinder.active_mode == "LIVE"

        return session_is_running and viewfinder_enabled and viewfinder_in_live_mode

    def execute(self, context):
        scene = context.scene
        captures = scene.vr_captures

        wm = context.window_manager
        xr_viewfinder = wm.xr_session_state.viewfinder

        # Returns the first available name in the style (Base_001, Base_002, Base_003, etc...).
        def unique_name(col, base: str) -> str:
            existing_indexes = set()

            for item in col:
                name = item.name
                if name.startswith(base) and len(name) == len(base) + 4:
                    suffix = name[len(base) + 1:]
                    if suffix.isdigit():
                        existing_indexes.add(int(suffix))

            idx = 1
            while idx in existing_indexes:
                idx += 1

            return f"{base}_{idx:03d}"

        capture = captures.add()
        captures[-1].name = unique_name(captures, "Capture")
        scene.vr_captures_selected = len(captures) - 1

        capture.location = xr_viewfinder.location
        capture.orientation = xr_viewfinder.orientation

        capture.lens_focal = xr_viewfinder.capture_lens_focal
        capture.dof_enabled = xr_viewfinder.capture_dof_enabled
        capture.dof_distance = xr_viewfinder.capture_dof_distance
        capture.dof_fstop = xr_viewfinder.capture_dof_fstop

        xr_viewfinder.trigger_flash()

        viewfinder_camera_gizmo_view3d_redraw_workaround()

        return {'FINISHED'}

    def invoke(self, context, event):
        xr_event = event.xr
        xr_settings = context.window_manager.xr_session_settings

        if not xr_event_match_viewfinder_hand(xr_event, xr_settings):
            return {'CANCELLED'}

        return self.execute(context)


class VIEW3D_OT_vr_location_scouting_viewfinder_apply_action(Operator):
    bl_idname = "view3d.vr_location_scouting_viewfinder_apply_action"
    bl_label = "Viewfinder Apply Action"
    bl_description = "Apply the currently selected Viewfinder action (Zoom Control/Playback selection for now)"
    bl_options = {'INTERNAL'}

    # Differentiate between an up and down action (two possible buttons).
    action_up: bpy.props.BoolProperty(
        name="Is Up Action",
        options={'HIDDEN'},
    )

    @staticmethod
    def get_next_in_map(current, map_, up_dir) -> int:
        # Find the closest map idx to the `current` value.
        diff_list = [abs(elem - current) for elem in map_]
        current_idx = diff_list.index(min(diff_list))

        # Find the next element going up or down, clamping at bounds.
        if up_dir:
            # Zoom in
            next_idx = min(current_idx + 1, len(map_) - 1)
        else:
            # Zoom out
            next_idx = max(current_idx - 1, 0)

        return next_idx

    @staticmethod
    def focus_distance_raycast(context, view_origin, view_quat):
        scene = context.scene
        depsgraph = context.evaluated_depsgraph_get()

        direction = Vector((0.0, 0.0, -1.0))
        world_dir = view_quat @ direction
        world_dir.normalize()

        hit_success, hit_location, _, _, _, _ = scene.ray_cast(depsgraph, view_origin, world_dir)

        if hit_success:
            return (hit_location - view_origin).length

        return None

    @classmethod
    def poll(cls, context):
        session_is_running = bpy.types.XrSessionState.is_running(context)
        has_scene_camera = context.scene.camera is not None
        viewfinder_enabled = context.window_manager.xr_session_settings.viewfinder_enabled

        return session_is_running and has_scene_camera and viewfinder_enabled

    def execute(self, context):
        wm = context.window_manager
        scene = context.scene

        xr_viewfinder = wm.xr_session_state.viewfinder
        captures = scene.vr_captures

        if xr_viewfinder.active_mode == 'LIVE':
            focal_map = (18, 20, 24, 28, 35, 50, 70, 85, 100, 135, 200, 300)
            fstop_map = (0.1, 0.2, 0.4, 0.8, 1, 1.2, 1.4, 1.7, 2, 2.4, 2.8, 3.3,
                         4, 4.8, 5.6, 6.7, 8, 9.5, 11, 13, 16, 19, 22, 27, 32)

            match xr_viewfinder.active_action_live:
                # View Zoom Control.
                case 'LENS':
                    current_focal = xr_viewfinder.capture_lens_focal

                    new_idx = self.get_next_in_map(current_focal, focal_map, self.action_up)
                    xr_viewfinder.capture_lens_focal = focal_map[new_idx]

                    return {'FINISHED'}

                # Toggle DoF on/off.
                case 'DOF':
                    xr_viewfinder.capture_dof_enabled = not xr_viewfinder.capture_dof_enabled

                    return {'FINISHED'}

                # Focus distance control (ray-cast autofocus).
                case 'FOCUS':
                    raycast_hit = self.focus_distance_raycast(context,
                                                              xr_viewfinder.location,
                                                              xr_viewfinder.orientation)

                    if raycast_hit is not None:
                        xr_viewfinder.capture_dof_distance = raycast_hit

                    xr_viewfinder.trigger_focus_indicator(raycast_hit is not None)

                    return {'FINISHED'}

                # F-Stop control.
                case 'APERTURE':
                    current_fstop = xr_viewfinder.capture_dof_fstop

                    new_idx = self.get_next_in_map(current_fstop, fstop_map, self.action_up)
                    xr_viewfinder.capture_dof_fstop = fstop_map[new_idx]

                    return {'FINISHED'}

        if xr_viewfinder.active_mode == 'PLAYBACK':
            # Playback control.
            if len(captures) == 0:
                return {'FINISHED'}

            match xr_viewfinder.active_action_playback:
                # Browse shots left/right.
                case 'BROWSE':
                    incr = 1 if self.action_up else -1
                    scene.vr_captures_selected = (scene.vr_captures_selected + incr) % len(captures)

                    return {'FINISHED'}

                # Preview the selected capture in space toggle.
                case 'PREVIEW':
                    xr_viewfinder.playback_show_active_capture_in_space_enabled = not xr_viewfinder.playback_show_active_capture_in_space_enabled

                    return {'FINISHED'}

                # Delete, switch to Confirm mode to confirm capture deletion.
                case 'DELETE':
                    xr_viewfinder.active_mode = 'CONFIRM'
                    # Set base action to Cancel, user has to explicitly switch to the Confirm action.
                    xr_viewfinder.active_action_confirm = 'CANCEL'

                    return {'FINISHED'}

        if xr_viewfinder.active_mode == 'CONFIRM':
            # Confirm mode, currently only used for capture deletion, and only
            # accessible from the 'DELETE' Playback action.
            match xr_viewfinder.active_action_confirm:
                # Cancel, simply switch back to Playback mode.
                case 'CANCEL':
                    xr_viewfinder.active_mode = 'PLAYBACK'

                    return {'FINISHED'}

                # Confirm, delete the selected capture, switch back to Playback mode.
                case 'CONFIRM':
                    captures.remove(scene.vr_captures_selected)
                    if scene.vr_captures_selected > 0:
                        scene.vr_captures_selected -= 1

                    viewfinder_camera_gizmo_view3d_redraw_workaround()

                    xr_viewfinder.active_mode = 'PLAYBACK'

                    return {'FINISHED'}

        return {'CANCELLED'}

    def invoke(self, context, event):
        xr_event = event.xr
        xr_settings = context.window_manager.xr_session_settings

        if not xr_event_match_viewfinder_hand(xr_event, xr_settings):
            return {'CANCELLED'}

        return self.execute(context)


class VIEW3D_OT_vr_location_scouting_viewfinder_cycle_mode(Operator):
    bl_idname = "view3d.vr_location_scouting_viewfinder_cycle_mode"
    bl_label = "Viewfinder Cycle Mode"
    bl_description = "Cycle the currently active Viewfinder mode"
    bl_options = {'INTERNAL'}

    def execute(self, context):
        xr_viewfinder = context.window_manager.xr_session_state.viewfinder

        # Limit cycling to Live and Playback, Confirm being an internal mode used for capture deletion.
        enum_values = ['LIVE', 'PLAYBACK']
        if xr_viewfinder.active_mode not in enum_values:
            return {'CANCELLED'}

        current_mode_idx = enum_values.index(xr_viewfinder.active_mode)

        xr_viewfinder.active_mode = enum_values[(current_mode_idx + 1) % len(enum_values)]

        return {'FINISHED'}

    def invoke(self, context, event):
        xr_event = event.xr
        xr_settings = context.window_manager.xr_session_settings

        if not xr_event_match_viewfinder_hand(xr_event, xr_settings):
            return {'CANCELLED'}

        return self.execute(context)


class VIEW3D_OT_vr_location_scouting_viewfinder_cycle_action(Operator):
    bl_idname = "view3d.vr_location_scouting_viewfinder_cycle_action"
    bl_label = "Viewfinder Cycle Action"
    bl_description = "Cycle the currently active Viewfinder action left or right"
    bl_options = {'INTERNAL'}

    cycle_left: bpy.props.BoolProperty(
        name="Cycle Left",
        default=False,
        options={'HIDDEN', 'SKIP_SAVE'},
    )

    def execute(self, context):
        xr_viewfinder = context.window_manager.xr_session_state.viewfinder

        increment = -1 if self.cycle_left else 1

        match xr_viewfinder.active_mode:
            case 'LIVE':
                action_rna_prop = xr_viewfinder.rna_type.properties["active_action_live"]
                enum_keys = action_rna_prop.enum_items.keys()
                current_action_idx = enum_keys.index(xr_viewfinder.active_action_live)

                # Special case: only allow cycling to non-DoF action if DoF is not enabled
                enum_length = len(enum_keys) if xr_viewfinder.capture_dof_enabled else (enum_keys.index('DOF') + 1)
                new_action_idx = (current_action_idx + increment) % enum_length

                xr_viewfinder.active_action_live = enum_keys[new_action_idx]

            case 'PLAYBACK':
                action_rna_prop = xr_viewfinder.rna_type.properties["active_action_playback"]
                enum_keys = action_rna_prop.enum_items.keys()
                current_action_idx = enum_keys.index(xr_viewfinder.active_action_playback)
                new_action_idx = (current_action_idx + increment) % len(enum_keys)

                xr_viewfinder.active_action_playback = enum_keys[new_action_idx]

            case 'CONFIRM':
                action_rna_prop = xr_viewfinder.rna_type.properties["active_action_confirm"]
                enum_keys = action_rna_prop.enum_items.keys()
                current_action_idx = enum_keys.index(xr_viewfinder.active_action_confirm)
                new_action_idx = (current_action_idx + increment) % len(enum_keys)

                xr_viewfinder.active_action_confirm = enum_keys[new_action_idx]

        return {'FINISHED'}

    def invoke(self, context, event):
        xr_event = event.xr
        xr_settings = context.window_manager.xr_session_settings

        if not xr_event_match_viewfinder_hand(xr_event, xr_settings):
            return {'CANCELLED'}

        axis_value = xr_event.state[0]
        self.cycle_left = axis_value <= 0

        return self.execute(context)


class VIEW3D_OT_vr_location_scouting_viewfinder_swap_hands(Operator):
    bl_idname = "view3d.vr_location_scouting_viewfinder_swap_hands"
    bl_label = "Viewfinder Swap Hands"
    bl_description = "Swap user hand used to hold the Viewfinder"
    bl_options = {'INTERNAL'}

    def execute(self, context):
        xr_viewfinder = context.window_manager.xr_session_state.viewfinder
        xr_settings = context.window_manager.xr_session_settings

        viewfinder_hand_rna_prop = xr_settings.rna_type.properties['viewfinder_hand']
        enum_values = viewfinder_hand_rna_prop.enum_items.keys()
        current_hand_idx = enum_values.index(xr_settings.viewfinder_hand)

        xr_settings.viewfinder_hand = enum_values[(current_hand_idx + 1) % len(enum_values)]

        xr_viewfinder.reset_view_smoothing()

        return {'FINISHED'}

    def invoke(self, context, _event):
        xr_settings = context.window_manager.xr_session_settings

        if not xr_settings.viewfinder_enabled:
            return {'CANCELLED'}

        return self.execute(context)


# Location Scouting Captures
def capture_camera_name(capture):
    return data_("Camera") + "_" + capture.name


def add_camera_from_capture(scene, capture):
    cam = bpy.data.cameras.new(capture_camera_name(capture))
    new_cam = bpy.data.objects.new(capture_camera_name(capture), cam)
    scene.collection.objects.link(new_cam)

    new_cam.location = capture.location
    new_cam.rotation_mode = 'QUATERNION'
    new_cam.rotation_quaternion = capture.orientation
    new_cam.rotation_mode = 'XYZ'

    new_cam.data.lens = capture.lens_focal
    new_cam.data.dof.use_dof = capture.dof_enabled
    new_cam.data.dof.focus_distance = capture.dof_distance
    new_cam.data.dof.aperture_fstop = capture.dof_fstop

    return new_cam


class VIEW3D_OT_vr_location_scouting_add_camera_from_capture(Operator):
    bl_idname = "view3d.vr_location_scouting_add_camera_from_capture"
    bl_label = "Add Camera from VR Capture"
    bl_description = "Create a new Camera Object from the selected VR Capture"
    bl_options = {'UNDO', 'REGISTER'}

    @classmethod
    def poll(cls, context):
        return len(context.scene.vr_captures) > 0

    def execute(self, context):
        scene = context.scene
        capture = properties.VRCapture.get_selected_capture(context)

        add_camera_from_capture(scene, capture)

        return {'FINISHED'}


class VIEW3D_OT_vr_location_scouting_add_marker_from_capture(Operator):
    bl_idname = "view3d.vr_location_scouting_add_marker_from_capture"
    bl_label = "Add Camera Marker from VR Capture"
    bl_description = "Create a new Camera bound to a Marker from the selected VR Capture at the current frame"
    bl_options = {'UNDO', 'REGISTER'}

    @classmethod
    def poll(cls, context):
        return len(context.scene.vr_captures) > 0

    def execute(self, context):
        scene = context.scene
        sel_capture = properties.VRCapture.get_selected_capture(context)

        # Check if the selected capture camera already exist, if not create it.
        capture_cam = scene.objects.get(capture_camera_name(sel_capture))

        def cam_matches_capture(camera, capture):
            if camera is None:
                return False

            return camera.location == capture.location and camera.rotation_quaternion == capture.orientation

        if not cam_matches_capture(capture_cam, sel_capture):
            capture_cam = add_camera_from_capture(scene, sel_capture)

        marker = scene.timeline_markers.new(capture_cam.name, frame=scene.frame_current)
        marker.camera = capture_cam

        return {'FINISHED'}


class VIEW3D_OT_vr_location_scouting_active_camera_to_capture(Operator):
    bl_idname = "view3d.vr_location_scouting_active_camera_to_capture"
    bl_label = "Set Active Camera from VR Capture"
    bl_description = "Set the active Scene Camera settings from the selected VR Capture"
    bl_options = {'UNDO', 'REGISTER'}

    @classmethod
    def poll(cls, context):
        return len(context.scene.vr_captures) > 0

    def execute(self, context):
        capture = properties.VRCapture.get_selected_capture(context)

        cam = context.scene.camera
        cam.location = capture.location
        cam.rotation_mode = 'QUATERNION'
        cam.rotation_quaternion = capture.orientation
        cam.rotation_mode = 'XYZ'

        cam.data.shift_x = 0
        cam.data.shift_y = 0

        cam.data.sensor_fit = 'AUTO'
        cam.data.sensor_width = 36

        cam.data.lens = capture.lens_focal
        cam.data.dof.use_dof = capture.dof_enabled
        cam.data.dof.focus_distance = capture.dof_distance
        cam.data.dof.aperture_fstop = capture.dof_fstop

        return {'FINISHED'}


class VIEW3D_OT_vr_location_scouting_remove_capture(Operator):
    bl_idname = "view3d.vr_location_scouting_remove_capture"
    bl_label = "Remove Location Scouting Capture"
    bl_description = "Remove the selected Location Scouting Capture"
    bl_options = {'UNDO', 'REGISTER'}

    @classmethod
    def poll(cls, context):
        return len(context.scene.vr_captures) > 0

    def execute(self, context):
        scene = context.scene
        captures = scene.vr_captures

        capture_selected_idx = scene.vr_captures_selected
        captures.remove(capture_selected_idx)

        if scene.vr_captures_selected > 0:
            scene.vr_captures_selected -= 1

        viewfinder_camera_gizmo_view3d_redraw_workaround()

        return {'FINISHED'}


class VIEW3D_OT_vr_location_scouting_browse_captures(Operator):
    bl_idname = "view3d.vr_location_scouting_browse_captures"
    bl_label = "Browse Location Scouting Captures"
    bl_description = "Browse Location Scouting Captures forward or backward"

    backward: bpy.props.BoolProperty(
        name="Browse Backward",
        default=False,
        options={'HIDDEN', 'SKIP_SAVE'},
    )

    @classmethod
    def poll(cls, context):
        return len(context.scene.vr_captures) > 0

    def execute(self, context):
        scene = context.scene

        incr = -1 if self.backward else 1
        scene.vr_captures_selected = (scene.vr_captures_selected + incr) % len(scene.vr_captures)

        viewfinder_camera_gizmo_view3d_redraw_workaround()

        return {'FINISHED'}


# Gizmos.
class VIEW3D_GT_vr_camera_cone(Gizmo):
    bl_idname = "VIEW_3D_GT_vr_camera_cone"

    aspect = 1.0, 1.0
    focal = 1.0

    def draw(self, context):
        if not hasattr(self, "frame_shape"):
            ui_scale = context.preferences.view.ui_scale
            aspect = tuple(a / ui_scale for a in self.aspect)
            focal = self.focal / ui_scale

            frame_shape_verts = (
                (-aspect[0], -aspect[1], -focal),
                (aspect[0], -aspect[1], -focal),
                (aspect[0], aspect[1], -focal),
                (-aspect[0], aspect[1], -focal),
                (-aspect[0], -aspect[1], -focal),
            )
            lines_shape_verts = (
                (0.0, 0.0, 0.0),
                frame_shape_verts[0],
                (0.0, 0.0, 0.0),
                frame_shape_verts[1],
                (0.0, 0.0, 0.0),
                frame_shape_verts[2],
                (0.0, 0.0, 0.0),
                frame_shape_verts[3],
            )

            self.frame_shape = self.new_custom_shape(
                'LINE_STRIP', frame_shape_verts)
            self.lines_shape = self.new_custom_shape(
                'LINES', lines_shape_verts)

        line_width = 2

        shader = gpu.shader.from_builtin('POLYLINE_UNIFORM_COLOR')
        shader.uniform_float("viewportSize", gpu.state.viewport_get()[2:])
        shader.uniform_float("lineWidth", line_width * gpu.state.line_width_get())

        # Override default shader given by new_custom_shape()
        self.frame_shape = (self.frame_shape[0], shader)
        self.lines_shape = (self.lines_shape[0], shader)

        gpu.state.blend_set('ALPHA')
        self.draw_custom_shape(self.frame_shape)
        self.draw_custom_shape(self.lines_shape)


class VIEW3D_GT_vr_controller_grip(Gizmo):
    bl_idname = "VIEW_3D_GT_vr_controller_grip"

    def draw(self, context):
        gpu.state.line_width_set(1.0)
        gpu.state.blend_set('ALPHA')

        self.color = 0.422, 0.438, 0.446
        self.draw_preset_circle(self.matrix_basis, axis='POS_X')
        self.draw_preset_circle(self.matrix_basis, axis='POS_Y')
        self.draw_preset_circle(self.matrix_basis, axis='POS_Z')


class VIEW3D_GT_vr_controller_aim(Gizmo):
    bl_idname = "VIEW_3D_GT_vr_controller_aim"

    def draw(self, context):
        gpu.state.line_width_set(1.0)
        gpu.state.blend_set('ALPHA')

        self.color = 1.0, 0.2, 0.322
        self.draw_preset_arrow(self.matrix_basis, axis='POS_X')
        self.color = 0.545, 0.863, 0.0
        self.draw_preset_arrow(self.matrix_basis, axis='POS_Y')
        self.color = 0.157, 0.565, 1.0
        self.draw_preset_arrow(self.matrix_basis, axis='POS_Z')


class VIEW3D_GGT_vr_viewer_pose(GizmoGroup):
    bl_idname = "VIEW3D_GGT_vr_viewer_pose"
    bl_label = "VR Viewer Pose Indicator"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'WINDOW'
    bl_options = {'3D', 'PERSISTENT', 'SCALE', 'VR_REDRAWS'}

    @classmethod
    def poll(cls, context):
        view3d = context.space_data
        return (
            view3d.shading.vr_show_virtual_camera and
            bpy.types.XrSessionState.is_running(context) and
            not view3d.mirror_xr_session
        )

    @staticmethod
    def _get_viewer_pose_matrix(context):
        wm = context.window_manager

        loc = wm.xr_session_state.viewer_pose_location
        rot = wm.xr_session_state.viewer_pose_rotation

        rotmat = Matrix.Identity(3)
        rotmat.rotate(rot)
        rotmat.resize_4x4()
        transmat = Matrix.Translation(loc)

        return transmat @ rotmat

    def setup(self, context):
        gizmo = self.gizmos.new(VIEW3D_GT_vr_camera_cone.bl_idname)
        gizmo.aspect = 1 / 3, 1 / 4

        gizmo.color = gizmo.color_highlight = 0.2, 0.6, 1.0
        gizmo.alpha = 1.0

        self.gizmo = gizmo

    def draw_prepare(self, context):
        self.gizmo.matrix_basis = self._get_viewer_pose_matrix(context)


class VIEW3D_GGT_vr_controller_poses(GizmoGroup):
    bl_idname = "VIEW3D_GGT_vr_controller_poses"
    bl_label = "VR Controller Poses Indicator"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'WINDOW'
    bl_options = {'3D', 'PERSISTENT', 'SCALE', 'VR_REDRAWS'}

    @classmethod
    def poll(cls, context):
        view3d = context.space_data
        return (
            view3d.shading.vr_show_controllers and
            bpy.types.XrSessionState.is_running(context) and
            not view3d.mirror_xr_session
        )

    @staticmethod
    def _get_controller_pose_matrix(context, idx, is_grip, scale):
        wm = context.window_manager

        loc = None
        rot = None
        if is_grip:
            loc = wm.xr_session_state.controller_grip_location_get(context, idx)
            rot = wm.xr_session_state.controller_grip_rotation_get(context, idx)
        else:
            loc = wm.xr_session_state.controller_aim_location_get(context, idx)
            rot = wm.xr_session_state.controller_aim_rotation_get(context, idx)

        rotmat = Matrix.Identity(3)
        rotmat.rotate(Quaternion(Vector(rot)))
        rotmat.resize_4x4()
        transmat = Matrix.Translation(loc)
        scalemat = Matrix.Scale(scale, 4)

        return transmat @ rotmat @ scalemat

    def setup(self, context):
        for idx in range(2):
            self.gizmos.new(VIEW3D_GT_vr_controller_grip.bl_idname)
            self.gizmos.new(VIEW3D_GT_vr_controller_aim.bl_idname)

        for gizmo in self.gizmos:
            gizmo.aspect = 1 / 3, 1 / 4
            gizmo.color_highlight = 1.0, 1.0, 1.0
            gizmo.alpha = 1.0

    def draw_prepare(self, context):
        grip_idx = 0
        aim_idx = 0
        idx = 0
        scale = 1.0
        for gizmo in self.gizmos:
            is_grip = (gizmo.bl_idname == VIEW3D_GT_vr_controller_grip.bl_idname)
            if (is_grip):
                idx = grip_idx
                grip_idx += 1
                scale = 0.1
            else:
                idx = aim_idx
                aim_idx += 1
                scale = 0.5
            gizmo.matrix_basis = self._get_controller_pose_matrix(context, idx, is_grip, scale)


class VIEW3D_GGT_vr_landmarks(GizmoGroup):
    bl_idname = "VIEW3D_GGT_vr_landmarks"
    bl_label = "VR Landmark Indicators"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'WINDOW'
    bl_options = {'3D', 'PERSISTENT', 'SCALE'}

    @classmethod
    def poll(cls, context):
        view3d = context.space_data
        return (
            view3d.shading.vr_show_landmarks
        )

    def setup(self, context):
        pass

    def draw_prepare(self, context):
        # first delete the old gizmos
        for g in self.gizmos:
            self.gizmos.remove(g)

        scene = context.scene
        landmarks = scene.vr_landmarks

        for lm in landmarks:
            if ((lm.type == 'SCENE_CAMERA' and not scene.camera) or
                    (lm.type == 'OBJECT' and not lm.base_pose_object)):
                continue

            gizmo = self.gizmos.new(VIEW3D_GT_vr_camera_cone.bl_idname)
            gizmo.aspect = 1 / 3, 1 / 4

            gizmo.color = gizmo.color_highlight = 0.2, 1.0, 0.6
            gizmo.alpha = 1.0

            self.gizmo = gizmo

            if lm.type == 'SCENE_CAMERA':
                cam = scene.camera
                lm_mat = cam.matrix_world if cam else Matrix.Identity(4)
            elif lm.type == 'OBJECT':
                lm_mat = lm.base_pose_object.matrix_world
            else:
                angle = lm.base_pose_angle
                raw_rot = Euler((radians(90.0), 0, angle))

                rotmat = Matrix.Identity(3)
                rotmat.rotate(raw_rot)
                rotmat.resize_4x4()

                transmat = Matrix.Translation(lm.base_pose_location)

                lm_mat = transmat @ rotmat

            self.gizmo.matrix_basis = lm_mat


class VIEW3D_GGT_vr_captures(GizmoGroup):
    bl_idname = "VIEW3D_GGT_vr_captures"
    bl_label = "VR Location Scouting Captures Indicators"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'WINDOW'
    bl_options = {'3D', 'DEPTH_3D', 'PERSISTENT', 'SCALE'}

    @staticmethod
    def compute_aspect(render_settings) -> tuple[float, float]:
        render_x = render_settings.resolution_x * render_settings.pixel_aspect_x
        render_y = render_settings.resolution_y * render_settings.pixel_aspect_y

        aspect_x = render_x / render_y if render_x < render_y else 1
        aspect_y = render_y / render_x if render_x > render_y else 1

        # Base aspect to match native Blender Camera Gizmo (using Auto Sensor Fit).
        base_aspect = 1 / 4
        return aspect_x * base_aspect, aspect_y * base_aspect

    @staticmethod
    def get_selection_color(context, is_active_capture) -> tuple[float, float, float]:
        selection_color = Color((0.25, 0.81, 1.0))

        # Shift the hue of the base theme selection color, decrease its saturation/value further for inactive captures.
        if not is_active_capture:
            selection_color.s += 0.2
            selection_color.v -= 0.5

        return selection_color[:3]

    @staticmethod
    def get_transform_matrix(capture):
        rot_mat = Matrix.Identity(3)
        rot_mat.rotate(capture.orientation)
        rot_mat.resize_4x4()
        loc_mat = Matrix.Translation(capture.location)

        return loc_mat @ rot_mat

    @classmethod
    def poll(cls, context):
        view3d = context.space_data
        return view3d.shading.vr_show_captures

    def setup(self, context):
        pass

    def draw_prepare(self, context):
        for g in self.gizmos:
            self.gizmos.remove(g)

        scene = context.scene

        for idx, capture in enumerate(scene.vr_captures):
            gizmo = self.gizmos.new(VIEW3D_GT_vr_camera_cone.bl_idname)
            gizmo.aspect = self.compute_aspect(scene.render)
            sensor_fit_fac = 36 * 2  # Twice the Blender default Camera sensor fit value (36mm).
            gizmo.focal = capture.lens_focal / sensor_fit_fac

            is_active_capture = (idx == scene.vr_captures_selected)
            color = self.get_selection_color(context, is_active_capture)

            gizmo.color = color
            gizmo.color_highlight = color
            gizmo.alpha = 1.0

            gizmo.matrix_basis = self.get_transform_matrix(capture)

            self.gizmo = gizmo


classes = (
    VIEW3D_OT_vr_landmark_add,
    VIEW3D_OT_vr_landmark_remove,
    VIEW3D_OT_vr_landmark_activate,
    VIEW3D_OT_vr_landmark_from_session,
    VIEW3D_OT_vr_camera_landmark_from_session,
    VIEW3D_OT_add_camera_from_vr_landmark,
    VIEW3D_OT_camera_to_vr_landmark,
    VIEW3D_OT_vr_landmark_from_camera,
    VIEW3D_OT_cursor_to_vr_landmark,
    VIEW3D_OT_update_vr_landmark,

    VIEW3D_OT_vr_location_scouting_viewfinder_capture,
    VIEW3D_OT_vr_location_scouting_viewfinder_apply_action,
    VIEW3D_OT_vr_location_scouting_add_camera_from_capture,
    VIEW3D_OT_vr_location_scouting_add_marker_from_capture,
    VIEW3D_OT_vr_location_scouting_active_camera_to_capture,
    VIEW3D_OT_vr_location_scouting_remove_capture,
    VIEW3D_OT_vr_location_scouting_browse_captures,
    VIEW3D_OT_vr_location_scouting_viewfinder_cycle_mode,
    VIEW3D_OT_vr_location_scouting_viewfinder_cycle_action,
    VIEW3D_OT_vr_location_scouting_viewfinder_swap_hands,

    VIEW3D_GT_vr_camera_cone,
    VIEW3D_GT_vr_controller_grip,
    VIEW3D_GT_vr_controller_aim,
    VIEW3D_GGT_vr_viewer_pose,
    VIEW3D_GGT_vr_controller_poses,
    VIEW3D_GGT_vr_landmarks,
    VIEW3D_GGT_vr_captures,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)
