# GPL # Originals by meta-androcto, Pablo Vazquez, Liero, Richard Wilks

import bpy
from bpy.types import Operator


def object_origin(width, height, depth):
    """
    This function takes inputs and returns vertex and face arrays.
    no actual mesh data creation is done here.
    """
    verts = [(+0.0, +0.0, +0.0)]
    faces = []

    # apply size
    for i, v in enumerate(verts):
        verts[i] = v[0] * width, v[1] * depth, v[2] * height

    return verts, faces


class AddVert(Operator):
    bl_idname = "mesh.primitive_vert_add"
    bl_label = "Single Vert"
    bl_description = "Add a Single Vertice to Edit Mode"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        mesh = bpy.data.meshes.new("Vert")
        mesh.vertices.add(1)

        from bpy_extras import object_utils
        object_utils.object_data_add(context, mesh, operator=None)
        bpy.ops.object.mode_set(mode='EDIT')

        return {'FINISHED'}


class AddEmptyVert(Operator):
    bl_idname = "mesh.primitive_emptyvert_add"
    bl_label = "Empty Object Origin"
    bl_description = "Add an Object Origin to Edit Mode"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        mesh = bpy.data.meshes.new("Vert")
        mesh.vertices.add(1)

        from bpy_extras import object_utils
        object_utils.object_data_add(context, mesh, operator=None)
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.delete(type='VERT')

        return {'FINISHED'}


def Add_Symmetrical_Empty():

    bpy.ops.mesh.primitive_plane_add(enter_editmode=True)

    sempty = bpy.context.object
    sempty.name = "SymmEmpty"

    # check if we have a mirror modifier, otherwise add
    if (sempty.modifiers and sempty.modifiers['Mirror']):
        pass
    else:
        bpy.ops.object.modifier_add(type='MIRROR')

    # Delete all!
    bpy.ops.mesh.select_all(action='TOGGLE')
    bpy.ops.mesh.select_all(action='TOGGLE')
    bpy.ops.mesh.delete(type='VERT')


def Add_Symmetrical_Vert():

    bpy.ops.mesh.primitive_plane_add(enter_editmode=True)

    sempty = bpy.context.object
    sempty.name = "SymmVert"

    # check if we have a mirror modifier, otherwise add
    if (sempty.modifiers and sempty.modifiers['Mirror']):
        pass
    else:
        bpy.ops.object.modifier_add(type='MIRROR')

    # Delete all!
    bpy.ops.mesh.select_all(action='TOGGLE')
    bpy.ops.mesh.select_all(action='TOGGLE')
    bpy.ops.mesh.merge(type='CENTER')


class AddSymmetricalEmpty(Operator):
    bl_idname = "mesh.primitive_symmetrical_empty_add"
    bl_label = "Add Symmetrical Object Origin"
    bl_description = "Object Origin with a Mirror Modifier for symmetrical modeling"
    bl_options = {'REGISTER', 'UNDO'}

    def draw(self, context):
        layout = self.layout
        mirror = bpy.context.object.modifiers['Mirror']

        layout.prop(mirror, "use_clip", text="Use Clipping")

        layout.label("Mirror Axis")
        row = layout.row(align=True)
        row.prop(mirror, "use_x")
        row.prop(mirror, "use_y")
        row.prop(mirror, "use_z")

    def execute(self, context):
        Add_Symmetrical_Empty()

        return {'FINISHED'}


class AddSymmetricalVert(Operator):
    bl_idname = "mesh.primitive_symmetrical_vert_add"
    bl_label = "Add Symmetrical Origin & Vert"
    bl_description = "Object Origin with a Mirror Modifier for symmetrical modeling"
    bl_options = {'REGISTER', 'UNDO'}

    def draw(self, context):
        layout = self.layout
        mirror = bpy.context.object.modifiers['Mirror']

        layout.prop(mirror, "use_clip", text="Use Clipping")

        layout.label("Mirror Axis")
        row = layout.row(align=True)
        row.prop(mirror, "use_x")
        row.prop(mirror, "use_y")
        row.prop(mirror, "use_z")

    def execute(self, context):
        Add_Symmetrical_Vert()

        return {'FINISHED'}
