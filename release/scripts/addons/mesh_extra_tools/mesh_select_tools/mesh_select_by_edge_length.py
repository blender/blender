# mesh_select_by_edge_length.py Copyright (C) 2011, Dolf Veenvliet
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
    Launch from from "Select -> By edge length"
    Select all items whose scale/length/surface matches a certain edge length

Additional links:
    Author Site: http://www.macouno.com
    e-mail: dolf {at} macouno {dot} com
"""

import bpy
from bpy.props import (
        FloatProperty,
        BoolProperty,
        EnumProperty,
        )


class Select_by_edge_length():

    # Initialize the class
    def __init__(self, context, edgeLength, edgeSize, extend, space, start_new):

        if start_new:
            bpy.ops.mesh.select_all(action='DESELECT')

        self.ob = context.active_object
        bpy.ops.object.mode_set(mode='OBJECT')

        self.space = space
        self.obMat = self.ob.matrix_world

        bigger = (True if edgeSize == 'BIG' else False)
        smaller = (True if edgeSize == 'SMALL' else False)

        # We ignore vert selections completely
        edgeSelect = bpy.context.tool_settings.mesh_select_mode[1]
        faceSelect = bpy.context.tool_settings.mesh_select_mode[2]

        # Edge select
        if edgeSelect:
            hasSelected = self.hasSelected(self.ob.data.edges)

            for e in self.ob.data.edges:

                if self.selectCheck(e.select, hasSelected, extend):

                    lene = self.getEdgeLength(e.vertices)

                    if (lene == edgeLength or (bigger and lene >= edgeLength) or
                      (smaller and lene <= edgeLength)):
                        e.select = True

                if self.deselectCheck(e.select, hasSelected, extend):
                    lene = self.getEdgeLength(e.vertices)

                    if (lene != edgeLength and not (bigger and lene >= edgeLength) and
                       not (smaller and lene <= edgeLength)):
                        e.select = False

        # Face select
        if faceSelect:
            hasSelected = self.hasSelected(self.ob.data.polygons)

            # Loop through all the given faces
            for f in self.ob.data.polygons:

                # Check if the faces match any of the directions
                if self.selectCheck(f.select, hasSelected, extend):

                    mine, maxe = 0.0, 0.0

                    for i, e in enumerate(f.edge_keys):
                        lene = self.getEdgeLength(e)
                        if not i:
                            mine = lene
                            maxe = lene
                        elif lene < mine:
                            mine = lene
                        elif lene > maxe:
                            maxe = lene

                    if ((mine == edgeLength and maxe == edgeLength) or
                       (bigger and mine >= edgeLength) or
                       (smaller and maxe <= edgeLength)):

                        f.select = True

                if self.deselectCheck(f.select, hasSelected, extend):

                    mine, maxe = 0.0, 0.0

                    for i, e in enumerate(f.edge_keys):
                        lene = self.getEdgeLength(e)
                        if not i:
                            mine = lene
                            maxe = lene
                        elif lene < mine:
                            mine = lene
                        elif lene > maxe:
                            maxe = lene

                    if ((mine != edgeLength and maxe != edgeLength) and
                       not (bigger and mine >= edgeLength) and
                       not (smaller and maxe <= edgeLength)):

                        f.select = False

        bpy.ops.object.mode_set(mode='EDIT')

    # Get the lenght of an edge, by giving this function all verts (2) in the edge
    def getEdgeLength(self, verts):

        vec1 = self.ob.data.vertices[verts[0]].co
        vec2 = self.ob.data.vertices[verts[1]].co

        vec = vec1 - vec2

        if self.space == 'GLO':
            vec = self.obMat * vec

        return round(vec.length, 5)

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


class Select_init(bpy.types.Operator):
    bl_idname = "mesh.select_by_edge_length"
    bl_label = "Select by edge length"
    bl_description = ("Select all items whose scale/length/surface matches a certain edge length \n"
                      "Does not work in Vertex Select mode")
    bl_options = {'REGISTER', 'UNDO'}

    edgeLength = FloatProperty(
            name="Edge length",
            description="The comparison scale in Blender units",
            default=1.0,
            min=0.0, max=1000.0,
            soft_min=0.0, soft_max=100.0,
            step=100,
            precision=2
            )
    # Changed to Enum as two separate Booleans didn't make much sense
    sizes = (('SMALL', 'Smaller', "Select items smaller or equal the size setting"),
             ('BIG', 'Bigger', "Select items bigger or equal to the size setting"),
             ('EQUAL', 'Equal', "Select edges equal to the size setting"))
    edgeSize = EnumProperty(
            items=sizes,
            name="Edge comparison",
            description="Choose the relation to set edge lenght",
            default='EQUAL'
            )
    extend = BoolProperty(
            name="Extend",
            description="Extend the current selection",
            default=False
            )
    start_new = BoolProperty(
            name="Fresh Start",
            default=False,
            description="Start from no previous selection"
            )
    # The spaces we use
    spaces = (('LOC', 'Local', "Use Local space"),
              ('GLO', 'Global', "Use Global Space"))
    space = EnumProperty(
            items=spaces,
            name="Space",
            description="The space to interpret the directions in",
            default='LOC'
            )

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH' and not bpy.context.tool_settings.mesh_select_mode[0])

    def execute(self, context):
        Select_by_edge_length(context, self.edgeLength, self.edgeSize,
                              self.extend, self.space, self.start_new)

        return {'FINISHED'}
