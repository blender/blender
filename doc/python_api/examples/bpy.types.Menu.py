"""
Basic Menu Example
++++++++++++++++++

Here is an example of a simple menu. Menus differ from panels in that they must
reference from a header, panel or another menu.

Notice the 'CATEGORY_MT_name' in  :class:`Menu.bl_idname`, this is a naming
convention for menus.

.. note::

   Menu subclasses must be registered before referencing them from Blender.

.. note::

   Menus have their :class:`UILayout.operator_context` initialized as
   'EXEC_REGION_WIN' rather than 'INVOKE_REGION_WIN' (see :ref:`Execution Context <operator-execution_context>`).
   If the operator context needs to initialize inputs from the
   :class:`Operator.invoke` function, then this needs to be explicitly set.
   When a menu is added to UI elements such as a panel or header,
   the operator execution context will be inherited from them.
"""
import bpy


class BasicMenu(bpy.types.Menu):
    bl_idname = "OBJECT_MT_select_test"
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("object.select_all", text="Select/Deselect All").action = 'TOGGLE'
        layout.operator("object.select_all", text="Inverse").action = 'INVERT'
        layout.operator("object.select_random", text="Random")


bpy.utils.register_class(BasicMenu)

# Test call to display immediately.
bpy.ops.wm.call_menu(name="OBJECT_MT_select_test")
