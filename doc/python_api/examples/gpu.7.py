"""
Generate a texture using Offscreen Rendering
--------------------------------------------

#. Create an :class:`gpu.types.GPUOffScreen` object.
#. Draw some circles into it.
#. Make a new shader for drawing a planar texture in 3D.
#. Draw the generated texture using the new shader.
"""
import bpy
import gpu
from mathutils import Matrix
from gpu_extras.batch import batch_for_shader
from gpu_extras.presets import draw_circle_2d

# Create and fill offscreen
##########################################

offscreen = gpu.types.GPUOffScreen(512, 512)

with offscreen.bind():
    fb = gpu.state.active_framebuffer_get()
    fb.clear(color=(0.0, 0.0, 0.0, 0.0))
    with gpu.matrix.push_pop():
        # reset matrices -> use normalized device coordinates [-1, 1]
        gpu.matrix.load_matrix(Matrix.Identity(4))
        gpu.matrix.load_projection_matrix(Matrix.Identity(4))

        amount = 10
        for i in range(-amount, amount + 1):
            x_pos = i / amount
            draw_circle_2d((x_pos, 0.0), (1, 1, 1, 1), 0.5, segments=200)


# Drawing the generated texture in 3D space
#############################################

vert_out = gpu.types.GPUStageInterfaceInfo("my_interface")
vert_out.smooth('VEC2', "uvInterp")

shader_info = gpu.types.GPUShaderCreateInfo()
shader_info.push_constant('MAT4', "viewProjectionMatrix")
shader_info.push_constant('MAT4', "modelMatrix")
shader_info.sampler(0, 'FLOAT_2D', "image")
shader_info.vertex_in(0, 'VEC2', "position")
shader_info.vertex_in(1, 'VEC2', "uv")
shader_info.vertex_out(vert_out)
shader_info.fragment_out(0, 'VEC4', "FragColor")

shader_info.vertex_source(
    "void main()"
    "{"
    "  uvInterp = uv;"
    "  gl_Position = viewProjectionMatrix * modelMatrix * vec4(position, 0.0, 1.0);"
    "}"
)

shader_info.fragment_source(
    "void main()"
    "{"
    "  FragColor = texture(image, uvInterp);"
    "}"
)

shader = gpu.shader.create_from_info(shader_info)
del vert_out
del shader_info

batch = batch_for_shader(
    shader, 'TRI_FAN',
    {
        "position": ((-1, -1), (1, -1), (1, 1), (-1, 1)),
        "uv": ((0, 0), (1, 0), (1, 1), (0, 1)),
    },
)


def draw():
    shader.bind()
    shader.uniform_float("modelMatrix", Matrix.Translation((1, 2, 3)) @ Matrix.Scale(3, 4))
    shader.uniform_float("viewProjectionMatrix", bpy.context.region_data.perspective_matrix)
    shader.uniform_sampler("image", offscreen.texture_color)
    batch.draw(shader)


bpy.types.SpaceView3D.draw_handler_add(draw, (), 'WINDOW', 'POST_VIEW')
