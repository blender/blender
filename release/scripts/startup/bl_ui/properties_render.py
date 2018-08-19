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
from bpy.types import Menu, Panel, UIList
from bl_operators.presets import PresetMenu
from .space_view3d import (
    VIEW3D_PT_shading_lighting,
    VIEW3D_PT_shading_color,
    VIEW3D_PT_shading_options,
)


class RENDER_PT_presets(PresetMenu):
    bl_label = "Render Presets"
    preset_subdir = "render"
    preset_operator = "script.execute_preset"
    preset_add_operator = "render.preset_add"


class RENDER_PT_ffmpeg_presets(PresetMenu):
    bl_label = "FFMPEG Presets"
    preset_subdir = "ffmpeg"
    preset_operator = "script.python_file_run"


class RENDER_MT_framerate_presets(Menu):
    bl_label = "Frame Rate Presets"
    preset_subdir = "framerate"
    preset_operator = "script.execute_preset"
    draw = Menu.draw_preset


class RenderButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)


class RENDER_PT_context(Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render"
    bl_options = {'HIDE_HEADER'}
    bl_label = ""

    @classmethod
    def poll(cls, context):
        return context.scene

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        rd = scene.render

        if rd.has_multiple_engines:
            layout.prop(rd, "engine", text="Render Engine")


class RENDER_PT_dimensions(RenderButtonsPanel, Panel):
    bl_label = "Dimensions"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    _frame_rate_args_prev = None
    _preset_class = None

    def draw_header_preset(self, context):
        RENDER_PT_presets.draw_panel_header(self.layout)

    @staticmethod
    def _draw_framerate_label(*args):
        # avoids re-creating text string each draw
        if RENDER_PT_dimensions._frame_rate_args_prev == args:
            return RENDER_PT_dimensions._frame_rate_ret

        fps, fps_base, preset_label = args

        if fps_base == 1.0:
            fps_rate = round(fps)
        else:
            fps_rate = round(fps / fps_base, 2)

        # TODO: Change the following to iterate over existing presets
        custom_framerate = (fps_rate not in {23.98, 24, 25, 29.97, 30, 50, 59.94, 60})

        if custom_framerate is True:
            fps_label_text = f"Custom ({fps_rate!r} fps)"
            show_framerate = True
        else:
            fps_label_text = f"{fps_rate!r} fps"
            show_framerate = (preset_label == "Custom")

        RENDER_PT_dimensions._frame_rate_args_prev = args
        RENDER_PT_dimensions._frame_rate_ret = args = (fps_label_text, show_framerate)
        return args

    @staticmethod
    def draw_framerate(layout, sub, rd):
        if RENDER_PT_dimensions._preset_class is None:
            RENDER_PT_dimensions._preset_class = bpy.types.RENDER_MT_framerate_presets

        args = rd.fps, rd.fps_base, RENDER_PT_dimensions._preset_class.bl_label
        fps_label_text, show_framerate = RENDER_PT_dimensions._draw_framerate_label(*args)

        sub.menu("RENDER_MT_framerate_presets", text=fps_label_text)

        if show_framerate:
            col = layout.column(align=True)
            col.prop(rd, "fps")
            col.prop(rd, "fps_base", text="Base")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        scene = context.scene
        rd = scene.render

        col = layout.column(align=True)
        col.prop(rd, "resolution_x", text="Resolution X")
        col.prop(rd, "resolution_y", text="Y")
        col.prop(rd, "resolution_percentage", text="%")

        col = layout.column(align=True)
        col.prop(rd, "pixel_aspect_x", text="Aspect X")
        col.prop(rd, "pixel_aspect_y", text="Y")

        col = layout.column(align=True)
        col.prop(rd, "use_border", text="Border")
        sub = col.column(align=True)
        sub.active = rd.use_border
        sub.prop(rd, "use_crop_to_border", text="Crop")

        col = layout.column(align=True)
        col.prop(scene, "frame_start", text="Frame Start")
        col.prop(scene, "frame_end", text="End")
        col.prop(scene, "frame_step", text="Step")

        col = layout.split()
        col.alignment = 'RIGHT'
        col.label(text="Frame Rate")
        self.draw_framerate(layout, col, rd)


class RENDER_PT_frame_remapping(RenderButtonsPanel, Panel):
    bl_label = "Time Remapping"
    bl_parent_id = "RENDER_PT_dimensions"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        rd = context.scene.render

        col = layout.column(align=True)
        col.prop(rd, "frame_map_old", text="Old")
        col.prop(rd, "frame_map_new", text="New")


class RENDER_PT_post_processing(RenderButtonsPanel, Panel):
    bl_label = "Post Processing"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        rd = context.scene.render

        col = layout.column(align=True)
        col.prop(rd, "use_compositing")
        col.prop(rd, "use_sequencer")

        col.prop(rd, "dither_intensity", text="Dither", slider=True)


class RENDER_PT_stamp(RenderButtonsPanel, Panel):
    bl_label = "Metadata"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        rd = context.scene.render

        split = layout.split()

        col = split.column(align=True)
        col.prop(rd, "use_stamp_date", text="Date")
        col.prop(rd, "use_stamp_time", text="Time")

        col.separator()

        col.prop(rd, "use_stamp_render_time", text="Render Time")
        col.prop(rd, "use_stamp_frame", text="Frame")
        col.prop(rd, "use_stamp_frame_range", text="Frame Range")
        col.prop(rd, "use_stamp_memory", text="Memory")

        col = split.column(align=True)
        col.prop(rd, "use_stamp_camera", text="Camera")
        col.prop(rd, "use_stamp_lens", text="Lens")

        col.separator()

        col.prop(rd, "use_stamp_scene", text="Scene")
        col.prop(rd, "use_stamp_marker", text="Marker")

        col.separator()

        col.prop(rd, "use_stamp_filename", text="Filename")

        col.separator()

        col.prop(rd, "use_stamp_sequencer_strip", text="Strip Name")

        if rd.use_sequencer:
            col.prop(rd, "use_stamp_strip_meta", text="Use Strip Metadata")

        row = layout.split(percentage=0.3)
        row.prop(rd, "use_stamp_note", text="Note")
        sub = row.row()
        sub.active = rd.use_stamp_note
        sub.prop(rd, "stamp_note_text", text="")


class RENDER_PT_stamp_burn(RenderButtonsPanel, Panel):
    bl_label = "Burn Into Image"
    bl_parent_id = "RENDER_PT_stamp"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw_header(self, context):
        rd = context.scene.render

        self.layout.prop(rd, "use_stamp", text="")

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        layout.use_property_split = True

        col = layout.column()
        col.active = rd.use_stamp
        col.prop(rd, "stamp_font_size", text="Font Size")
        col.prop(rd, "use_stamp_labels", text="Draw Labels")
        col.column().prop(rd, "stamp_foreground", slider=True)
        col.column().prop(rd, "stamp_background", slider=True)


class RENDER_PT_output(RenderButtonsPanel, Panel):
    bl_label = "Output"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = False
        layout.use_property_decorate = False  # No animation.

        rd = context.scene.render
        image_settings = rd.image_settings
        file_format = image_settings.file_format

        layout.prop(rd, "filepath", text="")

        layout.use_property_split = True

        col = layout.column(align=True)
        sub = col.column(align=True)
        sub.active = not rd.is_movie_format
        sub.prop(rd, "use_overwrite")
        sub.prop(rd, "use_placeholder")
        col.prop(rd, "use_file_extension")
        col.prop(rd, "use_render_cache")

        layout.use_property_split = False
        layout.template_image_settings(image_settings, color_management=False)
        if rd.use_multiview:
            layout.template_image_views(image_settings)


class RENDER_PT_encoding(RenderButtonsPanel, Panel):
    bl_label = "Encoding"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw_header_preset(self, context):
        RENDER_PT_ffmpeg_presets.draw_panel_header(self.layout)

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return rd.image_settings.file_format in {'FFMPEG', 'XVID', 'H264', 'THEORA'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render
        ffmpeg = rd.ffmpeg

        split = layout.split()
        split.prop(rd.ffmpeg, "format")
        split.prop(ffmpeg, "use_autosplit")

        # Video:
        layout.separator()
        self.draw_vcodec(context)

        # Audio:
        layout.separator()
        if ffmpeg.format != 'MP3':
            layout.prop(ffmpeg, "audio_codec", text="Audio Codec")

        if ffmpeg.audio_codec != 'NONE':
            row = layout.row()
            row.prop(ffmpeg, "audio_bitrate")
            row.prop(ffmpeg, "audio_volume", slider=True)

    def draw_vcodec(self, context):
        """Video codec options."""
        layout = self.layout
        ffmpeg = context.scene.render.ffmpeg

        needs_codec = ffmpeg.format in {'AVI', 'QUICKTIME', 'MKV', 'OGG', 'MPEG4'}
        if needs_codec:
            layout.prop(ffmpeg, "codec")

        if needs_codec and ffmpeg.codec == 'NONE':
            return

        if ffmpeg.codec in {'DNXHD'}:
            layout.prop(ffmpeg, "use_lossless_output")

        # Output quality
        use_crf = needs_codec and ffmpeg.codec in {'H264', 'MPEG4', 'WEBM'}
        if use_crf:
            layout.prop(ffmpeg, "constant_rate_factor")

        # Encoding speed
        layout.prop(ffmpeg, "ffmpeg_preset")
        # I-frames
        layout.prop(ffmpeg, "gopsize")
        # B-Frames
        row = layout.row()
        row.prop(ffmpeg, "use_max_b_frames", text="Max B-frames")
        pbox = row.split()
        pbox.prop(ffmpeg, "max_b_frames", text="")
        pbox.enabled = ffmpeg.use_max_b_frames

        if not use_crf or ffmpeg.constant_rate_factor == 'NONE':
            split = layout.split()
            col = split.column()
            col.label(text="Rate:")
            col.prop(ffmpeg, "video_bitrate")
            col.prop(ffmpeg, "minrate", text="Minimum")
            col.prop(ffmpeg, "maxrate", text="Maximum")
            col.prop(ffmpeg, "buffersize", text="Buffer")

            col = split.column()
            col.label(text="Mux:")
            col.prop(ffmpeg, "muxrate", text="Rate")
            col.prop(ffmpeg, "packetsize", text="Packet Size")


class RENDER_UL_renderviews(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        view = item
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            if view.name in {"left", "right"}:
                layout.label(view.name, icon_value=icon + (not view.use))
            else:
                layout.prop(view, "name", text="", index=index, icon_value=icon, emboss=False)
            layout.prop(view, "use", text="", index=index)

        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label("", icon_value=icon + (not view.use))


class RENDER_PT_stereoscopy(RenderButtonsPanel, Panel):
    bl_label = "Stereoscopy"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        rd = context.scene.render
        self.layout.prop(rd, "use_multiview", text="")

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render
        rv = rd.views.active

        layout.active = rd.use_multiview
        basic_stereo = rd.views_format == 'STEREO_3D'

        row = layout.row()
        row.prop(rd, "views_format", expand=True)

        if basic_stereo:
            row = layout.row()
            row.template_list("RENDER_UL_renderviews", "name", rd, "stereo_views", rd.views, "active_index", rows=2)

            row = layout.row()
            row.label(text="File Suffix:")
            row.prop(rv, "file_suffix", text="")

        else:
            row = layout.row()
            row.template_list("RENDER_UL_renderviews", "name", rd, "views", rd.views, "active_index", rows=2)

            col = row.column(align=True)
            col.operator("scene.render_view_add", icon='ZOOMIN', text="")
            col.operator("scene.render_view_remove", icon='ZOOMOUT', text="")

            row = layout.row()
            row.label(text="Camera Suffix:")
            row.prop(rv, "camera_suffix", text="")


class RENDER_PT_eevee_ambient_occlusion(RenderButtonsPanel, Panel):
    bl_label = "Ambient Occlusion"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        scene = context.scene
        props = scene.eevee
        self.layout.prop(props, "use_gtao", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        scene = context.scene
        props = scene.eevee

        layout.active = props.use_gtao
        col = layout.column()
        col.prop(props, "use_gtao_bent_normals")
        col.prop(props, "use_gtao_bounce")
        col.prop(props, "gtao_distance")
        col.prop(props, "gtao_factor")
        col.prop(props, "gtao_quality")


class RENDER_PT_eevee_motion_blur(RenderButtonsPanel, Panel):
    bl_label = "Motion Blur"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        scene = context.scene
        props = scene.eevee
        self.layout.prop(props, "use_motion_blur", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        scene = context.scene
        props = scene.eevee

        layout.active = props.use_motion_blur
        col = layout.column()
        col.prop(props, "motion_blur_samples")
        col.prop(props, "motion_blur_shutter")


class RENDER_PT_eevee_depth_of_field(RenderButtonsPanel, Panel):
    bl_label = "Depth of Field"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        scene = context.scene
        props = scene.eevee
        self.layout.prop(props, "use_dof", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        scene = context.scene
        props = scene.eevee

        layout.active = props.use_dof
        col = layout.column()
        col.prop(props, "bokeh_max_size")
        col.prop(props, "bokeh_threshold")


class RENDER_PT_eevee_bloom(RenderButtonsPanel, Panel):
    bl_label = "Bloom"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        scene = context.scene
        props = scene.eevee
        self.layout.prop(props, "use_bloom", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        props = scene.eevee

        layout.active = props.use_bloom
        col = layout.column()
        col.prop(props, "bloom_threshold")
        col.prop(props, "bloom_knee")
        col.prop(props, "bloom_radius")
        col.prop(props, "bloom_color")
        col.prop(props, "bloom_intensity")
        col.prop(props, "bloom_clamp")


class RENDER_PT_eevee_volumetric(RenderButtonsPanel, Panel):
    bl_label = "Volumetric"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        scene = context.scene
        props = scene.eevee
        self.layout.prop(props, "use_volumetric", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        props = scene.eevee

        layout.active = props.use_volumetric
        col = layout.column()
        sub = col.column(align=True)
        sub.prop(props, "volumetric_start")
        sub.prop(props, "volumetric_end")
        col.prop(props, "volumetric_tile_size")
        col.separator()
        col.prop(props, "volumetric_samples")
        sub.prop(props, "volumetric_sample_distribution")
        col.separator()
        col.prop(props, "use_volumetric_lights")

        sub = col.column()
        sub.active = props.use_volumetric_lights
        sub.prop(props, "volumetric_light_clamp", text="Light Clamping")
        col.separator()
        col.prop(props, "use_volumetric_shadows")
        sub = col.column()
        sub.active = props.use_volumetric_shadows
        sub.prop(props, "volumetric_shadow_samples", text="Shadow Samples")


class RENDER_PT_eevee_subsurface_scattering(RenderButtonsPanel, Panel):
    bl_label = "Subsurface Scattering"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        scene = context.scene
        props = scene.eevee
        self.layout.prop(props, "use_sss", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        props = scene.eevee

        layout.active = props.use_sss

        col = layout.column()
        col.prop(props, "sss_samples")
        col.prop(props, "sss_jitter_threshold")
        col.prop(props, "use_sss_separate_albedo")


class RENDER_PT_eevee_screen_space_reflections(RenderButtonsPanel, Panel):
    bl_label = "Screen Space Reflections"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        scene = context.scene
        props = scene.eevee
        self.layout.prop(props, "use_ssr", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        props = scene.eevee

        col = layout.column()
        col.active = props.use_ssr
        col.prop(props, "use_ssr_refraction", text="Refraction")
        col.prop(props, "use_ssr_halfres")
        col.prop(props, "ssr_quality")
        col.prop(props, "ssr_max_roughness")
        col.prop(props, "ssr_thickness")
        col.prop(props, "ssr_border_fade")
        col.prop(props, "ssr_firefly_fac")


class RENDER_PT_eevee_shadows(RenderButtonsPanel, Panel):
    bl_label = "Shadows"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        props = scene.eevee

        col = layout.column()
        col.prop(props, "shadow_method")
        col.prop(props, "shadow_cube_size", text="Cube Size")
        col.prop(props, "shadow_cascade_size", text="Cascade Size")
        col.prop(props, "use_shadow_high_bitdepth")


class RENDER_PT_eevee_sampling(RenderButtonsPanel, Panel):
    bl_label = "Sampling"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        scene = context.scene
        props = scene.eevee

        col = layout.column()
        col.prop(props, "taa_samples")
        col.prop(props, "taa_render_samples")
        col.prop(props, "use_taa_reprojection")


class RENDER_PT_eevee_indirect_lighting(RenderButtonsPanel, Panel):
    bl_label = "Indirect Lighting"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        scene = context.scene
        props = scene.eevee

        col = layout.column()
        col.operator("scene.light_cache_bake", text="Bake Indirect Lighting", icon='RENDER_STILL')
        col.operator("scene.light_cache_bake", text="Bake Cubemap Only", icon='LIGHTPROBE_CUBEMAP').subset = "CUBEMAPS"
        col.operator("scene.light_cache_free", text="Free Lighting Cache")

        cache_info = scene.eevee.gi_cache_info
        if cache_info:
            col.label(text=cache_info)

        col.prop(props, "gi_auto_bake")

        col.prop(props, "gi_diffuse_bounces")
        col.prop(props, "gi_cubemap_resolution")
        col.prop(props, "gi_visibility_resolution", text="Diffuse Occlusion")

        layout.use_property_split = False
        row = layout.split(percentage=0.5)
        row.alignment = 'RIGHT'
        row.label("Cubemap Display")

        sub = row.row(align=True)
        sub.prop(props, "gi_cubemap_draw_size", text="Size")
        if props.gi_show_cubemaps:
            sub.prop(props, "gi_show_cubemaps", text="", toggle=True, icon='HIDE_OFF')
        else:
            sub.prop(props, "gi_show_cubemaps", text="", toggle=True, icon='HIDE_ON')

        row = layout.split(percentage=0.5)
        row.alignment = 'RIGHT'
        row.label("Irradiance Display")

        sub = row.row(align=True)
        sub.prop(props, "gi_irradiance_draw_size", text="Size")
        if props.gi_show_irradiance:
            sub.prop(props, "gi_show_irradiance", text="", toggle=True, icon='HIDE_OFF')
        else:
            sub.prop(props, "gi_show_irradiance", text="", toggle=True, icon='HIDE_ON')


class RENDER_PT_eevee_film(RenderButtonsPanel, Panel):
    bl_label = "Film"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        scene = context.scene
        rd = scene.render

        col = layout.column()
        col.prop(rd, "filter_size")
        col.prop(rd, "alpha_mode", text="Alpha")


class RENDER_PT_eevee_hair(RenderButtonsPanel, Panel):
    bl_label = "Hair"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        rd = scene.render

        row = layout.row()
        row.prop(rd, "hair_type", expand=True)

        layout.use_property_split = True
        layout.prop(rd, "hair_subdiv")


class RENDER_PT_opengl_film(RenderButtonsPanel, Panel):
    bl_label = "Film"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        rd = context.scene.render

        layout.prop(rd, "use_antialiasing")

        layout.prop(rd, "antialiasing_samples")
        layout.prop(rd, "alpha_mode")


class RENDER_PT_opengl_lighting(RenderButtonsPanel, Panel):
    bl_label = "Lighting"
    COMPAT_ENGINES = {'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        VIEW3D_PT_shading_lighting.draw(self, context)


class RENDER_PT_opengl_color(RenderButtonsPanel, Panel):
    bl_label = "Color"
    COMPAT_ENGINES = {'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        VIEW3D_PT_shading_color._draw_color_type(self, context)


class RENDER_PT_opengl_options(RenderButtonsPanel, Panel):
    bl_label = "Options"
    COMPAT_ENGINES = {'BLENDER_OPENGL'}

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        VIEW3D_PT_shading_options.draw(self, context)


classes = (
    RENDER_PT_presets,
    RENDER_PT_ffmpeg_presets,
    RENDER_MT_framerate_presets,
    RENDER_PT_context,
    RENDER_PT_dimensions,
    RENDER_PT_frame_remapping,
    RENDER_PT_post_processing,
    RENDER_PT_output,
    RENDER_PT_encoding,
    RENDER_PT_stamp,
    RENDER_PT_stamp_burn,
    RENDER_UL_renderviews,
    RENDER_PT_stereoscopy,
    RENDER_PT_eevee_hair,
    RENDER_PT_eevee_sampling,
    RENDER_PT_eevee_film,
    RENDER_PT_eevee_shadows,
    RENDER_PT_eevee_indirect_lighting,
    RENDER_PT_eevee_subsurface_scattering,
    RENDER_PT_eevee_screen_space_reflections,
    RENDER_PT_eevee_ambient_occlusion,
    RENDER_PT_eevee_volumetric,
    RENDER_PT_eevee_motion_blur,
    RENDER_PT_eevee_depth_of_field,
    RENDER_PT_eevee_bloom,
    RENDER_PT_opengl_film,
    RENDER_PT_opengl_lighting,
    RENDER_PT_opengl_color,
    RENDER_PT_opengl_options,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
