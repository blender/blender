from mathutils import Vector
from bpy.props import FloatVectorProperty

class ViewOperator(bpy.types.Operator):
    '''Translate the view using mouse events.'''
    bl_idname = "view3d.modal_operator"
    bl_label = "Simple View Operator"

    offset = FloatVectorProperty(name="Offset", size=3)


    def execute(self, context):
        v3d = context.space_data
        rv3d = v3d.region_3d

        rv3d.view_location = self._initial_location + Vector(self.properties.offset)

    def modal(self, context, event):
        v3d = context.space_data
        rv3d = v3d.region_3d

        if event.type == 'MOUSEMOVE':
            self.properties.offset = (self._initial_mouse - Vector((event.mouse_x, event.mouse_y, 0.0))) * 0.02
            self.execute(context)

        elif event.type == 'LEFTMOUSE':
            return {'FINISHED'}

        elif event.type in ('RIGHTMOUSE', 'ESC'):
            rv3d.view_location = self._initial_location
            return {'CANCELLED'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):

        if context.space_data.type == 'VIEW_3D':
            v3d = context.space_data
            rv3d = v3d.region_3d

            context.manager.add_modal_handler(self)

            if rv3d.view_perspective == 'CAMERA':
                rv3d.view_perspective = 'PERSP'

            self._initial_mouse = Vector((event.mouse_x, event.mouse_y, 0.0))
            self._initial_location = rv3d.view_location.copy()

            return {'RUNNING_MODAL'}
        else:
            self.report({'WARNING'}, "Active space must be a View3d")
            return {'CANCELLED'}


bpy.types.register(ViewOperator)
