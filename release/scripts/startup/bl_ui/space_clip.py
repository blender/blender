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

        row = layout.row(align=True)
        row.template_header()

        if context.area.show_menus:
            sub = row.row(align=True)
            sub.menu("CLIP_MT_view")
            sub.menu("CLIP_MT_clip")

        layout.template_ID(sc, "clip")


class CLIP_PT_tools(bpy.types.Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Tools"

    def draw(self, context):
        layout = self.layout
        clip = context.space_data.clip

        if clip:
            ts = context.tool_settings
            col = layout.column()
            col.prop(ts.movieclip, 'tool', expand=True)
        else:
            layout.operator('clip.open', icon='FILESEL')

class CLIP_PT_footage_camera(bpy.types.Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Footage Settings"

    @classmethod
    def poll(cls, context):
        clip = context.space_data.clip
        ts = context.tool_settings
        tool = ts.movieclip.tool

        return (clip and tool == 'FOOTAGE')

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        layout.label(text="File Path:")
        layout.prop(clip, "filepath", text="")


class CLIP_PT_tracking_camera(bpy.types.Panel):
    bl_space_type = 'CLIP_EDITOR'
    bl_region_type = 'TOOLS'
    bl_label = "Camera Data"

    @classmethod
    def poll(cls, context):
        clip = context.space_data.clip
        ts = context.tool_settings
        tool = ts.movieclip.tool

        return (clip and tool == 'CAMERA')

    def draw(self, context):
        layout = self.layout

        sc = context.space_data
        clip = sc.clip

        layout.prop(clip.tracking.camera, "focal_length")


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

        layout.operator('clip.open')

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
