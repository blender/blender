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

# <pep8 compliant>
import bpy

narrowui = 180


class RENDER_MT_presets(bpy.types.Menu):
    bl_label = "Render Presets"
    preset_subdir = "render"
    preset_operator = "script.python_file_run"
    draw = bpy.types.Menu.draw_preset


class RenderButtonsPanel(bpy.types.Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    def poll(self, context):
        rd = context.scene.render_data
        return (context.scene and rd.use_game_engine is False) and (rd.engine in self.COMPAT_ENGINES)


class RENDER_PT_render(RenderButtonsPanel):
    bl_label = "Render"
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render_data
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.operator("screen.render", text="Image", icon='ICON_RENDER_STILL')

        if wide_ui:
            col = split.column()
        col.operator("screen.render", text="Animation", icon='ICON_RENDER_ANIMATION').animation = True

        layout.prop(rd, "display_mode", text="Display")


class RENDER_PT_layers(RenderButtonsPanel):
    bl_label = "Layers"
    bl_default_closed = True
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render_data
        wide_ui = context.region.width > narrowui

        row = layout.row()
        row.template_list(rd, "layers", rd, "active_layer_index", rows=2)

        col = row.column(align=True)
        col.operator("scene.render_layer_add", icon='ICON_ZOOMIN', text="")
        col.operator("scene.render_layer_remove", icon='ICON_ZOOMOUT', text="")

        rl = rd.layers[rd.active_layer_index]

        if rl:
            layout.prop(rl, "name")

        split = layout.split()

        col = split.column()
        col.prop(scene, "visible_layers", text="Scene")
        if wide_ui:
            col = split.column()
        col.prop(rl, "visible_layers", text="Layer")

        layout.prop(rl, "light_override", text="Light")
        layout.prop(rl, "material_override", text="Material")

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

        col = split.column()
        col.prop(rl, "sky")
        col.prop(rl, "edge")
        col.prop(rl, "strand")

        if rl.zmask:
            split = layout.split()
            split.label(text="Zmask Layers:")
            split.column().prop(rl, "zmask_layers", text="")

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

        if wide_ui:
            col = split.column()
        col.label()
        col.prop(rl, "pass_color")
        col.prop(rl, "pass_diffuse")
        row = col.row()
        row.prop(rl, "pass_specular")
        row.prop(rl, "pass_specular_exclude", text="", icon='ICON_X')
        row = col.row()
        row.prop(rl, "pass_shadow")
        row.prop(rl, "pass_shadow_exclude", text="", icon='ICON_X')
        row = col.row()
        row.prop(rl, "pass_ao")
        row.prop(rl, "pass_ao_exclude", text="", icon='ICON_X')
        row = col.row()
        row.prop(rl, "pass_reflection")
        row.prop(rl, "pass_reflection_exclude", text="", icon='ICON_X')
        row = col.row()
        row.prop(rl, "pass_refraction")
        row.prop(rl, "pass_refraction_exclude", text="", icon='ICON_X')


class RENDER_PT_shading(RenderButtonsPanel):
    bl_label = "Shading"
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render_data
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.prop(rd, "render_textures", text="Textures")
        col.prop(rd, "render_shadows", text="Shadows")
        col.prop(rd, "render_sss", text="Subsurface Scattering")
        col.prop(rd, "render_envmaps", text="Environment Map")

        if wide_ui:
            col = split.column()
        col.prop(rd, "render_raytracing", text="Ray Tracing")
        col.prop(rd, "color_management")
        col.prop(rd, "alpha_mode", text="Alpha")


class RENDER_PT_performance(RenderButtonsPanel):
    bl_label = "Performance"
    bl_default_closed = True
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render_data
        wide_ui = context.region.width > narrowui

        split = layout.split()

        col = split.column()
        col.label(text="Threads:")
        col.row().prop(rd, "threads_mode", expand=True)
        sub = col.column()
        sub.enabled = rd.threads_mode == 'THREADS_FIXED'
        sub.prop(rd, "threads")
        sub = col.column(align=True)
        sub.label(text="Tiles:")
        sub.prop(rd, "parts_x", text="X")
        sub.prop(rd, "parts_y", text="Y")

        if wide_ui:
            col = split.column()
        col.label(text="Memory:")
        sub = col.column()
        sub.prop(rd, "save_buffers")
        sub.enabled = not rd.full_sample
        sub = col.column()
        sub.active = rd.use_compositing
        sub.prop(rd, "free_image_textures")
        sub = col.column()
        sub.active = rd.render_raytracing
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
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render_data
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


class RENDER_PT_output(RenderButtonsPanel):
    bl_label = "Output"
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render_data
        wide_ui = context.region.width > narrowui

        layout.prop(rd, "output_path", text="")

        split = layout.split()
        col = split.column()
        col.prop(rd, "file_format", text="")
        col.row().prop(rd, "color_mode", text="Color", expand=True)

        if wide_ui:
            col = split.column()
        col.prop(rd, "file_extensions")
        col.prop(rd, "use_overwrite")
        col.prop(rd, "use_placeholder")

        if rd.file_format in ('AVIJPEG', 'JPEG'):
            split = layout.split()
            split.prop(rd, "quality", slider=True)

        elif rd.file_format == 'OPENEXR':
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


class RENDER_PT_QTencoding(RenderButtonsPanel):
    bl_label = "Encoding"
    bl_default_closed = True
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

    def poll(self, context):
        rd = context.scene.render_data
        return rd.file_format in ('QUICKTIME_QTKIT') # QUICKTIME will be added later

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render_data
        wide_ui = context.region.width > narrowui

        split = layout.split()

        split.prop(rd, "quicktime_codec_type")

        split = layout.split()

        if rd.file_format == 'QUICKTIME_QTKIT':
            split.prop(rd, "quicktime_codec_spatial_quality", text="Quality", slider=True)


class RENDER_PT_encoding(RenderButtonsPanel):
    bl_label = "Encoding"
    bl_default_closed = True
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

    def poll(self, context):
        rd = context.scene.render_data
        return rd.file_format in ('FFMPEG', 'XVID', 'H264', 'THEORA')

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render_data
        wide_ui = context.region.width > narrowui

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

        row = layout.row()
        row.label(text="Audio:")
        row = layout.row()
        row.prop(rd, "ffmpeg_audio_codec", text="Codec")

        split = layout.split()

        col = split.column()
        col.prop(rd, "ffmpeg_audio_bitrate")
        col.prop(rd, "ffmpeg_audio_mixrate")

        if wide_ui:
            col = split.column()
        col.prop(rd, "ffmpeg_multiplex_audio")
        col.prop(rd, "ffmpeg_audio_volume", slider=True)


class RENDER_PT_antialiasing(RenderButtonsPanel):
    bl_label = "Anti-Aliasing"
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

    def draw_header(self, context):
        rd = context.scene.render_data

        self.layout.prop(rd, "antialiasing", text="")

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render_data
        wide_ui = context.region.width > narrowui
        layout.active = rd.antialiasing

        split = layout.split()

        col = split.column()
        col.row().prop(rd, "antialiasing_samples", expand=True)
        col.prop(rd, "full_sample")

        if wide_ui:
            col = split.column()
        col.prop(rd, "pixel_filter", text="")
        col.prop(rd, "filter_size", text="Size", slider=True)


class RENDER_PT_dimensions(RenderButtonsPanel):
    bl_label = "Dimensions"
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render_data
        wide_ui = context.region.width > narrowui
        
        row = layout.row().split()
        sub = row.row(align=True).split(percentage=0.75)
        sub.menu("RENDER_MT_presets", text="Presets")
        sub.operator("render.preset_add", text="Add")

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
        sub.prop(scene, "start_frame", text="Start")
        sub.prop(scene, "end_frame", text="End")
        sub.prop(scene, "frame_step", text="Step")

        sub.label(text="Frame Rate:")
        sub.prop(rd, "fps")
        sub.prop(rd, "fps_base", text="/")  


class RENDER_PT_stamp(RenderButtonsPanel):
    bl_label = "Stamp"
    bl_default_closed = True
    COMPAT_ENGINES = set(['BLENDER_RENDER'])

    def draw_header(self, context):
        rd = context.scene.render_data

        self.layout.prop(rd, "render_stamp", text="")

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render_data
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
        col.prop(rd, "stamp_sequence_strip", text="Seq. Strip")

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


bpy.types.register(RENDER_MT_presets)

bpy.types.register(RENDER_PT_render)
bpy.types.register(RENDER_PT_layers)
bpy.types.register(RENDER_PT_dimensions)
bpy.types.register(RENDER_PT_antialiasing)
bpy.types.register(RENDER_PT_shading)
bpy.types.register(RENDER_PT_output)
bpy.types.register(RENDER_PT_QTencoding)
bpy.types.register(RENDER_PT_encoding)
bpy.types.register(RENDER_PT_performance)
bpy.types.register(RENDER_PT_post_processing)
bpy.types.register(RENDER_PT_stamp)
