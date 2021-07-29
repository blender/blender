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
    "name": "Btrace",
    "author": "liero, crazycourier, Atom, Meta-Androcto, MacKracken",
    "version": (1, 2, 1),
    "blender": (2, 78, 0),
    "location": "View3D > Tools",
    "description": "Tools for converting/animating objects/particles into curves",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/Scripts/Curve/Btrace",
    "category": "Add Curve"}

if "bpy" in locals():
    import importlib
    importlib.reload(bTrace_props)
    importlib.reload(bTrace)
else:
    from . import bTrace_props
    from . import bTrace

import bpy
from bpy.types import AddonPreferences
from .bTrace_props import (
        TracerProperties,
        addTracerObjectPanel,
        )
from .bTrace import (
        OBJECT_OT_convertcurve,
        OBJECT_OT_objecttrace,
        OBJECT_OT_objectconnect,
        OBJECT_OT_writing,
        OBJECT_OT_particletrace,
        OBJECT_OT_traceallparticles,
        OBJECT_OT_curvegrow,
        OBJECT_OT_reset,
        OBJECT_OT_fcnoise,
        OBJECT_OT_meshfollow,
        OBJECT_OT_materialChango,
        OBJECT_OT_clearColorblender,
        )
from bpy.props import (
        EnumProperty,
        PointerProperty,
        )


# Add-on Preferences
class btrace_preferences(AddonPreferences):
    bl_idname = __name__

    expand_enum = EnumProperty(
            name="UI Options",
            items=[
                 ('list', "Drop down list",
                  "Show all the items as dropdown list in the Tools Region"),
                 ('col', "Enable Expanded UI Panel",
                  "Show all the items expanded in the Tools Region in a column"),
                 ('row', "Icons only in a row",
                  "Show all the items as icons expanded in a row in the Tools Region")
                  ],
            description="",
            default='list'
            )

    def draw(self, context):
        layout = self.layout
        layout.label("UI Options:")

        row = layout.row(align=True)
        row.prop(self, "expand_enum", text="UI Options", expand=True)


# Define Classes to register
classes = (
    TracerProperties,
    addTracerObjectPanel,
    OBJECT_OT_convertcurve,
    OBJECT_OT_objecttrace,
    OBJECT_OT_objectconnect,
    OBJECT_OT_writing,
    OBJECT_OT_particletrace,
    OBJECT_OT_traceallparticles,
    OBJECT_OT_curvegrow,
    OBJECT_OT_reset,
    OBJECT_OT_fcnoise,
    OBJECT_OT_meshfollow,
    OBJECT_OT_materialChango,
    OBJECT_OT_clearColorblender,
    btrace_preferences,
    )


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.WindowManager.curve_tracer = PointerProperty(type=TracerProperties)


def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)
    del bpy.types.WindowManager.curve_tracer


if __name__ == "__main__":
    register()
