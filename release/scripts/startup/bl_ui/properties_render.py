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
from bpy.types import Menu, Panel
from blf import gettext as _


class RENDER_MT_presets(Menu):
    bl_label = _("Render Presets")
    preset_subdir = "render"
    preset_operator = "script.execute_preset"
    draw = Menu.draw_preset


class RENDER_MT_ffmpeg_presets(Menu):
    bl_label = _("FFMPEG Presets")
    preset_subdir = "ffmpeg"
    preset_operator = "script.python_file_run"
    draw = Menu.draw_preset


class RENDER_MT_framerate_presets(Menu):
    bl_label = _("Frame Rate Presets")
    preset_subdir = "framerate"
    preset_operator = "script.execute_preset"
    draw = Menu.draw_preset


class RenderButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return (context.scene and rd.use_game_engine is False) and (rd.engine in cls.COMPAT_ENGINES)


class RENDER_PT_render(RenderButtonsPanel, Panel):
    bl_label = _("Render")
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        row = layout.row()
        row.operator("render.render", text=_("Image"), icon='RENDER_STILL')
        row.operator("render.render", text=_("Animation"), icon='RENDER_ANIMATION').animation = True

        layout.prop(rd, "display_mode", text=_("Display"))


class RENDER_PT_layers(RenderButtonsPanel, Panel):
    bl_label = _("Layers")
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

        row = layout.row()
        rl = rd.layers.active
        if rl:
            row.prop(rl, "name")
        row.prop(rd, "use_single_layer", text="", icon_only=True)

        split = layout.split()

        col = split.column()
        col.prop(scene, "layers", text=_("Scene"))
        col.label(text="")
        col.prop(rl, "light_override", text=_("Light"))
        col.prop(rl, "material_override", text=_("Material"))

        col = split.column()
        col.prop(rl, "layers", text=_("Layer"))
        col.label(text=_("Mask Layers:"))
        col.prop(rl, "layers_zmask", text="")

        layout.separator()
        layout.label(text=_("Include:"))

        split = layout.split()

        col = split.column()
        col.prop(rl, "use_zmask")
        row = col.row()
        row.prop(rl, "invert_zmask", text=_("Negate"))
        row.active = rl.use_zmask
        col.prop(rl, "use_all_z")

        col = split.column()
        col.prop(rl, "use_solid")
        col.prop(rl, "use_halo")
        col.prop(rl, "use_ztransp")

        col = split.column()
        col.prop(rl, "use_sky")
        col.prop(rl, "use_edge_enhance")
        col.prop(rl, "use_strand")

        layout.separator()

        split = layout.split()

        col = split.column()
        col.label(text=_("Passes:"))
        col.prop(rl, "use_pass_combined")
        col.prop(rl, "use_pass_z")
        col.prop(rl, "use_pass_vector")
        col.prop(rl, "use_pass_normal")
        col.prop(rl, "use_pass_uv")
        col.prop(rl, "use_pass_mist")
        col.prop(rl, "use_pass_object_index")
        col.prop(rl, "use_pass_material_index")
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


class RENDER_PT_dimensions(RenderButtonsPanel, Panel):
    bl_label = _("Dimensions")
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
        sub.label(text=_("Resolution:"))
        sub.prop(rd, "resolution_x", text="X")
        sub.prop(rd, "resolution_y", text="Y")
        sub.prop(rd, "resolution_percentage", text="")

        sub.label(text=_("Aspect Ratio:"))
        sub.prop(rd, "pixel_aspect_x", text="X")
        sub.prop(rd, "pixel_aspect_y", text="Y")

        row = col.row()
        row.prop(rd, "use_border", text=_("Border"))
        sub = row.row()
        sub.active = rd.use_border
        sub.prop(rd, "use_crop_to_border", text=_("Crop"))

        col = split.column()
        sub = col.column(align=True)
        sub.label(text=_("Frame Range:"))
        sub.prop(scene, "frame_start", text=_("Start"))
        sub.prop(scene, "frame_end", text=_("End"))
        sub.prop(scene, "frame_step", text=_("Step"))

        sub.label(text=_("Frame Rate:"))
        if rd.fps_base == 1:
            fps_rate = round(rd.fps / rd.fps_base)
        else:
            fps_rate = round(rd.fps / rd.fps_base, 2)

        # TODO: Change the following to iterate over existing presets
        custom_framerate = (fps_rate not in {23.98, 24, 25, 29.97, 30, 50, 59.94, 60})

        if custom_framerate == True:
            fps_label_text = _("Custom (") + str(fps_rate) + " fps)"
        else:
            fps_label_text = str(fps_rate) + " fps"

        sub.menu("RENDER_MT_framerate_presets", text=fps_label_text)

        if custom_framerate or (bpy.types.RENDER_MT_framerate_presets.bl_label == "Custom"):
            sub.prop(rd, "fps")
            sub.prop(rd, "fps_base", text="/")
        subrow = sub.row(align=True)
        subrow.label(text=_("Time Remapping:"))
        subrow = sub.row(align=True)
        subrow.prop(rd, "frame_map_old", text=_("Old"))
        subrow.prop(rd, "frame_map_new", text=_("New"))


class RENDER_PT_antialiasing(RenderButtonsPanel, Panel):
    bl_label = _("Anti-Aliasing")
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
        col.prop(rd, "filter_size", text=_("Size"))


class RENDER_PT_motion_blur(RenderButtonsPanel, Panel):
    bl_label = _("Sampled Motion Blur")
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return not rd.use_full_sample and (rd.engine in cls.COMPAT_ENGINES)

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


class RENDER_PT_shading(RenderButtonsPanel, Panel):
    bl_label = _("Shading")
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        split = layout.split()

        col = split.column()
        col.prop(rd, "use_textures", text=_("Textures"))
        col.prop(rd, "use_shadows", text=_("Shadows"))
        col.prop(rd, "use_sss", text=_("Subsurface Scattering"))
        col.prop(rd, "use_envmaps", text=_("Environment Map"))

        col = split.column()
        col.prop(rd, "use_raytrace", text=_("Ray Tracing"))
        col.prop(rd, "use_color_management")
        col.prop(rd, "alpha_mode", text=_("Alpha"))


class RENDER_PT_performance(RenderButtonsPanel, Panel):
    bl_label = _("Performance")
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        split = layout.split()

        col = split.column()
        col.label(text=_("Threads:"))
        col.row().prop(rd, "threads_mode", expand=True)
        sub = col.column()
        sub.enabled = rd.threads_mode == 'FIXED'
        sub.prop(rd, "threads")
        sub = col.column(align=True)
        sub.label(text=_("Tiles:"))
        sub.prop(rd, "parts_x", text="X")
        sub.prop(rd, "parts_y", text="Y")

        col = split.column()
        col.label(text=_("Memory:"))
        sub = col.column()
        sub.enabled = not (rd.use_border or rd.use_full_sample)
        sub.prop(rd, "use_save_buffers")
        sub = col.column()
        sub.active = rd.use_compositing
        sub.prop(rd, "use_free_image_textures")
        sub.prop(rd, "use_free_unused_nodes")
        sub = col.column()
        sub.active = rd.use_raytrace
        sub.label(text=_("Acceleration structure:"))
        sub.prop(rd, "raytrace_method", text="")
        if rd.raytrace_method == 'OCTREE':
            sub.prop(rd, "octree_resolution", text=_("Resolution"))
        else:
            sub.prop(rd, "use_instances", text=_("Instances"))
        sub.prop(rd, "use_local_coords", text=_("Local Coordinates"))


class RENDER_PT_post_processing(RenderButtonsPanel, Panel):
    bl_label = _("Post Processing")
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        split = layout.split()

        col = split.column()
        col.prop(rd, "use_compositing")
        col.prop(rd, "use_sequencer")

        split.prop(rd, "dither_intensity", text=_("Dither"), slider=True)

        layout.separator()

        split = layout.split()

        col = split.column()
        col.prop(rd, "use_fields", text=_("Fields"))
        sub = col.column()
        sub.active = rd.use_fields
        sub.row().prop(rd, "field_order", expand=True)
        sub.prop(rd, "use_fields_still", text=_("Still"))

        col = split.column()
        col.prop(rd, "use_edge_enhance")
        sub = col.column()
        sub.active = rd.use_edge_enhance
        sub.prop(rd, "edge_threshold", text=_("Threshold"), slider=True)
        sub.prop(rd, "edge_color", text="")


class RENDER_PT_stamp(RenderButtonsPanel, Panel):
    bl_label = _("Stamp")
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
        col.prop(rd, "use_stamp_time", text=_("Time"))
        col.prop(rd, "use_stamp_date", text=_("Date"))
        col.prop(rd, "use_stamp_render_time", text=_("RenderTime"))
        col.prop(rd, "use_stamp_frame", text=_("Frame"))
        col.prop(rd, "use_stamp_scene", text=_("Scene"))
        col.prop(rd, "use_stamp_camera", text=_("Camera"))
        col.prop(rd, "use_stamp_lens", text=_("Lens"))
        col.prop(rd, "use_stamp_filename", text=_("Filename"))
        col.prop(rd, "use_stamp_marker", text=_("Marker"))
        col.prop(rd, "use_stamp_sequencer_strip", text=_("Seq. Strip"))

        col = split.column()
        col.active = rd.use_stamp
        col.prop(rd, "stamp_foreground", slider=True)
        col.prop(rd, "stamp_background", slider=True)
        col.separator()
        col.prop(rd, "stamp_font_size", text=_("Font Size"))

        row = layout.split(percentage=0.2)
        row.prop(rd, "use_stamp_note", text=_("Note"))
        sub = row.row()
        sub.active = rd.use_stamp_note
        sub.prop(rd, "stamp_note_text", text="")


class RENDER_PT_output(RenderButtonsPanel, Panel):
    bl_label = _("Output")
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        file_format = rd.file_format

        layout.prop(rd, "filepath", text="")

        split = layout.split()

        col = split.column()
        col.prop(rd, "file_format", text="")
        col.row().prop(rd, "color_mode", text=_("Color"), expand=True)

        col = split.column()
        col.prop(rd, "use_file_extension")
        col.prop(rd, "use_overwrite")
        col.prop(rd, "use_placeholder")

        if file_format in {'AVI_JPEG', 'JPEG'}:
            layout.prop(rd, "file_quality", slider=True)

        if file_format == 'PNG':
            layout.prop(rd, "file_quality", slider=True, text=_("Compression"))

        if file_format in {'OPEN_EXR', 'MULTILAYER'}:
            row = layout.row()
            row.prop(rd, "exr_codec", text=_("Codec"))

            if file_format == 'OPEN_EXR':
                row = layout.row()
                row.prop(rd, "use_exr_half")
                row.prop(rd, "exr_zbuf")
                row.prop(rd, "exr_preview")

        elif file_format == 'JPEG2000':
            split = layout.split()
            col = split.column()
            col.label(text=_("Depth:"))
            col.row().prop(rd, "jpeg2k_depth", expand=True)

            col = split.column()
            col.prop(rd, "jpeg2k_preset", text="")
            col.prop(rd, "jpeg2k_ycc")

        elif file_format in {'CINEON', 'DPX'}:

            split = layout.split()
            split.label("FIXME: hard coded Non-Linear, Gamma:1.0")
            '''
            col = split.column()
            col.prop(rd, "use_cineon_log", text=_("Convert to Log"))

            col = split.column(align=True)
            col.active = rd.use_cineon_log
            col.prop(rd, "cineon_black", text=_("Black"))
            col.prop(rd, "cineon_white", text=_("White"))
            col.prop(rd, "cineon_gamma", text=_("Gamma"))
            '''

        elif file_format == 'TIFF':
            layout.prop(rd, "use_tiff_16bit")

        elif file_format == 'QUICKTIME_CARBON':
            layout.operator("scene.render_data_set_quicktime_codec")

        elif file_format == 'QUICKTIME_QTKIT':
            split = layout.split()
            col = split.column()
            col.prop(rd, "quicktime_codec_type", text=_("Video Codec"))
            col.prop(rd, "quicktime_codec_spatial_quality", text=_("Quality"))

            # Audio
            col.prop(rd, "quicktime_audiocodec_type", text=_("Audio Codec"))
            if rd.quicktime_audiocodec_type != 'No audio':
                split = layout.split()
                if rd.quicktime_audiocodec_type == 'LPCM':
                    split.prop(rd, "quicktime_audio_bitdepth", text="")

                split.prop(rd, "quicktime_audio_samplerate", text="")

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


class RENDER_PT_encoding(RenderButtonsPanel, Panel):
    bl_label = _("Encoding")
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return rd.file_format in {'FFMPEG', 'XVID', 'H264', 'THEORA'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        layout.menu("RENDER_MT_ffmpeg_presets", text=_("Presets"))

        split = layout.split()
        split.prop(rd, "ffmpeg_format")
        if rd.ffmpeg_format in {'AVI', 'QUICKTIME', 'MKV', 'OGG'}:
            split.prop(rd, "ffmpeg_codec")
        else:
            split.label()

        row = layout.row()
        row.prop(rd, "ffmpeg_video_bitrate")
        row.prop(rd, "ffmpeg_gopsize")

        split = layout.split()

        col = split.column()
        col.label(text=_("Rate:"))
        col.prop(rd, "ffmpeg_minrate", text=_("Minimum"))
        col.prop(rd, "ffmpeg_maxrate", text=_("Maximum"))
        col.prop(rd, "ffmpeg_buffersize", text=_("Buffer"))

        col = split.column()
        col.prop(rd, "ffmpeg_autosplit")
        col.label(text=_("Mux:"))
        col.prop(rd, "ffmpeg_muxrate", text=_("Rate"))
        col.prop(rd, "ffmpeg_packetsize", text=_("Packet Size"))

        layout.separator()

        # Audio:
        if rd.ffmpeg_format not in {'MP3'}:
            layout.prop(rd, "ffmpeg_audio_codec", text=_("Audio Codec"))

        row = layout.row()
        row.prop(rd, "ffmpeg_audio_bitrate")
        row.prop(rd, "ffmpeg_audio_volume", slider=True)


class RENDER_PT_bake(RenderButtonsPanel, Panel):
    bl_label = _("Bake")
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        layout.operator("object.bake_image", icon='RENDER_STILL')

        layout.prop(rd, "bake_type")

        multires_bake = False
        if rd.bake_type in ['NORMALS', 'DISPLACEMENT']:
            layout.prop(rd, 'use_bake_multires')
            multires_bake = rd.use_bake_multires

        if not multires_bake:
            if rd.bake_type == 'NORMALS':
                layout.prop(rd, "bake_normal_space")
            elif rd.bake_type in {'DISPLACEMENT', 'AO'}:
                layout.prop(rd, "use_bake_normalize")

            # col.prop(rd, "bake_aa_mode")
            # col.prop(rd, "use_bake_antialiasing")

            layout.separator()

            split = layout.split()

            col = split.column()
            col.prop(rd, "use_bake_clear")
            col.prop(rd, "bake_margin")
            col.prop(rd, "bake_quad_split", text=_("Split"))

            col = split.column()
            col.prop(rd, "use_bake_selected_to_active")
            sub = col.column()
            sub.active = rd.use_bake_selected_to_active
            sub.prop(rd, "bake_distance")
            sub.prop(rd, "bake_bias")
        else:
            if rd.bake_type == 'DISPLACEMENT':
                layout.prop(rd, "use_bake_lores_mesh")

            layout.prop(rd, "use_bake_clear")
            layout.prop(rd, "bake_margin")


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
