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

# <pep8-80 compliant>


bl_info = {
    "name": "Raw mesh format (.raw)",
    "author": "Anthony D,Agostino (Scorpius), Aurel Wildfellner",
    "version": (0, 2),
    "blender": (2, 57, 0),
    "location": "File > Import-Export > Raw Faces (.raw) ",
    "description": "Import-Export Raw Faces",
    "warning": "",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Import-Export/Raw_Mesh_IO",
    "category": "Import-Export",
}

if "bpy" in locals():
    import importlib
    if "import_raw" in locals():
        importlib.reload(import_raw)
    if "export_raw" in locals():
        importlib.reload(export_raw)
else:
    import bpy

from bpy.props import StringProperty, BoolProperty
from bpy_extras.io_utils import ExportHelper


class RawImporter(bpy.types.Operator):
    """Load Raw triangle mesh data"""
    bl_idname = "import_mesh.raw"
    bl_label = "Import RAW"
    bl_options = {'UNDO'}

    filepath = StringProperty(
            subtype='FILE_PATH',
            )
    filter_glob = StringProperty(default="*.raw", options={'HIDDEN'})

    def execute(self, context):
        from . import import_raw
        import_raw.read(self.filepath)
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class RawExporter(bpy.types.Operator, ExportHelper):
    """Save Raw triangle mesh data"""
    bl_idname = "export_mesh.raw"
    bl_label = "Export RAW"

    filename_ext = ".raw"
    filter_glob = StringProperty(default="*.raw", options={'HIDDEN'})

    apply_modifiers = BoolProperty(
            name="Apply Modifiers",
            description="Use transformed mesh data from each object",
            default=True,
            )
    triangulate = BoolProperty(
            name="Triangulate",
            description="Triangulate quads",
            default=True,
            )

    def execute(self, context):
        from . import export_raw
        export_raw.write(self.filepath,
                         self.apply_modifiers,
                         self.triangulate,
                         )

        return {'FINISHED'}


def menu_import(self, context):
    self.layout.operator(RawImporter.bl_idname, text="Raw Faces (.raw)")


def menu_export(self, context):
    self.layout.operator(RawExporter.bl_idname, text="Raw Faces (.raw)")


def register():
    bpy.utils.register_module(__name__)

    bpy.types.INFO_MT_file_import.append(menu_import)
    bpy.types.INFO_MT_file_export.append(menu_export)


def unregister():
    bpy.utils.unregister_module(__name__)

    bpy.types.INFO_MT_file_import.remove(menu_import)
    bpy.types.INFO_MT_file_export.remove(menu_export)

if __name__ == "__main__":
    register()
