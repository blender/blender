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
    if "import_3ds" in locals():
        imp.reload(import_3ds)
    if "export_3ds" in locals():
        imp.reload(export_3ds)


import bpy
from bpy.props import *
from io_utils import ImportHelper, ExportHelper


class Import3DS(bpy.types.Operator, ImportHelper):
    '''Import from 3DS file format (.3ds)'''
    bl_idname = "import_scene.autodesk_3ds"
    bl_label = 'Import 3DS'

    filename_ext = ".3ds"
    filter_glob = StringProperty(default="*.3ds", options={'HIDDEN'})

    constrain_size = FloatProperty(name="Size Constraint", description="Scale the model by 10 until it reacehs the size constraint. Zero Disables.", min=0.0, max=1000.0, soft_min=0.0, soft_max=1000.0, default=10.0)
    use_image_search = BoolProperty(name="Image Search", description="Search subdirectories for any assosiated images (Warning, may be slow)", default=True)
    use_apply_transform = BoolProperty(name="Apply Transform", description="Workaround for object transformations importing incorrectly", default=True)

    def execute(self, context):
        from . import import_3ds
        return import_3ds.load(self, context, **self.as_keywords(ignore=("filter_glob",)))


class Export3DS(bpy.types.Operator, ExportHelper):
    '''Export to 3DS file format (.3ds)'''
    bl_idname = "export_scene.autodesk_3ds"
    bl_label = 'Export 3DS'

    filename_ext = ".3ds"
    filter_glob = StringProperty(default="*.3ds", options={'HIDDEN'})

    def execute(self, context):
        from . import export_3ds
        return export_3ds.save(self, context, **self.as_keywords(ignore=("check_existing", "filter_glob")))


# Add to a menu
def menu_func_export(self, context):
    self.layout.operator(Export3DS.bl_idname, text="3D Studio (.3ds)")


def menu_func_import(self, context):
    self.layout.operator(Import3DS.bl_idname, text="3D Studio (.3ds)")


def register():
    bpy.types.INFO_MT_file_import.append(menu_func_import)
    bpy.types.INFO_MT_file_export.append(menu_func_export)


def unregister():
    bpy.types.INFO_MT_file_import.remove(menu_func_import)
    bpy.types.INFO_MT_file_export.remove(menu_func_export)

# NOTES:
# why add 1 extra vertex? and remove it when done? - "Answer - eekadoodle - would need to re-order UV's without this since face order isnt always what we give blender, BMesh will solve :D"
# disabled scaling to size, this requires exposing bb (easy) and understanding how it works (needs some time)

if __name__ == "__main__":
    register()
