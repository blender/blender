# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

bl_info = {
    "name": "Scalable Vector Graphics (SVG) 1.1 format",
    "author": "JM Soler, Sergey Sharybin",
    "blender": (2, 80, 0),
    "location": "File > Import > Scalable Vector Graphics (.svg)",
    "description": "Import SVG as curves",
    "warning": "",
    "doc_url": "{BLENDER_MANUAL_URL}/addons/import_export/curve_svg.html",
    "support": 'OFFICIAL',
    "category": "Import-Export",
}


# To support reload properly, try to access a package var,
# if it's there, reload everything
if "bpy" in locals():
    import importlib
    if "import_svg" in locals():
        importlib.reload(import_svg)


import bpy
from bpy.props import StringProperty
from bpy_extras.io_utils import ImportHelper


class ImportSVG(bpy.types.Operator, ImportHelper):
    """Load a SVG file"""
    bl_idname = "import_curve.svg"
    bl_label = "Import SVG"
    bl_options = {'UNDO'}

    filename_ext = ".svg"
    filter_glob: StringProperty(default="*.svg", options={'HIDDEN'})

    def execute(self, context):
        from . import import_svg

        return import_svg.load(self, context, filepath=self.filepath)


def menu_func_import(self, context):
    self.layout.operator(ImportSVG.bl_idname,
                         text="Scalable Vector Graphics (.svg)")


def register():
    bpy.utils.register_class(ImportSVG)

    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)


def unregister():
    bpy.utils.unregister_class(ImportSVG)

    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)

# NOTES
# - blender version is hardcoded


if __name__ == "__main__":
    register()
