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
from bpy.types import (
    Menu,
    Operator,
    WindowManager,
)
from bpy.props import (
    BoolProperty,
    StringProperty,
)

# For preset popover menu
WindowManager.preset_name = StringProperty(
    name="Preset Name",
    description="Name for new preset",
    default="New Preset"
)


class AddPresetBase:
    """Base preset class, only for subclassing
    subclasses must define
     - preset_values
     - preset_subdir """
    # bl_idname = "script.preset_base_add"
    # bl_label = "Add a Python Preset"

    # only because invoke_props_popup requires. Also do not add to search menu.
    bl_options = {'REGISTER', 'INTERNAL'}

    name: StringProperty(
        name="Name",
        description="Name of the preset, used to make the path name",
        maxlen=64,
        options={'SKIP_SAVE'},
    )
    remove_name: BoolProperty(
        default=False,
        options={'HIDDEN', 'SKIP_SAVE'},
    )
    remove_active: BoolProperty(
        default=False,
        options={'HIDDEN', 'SKIP_SAVE'},
    )

    @staticmethod
    def as_filename(name):  # could reuse for other presets

        # lazy init maketrans
        def maketrans_init():
            cls = AddPresetBase
            attr = "_as_filename_trans"

            trans = getattr(cls, attr, None)
            if trans is None:
                trans = str.maketrans({char: "_" for char in " !@#$%^&*(){}:\";'[]<>,.\\/?"})
                setattr(cls, attr, trans)
            return trans

        name = name.strip()
        name = bpy.path.display_name_to_filepath(name)
        trans = maketrans_init()
        # Strip surrounding "_" as they are displayed as spaces.
        return name.translate(trans).strip("_")

    def execute(self, context):
        import os
        from bpy.utils import is_path_builtin

        if hasattr(self, "pre_cb"):
            self.pre_cb(context)

        preset_menu_class = getattr(bpy.types, self.preset_menu)

        is_xml = getattr(preset_menu_class, "preset_type", None) == 'XML'
        is_preset_add = not (self.remove_name or self.remove_active)

        if is_xml:
            ext = ".xml"
        else:
            ext = ".py"

        name = self.name.strip() if is_preset_add else self.name

        if is_preset_add:
            if not name:
                return {'FINISHED'}

            # Reset preset name
            wm = bpy.data.window_managers[0]
            if name == wm.preset_name:
                wm.preset_name = 'New Preset'

            filename = self.as_filename(name)

            target_path = os.path.join("presets", self.preset_subdir)
            target_path = bpy.utils.user_resource('SCRIPTS',
                                                  target_path,
                                                  create=True)

            if not target_path:
                self.report({'WARNING'}, "Failed to create presets path")
                return {'CANCELLED'}

            filepath = os.path.join(target_path, filename) + ext

            if hasattr(self, "add"):
                self.add(context, filepath)
            else:
                print("Writing Preset: %r" % filepath)

                if is_xml:
                    import rna_xml
                    rna_xml.xml_file_write(context,
                                           filepath,
                                           preset_menu_class.preset_xml_map)
                else:

                    def rna_recursive_attr_expand(value, rna_path_step, level):
                        if isinstance(value, bpy.types.PropertyGroup):
                            for sub_value_attr in value.bl_rna.properties.keys():
                                if sub_value_attr == "rna_type":
                                    continue
                                sub_value = getattr(value, sub_value_attr)
                                rna_recursive_attr_expand(sub_value, "%s.%s" % (rna_path_step, sub_value_attr), level)
                        elif type(value).__name__ == "bpy_prop_collection_idprop":  # could use nicer method
                            file_preset.write("%s.clear()\n" % rna_path_step)
                            for sub_value in value:
                                file_preset.write("item_sub_%d = %s.add()\n" % (level, rna_path_step))
                                rna_recursive_attr_expand(sub_value, "item_sub_%d" % level, level + 1)
                        else:
                            # convert thin wrapped sequences
                            # to simple lists to repr()
                            try:
                                value = value[:]
                            except:
                                pass

                            file_preset.write("%s = %r\n" % (rna_path_step, value))

                    file_preset = open(filepath, 'w', encoding="utf-8")
                    file_preset.write("import bpy\n")

                    if hasattr(self, "preset_defines"):
                        for rna_path in self.preset_defines:
                            exec(rna_path)
                            file_preset.write("%s\n" % rna_path)
                        file_preset.write("\n")

                    for rna_path in self.preset_values:
                        value = eval(rna_path)
                        rna_recursive_attr_expand(value, rna_path, 1)

                    file_preset.close()

            preset_menu_class.bl_label = bpy.path.display_name(filename)

        else:
            if self.remove_active:
                name = preset_menu_class.bl_label

            # fairly sloppy but convenient.
            filepath = bpy.utils.preset_find(name,
                                             self.preset_subdir,
                                             ext=ext)

            if not filepath:
                filepath = bpy.utils.preset_find(name,
                                                 self.preset_subdir,
                                                 display_name=True,
                                                 ext=ext)

            if not filepath:
                return {'CANCELLED'}

            # Do not remove bundled presets
            if is_path_builtin(filepath):
                self.report({'WARNING'}, "Unable to remove default presets")
                return {'CANCELLED'}

            try:
                if hasattr(self, "remove"):
                    self.remove(context, filepath)
                else:
                    os.remove(filepath)
            except Exception as e:
                self.report({'ERROR'}, "Unable to remove preset: %r" % e)
                import traceback
                traceback.print_exc()
                return {'CANCELLED'}

            # XXX, stupid!
            preset_menu_class.bl_label = "Presets"

        if hasattr(self, "post_cb"):
            self.post_cb(context)

        return {'FINISHED'}

    def check(self, _context):
        self.name = self.as_filename(self.name.strip())

    def invoke(self, context, _event):
        if not (self.remove_active or self.remove_name):
            wm = context.window_manager
            return wm.invoke_props_dialog(self)
        else:
            return self.execute(context)


class ExecutePreset(Operator):
    """Execute a preset"""
    bl_idname = "script.execute_preset"
    bl_label = "Execute a Python Preset"

    filepath: StringProperty(
        subtype='FILE_PATH',
        options={'SKIP_SAVE'},
    )
    menu_idname: StringProperty(
        name="Menu ID Name",
        description="ID name of the menu this was called from",
        options={'SKIP_SAVE'},
    )

    def execute(self, context):
        from os.path import basename, splitext
        filepath = self.filepath

        # change the menu title to the most recently chosen option
        preset_class = getattr(bpy.types, self.menu_idname)
        preset_class.bl_label = bpy.path.display_name(basename(filepath), title_case=False)

        ext = splitext(filepath)[1].lower()

        if ext not in {".py", ".xml"}:
            self.report({'ERROR'}, "Unknown file type: %r" % ext)
            return {'CANCELLED'}

        if hasattr(preset_class, "reset_cb"):
            preset_class.reset_cb(context)

        if ext == ".py":
            try:
                bpy.utils.execfile(filepath)
            except Exception as ex:
                self.report({'ERROR'}, "Failed to execute the preset: " + repr(ex))

        elif ext == ".xml":
            import rna_xml
            rna_xml.xml_file_run(context,
                                 filepath,
                                 preset_class.preset_xml_map)

        if hasattr(preset_class, "post_cb"):
            preset_class.post_cb(context)

        return {'FINISHED'}


class AddPresetRender(AddPresetBase, Operator):
    """Add or remove a Render Preset"""
    bl_idname = "render.preset_add"
    bl_label = "Add Render Preset"
    preset_menu = "RENDER_PT_presets"

    preset_defines = [
        "scene = bpy.context.scene"
    ]

    preset_values = [
        "scene.render.fps",
        "scene.render.fps_base",
        "scene.render.pixel_aspect_x",
        "scene.render.pixel_aspect_y",
        "scene.render.resolution_percentage",
        "scene.render.resolution_x",
        "scene.render.resolution_y",
    ]

    preset_subdir = "render"


class AddPresetCamera(AddPresetBase, Operator):
    """Add or remove a Camera Preset"""
    bl_idname = "camera.preset_add"
    bl_label = "Add Camera Preset"
    preset_menu = "CAMERA_PT_presets"

    preset_defines = [
        "cam = bpy.context.camera"
    ]

    preset_subdir = "camera"

    use_focal_length: BoolProperty(
        name="Include Focal Length",
        description="Include focal length into the preset",
        options={'SKIP_SAVE'},
    )

    @property
    def preset_values(self):
        preset_values = [
            "cam.sensor_width",
            "cam.sensor_height",
            "cam.sensor_fit"
        ]
        if self.use_focal_length:
            preset_values.append("cam.lens")
            preset_values.append("cam.lens_unit")
        return preset_values


class AddPresetSafeAreas(AddPresetBase, Operator):
    """Add or remove a Safe Areas Preset"""
    bl_idname = "safe_areas.preset_add"
    bl_label = "Add Safe Area Preset"
    preset_menu = "SAFE_AREAS_PT_presets"

    preset_defines = [
        "safe_areas = bpy.context.scene.safe_areas"
    ]

    preset_values = [
        "safe_areas.title",
        "safe_areas.action",
        "safe_areas.title_center",
        "safe_areas.action_center",
    ]

    preset_subdir = "safe_areas"


class AddPresetCloth(AddPresetBase, Operator):
    """Add or remove a Cloth Preset"""
    bl_idname = "cloth.preset_add"
    bl_label = "Add Cloth Preset"
    preset_menu = "CLOTH_PT_presets"

    preset_defines = [
        "cloth = bpy.context.cloth"
    ]

    preset_values = [
        "cloth.settings.quality",
        "cloth.settings.mass",
        "cloth.settings.air_damping",
        "cloth.settings.bending_model",
        "cloth.settings.tension_stiffness",
        "cloth.settings.compression_stiffness",
        "cloth.settings.shear_stiffness",
        "cloth.settings.bending_stiffness",
        "cloth.settings.tension_damping",
        "cloth.settings.compression_damping",
        "cloth.settings.shear_damping",
        "cloth.settings.bending_damping",
    ]

    preset_subdir = "cloth"


class AddPresetFluid(AddPresetBase, Operator):
    """Add or remove a Fluid Preset"""
    bl_idname = "fluid.preset_add"
    bl_label = "Add Fluid Preset"
    preset_menu = "FLUID_PT_presets"

    preset_defines = [
        "fluid = bpy.context.fluid"
    ]

    preset_values = [
        "fluid.domain_settings.viscosity_base",
        "fluid.domain_settings.viscosity_exponent",
    ]

    preset_subdir = "fluid"


class AddPresetHairDynamics(AddPresetBase, Operator):
    """Add or remove a Hair Dynamics Preset"""
    bl_idname = "particle.hair_dynamics_preset_add"
    bl_label = "Add Hair Dynamics Preset"
    preset_menu = "PARTICLE_PT_hair_dynamics_presets"

    preset_defines = [
        "psys = bpy.context.particle_system",
        "cloth = bpy.context.particle_system.cloth",
        "settings = bpy.context.particle_system.cloth.settings",
        "collision = bpy.context.particle_system.cloth.collision_settings",
    ]

    preset_subdir = "hair_dynamics"

    preset_values = [
        "settings.quality",
        "settings.mass",
        "settings.bending_stiffness",
        "psys.settings.bending_random",
        "settings.bending_damping",
        "settings.air_damping",
        "settings.internal_friction",
        "settings.density_target",
        "settings.density_strength",
        "settings.voxel_cell_size",
        "settings.pin_stiffness",
    ]


class AddPresetTrackingCamera(AddPresetBase, Operator):
    """Add or remove a Tracking Camera Intrinsics Preset"""
    bl_idname = "clip.camera_preset_add"
    bl_label = "Add Camera Preset"
    preset_menu = "CLIP_PT_camera_presets"

    preset_defines = [
        "camera = bpy.context.edit_movieclip.tracking.camera"
    ]

    preset_subdir = "tracking_camera"

    use_focal_length: BoolProperty(
        name="Include Focal Length",
        description="Include focal length into the preset",
        options={'SKIP_SAVE'},
        default=True,
    )

    @property
    def preset_values(self):
        preset_values = [
            "camera.sensor_width",
            "camera.pixel_aspect",
            "camera.k1",
            "camera.k2",
            "camera.k3"
        ]
        if self.use_focal_length:
            preset_values.append("camera.units")
            preset_values.append("camera.focal_length")
        return preset_values


class AddPresetTrackingTrackColor(AddPresetBase, Operator):
    """Add or remove a Clip Track Color Preset"""
    bl_idname = "clip.track_color_preset_add"
    bl_label = "Add Track Color Preset"
    preset_menu = "CLIP_PT_track_color_presets"

    preset_defines = [
        "track = bpy.context.edit_movieclip.tracking.tracks.active"
    ]

    preset_values = [
        "track.color",
        "track.use_custom_color"
    ]

    preset_subdir = "tracking_track_color"


class AddPresetTrackingSettings(AddPresetBase, Operator):
    """Add or remove a motion tracking settings preset"""
    bl_idname = "clip.tracking_settings_preset_add"
    bl_label = "Add Tracking Settings Preset"
    preset_menu = "CLIP_PT_tracking_settings_presets"

    preset_defines = [
        "settings = bpy.context.edit_movieclip.tracking.settings"
    ]

    preset_values = [
        "settings.default_correlation_min",
        "settings.default_pattern_size",
        "settings.default_search_size",
        "settings.default_frames_limit",
        "settings.default_pattern_match",
        "settings.default_margin",
        "settings.default_motion_model",
        "settings.use_default_brute",
        "settings.use_default_normalization",
        "settings.use_default_mask",
        "settings.use_default_red_channel",
        "settings.use_default_green_channel",
        "settings.use_default_blue_channel",
        "settings.default_weight"
    ]

    preset_subdir = "tracking_settings"


class AddPresetNodeColor(AddPresetBase, Operator):
    """Add or remove a Node Color Preset"""
    bl_idname = "node.node_color_preset_add"
    bl_label = "Add Node Color Preset"
    preset_menu = "NODE_PT_node_color_presets"

    preset_defines = [
        "node = bpy.context.active_node"
    ]

    preset_values = [
        "node.color",
        "node.use_custom_color"
    ]

    preset_subdir = "node_color"


class AddPresetInterfaceTheme(AddPresetBase, Operator):
    """Add or remove a theme preset"""
    bl_idname = "wm.interface_theme_preset_add"
    bl_label = "Add Theme Preset"
    preset_menu = "USERPREF_MT_interface_theme_presets"
    preset_subdir = "interface_theme"


class AddPresetKeyconfig(AddPresetBase, Operator):
    """Add or remove a Key-config Preset"""
    bl_idname = "wm.keyconfig_preset_add"
    bl_label = "Add Keyconfig Preset"
    preset_menu = "USERPREF_MT_keyconfigs"
    preset_subdir = "keyconfig"

    def add(self, _context, filepath):
        bpy.ops.preferences.keyconfig_export(filepath=filepath)
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
    """Add or remove an Operator Preset"""
    bl_idname = "wm.operator_preset_add"
    bl_label = "Operator Preset"
    preset_menu = "WM_MT_operator_presets"

    operator: StringProperty(
        name="Operator",
        maxlen=64,
        options={'HIDDEN', 'SKIP_SAVE'},
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
        operator_rna = op.get_rna_type()
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

        # dummy 'default' menu item
        layout = self.layout
        layout.operator("wm.operator_defaults")
        layout.separator()

        Menu.draw_preset(self, context)

    @property
    def preset_subdir(self):
        return AddPresetOperator.operator_path(self.operator)

    preset_operator = "script.execute_preset"


class AddPresetGpencilBrush(AddPresetBase, Operator):
    """Add or remove grease pencil brush preset"""
    bl_idname = "scene.gpencil_brush_preset_add"
    bl_label = "Add Grease Pencil Brush Preset"
    preset_menu = "VIEW3D_PT_gpencil_brush_presets"

    preset_defines = [
        "brush = bpy.context.tool_settings.gpencil_paint.brush",
        "settings = brush.gpencil_settings"
    ]

    preset_values = [
        "settings.input_samples",
        "settings.active_smooth_factor",
        "settings.angle",
        "settings.angle_factor",
        "settings.use_settings_stabilizer",
        "brush.smooth_stroke_radius",
        "brush.smooth_stroke_factor",
        "settings.pen_smooth_factor",
        "settings.pen_smooth_steps",
        "settings.pen_subdivision_steps",
        "settings.use_settings_random",
        "settings.random_pressure",
        "settings.random_strength",
        "settings.uv_random",
        "settings.pen_jitter",
        "settings.use_jitter_pressure",
        "settings.trim",
    ]

    preset_subdir = "gpencil_brush"


class AddPresetGpencilMaterial(AddPresetBase, Operator):
    """Add or remove grease pencil material preset"""
    bl_idname = "scene.gpencil_material_preset_add"
    bl_label = "Add Grease Pencil Material Preset"
    preset_menu = "MATERIAL_PT_gpencil_material_presets"

    preset_defines = [
        "material = bpy.context.object.active_material",
        "gpcolor = material.grease_pencil"
    ]

    preset_values = [
        "gpcolor.mode",
        "gpcolor.stroke_style",
        "gpcolor.color",
        "gpcolor.stroke_image",
        "gpcolor.pixel_size",
        "gpcolor.mix_stroke_factor",
        "gpcolor.alignment_mode",
        "gpcolor.alignment_rotation",
        "gpcolor.fill_style",
        "gpcolor.fill_color",
        "gpcolor.fill_image",
        "gpcolor.gradient_type",
        "gpcolor.mix_color",
        "gpcolor.mix_factor",
        "gpcolor.flip",
        "gpcolor.texture_offset",
        "gpcolor.texture_scale",
        "gpcolor.texture_angle",
        "gpcolor.texture_clamp",
        "gpcolor.mix_factor",
        "gpcolor.show_stroke",
        "gpcolor.show_fill",
    ]

    preset_subdir = "gpencil_material"


classes = (
    AddPresetCamera,
    AddPresetCloth,
    AddPresetFluid,
    AddPresetHairDynamics,
    AddPresetInterfaceTheme,
    AddPresetKeyconfig,
    AddPresetNodeColor,
    AddPresetOperator,
    AddPresetRender,
    AddPresetSafeAreas,
    AddPresetTrackingCamera,
    AddPresetTrackingSettings,
    AddPresetTrackingTrackColor,
    AddPresetGpencilBrush,
    AddPresetGpencilMaterial,
    ExecutePreset,
    WM_MT_operator_presets,
)
