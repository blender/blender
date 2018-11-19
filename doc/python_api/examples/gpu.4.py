"""
Wireframe Cube using Index Buffer
---------------------------------
"""
import bpy
import gpu
from gpu_extras.batch import batch_for_shader

coords = (
    (-1, -1, -1), (+1, -1, -1),
    (-1, +1, -1), (+1, +1, -1),
    (-1, -1, +1), (+1, -1, +1),
    (-1, +1, +1), (+1, +1, +1))

indices = (
    (0, 1), (0, 2), (1, 3), (2, 3),
    (4, 5), (4, 6), (5, 7), (6, 7),
    (0, 4), (1, 5), (2, 6), (3, 7))

shader = gpu.shader.from_builtin('3D_UNIFORM_COLOR')
batch = batch_for_shader(shader, 'LINES', {"pos": coords}, indices=indices)


def draw():
    shader.bind()
    shader.uniform_float("color", (1, 0, 0, 1))
    batch.draw(shader)


bpy.types.SpaceView3D.draw_handler_add(draw, (), 'WINDOW', 'POST_VIEW')
