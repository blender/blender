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

shader = gpu.shader.from_builtin('IMAGE_SCENE_LINEAR_TO_REC709_SRGB')
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

"""
3D Image
--------

Similar to the 2D Image shader, but works with 3D positions for the image vertices.
To use this example you have to provide an image that should be displayed.
"""
import bpy
import gpu
from gpu_extras.batch import batch_for_shader

IMAGE_NAME = "Untitled"
image = bpy.data.images[IMAGE_NAME]
texture = gpu.texture.from_image(image)

shader = gpu.shader.from_builtin('IMAGE_SCENE_LINEAR_TO_REC709_SRGB')
batch = batch_for_shader(
    shader, 'TRIS',
    {
        "pos": ((0, 0, 0), (0, 1, 1), (1, 1, 1), (1, 1, 1), (1, 0, 0), (0, 0, 0)),
        "texCoord": ((0, 0), (0, 1), (1, 1), (1, 1), (1, 0), (0, 0)),
    },
)


def draw():
    shader.uniform_sampler("image", texture)
    batch.draw(shader)


bpy.types.SpaceView3D.draw_handler_add(draw, (), 'WINDOW', 'POST_VIEW')
