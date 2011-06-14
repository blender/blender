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


class CLIP_HT_header(bpy.types.Header):
    bl_space_type = 'CLIP_EDITOR'

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        row = layout.row(align=True)
        row.template_header()

        if context.area.show_menus:
            sub = row.row(align=True)
            sub.menu("CLIP_MT_view")
            sub.menu("CLIP_MT_clip")

            if clip and sc.mode == 'TRACKING':
                sub.menu("CLIP_MT_select")
                sub.menu("CLIP_MT_edit")

        layout.template_ID(sc, "clip")

        if clip:
            layout.prop(sc, "mode", text="")


class CLIP_PT_tools(bpy.types.Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Tools"

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip

        return (sc.mode == 'TRACKING' and clip)

    def draw(self, context):
        layout = self.layout
        clip = context.space_data.clip

        if clip:
            ts = context.tool_settings
            col = layout.column()
            col.prop(ts.movieclip, 'tool', expand=True)


class CLIP_PT_footage(bpy.types.Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Footage Settings"

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip
        ts = context.tool_settings
        tool = ts.movieclip.tool

        if sc.mode == 'TRACKING':
            return clip and tool == 'FOOTAGE'

        return True

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        if clip:
            layout.template_movieclip(sc, "clip", sc.clip_user, compact=True)
        else:
            layout.operator('clip.open', icon='FILESEL')


class CLIP_PT_tracking_camera(bpy.types.Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Camera Data"

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip
        ts = context.tool_settings
        tool = ts.movieclip.tool

        return (sc.mode == 'TRACKING' and clip and tool == 'CAMERA')

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        layout.prop(clip.tracking.camera, "focal_length")


class CLIP_PT_tracking_marker_tools(bpy.types.Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = 'Marker Tools'

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip
        ts = context.tool_settings
        tool = ts.movieclip.tool

        return (sc.mode == 'TRACKING' and clip and tool == 'MARKER')

    def draw(self, context):
        layout = self.layout
        clip = context.space_data.clip

        layout.operator('clip.add_marker_move', icon='ZOOMIN')
        layout.operator('clip.delete', icon='X')


class CLIP_PT_tracking_marker(bpy.types.Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = 'Marker'

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip
        ts = context.tool_settings
        tool = ts.movieclip.tool

        return (sc.mode == 'TRACKING' and clip and \
            tool == 'MARKER' and clip.tracking.act_track)

    def draw(self, context):
        layout = self.layout
        sc = context.space_data
        clip = context.space_data.clip

        layout.template_marker(clip.tracking, 'act_track', sc.clip_user, clip)


class CLIP_PT_track(bpy.types.Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = 'Track Tools'

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip
        ts = context.tool_settings
        tool = ts.movieclip.tool

        return (sc.mode == 'TRACKING' and clip and tool == 'TRACK')

    def draw(self, context):
        layout = self.layout
        clip = context.space_data.clip

        layout.operator('clip.track_markers', icon='PLAY')


class CLIP_PT_track_settings(bpy.types.Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = 'Tracking Settings'
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip
        ts = context.tool_settings
        tool = ts.movieclip.tool

        return (sc.mode == 'TRACKING' and clip and tool == 'TRACK')

    def draw(self, context):
        layout = self.layout
        clip = context.space_data.clip
        settings = clip.tracking.settings

        layout.prop(settings, 'max_iterations')
        layout.prop(settings, 'pyramid_level')
        layout.prop(settings, 'tolerance')

        layout.operator('clip.reset_tracking_settings', \
            text="Reset To Defaults")


class CLIP_PT_display(bpy.types.Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Display"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip

        return sc.mode == 'TRACKING' and clip

    def draw(self, context):
        layout = self.layout
        sc = context.space_data

        layout.prop(sc, "show_marker_pattern")
        layout.prop(sc, "show_marker_search")


class CLIP_PT_debug(bpy.types.Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Debug"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        sc = context.space_data

        layout.prop(sc, "show_cache")


class CLIP_MT_view(bpy.types.Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        # layout.operator("clip.properties", icon='MENU_PANEL')
        layout.operator("clip.tools", icon='MENU_PANEL')
        layout.separator()

        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area")


class CLIP_MT_clip(bpy.types.Menu):
    bl_label = "Clip"

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        if clip:
            layout.operator('clip.reload')

        layout.operator('clip.open')


class CLIP_MT_edit(bpy.types.Menu):
    bl_label = "Edit"

    def draw(self, context):
        layout = self.layout

        sc = context.space_data

        layout.operator('clip.delete')
        layout.menu("CLIP_MT_marker")


class CLIP_MT_marker(bpy.types.Menu):
    bl_label = "Marker"

    def draw(self, context):
        layout = self.layout

        sc = context.space_data

        layout.operator('clip.add_marker')


class CLIP_MT_select(bpy.types.Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        sc = context.space_data

        layout.operator('clip.select_border')
        layout.operator('clip.select_circle')
        layout.operator('clip.select_all', text="Select/Deselect all")
        layout.operator('clip.select_all', text="Inverse").action = 'INVERT'

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
