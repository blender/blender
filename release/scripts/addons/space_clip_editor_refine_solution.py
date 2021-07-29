# -*- coding:utf-8 -*-

# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#  All rights reserved.
#
# ##### END GPL LICENSE BLOCK #####

bl_info = {
    "name": "Refine tracking solution",
    "author": "Stephen Leger",
    "license": "GPL",
    "version": (1, 1, 3),
    "blender": (2, 7, 8),
    "location": "Clip Editor > Tools > Solve > Refine Solution",
    "description": "Refine motion solution by setting track weight according"
                   " to reprojection error",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/Scripts/"
                "Motion_Tracking/Refine_Track",
    "category": "Motion Tracking",
}

import bpy
from bpy.types import (
        Operator,
        Panel,
        )
from bpy.props import FloatProperty
from mathutils import Vector


class OP_Tracking_refine_solution(Operator):
    bl_idname = "tracking.refine_solution"
    bl_label = "Refine"
    bl_description = "Set track weight by error and solve camera motion"
    bl_options = {"UNDO"}

    @classmethod
    def poll(cls, context):
        return (context.area.spaces.active.clip is not None)

    def execute(self, context):
        error = context.window_manager.TrackingTargetError
        smooth = context.window_manager.TrackingSmooth
        clip = context.area.spaces.active.clip
        try:
            tracking = clip.tracking
            tracks = tracking.tracks
            winx = float(clip.size[0])
            winy = float(clip.size[1])
            aspy = 1.0 / tracking.camera.pixel_aspect
            start = tracking.reconstruction.cameras[0].frame
            end = tracking.reconstruction.cameras[-1].frame
        except:
            return {'CANCELLED'}

        marker_position = Vector()

        for frame in range(start, end):
            camera = tracking.reconstruction.cameras.find_frame(frame)
            if camera is not None:
                imat = camera.matrix.inverted()
                projection_matrix = imat.transposed()
            else:
                continue

            for track in tracking.tracks:
                marker = track.markers.find_frame(frame)
                if marker is None:
                    continue

                # weight incomplete tracks on start and end
                if frame > start + smooth and frame < end - smooth:
                    for m in track.markers:
                        if not m.mute:
                            tstart = m
                            break
                    for m in reversed(track.markers):
                        if not m.mute:
                            tend = m
                            break
                    dt = min(0.5 * (tend.frame - tstart.frame), smooth)
                    if dt > 0:
                        t0 = min(1.0, (frame - tstart.frame) / dt)
                        t1 = min(1.0, (tend.frame - frame) / dt)
                        tw = min(t0, t1)
                    else:
                        tw = 0.0
                else:
                    tw = 1.0

                reprojected_position = track.bundle * projection_matrix
                if reprojected_position.z == 0:
                    track.weight = 0
                    track.keyframe_insert("weight", frame=frame)
                    continue
                reprojected_position = reprojected_position / -reprojected_position.z * \
                                       tracking.camera.focal_length_pixels
                reprojected_position = Vector(
                                        (tracking.camera.principal[0] + reprojected_position[0],
                                        tracking.camera.principal[1] * aspy + reprojected_position[1], 0)
                                        )

                marker_position[0] = (marker.co[0] + track.offset[0]) * winx
                marker_position[1] = (marker.co[1] + track.offset[1]) * winy * aspy

                dp = marker_position - reprojected_position
                if dp.length == 0:
                    track.weight = 1.0
                else:
                    track.weight = min(1.0, tw * error / dp.length)
                track.keyframe_insert("weight", frame=frame)

        bpy.ops.clip.solve_camera('INVOKE_DEFAULT')

        return{'FINISHED'}


class OP_Tracking_reset_solution(Operator):
    bl_idname = "tracking.reset_solution"
    bl_label = "Reset"
    bl_description = "Reset track weight and solve camera motion"
    bl_options = {"UNDO"}

    @classmethod
    def poll(cls, context):
        return (context.area.spaces.active.clip is not None)

    def execute(self, context):
        clip = context.area.spaces.active.clip
        try:
            tracking = clip.tracking
            tracks = tracking.tracks
            start = tracking.reconstruction.cameras[0].frame
            end = tracking.reconstruction.cameras[-1].frame
        except:
            return {'CANCELLED'}

        start = tracking.reconstruction.cameras[0].frame
        end = tracking.reconstruction.cameras[-1].frame
        for frame in range(start, end):
            camera = tracking.reconstruction.cameras.find_frame(frame)
            if camera is None:
                continue
            for track in tracking.tracks:
                marker = track.markers.find_frame(frame)
                if marker is None:
                    continue
                track.weight = 1.0
                track.keyframe_insert("weight", frame=frame)

        bpy.ops.clip.solve_camera('INVOKE_DEFAULT')

        return{'FINISHED'}


class RefineMotionTrackingPanel(Panel):
    bl_label = "Refine solution"
    bl_space_type = "CLIP_EDITOR"
    bl_region_type = "TOOLS"
    bl_category = "Solve"

    @classmethod
    def poll(cls, context):
        return (context.area.spaces.active.clip is not None)

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.prop(context.window_manager, "TrackingTargetError", text="Target error")
        col.prop(context.window_manager, "TrackingSmooth", text="Smooth transition")
        sub_box = col.box()
        sub_box.scale_y = 0.25

        row = col.row(align=True)
        row.operator("tracking.refine_solution")
        row.operator("tracking.reset_solution")


def register():
    bpy.types.WindowManager.TrackingTargetError = FloatProperty(
            name="Target Error",
            description="Refine motion track target error",
            default=0.3,
            min=0.01
            )
    bpy.types.WindowManager.TrackingSmooth = FloatProperty(
            name="Smooth Transition",
            description="Smooth weight transition on start and end of incomplete tracks",
            default=25,
            min=1
            )
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)
    del bpy.types.WindowManager.TrackingTargetError
    del bpy.types.WindowManager.TrackingSmooth


if __name__ == "__main__":
    register()
