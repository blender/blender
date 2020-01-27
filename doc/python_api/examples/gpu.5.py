"""
Mesh with Random Vertex Colors
------------------------------
"""
import bpy
import gpu
import bgl
import numpy as np
from random import random
from gpu_extras.batch import batch_for_shader

mesh = bpy.context.active_object.data
mesh.calc_loop_triangles()

vertices = np.empty((len(mesh.vertices), 3), 'f')
indices = np.empty((len(mesh.loop_triangles), 3), 'i')

mesh.vertices.foreach_get(
    "co", np.reshape(vertices, len(mesh.vertices) * 3))
mesh.loop_triangles.foreach_get(
    "vertices", np.reshape(indices, len(mesh.loop_triangles) * 3))

vertex_colors = [(random(), random(), random(), 1) for _ in range(len(mesh.vertices))]

shader = gpu.shader.from_builtin('3D_SMOOTH_COLOR')
batch = batch_for_shader(
    shader, 'TRIS',
    {"pos": vertices, "color": vertex_colors},
    indices=indices,
)


def draw():
    bgl.glEnable(bgl.GL_DEPTH_TEST)
    batch.draw(shader)
    bgl.glDisable(bgl.GL_DEPTH_TEST)


bpy.types.SpaceView3D.draw_handler_add(draw, (), 'WINDOW', 'POST_VIEW')
