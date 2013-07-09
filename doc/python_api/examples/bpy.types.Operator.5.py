"""
Modal Execution
+++++++++++++++
This operator defines a :class:`Operator.modal` function which running,
handling events until it returns {'FINISHED'} or {'CANCELLED'}.

Grab, Rotate, Scale and Fly-Mode are examples of modal operators.
They are especially useful for interactive tools,
your operator can have its own state where keys toggle options as the operator
runs.

:class:`Operator.invoke` is used to initialize the operator as being by
returning {'RUNNING_MODAL'}, initializing the modal loop.

Notice __init__() and __del__() are declared.
For other operator types they are not useful but for modal operators they will
be called before the :class:`Operator.invoke` and after the operator finishes.
"""
import bpy


class ModalOperator(bpy.types.Operator):
    bl_idname = "object.modal_operator"
    bl_label = "Simple Modal Operator"

    def __init__(self):
        print("Start")

    def __del__(self):
        print("End")

    def execute(self, context):
        context.object.location.x = self.value / 100.0
        return {'FINISHED'}

    def modal(self, context, event):
        if event.type == 'MOUSEMOVE':  # Apply
            self.value = event.mouse_x
            self.execute(context)
        elif event.type == 'LEFTMOUSE':  # Confirm
            return {'FINISHED'}
        elif event.type in ('RIGHTMOUSE', 'ESC'):  # Cancel
            context.object.location.x = self.init_loc_x
            return {'CANCELLED'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        self.init_loc_x = context.object.location.x
        self.value = event.mouse_x
        self.execute(context)

        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}


bpy.utils.register_class(ModalOperator)

# test call
bpy.ops.object.modal_operator('INVOKE_DEFAULT')
