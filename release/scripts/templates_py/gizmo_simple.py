# Example of a group that edits a single property
# using the predefined gizmo arrow.
#
# Usage: Select a light in the 3D view and drag the arrow at it's rear
# to change it's energy value.
#
import bpy
from bpy.types import (
    GizmoGroup,
)


class MyLightWidgetGroup(GizmoGroup):
    bl_idname = "OBJECT_GGT_light_test"
    bl_label = "Test Light Widget"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'WINDOW'
    bl_options = {'3D', 'PERSISTENT'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.type == 'LIGHT')

    def setup(self, context):
        # Arrow gizmo has one 'offset' property we can assign to the light energy.
        ob = context.object
        mpr = self.gizmos.new("GIZMO_GT_arrow_3d")
        mpr.target_set_prop("offset", ob.data, "energy")
        mpr.matrix_basis = ob.matrix_world.normalized()
        mpr.draw_style = 'BOX'

        mpr.color = 1.0, 0.5, 0.0
        mpr.alpha = 0.5

        mpr.color_highlight = 1.0, 0.5, 1.0
        mpr.alpha_highlight = 0.5

        self.energy_widget = mpr

    def refresh(self, context):
        ob = context.object
        mpr = self.energy_widget
        mpr.matrix_basis = ob.matrix_world.normalized()


bpy.utils.register_class(MyLightWidgetGroup)
