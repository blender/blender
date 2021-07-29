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

"""Blender menu importer for loading a DTM"""

import bpy
from bpy.props import (
        BoolProperty,
        FloatProperty,
        StringProperty,
        )
from bpy_extras.io_utils import ImportHelper

from ..mesh.terrain import BTerrain
from ..mesh.dtm import DTM


class ImportHiRISETerrain(bpy.types.Operator, ImportHelper):
    """DTM Import Helper"""
    bl_idname = "import_mesh.pds_dtm"
    bl_label = "Import HiRISE Terrain Model"
    bl_options = {'UNDO'}

    filename_ext = ".img"
    filter_glob = StringProperty(
        options={'HIDDEN'},
        default="*.img"
    )

    # Allow the user to specify a resolution factor for loading the
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
    dtm_resolution = FloatProperty(
        subtype="PERCENTAGE",
        description=(
            "Percentage scale for terrain model resolution. 100\% loads the "
            "model at full resolution (i.e. one vertex for each post in the "
            "original terrain model) and is *MEMORY INTENSIVE*. Downsampling "
            "uses Nearest Neighbors. You will be able to increase the "
            "resolution of your mesh later, and still maintain all textures, "
            "transformations, modifiers, etc., so best practice is to start "
            "small. The downsampling algorithm may need to alter the "
            "resolution you specify here to ensure it results in a whole "
            "number of vertices. If it needs to alter the value you specify, "
            "you are guaranteed that it will shrink it (i.e. decrease the "
            "DTM resolution"
        ),
        name="Terrain Model Resolution",
        min=1.0, max=100.0, default=10.0
    )
    scaled_dtm_resolution = FloatProperty(
        options={'HIDDEN'},
        name="Scaled Terrain Model Resolution",
        get=(lambda self: self.dtm_resolution / 100)
    )

    # HiRISE DTMs are huge, but it can be nice to load them in at scale. Here,
    # we present the user with the option of setting up the Blender viewport
    # to avoid a couple of common pitfalls encountered when working with such
    # a large mesh.
    #
    # 1. The Blender viewport has a default clipping distance of 1km. HiRISE
    #    DTMs are often many kilometers in each direction. If this setting is
    #    not changed, an unsuspecting user may only see part (or even nothing
    #    at all) of the terrain. This option (true, by default) instructs
    #    Blender to change the clipping distance to something appropriate for
    #    the DTM, and scales the grid floor to have gridlines 1km apart,
    #    instead of 1m apart.
    should_setup_viewport = BoolProperty(
        description=(
            "Set up the Blender screen to try and avoid clipping the DTM "
            "and to make the grid floor larger. *WARNING* This will change "
            "clipping distances and the Blender grid floor, and will fit the "
            "DTM in the viewport"
        ),
        name="Setup Blender Scene", default=True
    )
    # 2. Blender's default units are dimensionless. This option instructs
    #    Blender to change its unit's dimension to meters.
    should_setup_units = BoolProperty(
        description=(
            "Set the Blender scene to use meters as its unit"
        ),
        name="Set Blender Units to Meters", default=True
    )

    def execute(self, context):
        """Runs when the "Import HiRISE Terrain Model" button is pressed"""
        filepath = bpy.path.ensure_ext(self.filepath, self.filename_ext)
        # Create a BTerrain from the DTM
        dtm = DTM(filepath, self.scaled_dtm_resolution)
        BTerrain.new(dtm)

        # Set up the Blender UI
        if self.should_setup_units:
            self._setup_units(context)
        if self.should_setup_viewport:
            self._setup_viewport(context)

        return {"FINISHED"}

    def _setup_units(self, context):
        """Sets up the Blender scene for viewing the DTM"""
        scene = bpy.context.scene

        # Set correct units
        scene.unit_settings.system = 'METRIC'
        scene.unit_settings.scale_length = 1.0

        return {'FINISHED'}

    def _setup_viewport(self, context):
        """Sets up the Blender screen to make viewing the DTM easier"""
        screen = bpy.context.screen

        # Fetch the 3D_VIEW Area
        for area in screen.areas:
            if area.type == 'VIEW_3D':
                space = area.spaces[0]
                # Adjust 3D View Properties
                # TODO: Can these be populated more intelligently?
                space.clip_end = 100000
                space.grid_scale = 1000
                space.grid_lines = 50

        # Fly to a nice view of the DTM
        self._view_dtm(context)

        return {'FINISHED'}

    def _view_dtm(self, context):
        """Sets up the Blender screen to make viewing the DTM easier"""
        screen = bpy.context.screen

        # Fetch the 3D_VIEW Area
        for area in screen.areas:
            if area.type == 'VIEW_3D':
                # Move the camera around in the viewport. This requires
                # a context override.
                for region in area.regions:
                    if region.type == 'WINDOW':
                        override = {
                            'area': area,
                            'region': region,
                            'edit_object': bpy.context.edit_object
                        }
                        # Center View on DTM (SHORTCUT: '.')
                        bpy.ops.view3d.view_selected(override)
                        # Move to 'TOP' viewport (SHORTCUT: NUMPAD7)
                        bpy.ops.view3d.viewnumpad(override, type='TOP')

        return {'FINISHED'}
