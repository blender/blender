"""
Custom compute shader (using image store) and vertex/fragment shader
--------------------------------------------------------------------

This is an example of how to use a custom compute shader
to write to a texture and then use that texture in a vertex/fragment shader.
The expected result is a 2x2 plane (size of the default cube),
which changes color from a green-black gradient to a green-red gradient,
based on current time.
"""
import bpy
import gpu
from mathutils import Matrix
from gpu_extras.batch import batch_for_shader
import time

start_time = time.time()

size = 128
texture = gpu.types.GPUTexture((size, size), format='RGBA32F')

# Create the compute shader to write to the texture.
compute_shader_info = gpu.types.GPUShaderCreateInfo()
compute_shader_info.image(0, 'RGBA32F', "FLOAT_2D", "img_output", qualifiers={"WRITE"})
compute_shader_info.compute_source('''
void main()
{
  vec4 pixel = vec4(
    sin(time / 1.0),
    gl_GlobalInvocationID.y/128.0,
    0.0,
    1.0
  );
  imageStore(img_output, ivec2(gl_GlobalInvocationID.xy), pixel);
}''')
compute_shader_info.push_constant('FLOAT', "time")
compute_shader_info.local_group_size(1, 1)
compute_shader = gpu.shader.create_from_info(compute_shader_info)

# Create the shader to draw the texture.
vert_out = gpu.types.GPUStageInterfaceInfo("my_interface")
vert_out.smooth('VEC2', "uvInterp")
shader_info = gpu.types.GPUShaderCreateInfo()
shader_info.push_constant('MAT4', "viewProjectionMatrix")
shader_info.push_constant('MAT4', "modelMatrix")
shader_info.sampler(0, 'FLOAT_2D', "img_input")
shader_info.vertex_in(0, 'VEC2', "position")
shader_info.vertex_in(1, 'VEC2', "uv")
shader_info.vertex_out(vert_out)
shader_info.fragment_out(0, 'VEC4', "FragColor")

shader_info.vertex_source(
    "void main()"
    "{"
    "  uvInterp = uv;"
    "  gl_Position = viewProjectionMatrix * modelMatrix * vec4(position, 0.0, 1.0);"
    "}"
)

shader_info.fragment_source(
    "void main()"
    "{"
    "  FragColor = texture(img_input, uvInterp);"
    "}"
)

shader = gpu.shader.create_from_info(shader_info)

batch = batch_for_shader(
    shader, 'TRI_FAN',
    {
        "position": ((-1, -1), (1, -1), (1, 1), (-1, 1)),
        "uv": ((0, 0), (1, 0), (1, 1), (0, 1)),
    },
)


def draw():
    shader.uniform_float("modelMatrix", Matrix.Translation((0, 0, 0)) @ Matrix.Scale(1, 4))
    shader.uniform_float("viewProjectionMatrix", bpy.context.region_data.perspective_matrix)
    shader.uniform_sampler("img_input", texture)
    batch.draw(shader)
    compute_shader.image('img_output', texture)
    compute_shader.uniform_float("time", time.time() - start_time)
    gpu.compute.dispatch(compute_shader, 128, 128, 1)


def drawTimer():
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            area.tag_redraw()
    return 1.0 / 60.0


bpy.app.timers.register(drawTimer)
bpy.types.SpaceView3D.draw_handler_add(draw, (), 'WINDOW', 'POST_VIEW')
