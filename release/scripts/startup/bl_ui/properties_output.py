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
from bl_ui.utils import PresetPanel

from bpy.app.translations import pgettext_tip as tip_


class RENDER_PT_presets(PresetPanel, Panel):
    bl_label = "Render Presets"
    preset_subdir = "render"
    preset_operator = "script.execute_preset"
    preset_add_operator = "render.preset_add"


class RENDER_PT_ffmpeg_presets(PresetPanel, Panel):
    bl_label = "FFMPEG Presets"
    preset_subdir = "ffmpeg"
    preset_operator = "script.python_file_run"


class RENDER_MT_framerate_presets(Menu):
    bl_label = "Frame Rate Presets"
    preset_subdir = "framerate"
    preset_operator = "script.execute_preset"
    draw = Menu.draw_preset


class RenderOutputButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "output"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)


class RENDER_PT_dimensions(RenderOutputButtonsPanel, Panel):
    bl_label = "Dimensions"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    _frame_rate_args_prev = None
    _preset_class = None

    def draw_header_preset(self, _context):
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
            fps_label_text = tip_("Custom (%.4g fps)") % fps_rate
            show_framerate = True
        else:
            fps_label_text = tip_("%.4g fps") % fps_rate
            show_framerate = (preset_label == "Custom")

        RENDER_PT_dimensions._frame_rate_args_prev = args
        RENDER_PT_dimensions._frame_rate_ret = args = (fps_label_text, show_framerate)
        return args

    @staticmethod
    def draw_framerate(layout, rd):
        if RENDER_PT_dimensions._preset_class is None:
            RENDER_PT_dimensions._preset_class = bpy.types.RENDER_MT_framerate_presets

        args = rd.fps, rd.fps_base, RENDER_PT_dimensions._preset_class.bl_label
        fps_label_text, show_framerate = RENDER_PT_dimensions._draw_framerate_label(*args)

        layout.menu("RENDER_MT_framerate_presets", text=fps_label_text)

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
        col.prop(rd, "use_border")
        sub = col.column(align=True)
        sub.active = rd.use_border
        sub.prop(rd, "use_crop_to_border")

        col = layout.column(align=True)
        col.prop(scene, "frame_start", text="Frame Start")
        col.prop(scene, "frame_end", text="End")
        col.prop(scene, "frame_step", text="Step")

        col = layout.column(heading="Frame Rate")
        self.draw_framerate(col, rd)


class RENDER_PT_frame_remapping(RenderOutputButtonsPanel, Panel):
    bl_label = "Time Remapping"
    bl_parent_id = "RENDER_PT_dimensions"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        rd = context.scene.render

        col = layout.column(align=True)
        col.prop(rd, "frame_map_old", text="Old")
        col.prop(rd, "frame_map_new", text="New")


class RENDER_PT_post_processing(RenderOutputButtonsPanel, Panel):
    bl_label = "Post Processing"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        rd = context.scene.render

        col = layout.column(heading="Pipeline")
        col.prop(rd, "use_compositing")
        col.prop(rd, "use_sequencer")

        layout.prop(rd, "dither_intensity", text="Dither", slider=True)


class RENDER_PT_stamp(RenderOutputButtonsPanel, Panel):
    bl_label = "Metadata"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        rd = context.scene.render

        if rd.use_sequencer:
            layout.prop(rd, "metadata_input")

        col = layout.column(heading="Include")
        col.prop(rd, "use_stamp_date", text="Date")
        col.prop(rd, "use_stamp_time", text="Time")
        col.prop(rd, "use_stamp_render_time", text="Render Time")
        col.prop(rd, "use_stamp_frame", text="Frame")
        col.prop(rd, "use_stamp_frame_range", text="Frame Range")
        col.prop(rd, "use_stamp_memory", text="Memory")
        col.prop(rd, "use_stamp_hostname", text="Hostname")
        col.prop(rd, "use_stamp_camera", text="Camera")
        col.prop(rd, "use_stamp_lens", text="Lens")
        col.prop(rd, "use_stamp_scene", text="Scene")
        col.prop(rd, "use_stamp_marker", text="Marker")
        col.prop(rd, "use_stamp_filename", text="Filename")


class RENDER_PT_stamp_note(RenderOutputButtonsPanel, Panel):
    bl_label = "Note"
    bl_parent_id = "RENDER_PT_stamp"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    def draw_header(self, context):
        rd = context.scene.render

        self.layout.prop(rd, "use_stamp_note", text="")

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        layout.active = rd.use_stamp_note
        layout.prop(rd, "stamp_note_text", text="")


class RENDER_PT_stamp_burn(RenderOutputButtonsPanel, Panel):
    bl_label = "Burn Into Image"
    bl_parent_id = "RENDER_PT_stamp"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

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
        col.column().prop(rd, "stamp_foreground", slider=True)
        col.column().prop(rd, "stamp_background", slider=True)
        col.prop(rd, "use_stamp_labels", text="Include Labels")


class RENDER_PT_output(RenderOutputButtonsPanel, Panel):
    bl_label = "Output"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = False
        layout.use_property_decorate = False  # No animation.

        rd = context.scene.render
        image_settings = rd.image_settings

        layout.prop(rd, "filepath", text="")

        layout.use_property_split = True

        col = layout.column(heading="Saving")
        col.prop(rd, "use_file_extension")
        col.prop(rd, "use_render_cache")

        layout.template_image_settings(image_settings, color_management=False)

        if not rd.is_movie_format:
            col = layout.column(heading="Image Sequence")
            col.prop(rd, "use_overwrite")
            col.prop(rd, "use_placeholder")


class RENDER_PT_output_views(RenderOutputButtonsPanel, Panel):
    bl_label = "Views"
    bl_parent_id = "RENDER_PT_output"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return rd.use_multiview

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = False
        layout.use_property_decorate = False  # No animation.

        rd = context.scene.render
        layout.template_image_views(rd.image_settings)


class RENDER_PT_encoding(RenderOutputButtonsPanel, Panel):
    bl_label = "Encoding"
    bl_parent_id = "RENDER_PT_output"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    def draw_header_preset(self, _context):
        RENDER_PT_ffmpeg_presets.draw_panel_header(self.layout)

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return rd.image_settings.file_format in {'FFMPEG', 'XVID', 'H264', 'THEORA'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        rd = context.scene.render
        ffmpeg = rd.ffmpeg

        layout.prop(rd.ffmpeg, "format")
        layout.prop(ffmpeg, "use_autosplit")


class RENDER_PT_encoding_video(RenderOutputButtonsPanel, Panel):
    bl_label = "Video"
    bl_parent_id = "RENDER_PT_encoding"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return rd.image_settings.file_format in {'FFMPEG', 'XVID', 'H264', 'THEORA'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        self.draw_vcodec(context)

    def draw_vcodec(self, context):
        """Video codec options."""
        layout = self.layout
        ffmpeg = context.scene.render.ffmpeg

        needs_codec = ffmpeg.format in {'AVI', 'QUICKTIME', 'MKV', 'OGG', 'MPEG4', 'WEBM'}
        if needs_codec:
            layout.prop(ffmpeg, "codec")

        if needs_codec and ffmpeg.codec == 'NONE':
            return

        if ffmpeg.codec == 'DNXHD':
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
        row = layout.row(align=True, heading="Max B-frames")
        row.prop(ffmpeg, "use_max_b_frames", text="")
        sub = row.row(align=True)
        sub.active = ffmpeg.use_max_b_frames
        sub.prop(ffmpeg, "max_b_frames", text="")

        if not use_crf or ffmpeg.constant_rate_factor == 'NONE':
            col = layout.column()

            sub = col.column(align=True)
            sub.prop(ffmpeg, "video_bitrate")
            sub.prop(ffmpeg, "minrate", text="Minimum")
            sub.prop(ffmpeg, "maxrate", text="Maximum")

            col.prop(ffmpeg, "buffersize", text="Buffer")

            col.separator()

            col.prop(ffmpeg, "muxrate", text="Mux Rate")
            col.prop(ffmpeg, "packetsize", text="Mux Packet Size")


class RENDER_PT_encoding_audio(RenderOutputButtonsPanel, Panel):
    bl_label = "Audio"
    bl_parent_id = "RENDER_PT_encoding"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return rd.image_settings.file_format in {'FFMPEG', 'XVID', 'H264', 'THEORA'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        rd = context.scene.render
        ffmpeg = rd.ffmpeg

        if ffmpeg.format != 'MP3':
            layout.prop(ffmpeg, "audio_codec", text="Audio Codec")

        if ffmpeg.audio_codec != 'NONE':
            layout.prop(ffmpeg, "audio_bitrate")
            layout.prop(ffmpeg, "audio_volume", slider=True)


class RENDER_UL_renderviews(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, index):
        view = item
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            if view.name in {"left", "right"}:
                layout.label(text=view.name, icon_value=icon + (not view.use))
            else:
                layout.prop(view, "name", text="", index=index, icon_value=icon, emboss=False)
            layout.prop(view, "use", text="", index=index)

        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon + (not view.use))


class RENDER_PT_stereoscopy(RenderOutputButtonsPanel, Panel):
    bl_label = "Stereoscopy"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}
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
        layout.row().prop(rd, "views_format", expand=True)

        if basic_stereo:
            row = layout.row()
            row.template_list("RENDER_UL_renderviews", "name", rd, "stereo_views", rd.views, "active_index", rows=2)

            row = layout.row()
            row.use_property_split = True
            row.use_property_decorate = False
            row.prop(rv, "file_suffix")

        else:
            row = layout.row()
            row.template_list("RENDER_UL_renderviews", "name", rd, "views", rd.views, "active_index", rows=2)

            col = row.column(align=True)
            col.operator("scene.render_view_add", icon='ADD', text="")
            col.operator("scene.render_view_remove", icon='REMOVE', text="")

            row = layout.row()
            row.use_property_split = True
            row.use_property_decorate = False
            row.prop(rv, "camera_suffix")


classes = (
    RENDER_PT_presets,
    RENDER_PT_ffmpeg_presets,
    RENDER_MT_framerate_presets,
    RENDER_PT_dimensions,
    RENDER_PT_frame_remapping,
    RENDER_PT_stereoscopy,
    RENDER_PT_output,
    RENDER_PT_output_views,
    RENDER_PT_encoding,
    RENDER_PT_encoding_video,
    RENDER_PT_encoding_audio,
    RENDER_PT_stamp,
    RENDER_PT_stamp_note,
    RENDER_PT_stamp_burn,
    RENDER_UL_renderviews,
    RENDER_PT_post_processing,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
