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

# To support reload properly, try to access a package var, if it's there, reload everything
if "bpy" in locals():
    import imp
    if "export_ply" in locals():
        imp.reload(export_ply)
    if "import_ply" in locals():
        imp.reload(import_ply)


import bpy
from bpy.props import *
from io_utils import ImportHelper, ExportHelper


class ImportPLY(bpy.types.Operator, ImportHelper):
    '''Load a BVH motion capture file'''
    bl_idname = "import_mesh.ply"
    bl_label = "Import PLY"

    filename_ext = ".ply"
    filter_glob = StringProperty(default="*.ply", options={'HIDDEN'})

    def execute(self, context):
        from . import import_ply
        return import_ply.load(self, context, **self.as_keywords(ignore=("filter_glob",)))


class ExportPLY(bpy.types.Operator, ExportHelper):
    '''Export a single object as a stanford PLY with normals, colours and texture coordinates.'''
    bl_idname = "export_mesh.ply"
    bl_label = "Export PLY"

    filename_ext = ".ply"
    filter_glob = StringProperty(default="*.ply", options={'HIDDEN'})

    use_modifiers = BoolProperty(name="Apply Modifiers", description="Apply Modifiers to the exported mesh", default=True)
    use_normals = BoolProperty(name="Normals", description="Export Normals for smooth and hard shaded faces", default=True)
    use_uv_coords = BoolProperty(name="UVs", description="Exort the active UV layer", default=True)
    use_colors = BoolProperty(name="Vertex Colors", description="Exort the active vertex color layer", default=True)

    @classmethod
    def poll(cls, context):
        return context.active_object != None

    def execute(self, context):
        filepath = self.filepath
        filepath = bpy.path.ensure_ext(filepath, self.filename_ext)
        from . import export_ply
        return export_ply.save(self, context, **self.as_keywords(ignore=("check_existing", "filter_glob")))

    def draw(self, context):
        layout = self.layout

        row = layout.row()
        row.prop(self, "use_modifiers")
        row.prop(self, "use_normals")
        row = layout.row()
        row.prop(self, "use_uv_coords")
        row.prop(self, "use_colors")


def menu_func_import(self, context):
    self.layout.operator(ImportPLY.bl_idname, text="Stanford (.ply)")


def menu_func_export(self, context):
    self.layout.operator(ExportPLY.bl_idname, text="Stanford (.ply)")


def register():
    bpy.types.INFO_MT_file_import.append(menu_func_import)
    bpy.types.INFO_MT_file_export.append(menu_func_export)


def unregister():
    bpy.types.INFO_MT_file_import.remove(menu_func_import)
    bpy.types.INFO_MT_file_export.remove(menu_func_export)

if __name__ == "__main__":
    register()
