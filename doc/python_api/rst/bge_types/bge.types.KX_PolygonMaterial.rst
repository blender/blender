KX_PolygonMaterial(PyObjectPlus)
================================

.. module:: bge.types

base class --- :class:`PyObjectPlus`

.. class:: KX_PolygonMaterial(PyObjectPlus)

   This is the interface to materials in the game engine.

   Materials define the render state to be applied to mesh objects.

   .. warning::

      Some of the methods/variables are CObjects.  If you mix these up, you will crash blender.

   .. code-block:: python

      from bge import logic
      
      vertex_shader = """
      
      void main(void)
      {
         // original vertex position, no changes
         gl_Position = ftransform();
         // coordinate of the 1st texture channel
         gl_TexCoord[0] = gl_MultiTexCoord0;
         // coordinate of the 2nd texture channel
         gl_TexCoord[1] = gl_MultiTexCoord1;
      }
      """
      
      fragment_shader ="""

      uniform sampler2D color_0;
      uniform sampler2D color_1;
      uniform float factor;

      void main(void)
      {
         vec4 color_0 = texture2D(color_0, gl_TexCoord[0].st);
         vec4 color_1 = texture2D(color_1, gl_TexCoord[1].st);
         gl_FragColor = mix(color_0, color_1, factor);
      }
      """

      object = logic.getCurrentController().owner
      object = cont.owner
      for mesh in object.meshes:
          for material in mesh.materials:
              shader = material.getShader()
              if shader != None:
                  if not shader.isValid():
                      shader.setSource(vertex_shader, fragment_shader, True)

                  # get the first texture channel of the material
                  shader.setSampler('color_0', 0)
                  # get the second texture channel of the material
                  shader.setSampler('color_1', 1)
                  # pass another uniform to the shader
                  shader.setUniform1f('factor', 0.3)


   .. attribute:: texture

      Texture name.

      :type: string (read-only)

   .. attribute:: gl_texture

      OpenGL texture handle (eg for glBindTexture(GL_TEXTURE_2D, gl_texture).

      :type: integer (read-only)

   .. attribute:: material

      Material name.

      :type: string (read-only)

   .. attribute:: tface

      Texture face properties.

      :type: CObject (read-only)

   .. attribute:: tile

      Texture is tiling.

      :type: boolean

   .. attribute:: tilexrep

      Number of tile repetitions in x direction.

      :type: integer

   .. attribute:: tileyrep

      Number of tile repetitions in y direction.

      :type: integer

   .. attribute:: drawingmode

      Drawing mode for the material.
      - 2  (drawingmode & 4)     Textured
      - 4  (drawingmode & 16)    Light
      - 14 (drawingmode & 16384) 3d Polygon Text.

      :type: bitfield

   .. attribute:: transparent

      This material is transparent. All meshes with this
      material will be rendered after non transparent meshes from back
      to front.

      :type: boolean

   .. attribute:: zsort

      Transparent polygons in meshes with this material will be sorted back to
      front before rendering.
      Non-Transparent polygons will be sorted front to back before rendering.

      :type: boolean

   .. attribute:: diffuse

      The diffuse color of the material. black = [0.0, 0.0, 0.0] white = [1.0, 1.0, 1.0].

      :type: list [r, g, b]

   .. attribute:: specular

      The specular color of the material. black = [0.0, 0.0, 0.0] white = [1.0, 1.0, 1.0].

      :type: list [r, g, b]

   .. attribute:: shininess

      The shininess (specular exponent) of the material. 0.0 <= shininess <= 128.0.

      :type: float

   .. attribute:: specularity

      The amount of specular of the material. 0.0 <= specularity <= 1.0.

      :type: float

   .. method:: updateTexture(tface, rasty)

      Updates a realtime animation.

      :arg tface: Texture face (eg mat.tface)
      :type tface: CObject
      :arg rasty: Rasterizer
      :type rasty: CObject

   .. method:: setTexture(tface)

      Sets texture render state.

      :arg tface: Texture face
      :type tface: CObject

      .. code-block:: python

         mat.setTexture(mat.tface)
         
   .. method:: activate(rasty, cachingInfo)

      Sets material parameters for this object for rendering.

      Material Parameters set:

      #. Texture
      #. Backface culling
      #. Line drawing
      #. Specular Colour
      #. Shininess
      #. Diffuse Colour
      #. Polygon Offset.

      :arg rasty: Rasterizer instance.
      :type rasty: CObject
      :arg cachingInfo: Material cache instance.
      :type cachingInfo: CObject

   .. method:: setCustomMaterial(material)

      Sets the material state setup object.

      Using this method, you can extend or completely replace the gameengine material
      to do your own advanced multipass effects.

      Use this method to register your material class.  Instead of the normal material, 
      your class's activate method will be called just before rendering the mesh.
      This should setup the texture, material, and any other state you would like.
      It should return True to render the mesh, or False if you are finished.  You should
      clean up any state Blender does not set before returning False.

      Activate Method Definition:

      .. code-block:: python
      
         def activate(self, rasty, cachingInfo, material):

      :arg material: The material object.
      :type material: instance

      .. code-block:: python

         class PyMaterial:
           def __init__(self):
             self.pass_no = -1
           
           def activate(self, rasty, cachingInfo, material):
             # Activate the material here.
             #
             # The activate method will be called until it returns False.
             # Every time the activate method returns True the mesh will
             # be rendered.
             #
             # rasty is a CObject for passing to material.updateTexture() 
             #       and material.activate()
             # cachingInfo is a CObject for passing to material.activate()
             # material is the KX_PolygonMaterial instance this material
             #          was added to
             
             # default material properties:
             self.pass_no += 1
             if self.pass_no == 0:
               material.activate(rasty, cachingInfo)
               # Return True to do this pass
               return True
             
             # clean up and return False to finish.
             self.pass_no = -1
             return False
         
         # Create a new Python Material and pass it to the renderer.
         mat.setCustomMaterial(PyMaterial())
         
