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

__all__ = (
    "add_object_align_init",
    "object_data_add",
)


import bpy
import mathutils


def add_object_align_init(context, operator):
    space_data = context.space_data
    if space_data.type != 'VIEW_3D':
        space_data = None

    # location
    if operator and operator.properties.is_property_set("location"):
        location = mathutils.Matrix.Translation(mathutils.Vector(operator.properties.location))
    else:
        if space_data:  # local view cursor is detected below
            location = mathutils.Matrix.Translation(space_data.cursor_location)
        else:
            location = mathutils.Matrix.Translation(context.scene.cursor_location)

        if operator:
            operator.properties.location = location.to_translation()

    # rotation
    view_align = (context.user_preferences.edit.object_align == 'VIEW')
    view_align_force = False
    if operator:
        if operator.properties.is_property_set("view_align"):
            view_align = view_align_force = operator.view_align
        else:
            operator.properties.view_align = view_align

    if operator and operator.properties.is_property_set("rotation") and not view_align_force:
        rotation = mathutils.Euler(operator.properties.rotation).to_matrix().to_4x4()
    else:
        if view_align and space_data:
            rotation = space_data.region_3d.view_matrix.to_3x3().inverted().to_4x4()
        else:
            rotation = mathutils.Matrix()

        # set the operator properties
        if operator:
            operator.properties.rotation = rotation.to_euler()

    return location * rotation


def object_data_add(context, obdata, operator=None):

    scene = context.scene

    # ugh, could be made nicer
    for ob in scene.objects:
        ob.select = False

    obj_new = bpy.data.objects.new(obdata.name, obdata)

    base = scene.objects.link(obj_new)
    base.select = True

    if context.space_data and context.space_data.type == 'VIEW_3D':
        base.layers_from_view(context.space_data)

    obj_new.matrix_world = add_object_align_init(context, operator)

    obj_act = scene.objects.active

    # XXX
    # caused because entering editmodedoes not add a empty undo slot!
    if context.user_preferences.edit.use_enter_edit_mode:
        if not (obj_act and obj_act.mode == 'EDIT' and obj_act.type == obj_new.type):
            _obdata = bpy.data.meshes.new(obdata.name)
            obj_act = bpy.data.objects.new(_obdata.name, _obdata)
            obj_act.matrix_world = obj_new.matrix_world
            scene.objects.link(obj_act)
            scene.objects.active = obj_act
            bpy.ops.object.mode_set(mode='EDIT')
            bpy.ops.ed.undo_push(message="Enter Editmode")  # need empty undo step
    # XXX

    if obj_act and obj_act.mode == 'EDIT' and obj_act.type == obj_new.type:
        bpy.ops.mesh.select_all(action='DESELECT')
        bpy.ops.object.mode_set(mode='OBJECT')

        obj_act.select = True
        scene.update()  # apply location
        #scene.objects.active = obj_new

        bpy.ops.object.join()  # join into the active.
        bpy.data.meshes.remove(obdata)

        bpy.ops.object.mode_set(mode='EDIT')
    else:
        scene.objects.active = obj_new
        if context.user_preferences.edit.use_enter_edit_mode:
            bpy.ops.object.mode_set(mode='EDIT')

    return base
