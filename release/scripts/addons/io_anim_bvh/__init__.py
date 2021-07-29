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
    "name": "BioVision Motion Capture (BVH) format",
    "author": "Campbell Barton",
    "blender": (2, 74, 0),
    "location": "File > Import-Export",
    "description": "Import-Export BVH from armature objects",
    "warning": "",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Import-Export/BVH_Importer_Exporter",
    "support": 'OFFICIAL',
    "category": "Import-Export"}

if "bpy" in locals():
    import importlib
    if "import_bvh" in locals():
        importlib.reload(import_bvh)
    if "export_bvh" in locals():
        importlib.reload(export_bvh)

import bpy
from bpy.props import (
        StringProperty,
        FloatProperty,
        IntProperty,
        BoolProperty,
        EnumProperty,
        )
from bpy_extras.io_utils import (
        ImportHelper,
        ExportHelper,
        orientation_helper_factory,
        axis_conversion,
        )


ImportBVHOrientationHelper = orientation_helper_factory("ImportBVHOrientationHelper", axis_forward='-Z', axis_up='Y')


class ImportBVH(bpy.types.Operator, ImportHelper, ImportBVHOrientationHelper):
    """Load a BVH motion capture file"""
    bl_idname = "import_anim.bvh"
    bl_label = "Import BVH"
    bl_options = {'REGISTER', 'UNDO'}

    filename_ext = ".bvh"
    filter_glob = StringProperty(default="*.bvh", options={'HIDDEN'})

    target = EnumProperty(items=(
            ('ARMATURE', "Armature", ""),
            ('OBJECT', "Object", ""),
            ),
                name="Target",
                description="Import target type",
                default='ARMATURE')

    global_scale = FloatProperty(
            name="Scale",
            description="Scale the BVH by this value",
            min=0.0001, max=1000000.0,
            soft_min=0.001, soft_max=100.0,
            default=1.0,
            )
    frame_start = IntProperty(
            name="Start Frame",
            description="Starting frame for the animation",
            default=1,
            )
    use_fps_scale = BoolProperty(
            name="Scale FPS",
            description=("Scale the framerate from the BVH to the current scenes, "
                         "otherwise each BVH frame maps directly to a Blender frame"),
            default=False,
            )
    update_scene_fps = BoolProperty(
            name="Update Scene FPS",
            description="Set the scene framerate to that of the BVH file (note that this "
                        "nullifies the 'Scale FPS' option, as the scale will be 1:1)",
            default=False
            )
    update_scene_duration = BoolProperty(
            name="Update Scene Duration",
            description="Extend the scene's duration to the BVH duration (never shortens the scene)",
            default=False,
            )
    use_cyclic = BoolProperty(
            name="Loop",
            description="Loop the animation playback",
            default=False,
            )
    rotate_mode = EnumProperty(
            name="Rotation",
            description="Rotation conversion",
            items=(('QUATERNION', "Quaternion",
                    "Convert rotations to quaternions"),
                   ('NATIVE', "Euler (Native)",
                              "Use the rotation order defined in the BVH file"),
                   ('XYZ', "Euler (XYZ)", "Convert rotations to euler XYZ"),
                   ('XZY', "Euler (XZY)", "Convert rotations to euler XZY"),
                   ('YXZ', "Euler (YXZ)", "Convert rotations to euler YXZ"),
                   ('YZX', "Euler (YZX)", "Convert rotations to euler YZX"),
                   ('ZXY', "Euler (ZXY)", "Convert rotations to euler ZXY"),
                   ('ZYX', "Euler (ZYX)", "Convert rotations to euler ZYX"),
                   ),
            default='NATIVE',
            )

    def execute(self, context):
        keywords = self.as_keywords(ignore=("axis_forward",
                                            "axis_up",
                                            "filter_glob",
                                            ))

        global_matrix = axis_conversion(from_forward=self.axis_forward,
                                        from_up=self.axis_up,
                                        ).to_4x4()

        keywords["global_matrix"] = global_matrix

        from . import import_bvh
        return import_bvh.load(context, report=self.report, **keywords)


class ExportBVH(bpy.types.Operator, ExportHelper):
    """Save a BVH motion capture file from an armature"""
    bl_idname = "export_anim.bvh"
    bl_label = "Export BVH"

    filename_ext = ".bvh"
    filter_glob = StringProperty(
            default="*.bvh",
            options={'HIDDEN'},
            )

    global_scale = FloatProperty(
            name="Scale",
            description="Scale the BVH by this value",
            min=0.0001, max=1000000.0,
            soft_min=0.001, soft_max=100.0,
            default=1.0,
            )
    frame_start = IntProperty(
            name="Start Frame",
            description="Starting frame to export",
            default=0,
            )
    frame_end = IntProperty(
            name="End Frame",
            description="End frame to export",
            default=0,
            )
    rotate_mode = EnumProperty(
            name="Rotation",
            description="Rotation conversion",
            items=(('NATIVE', "Euler (Native)",
                              "Use the rotation order defined in the BVH file"),
                   ('XYZ', "Euler (XYZ)", "Convert rotations to euler XYZ"),
                   ('XZY', "Euler (XZY)", "Convert rotations to euler XZY"),
                   ('YXZ', "Euler (YXZ)", "Convert rotations to euler YXZ"),
                   ('YZX', "Euler (YZX)", "Convert rotations to euler YZX"),
                   ('ZXY', "Euler (ZXY)", "Convert rotations to euler ZXY"),
                   ('ZYX', "Euler (ZYX)", "Convert rotations to euler ZYX"),
                   ),
            default='NATIVE',
            )
    root_transform_only = BoolProperty(
            name="Root Translation Only",
            description="Only write out translation channels for the root bone",
            default=False,
            )

    @classmethod
    def poll(cls, context):
        obj = context.object
        return obj and obj.type == 'ARMATURE'

    def invoke(self, context, event):
        self.frame_start = context.scene.frame_start
        self.frame_end = context.scene.frame_end

        return super().invoke(context, event)

    def execute(self, context):
        if self.frame_start == 0 and self.frame_end == 0:
            self.frame_start = context.scene.frame_start
            self.frame_end = context.scene.frame_end

        keywords = self.as_keywords(ignore=("check_existing", "filter_glob"))

        from . import export_bvh
        return export_bvh.save(self, context, **keywords)


def menu_func_import(self, context):
    self.layout.operator(ImportBVH.bl_idname, text="Motion Capture (.bvh)")


def menu_func_export(self, context):
    self.layout.operator(ExportBVH.bl_idname, text="Motion Capture (.bvh)")


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
