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

# blender 1 line description
"Raw Mesh IO (File > Import/Export > Raw Faces (.raw))"

import bpy


def menu_import(self, context):
    from io_mesh_raw import import_raw
    self.layout.operator(import_raw.RawImporter.bl_idname, text="Raw Faces (.raw)").path = "*.raw"


def menu_export(self, context):
    from io_mesh_raw import export_raw
    default_path = bpy.data.filename.replace(".blend", ".raw")
    self.layout.operator(export_raw.RawExporter.bl_idname, text="Raw Faces (.raw)").path = default_path


def register():
    from io_mesh_raw import import_raw, export_raw
    bpy.types.register(import_raw.RawImporter)
    bpy.types.register(export_raw.RawExporter)
    bpy.types.INFO_MT_file_import.append(menu_import)
    bpy.types.INFO_MT_file_export.append(menu_export)

def unregister():
    from io_mesh_raw import import_raw, export_raw
    bpy.types.unregister(import_raw.RawImporter)
    bpy.types.unregister(export_raw.RawExporter)
    bpy.types.INFO_MT_file_import.remove(menu_import)
    bpy.types.INFO_MT_file_export.remove(menu_export)

if __name__ == "__main__":
    register()
