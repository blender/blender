import bpy


class ModalTimerOperator(bpy.types.Operator):
    """Operator which runs itself from a timer"""
    bl_idname = "wm.modal_timer_operator"
    bl_label = "Modal Timer Operator"

    _timer = None
    _theme = None
    _original_color = None

    def modal(self, context, event):
        if event.type in {'RIGHTMOUSE', 'ESC'}:
            self.cancel(context)
            return {'CANCELLED'}

        if event.type == 'TIMER':
            # Change theme color, silly!
            color = self._theme.view_3d.space.gradients.high_gradient
            color.s = 1.0
            color.h += 0.01

        return {'PASS_THROUGH'}

    def execute(self, context):
        # Store the original color to avoid altering user settings
        self._theme = context.preferences.themes[0]
        color = self._theme.view_3d.space.gradients.high_gradient
        self._original_color = color[:]
        wm = context.window_manager
        self._timer = wm.event_timer_add(0.1, window=context.window)
        wm.modal_handler_add(self)
        return {'RUNNING_MODAL'}

    def cancel(self, context):
        wm = context.window_manager
        wm.event_timer_remove(self._timer)
        self._theme.view_3d.space.gradients.high_gradient = self._original_color


def menu_func(self, context):
    self.layout.operator(ModalTimerOperator.bl_idname, text=ModalTimerOperator.bl_label)


def register():
    bpy.utils.register_class(ModalTimerOperator)
    bpy.types.VIEW3D_MT_view.append(menu_func)


# Register and add to the "view" menu (required to also use F3 search "Modal Timer Operator" for quick access).
def unregister():
    bpy.utils.unregister_class(ModalTimerOperator)
    bpy.types.VIEW3D_MT_view.remove(menu_func)


if __name__ == "__main__":
    register()

    # Test call.
    bpy.ops.wm.modal_timer_operator()
