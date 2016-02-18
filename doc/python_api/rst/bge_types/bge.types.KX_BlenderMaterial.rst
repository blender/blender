KX_BlenderMaterial(PyObjectPlus)
================================

.. module:: bge.types

base class --- :class:`PyObjectPlus`

.. class:: KX_BlenderMaterial(PyObjectPlus)

   This is the interface to materials in the game engine.

   Materials define the render state to be applied to mesh objects.
   
   The example below shows a simple GLSL shader setup allowing to dynamically mix two texture channels
   in a material. All materials of the object executing this script should have two textures using
   separate UV maps in the two first texture channels.
   
   The code works for both Multitexture and GLSL rendering modes.

   .. code-block:: python

      from bge import logic
      
      vertex_shader = """
      
      void main(void)
      {
         // simple projection of the vertex position to view space
         gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
         // coordinate of the 1st texture channel
         gl_TexCoord[0] = gl_MultiTexCoord0;
         // coordinate of the 2nd texture channel
         gl_TexCoord[1] = gl_MultiTexCoord1;
      }
      """
      
      fragment_shader ="""

      uniform sampler2D texture_0;
      uniform sampler2D texture_1;
      uniform float factor;

      void main(void)
      {
         vec4 color_0 = texture2D(texture_0, gl_TexCoord[0].st);
         vec4 color_1 = texture2D(texture_1, gl_TexCoord[1].st);
         gl_FragColor = mix(color_0, color_1, factor);
      }
      """

      object = logic.getCurrentController().owner
      
      for mesh in object.meshes:
          for material in mesh.materials:
              shader = material.getShader()
              if shader is not None:
                  if not shader.isValid():
                      shader.setSource(vertex_shader, fragment_shader, True)

                  # get the first texture channel of the material
                  shader.setSampler('texture_0', 0)
                  # get the second texture channel of the material
                  shader.setSampler('texture_1', 1)
                  # pass another uniform to the shader
                  shader.setUniform1f('factor', 0.3)

   .. attribute:: shader

      The material's shader.

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

   .. method:: getTextureBindcode(textureslot)

      Returns the material's texture OpenGL bind code/id/number/name.

      :arg textureslot: Specifies the texture slot number
      :type textureslot: integer
      :return: the material's texture OpenGL bind code/id/number/name
      :rtype: integer

   .. attribute:: alpha

      The material's alpha transparency.

      :type: float between 0.0 and 1.0 inclusive

   .. attribute:: hardness

      How hard (sharp) the material's specular reflection is.

      :type: integer between 1 and 511 inclusive

   .. attribute:: emit

      Amount of light to emit.

      :type: float between 0.0 and 2.0 inclusive

   .. attribute:: specularIntensity

      How intense (bright) the material's specular reflection is.

      :type: float between 0.0 and 1.0 inclusive

   .. attribute:: diffuseIntensity

      The material's amount of diffuse reflection.

      :type: float between 0.0 and 1.0 inclusive

   .. attribute:: specularColor

      The material's specular color.

      :type: :class:`mathutils.Color`

   .. attribute:: diffuseColor

      The material's diffuse color.

      :type: :class:`mathutils.Color`

   .. method:: setBlending(src, dest)

      Set the pixel color arithmetic functions.

      :arg src: Specifies how the red, green, blue, and alpha source blending factors are computed, one of...
      
         * :data:`~bgl.GL_ZERO`
         * :data:`~bgl.GL_ONE`
         * :data:`~bgl.GL_SRC_COLOR`
         * :data:`~bgl.GL_ONE_MINUS_SRC_COLOR`
         * :data:`~bgl.GL_DST_COLOR`
         * :data:`~bgl.GL_ONE_MINUS_DST_COLOR`
         * :data:`~bgl.GL_SRC_ALPHA`
         * :data:`~bgl.GL_ONE_MINUS_SRC_ALPHA`
         * :data:`~bgl.GL_DST_ALPHA`
         * :data:`~bgl.GL_ONE_MINUS_DST_ALPHA`
         * :data:`~bgl.GL_SRC_ALPHA_SATURATE`
      
      :type src: int

      :arg dest: Specifies how the red, green, blue, and alpha destination blending factors are computed, one of...
      
         * :data:`~bgl.GL_ZERO`
         * :data:`~bgl.GL_ONE`
         * :data:`~bgl.GL_SRC_COLOR`
         * :data:`~bgl.GL_ONE_MINUS_SRC_COLOR`
         * :data:`~bgl.GL_DST_COLOR`
         * :data:`~bgl.GL_ONE_MINUS_DST_COLOR`
         * :data:`~bgl.GL_SRC_ALPHA`
         * :data:`~bgl.GL_ONE_MINUS_SRC_ALPHA`
         * :data:`~bgl.GL_DST_ALPHA`
         * :data:`~bgl.GL_ONE_MINUS_DST_ALPHA`
         * :data:`~bgl.GL_SRC_ALPHA_SATURATE`
      
      :type dest: int

   .. method:: getMaterialIndex()

      Returns the material's index.

      :return: the material's index
      :rtype: integer

