"""
Basic Operator Example
++++++++++++++++++++++

This script shows simple operator which prints a message.

Since the operator only has an :class:`Operator.execute` function it takes no
user input.

The function should return ``{'FINISHED'}`` or ``{'CANCELLED'}``, the latter
meaning that operator execution was aborted without making any changes, and
saving an undo entry isn't neccesary. If an error is detected after some changes
have already been made, use the ``{'FINISHED'}`` return code, or the behavior
of undo will be confusing for the user.

.. note::

   Operator subclasses must be registered before accessing them from blender.

"""
import bpy


class HelloWorldOperator(bpy.types.Operator):
    bl_idname = "wm.hello_world"
    bl_label = "Minimal Operator"

    def execute(self, context):
        print("Hello World")
        return {'FINISHED'}


# Only needed if you want to add into a dynamic menu.
def menu_func(self, context):
    self.layout.operator(HelloWorldOperator.bl_idname, text="Hello World Operator")


# Register and add to the view menu (required to also use F3 search "Hello World Operator" for quick access).
bpy.utils.register_class(HelloWorldOperator)
bpy.types.VIEW3D_MT_view.append(menu_func)

# Test call to the newly defined operator.
bpy.ops.wm.hello_world()
