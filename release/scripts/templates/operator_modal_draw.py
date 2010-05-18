import bgl
import blf

def draw_callback_px(self, context):
    print("mouse points", len(self.mouse_path))

    font_id = 0 # XXX, need to find out how best to get this.

    # draw some text
    blf.position(font_id, 15, 30, 0)
    blf.size(font_id, 20, 72)
    blf.draw(font_id, "Hello Word " + str(len(self.mouse_path)))

    # 50% alpha, 2 pixel width line
    bgl.glEnable(bgl.GL_BLEND)
    bgl.glColor4f(0.0, 0.0, 0.0, 0.5)
    bgl.glLineWidth(2)

    bgl.glBegin(bgl.GL_LINE_STRIP)
    for x, y in self.mouse_path:
        bgl.glVertex2i(x, y)

    bgl.glEnd()

    # restore opengl defaults
    bgl.glLineWidth(1)
    bgl.glDisable(bgl.GL_BLEND)
    bgl.glColor4f(0.0, 0.0, 0.0, 1.0)


class ModalDrawOperator(bpy.types.Operator):
    '''Draw a line with the mouse'''
    bl_idname = "view3d.modal_operator"
    bl_label = "Simple Modal View3D Operator"

    def modal(self, context, event):
        context.area.tag_redraw()

        if event.type == 'MOUSEMOVE':
            self.mouse_path.append((event.mouse_region_x, event.mouse_region_y))

        elif event.type == 'LEFTMOUSE':
            context.region.callback_remove(self._handle)
            return {'FINISHED'}

        elif event.type in ('RIGHTMOUSE', 'ESC'):
            context.region.callback_remove(self._handle)
            return {'CANCELLED'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        if context.area.type == 'VIEW_3D':
            context.manager.add_modal_handler(self)

            # Add the region OpenGL drawing callback
            # draw in view space with 'POST_VIEW' and 'PRE_VIEW'
            self._handle = context.region.callback_add(draw_callback_px, (self, context), 'POST_PIXEL')

            self.mouse_path = []

            return {'RUNNING_MODAL'}
        else:
            self.report({'WARNING'}, "View3D not found, cannot run operator")
            return {'CANCELLED'}


def register():
    bpy.types.register(ModalDrawOperator)


def unregister():
    bpy.types.unregister(ModalDrawOperator)


if __name__ == "__main__":
    register()
