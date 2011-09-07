"""
Mix-in Classes
++++++++++++++
A mix-in parent class can be used to share common properties and
:class:`Menu.poll` function.
"""
import bpy


class View3DPanel():
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'

    @classmethod
    def poll(cls, context):
        return (context.object is not None)


class PanelOne(View3DPanel, bpy.types.Panel):
    bl_idname = "VIEW3D_PT_test_1"
    bl_label = "Panel One"

    def draw(self, context):
        self.layout.label("Small Class")


class PanelTwo(View3DPanel, bpy.types.Panel):
    bl_idname = "VIEW3D_PT_test_2"
    bl_label = "Panel Two"

    def draw(self, context):
        self.layout.label("Also Small Class")


bpy.utils.register_class(PanelOne)
bpy.utils.register_class(PanelTwo)
