"""
Triangle with Custom Shader
---------------------------
"""
import bpy
import gpu
from gpu_extras.batch import batch_for_shader

vertex_shader = '''
    uniform mat4 viewProjectionMatrix;

    in vec3 position;
    out vec3 pos;

    void main()
    {
        pos = position;
        gl_Position = viewProjectionMatrix * vec4(position, 1.0f);
    }
'''

fragment_shader = '''
    uniform float brightness;

    in vec3 pos;

    void main()
    {
        gl_FragColor = vec4(pos * brightness, 1.0);
    }
'''

coords = [(1, 1, 1), (2, 0, 0), (-2, -1, 3)]
shader = gpu.types.GPUShader(vertex_shader, fragment_shader)
batch = batch_for_shader(shader, 'TRIS', {"position": coords})


def draw():
    shader.bind()
    matrix = bpy.context.region_data.perspective_matrix
    shader.uniform_float("viewProjectionMatrix", matrix)
    shader.uniform_float("brightness", 0.5)
    batch.draw(shader)


bpy.types.SpaceView3D.draw_handler_add(draw, (), 'WINDOW', 'POST_VIEW')
