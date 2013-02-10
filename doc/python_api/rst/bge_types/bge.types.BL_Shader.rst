BL_Shader(PyObjectPlus)
=======================

.. module:: bge.types

base class --- :class:`PyObjectPlus`

.. class:: BL_Shader(PyObjectPlus)

   BL_Shader GLSL shaders.

   TODO - Description

   .. method:: setUniformfv(name, fList)

      Set a uniform with a list of float values

      :arg name: the uniform name
      :type name: string
      :arg fList: a list (2, 3 or 4 elements) of float values
      :type fList: list[float]

   .. method:: delSource()

      Clear the shader. Use this method before the source is changed with :data:`setSource`.

   .. method:: getFragmentProg()

      Returns the fragment program.

      :return: The fragment program.
      :rtype: string

   .. method:: getVertexProg()

      Get the vertex program.

      :return: The vertex program.
      :rtype: string

   .. method:: isValid()

      Check if the shader is valid.

      :return: True if the shader is valid
      :rtype: boolean

   .. method:: setAttrib(enum)

      Set attribute location. (The parameter is ignored a.t.m. and the value of "tangent" is always used.)

      :arg enum: attribute location value
      :type enum: integer

   .. method:: setNumberOfPasses( max_pass )

      Set the maximum number of passes. Not used a.t.m.

      :arg max_pass: the maximum number of passes
      :type max_pass: integer

   .. method:: setSampler(name, index)

      Set uniform texture sample index.

      :arg name: Uniform name
      :type name: string
      :arg index: Texture sample index.
      :type index: integer

   .. method:: setSource(vertexProgram, fragmentProgram)

      Set the vertex and fragment programs

      :arg vertexProgram: Vertex program
      :type vertexProgram: string
      :arg fragmentProgram: Fragment program
      :type fragmentProgram: string

   .. method:: setUniform1f(name, fx)

      Set a uniform with 1 float value.

      :arg name: the uniform name
      :type name: string
      :arg fx: Uniform value
      :type fx: float

   .. method:: setUniform1i(name, ix)

      Set a uniform with an integer value.

      :arg name: the uniform name
      :type name: string
      :arg ix: the uniform value
      :type ix: integer

   .. method:: setUniform2f(name, fx, fy)

      Set a uniform with 2 float values

      :arg name: the uniform name
      :type name: string
      :arg fx: first float value
      :type fx: float

      :arg fy: second float value
      :type fy: float

   .. method:: setUniform2i(name, ix, iy)

      Set a uniform with 2 integer values

      :arg name: the uniform name
      :type name: string
      :arg ix: first integer value
      :type ix: integer
      :arg iy: second integer value
      :type iy: integer

   .. method:: setUniform3f(name, fx, fy, fz)

      Set a uniform with 3 float values.

      :arg name: the uniform name
      :type name: string
      :arg fx: first float value
      :type fx: float
      :arg fy: second float value
      :type fy: float
      :arg fz: third float value
      :type fz: float

   .. method:: setUniform3i(name, ix, iy, iz)

      Set a uniform with 3 integer values

      :arg name: the uniform name
      :type name: string
      :arg ix: first integer value
      :type ix: integer
      :arg iy: second integer value
      :type iy: integer
      :arg iz: third integer value
      :type iz: integer

   .. method:: setUniform4f(name, fx, fy, fz, fw)

      Set a uniform with 4 float values.

      :arg name: the uniform name
      :type name: string
      :arg fx: first float value
      :type fx: float
      :arg fy: second float value
      :type fy: float
      :arg fz: third float value
      :type fz: float
      :arg fw: fourth float value
      :type fw: float

   .. method:: setUniform4i(name, ix, iy, iz, iw)

      Set a uniform with 4 integer values

      :arg name: the uniform name
      :type name: string
      :arg ix: first integer value
      :type ix: integer
      :arg iy: second integer value
      :type iy: integer
      :arg iz: third integer value
      :type iz: integer
      :arg iw: fourth integer value
      :type iw: integer

   .. method:: setUniformDef(name, type)

      Define a new uniform

      :arg name: the uniform name
      :type name: string
      :arg type: uniform type
      :type type: UNI_NONE, UNI_INT, UNI_FLOAT, UNI_INT2, UNI_FLOAT2, UNI_INT3, UNI_FLOAT3, UNI_INT4, UNI_FLOAT4, UNI_MAT3, UNI_MAT4, UNI_MAX

   .. method:: setUniformMatrix3(name, mat, transpose)

      Set a uniform with a 3x3 matrix value

      :arg name: the uniform name
      :type name: string
      :arg mat: A 3x3 matrix [[f, f, f], [f, f, f], [f, f, f]]
      :type mat: 3x3 matrix
      :arg transpose: set to True to transpose the matrix
      :type transpose: boolean

   .. method:: setUniformMatrix4(name, mat, transpose)

      Set a uniform with a 4x4 matrix value

      :arg name: the uniform name
      :type name: string
      :arg mat: A 4x4 matrix [[f, f, f, f], [f, f, f, f], [f, f, f, f], [f, f, f, f]]
      :type mat: 4x4 matrix
      :arg transpose: set to True to transpose the matrix
      :type transpose: boolean

   .. method:: setUniformiv(name, iList)

      Set a uniform with a list of integer values

      :arg name: the uniform name
      :type name: string
      :arg iList: a list (2, 3 or 4 elements) of integer values
      :type iList: list[integer]

   .. method:: validate()

      Validate the shader object.

