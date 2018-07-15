# Example of an operator which uses gizmos to control its properties.
#
# Usage: Run this script, then in mesh edit-mode press Spacebar
# to activate the operator "Select Side of Plane"
# The gizmos can then be used to adjust the plane in the 3D view.
#
import bpy
import bmesh

from bpy.types import (
    Operator,
    GizmoGroup,
)

from bpy.props import (
    FloatVectorProperty,
)


def main(context, plane_co, plane_no):
    obj = context.active_object
    matrix = obj.matrix_world.copy()
    me = obj.data
    bm = bmesh.from_edit_mesh(me)

    plane_dot = plane_no.dot(plane_co)

    for v in bm.verts:
        co = matrix * v.co
        v.select = (plane_no.dot(co) > plane_dot)
    bm.select_flush_mode()

    bmesh.update_edit_mesh(me)


class SelectSideOfPlane(Operator):
    """UV Operator description"""
    bl_idname = "mesh.select_side_of_plane"
    bl_label = "Select Side of Plane"
    bl_options = {'REGISTER', 'UNDO'}

    plane_co: FloatVectorProperty(
        size=3,
        default=(0, 0, 0),
    )
    plane_no: FloatVectorProperty(
        size=3,
        default=(0, 0, 1),
    )

    @classmethod
    def poll(cls, context):
        return (context.mode == 'EDIT_MESH')

    def invoke(self, context, event):

        if not self.properties.is_property_set("plane_co"):
            self.plane_co = context.scene.cursor_location

        if not self.properties.is_property_set("plane_no"):
            if context.space_data.type == 'VIEW_3D':
                rv3d = context.space_data.region_3d
                view_inv = rv3d.view_matrix.to_3x3()
                # view y axis
                self.plane_no = view_inv[1].normalized()

        self.execute(context)

        if context.space_data.type == 'VIEW_3D':
            wm = context.window_manager
            wm.gizmo_group_type_add(SelectSideOfPlaneGizmoGroup.bl_idname)

        return {'FINISHED'}

    def execute(self, context):
        from mathutils import Vector
        main(context, Vector(self.plane_co), Vector(self.plane_no))
        return {'FINISHED'}


# Gizmos for plane_co, plane_no
class SelectSideOfPlaneGizmoGroup(GizmoGroup):
    bl_idname = "MESH_GGT_select_side_of_plane"
    bl_label = "Side of Plane Gizmo"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'WINDOW'
    bl_options = {'3D'}

    # Helper functions
    @staticmethod
    def my_target_operator(context):
        wm = context.window_manager
        op = wm.operators[-1] if wm.operators else None
        if isinstance(op, SelectSideOfPlane):
            return op
        return None

    @staticmethod
    def my_view_orientation(context):
        rv3d = context.space_data.region_3d
        view_inv = rv3d.view_matrix.to_3x3()
        return view_inv.normalized()

    @classmethod
    def poll(cls, context):
        op = cls.my_target_operator(context)
        if op is None:
            wm = context.window_manager
            wm.gizmo_group_type_remove(SelectSideOfPlaneGizmoGroup.bl_idname)
            return False
        return True

    def setup(self, context):
        from mathutils import Matrix, Vector

        # ----
        # Grab

        def grab_get_cb():
            op = SelectSideOfPlaneGizmoGroup.my_target_operator(context)
            return op.plane_co

        def grab_set_cb(value):
            op = SelectSideOfPlaneGizmoGroup.my_target_operator(context)
            op.plane_co = value
            # XXX, this may change!
            op.execute(context)

        mpr = self.gizmos.new("GIZMO_GT_grab_3d")
        mpr.target_set_handler("offset", get=grab_get_cb, set=grab_set_cb)

        mpr.use_draw_value = True

        mpr.color = 0.8, 0.8, 0.8
        mpr.alpha = 0.5

        mpr.color_highlight = 1.0, 1.0, 1.0
        mpr.alpha_highlight = 1.0

        mpr.scale_basis = 0.2

        self.widget_grab = mpr

        # ----
        # Dial

        def direction_get_cb():
            op = SelectSideOfPlaneGizmoGroup.my_target_operator(context)

            no_a = self.widget_dial.matrix_basis.col[1].xyz
            no_b = Vector(op.plane_no)

            no_a = (no_a * self.view_inv).xy.normalized()
            no_b = (no_b * self.view_inv).xy.normalized()
            return no_a.angle_signed(no_b)

        def direction_set_cb(value):
            op = SelectSideOfPlaneGizmoGroup.my_target_operator(context)
            matrix_rotate = Matrix.Rotation(-value, 3, self.rotate_axis)
            no = matrix_rotate * self.widget_dial.matrix_basis.col[1].xyz
            op.plane_no = no
            op.execute(context)

        mpr = self.gizmos.new("GIZMO_GT_dial_3d")
        mpr.target_set_handler("offset", get=direction_get_cb, set=direction_set_cb)
        mpr.draw_options = {'ANGLE_START_Y'}

        mpr.use_draw_value = True

        mpr.color = 0.8, 0.8, 0.8
        mpr.alpha = 0.5

        mpr.color_highlight = 1.0, 1.0, 1.0
        mpr.alpha_highlight = 1.0

        self.widget_dial = mpr

    def draw_prepare(self, context):
        from mathutils import Vector

        view_inv = self.my_view_orientation(context)

        self.view_inv = view_inv
        self.rotate_axis = view_inv[2].xyz
        self.rotate_up = view_inv[1].xyz

        op = self.my_target_operator(context)

        co = Vector(op.plane_co)
        no = Vector(op.plane_no).normalized()

        # Grab
        no_z = no
        no_y = no_z.orthogonal()
        no_x = no_z.cross(no_y)

        matrix = self.widget_grab.matrix_basis
        matrix.identity()
        matrix.col[0].xyz = no_x
        matrix.col[1].xyz = no_y
        matrix.col[2].xyz = no_z
        matrix.col[3].xyz = co

        # Dial
        no_z = self.rotate_axis
        no_y = (no - (no.project(no_z))).normalized()
        no_x = self.rotate_axis.cross(no_y)

        matrix = self.widget_dial.matrix_basis
        matrix.identity()
        matrix.col[0].xyz = no_x
        matrix.col[1].xyz = no_y
        matrix.col[2].xyz = no_z
        matrix.col[3].xyz = co


classes = (
    SelectSideOfPlane,
    SelectSideOfPlaneGizmoGroup,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
