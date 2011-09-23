# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy
import os
import shutil
from bpy.types import Operator


class CLIP_OT_track_to_empty(Operator):
    bl_idname = "clip.track_to_empty"
    bl_label = "2D Track to Empty"
    bl_options = {'UNDO', 'REGISTER'}

    @classmethod
    def poll(cls, context):
        if context.space_data.type != 'CLIP_EDITOR':
            return False

        sc = context.space_data
        clip = sc.clip

        return clip and clip.tracking.active_track

    def execute(self, context):
        sc = context.space_data
        clip = sc.clip
        track = clip.tracking.active_track
        constraint = None
        ob = None

        if track.name in bpy.data.objects:
            if bpy.data.objects[track.name].type == 'Empty':
                ob = bpy.data.objects[track.name]

        if  not ob:
            ob = bpy.data.objects.new(name=track.name, object_data=None)
            ob.select = True
            bpy.context.scene.objects.link(ob)
            bpy.context.scene.objects.active = ob

        for con in ob.constraints:
            if con.type == 'FOLLOW_TRACK':
                constraint = con
                break

        if constraint is None:
            constraint = ob.constraints.new(type='FOLLOW_TRACK')

        constraint.clip = sc.clip
        constraint.track = track.name
        constraint.reference = 'TRACK'

        return {'FINISHED'}


class CLIP_OT_bundles_to_mesh(Operator):
    bl_idname = "clip.bundles_to_mesh"
    bl_label = "Bundles to Mesh"
    bl_options = {'UNDO', 'REGISTER'}

    @classmethod
    def poll(cls, context):
        if context.space_data.type != 'CLIP_EDITOR':
            return False

        sc = context.space_data
        clip = sc.clip

        return clip

    def execute(self, context):
        sc = context.space_data
        clip = sc.clip

        mesh = bpy.data.meshes.new(name="Bundles")
        for track in clip.tracking.tracks:
            if track.has_bundle:
                mesh.vertices.add(1)
                mesh.vertices[-1].co = track.bundle

        ob = bpy.data.objects.new(name="Bundles", object_data=mesh)

        bpy.context.scene.objects.link(ob)

        return {'FINISHED'}


class CLIP_OT_delete_proxy(Operator):
    bl_idname = "clip.delete_proxy"
    bl_label = "Delete Proxy"
    bl_options = {'UNDO', 'REGISTER'}

    def invoke(self, context, event):
        wm = context.window_manager

        return wm.invoke_confirm(self, event)

    def execute(self, context):
        sc = context.space_data
        clip = sc.clip

        if clip.use_proxy_custom_directory:
            proxydir = clip.proxy.directory
        else:
            clipdir = os.path.dirname(clip.filepath)
            proxydir = os.path.join(clipdir, 'BL_proxy')

        clipfile = os.path.basename(clip.filepath)
        proxy = os.path.join(proxydir, clipfile)
        absproxy = bpy.path.abspath(proxy)

        if os.path.exists(absproxy):
            shutil.rmtree(absproxy)
        else:
            return {'CANCELLED'}

        # remove [custom] proxy directory if empty
        try:
            absdir = bpy.path.abspath(proxydir)
            os.rmdir(absdir)
        except OSError:
            pass

        return {'FINISHED'}
