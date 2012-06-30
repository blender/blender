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

class ColorManagedViewSettingsPanel:

    def draw(self, context):
        layout = self.layout

        space = context.space_data
        window = context.window
        space_view_settings = space.view_settings

        if space_view_settings.use_global_settings:
            view_settings = window.view_settings
        else:
            view_settings = space.view_settings

        col = layout.column()
        col.prop(space_view_settings, "use_global_settings")
        col.prop(window, "display_device", text="Display")
        col.prop(view_settings, "view_transform", text="View")

        col = layout.column()
        col.active = view_settings.view_transform not in {'ACES ODT Tonecurve', 'NONE'}
        col.prop(view_settings, "exposure")
        col.prop(view_settings, "gamma")

