# Example of a group that edits a single property
# using the predefined manipulator arrow.
#
# Usage: Select a lamp in the 3D view and drag the arrow at it's rear
# to change it's energy value.
#
import bpy
from bpy.types import (
    ManipulatorGroup,
)

class MyLampWidgetGroup(ManipulatorGroup):
    bl_idname = "OBJECT_WGT_lamp_test"
    bl_label = "Test Lamp Widget"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'WINDOW'
    bl_options = {'3D', 'PERSISTENT'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.type == 'LAMP')

    def setup(self, context):
        # Arrow manipulator has one 'offset' property we can assign to the lamp energy.
        ob = context.object
        mpr = self.manipulators.new("MANIPULATOR_WT_arrow_3d")
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

bpy.utils.register_class(MyLampWidgetGroup)
