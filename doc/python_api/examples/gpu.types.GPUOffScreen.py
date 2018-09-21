# Draws an off-screen buffer and display it in the corner of the view.
import bpy
import bgl
import gpu
import numpy as np

g_imageVertSrc = '''
in vec2 texCoord;
in vec2 pos;

out vec2 texCoord_interp;

void main()
{
    gl_Position = vec4(pos.xy, 0.0f, 1.0);
    gl_Position.z = 1.0f;
    texCoord_interp = texCoord;
}
'''

g_imageFragSrc = '''
in vec2 texCoord_interp;
out vec4 fragColor;

uniform sampler2D image;

void main()
{
    fragColor = texture(image, texCoord_interp);
}
'''

g_plane_vertices = np.array([
    ([-1.0, -1.0], [0.0, 0.0]),
    ([1.0, -1.0], [1.0, 0.0]),
    ([1.0,  1.0], [1.0, 1.0]),
    ([1.0,  1.0], [1.0, 1.0]),
    ([-1.0,  1.0], [0.0, 1.0]),
    ([-1.0, -1.0], [0.0, 0.0]),
], [('pos', 'f4', 2), ('uv', 'f4', 2)])


class VIEW3D_OT_draw_offscreen(bpy.types.Operator):
    bl_idname = "view3d.offscreen_draw"
    bl_label = "Viewport Offscreen Draw"

    _handle_calc = None
    _handle_draw = None
    is_enabled = False

    global_shader = None
    batch_plane = None
    uniform_image = -1
    shader = None

    # manage draw handler
    @staticmethod
    def draw_callback_px(self, context):
        scene = context.scene
        aspect_ratio = scene.render.resolution_x / scene.render.resolution_y

        self._update_offscreen(context, self._offscreen)
        self._opengl_draw(context, self._texture, aspect_ratio, 0.2)

    @staticmethod
    def handle_add(self, context):
        VIEW3D_OT_draw_offscreen._handle_draw = bpy.types.SpaceView3D.draw_handler_add(
            self.draw_callback_px, (self, context),
            'WINDOW', 'POST_PIXEL',
        )

    @staticmethod
    def handle_remove():
        if VIEW3D_OT_draw_offscreen._handle_draw is not None:
            bpy.types.SpaceView3D.draw_handler_remove(VIEW3D_OT_draw_offscreen._handle_draw, 'WINDOW')

            VIEW3D_OT_draw_offscreen._handle_draw = None

    # off-screen buffer
    @staticmethod
    def _setup_offscreen(context):
        scene = context.scene
        aspect_ratio = scene.render.resolution_x / scene.render.resolution_y

        try:
            offscreen = gpu.types.GPUOffScreen(512, int(512 / aspect_ratio))
        except Exception as e:
            print(e)
            offscreen = None

        return offscreen

    @staticmethod
    def _update_offscreen(context, offscreen):
        scene = context.scene
        view_layer = context.view_layer
        render = scene.render
        camera = scene.camera

        modelview_matrix = camera.matrix_world.inverted()
        projection_matrix = camera.calc_matrix_camera(
            context.depsgraph,
            x=render.resolution_x,
            y=render.resolution_y,
            scale_x=render.pixel_aspect_x,
            scale_y=render.pixel_aspect_y,
        )

        offscreen.draw_view3d(
            scene,
            view_layer,
            context.space_data,
            context.region,
            projection_matrix,
            modelview_matrix,
        )

    def _opengl_draw(self, context, texture, aspect_ratio, scale):
        """
        OpenGL code to draw a rectangle in the viewport
        """
        # view setup
        bgl.glDisable(bgl.GL_DEPTH_TEST)

        viewport = bgl.Buffer(bgl.GL_INT, 4)
        bgl.glGetIntegerv(bgl.GL_VIEWPORT, viewport)

        active_texture = bgl.Buffer(bgl.GL_INT, 1)
        bgl.glGetIntegerv(bgl.GL_TEXTURE_2D, active_texture)

        width = int(scale * viewport[2])
        height = int(width / aspect_ratio)

        bgl.glViewport(viewport[0], viewport[1], width, height)
        bgl.glScissor(viewport[0], viewport[1], width, height)

        # draw routine
        batch_plane = self.get_batch_plane()

        shader = VIEW3D_OT_draw_offscreen.shader
        # bind it so we can pass the new uniform values
        shader.bind()

        bgl.glEnable(bgl.GL_TEXTURE_2D)
        bgl.glActiveTexture(bgl.GL_TEXTURE0)
        bgl.glBindTexture(bgl.GL_TEXTURE_2D, texture)

        shader.uniform_int(VIEW3D_OT_draw_offscreen.uniform_image, 0)
        batch_plane.draw()

        # restoring settings
        bgl.glBindTexture(bgl.GL_TEXTURE_2D, active_texture[0])
        bgl.glDisable(bgl.GL_TEXTURE_2D)

        # reset view
        bgl.glViewport(viewport[0], viewport[1], viewport[2], viewport[3])
        bgl.glScissor(viewport[0], viewport[1], viewport[2], viewport[3])

    def get_batch_plane(self):
        if self.batch_plane is None:
            global g_plane_vertices

            format = gpu.types.GPUVertFormat()
            pos_id = format.attr_add(
                    id="pos",
                    comp_type="F32",
                    len=2,
                    fetch_mode="FLOAT")

            uv_id = format.attr_add(
                    id="texCoord",
                    comp_type="F32",
                    len=2,
                    fetch_mode="FLOAT")

            vbo = gpu.types.GPUVertBuf(
                    len=len(g_plane_vertices),
                    format=format)

            vbo.fill(id=pos_id, data=g_plane_vertices["pos"])
            vbo.fill(id=uv_id, data=g_plane_vertices["uv"])

            batch_plane = gpu.types.GPUBatch(type="TRIS", buf=vbo)
            shader = self.global_shader

            VIEW3D_OT_draw_offscreen.shader = shader
            VIEW3D_OT_draw_offscreen.uniform_image = shader.uniform_from_name("image")

            batch_plane.program_set(shader)
            VIEW3D_OT_draw_offscreen.batch_plane = batch_plane
        return VIEW3D_OT_draw_offscreen.batch_plane

    # operator functions
    @classmethod
    def poll(cls, context):
        return context.area.type == 'VIEW_3D'

    def modal(self, context, event):
        if context.area:
            context.area.tag_redraw()

        if event.type in {'RIGHTMOUSE', 'ESC'}:
            self.cancel(context)
            return {'CANCELLED'}

        return {'PASS_THROUGH'}

    def invoke(self, context, event):
        if VIEW3D_OT_draw_offscreen.is_enabled:
            self.cancel(context)
            return {'FINISHED'}
        else:
            self._offscreen = VIEW3D_OT_draw_offscreen._setup_offscreen(context)
            if self._offscreen:
                self._texture = self._offscreen.color_texture
            else:
                self.report({'ERROR'}, "Error initializing offscreen buffer. More details in the console")
                return {'CANCELLED'}

            VIEW3D_OT_draw_offscreen.handle_add(self, context)
            VIEW3D_OT_draw_offscreen.is_enabled = True

            if context.area:
                context.area.tag_redraw()

            context.window_manager.modal_handler_add(self)
            return {'RUNNING_MODAL'}

    def cancel(self, context):
        VIEW3D_OT_draw_offscreen.handle_remove()
        VIEW3D_OT_draw_offscreen.is_enabled = False

        if VIEW3D_OT_draw_offscreen.batch_plane is not None:
            del VIEW3D_OT_draw_offscreen.batch_plane
            VIEW3D_OT_draw_offscreen.batch_plane = None

        VIEW3D_OT_draw_offscreen.shader = None

        if context.area:
            context.area.tag_redraw()


def register():
    try:
        cls = getattr(bpy.types, "VIEW3D_OT_draw_offscreen")
        del cls.global_shader
    except:
        pass

    shader = gpu.types.GPUShader(g_imageVertSrc, g_imageFragSrc)
    VIEW3D_OT_draw_offscreen.global_shader = shader

    bpy.utils.register_class(VIEW3D_OT_draw_offscreen)


def unregister():
    bpy.utils.unregister_class(VIEW3D_OT_draw_offscreen)
    VIEW3D_OT_draw_offscreen.global_shader = None


if __name__ == "__main__":
    try:
        unregister()
    except RuntimeError:
        pass

    register()
