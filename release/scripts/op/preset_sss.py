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

class AddPreset(bpy.types.Operator):
    '''Add a Subsurface Scattering Preset'''
    bl_idname = "sss.preset_add"
    bl_label = "Add Subsurface Scattering Preset"
    
    name = bpy.props.StringProperty(name="Name", description="Name of the preset, used to make the path name", maxlen= 64, default= "New Preset")
    
    _preset_values = [
        "bpy.context.active_object.active_material.subsurface_scattering.radius",
        "bpy.context.active_object.active_material.subsurface_scattering.color",
    ]
    
    _last_preset = "" # hack to avoid remaking
    
    def _as_filename(self, name): # could reuse for other presets
        for char in " !@#$%^&*(){}:\";'[]<>,./?":
            name = name.replace('.', '_')
        return name.lower()

    def execute(self, context):
        filename = self._as_filename(self.properties.name) + ".py"
        
        target_path = os.path.join(os.path.dirname(__file__), os.path.pardir, "presets", "sss", filename)
        print(target_path)
        file_preset = open(target_path, 'w')
        
        for rna_path in self._preset_values:
            file_preset.write("%s = %s\n" % (rna_path, eval(rna_path)))
        
        file_preset.close()
        
        return ('FINISHED',)

    def invoke(self, context, event):
        wm = context.manager
        wm.invoke_props_popup(self, event)
        return ('RUNNING_MODAL',)

bpy.ops.add(AddPreset)
