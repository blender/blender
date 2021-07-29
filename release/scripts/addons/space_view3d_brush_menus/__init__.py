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
# Modified by Meta-Androcto

""" Copyright 2011 GPL licence applies"""

bl_info = {
    "name": "Sculpt/Paint Brush Menus",
    "description": "Fast access to brushes & tools in Sculpt and Paint Modes",
    "author": "Ryan Inch (Imaginer)",
    "version": (1, 1, 6),
    "blender": (2, 78, 0),
    "location": "Alt V in Sculpt/Paint Modes",
    "warning": '',
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/3D_interaction/Advanced_UI_Menus",
    "category": "3D View"}


if "bpy" in locals():
    import importlib
    importlib.reload(utils_core)
    importlib.reload(brush_menu)
    importlib.reload(brushes)
    importlib.reload(curve_menu)
    importlib.reload(dyntopo_menu)
    importlib.reload(stroke_menu)
    importlib.reload(symmetry_menu)
    importlib.reload(texture_menu)
else:
    from . import utils_core
    from . import brush_menu
    from . import brushes
    from . import curve_menu
    from . import dyntopo_menu
    from . import stroke_menu
    from . import symmetry_menu
    from . import texture_menu


import bpy
from bpy.types import AddonPreferences
from bpy.props import (
        EnumProperty,
        IntProperty,
        )


class VIEW3D_MT_Brushes_Pref(AddonPreferences):
    bl_idname = __name__

    use_brushes_menu_type = EnumProperty(
        name="Choose Brushes Selection",
        description="",
        items=[('lists', "Use compact Menus",
                "Use more compact menus instead  \n"
                "of thumbnails for displaying brushes"),
               ('template', "Template ID Preview",
                "Use Template ID preview menu (thumbnails) for brushes\n"
                "(Still part of the menu)"),
               ('popup', "Pop up menu",
                "Use a separate pop-up window for accessing brushes")
            ],
        default='lists'
        )
    column_set = IntProperty(
        name="Number of Columns",
        description="Number of columns used for the brushes menu",
        default=2,
        min=1,
        max=10
        )

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop(self, "use_brushes_menu_type", expand=True)
        col.prop(self, "column_set", slider=True)


# New hotkeys and registration

addon_keymaps = []


def register():
    # register all blender classes
    bpy.utils.register_module(__name__)

    # set the add-on name variable to access the preferences
    utils_core.get_addon_name = __name__

    # register hotkeys
    wm = bpy.context.window_manager
    modes = ('Sculpt', 'Vertex Paint', 'Weight Paint', 'Image Paint', 'Particle')

    for mode in modes:
        km = wm.keyconfigs.addon.keymaps.new(name=mode)
        kmi = km.keymap_items.new('wm.call_menu', 'V', 'PRESS', alt=True)
        kmi.properties.name = "VIEW3D_MT_sv3_brush_options"
        addon_keymaps.append((km, kmi))


def unregister():
    for km, kmi in addon_keymaps:
        km.keymap_items.remove(kmi)
    addon_keymaps.clear()

    # unregister all blender classes
    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
