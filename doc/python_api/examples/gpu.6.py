"""
2D Image
--------

To use this example you have to provide an image that should be displayed.
"""
import bpy
import gpu
from gpu_extras.batch import batch_for_shader

IMAGE_NAME = "Untitled"
image = bpy.data.images[IMAGE_NAME]
texture = gpu.texture.from_image(image)

shader = gpu.shader.from_builtin('2D_IMAGE')
batch = batch_for_shader(
    shader, 'TRI_FAN',
    {
        "pos": ((100, 100), (200, 100), (200, 200), (100, 200)),
        "texCoord": ((0, 0), (1, 0), (1, 1), (0, 1)),
    },
)

def draw():
    shader.bind()
    shader.uniform_sampler("image", texture)
    batch.draw(shader)


bpy.types.SpaceView3D.draw_handler_add(draw, (), 'WINDOW', 'POST_PIXEL')
