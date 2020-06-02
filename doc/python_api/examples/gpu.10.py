"""
Custom Shader for dotted 3D Line
--------------------------------

In this example the arc length (distance to the first point on the line) is calculated in every vertex.
Between the vertex and fragment shader that value is automatically interpolated
for all points that will be visible on the screen.
In the fragment shader the ``sin`` of the arc length is calculated.
Based on the result a decision is made on whether the fragment should be drawn or not.
"""
import bpy
import gpu
from random import random
from mathutils import Vector
from gpu_extras.batch import batch_for_shader

vertex_shader = '''
    uniform mat4 u_ViewProjectionMatrix;

    in vec3 position;
    in float arcLength;

    out float v_ArcLength;

    void main()
    {
        v_ArcLength = arcLength;
        gl_Position = u_ViewProjectionMatrix * vec4(position, 1.0f);
    }
'''

fragment_shader = '''
    uniform float u_Scale;

    in float v_ArcLength;

    void main()
    {
        if (step(sin(v_ArcLength * u_Scale), 0.5) == 1) discard;
        gl_FragColor = vec4(1.0);
    }
'''

coords = [Vector((random(), random(), random())) * 5 for _ in range(5)]

arc_lengths = [0]
for a, b in zip(coords[:-1], coords[1:]):
    arc_lengths.append(arc_lengths[-1] + (a - b).length)

shader = gpu.types.GPUShader(vertex_shader, fragment_shader)
batch = batch_for_shader(
    shader, 'LINE_STRIP',
    {"position": coords, "arcLength": arc_lengths},
)


def draw():
    shader.bind()
    matrix = bpy.context.region_data.perspective_matrix
    shader.uniform_float("u_ViewProjectionMatrix", matrix)
    shader.uniform_float("u_Scale", 10)
    batch.draw(shader)


bpy.types.SpaceView3D.draw_handler_add(draw, (), 'WINDOW', 'POST_VIEW')
