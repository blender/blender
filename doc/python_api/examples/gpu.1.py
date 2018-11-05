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
Which attributes the vertex buffer of a batch contains, depends on the shader that will be used to process the batch.

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

Examples
++++++++

To try these examples, just copy them into Blenders text editor and execute them.
To keep the examples relatively small, they just register a draw function that can't easily be removed anymore.
Blender has to be restarted in order to delete the draw handlers.
"""