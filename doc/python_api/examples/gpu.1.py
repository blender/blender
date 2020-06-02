"""
Geometry Batches
++++++++++++++++

Geometry is drawn in batches.
A batch contains the necessary data to perform the drawing.
That includes an obligatory *Vertex Buffer* and an optional *Index Buffer*,
each of which is described in more detail in the following sections.
A batch also defines a draw type.
Typical draw types are `POINTS`, `LINES` and `TRIS`.
The draw type determines how the data will be interpreted and drawn.

Vertex Buffers
++++++++++++++

A *Vertex Buffer Object* (VBO) (:class:`gpu.types.GPUVertBuf`)
is an array that contains the vertex attributes needed for drawing using a specific shader.
Typical vertex attributes are *location*, *normal*, *color*, and *uv*.
Every vertex buffer has a *Vertex Format* (:class:`gpu.types.GPUVertFormat`)
and a length corresponding to the number of vertices in the buffer.
A vertex format describes the attributes stored per vertex and their types.

The following code demonstrates the creation of a vertex buffer that contains 6 vertices.
For each vertex 2 attributes will be stored: The position and the normal.

.. code-block:: python

   import gpu
   vertex_positions = [(0, 0, 0), ...]
   vertex_normals = [(0, 0, 1), ...]

   fmt = gpu.types.GPUVertFormat()
   fmt.attr_add(id="pos", comp_type='F32', len=3, fetch_mode='FLOAT')
   fmt.attr_add(id="normal", comp_type='F32', len=3, fetch_mode='FLOAT')

   vbo = gpu.types.GPUVertBuf(len=6, format=fmt)
   vbo.attr_fill(id="pos", data=vertex_positions)
   vbo.attr_fill(id="normal", data=vertex_normals)

This vertex buffer could be used to draw 6 points, 3 separate lines, 5 consecutive lines, 2 separate triangles, ...
E.g. in the case of lines, each two consecutive vertices define a line.
The type that will actually be drawn is determined when the batch is created later.

Index Buffers
+++++++++++++

Often triangles and lines share one or more vertices.
With only a vertex buffer one would have to store all attributes for the these vertices multiple times.
This is very inefficient because in a connected triangle mesh every vertex is used 6 times on average.
A more efficient approach would be to use an *Index Buffer* (IBO) (:class:`gpu.types.GPUIndexBuf`),
sometimes referred to as *Element Buffer*.
An *Index Buffer* is an array that references vertices based on their index in the vertex buffer.

For instance, to draw a rectangle composed of two triangles, one could use an index buffer.

.. code-block:: python

   positions = (
       (-1,  1), (1,  1),
       (-1, -1), (1, -1))

   indices = ((0, 1, 2), (2, 1, 3))

   ibo = gpu.types.GPUIndexBuf(type='TRIS', seq=indices)

Here the first tuple in `indices` describes which vertices should be used for the first triangle
(same for the second tuple).
Note how the diagonal vertices 1 and 2 are shared between both triangles.

Shaders
+++++++

A shader is a program that runs on the GPU (written in GLSL in our case).
There are multiple types of shaders.
The most important ones are *Vertex Shaders* and *Fragment Shaders*.
Typically multiple shaders are linked together into a *Program*.
However, in the Blender Python API the term *Shader* refers to an OpenGL Program.
Every :class:`gpu.types.GPUShader` consists of a vertex shader, a fragment shader and an optional geometry shader.
For common drawing tasks there are some built-in shaders accessible from :class:`gpu.shader.from_builtin`
with an identifier such as `2D_UNIFORM_COLOR` or `3D_FLAT_COLOR`.

Every shader defines a set of attributes and uniforms that have to be set in order to use the shader.
Attributes are properties that are set using a vertex buffer and can be different for individual vertices.
Uniforms are properties that are constant per draw call.
They can be set using the `shader.uniform_*` functions after the shader has been bound.

Batch Creation
++++++++++++++

Batches can be creates by first manually creating VBOs and IBOs.
However, it is recommended to use the :class:`gpu_extras.batch.batch_for_shader` function.
It makes sure that all the vertex attributes necessary for a specific shader are provided.
Consequently, the shader has to be passed to the function as well.
When using this function one rarely has to care about the vertex format, VBOs and IBOs created in the background.
This is still something one should know when drawing stuff though.

Since batches can be drawn multiple times, they should be cached and reused whenever possible.

Offscreen Rendering
+++++++++++++++++++

What one can see on the screen after rendering is called the *Front Buffer*.
When draw calls are issued, batches are drawn on a *Back Buffer* that will only be displayed
when all drawing is done and the current back buffer will become the new front buffer.
Sometimes, one might want to draw the batches into a distinct buffer that could be used as
texture to display on another object or to be saved as image on disk.
This is called Offscreen Rendering.
In Blender Offscreen Rendering is done using the :class:`gpu.types.GPUOffScreen` type.

.. warning::

   `GPUOffScreen` objects are bound to the OpenGL context they have been created in.
   This means that once Blender discards this context (i.e. the window is closed),
   the offscreen instance will be freed.

Examples
++++++++

To try these examples, just copy them into Blenders text editor and execute them.
To keep the examples relatively small, they just register a draw function that can't easily be removed anymore.
Blender has to be restarted in order to delete the draw handlers.

3D Lines with Single Color
--------------------------
"""

import bpy
import gpu
from gpu_extras.batch import batch_for_shader

coords = [(1, 1, 1), (-2, 0, 0), (-2, -1, 3), (0, 1, 1)]
shader = gpu.shader.from_builtin('3D_UNIFORM_COLOR')
batch = batch_for_shader(shader, 'LINES', {"pos": coords})


def draw():
    shader.bind()
    shader.uniform_float("color", (1, 1, 0, 1))
    batch.draw(shader)


bpy.types.SpaceView3D.draw_handler_add(draw, (), 'WINDOW', 'POST_VIEW')
