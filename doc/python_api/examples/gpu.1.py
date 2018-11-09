"""
Geometry Batches
++++++++++++++++

To draw geometry using the gpu module you need to create a :class:`gpu.types.GPUBatch` object.
Batches contain a sequence of points, lines or triangles and associated geometry attributes.

A batch can be drawn multiple times, so they should be cached whenever possible.
This makes them much faster than using the legacy `glBegin` and `glEnd` method, which would recreate the geometry data every time.

Every batch has a so called `Vertex Buffer`.
It contains the attributes for every vertex.
Typical attributes are `position`, `color` and `uv`.
Which attributes the vertex buffer of a batch contains, depends on the shader that will be used to process the batch. The best way to create a new batch is to use the :class:`gpu_extras.batch.batch_for_shader` function.

Furthermore, when creating a new batch, you have to specify the draw type.
The most used types are ``POINTS``, ``LINES`` and ``TRIS``.

Shaders
+++++++

A shader is a small program that tells the GPU how to draw batch geometry.
There are a couple of built-in shaders for the most common tasks.
Built-in shaders can be accessed with :class:`gpu.shader.from_builtin`.
Every built-in shader has an identifier (e.g. `2D_UNIFORM_COLOR` and `3D_FLAT_COLOR`).

Custom shaders can be used as well.
The :class:`gpu.types.GPUShader` takes the shader code as input and compiles it.
Every shader has at least a vertex and a fragment shader.
Optionally a geometry shader can be used as well.

.. note::
   A `GPUShader` is actually a `program` in OpenGL terminology.

Shaders define a set of `uniforms` and `attributes`.
**Uniforms** are properties that are constant for every vertex in a batch.
They have to be set before the batch but after the shader has been bound.
**Attributes** are properties that can be different for every vertex.

The attributes and uniforms used by built-in shaders are listed here: :class:`gpu.shader`

A batch can only be processed/drawn by a shader when it provides all the attributes that the shader specifies.

Vertex Buffers
++++++++++++++

A vertex buffer is an array that contains the attributes for every vertex.
To create a new vertex buffer (:class:`gpu.types.GPUVertBuf`) you have to provide two things: 1) the amount of vertices in the buffer and 2) the format of the buffer.

The format (:class:`gpu.types.GPUVertFormat`) describes which attributes are stored in the buffer.
E.g. to create a vertex buffer that contains 6 vertices, each with a position and a normal could look like so::

    import gpu
    vertex_positions = [(0, 0, 0), ...]
    vertex_normals = [(0, 0, 1), ...]

    fmt = gpu.types.GPUVertFormat()
    fmt.attr_add(id="pos", comp_type='F32', len=3, fetch_mode='FLOAT')
    fmt.attr_add(id="normal", comp_type='F32', len=3, fetch_mode='FLOAT')

    vbo = gpu.types.GPUVertBuf(len=6, format=fmt)
    vbo.attr_fill(id="pos", data=vertex_positions)
    vbo.attr_fill(id="normal", data=vertex_normals)

    batch = gpu.types.GPUBatch(type='TRIS', buf=vbo)

This batch contains two triangles now.
Vertices 0-2 describe the first and vertices 3-5 the second triangle.

.. note::
    The recommended way to create batches is to use the :class:`gpu_extras.batch.batch_for_shader` function. It makes sure that you provide all the vertex attributes that are necessary to be able to use a specific shader.

Index Buffers
+++++++++++++

The main reason why index buffers exist is to reduce the amount of memory required to store and send geometry.
E.g. often the same vertex is used by multiple triangles in a mesh.
Instead of vertex attributes multiple times to the gpu, an index buffer can be used.
An index buffer is an array of integers that describes in which order the vertex buffer should be read.
E.g. when you have a vertex buffer ``[a, b, c]`` and an index buffer ``[0, 2, 1, 2, 1, 0]`` it is like if you just had the vertex buffer ``[a, c, b, c, b, a]``.
Using an index buffer saves memory because usually a single integer is smaller than all attributes for one vertex combined.

Index buffers can be used like so::

    indices = [(0, 1), (2, 0), (2, 3), ...]
    ibo = gpu.types.GPUIndexBuf(type='LINES', seq=indices)
    batch = gpu.types.GPUBatch(type='LINES', buf=vbo, elem=ibo)

.. note::
    Instead of creating index buffers object manually, you can also just use the optional `indices` parameter of the :class:`gpu_extras.batch.batch_for_shader` function.

Offscreen Rendering
+++++++++++++++++++

Everytime something is drawn, the result is written into a framebuffer.
Usually this buffer will later be displayed on the screen.
However, sometimes you might want to draw into a separate "texture" and use it further.
E.g. you could use the render result as a texture on another object or save the rendered result on disk.
Offscreen Rendering is done using the :class:`gpu.types.GPUOffScreen` type.

.. warning::
    ``GPUOffScreen`` objects are bound to the opengl context they have been created in.
    This also means that once Blender discards this context (i.e. a window is closed) the offscreen instance will also be freed.


Examples
++++++++

To try these examples, just copy them into Blenders text editor and execute them.
To keep the examples relatively small, they just register a draw function that can't easily be removed anymore.
Blender has to be restarted in order to delete the draw handlers.
"""