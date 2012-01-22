
Rasterizer (bge.render)
=======================

*****
Intro
*****

.. module:: bge.render

.. code-block:: python

   # Example Uses an L{SCA_MouseSensor}, and two L{KX_ObjectActuator}s to implement MouseLook::
   # To use a mouse movement sensor "Mouse" and a 
   # motion actuator to mouse look:
   import bge.render
   import bge.logic

   # scale sets the speed of motion
   scale = 1.0, 0.5
   
   co = bge.logic.getCurrentController()
   obj = co.getOwner()
   mouse = co.getSensor("Mouse")
   lmotion = co.getActuator("LMove")
   wmotion = co.getActuator("WMove")
   
   # Transform the mouse coordinates to see how far the mouse has moved.
   def mousePos():
      x = (bge.render.getWindowWidth() / 2 - mouse.getXPosition()) * scale[0]
      y = (bge.render.getWindowHeight() / 2 - mouse.getYPosition()) * scale[1]
      return (x, y)
   
   pos = mousePos()
   
   # Set the amount of motion: X is applied in world coordinates...
   lmotion.setTorque(0.0, 0.0, pos[0], False)
   # ...Y is applied in local coordinates
   wmotion.setTorque(-pos[1], 0.0, 0.0, True)
   
   # Activate both actuators
   bge.logic.addActiveActuator(lmotion, True)
   bge.logic.addActiveActuator(wmotion, True)
   
   # Centre the mouse
   bge.render.setMousePosition(bge.render.getWindowWidth() / 2, bge.render.getWindowHeight() / 2)

*********
Constants
*********

.. data:: KX_TEXFACE_MATERIAL

   Materials as defined by the texture face settings.

.. data:: KX_BLENDER_MULTITEX_MATERIAL

   Materials approximating blender materials with multitexturing.

.. data:: KX_BLENDER_GLSL_MATERIAL

   Materials approximating blender materials with GLSL.

*********
Functions
*********

.. function:: getWindowWidth()

   Gets the width of the window (in pixels)
   
   :rtype: integer

.. function:: getWindowHeight()

   Gets the height of the window (in pixels)
   
   :rtype: integer

.. function:: setWindowSize(width, height)

   Set the width and height of the window (in pixels). This also works for fullscreen applications.
   
   :type width: integer
   :type height: integer


.. function:: makeScreenshot(filename)

   Writes a screenshot to the given filename.
   
   If filename starts with // the image will be saved relative to the current directory.
   If the filename contains # it will be replaced with the frame number.
   
   The standalone player saves .png files. It does not support colour space conversion 
   or gamma correction.
   
   When run from Blender, makeScreenshot supports Iris, IrisZ, TGA, Raw TGA, PNG, HamX, and Jpeg.
   Gamma, Colourspace conversion and Jpeg compression are taken from the Render settings panels.
   
   :type filename: string


.. function:: enableVisibility(visible)

   Doesn't really do anything...


.. function:: showMouse(visible)

   Enables or disables the operating system mouse cursor.
   
   :type visible: boolean


.. function:: setMousePosition(x, y)

   Sets the mouse cursor position.
   
   :type x: integer
   :type y: integer


.. function:: setBackgroundColor(rgba)

   Sets the window background colour.
   
   :type rgba: list [r, g, b, a]


.. function:: setMistColor(rgb)

   Sets the mist colour.
   
   :type rgb: list [r, g, b]

   
.. function:: setAmbientColor(rgb)

   Sets the color of ambient light.
   
   :type rgb: list [r, g, b]


.. function:: setMistStart(start)

   Sets the mist start value.  Objects further away than start will have mist applied to them.
   
   :type start: float


.. function:: setMistEnd(end)

   Sets the mist end value.  Objects further away from this will be coloured solid with
   the colour set by setMistColor().
   
   :type end: float

   
.. function:: disableMist()

   Disables mist.
   
   .. note:: Set any of the mist properties to enable mist.

   
.. function:: setEyeSeparation(eyesep)

   Sets the eye separation for stereo mode. Usually Focal Length/30 provides a confortable value.
   
   :arg eyesep: The distance between the left and right eye.
   :type eyesep: float


.. function:: getEyeSeparation()

   Gets the current eye separation for stereo mode.
   
   :rtype: float

   
.. function:: setFocalLength(focallength)

   Sets the focal length for stereo mode. It uses the current camera focal length as initial value.
   
   :arg focallength: The focal length.  
   :type focallength: float

.. function:: getFocalLength()

   Gets the current focal length for stereo mode.
   
   :rtype: float

.. function:: setMaterialMode(mode)

   Set the material mode to use for OpenGL rendering.
   
   :type mode: KX_TEXFACE_MATERIAL, KX_BLENDER_MULTITEX_MATERIAL, KX_BLENDER_GLSL_MATERIAL

   .. note:: Changes will only affect newly created scenes.


.. function:: getMaterialMode(mode)

   Get the material mode to use for OpenGL rendering.
   
   :rtype: KX_TEXFACE_MATERIAL, KX_BLENDER_MULTITEX_MATERIAL, KX_BLENDER_GLSL_MATERIAL


.. function:: setGLSLMaterialSetting(setting, enable)

   Enables or disables a GLSL material setting.
   
   :type setting: string (lights, shaders, shadows, ramps, nodes, extra_textures)
   :type enable: boolean


.. function:: getGLSLMaterialSetting(setting, enable)

   Get the state of a GLSL material setting.
   
   :type setting: string (lights, shaders, shadows, ramps, nodes, extra_textures)
   :rtype: boolean

.. function:: setAnisotropicFiltering(level)

   Set the anisotropic filtering level for textures.
   
   :arg level: The new anisotropic filtering level to use
   :type level: integer (must be one of 1, 2, 4, 8, 16)
   
.. function:: getAnisotropicFiltering()

   Get the anisotropic filtering level used for textures.
   
   :rtype: integer (one of 1, 2, 4, 8, 16)
   
.. function:: drawLine(fromVec,toVec,color)

   Draw a line in the 3D scene.
   
   :arg fromVec: the origin of the line
   :type fromVec: list [x, y, z]
   :arg toVec: the end of the line
   :type toVec: list [x, y, z]
   :arg color: the color of the line
   :type color: list [r, g, b]


.. function:: enableMotionBlur(factor)

   Enable the motion blur effect.
   
   :arg factor: the ammount of motion blur to display.
   :type factor: float [0.0 - 1.0]


.. function:: disableMotionBlur()

   Disable the motion blur effect.

