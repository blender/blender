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
    reload(sys.modules.get("io_scene_fbx.export_fbx", sys))


import bpy
from bpy.props import *
from io_utils import ExportHelper


class ExportFBX(bpy.types.Operator, ExportHelper):
    '''Selection to an ASCII Autodesk FBX'''
    bl_idname = "export_scene.fbx"
    bl_label = "Export FBX"

    filename_ext = ".fbx"

    # List of operator properties, the attributes will be assigned
    # to the class instance from the operator settings before calling.

    EXP_OBS_SELECTED = BoolProperty(name="Selected Objects", description="Export selected objects on visible layers", default=True)
# 	EXP_OBS_SCENE = BoolProperty(name="Scene Objects", description="Export all objects in this scene", default=True)
    TX_SCALE = FloatProperty(name="Scale", description="Scale all data, (Note! some imports dont support scaled armatures)", min=0.01, max=1000.0, soft_min=0.01, soft_max=1000.0, default=1.0)
    TX_XROT90 = BoolProperty(name="Rot X90", description="Rotate all objects 90 degrees about the X axis", default=True)
    TX_YROT90 = BoolProperty(name="Rot Y90", description="Rotate all objects 90 degrees about the Y axis", default=False)
    TX_ZROT90 = BoolProperty(name="Rot Z90", description="Rotate all objects 90 degrees about the Z axis", default=False)
    EXP_EMPTY = BoolProperty(name="Empties", description="Export empty objects", default=True)
    EXP_CAMERA = BoolProperty(name="Cameras", description="Export camera objects", default=True)
    EXP_LAMP = BoolProperty(name="Lamps", description="Export lamp objects", default=True)
    EXP_ARMATURE = BoolProperty(name="Armatures", description="Export armature objects", default=True)
    EXP_MESH = BoolProperty(name="Meshes", description="Export mesh objects", default=True)
    EXP_MESH_APPLY_MOD = BoolProperty(name="Modifiers", description="Apply modifiers to mesh objects", default=True)
#    EXP_MESH_HQ_NORMALS = BoolProperty(name="HQ Normals", description="Generate high quality normals", default=True)
    EXP_IMAGE_COPY = BoolProperty(name="Copy Image Files", description="Copy image files to the destination path", default=False)
    # armature animation
    ANIM_ENABLE = BoolProperty(name="Enable Animation", description="Export keyframe animation", default=True)
    ANIM_OPTIMIZE = BoolProperty(name="Optimize Keyframes", description="Remove double keyframes", default=True)
    ANIM_OPTIMIZE_PRECISSION = FloatProperty(name="Precision", description="Tolerence for comparing double keyframes (higher for greater accuracy)", min=1, max=16, soft_min=1, soft_max=16, default=6.0)
# 	ANIM_ACTION_ALL = BoolProperty(name="Current Action", description="Use actions currently applied to the armatures (use scene start/end frame)", default=True)
    ANIM_ACTION_ALL = BoolProperty(name="All Actions", description="Use all actions for armatures, if false, use current action", default=False)
    # batch
    BATCH_ENABLE = BoolProperty(name="Enable Batch", description="Automate exporting multiple scenes or groups to files", default=False)
    BATCH_GROUP = BoolProperty(name="Group > File", description="Export each group as an FBX file, if false, export each scene as an FBX file", default=False)
    BATCH_OWN_DIR = BoolProperty(name="Own Dir", description="Create a dir for each exported file", default=True)
    BATCH_FILE_PREFIX = StringProperty(name="Prefix", description="Prefix each file with this name", maxlen=1024, default="")

    def execute(self, context):
        import math
        from mathutils import Matrix
        if not self.filepath:
            raise Exception("filepath not set")

        mtx4_x90n = Matrix.Rotation(-math.pi / 2.0, 4, 'X')
        mtx4_y90n = Matrix.Rotation(-math.pi / 2.0, 4, 'Y')
        mtx4_z90n = Matrix.Rotation(-math.pi / 2.0, 4, 'Z')

        GLOBAL_MATRIX = Matrix()
        GLOBAL_MATRIX[0][0] = GLOBAL_MATRIX[1][1] = GLOBAL_MATRIX[2][2] = self.TX_SCALE
        if self.TX_XROT90:
            GLOBAL_MATRIX = mtx4_x90n * GLOBAL_MATRIX
        if self.TX_YROT90:
            GLOBAL_MATRIX = mtx4_y90n * GLOBAL_MATRIX
        if self.TX_ZROT90:
            GLOBAL_MATRIX = mtx4_z90n * GLOBAL_MATRIX

        import io_scene_fbx.export_fbx
        return io_scene_fbx.export_fbx.save(self, context, **self.as_keywords(ignore=("TX_XROT90", "TX_YROT90", "TX_ZROT90", "TX_SCALE", "check_existing")))


def menu_func(self, context):
    self.layout.operator(ExportFBX.bl_idname, text="Autodesk FBX (.fbx)")


def register():
    bpy.types.INFO_MT_file_export.append(menu_func)


def unregister():
    bpy.types.INFO_MT_file_export.remove(menu_func)

if __name__ == "__main__":
    register()
