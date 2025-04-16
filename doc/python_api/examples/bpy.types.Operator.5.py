"""
Custom Drawing
++++++++++++++

By default operator properties use an automatic user interface layout.
If you need more control you can create your own layout with a
:class:`Operator.draw` function.

This works like the :class:`Panel` and :class:`Menu` draw functions, its used
for dialogs and file selectors.
"""
import bpy


class CustomDrawOperator(bpy.types.Operator):
    bl_idname = "object.custom_draw"
    bl_label = "Simple Modal Operator"

    filepath: bpy.props.StringProperty(subtype="FILE_PATH")

    my_float: bpy.props.FloatProperty(name="Float")
    my_bool: bpy.props.BoolProperty(name="Toggle Option")
    my_string: bpy.props.StringProperty(name="String Value")

    def execute(self, context):
        print("Test", self)
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)

    def draw(self, context):
        layout = self.layout
        col = layout.column()
        col.label(text="Custom Interface!")

        row = col.row()
        row.prop(self, "my_float")
        row.prop(self, "my_bool")

        col.prop(self, "my_string")


# Only needed if you want to add into a dynamic menu.
def menu_func(self, context):
    self.layout.operator(CustomDrawOperator.bl_idname, text="Custom Draw Operator")


# Register and add to the object menu (required to also use F3 search "Custom Draw Operator" for quick access).
bpy.utils.register_class(CustomDrawOperator)
bpy.types.VIEW3D_MT_object.append(menu_func)

# Test call.
bpy.ops.object.custom_draw('INVOKE_DEFAULT')
