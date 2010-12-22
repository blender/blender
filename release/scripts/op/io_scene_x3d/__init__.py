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

# To support reload properly, try to access a package var, if it's there, reload everything
if "bpy" in locals():
    import imp
    if "export_x3d" in locals():
        imp.reload(export_x3d)


import bpy
from bpy.props import *
from io_utils import ExportHelper


class ExportX3D(bpy.types.Operator, ExportHelper):
    '''Export selection to Extensible 3D file (.x3d)'''
    bl_idname = "export_scene.x3d"
    bl_label = 'Export X3D'

    filename_ext = ".x3d"
    filter_glob = StringProperty(default="*.x3d", options={'HIDDEN'})

    use_apply_modifiers = BoolProperty(name="Apply Modifiers", description="Use transformed mesh data from each object", default=True)
    use_triangulate = BoolProperty(name="Triangulate", description="Triangulate quads.", default=False)
    use_compress = BoolProperty(name="Compress", description="GZip the resulting file, requires a full python install", default=False)

    def execute(self, context):
        from . import export_x3d
        return export_x3d.save(self, context, **self.as_keywords(ignore=("check_existing", "filter_glob")))


def menu_func(self, context):
    self.layout.operator(ExportX3D.bl_idname, text="X3D Extensible 3D (.x3d)")


def register():
    bpy.types.INFO_MT_file_export.append(menu_func)

def unregister():
    bpy.types.INFO_MT_file_export.remove(menu_func)

# NOTES
# - blender version is hardcoded

if __name__ == "__main__":
    register()
