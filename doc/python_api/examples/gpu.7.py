"""
2D Image
--------

To use this example you have to provide an image that should be displayed.
"""
import bpy
import gpu
import bgl
from gpu_extras.batch import batch_for_shader

IMAGE_NAME = "Untitled"
image = bpy.data.images[IMAGE_NAME]

shader = gpu.shader.from_builtin('2D_IMAGE')
batch = batch_for_shader(
    shader, 'TRI_FAN',
    {
        "pos": ((100, 100), (200, 100), (200, 200), (100, 200)),
        "texCoord": ((0, 0), (1, 0), (1, 1), (0, 1)),
    },
)

if image.gl_load():
    raise Exception()


def draw():
    bgl.glActiveTexture(bgl.GL_TEXTURE0)
    bgl.glBindTexture(bgl.GL_TEXTURE_2D, image.bindcode)

    shader.bind()
    shader.uniform_int("image", 0)
    batch.draw(shader)


bpy.types.SpaceView3D.draw_handler_add(draw, (), 'WINDOW', 'POST_PIXEL')
