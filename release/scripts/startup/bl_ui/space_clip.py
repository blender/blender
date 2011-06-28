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

            if clip:
                sub.menu("CLIP_MT_select")
                sub.menu("CLIP_MT_edit")

        layout.template_ID(sc, "clip")


class CLIP_PT_tools(bpy.types.Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Tools"

    def draw(self, context):
        layout = self.layout
        clip = context.space_data.clip

        if clip:
            col = layout.column(align=True)

            col.label(text="Transform:")
            col.operator("transform.translate")
            col.operator("transform.resize")

            col.label(text="Marker:")
            col.operator("clip.add_marker_move")
            col.operator("clip.delete")

            col = layout.column(align=True)
            col.label(text="2D tracking:")
            row = col.row(align=True)

            op = row.operator("clip.track_markers", text="", icon='REW')
            op.sequence = True
            op.backwards = True
            op = row.operator("clip.track_markers", text="", icon='PLAY_REVERSE')
            op.backwards = True
            row.operator("clip.track_markers", text="", icon='PLAY')
            op = row.operator("clip.track_markers", text="", icon='FF')
            op.sequence = True

            layout.operator("clip.clear_track_path")
        else:
            layout.operator('clip.open')


class CLIP_PT_track(bpy.types.Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Track"

    @classmethod
    def poll(cls, context):
        sc = context.space_data
        clip = sc.clip

        return clip and clip.tracking.act_track

    def draw(self, context):
        layout = self.layout
        sc = context.space_data
        clip = context.space_data.clip

        layout.template_track(clip.tracking, "act_track", sc.clip_user, clip)

        act_track = clip.tracking.act_track

        if act_track:
            row = layout.row()
            row.prop(act_track, "use_red_channel", text="Red")
            row.prop(act_track, "use_green_channel", text="Green")
            row.prop(act_track, "use_blue_channel", text="Blue")


class CLIP_PT_track_settings(bpy.types.Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Tracking Settings"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        sc = context.space_data

        return sc.clip

    def draw(self, context):
        layout = self.layout
        clip = context.space_data.clip
        settings = clip.tracking.settings

        layout.prop(settings, "speed")
        layout.prop(settings, "use_frames_limit")

        row = layout.row()
        row.active = settings.use_frames_limit
        row.prop(settings, "frames_limit")


class CLIP_PT_tracking_camera(bpy.types.Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Camera Data"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        sc = context.space_data

        return sc.clip

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        layout.prop(clip.tracking.camera, "focal_length")


class CLIP_PT_display(bpy.types.Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Display"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        sc = context.space_data

        layout.prop(sc, "show_marker_pattern")
        layout.prop(sc, "show_marker_search")
        layout.prop(sc, "lock_selection")
        layout.prop(sc, "show_marker_path")

        row = layout.row()
        row.active = sc.show_marker_path
        row.prop(sc, "path_length")


class CLIP_PT_test(bpy.types.Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Test"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        sc = context.space_data

        layout.operator("clip.track_to_fcurves")


class CLIP_PT_debug(bpy.types.Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Debug"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        sc = context.space_data

        layout.prop(sc, "show_cache")
        layout.prop(sc, "show_tiny_markers")


class CLIP_PT_footage(bpy.types.Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Footage Settings"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        sc = context.space_data

        return sc.clip

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        if clip:
            layout.template_movieclip(sc, "clip", sc.clip_user, compact=True)
        else:
            layout.operator("clip.open", icon='FILESEL')


class CLIP_MT_view(bpy.types.Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        layout.operator("clip.properties", icon='MENU_PANEL')
        layout.operator("clip.tools", icon='MENU_PANEL')
        layout.separator()

        layout.operator("clip.view_selected")
        layout.operator("clip.view_all")

        layout.separator()
        layout.operator("clip.view_zoom_in")
        layout.operator("clip.view_zoom_out")

        layout.separator()

        ratios = [[1, 8], [1, 4], [1, 2], [1, 1], [2, 1], [4, 1], [8, 1]]

        for a, b in ratios:
            text = "Zoom %d:%d" % (a, b)
            layout.operator("clip.view_zoom_ratio", text=text).ratio = a / b

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
            layout.operator("clip.reload")

        layout.operator("clip.open")


class CLIP_MT_edit(bpy.types.Menu):
    bl_label = "Edit"

    def draw(self, context):
        layout = self.layout

        sc = context.space_data

        layout.operator("clip.delete")
        layout.menu("CLIP_MT_marker")


class CLIP_MT_marker(bpy.types.Menu):
    bl_label = "Marker"

    def draw(self, context):
        layout = self.layout

        sc = context.space_data

        layout.operator("clip.add_marker_move")


class CLIP_MT_select(bpy.types.Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        sc = context.space_data

        layout.operator("clip.select_border")
        layout.operator("clip.select_circle")
        layout.operator("clip.select_all", text="Select/Deselect all")
        layout.operator("clip.select_all", text="Inverse").action = 'INVERT'

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
