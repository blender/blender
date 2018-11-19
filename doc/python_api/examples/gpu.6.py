"""
2D Rectangle
------------
"""
import bpy
import gpu
from gpu_extras.batch import batch_for_shader

vertices = (
    (100, 100), (300, 100),
    (100, 200), (300, 200))

indices = (
    (0, 1, 2), (2, 1, 3))

shader = gpu.shader.from_builtin('2D_UNIFORM_COLOR')
batch = batch_for_shader(shader, 'TRIS', {"pos": vertices}, indices=indices)


def draw():
    shader.bind()
    shader.uniform_float("color", (0, 0.5, 0.5, 1.0))
    batch.draw(shader)


bpy.types.SpaceView3D.draw_handler_add(draw, (), 'WINDOW', 'POST_PIXEL')
