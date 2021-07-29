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
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

"""
Usage:
    Select all items whose normals face a certain direction
Additional links:
    Author Site: http://www.macouno.com
    e-mail: dolf {at} macouno {dot} com
"""

import bpy
from bpy.types import Operator
from mathutils import Vector
from math import radians
from bpy.props import (
        FloatVectorProperty,
        FloatProperty,
        BoolProperty,
        EnumProperty,
        )


class Select_by_direction():

    # Initialise the class
    def __init__(self, context, direction, divergence, extend, space):

        self.ob = context.active_object
        bpy.ops.object.mode_set(mode='OBJECT')

        self.space = space

        # if we do stuff in global space we need to object matrix
        if self.space == 'GLO':
            # TODO: not sure if this is the correct way to solve the crash - lijenstina
            mat = self.ob.matrix_world
            mat_rot = mat.to_3x3().inverted()
            direction = mat_rot * Vector(direction)
        else:
            direction = Vector(direction)

        direction = direction.normalized()

        vertSelect = bpy.context.tool_settings.mesh_select_mode[0]
        edgeSelect = bpy.context.tool_settings.mesh_select_mode[1]
        faceSelect = bpy.context.tool_settings.mesh_select_mode[2]

        if Vector(direction).length:
            # Vert select
            if vertSelect:
                hasSelected = self.hasSelected(self.ob.data.vertices)

                for v in self.ob.data.vertices:
                    normal = v.normal
                    s = self.selectCheck(v.select, hasSelected, extend)
                    d = self.deselectCheck(v.select, hasSelected, extend)

                    if s or d:
                        angle = direction.angle(normal)

                    # Check if the verts match any of the directions
                    if s and angle <= divergence:
                        v.select = True

                    if d and angle > divergence:
                        v.select = False
            # Edge select
            if edgeSelect:
                hasSelected = self.hasSelected(self.ob.data.edges)

                for e in self.ob.data.edges:
                    s = self.selectCheck(e.select, hasSelected, extend)
                    d = self.deselectCheck(e.select, hasSelected, extend)

                    # Check if the edges match any of the directions
                    if s or d:
                        normal = self.ob.data.vertices[e.vertices[0]].normal
                        normal += self.ob.data.vertices[e.vertices[1]].normal

                        angle = direction.angle(normal)

                    if s and angle <= divergence:
                        e.select = True

                    if d and angle > divergence:
                        e.select = False

            # Face select
            if faceSelect:
                hasSelected = self.hasSelected(self.ob.data.polygons)

                # Loop through all the given faces
                for f in self.ob.data.polygons:
                    s = self.selectCheck(f.select, hasSelected, extend)
                    d = self.deselectCheck(f.select, hasSelected, extend)

                    if s or d:
                        angle = direction.angle(f.normal)

                    # Check if the faces match any of the directions
                    if s and angle <= divergence:
                        f.select = True

                    if d and angle > divergence:
                        f.select = False

        bpy.ops.object.mode_set(mode='EDIT')

    # See if the current item should be selected or not
    def selectCheck(self, isSelected, hasSelected, extend):
        # If the current item is not selected we may want to select
        if not isSelected:
            # If we are extending or nothing is selected we want to select
            if extend or not hasSelected:
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
    bl_idname = "mesh.select_by_direction"
    bl_label = "Select by direction"
    bl_description = ("Select all items with normals facing a certain direction,\n"
                      "defined by a vector with coordinates X, Y, Z")
    bl_options = {'REGISTER', 'UNDO'}

    direction = FloatVectorProperty(
            name="Direction",
            description="Define a vector from the inputs axis X, Y, Z\n"
                        "Used to define the normals direction",
            default=(0.0, 0.0, 1.0),
            min=-100.0, max=100.0,
            soft_min=-10.0, soft_max=10.0,
            step=100,
            precision=2
            )
    divergence = FloatProperty(
            name="Divergence",
            description="The number of degrees the selection may differ from the Vector\n"
                        "(Input is converted to radians)",
            default=radians(30.0),
            min=0.0, max=radians(360.0),
            soft_min=0.0, soft_max=radians(360.0),
            step=radians(5000),
            precision=2,
            subtype='ANGLE'
            )
    extend = BoolProperty(
            name="Extend",
            description="Extend the current selection",
            default=False
            )
    # The spaces we use
    spaces = (('LOC', 'Local', ''), ('GLO', 'Global', ''))
    space = EnumProperty(
            items=spaces,
            name="Space",
            description="The space to interpret the directions in",
            default='LOC'
            )

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH')

    def execute(self, context):
        Select_by_direction(context, self.direction, self.divergence, self.extend, self.space)

        return {'FINISHED'}
