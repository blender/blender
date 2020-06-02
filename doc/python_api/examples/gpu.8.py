"""
Copy Offscreen Rendering result back to RAM
-------------------------------------------

This will create a new image with the given name.
If it already exists, it will override the existing one.

Currently almost all of the execution time is spent in the last line.
In the future this will hopefully be solved by implementing the Python buffer protocol
for :class:`bgl.Buffer` and :class:`bpy.types.Image.pixels` (aka ``bpy_prop_array``).
"""
import bpy
import gpu
import bgl
import random
from mathutils import Matrix
from gpu_extras.presets import draw_circle_2d

IMAGE_NAME = "Generated Image"
WIDTH = 512
HEIGHT = 512
RING_AMOUNT = 10


offscreen = gpu.types.GPUOffScreen(WIDTH, HEIGHT)

with offscreen.bind():
    bgl.glClear(bgl.GL_COLOR_BUFFER_BIT)
    with gpu.matrix.push_pop():
        # reset matrices -> use normalized device coordinates [-1, 1]
        gpu.matrix.load_matrix(Matrix.Identity(4))
        gpu.matrix.load_projection_matrix(Matrix.Identity(4))

        for i in range(RING_AMOUNT):
            draw_circle_2d(
                (random.uniform(-1, 1), random.uniform(-1, 1)),
                (1, 1, 1, 1), random.uniform(0.1, 1), 20)

    buffer = bgl.Buffer(bgl.GL_BYTE, WIDTH * HEIGHT * 4)
    bgl.glReadBuffer(bgl.GL_BACK)
    bgl.glReadPixels(0, 0, WIDTH, HEIGHT, bgl.GL_RGBA, bgl.GL_UNSIGNED_BYTE, buffer)

offscreen.free()


if not IMAGE_NAME in bpy.data.images:
    bpy.data.images.new(IMAGE_NAME, WIDTH, HEIGHT)
image = bpy.data.images[IMAGE_NAME]
image.scale(WIDTH, HEIGHT)
image.pixels = [v / 255 for v in buffer]
