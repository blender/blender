*******************
GPU functions (gpu)
*******************

.. module:: gpu

This module provides access to materials GLSL shaders.

Submodules:

.. toctree::
   :maxdepth: 1

   gpu.offscreen.rst


Intro
=====

Module to provide functions concerning the GPU implementation in Blender, in particular
the GLSL shaders that blender generates automatically to render materials in the 3D view
and in the game engine.

.. warning::

   The API provided by this module is subject to change.
   The data exposed by the API are are closely related to Blender's internal GLSL code
   and may change if the GLSL code is modified (e.g. new uniform type).


Constants
=========

GLSL Data Type
--------------

.. _data-type:

Type of GLSL data.
For shader uniforms, the data type determines which ``glUniform`` function
variant to use to send the uniform value to the GPU.
For vertex attributes, the data type determines which ``glVertexAttrib`` function
variant to use to send the vertex attribute to the GPU.

See export_shader_

.. data:: GPU_DATA_1I

   one integer

.. data:: GPU_DATA_1F

   one float

.. data:: GPU_DATA_2F

   two floats

.. data:: GPU_DATA_3F

   three floats

.. data:: GPU_DATA_4F

   four floats

.. data:: GPU_DATA_9F

   matrix 3x3 in column-major order

.. data:: GPU_DATA_16F

   matrix 4x4 in column-major order

.. data:: GPU_DATA_4UB

   four unsigned byte


GLSL Uniform Types
------------------

.. _uniform-type:

Constants that specify the type of uniform used in a GLSL shader.
The uniform type determines the data type, origin and method
of calculation used by Blender to compute the uniform value.

The calculation of some of the uniforms is based on matrices available in the scene:

   .. _mat4_cam_to_world:
   .. _mat4_world_to_cam:

   ``mat4_cam_to_world``
     Model matrix of the camera. OpenGL 4x4 matrix that converts
     camera local coordinates to world coordinates. In blender this is obtained from the
     'matrix_world' attribute of the camera object.

     Some uniform will need the *mat4_world_to_cam*
     matrix computed as the inverse of this matrix.

   .. _mat4_object_to_world:
   .. _mat4_world_to_object:

   ``mat4_object_to_world``
     Model matrix of the object that is being rendered. OpenGL 4x4 matric that converts
     object local coordinates to world coordinates. In blender this is obtained from the
     'matrix_world' attribute of the object.

     Some uniform will need the *mat4_world_to_object* matrix, computed as the inverse of this matrix.

   .. _mat4_lamp_to_world:
   .. _mat4_world_to_lamp:

   ``mat4_lamp_to_world``
     Model matrix of the lamp lighting the object. OpenGL 4x4 matrix that converts lamp
     local coordinates to world coordinates. In blender this is obtained from the
     'matrix_world' attribute of the lamp object.

     Some uniform will need the *mat4_world_to_lamp* matrix
     computed as the inverse of this matrix.


.. note::

   Any uniforms used for view projections or transformations (object, lamp matrices for eg),
   can only be set once per frame.


GLSL Object Uniforms
^^^^^^^^^^^^^^^^^^^^

.. note::

   - Object transformations and color must be set before drawing the object.
   - There is at most one uniform of these types per shader.

.. data:: GPU_DYNAMIC_OBJECT_VIEWMAT

   A matrix that converts world coordinates to camera coordinates (see mat4_world_to_cam_).

   :type: matrix4x4

.. data:: GPU_DYNAMIC_OBJECT_MAT

   A matrix that converts object coordinates to world coordinates (see mat4_object_to_world_).

   :type: matrix4x4

.. data:: GPU_DYNAMIC_OBJECT_VIEWIMAT

   The uniform is a 4x4 GL matrix that converts coordinates
   in camera space to world coordinates (see mat4_cam_to_world_).

   :type: matrix4x4

.. data:: GPU_DYNAMIC_OBJECT_IMAT

   The uniform is a 4x4 GL matrix that converts world coodinates
   to object coordinates (see mat4_world_to_object_).

   :type: matrix4x4

.. data:: GPU_DYNAMIC_OBJECT_COLOR

   An RGB color + alpha defined at object level.
   Each values between 0.0 and 1.0.

   See :class:`bpy.types.Object.color`.

   :type: float4

.. data:: GPU_DYNAMIC_OBJECT_AUTOBUMPSCALE

   Multiplier for bump-map scaling.

   :type: float


GLSL Lamp Uniforms
^^^^^^^^^^^^^^^^^^

.. note::

   There is one uniform of that type per lamp lighting the material.

.. data:: GPU_DYNAMIC_LAMP_DYNVEC

   Represents the direction of light in camera space.

   Computed as:
      mat4_world_to_cam_ * (-vec3_lamp_Z_axis)

   .. note::

      - The lamp Z axis points to the opposite direction of light.
      - The norm of the vector should be unit length.

   :type: float3

.. data:: GPU_DYNAMIC_LAMP_DYNCO

   Represents the position of the light in camera space.

   Computed as:
      mat4_world_to_cam_ * vec3_lamp_pos

   :type: float3

.. data:: GPU_DYNAMIC_LAMP_DYNIMAT

   Matrix that converts vector in camera space to lamp space.

   Computed as:
      mat4_world_to_lamp_ * mat4_cam_to_world_

   :type: matrix4x4

.. data:: GPU_DYNAMIC_LAMP_DYNPERSMAT

   Matrix that converts a vector in camera space to shadow buffer depth space.

   Computed as:
      mat4_perspective_to_depth_ * mat4_lamp_to_perspective_ * mat4_world_to_lamp_ * mat4_cam_to_world_.

   .. _mat4_perspective_to_depth:

   ``mat4_perspective_to_depth`` is a fixed matrix defined as follow::

      0.5 0.0 0.0 0.5
      0.0 0.5 0.0 0.5
      0.0 0.0 0.5 0.5
      0.0 0.0 0.0 1.0

   .. note::

      - There is one uniform of that type per lamp casting shadow in the scene.

   :type: matrix4x4

.. data:: GPU_DYNAMIC_LAMP_DYNENERGY

   See :class:`bpy.types.Lamp.energy`.

   :type: float

.. data:: GPU_DYNAMIC_LAMP_DYNCOL

   See :class:`bpy.types.Lamp.color`.

   :type: float3

.. data:: GPU_DYNAMIC_LAMP_DISTANCE

   See :class:`bpy.types.Lamp.distance`.

   :type: float

.. data:: GPU_DYNAMIC_LAMP_ATT1

   See
   :class:`bpy.types.PointLamp.linear_attenuation`,
   :class:`bpy.types.SpotLamp.linear_attenuation`.

   :type: float

.. data:: GPU_DYNAMIC_LAMP_ATT2

   See
   :class:`bpy.types.PointLamp.quadratic_attenuation`,
   :class:`bpy.types.SpotLamp.quadratic_attenuation`.

   :type: float

.. data:: GPU_DYNAMIC_LAMP_SPOTSIZE

   See :class:`bpy.types.SpotLamp.spot_size`.

   :type: float

.. data:: GPU_DYNAMIC_LAMP_SPOTBLEND

   See :class:`bpy.types.SpotLamp.spot_blend`.

   :type: float

.. data:: GPU_DYNAMIC_LAMP_SPOTSCALE

   Represents the SpotLamp local scale.

   :type: float2


GLSL Sampler Uniforms
^^^^^^^^^^^^^^^^^^^^^

.. data:: GPU_DYNAMIC_SAMPLER_2DBUFFER

   Represents an internal texture used for certain effect
   (color band, etc).

   :type: integer

.. data:: GPU_DYNAMIC_SAMPLER_2DIMAGE

   Represents a texture loaded from an image file.

   :type: integer

.. data:: GPU_DYNAMIC_SAMPLER_2DSHADOW

   Represents a texture loaded from a shadow buffer file.

   :type: integer


GLSL Mist Uniforms
^^^^^^^^^^^^^^^^^^

.. data:: GPU_DYNAMIC_MIST_ENABLE:

   See :class:`bpy.types.WorldMistSettings.use_mist`.

   :type: float (0 or 1)

.. data:: GPU_DYNAMIC_MIST_START

   See :class:`bpy.types.WorldMistSettings.start`.

   :type: float

   See :class:`bpy.types.WorldMistSettings.depth`.

.. data:: GPU_DYNAMIC_MIST_DISTANCE

   :type: float

   See :class:`bpy.types.WorldMistSettings.intensity`.

.. data:: GPU_DYNAMIC_MIST_INTENSITY

   :type: float

.. data:: GPU_DYNAMIC_MIST_TYPE

   See :class:`bpy.types.WorldMistSettings.falloff`.

   :type: float (used as an index into the type)

.. data:: GPU_DYNAMIC_MIST_COLOR


GLSL World Uniforms
^^^^^^^^^^^^^^^^^^^

.. data:: GPU_DYNAMIC_HORIZON_COLOR

   See :class:`bpy.types.World.horizon_color`.

   :type: float3

.. data:: GPU_DYNAMIC_AMBIENT_COLOR

   See :class:`bpy.types.World.ambient_color`.

   :type: float3


GLSL Material Uniforms
^^^^^^^^^^^^^^^^^^^^^^

.. data:: GPU_DYNAMIC_MAT_DIFFRGB

   See :class:`bpy.types.Material.diffuse_color`.

   :type: float3

.. data:: GPU_DYNAMIC_MAT_REF

   See :class:`bpy.types.Material.diffuse_intensity`.

   :type: float

.. data:: GPU_DYNAMIC_MAT_SPECRGB

   See :class:`bpy.types.Material.specular_color`.

   :type: float3

.. data:: GPU_DYNAMIC_MAT_SPEC

   See :class:`bpy.types.Material.specular_intensity`.

   :type: float

.. data:: GPU_DYNAMIC_MAT_HARD

   See :class:`bpy.types.Material.specular_hardness`.

   :type: float

.. data:: GPU_DYNAMIC_MAT_EMIT

   See :class:`bpy.types.Material.emit`.

   :type: float

.. data:: GPU_DYNAMIC_MAT_AMB

   See :class:`bpy.types.Material.ambient`.

   :type: float

.. data:: GPU_DYNAMIC_MAT_ALPHA

   See :class:`bpy.types.Material.alpha`.

   :type: float



GLSL Attribute Type
-------------------

.. _attribute-type:

Type of the vertex attribute used in the GLSL shader. Determines the mesh custom data
layer that contains the vertex attribute.

.. data:: CD_MTFACE

   Vertex attribute is a UV Map. Data type is vector of 2 float.

   There can be more than one attribute of that type, they are differenciated by name.
   In blender, you can retrieve the attribute data with:

   .. code-block:: python

      mesh.uv_layers[attribute["name"]]

.. data:: CD_MCOL

   Vertex attribute is color layer. Data type is vector 4 unsigned byte (RGBA).

   There can be more than one attribute of that type, they are differenciated by name.
   In blender you can retrieve the attribute data with:

   .. code-block:: python

      mesh.vertex_colors[attribute["name"]]

.. data:: CD_ORCO

   Vertex attribute is original coordinates. Data type is vector 3 float.

   There can be only 1 attribute of that type per shader.
   In blender you can retrieve the attribute data with:

   .. code-block:: python

      mesh.vertices

.. data:: CD_TANGENT

   Vertex attribute is the tangent vector. Data type is vector 4 float.

   There can be only 1 attribute of that type per shader.
   There is currently no way to retrieve this attribute data via the RNA API but a standalone
   C function to compute the tangent layer from the other layers can be obtained from
   blender.org.


Functions
=========

.. _export_shader:

.. function:: export_shader(scene,material)

   Extracts the GLSL shader producing the visual effect of material in scene for the purpose of
   reusing the shader in an external engine.

   This function is meant to be used in material exporter
   so that the GLSL shader can be exported entirely.

   The return value is a dictionary containing the
   shader source code and all associated data.

   :arg scene: the scene in which the material in rendered.
   :type scene: :class:`bpy.types.Scene`
   :arg material: the material that you want to export the GLSL shader
   :type material: :class:`bpy.types.Material`
   :return: the shader source code and all associated data in a dictionary
   :rtype: dictionary

   The dictionary contains the following elements:

   - ``["fragment"]``: string
      fragment shader source code.

   - ``["vertex"]``: string
      vertex shader source code.

   - ``["uniforms"]``: sequence
      list of uniforms used in fragment shader, can be empty list. Each element of the
      sequence is a dictionary with the following elements:

      - ``["varname"]``: string
         name of the uniform in the fragment shader. Always of the form 'unf<number>'.

      - ``["datatype"]``: integer
         data type of the uniform variable. Can be one of the following:

         .. hlist::
            :columns: 2

            - :data:`gpu.GPU_DATA_1I` : use ``glUniform1i``
            - :data:`gpu.GPU_DATA_1F` : use ``glUniform1fv``
            - :data:`gpu.GPU_DATA_2F` : use ``glUniform2fv``
            - :data:`gpu.GPU_DATA_3F` : use ``glUniform3fv``
            - :data:`gpu.GPU_DATA_4F` : use ``glUniform4fv``
            - :data:`gpu.GPU_DATA_9F` : use ``glUniformMatrix3fv``
            - :data:`gpu.GPU_DATA_16F` : use ``glUniformMatrix4fv``

      - ``["type"]``: integer
         type of uniform, determines the origin and method of calculation. See uniform-type_.
         Depending on the type, more elements will be be present.

      - ``["lamp"]``: :class:`bpy.types.Object`
         Reference to the lamp object from which the uniforms value are extracted.
         Set for the following uniforms types:

         .. hlist::
            :columns: 2

            - :data:`gpu.GPU_DYNAMIC_LAMP_DYNVEC`
            - :data:`gpu.GPU_DYNAMIC_LAMP_DYNCO`
            - :data:`gpu.GPU_DYNAMIC_LAMP_DYNIMAT`
            - :data:`gpu.GPU_DYNAMIC_LAMP_DYNPERSMAT`
            - :data:`gpu.GPU_DYNAMIC_LAMP_DYNENERGY`
            - :data:`gpu.GPU_DYNAMIC_LAMP_DYNCOL`
            - :data:`gpu.GPU_DYNAMIC_SAMPLER_2DSHADOW`

         Notes:

         - The uniforms
           :data:`gpu.GPU_DYNAMIC_LAMP_DYNVEC`,
           :data:`gpu.GPU_DYNAMIC_LAMP_DYNCO`,
           :data:`gpu.GPU_DYNAMIC_LAMP_DYNIMAT` and
           :data:`gpu.GPU_DYNAMIC_LAMP_DYNPERSMAT`
           refer to the lamp object position and orientation,
           both of can be derived from the object world matrix:

            .. code-block:: python

               obmat = uniform["lamp"].matrix_world

            where obmat is the mat4_lamp_to_world_ matrix of the lamp as a 2 dimensional array,
            the lamp world location location is in ``obmat[3]``.

         - The uniform types
           :data:`gpu.GPU_DYNAMIC_LAMP_DYNENERGY` and
           :data:`gpu.GPU_DYNAMIC_LAMP_DYNCOL`
           refer to the lamp data bloc that you get from:

            .. code-block:: python

               la = uniform["lamp"].data

            from which you get ``lamp.energy`` and ``lamp.color``

         - Lamp duplication is not supported: if you have duplicated lamps in your scene
            (i.e. lamp that are instantiated by dupligroup, etc), this element will only
            give you a reference to the orignal lamp and you will not know which instance
            of the lamp it is refering too. You can still handle that case in the exporter
            by distributing the uniforms amongst the duplicated lamps.

      - ``["image"]``: :class:`bpy.types.Image`
         Reference to the image databloc.
         Set for uniform type
         :data:`gpu.GPU_DYNAMIC_SAMPLER_2DIMAGE`.
         You can get the image data from:

         .. code-block:: python

            # full path to image file
            uniform["image"].filepath
            # image size as a 2-dimensional array of int
            uniform["image"].size

      - ``["texnumber"]``: integer
         Channel number to which the texture is bound when drawing the object.
         Set for uniform types
         :data:`gpu.GPU_DYNAMIC_SAMPLER_2DBUFFER`,
         :data:`gpu.GPU_DYNAMIC_SAMPLER_2DIMAGE` and
         :data:`gpu.GPU_DYNAMIC_SAMPLER_2DSHADOW`.

         This is provided for information only: when reusing the shader outside blencer,
         you are free to assign the textures to the channel of your choice and to pass
         that number channel to the GPU in the uniform.

      - ``["texpixels"]``: byte array
         texture data for uniform type :data:`gpu.GPU_DYNAMIC_SAMPLER_2DBUFFER`.
         Although the corresponding uniform is a 2D sampler,
         the texture is always a 1D texture of n x 1 pixel.
         The texture size n is provided in ["texsize"] element.
         These texture are only used for computer generated texture (colorband, etc).
         The texture data is provided so that you can make a real image out of it in the exporter.

      - ``["texsize"]``: integer
         horizontal size of texture for uniform type :data:`gpu.GPU_DYNAMIC_SAMPLER_2DBUFFER`.
         The texture data is in ["texpixels"].

   - ``["attributes"]``: sequence
      list of attributes used in vertex shader, can be empty. Blender doesn't use
      standard attributes except for vertex position and normal. All other vertex
      attributes must be passed using the generic ``glVertexAttrib`` functions.
      The attribute data can be found in the derived mesh custom data using RNA.
      Each element of the sequence is a dictionary containing the following elements:

      - ``["varname"]``: string
         name of the uniform in the vertex shader. Always of the form 'att<number>'.

      - ``["datatype"]``: integer
         data type of vertex attribute, can be one of the following:

         - :data:`gpu.GPU_DATA_2F`: use ``glVertexAttrib2fv``
         - :data:`gpu.GPU_DATA_3F`: use ``glVertexAttrib3fv``
         - :data:`gpu.GPU_DATA_4F`: use ``glVertexAttrib4fv``
         - :data:`gpu.GPU_DATA_4UB`: use ``glVertexAttrib4ubv``

      - ``["number"]``: integer
         Generic attribute number. This is provided for information only.
         Blender doesn't use ``glBindAttribLocation`` to place generic attributes at specific location,
         it lets the shader compiler place the attributes automatically and query the
         placement with ``glGetAttribLocation``.
         The result of this placement is returned in this element.

         When using this shader in a render engine, you should either use
         ``glBindAttribLocation`` to force the attribute at this location or use
         ``glGetAttribLocation`` to get the placement chosen by the compiler of your GPU.

      - ``["type"]``: integer
         type of the mesh custom data from which the vertex attribute is loaded.
         See attribute-type_.

      - ``["name"]``: string or integer
         custom data layer name, used for attribute type :data:`gpu.CD_MTFACE` and :data:`gpu.CD_MCOL`.

   Example:

   .. code-block:: python

      import gpu
      # get GLSL shader of material Mat.001 in scene Scene.001
      scene = bpy.data.scenes["Scene.001"]
      material = bpy.data.materials["Mat.001"]
      shader = gpu.export_shader(scene,material)
      # scan the uniform list and find the images used in the shader
      for uniform in shader["uniforms"]:
          if uniform["type"] == gpu.GPU_DYNAMIC_SAMPLER_2DIMAGE:
              print("uniform {0} is using image {1}".format(uniform["varname"], uniform["image"].filepath))
      # scan the attribute list and find the UV Map used in the shader
      for attribute in shader["attributes"]:
          if attribute["type"] == gpu.CD_MTFACE:
              print("attribute {0} is using UV Map {1}".format(attribute["varname"], attribute["name"]))


Notes
=====

.. _mat4_lamp_to_perspective:

#. Calculation of the ``mat4_lamp_to_perspective`` matrix for a spot lamp.

   The following pseudo code shows how the ``mat4_lamp_to_perspective`` matrix is computed
   in blender for uniforms of :data:`gpu.GPU_DYNAMIC_LAMP_DYNPERSMAT` type:

   .. code-block:: python

      # Get the lamp datablock with:
      lamp = bpy.data.objects[uniform["lamp"]].data

      # Compute the projection matrix:
      #  You will need these lamp attributes:
      #  lamp.clipsta : near clip plane in world unit
      #  lamp.clipend : far clip plane in world unit
      #  lamp.spotsize : angle in degree of the spot light

      # The size of the projection plane is computed with the usual formula:
      wsize = lamp.clista * tan(lamp.spotsize/2)

      # And the projection matrix:
      mat4_lamp_to_perspective = glFrustum(-wsize, wsize, -wsize, wsize, lamp.clista, lamp.clipend)

#. Creation of the shadow map for a spot lamp.

   The shadow map is the depth buffer of a render performed by placing the camera at the
   spot light position. The size of the shadow map is given by the attribute ``lamp.bufsize``:
   shadow map size in pixel, same size in both dimensions.
