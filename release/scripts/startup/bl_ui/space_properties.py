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
from bpy.types import Header, Panel


class PROPERTIES_HT_header(Header):
    bl_space_type = 'PROPERTIES'

    def draw(self, context):
        layout = self.layout
        view = context.space_data
        region = context.region
        ui_scale = context.preferences.system.ui_scale

        layout.template_header()

        layout.separator_spacer()

        # The following is an ugly attempt to make the search button center-align better visually.
        # A dummy icon is inserted that has to be scaled as the available width changes.
        content_size_est = 160 * ui_scale
        layout_scale = min(1, max(0, (region.width / content_size_est) - 1))
        if layout_scale > 0:
            row = layout.row()
            row.scale_x = layout_scale
            row.label(icon='BLANK1')

        layout.prop(view, "search_filter", icon='VIEWZOOM', text="")

        layout.separator_spacer()


class PROPERTIES_PT_navigation_bar(Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'NAVIGATION_BAR'
    bl_label = "Navigation Bar"
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        view = context.space_data

        layout.scale_x = 1.4
        layout.scale_y = 1.4
        if view.search_filter:
            layout.prop_tabs_enum(
                view, "context", data_highlight=view,
                property_highlight="tab_search_results", icon_only=True,
            )
        else:
            layout.prop_tabs_enum(view, "context", icon_only=True)


classes = (
    PROPERTIES_HT_header,
    PROPERTIES_PT_navigation_bar,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
