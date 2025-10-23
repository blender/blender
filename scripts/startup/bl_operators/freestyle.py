# SPDX-FileCopyrightText: 2011-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy

from bpy.app.translations import (
    pgettext_rpt as rpt_,
)
from bpy.types import (
    Operator,
)
from bpy.props import (
    BoolProperty,
    EnumProperty,
    StringProperty,
)


class SCENE_OT_freestyle_fill_range_by_selection(Operator):
    """Fill the Range Min/Max entries by the min/max distance between selected mesh objects and the source object """ \
        """(either a user-specified object or the active camera)"""
    bl_idname = "scene.freestyle_fill_range_by_selection"
    bl_label = "Fill Range by Selection"
    bl_options = {'INTERNAL'}

    type: EnumProperty(
        name="Type", description="Type of the modifier to work on",
        items=(
            ('COLOR', "Color", "Color modifier type"),
            ('ALPHA', "Alpha", "Alpha modifier type"),
            ('THICKNESS', "Thickness", "Thickness modifier type"),
        ),
    )
    name: StringProperty(
        name="Name",
        description="Name of the modifier to work on",
    )

    @classmethod
    def poll(cls, context):
        view_layer = context.view_layer
        return view_layer and view_layer.freestyle_settings.linesets.active

    def execute(self, context):
        import sys

        scene = context.scene
        view_layer = context.view_layer
        lineset = view_layer.freestyle_settings.linesets.active
        linestyle = lineset.linestyle
        # Find the modifier to work on
        if self.type == 'COLOR':
            m = linestyle.color_modifiers[self.name]
        elif self.type == 'ALPHA':
            m = linestyle.alpha_modifiers[self.name]
        else:
            m = linestyle.thickness_modifiers[self.name]
        # Find the reference object
        if m.type == 'DISTANCE_FROM_CAMERA':
            ref = scene.camera
            if ref is None:
                self.report({'ERROR'}, "No active camera in the scene")
                return {'CANCELLED'}
            matrix_to_camera = ref.matrix_world.inverted()
        elif m.type == 'DISTANCE_FROM_OBJECT':
            if m.target is None:
                self.report({'ERROR'}, "Target object not specified")
                return {'CANCELLED'}
            ref = m.target
            target_location = ref.location
        else:
            self.report({'ERROR'}, rpt_("Unexpected modifier type: {:s}").format(m.type))
            return {'CANCELLED'}
        # Find selected vertices in edit-mesh.
        ob = context.active_object
        if ob.type == 'MESH' and ob.mode == 'EDIT' and ob.name != ref.name:
            bpy.ops.object.mode_set(mode='OBJECT')
            selected_verts = [v for v in ob.data.vertices if v.select]
            bpy.ops.object.mode_set(mode='EDIT')
            # Compute the min/max distance from the reference to mesh vertices
            min_dist = sys.float_info.max
            max_dist = -min_dist
            if m.type == 'DISTANCE_FROM_CAMERA':
                ob_to_cam = matrix_to_camera @ ob.matrix_world
                for vert in selected_verts:
                    # dist in the camera space
                    dist = (ob_to_cam @ vert.co).length
                    min_dist = min(dist, min_dist)
                    max_dist = max(dist, max_dist)
            elif m.type == 'DISTANCE_FROM_OBJECT':
                for vert in selected_verts:
                    # dist in the world space
                    dist = (ob.matrix_world @ vert.co - target_location).length
                    min_dist = min(dist, min_dist)
                    max_dist = max(dist, max_dist)
            # Fill the Range Min/Max entries with the computed distances
            m.range_min = min_dist
            m.range_max = max_dist
            return {'FINISHED'}
        # Find selected mesh objects
        selection = [ob for ob in scene.objects if ob.select_get() and ob.type == 'MESH' and ob.name != ref.name]
        if selection:
            # Compute the min/max distance from the reference to mesh vertices
            min_dist = sys.float_info.max
            max_dist = -min_dist
            if m.type == 'DISTANCE_FROM_CAMERA':
                for ob in selection:
                    ob_to_cam = matrix_to_camera @ ob.matrix_world
                    for vert in ob.data.vertices:
                        # dist in the camera space
                        dist = (ob_to_cam @ vert.co).length
                        min_dist = min(dist, min_dist)
                        max_dist = max(dist, max_dist)
            elif m.type == 'DISTANCE_FROM_OBJECT':
                for ob in selection:
                    for vert in ob.data.vertices:
                        # dist in the world space
                        dist = (ob.matrix_world @ vert.co - target_location).length
                        min_dist = min(dist, min_dist)
                        max_dist = max(dist, max_dist)
            # Fill the Range Min/Max entries with the computed distances
            m.range_min = min_dist
            m.range_max = max_dist
        return {'FINISHED'}


class SCENE_OT_freestyle_add_edge_marks_to_keying_set(Operator):
    """Add the data paths to the Freestyle Edge Mark property of selected edges to the active keying set"""
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
                path = "attributes[\"freestyle_edge\"].data[{:d}].value".format(i)
                ks.paths.add(mesh, path, index=0)
        bpy.ops.object.mode_set(mode=ob_mode, toggle=False)
        return {'FINISHED'}


class SCENE_OT_freestyle_add_face_marks_to_keying_set(Operator):
    """Add the data paths to the Freestyle Face Mark property of selected polygons to the active keying set"""
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
                path = "attributes[\"freestyle_face\"].data[{:d}].value".format(i)
                ks.paths.add(mesh, path, index=0)
        bpy.ops.object.mode_set(mode=ob_mode, toggle=False)
        return {'FINISHED'}


class SCENE_OT_freestyle_module_open(Operator):
    """Open a style module file"""
    bl_idname = "scene.freestyle_module_open"
    bl_label = "Open Style Module File"
    bl_options = {'INTERNAL'}

    filepath: StringProperty(subtype='FILE_PATH')

    make_internal: BoolProperty(
        name="Make internal",
        description="Make module file internal after loading",
        default=True,
    )

    @classmethod
    def poll(cls, context):
        view_layer = context.view_layer
        return view_layer and view_layer.freestyle_settings.mode == 'SCRIPT'

    def invoke(self, context, _event):
        self.freestyle_module = context.freestyle_module
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def execute(self, _context):
        text = bpy.data.texts.load(self.filepath, internal=self.make_internal)
        self.freestyle_module.script = text
        return {'FINISHED'}


classes = (
    SCENE_OT_freestyle_add_edge_marks_to_keying_set,
    SCENE_OT_freestyle_add_face_marks_to_keying_set,
    SCENE_OT_freestyle_fill_range_by_selection,
    SCENE_OT_freestyle_module_open,
)
