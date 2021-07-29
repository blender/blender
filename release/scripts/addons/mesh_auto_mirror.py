# ########################################################### #
#   An simple add-on to auto cut in two and mirror an object  #
#   Actualy partialy uncommented (see further version)        #
#   Author: Lapineige                                         #
#   License: GPL v3                                           #
# ########################################################### #

bl_info = {
    "name": "Auto Mirror",
    "description": "Super fast cutting and mirroring for Mesh objects",
    "author": "Lapineige",
    "version": (2, 4, 2),
    "blender": (2, 7, 1),
    "location": "View 3D > Toolbar > Tools tab > AutoMirror (panel)",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/"
                "Py/Scripts/Modeling/AutoMirror",
    "category": "Mesh"}


import bpy
from bpy.props import (
        BoolProperty,
        EnumProperty,
        FloatProperty,
        PointerProperty,
        )
from bpy.types import (
        Operator,
        Panel,
        PropertyGroup,
        )
from mathutils import Vector


# Operators
class AlignVertices(Operator):
    bl_idname = "object.align_vertices"
    bl_label = "Align Vertices on an Axis"
    bl_description = ("Align Vertices on an Axis\n"
                      "Needs an Active Mesh Object")

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj and obj.type == "MESH"

    def execute(self, context):
        auto_m = context.scene.auto_mirror
        bpy.ops.object.mode_set(mode='OBJECT')

        x1, y1, z1 = bpy.context.scene.cursor_location
        bpy.ops.view3d.snap_cursor_to_selected()

        x2, y2, z2 = bpy.context.scene.cursor_location

        bpy.context.scene.cursor_location[0], \
        bpy.context.scene.cursor_location[1], \
        bpy.context.scene.cursor_location[2] = 0, 0, 0

        # Vertices coordinate to 0 (local coordinate, so on the origin)
        for vert in bpy.context.object.data.vertices:
            if vert.select:
                if auto_m.axis == 'x':
                    axis = 0
                elif auto_m.axis == 'y':
                    axis = 1
                elif auto_m.axis == 'z':
                    axis = 2
                vert.co[axis] = 0

        bpy.context.scene.cursor_location = x2, y2, z2
        bpy.ops.object.origin_set(type='ORIGIN_CURSOR')

        bpy.context.scene.cursor_location = x1, y1, z1
        bpy.ops.object.mode_set(mode='EDIT')

        return {'FINISHED'}


class AutoMirror(Operator):
    bl_idname = "object.automirror"
    bl_label = "AutoMirror"
    bl_description = ("Automatically cut an object along an axis\n"
                      "Needs an Active Mesh Object")
    bl_options = {'REGISTER'}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj and obj.type == "MESH"

    def get_local_axis_vector(self, context, X, Y, Z, orientation):
        loc = context.object.location
        bpy.ops.object.mode_set(mode="OBJECT")  # Needed to avoid to translate vertices

        v1 = Vector((loc[0], loc[1], loc[2]))
        bpy.ops.transform.translate(
                value=(X * orientation, Y * orientation, Z * orientation),
                constraint_axis=((X == 1), (Y == 1), (Z == 1)),
                constraint_orientation='LOCAL'
                )
        v2 = Vector((loc[0], loc[1], loc[2]))
        bpy.ops.transform.translate(
                value=(-X * orientation, -Y * orientation, -Z * orientation),
                constraint_axis=((X == 1), (Y == 1), (Z == 1)),
                constraint_orientation='LOCAL'
                )

        bpy.ops.object.mode_set(mode="EDIT")
        return v2 - v1

    def execute(self, context):
        auto_m = context.scene.auto_mirror

        X, Y, Z = 0, 0, 0
        if auto_m.axis == 'x':
            X = 1
        elif auto_m.axis == 'y':
            Y = 1
        elif auto_m.axis == 'z':
            Z = 1

        current_mode = bpy.context.object.mode    # Save the current mode

        if bpy.context.object.mode != "EDIT":
            bpy.ops.object.mode_set(mode="EDIT")  # Go to edit mode

        bpy.ops.mesh.select_all(action='SELECT')  # Select all the vertices
        if auto_m.orientation == 'positive':
            orientation = 1
        else:
            orientation = -1
        cut_normal = self.get_local_axis_vector(context, X, Y, Z, orientation)

        # Cut the mesh
        bpy.ops.mesh.bisect(
                plane_co=(
                    bpy.context.object.location[0],
                    bpy.context.object.location[1],
                    bpy.context.object.location[2]
                    ),
                plane_no=cut_normal,
                use_fill=False,
                clear_inner=auto_m.cut,
                clear_outer=0,
                threshold=auto_m.threshold
                )

        # Use to align the vertices on the origin, needed by the "threshold"
        bpy.ops.object.align_vertices()

        if not auto_m.toggle_edit:
            bpy.ops.object.mode_set(mode=current_mode)  # Reload previous mode

        if auto_m.cut:
            bpy.ops.object.modifier_add(type='MIRROR')  # Add a mirror modifier
            bpy.context.object.modifiers[-1].use_x = X  # Choose the axis to use, based on the cut's axis
            bpy.context.object.modifiers[-1].use_y = Y
            bpy.context.object.modifiers[-1].use_z = Z
            bpy.context.object.modifiers[-1].use_clip = auto_m.use_clip
            bpy.context.object.modifiers[-1].show_on_cage = auto_m.show_on_cage

            if auto_m.apply_mirror:
                bpy.ops.object.mode_set(mode='OBJECT')
                bpy.ops.object.modifier_apply(
                        apply_as='DATA',
                        modifier=bpy.context.object.modifiers[-1].name
                        )
                if auto_m.toggle_edit:
                    bpy.ops.object.mode_set(mode='EDIT')
                else:
                    bpy.ops.object.mode_set(mode=current_mode)

        return {'FINISHED'}


# Panel
class BisectMirror(Panel):
    bl_label = "Auto Mirror"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'
    bl_category = "Tools"
    bl_options = {"DEFAULT_CLOSED"}

    def draw(self, context):
        layout = self.layout
        auto_m = context.scene.auto_mirror
        obj = context.active_object

        if obj and obj.type == 'MESH':
            layout.operator("object.automirror", icon="MOD_MIRROR")
            layout.label("Options:")
            layout.prop(auto_m, "axis", text="Mirror Axis", expand=True)
            layout.prop(auto_m, "orientation", text="Orientation")
            layout.prop(auto_m, "threshold", text="Threshold")
            layout.prop(auto_m, "toggle_edit", text="Toggle Edit")
            layout.prop(auto_m, "cut", text="Cut and Mirror", toggle=True, icon="MOD_REMESH")

            if auto_m.cut:
                col = layout.column(align=True)
                row = col.row(align=True)
                row.prop(auto_m, "use_clip", text="Use Clip", toggle=True)
                row.prop(auto_m, "show_on_cage", text="Editable", toggle=True)
                col.prop(auto_m, "apply_mirror", text="Apply Mirror", toggle=True)
        else:
            layout.label(icon="INFO", text="No Mesh selected")


class AutoMirrorProperties(PropertyGroup):
    axis = EnumProperty(
            name="Axis",
            items=[
                ("x", "X", "", 1),
                ("y", "Y", "", 2),
                ("z", "Z", "", 3)
                ],
            description="Axis used by the mirror modifier"
            )
    orientation = EnumProperty(
            name="Orientation",
            items=[
                ("positive", "Positive", "", 1),
                ("negative", "Negative", "", 2)
                ],
            description="Choose the side along the axis of the editable part (+/- coordinates)"
            )
    threshold = FloatProperty(
            default=0.001,
            min=0.001,
            description="Vertices closer than this distance are merged on the loopcut"
            )
    toggle_edit = BoolProperty(
            name="Toggle Edit Mode",
            default=True,
            description="If not in Edit mode, change mode to it"
            )
    cut = BoolProperty(
            name="Cut",
            default=True,
            description="If enabled, cut the mesh in two parts and mirror it\n"
                        "If not, just make a loopcut"
            )
    clipping = BoolProperty(
            default=True
            )
    use_clip = BoolProperty(
            default=True,
            description="Use clipping for the mirror modifier"
            )
    show_on_cage = BoolProperty(
            default=True,
            description="Enable editing the cage (it's the classical modifier's option)"
            )
    apply_mirror = BoolProperty(
            description="Apply the mirror modifier (useful to symmetrise the mesh)"
            )


def register():
    bpy.utils.register_class(BisectMirror)
    bpy.utils.register_class(AutoMirror)
    bpy.utils.register_class(AlignVertices)
    bpy.utils.register_class(AutoMirrorProperties)
    bpy.types.Scene.auto_mirror = PointerProperty(
                                        type=AutoMirrorProperties
                                        )


def unregister():
    bpy.utils.unregister_class(BisectMirror)
    bpy.utils.unregister_class(AutoMirror)
    bpy.utils.unregister_class(AlignVertices)
    bpy.utils.unregister_class(AutoMirrorProperties)
    del bpy.types.Scene.auto_mirror


if __name__ == "__main__":
    register()
