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
    "name": "STL format",
    "author": "Guillaume Bouchard (Guillaum)",
    "version": (1, 1, 2),
    "blender": (2, 74, 0),
    "location": "File > Import-Export > Stl",
    "description": "Import-Export STL files",
    "warning": "",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Import-Export/STL",
    "support": 'OFFICIAL',
    "category": "Import-Export",
}


# @todo write the wiki page

"""
Import-Export STL files (binary or ascii)

- Import automatically remove the doubles.
- Export can export with/without modifiers applied

Issues:

Import:
    - Does not handle endien
"""

if "bpy" in locals():
    import importlib
    if "stl_utils" in locals():
        importlib.reload(stl_utils)
    if "blender_utils" in locals():
        importlib.reload(blender_utils)

import os

import bpy
from bpy.props import (
        StringProperty,
        BoolProperty,
        CollectionProperty,
        EnumProperty,
        FloatProperty,
        )
from bpy_extras.io_utils import (
        ImportHelper,
        ExportHelper,
        orientation_helper_factory,
        axis_conversion,
        )
from bpy.types import (
        Operator,
        OperatorFileListElement,
        )


IOSTLOrientationHelper = orientation_helper_factory("IOSTLOrientationHelper", axis_forward='Y', axis_up='Z')


class ImportSTL(Operator, ImportHelper, IOSTLOrientationHelper):
    """Load STL triangle mesh data"""
    bl_idname = "import_mesh.stl"
    bl_label = "Import STL"
    bl_options = {'UNDO'}

    filename_ext = ".stl"

    filter_glob = StringProperty(
            default="*.stl",
            options={'HIDDEN'},
            )
    files = CollectionProperty(
            name="File Path",
            type=OperatorFileListElement,
            )
    directory = StringProperty(
            subtype='DIR_PATH',
            )

    global_scale = FloatProperty(
            name="Scale",
            soft_min=0.001, soft_max=1000.0,
            min=1e-6, max=1e6,
            default=1.0,
            )

    use_scene_unit = BoolProperty(
            name="Scene Unit",
            description="Apply current scene's unit (as defined by unit scale) to imported data",
            default=False,
            )

    use_facet_normal = BoolProperty(
            name="Facet Normals",
            description="Use (import) facet normals (note that this will still give flat shading)",
            default=False,
            )

    def execute(self, context):
        from . import stl_utils
        from . import blender_utils
        from mathutils import Matrix

        paths = [os.path.join(self.directory, name.name)
                 for name in self.files]

        scene = context.scene

        # Take into account scene's unit scale, so that 1 inch in Blender gives 1 inch elsewhere! See T42000.
        global_scale = self.global_scale
        if scene.unit_settings.system != 'NONE' and self.use_scene_unit:
            global_scale /= scene.unit_settings.scale_length

        global_matrix = axis_conversion(from_forward=self.axis_forward,
                                        from_up=self.axis_up,
                                        ).to_4x4() * Matrix.Scale(global_scale, 4)

        if not paths:
            paths.append(self.filepath)

        if bpy.ops.object.mode_set.poll():
            bpy.ops.object.mode_set(mode='OBJECT')

        if bpy.ops.object.select_all.poll():
            bpy.ops.object.select_all(action='DESELECT')

        for path in paths:
            objName = bpy.path.display_name(os.path.basename(path))
            tris, tri_nors, pts = stl_utils.read_stl(path)
            tri_nors = tri_nors if self.use_facet_normal else None
            blender_utils.create_and_link_mesh(objName, tris, tri_nors, pts, global_matrix)

        return {'FINISHED'}


class ExportSTL(Operator, ExportHelper, IOSTLOrientationHelper):
    """Save STL triangle mesh data from the active object"""
    bl_idname = "export_mesh.stl"
    bl_label = "Export STL"

    filename_ext = ".stl"
    filter_glob = StringProperty(default="*.stl", options={'HIDDEN'})

    use_selection = BoolProperty(
            name="Selection Only",
            description="Export selected objects only",
            default=False,
            )
    global_scale = FloatProperty(
            name="Scale",
            min=0.01, max=1000.0,
            default=1.0,
            )

    use_scene_unit = BoolProperty(
            name="Scene Unit",
            description="Apply current scene's unit (as defined by unit scale) to exported data",
            default=False,
            )
    ascii = BoolProperty(
            name="Ascii",
            description="Save the file in ASCII file format",
            default=False,
            )
    use_mesh_modifiers = BoolProperty(
            name="Apply Modifiers",
            description="Apply the modifiers before saving",
            default=True,
            )
    batch_mode = EnumProperty(
            name="Batch Mode",
            items=(('OFF', "Off", "All data in one file"),
                   ('OBJECT', "Object", "Each object as a file"),
                   ))

    @property
    def check_extension(self):
        return self.batch_mode == 'OFF'

    def execute(self, context):
        from . import stl_utils
        from . import blender_utils
        import itertools
        from mathutils import Matrix
        keywords = self.as_keywords(ignore=("axis_forward",
                                            "axis_up",
                                            "use_selection",
                                            "global_scale",
                                            "check_existing",
                                            "filter_glob",
                                            "use_scene_unit",
                                            "use_mesh_modifiers",
                                            "batch_mode"
                                            ))

        scene = context.scene
        if self.use_selection:
            data_seq = context.selected_objects
        else:
            data_seq = scene.objects

        # Take into account scene's unit scale, so that 1 inch in Blender gives 1 inch elsewhere! See T42000.
        global_scale = self.global_scale
        if scene.unit_settings.system != 'NONE' and self.use_scene_unit:
            global_scale *= scene.unit_settings.scale_length

        global_matrix = axis_conversion(to_forward=self.axis_forward,
                                        to_up=self.axis_up,
                                        ).to_4x4() * Matrix.Scale(global_scale, 4)

        if self.batch_mode == 'OFF':
            faces = itertools.chain.from_iterable(
                    blender_utils.faces_from_mesh(ob, global_matrix, self.use_mesh_modifiers)
                    for ob in data_seq)

            stl_utils.write_stl(faces=faces, **keywords)
        elif self.batch_mode == 'OBJECT':
            prefix = os.path.splitext(self.filepath)[0]
            keywords_temp = keywords.copy()
            for ob in data_seq:
                faces = blender_utils.faces_from_mesh(ob, global_matrix, self.use_mesh_modifiers)
                keywords_temp["filepath"] = prefix + bpy.path.clean_name(ob.name) + ".stl"
                stl_utils.write_stl(faces=faces, **keywords_temp)

        return {'FINISHED'}


def menu_import(self, context):
    self.layout.operator(ImportSTL.bl_idname, text="Stl (.stl)")


def menu_export(self, context):
    default_path = os.path.splitext(bpy.data.filepath)[0] + ".stl"
    self.layout.operator(ExportSTL.bl_idname, text="Stl (.stl)")


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
