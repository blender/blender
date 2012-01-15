
Video Texture (bge.texture)
===========================

*****
Intro
*****

The bge.texture module allows you to manipulate textures during the game.

Several sources for texture are possible: video files, image files, video capture, memory buffer, camera render or a mix of that.

The video and image files can be loaded from the internet using an URL instead of a file name.

In addition, you can apply filters on the images before sending them to the GPU, allowing video effect: blue screen, color band, gray, normal map.

bge.texture uses FFmpeg to load images and videos. All the formats and codecs that FFmpeg supports are supported by this module, including but not limited to::

* AVI
* Ogg
* Xvid
* Theora
* dv1394 camera
* video4linux capture card (this includes many webcams)
* videoForWindows capture card (this includes many webcams)
* JPG

The principle is simple: first you identify a texture on an existing object using
the :materialID: function, then you create a new texture with dynamic content
and swap the two textures in the GPU.

The GE is not aware of the substitution and continues to display the object as always,
except that you are now in control of the texture.

When the texture object is deleted, the new texture is deleted and the old texture restored.

.. module:: bge.texture

.. literalinclude:: ../examples/bge.texture.py

.. literalinclude:: ../examples/bge.texture.1.py

.. class:: VideoFFmpeg(file [, capture=-1, rate=25.0, width=0, height=0])

   FFmpeg video source

   .. attribute:: status

      video status

   .. attribute:: range

      replay range

   .. attribute:: repeat

      repeat count, -1 for infinite repeat

      :type: int

   .. attribute:: framerate

      frame rate

      :type: float

   .. attribute:: valid

      Tells if an image is available

      :type: bool

   .. attribute:: image

      image data

   .. attribute:: size

      image size

   .. attribute:: scale

      fast scale of image (near neighbour)

   .. attribute:: flip

      flip image vertically

   .. attribute:: filter

      pixel filter

   .. attribute:: preseek

      number of frames of preseek

      :type: int

   .. attribute:: deinterlace

      deinterlace image

      :type: bool

   .. method:: play()

      Play (restart) video

   .. method:: pause()

      pause video

   .. method:: stop()

      stop video (play will replay it from start)

   .. method:: refresh()

      Refresh video - get its status

.. class:: ImageFFmpeg(file)

   FFmpeg image source

   .. attribute:: status

      video status

   .. attribute:: valid

      Tells if an image is available

      :type: bool

   .. attribute:: image

      image data

   .. attribute:: size

      image size

   .. attribute:: scale

      fast scale of image (near neighbour)

   .. attribute:: flip

      flip image vertically

   .. attribute:: filter

      pixel filter

   .. method:: refresh()

      Refresh image, i.e. load it

   .. method:: reload([newname])

      Reload image, i.e. reopen it

.. class:: ImageBuff()

   Image source from image buffer

   .. attribute:: filter

      pixel filter

   .. attribute:: flip

      flip image vertically

   .. attribute:: image

      image data

   .. method:: load(imageBuffer, width, height)

      Load image from buffer

   .. method:: plot(imageBuffer, width, height, positionX, positionY)

      update image buffer

   .. attribute:: scale

      fast scale of image (near neighbour)

   .. attribute:: size

      image size

   .. attribute:: valid

      bool to tell if an image is available

.. class:: ImageMirror(scene)

   Image source from mirror

   .. attribute:: alpha

      use alpha in texture

   .. attribute:: background

      background color

   .. attribute:: capsize

      size of render area

   .. attribute:: clip

      clipping distance

   .. attribute:: filter

      pixel filter

   .. attribute:: flip

      flip image vertically

   .. attribute:: image

      image data

   .. method:: refresh(imageMirror)

      Refresh image - invalidate its current content

   .. attribute:: scale

      fast scale of image (near neighbour)

   .. attribute:: size

      image size

   .. attribute:: valid

      bool to tell if an image is available

   .. attribute:: whole

      use whole viewport to render

.. class:: ImageMix()

   Image mixer

   .. attribute:: filter

      pixel filter

   .. attribute:: flip

      flip image vertically

   .. method:: getSource(imageMix)

      get image source

   .. method:: getWeight(imageMix)

      get image source weight


   .. attribute:: image

      image data

   .. method:: refresh(imageMix)

      Refresh image - invalidate its current content

   .. attribute:: scale

      fast scale of image (near neighbour)

   .. method:: setSource(imageMix)

      set image source

   .. method:: setWeight(imageMix)

      set image source weight

   .. attribute:: valid

      bool to tell if an image is available

.. class:: ImageRender(scene, camera)

   Image source from render

   .. attribute:: alpha

      use alpha in texture

   .. attribute:: background

      background color

   .. attribute:: capsize

      size of render area

   .. attribute:: filter

      pixel filter

   .. attribute:: flip

      flip image vertically

   .. attribute:: image

      image data

   .. method:: refresh(imageRender)

      Refresh image - invalidate its current content

   .. attribute:: scale

      fast scale of image (near neighbour)

   .. attribute:: size

      image size

   .. attribute:: valid

      bool to tell if an image is available

   .. attribute:: whole

      use whole viewport to render

.. class:: ImageViewport()

   Image source from viewport

   .. attribute:: alpha

      use alpha in texture

   .. attribute:: capsize

      size of viewport area being captured

   .. attribute:: filter

      pixel filter

   .. attribute:: flip

      flip image vertically

   .. attribute:: image

      image data

   .. attribute:: position

      upper left corner of captured area

   .. method:: refresh(imageViewport)

      Refresh image - invalidate its current content

   .. attribute:: scale

      fast scale of image (near neighbour)

   .. attribute:: size

      image size

   .. attribute:: valid

      bool to tell if an image is available

   .. attribute:: whole

      use whole viewport to capture

.. class:: Texture(gameObj)

   Texture objects

   .. attribute:: bindId

      OpenGL Bind Name

   .. method:: close(texture)

      Close dynamic texture and restore original

   .. attribute:: mipmap

      mipmap texture

   .. method:: refresh(texture)

      Refresh texture from source

   .. attribute:: source

      source of texture

.. class:: FilterBGR24()

   Source filter BGR24 objects

.. class:: FilterBlueScreen()

   Filter for Blue Screen objects

   .. attribute:: color

      blue screen color

   .. attribute:: limits

      blue screen color limits

   .. attribute:: previous

      previous pixel filter

.. class:: FilterColor()

   Filter for color calculations

   .. attribute:: matrix

      matrix [4][5] for color calculation

   .. attribute:: previous

      previous pixel filter

.. class:: FilterGray()

   Filter for gray scale effect

   .. attribute:: previous

      previous pixel filter

.. class:: FilterLevel()

   Filter for levels calculations

   .. attribute:: levels

      levels matrix [4] (min, max)

   .. attribute:: previous

      previous pixel filter

.. class:: FilterNormal()

   Filter for Blue Screen objects

   .. attribute:: colorIdx

      index of color used to calculate normal (0 - red, 1 - green, 2 - blue)

   .. attribute:: depth

      depth of relief

   .. attribute:: previous

      previous pixel filter

.. class:: FilterRGB24()

   Returns a new input filter object to be used with :class:`ImageBuff` object when the image passed
   to the ImageBuff.load() function has the 3-bytes pixel format BGR.

.. class:: FilterRGBA32()

   Source filter RGBA32 objects

.. function:: getLastError()

   Last error that occurred in a bge.texture function.

   :return: the description of the last error occurred in a bge.texture function.
   :rtype: string

.. function:: imageToArray(image,mode)

   Returns a :class:`~bgl.buffer` corresponding to the current image stored in a texture source object.

   :arg image: Image source object.
   :type image: object of type :class:`VideoFFmpeg`, :class:`ImageFFmpeg`, :class:`ImageBuff`, :class:`ImageMix`, :class:`ImageRender`, :class:`ImageMirror` or :class:`ImageViewport`
   :arg mode: optional argument representing the pixel format.
      You can use the characters R, G, B for the 3 color channels, A for the alpha channel,
      0 to force a fixed 0 color channel and 1 to force a fixed 255 color channel.
      Example: "BGR" will return 3 bytes per pixel with the Blue, Green and Red channels in that order.
      "RGB1" will return 4 bytes per pixel with the Red, Green, Blue channels in that order and the alpha channel forced to 255.
      The default mode is "RGBA".

   :type mode: string
   :rtype: :class:`~bgl.buffer`
   :return: A object representing the image as one dimensional array of bytes of size (pixel_size*width*height),
      line by line starting from the bottom of the image. The pixel size and format is determined by the mode
      parameter.

.. function:: materialID(object,name)

   Returns a numeric value that can be used in :class:`Texture` to create a dynamic texture.

   The value corresponds to an internal material number that uses the texture identified
   by name. name is a string representing a texture name with IM prefix if you want to
   identify the texture directly.    This method works for basic tex face and for material,
   provided the material has a texture channel using that particular texture in first
   position of the texture stack.    name can also have MA prefix if you want to identify
   the texture by material. In that case the material must have a texture channel in first
   position.

   If the object has no material that matches name, it generates a runtime error. Use try/except to catch the exception.

   Ex: bge.texture.materialID(obj, 'IMvideo.png')

   :arg object: the game object that uses the texture you want to make dynamic
   :type object: game object
   :arg name: name of the texture/material you want to make dynamic.
   :type name: string
   :rtype: integer

.. function:: setLogFile(filename)

   Sets the name of a text file in which runtime error messages will be written, in addition to the printing
   of the messages on the Python console. Only the runtime errors specific to the VideoTexture module
   are written in that file, ordinary runtime time errors are not written.

   :arg filename: name of error log file
   :type filename: string
   :rtype: integer
