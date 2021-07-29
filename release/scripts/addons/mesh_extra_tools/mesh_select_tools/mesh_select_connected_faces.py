# Copyright (C) 2011, Dolf Veenvliet
# Extrude a selection from a mesh multiple times

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
#  GNU General Public License for more details
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
#
# ##### END GPL LICENSE BLOCK #####

"""
Usage:
Launch from from "Select -> Connected faces"

Additional links:
    Author Site: http://www.macouno.com
    e-mail: dolf {at} macouno {dot} com
"""


import bpy
from bpy.types import Operator
from bpy.props import (
        IntProperty,
        BoolProperty,
        )


class Select_connected_faces():
    # Initialize the class
    def __init__(self, context, iterations, extend):

        self.ob = context.active_object
        bpy.ops.object.mode_set(mode='OBJECT')

        # Make a list of all selected vertices
        selVerts = [v.index for v in self.ob.data.vertices if v.select]
        hasSelected = self.hasSelected(self.ob.data.polygons)

        for i in range(iterations):
            nextVerts = []

            for f in self.ob.data.polygons:
                if self.selectCheck(f.select, hasSelected, extend):

                    for v in f.vertices:
                        if v in selVerts:
                            f.select = True

                    if f.select:
                        for v in f.vertices:
                            if v not in selVerts:
                                nextVerts.append(v)

                elif self.deselectCheck(f.select, hasSelected, extend):
                    for v in f.vertices:
                        if v in selVerts:
                            f.select = False

            selVerts = nextVerts

        bpy.ops.object.mode_set(mode='EDIT')

    # See if the current item should be selected or not
    def selectCheck(self, isSelected, hasSelected, extend):
        # If the current item is not selected we may want to select
        if not isSelected:
            return True

        return False

    # See if the current item should be deselected or not
    def deselectCheck(self, isSelected, hasSelected, extend):
        # If the current item is selected we may want to deselect
        if isSelected:
            # If something is selected and we're not extending we want to deselect
            if hasSelected and not extend:
                return True

        return False

    # See if there is at least one selected item
    def hasSelected(self, items):
        for item in items:
            if item.select:
                return True

        return False


class Select_init(Operator):
    bl_idname = "mesh.select_connected_faces"
    bl_label = "Select connected faces"
    bl_description = ("Select all faces connected to the current selection \n"
                      "Works only in Face Selection mode")
    bl_options = {'REGISTER', 'UNDO'}

    # Iterations
    iterations = IntProperty(
            name="Iterations",
            description="Run the selection the given number of times",
            default=1,
            min=0, max=300,
            soft_min=0, soft_max=100
            )
    extend = BoolProperty(
            name="Extend",
            description="Extend the current selection",
            default=False
            )

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH' and
                bpy.context.tool_settings.mesh_select_mode[0] is False and
                bpy.context.tool_settings.mesh_select_mode[1] is False and
                bpy.context.tool_settings.mesh_select_mode[2] is True)

    def execute(self, context):
        Select_connected_faces(context, self.iterations, self.extend)

        return {'FINISHED'}
