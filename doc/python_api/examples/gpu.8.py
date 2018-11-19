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
import bgl
from mathutils import Matrix
from gpu_extras.batch import batch_for_shader
from gpu_extras.presets import draw_circle_2d

# Create and fill offscreen
##########################################

offscreen = gpu.types.GPUOffScreen(512, 512)

with offscreen.bind():
    bgl.glClear(bgl.GL_COLOR_BUFFER_BIT)
    with gpu.matrix.push_pop():
        # reset matrices -> use normalized device coordinates [-1, 1]
        gpu.matrix.load_matrix(Matrix.Identity(4))
        gpu.matrix.load_projection_matrix(Matrix.Identity(4))

        amount = 10
        for i in range(-amount, amount + 1):
            x_pos = i / amount
            draw_circle_2d((x_pos, 0.0), (1, 1, 1, 1), 0.5, 200)


# Drawing the generated texture in 3D space
#############################################

vertex_shader = '''
    uniform mat4 modelMatrix;
    uniform mat4 viewProjectionMatrix;

    in vec2 position;
    in vec2 uv;

    out vec2 uvInterp;

    void main()
    {
        uvInterp = uv;
        gl_Position = viewProjectionMatrix * modelMatrix * vec4(position, 0.0, 1.0);
    }
'''

fragment_shader = '''
    uniform sampler2D image;

    in vec2 uvInterp;

    void main()
    {
        gl_FragColor = texture(image, uvInterp);
    }
'''

shader = gpu.types.GPUShader(vertex_shader, fragment_shader)
batch = batch_for_shader(
    shader, 'TRI_FAN',
    {
        "position": ((-1, -1), (1, -1), (1, 1), (-1, 1)),
        "uv": ((0, 0), (1, 0), (1, 1), (0, 1)),
    },
)


def draw():
    bgl.glActiveTexture(bgl.GL_TEXTURE0)
    bgl.glBindTexture(bgl.GL_TEXTURE_2D, offscreen.color_texture)

    shader.bind()
    shader.uniform_float("modelMatrix", Matrix.Translation((1, 2, 3)) @ Matrix.Scale(3, 4))
    shader.uniform_float("viewProjectionMatrix", bpy.context.region_data.perspective_matrix)
    shader.uniform_float("image", 0)
    batch.draw(shader)


bpy.types.SpaceView3D.draw_handler_add(draw, (), 'WINDOW', 'POST_VIEW')
