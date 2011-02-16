"""
Basic Operator Example
++++++++++++++++++++++
This script is the most simple operator you can write that does something.
"""
import bpy


class HelloWorldOperator(bpy.types.Operator):
    bl_idname = "wm.hello_world"
    bl_label = "Minimal Operator"

    def execute(self, context):
        print("Hello World")
        return {'FINISHED'}

bpy.utils.register_class(SimpleOperator)

# test call to the newly defined operator
bpy.ops.wm.hello_world()
