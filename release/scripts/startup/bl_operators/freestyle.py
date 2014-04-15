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

import sys
import bpy

from bpy.props import (BoolProperty, EnumProperty, StringProperty)


class SCENE_OT_freestyle_fill_range_by_selection(bpy.types.Operator):
    """Fill the Range Min/Max entries by the min/max distance between selected mesh objects and the source object """
    """(either a user-specified object or the active camera)"""
    bl_idname = "scene.freestyle_fill_range_by_selection"
    bl_label = "Fill Range by Selection"
    bl_options = {'INTERNAL'}

    type = EnumProperty(name="Type", description="Type of the modifier to work on",
                        items=(("COLOR", "Color", "Color modifier type"),
                               ("ALPHA", "Alpha", "Alpha modifier type"),
                               ("THICKNESS", "Thickness", "Thickness modifier type")))
    name = StringProperty(name="Name", description="Name of the modifier to work on")

    @classmethod
    def poll(cls, context):
        rl = context.scene.render.layers.active
        return rl and rl.freestyle_settings.linesets.active

    def execute(self, context):
        scene = context.scene
        rl = scene.render.layers.active
        lineset = rl.freestyle_settings.linesets.active
        linestyle = lineset.linestyle
        # Find the modifier to work on
        if self.type == 'COLOR':
            m = linestyle.color_modifiers[self.name]
        elif self.type == 'ALPHA':
            m = linestyle.alpha_modifiers[self.name]
        else:
            m = linestyle.thickness_modifiers[self.name]
        # Find the source object
        if m.type == 'DISTANCE_FROM_CAMERA':
            source = scene.camera
        elif m.type == 'DISTANCE_FROM_OBJECT':
            if m.target is None:
                self.report({'ERROR'}, "Target object not specified")
                return {'CANCELLED'}
            source = m.target
        else:
            self.report({'ERROR'}, "Unexpected modifier type: " + m.type)
            return {'CANCELLED'}
        # Find selected mesh objects
        selection = [ob for ob in scene.objects if ob.select and ob.type == 'MESH' and ob.name != source.name]
        if selection:
            # Compute the min/max distance between selected mesh objects and the source
            min_dist = sys.float_info.max
            max_dist = -min_dist
            for ob in selection:
                for vert in ob.data.vertices:
                    dist = (ob.matrix_world * vert.co - source.location).length
                    min_dist = min(dist, min_dist)
                    max_dist = max(dist, max_dist)
            # Fill the Range Min/Max entries with the computed distances
            m.range_min = min_dist
            m.range_max = max_dist
        return {'FINISHED'}


class SCENE_OT_freestyle_add_edge_marks_to_keying_set(bpy.types.Operator):
    '''Add the data paths to the Freestyle Edge Mark property of selected edges to the active keying set'''
    bl_idname = "scene.freestyle_add_edge_marks_to_keying_set"
    bl_label = "Add Edge Marks to Keying Set"
    bl_options = {'UNDO'}

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return (ob and ob.type == 'MESH')

    def execute(self, context):
        # active keying set
        scene = context.scene
        ks = scene.keying_sets.active
        if ks is None:
            ks = scene.keying_sets.new(idname="FreestyleEdgeMarkKeyingSet", name="Freestyle Edge Mark Keying Set")
            ks.bl_description = ""
        # add data paths to the keying set
        ob = context.active_object
        ob_mode = ob.mode
        mesh = ob.data
        bpy.ops.object.mode_set(mode='OBJECT', toggle=False)
        for i, edge in enumerate(mesh.edges):
            if not edge.hide and edge.select:
                path = 'edges[%d].use_freestyle_mark' % i
                ks.paths.add(mesh, path, index=0)
        bpy.ops.object.mode_set(mode=ob_mode, toggle=False)
        return {'FINISHED'}


class SCENE_OT_freestyle_add_face_marks_to_keying_set(bpy.types.Operator):
    '''Add the data paths to the Freestyle Face Mark property of selected polygons to the active keying set'''
    bl_idname = "scene.freestyle_add_face_marks_to_keying_set"
    bl_label = "Add Face Marks to Keying Set"
    bl_options = {'UNDO'}

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return (ob and ob.type == 'MESH')

    def execute(self, context):
        # active keying set
        scene = context.scene
        ks = scene.keying_sets.active
        if ks is None:
            ks = scene.keying_sets.new(idname="FreestyleFaceMarkKeyingSet", name="Freestyle Face Mark Keying Set")
            ks.bl_description = ""
        # add data paths to the keying set
        ob = context.active_object
        ob_mode = ob.mode
        mesh = ob.data
        bpy.ops.object.mode_set(mode='OBJECT', toggle=False)
        for i, polygon in enumerate(mesh.polygons):
            if not polygon.hide and polygon.select:
                path = 'polygons[%d].use_freestyle_mark' % i
                ks.paths.add(mesh, path, index=0)
        bpy.ops.object.mode_set(mode=ob_mode, toggle=False)
        return {'FINISHED'}


class SCENE_OT_freestyle_module_open(bpy.types.Operator):
    """Open a style module file"""
    bl_idname = "scene.freestyle_module_open"
    bl_label = "Open Style Module File"
    bl_options = {'INTERNAL'}

    filepath = StringProperty(subtype='FILE_PATH')

    make_internal = BoolProperty(
        name="Make internal",
        description="Make module file internal after loading",
        default=True)

    @classmethod
    def poll(cls, context):
        rl = context.scene.render.layers.active
        return rl and rl.freestyle_settings.mode == 'SCRIPT'

    def invoke(self, context, event):
        self.freestyle_module = context.freestyle_module
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def execute(self, context):
        text = bpy.data.texts.load(self.filepath, self.make_internal)
        self.freestyle_module.script = text
        return {'FINISHED'}
