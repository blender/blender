"""
.. _modal_operator:

Modal Execution
+++++++++++++++

This operator defines a :class:`Operator.modal` function that will keep being
run to handle events until it returns ``{'FINISHED'}`` or ``{'CANCELLED'}``.

Modal operators run every time a new event is detected, such as a mouse click
or key press. Conversely, when no new events are detected, the modal operator
will not run. Modal operators are especially useful for interactive tools, an
operator can have its own state where keys toggle options as the operator runs.
Grab, Rotate, Scale, and Fly-Mode are examples of modal operators.

:class:`Operator.invoke` is used to initialize the operator as being active
by returning ``{'RUNNING_MODAL'}``, initializing the modal loop.

Notice ``__init__()`` and ``__del__()`` are declared.
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
        elif event.type in {'RIGHTMOUSE', 'ESC'}:  # Cancel
            # Revert all changes that have been made
            context.object.location.x = self.init_loc_x
            return {'CANCELLED'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        self.init_loc_x = context.object.location.x
        self.value = event.mouse_x
        self.execute(context)

        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}


# Only needed if you want to add into a dynamic menu.
def menu_func(self, context):
    self.layout.operator(ModalOperator.bl_idname, text="Modal Operator")


# Register and add to the object menu (required to also use F3 search "Modal Operator" for quick access).
bpy.utils.register_class(ModalOperator)
bpy.types.VIEW3D_MT_object.append(menu_func)

# test call
bpy.ops.object.modal_operator('INVOKE_DEFAULT')
