# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

bl_info = {
    "name": "Scalable Vector Graphics (SVG) 1.1 format",
    # This is now displayed as the maintainer, so show the foundation.
    # "author": "JM Soler, Sergey Sharybin", # Original Authors
    "author": "Blender Foundation",
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


import os
import bpy
from bpy.props import (
    StringProperty,
    CollectionProperty
)
from bpy_extras.io_utils import (
    ImportHelper,
    poll_file_object_drop,
)


class ImportSVG(bpy.types.Operator, ImportHelper):
    """Load a SVG file"""
    bl_idname = "import_curve.svg"
    bl_label = "Import SVG"
    bl_options = {'UNDO'}

    filename_ext = ".svg"
    filter_glob: StringProperty(default="*.svg", options={'HIDDEN'})

    directory: StringProperty(subtype='DIR_PATH', options={'SKIP_SAVE', 'HIDDEN'})
    files: CollectionProperty(
        name="File Path",
        type=bpy.types.OperatorFileListElement,
    )

    def invoke(self, context, event):
        if self.properties.is_property_set("filepath"):
            return self.execute(context)
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def execute(self, context):
        from . import import_svg

        if self.files:
            ret = {'CANCELLED'}
            for file in self.files:
                path = os.path.join(self.directory, file.name)
                if import_svg.load(self, context, filepath=path) == {'FINISHED'}:
                    ret = {'FINISHED'}
            return ret
        else:
            return import_svg.load(self, context, filepath=self.filepath)


class IO_FH_svg_as_curves(bpy.types.FileHandler):
    bl_idname = "IO_FH_svg_as_curves"
    bl_label = "SVG as Curves"
    bl_import_operator = "import_curve.svg"
    bl_file_extensions = ".svg"

    @classmethod
    def poll_drop(cls, context):
        return poll_file_object_drop(context)


def menu_func_import(self, context):
    self.layout.operator(ImportSVG.bl_idname,
                         text="Scalable Vector Graphics (.svg)")


classes = [
    ImportSVG,
    IO_FH_svg_as_curves,
]


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)


def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)

    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)

# NOTES
# - blender version is hardcoded


if __name__ == "__main__":
    register()
