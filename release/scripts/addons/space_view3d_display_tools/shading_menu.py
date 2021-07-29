# space_view_3d_display_tools.py Copyright (C) 2014, Jordi Vall-llovera
# Multiple display tools for fast navigate/interact with the viewport

# ##### BEGIN GPL LICENSE BLOCK #####
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENCE BLOCK #####

bl_info = {
    "name": "shade Tools",
    "author": "Jordi Vall-llovera Medina, Jhon Wallace",
    "version": (1, 6, 0),
    "blender": (2, 7, 0),
    "location": "Toolshelf",
    "description": "Display tools for fast navigate/interact with the viewport",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/"
                "Py/Scripts/3D_interaction/Display_Tools",
    "category": "3D View"}


import bpy
from bpy.types import Menu


class VIEW3D_MT_Shade_menu(Menu):
    bl_label = "Shade"
    bl_description = "Global Shading settings"

    def draw(self, context):
        layout = self.layout

        layout.prop(context.space_data, "viewport_shade", expand=True)

        if context.space_data.use_matcap:
            row = layout.column(1)
            row.scale_y = 0.3
            row.scale_x = 0.5
            row.template_icon_view(context.space_data, "matcap_icon")


# Register
def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
