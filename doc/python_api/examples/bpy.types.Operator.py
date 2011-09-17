"""
Basic Operator Example
++++++++++++++++++++++
This script shows simple operator which prints a message.

Since the operator only has an :class:`Operator.execute` function it takes no
user input.

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

bpy.utils.register_class(HelloWorldOperator)

# test call to the newly defined operator
bpy.ops.wm.hello_world()
