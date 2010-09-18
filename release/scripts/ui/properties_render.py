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


class RENDER_MT_presets(bpy.types.Menu):
    bl_label = "Render Presets"
    preset_subdir = "render"
    preset_operator = "script.execute_preset"
    draw = bpy.types.Menu.draw_preset


class RENDER_MT_ffmpeg_presets(bpy.types.Menu):
    bl_label = "FFMPEG Presets"
    preset_subdir = "ffmpeg"
    preset_operator = "script.python_file_run"
    draw = bpy.types.Menu.draw_preset


class RenderButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return (context.scene and rd.use_game_engine is False) and (rd.engine in cls.COMPAT_ENGINES)


class RENDER_PT_render(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Render"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        split = layout.split()

        col = split.column()
        col.operator("render.render", text="Image", icon='RENDER_STILL')

        col = split.column()
        col.operator("render.render", text="Animation", icon='RENDER_ANIMATION').animation = True

        layout.prop(rd, "display_mode", text="Display")


class RENDER_PT_layers(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Layers"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render

        row = layout.row()
        row.template_list(rd, "layers", rd.layers, "active_index", rows=2)

        col = row.column(align=True)
        col.operator("scene.render_layer_add", icon='ZOOMIN', text="")
        col.operator("scene.render_layer_remove", icon='ZOOMOUT', text="")

        rl = rd.layers.active

        if rl:
            layout.prop(rl, "name")

        split = layout.split()

        col = split.column()
        col.prop(scene, "layers", text="Scene")
        col.label(text="")
        col.prop(rl, "light_override", text="Light")
        col.prop(rl, "material_override", text="Material")

        col = split.column()
        col.prop(rl, "layers", text="Layer")
        col.label(text="Mask Layers:")
        col.prop(rl, "layers_zmask", text="")

        layout.separator()
        layout.label(text="Include:")

        split = layout.split()

        col = split.column()
        col.prop(rl, "use_zmask")
        row = col.row()
        row.prop(rl, "invert_zmask", text="Negate")
        row.active = rl.use_zmask
        col.prop(rl, "use_all_z")

        col = split.column()
        col.prop(rl, "use_solid")
        col.prop(rl, "use_halo")
        col.prop(rl, "use_ztransp")
        col.prop(rl, "use_sky")
		
        col = split.column()
        col.prop(rl, "use_edge_enhance")
        col.prop(rl, "use_strand")
        col.prop(rl, "use_freestyle")

        layout.separator()

        split = layout.split()

        col = split.column()
        col.label(text="Passes:")
        col.prop(rl, "use_pass_combined")
        col.prop(rl, "use_pass_z")
        col.prop(rl, "use_pass_vector")
        col.prop(rl, "use_pass_normal")
        col.prop(rl, "use_pass_uv")
        col.prop(rl, "use_pass_mist")
        col.prop(rl, "use_pass_object_index")
        col.prop(rl, "use_pass_color")

        col = split.column()
        col.label()
        col.prop(rl, "use_pass_diffuse")
        row = col.row()
        row.prop(rl, "use_pass_specular")
        row.prop(rl, "exclude_specular", text="")
        row = col.row()
        row.prop(rl, "use_pass_shadow")
        row.prop(rl, "exclude_shadow", text="")
        row = col.row()
        row.prop(rl, "use_pass_emit")
        row.prop(rl, "exclude_emit", text="")
        row = col.row()
        row.prop(rl, "use_pass_ambient_occlusion")
        row.prop(rl, "exclude_ambient_occlusion", text="")
        row = col.row()
        row.prop(rl, "use_pass_environment")
        row.prop(rl, "exclude_environment", text="")
        row = col.row()
        row.prop(rl, "use_pass_indirect")
        row.prop(rl, "exclude_indirect", text="")
        row = col.row()
        row.prop(rl, "use_pass_reflection")
        row.prop(rl, "exclude_reflection", text="")
        row = col.row()
        row.prop(rl, "use_pass_refraction")
        row.prop(rl, "exclude_refraction", text="")


class RENDER_PT_freestyle(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Freestyle"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        rl = rd.layers.active
        return rl and rl.use_freestyle

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        rl = rd.layers.active
        freestyle = rl.freestyle_settings

        split = layout.split()

        col = split.column()
        col.prop(freestyle, "mode", text="Control Mode")

        if freestyle.mode == "EDITOR":
            col.label(text="Edge Detection Options:")
            col.prop(freestyle, "crease_angle")
            col.prop(freestyle, "sphere_radius")
            col.prop(freestyle, "kr_derivative_epsilon")

            lineset = freestyle.linesets.active

            col.label(text="Line Sets:")
            row = col.row()
            rows = 2
            if lineset:
                rows = 5
            row.template_list(freestyle, "linesets", freestyle.linesets, "active_index", rows=rows)

            sub = row.column()
            subsub = sub.column(align=True)
            subsub.operator("scene.freestyle_lineset_add", icon='ZOOMIN', text="")
            subsub.operator("scene.freestyle_lineset_remove", icon='ZOOMOUT', text="")
            if lineset:
                sub.separator()
                subsub = sub.column(align=True)
                subsub.operator("scene.freestyle_lineset_move", icon='TRIA_UP', text="").direction = 'UP'
                subsub.operator("scene.freestyle_lineset_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

            if lineset:
                col.prop(lineset, "name")
                col.prop(lineset, "select_by_visibility")
                col.prop(lineset, "select_by_edge_types")

                if lineset.select_by_visibility:
                    col.label(text="Visibility:")
                    sub = col.row(align=True)
                    sub.prop(lineset, "visibility", expand=True)
                    if lineset.visibility == "RANGE":
                        sub = col.row(align=True)
                        sub.prop(lineset, "qi_start")
                        sub.prop(lineset, "qi_end")

                if lineset.select_by_edge_types:
                    col.label(text="Edge Types:")
                    row = col.row()
                    row.prop(lineset, "edge_type_negation", expand=True)
                    row = col.row()
                    row.prop(lineset, "edge_type_combination", expand=True)

                    row = col.row()
                    sub = row.column()
                    sub.prop(lineset, "select_silhouette")
                    sub.prop(lineset, "select_border")
                    sub.prop(lineset, "select_crease")
                    sub.prop(lineset, "select_ridge")
                    sub.prop(lineset, "select_valley")
                    sub.prop(lineset, "select_suggestive_contour")
                    sub.prop(lineset, "select_material_boundary")
                    sub = row.column()
                    sub.prop(lineset, "select_contour")
                    sub.prop(lineset, "select_external_contour")

        else: # freestyle.mode == "SCRIPT"

            col.prop(freestyle, "crease_angle")
            col.prop(freestyle, "sphere_radius")
            col.prop(freestyle, "use_ridges_and_valleys")
            col.prop(freestyle, "use_suggestive_contours")
            sub = col.row()
            sub.prop(freestyle, "kr_derivative_epsilon")
            sub.active = freestyle.use_suggestive_contours
            col.prop(freestyle, "use_material_boundaries")
            col.operator("scene.freestyle_module_add")

            for i, module in enumerate(freestyle.modules):
                    box = layout.box()
                    box.context_pointer_set("freestyle_module", module)
                    row = box.row(align=True)
                    row.prop(module, "is_displayed", text="")
                    row.prop(module, "module_path", text="")
                    row.operator("scene.freestyle_module_remove", icon='X', text="")
                    row.operator("scene.freestyle_module_move", icon='TRIA_UP', text="").direction = 'UP'
                    row.operator("scene.freestyle_module_move", icon='TRIA_DOWN', text="").direction = 'DOWN'


class RENDER_PT_freestyle_linestyle(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Freestyle: Line Style"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        rl = rd.layers.active
        if rl and rl.use_freestyle:
            freestyle = rl.freestyle_settings
            return freestyle.mode == "EDITOR" and freestyle.linesets.active
        return False

    def draw_modifier_box_header(self, box, modifier):
        row = box.row()
        row.context_pointer_set("modifier", modifier)
        if modifier.expanded:
            icon = "TRIA_DOWN"
        else:
            icon = "TRIA_RIGHT"
        row.operator("scene.freestyle_modifier_toggle_fold", icon=icon, text="", emboss=False)
        row.label(text=modifier.rna_type.name)
        row.prop(modifier, "name", text="")
        row.prop(modifier, "enabled", text="")
        sub = row.row(align=True)
        sub.operator("scene.freestyle_modifier_move", icon='TRIA_UP', text="").direction = 'UP'
        sub.operator("scene.freestyle_modifier_move", icon='TRIA_DOWN', text="").direction = 'DOWN'
        row.operator("scene.freestyle_modifier_remove", icon='X', text="")

    def draw_color_modifier(self, context, modifier):
        layout = self.layout

        col = layout.column(align=True)
        self.draw_modifier_box_header(col.box(), modifier)
        if modifier.expanded:
            box = col.box()
            row = box.row()
            row.prop(modifier, "blend", text="")
            row.prop(modifier, "influence")
            if modifier.type == "DISTANCE_FROM_OBJECT":
                box.prop(modifier, "target")
            box.template_color_ramp(modifier, "color_ramp", expand=True)
            if modifier.type not in ["ALONG_STROKE"]:
                row = box.row(align=True)
                row.prop(modifier, "range_min")
                row.prop(modifier, "range_max")

    def draw_alpha_modifier(self, context, modifier):
        layout = self.layout

        col = layout.column(align=True)
        self.draw_modifier_box_header(col.box(), modifier)
        if modifier.expanded:
            box = col.box()
            row = box.row()
            row.prop(modifier, "blend", text="")
            row.prop(modifier, "influence")
            if modifier.type == "DISTANCE_FROM_OBJECT":
                box.prop(modifier, "target")
            row = box.row()
            row.prop(modifier, "mapping", text="")
            sub = row.column()
            sub.prop(modifier, "invert")
            if modifier.mapping == "CURVE":
                sub.enabled = False
                box.template_curve_mapping(modifier, "curve")
            if modifier.type not in ["ALONG_STROKE"]:
                row = box.row(align=True)
                row.prop(modifier, "range_min")
                row.prop(modifier, "range_max")

    def draw_thickness_modifier(self, context, modifier):
        layout = self.layout

        col = layout.column(align=True)
        self.draw_modifier_box_header(col.box(), modifier)
        if modifier.expanded:
            box = col.box()
            row = box.row()
            row.prop(modifier, "blend", text="")
            row.prop(modifier, "influence")
            if modifier.type == "DISTANCE_FROM_OBJECT":
                box.prop(modifier, "target")
            row = box.row()
            row.prop(modifier, "mapping", text="")
            sub = row.column()
            sub.prop(modifier, "invert")
            if modifier.mapping == "CURVE":
                sub.enabled = False
                box.template_curve_mapping(modifier, "curve")
            if modifier.type not in ["ALONG_STROKE"]:
                row = box.row(align=True)
                row.prop(modifier, "range_min")
                row.prop(modifier, "range_max")
            row = box.row(align=True)
            row.prop(modifier, "value_min")
            row.prop(modifier, "value_max")

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        rl = rd.layers.active
        lineset = rl.freestyle_settings.linesets.active
        linestyle = lineset.linestyle

        split = layout.split()
        col = split.column()
        col.template_ID(lineset, "linestyle", new="scene.freestyle_linestyle_new")

        col.separator()
        sub = col.row(align=True)
        sub.prop(linestyle, "panel", expand=True)

        if linestyle.panel == "COLOR":
            col.label(text="Base Color:")
            col.prop(linestyle, "color", text="")
            col.label(text="Modifiers:")
            layout.operator_menu_enum("scene.freestyle_color_modifier_add", "type", text="Add Modifier")
            for modifier in linestyle.color_modifiers:
                self.draw_color_modifier(context, modifier)
        elif linestyle.panel == "ALPHA":
            col.label(text="Base Transparency:")
            col.prop(linestyle, "alpha")
            col.label(text="Modifiers:")
            layout.operator_menu_enum("scene.freestyle_alpha_modifier_add", "type", text="Add Modifier")
            for modifier in linestyle.alpha_modifiers:
                self.draw_alpha_modifier(context, modifier)
        elif linestyle.panel == "THICKNESS":
            col.label(text="Base Thickness:")
            col.prop(linestyle, "thickness")
            col.label(text="Modifiers:")
            layout.operator_menu_enum("scene.freestyle_thickness_modifier_add", "type", text="Add Modifier")
            for modifier in linestyle.thickness_modifiers:
                self.draw_thickness_modifier(context, modifier)
        elif linestyle.panel == "STROKES":
            pass
        elif linestyle.panel == "DISTORT":
            pass
        elif linestyle.panel == "MISC":
            pass


class RENDER_PT_shading(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Shading"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        split = layout.split()

        col = split.column()
        col.prop(rd, "use_textures", text="Textures")
        col.prop(rd, "use_shadows", text="Shadows")
        col.prop(rd, "use_sss", text="Subsurface Scattering")
        col.prop(rd, "use_envmaps", text="Environment Map")

        col = split.column()
        col.prop(rd, "use_raytrace", text="Ray Tracing")
        col.prop(rd, "use_color_management")
        col.prop(rd, "alpha_mode", text="Alpha")


class RENDER_PT_performance(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Performance"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        split = layout.split()

        col = split.column()
        col.label(text="Threads:")
        col.row().prop(rd, "threads_mode", expand=True)
        sub = col.column()
        sub.enabled = rd.threads_mode == 'FIXED'
        sub.prop(rd, "threads")
        sub = col.column(align=True)
        sub.label(text="Tiles:")
        sub.prop(rd, "parts_x", text="X")
        sub.prop(rd, "parts_y", text="Y")

        col = split.column()
        col.label(text="Memory:")
        sub = col.column()
        sub.enabled = not (rd.use_border or rd.use_full_sample)
        sub.prop(rd, "use_save_buffers")
        sub = col.column()
        sub.active = rd.use_compositing
        sub.prop(rd, "use_free_image_textures")
        sub = col.column()
        sub.active = rd.use_raytrace
        sub.label(text="Acceleration structure:")
        sub.prop(rd, "raytrace_method", text="")
        if rd.raytrace_method == 'OCTREE':
            sub.prop(rd, "octree_resolution", text="Resolution")
        else:
            sub.prop(rd, "use_instances", text="Instances")
        sub.prop(rd, "use_local_coords", text="Local Coordinates")


class RENDER_PT_post_processing(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Post Processing"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        split = layout.split()

        col = split.column()
        col.prop(rd, "use_compositing")
        col.prop(rd, "use_sequencer")

        col = split.column()
        col.prop(rd, "dither_intensity", text="Dither", slider=True)

        layout.separator()

        split = layout.split()

        col = split.column()
        col.prop(rd, "use_fields", text="Fields")
        sub = col.column()
        sub.active = rd.use_fields
        sub.row().prop(rd, "field_order", expand=True)
        sub.prop(rd, "use_fields_still", text="Still")

        col = split.column()
        col.prop(rd, "use_edge_enhance")
        sub = col.column()
        sub.active = rd.use_edge_enhance
        sub.prop(rd, "edge_threshold", text="Threshold", slider=True)
        sub.prop(rd, "edge_color", text="")

        layout.separator()

        split = layout.split()
        col = split.column()
        col.prop(rd, "use_freestyle", text="Freestyle")


class RENDER_PT_output(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Output"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        file_format = rd.file_format

        layout.prop(rd, "filepath", text="")

        split = layout.split()
        col = split.column()
        col.prop(rd, "file_format", text="")
        col.row().prop(rd, "color_mode", text="Color", expand=True)

        col = split.column()
        col.prop(rd, "use_file_extension")
        col.prop(rd, "use_overwrite")
        col.prop(rd, "use_placeholder")

        if file_format in ('AVI_JPEG', 'JPEG'):
            split = layout.split()
            split.prop(rd, "file_quality", slider=True)

        if file_format == 'PNG':
            split = layout.split()
            split.prop(rd, "file_quality", slider=True, text="Compression")

        elif file_format == 'MULTILAYER':
            split = layout.split()

            col = split.column()
            col.label(text="Codec:")
            col.prop(rd, "exr_codec", text="")
            col = split.column()

        elif file_format == 'OPEN_EXR':
            split = layout.split()

            col = split.column()
            col.label(text="Codec:")
            col.prop(rd, "exr_codec", text="")

            subsplit = split.split()
            col = subsplit.column()
            col.prop(rd, "use_exr_half")
            col.prop(rd, "exr_zbuf")

            col = subsplit.column()
            col.prop(rd, "exr_preview")

        elif file_format == 'JPEG2000':
            split = layout.split()
            col = split.column()
            col.label(text="Depth:")
            col.row().prop(rd, "jpeg2k_depth", expand=True)

            col = split.column()
            col.prop(rd, "jpeg2k_preset", text="")
            col.prop(rd, "jpeg2k_ycc")

        elif file_format in ('CINEON', 'DPX'):
            split = layout.split()
            col = split.column()
            col.prop(rd, "use_cineon_log", text="Convert to Log")

            col = split.column(align=True)
            col.active = rd.use_cineon_log
            col.prop(rd, "cineon_black", text="Black")
            col.prop(rd, "cineon_white", text="White")
            col.prop(rd, "cineon_gamma", text="Gamma")

        elif file_format == 'TIFF':
            split = layout.split()
            split.prop(rd, "use_tiff_16bit")

        elif file_format == 'QUICKTIME_CARBON':
            split = layout.split()
            split.operator("scene.render_data_set_quicktime_codec")

        elif file_format == 'QUICKTIME_QTKIT':
            split = layout.split()
            col = split.column()
            col.prop(rd, "quicktime_codec_type", text="Video Codec")
            col.prop(rd, "quicktime_codec_spatial_quality", text="Quality")

            # Audio
            col.prop(rd, "quicktime_audiocodec_type", text="Audio Codec")
            if rd.quicktime_audiocodec_type != 'No audio':
                split = layout.split()
                col = split.column()
                if rd.quicktime_audiocodec_type == 'LPCM':
                    col.prop(rd, "quicktime_audio_bitdepth", text="")

                col = split.column()
                col.prop(rd, "quicktime_audio_samplerate", text="")

                split = layout.split()
                col = split.column()
                if rd.quicktime_audiocodec_type == 'AAC':
                    col.prop(rd, "quicktime_audio_bitrate")

                subsplit = split.split()
                col = subsplit.column()

                if rd.quicktime_audiocodec_type == 'AAC':
                    col.prop(rd, "quicktime_audio_codec_isvbr")

                col = subsplit.column()
                col.prop(rd, "quicktime_audio_resampling_hq")


class RENDER_PT_encoding(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Encoding"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return rd.file_format in ('FFMPEG', 'XVID', 'H264', 'THEORA')

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        layout.menu("RENDER_MT_ffmpeg_presets", text="Presets")

        split = layout.split()

        col = split.column()
        col.prop(rd, "ffmpeg_format")
        if rd.ffmpeg_format in ('AVI', 'QUICKTIME', 'MKV', 'OGG'):
            col = split.column()
            col.prop(rd, "ffmpeg_codec")
        else:
            split.label()

        split = layout.split()

        col = split.column()
        col.prop(rd, "ffmpeg_video_bitrate")

        col = split.column()
        col.prop(rd, "ffmpeg_gopsize")

        split = layout.split()

        col = split.column()
        col.label(text="Rate:")
        col.prop(rd, "ffmpeg_minrate", text="Minimum")
        col.prop(rd, "ffmpeg_maxrate", text="Maximum")
        col.prop(rd, "ffmpeg_buffersize", text="Buffer")

        col = split.column()

        col.prop(rd, "ffmpeg_autosplit")
        col.label(text="Mux:")
        col.prop(rd, "ffmpeg_muxrate", text="Rate")
        col.prop(rd, "ffmpeg_packetsize", text="Packet Size")

        # Audio:
        sub = layout.column()

        if rd.ffmpeg_format not in ('MP3', ):
            sub.prop(rd, "ffmpeg_audio_codec", text="Audio Codec")

        sub.separator()

        split = sub.split()

        col = split.column()
        col.prop(rd, "ffmpeg_audio_bitrate")
        col.prop(rd, "ffmpeg_audio_mixrate")

        col = split.column()
        col.prop(rd, "ffmpeg_audio_volume", slider=True)


class RENDER_PT_antialiasing(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Anti-Aliasing"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        rd = context.scene.render

        self.layout.prop(rd, "use_antialiasing", text="")

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        layout.active = rd.use_antialiasing

        split = layout.split()

        col = split.column()
        col.row().prop(rd, "antialiasing_samples", expand=True)
        sub = col.row()
        sub.enabled = not rd.use_border
        sub.prop(rd, "use_full_sample")

        col = split.column()
        col.prop(rd, "pixel_filter_type", text="")
        col.prop(rd, "filter_size", text="Size")


class RENDER_PT_motion_blur(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Full Sample Motion Blur"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        rd = context.scene.render

        self.layout.prop(rd, "use_motion_blur", text="")

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        layout.active = rd.use_motion_blur

        row = layout.row()
        row.prop(rd, "motion_blur_samples")
        row.prop(rd, "motion_blur_shutter")


class RENDER_PT_dimensions(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Dimensions"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render

        row = layout.row(align=True)
        row.menu("RENDER_MT_presets", text=bpy.types.RENDER_MT_presets.bl_label)
        row.operator("render.preset_add", text="", icon="ZOOMIN")
        row.operator("render.preset_add", text="", icon="ZOOMOUT").remove_active = True
        split = layout.split()

        col = split.column()
        sub = col.column(align=True)
        sub.label(text="Resolution:")
        sub.prop(rd, "resolution_x", text="X")
        sub.prop(rd, "resolution_y", text="Y")
        sub.prop(rd, "resolution_percentage", text="")

        sub.label(text="Aspect Ratio:")
        sub.prop(rd, "pixel_aspect_x", text="X")
        sub.prop(rd, "pixel_aspect_y", text="Y")

        row = col.row()
        row.prop(rd, "use_border", text="Border")
        sub = row.row()
        sub.active = rd.use_border
        sub.prop(rd, "use_crop_to_border", text="Crop")

        col = split.column()
        sub = col.column(align=True)
        sub.label(text="Frame Range:")
        sub.prop(scene, "frame_start", text="Start")
        sub.prop(scene, "frame_end", text="End")
        sub.prop(scene, "frame_step", text="Step")

        sub.label(text="Frame Rate:")
        sub.prop(rd, "fps")
        sub.prop(rd, "fps_base", text="/")


class RENDER_PT_stamp(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Stamp"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        rd = context.scene.render

        self.layout.prop(rd, "use_stamp", text="")

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        layout.active = rd.use_stamp

        split = layout.split()

        col = split.column()
        col.prop(rd, "use_stamp_time", text="Time")
        col.prop(rd, "use_stamp_date", text="Date")
        col.prop(rd, "use_stamp_render_time", text="RenderTime")
        col.prop(rd, "use_stamp_frame", text="Frame")
        col.prop(rd, "use_stamp_scene", text="Scene")
        col.prop(rd, "use_stamp_camera", text="Camera")
        col.prop(rd, "use_stamp_filename", text="Filename")
        col.prop(rd, "use_stamp_marker", text="Marker")
        col.prop(rd, "use_stamp_sequencer_strip", text="Seq. Strip")

        col = split.column()
        col.active = rd.use_stamp
        col.prop(rd, "stamp_foreground", slider=True)
        col.prop(rd, "stamp_background", slider=True)
        col.separator()
        col.prop(rd, "stamp_font_size", text="Font Size")

        row = layout.split(percentage=0.2)
        row.prop(rd, "use_stamp_note", text="Note")
        sub = row.row()
        sub.active = rd.use_stamp_note
        sub.prop(rd, "stamp_note_text", text="")


class RENDER_PT_bake(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Bake"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        layout.operator("object.bake_image", icon='RENDER_STILL')
        
        layout.prop(rd, "bake_type")
        
        if rd.bake_type == 'NORMALS':
            layout.prop(rd, "bake_normal_space")
        elif rd.bake_type in ('DISPLACEMENT', 'AO'):
            layout.prop(rd, "use_bake_normalize")
        
        # col.prop(rd, "bake_aa_mode")
        # col.prop(rd, "use_bake_antialiasing")
        
        layout.separator()
        
        split = layout.split()

        col = split.column()
        col.prop(rd, "use_bake_clear")
        col.prop(rd, "bake_margin")
        col.prop(rd, "bake_quad_split", text="Split")
        
        col = split.column()
        col.prop(rd, "use_bake_selected_to_active")
        sub = col.column()
        sub.active = rd.use_bake_selected_to_active
        sub.prop(rd, "bake_distance")
        sub.prop(rd, "bake_bias")

        
def register():
    pass


def unregister():
    pass

if __name__ == "__main__":
    register()
