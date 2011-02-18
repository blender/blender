"""
Basic Menu Example
++++++++++++++++++
This script is a simple menu, menus differ from panels in that they must
reference from a header, panel or another menu.

Notice the 'CATEGORY_MT_name' :class:`Menu.bl_idname`, this is a naming
convention for menus.

.. note::

   Menu subclasses must be registered before referencing them from blender.
"""
import bpy


class BasicMenu(bpy.types.Menu):
    bl_idname = "OBJECT_MT_select_test"
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout
        
        layout.operator("object.select_all", text="Select/Deselect All")
        layout.operator("object.select_inverse", text="Inverse")
        layout.operator("object.select_random", text="Random")


bpy.utils.register_class(BasicMenu)

# test call to display immediately.
bpy.ops.wm.call_menu(name="OBJECT_MT_select_test")
