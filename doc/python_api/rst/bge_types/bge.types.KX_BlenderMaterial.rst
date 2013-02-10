KX_BlenderMaterial(PyObjectPlus)
================================

.. module:: bge.types

base class --- :class:`PyObjectPlus`

.. class:: KX_BlenderMaterial(PyObjectPlus)

   KX_BlenderMaterial

   .. attribute:: shader

      The materials shader.

      :type: :class:`BL_Shader`

   .. attribute:: blending

      Ints used for pixel blending, (src, dst), matching the setBlending method.

      :type: (integer, integer)

   .. attribute:: material_index

      The material's index.

      :type: integer

   .. method:: getShader()

      Returns the material's shader.

      :return: the material's shader
      :rtype: :class:`BL_Shader`

   .. method:: setBlending(src, dest)

      Set the pixel color arithmetic functions.

      :arg src: Specifies how the red, green, blue, and alpha source blending factors are computed.
      :type src: Value in...

         * GL_ZERO,
         * GL_ONE, 
         * GL_SRC_COLOR, 
         * GL_ONE_MINUS_SRC_COLOR, 
         * GL_DST_COLOR, 
         * GL_ONE_MINUS_DST_COLOR, 
         * GL_SRC_ALPHA, 
         * GL_ONE_MINUS_SRC_ALPHA, 
         * GL_DST_ALPHA, 
         * GL_ONE_MINUS_DST_ALPHA, 
         * GL_SRC_ALPHA_SATURATE

      :arg dest: Specifies how the red, green, blue, and alpha destination blending factors are computed.
      :type dest: Value in...

         * GL_ZERO
         * GL_ONE
         * GL_SRC_COLOR
         * GL_ONE_MINUS_SRC_COLOR
         * GL_DST_COLOR
         * GL_ONE_MINUS_DST_COLOR
         * GL_SRC_ALPHA
         * GL_ONE_MINUS_SRC_ALPHA
         * GL_DST_ALPHA
         * GL_ONE_MINUS_DST_ALPHA
         * GL_SRC_ALPHA_SATURATE

   .. method:: getMaterialIndex()

      Returns the material's index.

      :return: the material's index
      :rtype: integer

