import bpy
from mathutils import Vector
from bpy.props import FloatVectorProperty


class ViewOperator(bpy.types.Operator):
    """Translate the view using mouse events"""
    bl_idname = "view3d.modal_operator"
    bl_label = "Simple View Operator"

    offset: FloatVectorProperty(
        name="Offset",
        size=3,
    )

    def execute(self, context):
        v3d = context.space_data
        rv3d = v3d.region_3d

        rv3d.view_location = self._initial_location + Vector(self.offset)

    def modal(self, context, event):
        v3d = context.space_data
        rv3d = v3d.region_3d

        if event.type == 'MOUSEMOVE':
            self.offset = (self._initial_mouse - Vector((event.mouse_x, event.mouse_y, 0.0))) * 0.02
            self.execute(context)
            context.area.header_text_set("Offset {:.4f} {:.4f} {:.4f}".format(*self.offset))

        elif event.type == 'LEFTMOUSE':
            context.area.header_text_set(None)
            return {'FINISHED'}

        elif event.type in {'RIGHTMOUSE', 'ESC'}:
            rv3d.view_location = self._initial_location
            context.area.header_text_set(None)
            return {'CANCELLED'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):

        if context.space_data.type == 'VIEW_3D':
            v3d = context.space_data
            rv3d = v3d.region_3d

            if rv3d.view_perspective == 'CAMERA':
                rv3d.view_perspective = 'PERSP'

            self._initial_mouse = Vector((event.mouse_x, event.mouse_y, 0.0))
            self._initial_location = rv3d.view_location.copy()

            context.window_manager.modal_handler_add(self)
            return {'RUNNING_MODAL'}
        else:
            self.report({'WARNING'}, "Active space must be a View3d")
            return {'CANCELLED'}


def menu_func(self, context):
    self.layout.operator(ViewOperator.bl_idname, text="Simple View Modal Operator")


# Register and add to the "view" menu (required to also use F3 search "Simple View Modal Operator" for quick access).
def register():
    bpy.utils.register_class(ViewOperator)
    bpy.types.VIEW3D_MT_view.append(menu_func)


def unregister():
    bpy.utils.unregister_class(ViewOperator)
    bpy.types.VIEW3D_MT_view.remove(menu_func)


if __name__ == "__main__":
    register()
