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
from bpy.types import Header


class STATUSBAR_HT_header(Header):
    bl_space_type = 'STATUSBAR'

    def draw(self, context):
        area = context.area
        region = context.region

        if region.alignment == 'RIGHT':
            if region == area.regions[0]:
                self.draw_right(context)
            else:
                self.draw_center(context)
        else:
            self.draw_left(context)

    def draw_left(self, context):
        layout = self.layout

        row = layout.row(align=True)
        if (bpy.data.filepath):
            row.label(text=bpy.data.filepath, translate=False)
        if bpy.data.is_dirty:
            row.label("(Modified)")

    def draw_center(self, context):
        layout = self.layout

        scene = context.scene
        view_layer = context.view_layer

        layout.label(text=scene.statistics(view_layer), translate=False)

    def draw_right(self, context):
        layout = self.layout

        layout.template_running_jobs()
        layout.template_reports_banner()

        row = layout.row(align=True)
        if bpy.app.autoexec_fail is True and bpy.app.autoexec_fail_quiet is False:
            row.label("Auto-run disabled", icon='ERROR')
            if bpy.data.is_saved:
                props = row.operator("wm.revert_mainfile", icon='SCREEN_BACK', text="Reload Trusted")
                props.use_scripts = True

            row.operator("script.autoexec_warn_clear", text="Ignore")

            # include last so text doesn't push buttons out of the header
            row.label(bpy.app.autoexec_fail_message)
            return



classes = (
    STATUSBAR_HT_header,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
