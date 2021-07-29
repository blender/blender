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

bl_info = {
    "name": "Stored Views",
    "description": "Save and restore User defined views, pov, layers and display configs",
    "author": "nfloyd, Francesco Siddi",
    "version": (0, 3, 6),
    "blender": (2, 7, 8),
    "location": "View3D > Properties > Stored Views",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.5/"
                "Py/Scripts/3D_interaction/stored_views",
    "category": "3D View"}

"""
ACKNOWLEDGMENT
==============
import/export functionality is mostly based
on Bart Crouch's Theme Manager Addon

TODO: quadview complete support : investigate. Where's the data?
TODO: lock_camera_and_layers. investigate usage
TODO: list reordering

NOTE: logging setup has to be provided by the user in a separate config file
    as Blender will not try to configure logging by default in an add-on
    The Config File should be in the Blender Config folder > /scripts/startup/config_logging.py
    For setting up /location of the config folder see:
    https://docs.blender.org/manual/en/dev/getting_started/
    installing/configuration/directories.html
    For configuring logging itself in the file, general Python documentation should work
    As the logging calls are not configured, they can be kept in the other modules of this add-on
    and will not have output until the logging configuration is set up
"""

if "bpy" in locals():
    import importlib
    importlib.reload(core)
    importlib.reload(ui)
    importlib.reload(properties)
    importlib.reload(operators)
    importlib.reload(io)
else:
    from . import core
    from . import ui
    from . import properties
    from . import operators
    from . import io

import bpy
from bpy.props import (
        BoolProperty,
        IntProperty,
        PointerProperty,
        )
from bpy.types import (
        AddonPreferences,
        Operator,
        )


class VIEW3D_stored_views_initialize(Operator):
    bl_idname = "view3d.stored_views_initialize"
    bl_label = "Initialize"

    @classmethod
    def poll(cls, context):
        return not hasattr(bpy.types.Scene, 'stored_views')

    def execute(self, context):
        bpy.types.Scene.stored_views = PointerProperty(
                                            type=properties.StoredViewsData
                                            )
        scenes = bpy.data.scenes
        for scene in scenes:
            core.DataStore.sanitize_data(scene)
        return {'FINISHED'}


# Addon Preferences

class VIEW3D_stored_views_preferences(AddonPreferences):
    bl_idname = __name__

    show_exporters = BoolProperty(
            name="Enable I/O Operators",
            default=False,
            description="Enable Import/Export Operations in the UI:\n"
                        "Import Stored Views preset,\n"
                        "Export Stored Views preset and \n"
                        "Import stored views from scene",
            )
    view_3d_update_rate = IntProperty(
            name="3D view update",
            description="Update rate of the 3D view redraw\n"
                        "Increse the value if the UI feels sluggish",
            min=1, max=10,
            default=1
            )

    def draw(self, context):
        layout = self.layout

        row = layout.row(align=True)
        row.prop(self, "view_3d_update_rate", toggle=True)
        row.prop(self, "show_exporters", toggle=True)


def register():
    bpy.utils.register_module(__name__)


def unregister():
    ui.VIEW3D_stored_views_draw.handle_remove(bpy.context)
    bpy.utils.unregister_module(__name__)
    if hasattr(bpy.types.Scene, "stored_views"):
        del bpy.types.Scene.stored_views


if __name__ == "__main__":
    register()
