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

# <pep8-80 compliant>

import bpy
from bpy.types import Menu, Operator


class AddPresetBase():
    '''Base preset class, only for subclassing
    subclasses must define
     - preset_values
     - preset_subdir '''
    # bl_idname = "script.preset_base_add"
    # bl_label = "Add a Python Preset"
    bl_options = {'REGISTER'}  # only because invoke_props_popup requires.

    name = bpy.props.StringProperty(
            name="Name",
            description="Name of the preset, used to make the path name",
            maxlen=64,
            )
    remove_active = bpy.props.BoolProperty(
            default=False,
            options={'HIDDEN'},
            )

    @staticmethod
    def as_filename(name):  # could reuse for other presets
        for char in " !@#$%^&*(){}:\";'[]<>,.\\/?":
            name = name.replace(char, '_')
        return name.lower().strip()

    def execute(self, context):
        import os

        if hasattr(self, "pre_cb"):
            self.pre_cb(context)

        preset_menu_class = getattr(bpy.types, self.preset_menu)

        if not self.remove_active:
            name = self.name.strip()
            if not name:
                return {'FINISHED'}

            filename = self.as_filename(name)

            target_path = os.path.join("presets", self.preset_subdir)
            target_path = bpy.utils.user_resource('SCRIPTS',
                                                  target_path,
                                                  create=True)

            if not target_path:
                self.report({'WARNING'}, "Failed to create presets path")
                return {'CANCELLED'}

            filepath = os.path.join(target_path, filename) + ".py"

            if hasattr(self, "add"):
                self.add(context, filepath)
            else:
                print("Writing Preset: %r" % filepath)
                file_preset = open(filepath, 'w')
                file_preset.write("import bpy\n")

                if hasattr(self, "preset_defines"):
                    for rna_path in self.preset_defines:
                        exec(rna_path)
                        file_preset.write("%s\n" % rna_path)
                    file_preset.write("\n")

                for rna_path in self.preset_values:
                    value = eval(rna_path)
                    # convert thin wrapped sequences to simple lists to repr()
                    try:
                        value = value[:]
                    except:
                        pass

                    file_preset.write("%s = %r\n" % (rna_path, value))

                file_preset.close()

            preset_menu_class.bl_label = bpy.path.display_name(filename)

        else:
            preset_active = preset_menu_class.bl_label

            # fairly sloppy but convenient.
            filepath = bpy.utils.preset_find(preset_active, self.preset_subdir)

            if not filepath:
                filepath = bpy.utils.preset_find(preset_active,
                                                 self.preset_subdir,
                                                 display_name=True)

            if not filepath:
                return {'CANCELLED'}

            if hasattr(self, "remove"):
                self.remove(context, filepath)
            else:
                try:
                    os.remove(filepath)
                except:
                    import traceback
                    traceback.print_exc()

            # XXX, stupid!
            preset_menu_class.bl_label = "Presets"

        if hasattr(self, "post_cb"):
            self.post_cb(context)

        return {'FINISHED'}

    def check(self, context):
        self.name = self.as_filename(self.name.strip())

    def invoke(self, context, event):
        if not self.remove_active:
            wm = context.window_manager
            return wm.invoke_props_dialog(self)
        else:
            return self.execute(context)


class ExecutePreset(Operator):
    '''Execute a preset'''
    bl_idname = "script.execute_preset"
    bl_label = "Execute a Python Preset"

    filepath = bpy.props.StringProperty(
            name="Path",
            description="Path of the Python file to execute",
            maxlen=512,
            )
    menu_idname = bpy.props.StringProperty(
            name="Menu ID Name",
            description="ID name of the menu this was called from",
            )

    def execute(self, context):
        from os.path import basename, splitext
        filepath = self.filepath

        # change the menu title to the most recently chosen option
        preset_class = getattr(bpy.types, self.menu_idname)
        preset_class.bl_label = bpy.path.display_name(basename(filepath))

        ext = splitext(filepath)[1].lower()

        # execute the preset using script.python_file_run
        if ext == ".py":
            bpy.ops.script.python_file_run(filepath=filepath)
        elif ext == ".xml":
            import rna_xml
            rna_xml.xml_file_run(context,
                                 filepath,
                                 preset_class.preset_xml_map)
        else:
            self.report({'ERROR'}, "unknown filetype: %r" % ext)
            return {'CANCELLED '}

        return {'FINISHED'}


class AddPresetRender(AddPresetBase, Operator):
    '''Add a Render Preset'''
    bl_idname = "render.preset_add"
    bl_label = "Add Render Preset"
    preset_menu = "RENDER_MT_presets"

    preset_defines = [
        "scene = bpy.context.scene"
    ]

    preset_values = [
        "scene.render.field_order",
        "scene.render.fps",
        "scene.render.fps_base",
        "scene.render.pixel_aspect_x",
        "scene.render.pixel_aspect_y",
        "scene.render.resolution_percentage",
        "scene.render.resolution_x",
        "scene.render.resolution_y",
        "scene.render.use_fields",
        "scene.render.use_fields_still",
    ]

    preset_subdir = "render"


class AddPresetCamera(AddPresetBase, Operator):
    '''Add a Camera Preset'''
    bl_idname = "camera.preset_add"
    bl_label = "Add Camera Preset"
    preset_menu = "CAMERA_MT_presets"

    preset_defines = [
        "cam = bpy.context.object.data"
    ]

    preset_values = [
        "cam.sensor_width",
        "cam.sensor_height",
        "cam.sensor_fit"
    ]

    preset_subdir = "camera"


class AddPresetSSS(AddPresetBase, Operator):
    '''Add a Subsurface Scattering Preset'''
    bl_idname = "material.sss_preset_add"
    bl_label = "Add SSS Preset"
    preset_menu = "MATERIAL_MT_sss_presets"

    preset_defines = [
        ("material = "
         "bpy.context.material.active_node_material "
         "if bpy.context.material.active_node_material "
         "else bpy.context.material")
    ]

    preset_values = [
        "material.subsurface_scattering.back",
        "material.subsurface_scattering.color",
        "material.subsurface_scattering.color_factor",
        "material.subsurface_scattering.error_threshold",
        "material.subsurface_scattering.front",
        "material.subsurface_scattering.ior",
        "material.subsurface_scattering.radius",
        "material.subsurface_scattering.scale",
        "material.subsurface_scattering.texture_factor",
    ]

    preset_subdir = "sss"


class AddPresetCloth(AddPresetBase, Operator):
    '''Add a Cloth Preset'''
    bl_idname = "cloth.preset_add"
    bl_label = "Add Cloth Preset"
    preset_menu = "CLOTH_MT_presets"

    preset_defines = [
        "cloth = bpy.context.cloth"
    ]

    preset_values = [
        "cloth.settings.air_damping",
        "cloth.settings.bending_stiffness",
        "cloth.settings.mass",
        "cloth.settings.quality",
        "cloth.settings.spring_damping",
        "cloth.settings.structural_stiffness",
    ]

    preset_subdir = "cloth"


class AddPresetSunSky(AddPresetBase, Operator):
    '''Add a Sky & Atmosphere Preset'''
    bl_idname = "lamp.sunsky_preset_add"
    bl_label = "Add Sunsky Preset"
    preset_menu = "LAMP_MT_sunsky_presets"

    preset_defines = [
        "sky = bpy.context.object.data.sky"
    ]

    preset_values = [
        "sky.atmosphere_extinction",
        "sky.atmosphere_inscattering",
        "sky.atmosphere_turbidity",
        "sky.backscattered_light",
        "sky.horizon_brightness",
        "sky.spread",
        "sky.sun_brightness",
        "sky.sun_intensity",
        "sky.sun_size",
        "sky.sky_blend",
        "sky.sky_blend_type",
        "sky.sky_color_space",
        "sky.sky_exposure",
    ]

    preset_subdir = "sunsky"


class AddPresetInteraction(AddPresetBase, Operator):
    '''Add an Application Interaction Preset'''
    bl_idname = "wm.interaction_preset_add"
    bl_label = "Add Interaction Preset"
    preset_menu = "USERPREF_MT_interaction_presets"

    preset_defines = [
        "user_preferences = bpy.context.user_preferences"
    ]

    preset_values = [
        "user_preferences.edit.use_drag_immediately",
        "user_preferences.edit.use_insertkey_xyz_to_rgb",
        "user_preferences.inputs.invert_mouse_zoom",
        "user_preferences.inputs.select_mouse",
        "user_preferences.inputs.use_emulate_numpad",
        "user_preferences.inputs.use_mouse_continuous",
        "user_preferences.inputs.use_mouse_emulate_3_button",
        "user_preferences.inputs.view_rotate_method",
        "user_preferences.inputs.view_zoom_axis",
        "user_preferences.inputs.view_zoom_method",
    ]

    preset_subdir = "interaction"


class AddPresetTrackingCamera(AddPresetBase, Operator):
    '''Add a Tracking Camera Intrinsics  Preset'''
    bl_idname = "clip.camera_preset_add"
    bl_label = "Add Camera Preset"
    preset_menu = "CLIP_MT_camera_presets"

    preset_defines = [
        "camera = bpy.context.edit_movieclip.tracking.camera"
    ]

    preset_values = [
        "camera.sensor_width",
        "camera.units",
        "camera.focal_length",
        "camera.pixel_aspect",
        "camera.k1",
        "camera.k2",
        "camera.k3"
    ]

    preset_subdir = "tracking_camera"


class AddPresetTrackingTrackColor(AddPresetBase, Operator):
    '''Add a Clip Track Color Preset'''
    bl_idname = "clip.track_color_preset_add"
    bl_label = "Add Track Color Preset"
    preset_menu = "CLIP_MT_track_color_presets"

    preset_defines = [
        "track = bpy.context.edit_movieclip.tracking.tracks.active"
    ]

    preset_values = [
        "track.color",
        "track.use_custom_color"
    ]

    preset_subdir = "tracking_track_color"


class AddPresetTrackingSettings(AddPresetBase, Operator):
    '''Add a motion tracking settings preset'''
    bl_idname = "clip.tracking_settings_preset_add"
    bl_label = "Add Tracking Settings Preset"
    preset_menu = "CLIP_MT_tracking_settings_presets"

    preset_defines = [
        "settings = bpy.context.edit_movieclip.tracking.settings"
    ]

    preset_values = [
        "settings.default_tracker",
        "settings.default_pyramid_levels",
        "settings.default_correlation_min",
        "settings.default_pattern_size",
        "settings.default_search_size",
        "settings.default_frames_limit",
        "settings.default_pattern_match",
        "settings.default_margin"
    ]

    preset_subdir = "tracking_settings"


class AddPresetKeyconfig(AddPresetBase, Operator):
    '''Add a Keyconfig Preset'''
    bl_idname = "wm.keyconfig_preset_add"
    bl_label = "Add Keyconfig Preset"
    preset_menu = "USERPREF_MT_keyconfigs"
    preset_subdir = "keyconfig"

    def add(self, context, filepath):
        bpy.ops.wm.keyconfig_export(filepath=filepath)
        bpy.utils.keyconfig_set(filepath)

    def pre_cb(self, context):
        keyconfigs = bpy.context.window_manager.keyconfigs
        if self.remove_active:
            preset_menu_class = getattr(bpy.types, self.preset_menu)
            preset_menu_class.bl_label = keyconfigs.active.name

    def post_cb(self, context):
        keyconfigs = bpy.context.window_manager.keyconfigs
        if self.remove_active:
            keyconfigs.remove(keyconfigs.active)


class AddPresetOperator(AddPresetBase, Operator):
    '''Add an Application Interaction Preset'''
    bl_idname = "wm.operator_preset_add"
    bl_label = "Operator Preset"
    preset_menu = "WM_MT_operator_presets"

    operator = bpy.props.StringProperty(
            name="Operator",
            maxlen=64,
            options={'HIDDEN'},
            )

    preset_defines = [
        "op = bpy.context.active_operator",
    ]

    @property
    def preset_subdir(self):
        return AddPresetOperator.operator_path(self.operator)

    @property
    def preset_values(self):
        properties_blacklist = Operator.bl_rna.properties.keys()

        prefix, suffix = self.operator.split("_OT_", 1)
        op = getattr(getattr(bpy.ops, prefix.lower()), suffix)
        operator_rna = op.get_rna().bl_rna
        del op

        ret = []
        for prop_id, prop in operator_rna.properties.items():
            if not (prop.is_hidden or prop.is_skip_save):
                if prop_id not in properties_blacklist:
                    ret.append("op.%s" % prop_id)

        return ret

    @staticmethod
    def operator_path(operator):
        import os
        prefix, suffix = operator.split("_OT_", 1)
        return os.path.join("operator", "%s.%s" % (prefix.lower(), suffix))


class WM_MT_operator_presets(Menu):
    bl_label = "Operator Presets"

    def draw(self, context):
        self.operator = context.active_operator.bl_idname
        Menu.draw_preset(self, context)

    @property
    def preset_subdir(self):
        return AddPresetOperator.operator_path(self.operator)

    preset_operator = "script.execute_preset"
