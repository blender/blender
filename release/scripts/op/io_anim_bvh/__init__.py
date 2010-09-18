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
    reload(sys.modules.get("io_anim_bvh.import_bvh", sys))


import bpy
from bpy.props import *
from io_utils import ImportHelper


class BvhImporter(bpy.types.Operator, ImportHelper):
    '''Load a OBJ Motion Capture File'''
    bl_idname = "import_anim.bvh"
    bl_label = "Import BVH"
    
    filename_ext = ".bvh"

    scale = FloatProperty(name="Scale", description="Scale the BVH by this value", min=0.0001, max=1000000.0, soft_min=0.001, soft_max=100.0, default=0.1)
    frame_start = IntProperty(name="Start Frame", description="Starting frame for the animation", default=1)
    use_cyclic = BoolProperty(name="Loop", description="Loop the animation playback", default=False)
    rotate_mode = EnumProperty(items=(
            ('QUATERNION', "Quaternion", "Convert rotations to quaternions"),
            ('NATIVE', "Euler (Native)", "Use the rotation order defined in the BVH file"),
            ('XYZ', "Euler (XYZ)", "Convert rotations to euler XYZ"),
            ('XZY', "Euler (XZY)", "Convert rotations to euler XZY"),
            ('YXZ', "Euler (YXZ)", "Convert rotations to euler YXZ"),
            ('YZX', "Euler (YZX)", "Convert rotations to euler YZX"),
            ('ZXY', "Euler (ZXY)", "Convert rotations to euler ZXY"),
            ('ZYX', "Euler (ZYX)", "Convert rotations to euler ZYX"),
            ),
                name="Rotation",
                description="Rotation conversion.",
                default='NATIVE')

    def execute(self, context):
        import io_anim_bvh.import_bvh
        return io_anim_bvh.import_bvh.load(self, context,
                                           filepath=self.filepath,
                                           rotate_mode=self.rotate_mode,
                                           scale=self.scale,
                                           use_cyclic=self.use_cyclic,
                                           frame_start=self.frame_start,
                                           )


def menu_func(self, context):
    self.layout.operator(BvhImporter.bl_idname, text="Motion Capture (.bvh)")


def register():
    bpy.types.INFO_MT_file_import.append(menu_func)


def unregister():
    bpy.types.INFO_MT_file_import.remove(menu_func)

if __name__ == "__main__":
    register()
