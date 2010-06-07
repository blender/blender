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
import mathutils

def add_object_align_init(context, operator):

    if operator and operator.properties.is_property_set("location") and operator.properties.is_property_set("rotation"):
        location = mathutils.TranslationMatrix(mathutils.Vector(operator.properties.location))
        rotation = mathutils.Euler(operator.properties.rotation).to_matrix().resize4x4()
    else:
        # TODO, local view cursor!
        location = mathutils.TranslationMatrix(context.scene.cursor_location)

        if context.user_preferences.edit.object_align == 'VIEW' and context.space_data.type == 'VIEW_3D':
            rotation = context.space_data.region_3d.view_matrix.rotation_part().invert().resize4x4()
        else:
            rotation = mathutils.Matrix()

        # set the operator properties
        if operator:
            operator.properties.location = location.translation_part()
            operator.properties.rotation = rotation.to_euler()

    return location * rotation


def add_object_data(context, obdata, operator=None):

    scene = context.scene

    # ugh, could be made nicer
    for ob in scene.objects:
        ob.selected = False

    obj_new = bpy.data.objects.new(obdata.name, obdata)

    base = scene.objects.link(obj_new)
    base.selected = True

    if context.space_data and context.space_data.type == 'VIEW_3D':
        base.layers_from_view(context.space_data)


    obj_new.matrix = add_object_align_init(context, operator)

    obj_act = scene.objects.active

    if obj_act and obj_act.mode == 'EDIT' and obj_act.type == obj_new.type:
        bpy.ops.object.mode_set(mode='OBJECT')

        obj_act.selected = True
        scene.update() # apply location
        #scene.objects.active = obj_new

        bpy.ops.object.join() # join into the active.

        bpy.ops.object.mode_set(mode='EDIT')
    else:
        scene.objects.active = obj_new
        if context.user_preferences.edit.enter_edit_mode:
            bpy.ops.object.mode_set(mode='EDIT')

    return base
