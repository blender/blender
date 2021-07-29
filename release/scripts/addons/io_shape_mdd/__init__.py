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

bl_info = {
    "name": "NewTek MDD format",
    "author": "Bill L.Nieuwendorp",
    "version": (1, 0, 1),
    "blender": (2, 57, 0),
    "location": "File > Import-Export",
    "description": "Import-Export MDD as mesh shape keys",
    "warning": "",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Import-Export/NewTek_OBJ",
    "support": 'OFFICIAL',
    "category": "Import-Export"}

if "bpy" in locals():
    import importlib
    if "import_mdd" in locals():
        importlib.reload(import_mdd)
    if "export_mdd" in locals():
        importlib.reload(export_mdd)


import bpy
from bpy.props import (
        BoolProperty,
        FloatProperty,
        IntProperty,
        StringProperty,
        )
from bpy_extras.io_utils import ExportHelper, ImportHelper


class ImportMDD(bpy.types.Operator, ImportHelper):
    """Import MDD vertex keyframe file to shape keys"""
    bl_idname = "import_shape.mdd"
    bl_label = "Import MDD"
    bl_options = {'UNDO'}

    filename_ext = ".mdd"

    filter_glob = StringProperty(
            default="*.mdd",
            options={'HIDDEN'},
            )
    frame_start = IntProperty(
            name="Start Frame",
            description="Start frame for inserting animation",
            min=-300000, max=300000,
            default=0,
            )
    frame_step = IntProperty(
            name="Step",
            min=1, max=1000,
            default=1,
            )

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH')

    def invoke(self, context, event):
        scene = context.scene
        self.frame_start = scene.frame_start

        return super().invoke(context, event)

    def execute(self, context):
        keywords = self.as_keywords(ignore=("filter_glob",))

        from . import import_mdd
        return import_mdd.load(context, **keywords)


class ExportMDD(bpy.types.Operator, ExportHelper):
    """Animated mesh to MDD vertex keyframe file"""
    bl_idname = "export_shape.mdd"
    bl_label = "Export MDD"

    filename_ext = ".mdd"
    filter_glob = StringProperty(default="*.mdd", options={'HIDDEN'})

    # get first scene to get min and max properties for frames, fps

    minframe = 0
    maxframe = 300000
    minfps = 1.0
    maxfps = 120.0

    # List of operator properties, the attributes will be assigned
    # to the class instance from the operator settings before calling.
    fps = FloatProperty(
            name="Frames Per Second",
            description="Number of frames/second",
            min=minfps, max=maxfps,
            default=25.0,
            )
    frame_start = IntProperty(
            name="Start Frame",
            description="Start frame for baking",
            min=minframe, max=maxframe,
            default=1,
            )
    frame_end = IntProperty(
            name="End Frame",
            description="End frame for baking",
            min=minframe, max=maxframe,
            default=250,
            )
    use_rest_frame = BoolProperty(
            name="Rest Frame",
            description="Write the rest state at the first frame",
            default=False,
            )

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH')

    def invoke(self, context, event):
        scene = context.scene
        self.frame_start = scene.frame_start
        self.frame_end = scene.frame_end
        self.fps = scene.render.fps / scene.render.fps_base

        return super().invoke(context, event)

    def execute(self, context):
        keywords = self.as_keywords(ignore=("check_existing", "filter_glob"))

        from . import export_mdd
        return export_mdd.save(context, **keywords)


def menu_func_import(self, context):
    self.layout.operator(ImportMDD.bl_idname,
                         text="Lightwave Point Cache (.mdd)",
                         )


def menu_func_export(self, context):
    self.layout.operator(ExportMDD.bl_idname,
                         text="Lightwave Point Cache (.mdd)",
                         )


def register():
    bpy.utils.register_module(__name__)

    bpy.types.INFO_MT_file_import.append(menu_func_import)
    bpy.types.INFO_MT_file_export.append(menu_func_export)


def unregister():
    bpy.utils.unregister_module(__name__)

    bpy.types.INFO_MT_file_import.remove(menu_func_import)
    bpy.types.INFO_MT_file_export.remove(menu_func_export)

if __name__ == "__main__":
    register()
