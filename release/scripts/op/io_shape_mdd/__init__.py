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
    # only reload if we alredy loaded, highly annoying
    import sys
    reload(sys.modules.get("io_shape_mdd.import_mdd", sys))
    reload(sys.modules.get("io_shape_mdd.export_mdd", sys))


import bpy
from bpy.props import *
from io_utils import ExportHelper, ImportHelper


class ImportMDD(bpy.types.Operator, ImportHelper):
    '''Import MDD vertex keyframe file to shape keys'''
    bl_idname = "import_shape.mdd"
    bl_label = "Import MDD"

    filename_ext = ".mdd"
    frame_start = IntProperty(name="Start Frame", description="Start frame for inserting animation", min=-300000, max=300000, default=0)
    frame_step = IntProperty(name="Step", min=1, max=1000, default=1)

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return (ob and ob.type == 'MESH')

    def execute(self, context):

        # initialize from scene if unset
        scene = context.scene
        if not self.frame_start:
            self.frame_start = scene.frame_current
        
        import io_shape_mdd.import_mdd
        return io_shape_mdd.import_mdd.load(self, context, **self.properties)

class ExportMDD(bpy.types.Operator, ExportHelper):
    '''Animated mesh to MDD vertex keyframe file'''
    bl_idname = "export_shape.mdd"
    bl_label = "Export MDD"
    
    filename_ext = ".mdd"

    # get first scene to get min and max properties for frames, fps

    minframe = 1
    maxframe = 300000
    minfps = 1
    maxfps = 120

    # List of operator properties, the attributes will be assigned
    # to the class instance from the operator settings before calling.
    fps = IntProperty(name="Frames Per Second", description="Number of frames/second", min=minfps, max=maxfps, default=25)
    frame_start = IntProperty(name="Start Frame", description="Start frame for baking", min=minframe, max=maxframe, default=1)
    frame_end = IntProperty(name="End Frame", description="End frame for baking", min=minframe, max=maxframe, default=250)

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH')

    def execute(self, context):
        # initialize from scene if unset
        scene = context.scene
        if not self.frame_start:
            self.frame_start = scene.frame_start
        if not self.frame_end:
            self.frame_end = scene.frame_end
        if not self.fps:
            self.fps = scene.render.fps

        import io_shape_mdd.export_mdd
        return io_shape_mdd.export_mdd.save(self, context, **self.properties)


def menu_func_import(self, context):
    self.layout.operator(ImportMDD.bl_idname, text="Lightwave Point Cache (.mdd)")


def menu_func_export(self, context):
    self.layout.operator(ExportMDD.bl_idname, text="Lightwave Point Cache (.mdd)")


def register():
    bpy.types.INFO_MT_file_import.append(menu_func_import)
    bpy.types.INFO_MT_file_export.append(menu_func_export)


def unregister():
    bpy.types.INFO_MT_file_import.remove(menu_func_import)
    bpy.types.INFO_MT_file_export.remove(menu_func_export)

if __name__ == "__main__":
    register()
