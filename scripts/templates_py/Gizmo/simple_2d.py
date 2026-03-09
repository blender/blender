# Example of a group that lets you move an object in the 3D view
# using the default translate transform operator.
#
# Usage: Select and object and drag the Gizmo to
#        move it in the 3D Viewport.
#
import bpy
from bpy.types import (
    GizmoGroup,
)


class MyTranslateWidgetGroup(GizmoGroup):
    bl_idname = "OBJECT_GGT_translate_test"
    bl_label = "Test Translate Widget"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'WINDOW'
    bl_options = {'PERSISTENT', 'SCALE'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return ob is not None

    def draw_prepare(self, context):
        region = context.region

        # Place the Gizmo in the lower center of the 3D Viewport.
        self.translate_gizmo.matrix_basis[0][3] = region.width / 2
        self.translate_gizmo.matrix_basis[1][3] = region.height / 16

    def setup(self, context):
        gz = self.gizmos.new("GIZMO_GT_button_2d")
        gz.target_set_operator("transform.translate")
        gz.draw_options = {'BACKDROP', 'OUTLINE'}

        gz.color = 0.0, 0.5, 1.0
        gz.alpha = 0.2
        gz.backdrop_fill_alpha = 0.1

        gz.color_highlight = 1.0, 0.5, 0.0
        gz.alpha_highlight = 0.8

        gz.use_tooltip = True
        gz.line_width = 1.5

        # Same as buttons defined in C++ code.
        gz.scale_basis = (80 * 0.35) / 2

        # Show a dragging mouse cursor when hovering the gizmo.
        gz.show_drag = True

        # Can also use gz.icon_value to use a custom/generated preview icon.
        gz.icon = 'EMPTY_ARROWS'

        self.translate_gizmo = gz


bpy.utils.register_class(MyTranslateWidgetGroup)
