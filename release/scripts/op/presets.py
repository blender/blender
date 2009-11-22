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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

import bpy
import os

from wm import AddPresetBase


class AddPresetRender(AddPresetBase):
    bl_idname = "render.preset_add"
    bl_label = "Add Render Preset"
    name = AddPresetBase.name

    preset_values = [
        "bpy.context.scene.render_data.resolution_x",
        "bpy.context.scene.render_data.resolution_y",
        "bpy.context.scene.render_data.pixel_aspect_x",
        "bpy.context.scene.render_data.pixel_aspect_y",
        "bpy.context.scene.render_data.fps",
        "bpy.context.scene.render_data.fps_base",
        "bpy.context.scene.render_data.resolution_percentage",
    ]

    preset_path = os.path.join("presets", "render")


class AddPresetSSS(AddPresetBase):
    bl_idname = "material.sss_preset_add"
    bl_label = "Add Render Preset"
    name = AddPresetBase.name

    preset_values = [
        "bpy.context.material.subsurface_scattering.back",
        "bpy.context.material.subsurface_scattering.color[0]",
        "bpy.context.material.subsurface_scattering.color[1]",
        "bpy.context.material.subsurface_scattering.color[2]",
        "bpy.context.material.subsurface_scattering.color_factor",
        "bpy.context.material.subsurface_scattering.error_tolerance",
        "bpy.context.material.subsurface_scattering.front",
        "bpy.context.material.subsurface_scattering.ior",
        "bpy.context.material.subsurface_scattering.radius[0]",
        "bpy.context.material.subsurface_scattering.radius[1]",
        "bpy.context.material.subsurface_scattering.radius[2]",
        "bpy.context.material.subsurface_scattering.scale",
        "bpy.context.material.subsurface_scattering.texture_factor",
    ]

    preset_path = os.path.join("presets", "sss")

bpy.ops.add(AddPresetRender)
bpy.ops.add(AddPresetSSS)


