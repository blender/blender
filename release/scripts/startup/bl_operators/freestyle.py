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

import bpy

from bpy.props import (EnumProperty, StringProperty)


class SCENE_OT_freestyle_fill_range_by_selection(bpy.types.Operator):
    '''Fill the Range Min/Max entries by the min/max distance between selected mesh objects and the active camera.'''
    bl_idname = "scene.freestyle_fill_range_by_selection"
    bl_label = "Fill Range by Selection"

    type = EnumProperty(name="Type", description="Type of the modifier to work on.",
                        items=[("COLOR", "Color", "Color modifier type."),
                               ("ALPHA", "Alpha", "Alpha modifier type."),
                               ("THICKNESS", "Thickness", "Thickness modifier type.")])
    name = StringProperty(name="Name", description="Name of the modifier to work on.")

    def execute(self, context):
        rl = context.scene.render.layers.active
        lineset = rl.freestyle_settings.linesets.active
        linestyle = lineset.linestyle
        # Find the modifier to work on
        if self.type == 'COLOR':
            m = linestyle.color_modifiers[self.name]
        elif self.type == 'ALPHA':
            m = linestyle.alpha_modifiers[self.name]
        else:
            m = linestyle.thickness_modifiers[self.name]
        # Find the active camera
        camera = context.scene.camera
        # Find selected mesh objects
        selection = [ob for ob in context.scene.objects if ob.select and ob.type == 'MESH']
        if len(selection) > 0:
            # Compute the min/max distance between selected mesh objects and the camera
            min_dist = float('inf')
            max_dist = -min_dist
            for ob in selection:
                for vert in ob.data.vertices:
                    dist = (ob.matrix_world * vert.co - camera.location).length
                    min_dist = min(dist, min_dist)
                    max_dist = max(dist, max_dist)
            # Fill the Range Min/Max entries with the computed distances
            m.range_min = min_dist
            m.range_max = max_dist
        return {'FINISHED'}
