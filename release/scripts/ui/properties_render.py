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

narrowui = bpy.context.user_preferences.view.properties_width_check


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


class RenderButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    def poll(self, context):
        rd = context.scene.render
        return (context.scene and rd.use_game_engine is False) and (rd.engine in self.COMPAT_ENGINES)


class RENDER_PT_render(RenderButtonsPanel):
    bl_label = "Render"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.operator("render.render", text="Image", icon='RENDER_STILL')

        if wide_ui:
            col = split.column()
        col.operator("render.render", text="Animation", icon='RENDER_ANIMATION').animation = True

        layout.prop(rd, "display_mode", text="Display")


class RENDER_PT_layers(RenderButtonsPanel):
    bl_label = "Layers"
    bl_default_closed = True
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render
        wide_ui = context.region.width > narrowui

        row = layout.row()
        row.template_list(rd, "layers", rd, "active_layer_index", rows=2)

        col = row.column(align=True)
        col.operator("scene.render_layer_add", icon='ZOOMIN', text="")
        col.operator("scene.render_layer_remove", icon='ZOOMOUT', text="")

        rl = rd.layers[rd.active_layer_index]

        if rl:
            layout.prop(rl, "name")

        split = layout.split()

        col = split.column()
        col.prop(scene, "layers", text="Scene")
        col.label(text="")
        col.prop(rl, "light_override", text="Light")
        col.prop(rl, "material_override", text="Material")
        if wide_ui:
            col = split.column()
        col.prop(rl, "visible_layers", text="Layer")
        col.label(text="Mask Layers:")
        col.prop(rl, "zmask_layers", text="")


        layout.separator()
        layout.label(text="Include:")

        split = layout.split()

        col = split.column()
        col.prop(rl, "zmask")
        row = col.row()
        row.prop(rl, "zmask_negate", text="Negate")
        row.active = rl.zmask
        col.prop(rl, "all_z")

        col = split.column()
        col.prop(rl, "solid")
        col.prop(rl, "halo")
        col.prop(rl, "ztransp")
        col.prop(rl, "sky")
		
        col = split.column()
        col.prop(rl, "edge")
        col.prop(rl, "strand")
        col.prop(rl, "freestyle")

        layout.separator()

        split = layout.split()

        col = split.column()
        col.label(text="Passes:")
        col.prop(rl, "pass_combined")
        col.prop(rl, "pass_z")
        col.prop(rl, "pass_vector")
        col.prop(rl, "pass_normal")
        col.prop(rl, "pass_uv")
        col.prop(rl, "pass_mist")
        col.prop(rl, "pass_object_index")
        col.prop(rl, "pass_color")

        if wide_ui:
            col = split.column()
        col.label()
        col.prop(rl, "pass_diffuse")
        row = col.row()
        row.prop(rl, "pass_specular")
        row.prop(rl, "pass_specular_exclude", text="")
        row = col.row()
        row.prop(rl, "pass_shadow")
        row.prop(rl, "pass_shadow_exclude", text="")
        row = col.row()
        row.prop(rl, "pass_emit")
        row.prop(rl, "pass_emit_exclude", text="")
        row = col.row()
        row.prop(rl, "pass_ao")
        row.prop(rl, "pass_ao_exclude", text="")
        row = col.row()
        row.prop(rl, "pass_environment")
        row.prop(rl, "pass_environment_exclude", text="")
        row = col.row()
        row.prop(rl, "pass_indirect")
        row.prop(rl, "pass_indirect_exclude", text="")
        row = col.row()
        row.prop(rl, "pass_reflection")
        row.prop(rl, "pass_reflection_exclude", text="")
        row = col.row()
        row.prop(rl, "pass_refraction")
        row.prop(rl, "pass_refraction_exclude", text="")


class RENDER_PT_freestyle(RenderButtonsPanel):
    bl_label = "Freestyle"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def poll(self, context):
        rd = context.scene.render
        rl = rd.layers[rd.active_layer_index]
        return rl and rl.freestyle

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        rl = rd.layers[rd.active_layer_index]
        freestyle = rl.freestyle_settings

        split = layout.split()

        col = split.column()
        col.prop(freestyle, "mode", text="Control Mode")

        if freestyle.mode == "EDITOR":
            col.label(text="Edge Detection Options:")
            col.prop(freestyle, "crease_angle")
            col.prop(freestyle, "sphere_radius")
            col.prop(freestyle, "dkr_epsilon")

            lineset = freestyle.active_lineset

            col.label(text="Line Sets:")
            row = col.row()
            rows = 2
            if lineset:
                rows = 5
            # FIXME: scrollbar does not work correctly
            row.template_list(freestyle, "linesets", freestyle, "active_lineset_index", rows=rows)

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
            col.prop(freestyle, "ridges_and_valleys")
            col.prop(freestyle, "suggestive_contours")
            sub = col.row()
            sub.prop(freestyle, "dkr_epsilon")
            sub.active = freestyle.suggestive_contours
            col.prop(freestyle, "material_boundaries")
            col.operator("scene.freestyle_module_add")

            for i, module in enumerate(freestyle.modules):
                    box = layout.box()
                    box.set_context_pointer("freestyle_module", module)
                    row = box.row(align=True)
                    row.prop(module, "is_displayed", text="")
                    row.prop(module, "module_path", text="")
                    row.operator("scene.freestyle_module_remove", icon='X', text="")
                    row.operator("scene.freestyle_module_move", icon='TRIA_UP', text="").direction = 'UP'
                    row.operator("scene.freestyle_module_move", icon='TRIA_DOWN', text="").direction = 'DOWN'


class RENDER_PT_freestyle_linestyle(RenderButtonsPanel):
    bl_label = "Freestyle: Line Style"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def poll(self, context):
        rd = context.scene.render
        rl = rd.layers[rd.active_layer_index]
        if rl and rl.freestyle:
            freestyle = rl.freestyle_settings
            return freestyle.mode == "EDITOR" and freestyle.active_lineset
        return False

    def draw_modifier_box_header(self, box, modifier):
        row = box.row()
        row.set_context_pointer("modifier", modifier)
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
                box.template_curve_mapping(modifier, "curve") # FIXME: not properly displayed
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
                box.template_curve_mapping(modifier, "curve") # FIXME: not properly displayed
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
        rl = rd.layers[rd.active_layer_index]
        lineset = rl.freestyle_settings.active_lineset
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


class RENDER_PT_shading(RenderButtonsPanel):
    bl_label = "Shading"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.prop(rd, "use_textures", text="Textures")
        col.prop(rd, "use_shadows", text="Shadows")
        col.prop(rd, "use_sss", text="Subsurface Scattering")
        col.prop(rd, "use_envmaps", text="Environment Map")

        if wide_ui:
            col = split.column()
        col.prop(rd, "use_raytracing", text="Ray Tracing")
        col.prop(rd, "color_management")
        col.prop(rd, "alpha_mode", text="Alpha")


class RENDER_PT_performance(RenderButtonsPanel):
    bl_label = "Performance"
    bl_default_closed = True
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        wide_ui = context.region.width > narrowui

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

        if wide_ui:
            col = split.column()
        col.label(text="Memory:")
        sub = col.column()
        sub.enabled = not (rd.use_border or rd.full_sample)
        sub.prop(rd, "save_buffers")
        sub = col.column()
        sub.active = rd.use_compositing
        sub.prop(rd, "free_image_textures")
        sub = col.column()
        sub.active = rd.use_raytracing
        sub.label(text="Acceleration structure:")
        sub.prop(rd, "raytrace_structure", text="")
        if rd.raytrace_structure == 'OCTREE':
            sub.prop(rd, "octree_resolution", text="Resolution")
        else:
            sub.prop(rd, "use_instances", text="Instances")
        sub.prop(rd, "use_local_coords", text="Local Coordinates")


class RENDER_PT_post_processing(RenderButtonsPanel):
    bl_label = "Post Processing"
    bl_default_closed = True
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.prop(rd, "use_compositing")
        col.prop(rd, "use_sequencer")

        if wide_ui:
            col = split.column()
        col.prop(rd, "dither_intensity", text="Dither", slider=True)

        layout.separator()

        split = layout.split()

        col = split.column()
        col.prop(rd, "fields", text="Fields")
        sub = col.column()
        sub.active = rd.fields
        sub.row().prop(rd, "field_order", expand=True)
        sub.prop(rd, "fields_still", text="Still")


        if wide_ui:
            col = split.column()
        else:
            col.separator()
        col.prop(rd, "edge")
        sub = col.column()
        sub.active = rd.edge
        sub.prop(rd, "edge_threshold", text="Threshold", slider=True)
        sub.prop(rd, "edge_color", text="")

        layout.separator()

        split = layout.split()
        col = split.column()
        col.prop(rd, "freestyle", text="Freestyle")


class RENDER_PT_output(RenderButtonsPanel):
    bl_label = "Output"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        wide_ui = context.region.width > narrowui

        layout.prop(rd, "output_path", text="")

        split = layout.split()
        col = split.column()
        col.prop(rd, "file_format", text="")
        col.row().prop(rd, "color_mode", text="Color", expand=True)

        if wide_ui:
            col = split.column()
        col.prop(rd, "use_file_extension")
        col.prop(rd, "use_overwrite")
        col.prop(rd, "use_placeholder")

        if rd.file_format in ('AVI_JPEG', 'JPEG'):
            split = layout.split()
            split.prop(rd, "file_quality", slider=True)

        elif rd.file_format == 'MULTILAYER':
            split = layout.split()

            col = split.column()
            col.label(text="Codec:")
            col.prop(rd, "exr_codec", text="")
            if wide_ui:
                col = split.column()

        elif rd.file_format == 'OPEN_EXR':
            split = layout.split()

            col = split.column()
            col.label(text="Codec:")
            col.prop(rd, "exr_codec", text="")

            if wide_ui:
                subsplit = split.split()
                col = subsplit.column()
            col.prop(rd, "exr_half")
            col.prop(rd, "exr_zbuf")

            if wide_ui:
                col = subsplit.column()
            col.prop(rd, "exr_preview")

        elif rd.file_format == 'JPEG2000':
            split = layout.split()
            col = split.column()
            col.label(text="Depth:")
            col.row().prop(rd, "jpeg2k_depth", expand=True)

            if wide_ui:
                col = split.column()
            col.prop(rd, "jpeg2k_preset", text="")
            col.prop(rd, "jpeg2k_ycc")

        elif rd.file_format in ('CINEON', 'DPX'):
            split = layout.split()
            col = split.column()
            col.prop(rd, "cineon_log", text="Convert to Log")

            if wide_ui:
                col = split.column(align=True)
            col.active = rd.cineon_log
            col.prop(rd, "cineon_black", text="Black")
            col.prop(rd, "cineon_white", text="White")
            col.prop(rd, "cineon_gamma", text="Gamma")

        elif rd.file_format == 'TIFF':
            split = layout.split()
            split.prop(rd, "tiff_bit")

        elif rd.file_format == 'QUICKTIME_CARBON':
            split = layout.split()
            split.operator("scene.render_data_set_quicktime_codec")

        elif rd.file_format == 'QUICKTIME_QTKIT':
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
                if wide_ui:
                    col = split.column()
                col.prop(rd, "quicktime_audio_samplerate", text="")

                split = layout.split()
                col = split.column()
                if rd.quicktime_audiocodec_type == 'AAC':
                    col.prop(rd, "quicktime_audio_bitrate")
                if wide_ui:
                    subsplit = split.split()
                    col = subsplit.column()
                if rd.quicktime_audiocodec_type == 'AAC':
                    col.prop(rd, "quicktime_audio_codec_isvbr")
                if wide_ui:
                    col = subsplit.column()
                col.prop(rd, "quicktime_audio_resampling_hq")


class RENDER_PT_encoding(RenderButtonsPanel):
    bl_label = "Encoding"
    bl_default_closed = True
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def poll(self, context):
        rd = context.scene.render
        return rd.file_format in ('FFMPEG', 'XVID', 'H264', 'THEORA')

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        wide_ui = context.region.width > narrowui

        layout.menu("RENDER_MT_ffmpeg_presets", text="Presets")

        split = layout.split()

        col = split.column()
        col.prop(rd, "ffmpeg_format")
        if rd.ffmpeg_format in ('AVI', 'QUICKTIME', 'MKV', 'OGG'):
            if wide_ui:
                col = split.column()
            col.prop(rd, "ffmpeg_codec")
        else:
            if wide_ui:
                split.label()

        split = layout.split()

        col = split.column()
        col.prop(rd, "ffmpeg_video_bitrate")
        if wide_ui:
            col = split.column()
        col.prop(rd, "ffmpeg_gopsize")

        split = layout.split()

        col = split.column()
        col.label(text="Rate:")
        col.prop(rd, "ffmpeg_minrate", text="Minimum")
        col.prop(rd, "ffmpeg_maxrate", text="Maximum")
        col.prop(rd, "ffmpeg_buffersize", text="Buffer")

        if wide_ui:
            col = split.column()

        col.prop(rd, "ffmpeg_autosplit")
        col.label(text="Mux:")
        col.prop(rd, "ffmpeg_muxrate", text="Rate")
        col.prop(rd, "ffmpeg_packetsize", text="Packet Size")

        # Audio:
        sub = layout.column()

        if rd.ffmpeg_format not in ('MP3'):
            sub.prop(rd, "ffmpeg_audio_codec", text="Audio Codec")

        sub.separator()

        split = sub.split()

        col = split.column()
        col.prop(rd, "ffmpeg_audio_bitrate")
        col.prop(rd, "ffmpeg_audio_mixrate")

        if wide_ui:
            col = split.column()
        col.prop(rd, "ffmpeg_audio_volume", slider=True)


class RENDER_PT_antialiasing(RenderButtonsPanel):
    bl_label = "Anti-Aliasing"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        rd = context.scene.render

        self.layout.prop(rd, "render_antialiasing", text="")

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        wide_ui = context.region.width > narrowui
        layout.active = rd.render_antialiasing

        split = layout.split()

        col = split.column()
        col.row().prop(rd, "antialiasing_samples", expand=True)
        sub = col.row()
        sub.enabled = not rd.use_border
        sub.prop(rd, "full_sample")

        if wide_ui:
            col = split.column()
        col.prop(rd, "pixel_filter", text="")
        col.prop(rd, "filter_size", text="Size")


class RENDER_PT_motion_blur(RenderButtonsPanel):
    bl_label = "Full Sample Motion Blur"
    bl_default_closed = True
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        rd = context.scene.render

        self.layout.prop(rd, "motion_blur", text="")

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        layout.active = rd.motion_blur

        row = layout.row()
        row.prop(rd, "motion_blur_samples")
        row.prop(rd, "motion_blur_shutter")


class RENDER_PT_dimensions(RenderButtonsPanel):
    bl_label = "Dimensions"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render
        wide_ui = context.region.width > narrowui

        row = layout.row(align=True)
        row.menu("RENDER_MT_presets", text=bpy.types.RENDER_MT_presets.bl_label)
        row.operator("render.preset_add", text="", icon="ZOOMIN")

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
        sub.prop(rd, "crop_to_border", text="Crop")

        if wide_ui:
            col = split.column()
        sub = col.column(align=True)
        sub.label(text="Frame Range:")
        sub.prop(scene, "frame_start", text="Start")
        sub.prop(scene, "frame_end", text="End")
        sub.prop(scene, "frame_step", text="Step")

        sub.label(text="Frame Rate:")
        sub.prop(rd, "fps")
        sub.prop(rd, "fps_base", text="/")


class RENDER_PT_stamp(RenderButtonsPanel):
    bl_label = "Stamp"
    bl_default_closed = True
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_header(self, context):
        rd = context.scene.render

        self.layout.prop(rd, "render_stamp", text="")

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        wide_ui = context.region.width > narrowui

        layout.active = rd.render_stamp

        split = layout.split()

        col = split.column()
        col.prop(rd, "stamp_time", text="Time")
        col.prop(rd, "stamp_date", text="Date")
        col.prop(rd, "stamp_render_time", text="RenderTime")
        col.prop(rd, "stamp_frame", text="Frame")
        col.prop(rd, "stamp_scene", text="Scene")
        col.prop(rd, "stamp_camera", text="Camera")
        col.prop(rd, "stamp_filename", text="Filename")
        col.prop(rd, "stamp_marker", text="Marker")
        col.prop(rd, "stamp_sequencer_strip", text="Seq. Strip")

        if wide_ui:
            col = split.column()
        col.active = rd.render_stamp
        col.prop(rd, "stamp_foreground", slider=True)
        col.prop(rd, "stamp_background", slider=True)
        col.separator()
        col.prop(rd, "stamp_font_size", text="Font Size")

        row = layout.split(percentage=0.2)
        row.prop(rd, "stamp_note", text="Note")
        sub = row.row()
        sub.active = rd.stamp_note
        sub.prop(rd, "stamp_note_text", text="")


class RENDER_PT_bake(RenderButtonsPanel):
    bl_label = "Bake"
    bl_default_closed = True
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        wide_ui = context.region.width > narrowui

        layout.operator("object.bake_image", icon='RENDER_STILL')
        
        if wide_ui:
            layout.prop(rd, "bake_type")
        else:
            layout.prop(rd, "bake_type", text="")
        
        if rd.bake_type == 'NORMALS':
            if wide_ui:
                layout.prop(rd, "bake_normal_space")
            else:
                layout.prop(rd, "bake_normal_space", text="")
        elif rd.bake_type in ('DISPLACEMENT', 'AO'):
            layout.prop(rd, "bake_normalized")
        
        # col.prop(rd, "bake_aa_mode")
        # col.prop(rd, "bake_enable_aa")
        
        layout.separator()
        
        split = layout.split()

        col = split.column()
        col.prop(rd, "bake_clear")
        col.prop(rd, "bake_margin")
        col.prop(rd, "bake_quad_split", text="Split")
        
        if wide_ui:
            col = split.column()
        col.prop(rd, "bake_active")
        sub = col.column()
        sub.active = rd.bake_active
        sub.prop(rd, "bake_distance")
        sub.prop(rd, "bake_bias")

        
classes = [
    RENDER_MT_presets,
    RENDER_MT_ffmpeg_presets,
    RENDER_PT_render,
    RENDER_PT_layers,
    RENDER_PT_freestyle,
    RENDER_PT_freestyle_linestyle,
    RENDER_PT_dimensions,
    RENDER_PT_antialiasing,
    RENDER_PT_motion_blur,
    RENDER_PT_shading,
    RENDER_PT_output,
    RENDER_PT_encoding,
    RENDER_PT_performance,
    RENDER_PT_post_processing,
    RENDER_PT_stamp,
    RENDER_PT_bake]


def register():
    register = bpy.types.register
    for cls in classes:
        register(cls)


def unregister():
    unregister = bpy.types.unregister
    for cls in classes:
        unregister(cls)

if __name__ == "__main__":
    register()
