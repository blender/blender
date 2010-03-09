from bpy.props import *

class ModalOperator(bpy.types.Operator):
    '''Move an object with the mouse, example.'''
    bl_idname = "object.modal_operator"
    bl_label = "Simple Modal Operator"

    first_mouse_x = IntProperty()
    first_value = FloatProperty()

    def modal(self, context, event):
        if event.type == 'MOUSEMOVE':
            delta = self.properties.first_mouse_x - event.mouse_x
            context.object.location.x = self.properties.first_value + delta * 0.01

        elif event.type == 'LEFTMOUSE':
            return {'FINISHED'}

        elif event.type in ('RIGHTMOUSE', 'ESC'):
            context.object.location.x = self.properties.first_value
            return {'CANCELLED'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        if context.object:
            context.manager.add_modal_handler(self)
            self.properties.first_mouse_x = event.mouse_x
            self.properties.first_value = context.object.location.x
            return {'RUNNING_MODAL'}
        else:
            self.report({'WARNING'}, "No active object, could not finish")
            return {'CANCELLED'}


bpy.types.register(ModalOperator)

if __name__ == "__main__":
    bpy.ops.object.modal_operator()
