"""
Example Macro
+++++++++++++

This example creates a simple macro operator that
moves the active object and then rotates it.
It demonstrates:

- Defining a macro operator class.
- Registering it and defining sub-operators.
- Setting property values for each step.
"""

import bpy


class OBJECT_OT_simple_macro(bpy.types.Macro):
    bl_idname = "object.simple_macro"
    bl_label = "Simple Transform Macro"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.active_object is not None


def register():
    bpy.utils.register_class(OBJECT_OT_simple_macro)

    # Define steps after registration and set operator values via .properties
    step = OBJECT_OT_simple_macro.define("transform.translate")
    props = step.properties
    props.value = (1.0, 0.0, 0.0)
    props.constraint_axis = (True, False, False)

    step = OBJECT_OT_simple_macro.define("transform.rotate")
    props = step.properties
    props.value = 0.785398  # 45 degrees in radians
    props.orient_axis = 'Z'


def unregister():
    bpy.utils.unregister_class(OBJECT_OT_simple_macro)


if __name__ == "__main__":
    register()

    # To run the macro:
    bpy.ops.object.simple_macro()
