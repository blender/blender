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
import os


class AddPresetBase():
    '''Base preset class, only for subclassing
    subclasses must define
     - preset_values
     - preset_subdir '''
    # bl_idname = "script.preset_base_add"
    # bl_label = "Add a Python Preset"

    name = bpy.props.StringProperty(name="Name", description="Name of the preset, used to make the path name", maxlen=64, default="")

    def _as_filename(self, name):  # could reuse for other presets
        for char in " !@#$%^&*(){}:\";'[]<>,./?":
            name = name.replace('.', '_')
        return name.lower()

    def execute(self, context):

        if not self.name:
            return {'FINISHED'}

        filename = self._as_filename(self.name) + ".py"

        target_path = bpy.utils.preset_paths(self.preset_subdir)[0]  # we need some way to tell the user and system preset path

        filepath = os.path.join(target_path, filename)
        if getattr(self, "save_keyconfig", False):
            bpy.ops.wm.keyconfig_export(filepath=filepath, kc_name=self.name)
            file_preset = open(filepath, 'a')
            file_preset.write("wm.keyconfigs.active = kc\n\n")
        else:
            file_preset = open(filepath, 'w')
            file_preset.write("import bpy\n")

        for rna_path in self.preset_values:
            value = eval(rna_path)
            file_preset.write("%s = %s\n" % (rna_path, repr(value)))

        file_preset.close()

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        #crashes, TODO - fix
        #return wm.invoke_props_popup(self, event)

        wm.invoke_props_popup(self, event)
        return {'RUNNING_MODAL'}


class ExecutePreset(bpy.types.Operator):
    ''' Executes a preset '''
    bl_idname = "script.execute_preset"
    bl_label = "Execute a Python Preset"

    filepath = bpy.props.StringProperty(name="Path", description="Path of the Python file to execute", maxlen=512, default="")
    preset_name = bpy.props.StringProperty(name="Preset Name", description="Name of the Preset being executed", default="")
    menu_idname = bpy.props.StringProperty(name="Menu ID Name", description="ID name of the menu this was called from", default="")

    def execute(self, context):
        # change the menu title to the most recently chosen option
        preset_class = getattr(bpy.types, self.menu_idname)
        preset_class.bl_label = self.preset_name

        # execute the preset using script.python_file_run
        bpy.ops.script.python_file_run(filepath=self.filepath)
        return {'FINISHED'}


class AddPresetRender(AddPresetBase, bpy.types.Operator):
    '''Add a Render Preset'''
    bl_idname = "render.preset_add"
    bl_label = "Add Render Preset"
    name = AddPresetBase.name

    preset_values = [
        "bpy.context.scene.render.field_order",
        "bpy.context.scene.render.fps",
        "bpy.context.scene.render.fps_base",
        "bpy.context.scene.render.pixel_aspect_x",
        "bpy.context.scene.render.pixel_aspect_y",
        "bpy.context.scene.render.resolution_percentage",
        "bpy.context.scene.render.resolution_x",
        "bpy.context.scene.render.resolution_y",
        "bpy.context.scene.render.use_fields",
        "bpy.context.scene.render.use_fields_still",
    ]

    preset_subdir = "render"


class AddPresetSSS(AddPresetBase, bpy.types.Operator):
    '''Add a Subsurface Scattering Preset'''
    bl_idname = "material.sss_preset_add"
    bl_label = "Add SSS Preset"
    name = AddPresetBase.name

    preset_values = [
        "bpy.context.material.subsurface_scattering.back",
        "bpy.context.material.subsurface_scattering.color[0]",
        "bpy.context.material.subsurface_scattering.color[1]",
        "bpy.context.material.subsurface_scattering.color[2]",
        "bpy.context.material.subsurface_scattering.color_factor",
        "bpy.context.material.subsurface_scattering.error_threshold",
        "bpy.context.material.subsurface_scattering.front",
        "bpy.context.material.subsurface_scattering.ior",
        "bpy.context.material.subsurface_scattering.radius[0]",
        "bpy.context.material.subsurface_scattering.radius[1]",
        "bpy.context.material.subsurface_scattering.radius[2]",
        "bpy.context.material.subsurface_scattering.scale",
        "bpy.context.material.subsurface_scattering.texture_factor",
    ]

    preset_subdir = "sss"


class AddPresetCloth(AddPresetBase, bpy.types.Operator):
    '''Add a Cloth Preset'''
    bl_idname = "cloth.preset_add"
    bl_label = "Add Cloth Preset"
    name = AddPresetBase.name

    preset_values = [
        "bpy.context.cloth.settings.air_damping",
        "bpy.context.cloth.settings.bending_stiffness",
        "bpy.context.cloth.settings.mass",
        "bpy.context.cloth.settings.quality",
        "bpy.context.cloth.settings.spring_damping",
        "bpy.context.cloth.settings.structural_stiffness",
    ]

    preset_subdir = "cloth"


class AddPresetSunSky(AddPresetBase, bpy.types.Operator):
    '''Add a Sky & Atmosphere Preset'''
    bl_idname = "lamp.sunsky_preset_add"
    bl_label = "Add Sunsky Preset"
    name = AddPresetBase.name

    preset_values = [
        "bpy.context.object.data.sky.atmosphere_extinction",
        "bpy.context.object.data.sky.atmosphere_inscattering",
        "bpy.context.object.data.sky.atmosphere_turbidity",
        "bpy.context.object.data.sky.backscattered_light",
        "bpy.context.object.data.sky.horizon_brightness",
        "bpy.context.object.data.sky.spread",
        "bpy.context.object.data.sky.sun_brightness",
        "bpy.context.object.data.sky.sun_intensity",
        "bpy.context.object.data.sky.sun_size",
        "bpy.context.object.data.sky.use_sky_blend",
        "bpy.context.object.data.sky.use_sky_blend_type",
        "bpy.context.object.data.sky.use_sky_color_space",
        "bpy.context.object.data.sky.use_sky_exposure",
    ]

    preset_subdir = "sunsky"


class AddPresetInteraction(AddPresetBase, bpy.types.Operator):
    '''Add an Application Interaction Preset'''
    bl_idname = "wm.interaction_preset_add"
    bl_label = "Add Interaction Preset"
    name = AddPresetBase.name
    save_keyconfig = True

    preset_values = [
        "bpy.context.user_preferences.edit.use_drag_immediately",
        "bpy.context.user_preferences.edit.use_insertkey_xyz_to_rgb",
        "bpy.context.user_preferences.inputs.invert_mouse_wheel_zoom",
        "bpy.context.user_preferences.inputs.select_mouse",
        "bpy.context.user_preferences.inputs.use_emulate_numpad",
        "bpy.context.user_preferences.inputs.use_mouse_continuous",
        "bpy.context.user_preferences.inputs.use_mouse_emulate_3_button",
        "bpy.context.user_preferences.inputs.view_rotate_method",
        "bpy.context.user_preferences.inputs.view_zoom_axis",
        "bpy.context.user_preferences.inputs.view_zoom_method",
    ]

    preset_subdir = "interaction"


def register():
    pass


def unregister():
    pass

if __name__ == "__main__":
    register()
