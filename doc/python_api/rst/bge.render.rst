
Rasterizer (bge.render)
=======================

*****
Intro
*****

.. module:: bge.render

Example of using a :class:`bge.types.SCA_MouseSensor`, and two :class:`bge.types.KX_ObjectActuator` to implement MouseLook:

.. note::
   This can also be achieved with the :class:`bge.types.KX_MouseActuator`.

.. code-block:: python

   # To use a mouse movement sensor "Mouse" and a
   # motion actuator to mouse look:
   import bge

   # scale sets the speed of motion
   scale = 1.0, 0.5

   co = bge.logic.getCurrentController()
   obj = co.owner
   mouse = co.sensors["Mouse"]
   lmotion = co.actuators["LMove"]
   wmotion = co.actuators["WMove"]

   # Transform the mouse coordinates to see how far the mouse has moved.
   def mousePos():
      x = (bge.render.getWindowWidth() / 2 - mouse.position[0]) * scale[0]
      y = (bge.render.getWindowHeight() / 2 - mouse.position[1]) * scale[1]
      return (x, y)

   pos = mousePos()

   # Set the amount of motion: X is applied in world coordinates...
   wmotion.useLocalTorque = False
   wmotion.torque = ((0.0, 0.0, pos[0]))

   # ...Y is applied in local coordinates
   lmotion.useLocalTorque = True
   lmotion.torque = ((-pos[1], 0.0, 0.0))

   # Activate both actuators
   co.activate(lmotion)
   co.activate(wmotion)

   # Centre the mouse
   bge.render.setMousePosition(int(bge.render.getWindowWidth() / 2), int(bge.render.getWindowHeight() / 2))

*********
Constants
*********

.. data:: KX_TEXFACE_MATERIAL

   Materials as defined by the texture face settings.

.. data:: KX_BLENDER_MULTITEX_MATERIAL

   Materials approximating blender materials with multitexturing.

.. data:: KX_BLENDER_GLSL_MATERIAL

   Materials approximating blender materials with GLSL.

.. DATA:: VSYNC_OFF

   Disables vsync

.. DATA:: VSYNC_ON

   Enables vsync

.. DATA:: VSYNC_ADAPTIVE

   Enables adaptive vsync if supported. Adaptive vsync enables vsync if the framerate is above the monitors refresh rate. Otherwise, vsync is diabled if the framerate is too low.

.. data:: LEFT_EYE

   Left eye being used during stereoscopic rendering.

.. data:: RIGHT_EYE

   Right eye being used during stereoscopic rendering.


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

.. function:: setFullScreen(enable)

   Set whether or not the window should be fullscreen.

   :type enable: bool

.. function:: getFullScreen()

   Returns whether or not the window is fullscreen.

   :rtype: bool

.. function:: getDisplayDimensions()

   Get the actual display dimensions, in pixels, of the physical display (e.g., the monitor).
   
   :type dimension: list [width,heigh] 

.. function:: makeScreenshot(filename)

   Writes an image file with the current displayed frame.

   The image is written to *'filename'*. The path may be absolute (eg. "/home/foo/image") or relative when started with
   "//" (eg. "//image"). Note that absolute paths are not portable between platforms.
   If the filename contains a "#", it will be replaced by an incremental index so that screenshots can be taken multiple
   times without overwriting the previous ones (eg. "image-#").

   Settings for the image are taken from the render settings (file format and respective settings, gamma and colospace
   conversion, etc). The image resolution matches the framebuffer, meaning, the window size and aspect ratio.
   When running from the standalone player, instead of the embedded player, only PNG files are supported. Additional
   color conversions are also not supported.

   :arg filename: path and name of the file to write
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

   Sets the window background color. (Deprecated: use KX_WorldInfo.background_color)

   :type rgba: list [r, g, b, a]


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

.. function:: getStereoEye()

   Gets the current stereoscopy eye being rendered.
   This function is mainly used in a :class:`bge.types.KX_Scene.pre_draw` callback
   function to customize the camera projection matrices for each
   stereoscopic eye.

   :rtype: LEFT_EYE, RIGHT_EYE

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

   .. note:: Changing this value can cause all textures to be recreated, which can be slow.

.. function:: getAnisotropicFiltering()

   Get the anisotropic filtering level used for textures.

   :rtype: integer (one of 1, 2, 4, 8, 16)

.. function:: setMipmapping(value)

   Change how to use mipmapping.

   :type value: RAS_MIPMAP_NONE, RAS_MIPMAP_NEAREST, RAS_MIPMAP_LINEAR

   .. note:: Changing this value can cause all textures to be recreated, which can be slow.

.. function:: getMipmapping()

   Get the current mipmapping setting.

   :rtype: RAS_MIPMAP_NONE, RAS_MIPMAP_NEAREST, RAS_MIPMAP_LINEAR

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

.. function:: showFramerate(enable)

   Show or hide the framerate.

   :type enable: boolean

.. function:: showProfile(enable)

   Show or hide the profile.

   :type enable: boolean

.. function:: showProperties(enable)

   Show or hide the debug properties.

   :type enable: boolean

.. function:: autoDebugList(enable)

   Enable or disable auto adding debug properties to the debug list.

   :type enable: boolean

.. function:: clearDebugList()

   Clears the debug property list.

.. function:: setVsync(value)

   Set the vsync value

   :arg value: One of VSYNC_OFF, VSYNC_ON, VSYNC_ADAPTIVE
   :type value: integer

.. function:: getVsync()

   Get the current vsync value

   :rtype: One of VSYNC_OFF, VSYNC_ON, VSYNC_ADAPTIVE
