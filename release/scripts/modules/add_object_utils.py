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

def add_object_data(obdata, context):

    scene = context.scene

    # ugh, could be made nicer
    for ob in scene.objects:
        ob.selected = False

    obj_new = bpy.data.objects.new(obdata.name, obdata)

    base = scene.objects.link(obj_new)
    base.selected = True

    if context.space_data and context.space_data.type == 'VIEW_3D':
        base.layers_from_view(context.space_data)

    # TODO, local view cursor!
    obj_new.location = scene.cursor_location

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
