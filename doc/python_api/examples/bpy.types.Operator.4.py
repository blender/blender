"""
Dialog Box
++++++++++

This operator uses its :class:`Operator.invoke` function to call a popup.
"""
import bpy


class DialogOperator(bpy.types.Operator):
    bl_idname = "object.dialog_operator"
    bl_label = "Simple Dialog Operator"

    my_float: bpy.props.FloatProperty(name="Some Floating Point")
    my_bool: bpy.props.BoolProperty(name="Toggle Option")
    my_string: bpy.props.StringProperty(name="String Value")

    def execute(self, context):
        message = "Popup Values: {:f}, {:d}, '{:s}'".format(
            self.my_float, self.my_bool, self.my_string,
        )
        self.report({'INFO'}, message)
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)


# Only needed if you want to add into a dynamic menu.
def menu_func(self, context):
    self.layout.operator(DialogOperator.bl_idname, text="Dialog Operator")


# Register and add to the object menu (required to also use F3 search "Dialog Operator" for quick access)
bpy.utils.register_class(DialogOperator)
bpy.types.VIEW3D_MT_object.append(menu_func)

# Test call.
bpy.ops.object.dialog_operator('INVOKE_DEFAULT')
