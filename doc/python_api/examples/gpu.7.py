"""
2D Image
--------
"""
import bpy
import gpu
import bgl
from gpu_extras.batch import batch_for_shader

IMAGE_NAME = "Untitled"
image = bpy.data.images[IMAGE_NAME]

coords = [
    (100, 100), (200, 100),
    (100, 200), (200, 200)]

uvs = [(0, 0), (1, 0), (0, 1), (1, 1)]

indices = [(0, 1, 2), (2, 1, 3)]

shader = gpu.shader.from_builtin('2D_IMAGE')
batch = batch_for_shader(shader, 'TRIS',
    {"pos" : coords,
     "texCoord" : uvs},
    indices=indices)

# send image to gpu if it isn't there already
if image.gl_load():
    raise Exception()

# texture identifier on gpu
texture_id = image.bindcode

def draw():
    # in case someone disabled it before
    bgl.glEnable(bgl.GL_TEXTURE_2D)

    # bind texture to image unit 0
    bgl.glActiveTexture(bgl.GL_TEXTURE0)
    bgl.glBindTexture(bgl.GL_TEXTURE_2D, texture_id)

    shader.bind()
    # tell shader to use the image that is bound to image unit 0
    shader.uniform_int("image", 0)
    batch.draw(shader)

    bgl.glDisable(bgl.GL_TEXTURE_2D)

bpy.types.SpaceView3D.draw_handler_add(draw, (), 'WINDOW', 'POST_PIXEL')