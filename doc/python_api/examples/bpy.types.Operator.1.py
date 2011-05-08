"""
Invoke Function
+++++++++++++++
:class:`Operator.invoke` is used to initialize the operator from the context
at the moment the operator is called.
invoke() is typically used to assign properties which are then used by
execute().
Some operators don't have an execute() function, removing the ability to be
repeated from a script or macro.

This example shows how to define an operator which gets mouse input to
execute a function and that this operator can be invoked or executed from
the python api.

Also notice this operator defines its own properties, these are different
to typical class properties because blender registers them with the
operator, to use as arguments when called, saved for operator undo/redo and
automatically added into the user interface.
"""
import bpy


class SimpleMouseOperator(bpy.types.Operator):
    """ This operator shows the mouse location,
        this string is used for the tooltip and API docs
    """
    bl_idname = "wm.mouse_position"
    bl_label = "Invoke Mouse Operator"

    x = bpy.props.IntProperty()
    y = bpy.props.IntProperty()

    def execute(self, context):
        # rather then printing, use the report function,
        # this way the message appears in the header,
        self.report({'INFO'}, "Mouse coords are %d %d" % (self.x, self.y))
        return {'FINISHED'}

    def invoke(self, context, event):
        self.x = event.mouse_x
        self.y = event.mouse_y
        return self.execute(context)

bpy.utils.register_class(SimpleMouseOperator)

# Test call to the newly defined operator.
# Here we call the operator and invoke it, meaning that the settings are taken
# from the mouse.
bpy.ops.wm.mouse_position('INVOKE_DEFAULT')

# Another test call, this time call execute() directly with pre-defined settings.
bpy.ops.wm.mouse_position('EXEC_DEFAULT', x=20, y=66)
