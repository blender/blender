# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

bl_info = {
    "name": "BioVision Motion Capture (BVH) format",
    "author": "Campbell Barton",
    "version": (1, 0, 1),
    "blender": (2, 81, 6),
    "location": "File > Import-Export",
    "description": "Import-Export BVH from armature objects",
    "warning": "",
    "doc_url": "{BLENDER_MANUAL_URL}/addons/import_export/anim_bvh.html",
    "support": 'OFFICIAL',
    "category": "Import-Export",
}

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
    orientation_helper,
    axis_conversion,
)


@orientation_helper(axis_forward='-Z', axis_up='Y')
class ImportBVH(bpy.types.Operator, ImportHelper):
    """Load a BVH motion capture file"""
    bl_idname = "import_anim.bvh"
    bl_label = "Import BVH"
    bl_options = {'REGISTER', 'UNDO'}

    filename_ext = ".bvh"
    filter_glob: StringProperty(default="*.bvh", options={'HIDDEN'})

    target: EnumProperty(
        items=(
            ('ARMATURE', "Armature", ""),
            ('OBJECT', "Object", ""),
        ),
        name="Target",
        description="Import target type",
        default='ARMATURE',
    )
    global_scale: FloatProperty(
        name="Scale",
        description="Scale the BVH by this value",
        min=0.0001, max=1000000.0,
        soft_min=0.001, soft_max=100.0,
        default=1.0,
    )
    frame_start: IntProperty(
        name="Start Frame",
        description="Starting frame for the animation",
        default=1,
    )
    use_fps_scale: BoolProperty(
        name="Scale FPS",
        description=(
            "Scale the framerate from the BVH to the current scenes, "
            "otherwise each BVH frame maps directly to a Blender frame"
        ),
        default=False,
    )
    update_scene_fps: BoolProperty(
        name="Update Scene FPS",
        description=(
            "Set the scene framerate to that of the BVH file (note that this "
            "nullifies the 'Scale FPS' option, as the scale will be 1:1)"
        ),
        default=False,
    )
    update_scene_duration: BoolProperty(
        name="Update Scene Duration",
        description="Extend the scene's duration to the BVH duration (never shortens the scene)",
        default=False,
    )
    use_cyclic: BoolProperty(
        name="Loop",
        description="Loop the animation playback",
        default=False,
    )
    rotate_mode: EnumProperty(
        name="Rotation",
        description="Rotation conversion",
        items=(
            ('QUATERNION', "Quaternion",
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
        keywords = self.as_keywords(
            ignore=(
                "axis_forward",
                "axis_up",
                "filter_glob",
            )
        )
        global_matrix = axis_conversion(
            from_forward=self.axis_forward,
            from_up=self.axis_up,
        ).to_4x4()

        keywords["global_matrix"] = global_matrix

        from . import import_bvh
        return import_bvh.load(context, report=self.report, **keywords)

    def draw(self, context):
        pass


class BVH_PT_import_main(bpy.types.Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOL_PROPS'
    bl_label = ""
    bl_parent_id = "FILE_PT_operator"
    bl_options = {'HIDE_HEADER'}

    @classmethod
    def poll(cls, context):
        sfile = context.space_data
        operator = sfile.active_operator

        return operator.bl_idname == "IMPORT_ANIM_OT_bvh"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        sfile = context.space_data
        operator = sfile.active_operator

        layout.prop(operator, "target")


class BVH_PT_import_transform(bpy.types.Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOL_PROPS'
    bl_label = "Transform"
    bl_parent_id = "FILE_PT_operator"

    @classmethod
    def poll(cls, context):
        sfile = context.space_data
        operator = sfile.active_operator

        return operator.bl_idname == "IMPORT_ANIM_OT_bvh"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        sfile = context.space_data
        operator = sfile.active_operator

        layout.prop(operator, "global_scale")
        layout.prop(operator, "rotate_mode")
        layout.prop(operator, "axis_forward")
        layout.prop(operator, "axis_up")


class BVH_PT_import_animation(bpy.types.Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOL_PROPS'
    bl_label = "Animation"
    bl_parent_id = "FILE_PT_operator"

    @classmethod
    def poll(cls, context):
        sfile = context.space_data
        operator = sfile.active_operator

        return operator.bl_idname == "IMPORT_ANIM_OT_bvh"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        sfile = context.space_data
        operator = sfile.active_operator

        layout.prop(operator, "frame_start")
        layout.prop(operator, "use_fps_scale")
        layout.prop(operator, "use_cyclic")

        layout.prop(operator, "update_scene_fps")
        layout.prop(operator, "update_scene_duration")


class ExportBVH(bpy.types.Operator, ExportHelper):
    """Save a BVH motion capture file from an armature"""
    bl_idname = "export_anim.bvh"
    bl_label = "Export BVH"

    filename_ext = ".bvh"
    filter_glob: StringProperty(
        default="*.bvh",
        options={'HIDDEN'},
    )

    global_scale: FloatProperty(
        name="Scale",
        description="Scale the BVH by this value",
        min=0.0001, max=1000000.0,
        soft_min=0.001, soft_max=100.0,
        default=1.0,
    )
    frame_start: IntProperty(
        name="Start Frame",
        description="Starting frame to export",
        default=0,
    )
    frame_end: IntProperty(
        name="End Frame",
        description="End frame to export",
        default=0,
    )
    rotate_mode: EnumProperty(
        name="Rotation",
        description="Rotation conversion",
        items=(
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
    root_transform_only: BoolProperty(
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

        keywords = self.as_keywords(
            ignore=(
                "axis_forward",
                "axis_up",
                "check_existing",
                "filter_glob",
            )
        )

        from . import export_bvh
        return export_bvh.save(context, **keywords)

    def draw(self, context):
        pass


class BVH_PT_export_transform(bpy.types.Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOL_PROPS'
    bl_label = "Transform"
    bl_parent_id = "FILE_PT_operator"

    @classmethod
    def poll(cls, context):
        sfile = context.space_data
        operator = sfile.active_operator

        return operator.bl_idname == "EXPORT_ANIM_OT_bvh"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        sfile = context.space_data
        operator = sfile.active_operator

        layout.prop(operator, "global_scale")
        layout.prop(operator, "rotate_mode")
        layout.prop(operator, "root_transform_only")


class BVH_PT_export_animation(bpy.types.Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOL_PROPS'
    bl_label = "Animation"
    bl_parent_id = "FILE_PT_operator"

    @classmethod
    def poll(cls, context):
        sfile = context.space_data
        operator = sfile.active_operator

        return operator.bl_idname == "EXPORT_ANIM_OT_bvh"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        sfile = context.space_data
        operator = sfile.active_operator

        col = layout.column(align=True)
        col.prop(operator, "frame_start", text="Frame Start")
        col.prop(operator, "frame_end", text="End")


def menu_func_import(self, context):
    self.layout.operator(ImportBVH.bl_idname, text="Motion Capture (.bvh)")


def menu_func_export(self, context):
    self.layout.operator(ExportBVH.bl_idname, text="Motion Capture (.bvh)")


classes = (
    ImportBVH,
    BVH_PT_import_main,
    BVH_PT_import_transform,
    BVH_PT_import_animation,
    ExportBVH,
    BVH_PT_export_transform,
    BVH_PT_export_animation,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)


def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)

    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)


if __name__ == "__main__":
    register()
