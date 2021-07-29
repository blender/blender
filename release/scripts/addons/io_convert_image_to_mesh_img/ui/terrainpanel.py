# This file is a part of the HiRISE DTM Importer for Blender
#
# Copyright (C) 2017 Arizona Board of Regents on behalf of the Planetary Image
# Research Laboratory, Lunar and Planetary Laboratory at the University of
# Arizona.
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program.  If not, see <http://www.gnu.org/licenses/>.

"""Blender panel for managing a DTM *after* it's been imported"""

import bpy
from bpy.types import (
        Operator,
        Panel,
        )
from bpy.props import FloatProperty

from ..mesh.terrain import BTerrain
from ..mesh.dtm import DTM


class TerrainPanel(Panel):
    """Creates a Panel in the Object properties window for terrain objects"""
    bl_label = "Terrain Model"
    bl_idname = "OBJECT_PT_terrain"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "object"

    # Allow the user to specify a new resolution factor for reloading the
    # terrain data at. This is useful because it allows the user to stage
    # a scene with a low resolution terrain map, apply textures, modifiers,
    # etc. and then increase the resolution to prepare for final rendering.
    #
    # Displaying this value as a percentage (0, 100] is an intuitive way
    # for users to grasp what this value does. The DTM importer, however,
    # wants to recieve a value between (0, 1]. This is obviously a
    # straightforward conversion:
    #
    #     f(x) = x / 100
    #
    # But this conversion should happen here, in the terrain panel, rather
    # than in the DTM importing utility itself. We can't pass get/set
    # functions to the property itself because they result in a recursion
    # error. Instead, we use another, hidden, property to store the scaled
    # resolution.
    bpy.types.Object.dtm_resolution = FloatProperty(
        subtype="PERCENTAGE",
        name="New Resolution",
        description=(
            "Percentage scale for terrain model resolution. 100\% loads the "
            "model at full resolution (i.e. one vertex for each post in the "
            "original terrain model) and is *MEMORY INTENSIVE*. Downsampling "
            "uses Nearest Neighbors. The downsampling algorithm may need to "
            "alter the resolution you specify here to ensure it results in a "
            "whole number of vertices. If it needs to alter the value you "
            "specify, you are guaranteed that it will shrink it (i.e. "
            "decrease the DTM resolution"
        ),
        min=1.0, max=100.0, default=10.0
    )
    bpy.types.Object.scaled_dtm_resolution = FloatProperty(
        options={'HIDDEN'},
        name="Scaled Terrain Model Resolution",
        get=(lambda self: self.dtm_resolution / 100.0)
    )

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj and obj.get("IS_TERRAIN", False)

    def draw(self, context):
        obj = context.active_object
        layout = self.layout

        # User Controls
        layout.prop(obj, 'dtm_resolution')
        layout.operator("terrain.reload")

        # Metadata
        self.draw_metadata_panel(context)

    def draw_metadata_panel(self, context):
        """Display some metadata about the DTM"""
        obj = context.active_object
        layout = self.layout

        metadata_panel = layout.box()

        dtm_resolution = metadata_panel.row()
        dtm_resolution.label('Current Resolution: ')
        dtm_resolution.label('{:9,.2%}'.format(
            obj['DTM_RESOLUTION']
        ))

        mesh_scale = metadata_panel.row()
        mesh_scale.label('Current Scale: ')
        mesh_scale.label('{:9,.2f} m/post'.format(
            obj['MESH_SCALE']
        ))

        dtm_scale = metadata_panel.row()
        dtm_scale.label('Original Scale: ')
        dtm_scale.label('{:9,.2f} m/post'.format(
            obj['MAP_SCALE']
        ))

        return {'FINISHED'}


class ReloadTerrain(Operator):
    """Button for reloading the terrain mesh at a new resolution"""
    bl_idname = "terrain.reload"
    bl_label = "Reload Terrain"

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj and obj.get("IS_TERRAIN", False)

    def execute(self, context):
        # Reload the terrain
        obj = context.active_object
        path = obj['PATH']

        scaled_dtm_resolution = obj.scaled_dtm_resolution

        # Reload BTerrain with new DTM
        dtm = DTM(path, scaled_dtm_resolution)
        BTerrain.reload(obj, dtm)

        return {"FINISHED"}
