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

class AddPresetBase(bpy.types.Operator):
    '''Base preset class, only for subclassing
    subclasses must define 
     - preset_values
     - preset_subdir '''
    bl_idname = "render.preset_add"
    bl_label = "Add Render Preset"

    name = bpy.props.StringProperty(name="Name", description="Name of the preset, used to make the path name", maxlen= 64, default= "")
    
    def _as_filename(self, name): # could reuse for other presets
        for char in " !@#$%^&*(){}:\";'[]<>,./?":
            name = name.replace('.', '_')
        return name.lower()

    def execute(self, context):

        if not self.properties.name:
            return ('FINISHED',)

        filename = self._as_filename(self.properties.name) + ".py"

        target_path = bpy.utils.preset_paths(self.preset_subdir)[0] # we need some way to tell the user and system preset path

        file_preset = open(os.path.join(target_path, filename), 'w')
        
        for rna_path in self.preset_values:
            file_preset.write("%s = %s\n" % (rna_path, eval(rna_path)))

        file_preset.close()

        return ('FINISHED',)

    def invoke(self, context, event):
        wm = context.manager
        wm.invoke_props_popup(self, event)
        return ('RUNNING_MODAL',)


class AddPresetRender(AddPresetBase):
    '''Add a Render Preset.'''
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

    preset_subdir = "render"


class AddPresetSSS(AddPresetBase):
    '''Add a Subsurface Scattering Preset.'''
    bl_idname = "material.sss_preset_add"
    bl_label = "Add SSS Preset"
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

    preset_subdir = "sss"
    
class AddPresetCloth(AddPresetBase):
    '''Add a Cloth Preset.'''
    bl_idname = "cloth.preset_add"
    bl_label = "Add Cloth Preset"
    name = AddPresetBase.name
    
    preset_values = [
        "bpy.context.cloth.settings.quality",
        "bpy.context.cloth.settings.mass",
        "bpy.context.cloth.settings.structural_stiffness",
        "bpy.context.cloth.settings.bending_stiffness",
        "bpy.context.cloth.settings.spring_damping",
        "bpy.context.cloth.settings.air_damping",
    ]
    
    preset_subdir = "cloth"

bpy.ops.add(AddPresetRender)
bpy.ops.add(AddPresetSSS)
bpy.ops.add(AddPresetCloth)

