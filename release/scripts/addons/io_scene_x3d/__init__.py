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
    "name": "Web3D X3D/VRML2 format",
    "author": "Campbell Barton, Bart, Bastien Montagne, Seva Alekseyev",
    "version": (1, 2, 0),
    "blender": (2, 76, 0),
    "location": "File > Import-Export",
    "description": "Import-Export X3D, Import VRML2",
    "warning": "",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/Scripts/Import-Export/Web3D",
    "support": 'OFFICIAL',
    "category": "Import-Export",
}

if "bpy" in locals():
    import importlib
    if "import_x3d" in locals():
        importlib.reload(import_x3d)
    if "export_x3d" in locals():
        importlib.reload(export_x3d)

import bpy
from bpy.props import (
        BoolProperty,
        EnumProperty,
        FloatProperty,
        StringProperty,
        )
from bpy_extras.io_utils import (
        ImportHelper,
        ExportHelper,
        orientation_helper_factory,
        axis_conversion,
        path_reference_mode,
        )


IOX3DOrientationHelper = orientation_helper_factory("IOX3DOrientationHelper", axis_forward='Z', axis_up='Y')


class ImportX3D(bpy.types.Operator, ImportHelper, IOX3DOrientationHelper):
    """Import an X3D or VRML2 file"""
    bl_idname = "import_scene.x3d"
    bl_label = "Import X3D/VRML2"
    bl_options = {'PRESET', 'UNDO'}

    filename_ext = ".x3d"
    filter_glob = StringProperty(default="*.x3d;*.wrl", options={'HIDDEN'})

    def execute(self, context):
        from . import import_x3d

        keywords = self.as_keywords(ignore=("axis_forward",
                                            "axis_up",
                                            "filter_glob",
                                            ))
        global_matrix = axis_conversion(from_forward=self.axis_forward,
                                        from_up=self.axis_up,
                                        ).to_4x4()
        keywords["global_matrix"] = global_matrix

        return import_x3d.load(context, **keywords)


class ExportX3D(bpy.types.Operator, ExportHelper, IOX3DOrientationHelper):
    """Export selection to Extensible 3D file (.x3d)"""
    bl_idname = "export_scene.x3d"
    bl_label = 'Export X3D'
    bl_options = {'PRESET'}

    filename_ext = ".x3d"
    filter_glob = StringProperty(default="*.x3d", options={'HIDDEN'})

    use_selection = BoolProperty(
            name="Selection Only",
            description="Export selected objects only",
            default=False,
            )
    use_mesh_modifiers = BoolProperty(
            name="Apply Modifiers",
            description="Use transformed mesh data from each object",
            default=True,
            )
    use_triangulate = BoolProperty(
            name="Triangulate",
            description="Write quads into 'IndexedTriangleSet'",
            default=False,
            )
    use_normals = BoolProperty(
            name="Normals",
            description="Write normals with geometry",
            default=False,
            )
    use_compress = BoolProperty(
            name="Compress",
            description="Compress the exported file",
            default=False,
            )
    use_hierarchy = BoolProperty(
            name="Hierarchy",
            description="Export parent child relationships",
            default=True,
            )
    name_decorations = BoolProperty(
            name="Name decorations",
            description=("Add prefixes to the names of exported nodes to "
                         "indicate their type"),
            default=True,
            )
    use_h3d = BoolProperty(
            name="H3D Extensions",
            description="Export shaders for H3D",
            default=False,
            )

    global_scale = FloatProperty(
            name="Scale",
            min=0.01, max=1000.0,
            default=1.0,
            )

    path_mode = path_reference_mode

    def execute(self, context):
        from . import export_x3d

        from mathutils import Matrix

        keywords = self.as_keywords(ignore=("axis_forward",
                                            "axis_up",
                                            "global_scale",
                                            "check_existing",
                                            "filter_glob",
                                            ))
        global_matrix = axis_conversion(to_forward=self.axis_forward,
                                        to_up=self.axis_up,
                                        ).to_4x4() * Matrix.Scale(self.global_scale, 4)
        keywords["global_matrix"] = global_matrix

        return export_x3d.save(context, **keywords)


def menu_func_import(self, context):
    self.layout.operator(ImportX3D.bl_idname,
                         text="X3D Extensible 3D (.x3d/.wrl)")


def menu_func_export(self, context):
    self.layout.operator(ExportX3D.bl_idname,
                         text="X3D Extensible 3D (.x3d)")


def register():
    bpy.utils.register_module(__name__)

    bpy.types.INFO_MT_file_import.append(menu_func_import)
    bpy.types.INFO_MT_file_export.append(menu_func_export)


def unregister():
    bpy.utils.unregister_module(__name__)

    bpy.types.INFO_MT_file_import.remove(menu_func_import)
    bpy.types.INFO_MT_file_export.remove(menu_func_export)

# NOTES
# - blender version is hardcoded

if __name__ == "__main__":
    register()
