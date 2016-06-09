
Video Texture (bge.texture)
===========================

************
Introduction
************

The bge.texture module allows you to manipulate textures during the game.

Several sources for texture are possible: video files, image files, video capture, memory buffer, camera render or a mix of that.

The video and image files can be loaded from the internet using an URL instead of a file name.

In addition, you can apply filters on the images before sending them to the GPU, allowing video effect: blue screen, color band, gray, normal map.

bge.texture uses FFmpeg to load images and videos. All the formats and codecs that FFmpeg supports are supported by this module, including but not limited to:

* AVI
* Ogg
* Xvid
* Theora
* dv1394 camera
* video4linux capture card (this includes many webcams)
* videoForWindows capture card (this includes many webcams)
* JPG

The principle is simple: first you identify a texture on an existing object using
the :class:`~bge.texture.materialID` function, then you create a new texture with dynamic content
and swap the two textures in the GPU.

The GE is not aware of the substitution and continues to display the object as always,
except that you are now in control of the texture.

When the texture object is deleted, the new texture is deleted and the old texture restored.

.. module:: bge.texture

.. include:: ../examples/bge.texture.py
   :start-line: 1
   :end-line: 5
   
.. literalinclude:: ../examples/bge.texture.py
   :lines: 7-

.. include:: ../examples/bge.texture.1.py
   :start-line: 1
   :end-line: 6
   
.. literalinclude:: ../examples/bge.texture.1.py
   :lines: 8-
   
   
*************
Video classes
*************

.. class:: VideoFFmpeg(file, capture=-1, rate=25.0, width=0, height=0)

   FFmpeg video source.
      
   :arg file: Path to the video to load; if capture >= 0 on Windows, this parameter will not be used.
   :type file: str
   :arg capture: Capture device number; if >= 0, the corresponding webcam will be used. (optional)
   :type capture: int
   :arg rate: Capture rate. (optional, used only if capture >= 0)
   :type rate: float
   :arg width: Capture width. (optional, used only if capture >= 0)
   :type width: int
   :arg height: Capture height. (optional, used only if capture >= 0)
   :type height: int

   .. attribute:: status

      Video status. (readonly)
      
      :type: int
      :value: see `FFmpeg Video and Image Status`_.

   .. attribute:: range

      Replay range.
      
      :type: sequence of two floats

   .. attribute:: repeat

      Repeat count, -1 for infinite repeat.

      :type: int

   .. attribute:: framerate

      Frame rate.

      :type: float

   .. attribute:: valid

      Tells if an image is available. (readonly)

      :type: bool

   .. attribute:: image

      Image data. (readonly)
      
      :type: :class:`~bgl.Buffer` or None

   .. attribute:: size

      Image size. (readonly)
      
      :type: tuple of two ints

   .. attribute:: scale

      Fast scale of image (near neighbour).
      
      :type: bool

   .. attribute:: flip

      Flip image vertically.
      
      :type: bool

   .. attribute:: filter

      Pixel filter.
      
      :type: one of...
      
         * :class:`FilterBGR24`
         * :class:`FilterBlueScreen`
         * :class:`FilterColor`
         * :class:`FilterGray`
         * :class:`FilterLevel`
         * :class:`FilterNormal`
         * :class:`FilterRGB24`
         * :class:`FilterRGBA32`

   .. attribute:: preseek

      Number of frames of preseek.

      :type: int

   .. attribute:: deinterlace

      Deinterlace image.

      :type: bool

   .. method:: play()

      Play (restart) video.
      
      :return: Whether the video was ready or stopped.
      :rtype: bool

   .. method:: pause()

      Pause video.
      
      :return: Whether the video was playing.
      :rtype: bool

   .. method:: stop()

      Stop video (play will replay it from start).
      
      :return: Whether the video was playing.
      :rtype: bool

   .. method:: refresh(buffer=None, format="RGBA", timestamp=-1.0)

      Refresh video - get its status and optionally copy the frame to an external buffer.

      :arg buffer: An optional object that implements the buffer protocol.
         If specified, the image is copied to the buffer, which must be big enough or an exception is thrown.
      :type buffer: any buffer type
      :arg format: An optional image format specifier for the image that will be copied to the buffer.
         Only valid values are "RGBA" or "BGRA"
      :type format: str
      :arg timestamp: An optional timestamp (in seconds from the start of the movie)
         of the frame to be copied to the buffer.
      :type timestamp: float
      :return: see `FFmpeg Video and Image Status`_.
      :rtype: int


*************
Image classes
*************

.. class:: ImageFFmpeg(file)

   FFmpeg image source.
      
   :arg file: Path to the image to load.
   :type file: str

   .. attribute:: status

      Image status. (readonly)
      
      :type: int
      :value: see `FFmpeg Video and Image Status`_.

   .. attribute:: valid

      Tells if an image is available. (readonly)

      :type: bool

   .. attribute:: image

      Image data. (readonly)
      
      :type: :class:`~bgl.Buffer` or None

   .. attribute:: size

      Image size. (readonly)
      
      :type: tuple of two ints

   .. attribute:: scale

      Fast scale of image (near neighbour).
      
      :type: bool

   .. attribute:: flip

      Flip image vertically.
      
      :type: bool

   .. attribute:: filter

      Pixel filter.
      
      :type: one of...
      
         * :class:`FilterBGR24`
         * :class:`FilterBlueScreen`
         * :class:`FilterColor`
         * :class:`FilterGray`
         * :class:`FilterLevel`
         * :class:`FilterNormal`
         * :class:`FilterRGB24`
         * :class:`FilterRGBA32`

   .. method:: refresh(buffer=None, format="RGBA")

      Refresh image, get its status and optionally copy the frame to an external buffer.
      
      :arg buffer: An optional object that implements the buffer protocol.
         If specified, the image is copied to the buffer, which must be big enough or an exception is thrown.
      :type buffer: any buffer type
      :arg format: An optional image format specifier for the image that will be copied to the buffer.
         Only valid values are "RGBA" or "BGRA"
      :type format: str
      :return: see `FFmpeg Video and Image Status`_.
      :rtype: int

   .. method:: reload(newname=None)

      Reload image, i.e. reopen it.
      
      :arg newname: Path to a new image. (optional)
      :type newname: str

.. class:: ImageBuff(width, height, color=0, scale=False)

   Image source from image buffer.
   
   :arg width: Width of the image.
   :type width: int
   :arg height: Height of the image.
   :type height: int
   :arg color: Value to initialize RGB channels with. The initialized buffer will have
               all pixels set to (color, color, color, 255). (optional)
   :type color: int in [0, 255]
   :arg scale: Image uses scaling. (optional)
   :type scale: bool

   .. attribute:: filter

      Pixel filter.
      
      :type: one of...
      
         * :class:`FilterBGR24`
         * :class:`FilterBlueScreen`
         * :class:`FilterColor`
         * :class:`FilterGray`
         * :class:`FilterLevel`
         * :class:`FilterNormal`
         * :class:`FilterRGB24`
         * :class:`FilterRGBA32`

   .. attribute:: flip

      Flip image vertically.
      
      :type: bool

   .. attribute:: image

      Image data. (readonly)
      
      :type: :class:`~bgl.Buffer` or None

   .. method:: load(imageBuffer, width, height)

      Load image from buffer.
      
      :arg imageBuffer: Buffer to load the image from.
      :type imageBuffer: :class:`~bgl.Buffer` or Python object implementing the buffer protocol (f.ex. bytes)
      :arg width: Width of the image to load.
      :type width: int
      :arg height: Height of the image to load.
      :type height: int

   .. method:: plot(imageBuffer, width, height, positionX, positionY, mode=IMB_BLEND_COPY)

      Update image buffer.
      
      :arg imageBuffer: Buffer to load the new data from.
      :type imageBuffer: :class:`~bgl.Buffer`, :class:`ImageBuff` or Python object implementing the buffer protocol (f.ex. bytes)
      :arg width: Width of the data to load.
      :type width: int
      :arg height: Height of the data to load.
      :type height: int
      :arg positionX: Left boundary of the region to be drawn on.
      :type positionX: int
      :arg positionY: Upper boundary of the region to be drawn on.
      :type positionY: int
      :arg mode: Drawing mode, see `Image Blending Modes`_.
      :type mode: int
      

   .. attribute:: scale

      Fast scale of image (near neighbour).
      
      :type: bool

   .. attribute:: size

      Image size. (readonly)
      
      :type: tuple of two ints

   .. attribute:: valid

      Tells if an image is available. (readonly)

      :type: bool

.. class:: ImageMirror(scene, observer, mirror, material=0)

   Image source from mirror.
   
   :arg scene: Scene in which the image has to be taken.
   :type scene: :class:`~bge.types.KX_Scene`
   :arg observer: Reference object for the mirror (the object from which the mirror has to be looked at, for example a camera).
   :type observer: :class:`~bge.types.KX_GameObject`
   :arg mirror: Object holding the mirror.
   :type mirror: :class:`~bge.types.KX_GameObject`
   :arg material: ID of the mirror's material to be used for mirroring. (optional)
   :type material: int

   .. attribute:: alpha

      Use alpha in texture.
      
      :type: bool

   .. attribute:: background

      Background color.
      
      :type: int or float list [r, g, b, a] in [0.0, 255.0]

   .. attribute:: capsize

      Size of render area.
      
      :type: sequence of two ints

   .. attribute:: clip

      Clipping distance.
      
      :type: float in [0.01, 5000.0]

   .. attribute:: filter

      Pixel filter.
      
      :type: one of...
      
         * :class:`FilterBGR24`
         * :class:`FilterBlueScreen`
         * :class:`FilterColor`
         * :class:`FilterGray`
         * :class:`FilterLevel`
         * :class:`FilterNormal`
         * :class:`FilterRGB24`
         * :class:`FilterRGBA32`

   .. attribute:: flip

      Flip image vertically.
      
      :type: bool

   .. attribute:: image

      Image data. (readonly)
      
      :type: :class:`~bgl.Buffer` or None

   .. method:: refresh(buffer=None, format="RGBA")

      Refresh image - render and copy the image to an external buffer (optional)
      then invalidate its current content.

      :arg buffer: An optional object that implements the buffer protocol.
         If specified, the image is rendered and copied to the buffer,
         which must be big enough or an exception is thrown.
      :type buffer: any buffer type
      :arg format: An optional image format specifier for the image that will be copied to the buffer.
         Only valid values are "RGBA" or "BGRA"
      :type format: str

   .. attribute:: scale

      Fast scale of image (near neighbour).
      
      :type: bool

   .. attribute:: size

      Image size (readonly).
      
      :type: tuple of two ints

   .. attribute:: valid

      Tells if an image is available. (readonly)

      :type: bool

   .. attribute:: whole

      Use whole viewport to render.
      
      :type: bool

.. class:: ImageMix

   Image mixer.

   .. attribute:: filter

      Pixel filter.
      
      :type: one of...
      
         * :class:`FilterBGR24`
         * :class:`FilterBlueScreen`
         * :class:`FilterColor`
         * :class:`FilterGray`
         * :class:`FilterLevel`
         * :class:`FilterNormal`
         * :class:`FilterRGB24`
         * :class:`FilterRGBA32`

   .. attribute:: flip

      Flip image vertically.
      
      :type: bool

   .. method:: getSource(id)

      Get image source.
      
      :arg id: Identifier of the source to get.
      :type id: str
      
      :return: Image source.
      :rtype: one of...
      
         * :class:`VideoFFmpeg`
         * :class:`ImageFFmpeg`
         * :class:`ImageBuff`
         * :class:`ImageMirror`
         * :class:`ImageMix`
         * :class:`ImageRender`
         * :class:`ImageViewport`

   .. method:: getWeight(id)

      Get image source weight.
      
      :arg id: Identifier of the source.
      :type id: str
      
      :return: Weight of the source.
      :rtype: int

   .. attribute:: image

      Image data. (readonly)
      
      :type: :class:`~bgl.Buffer` or None

   .. method:: refresh(buffer=None, format="RGBA")

      Refresh image - calculate and copy the image to an external buffer (optional) then invalidate its current content.

      :arg buffer: An optional object that implements the buffer protocol.
         If specified, the image is calculated and copied to the buffer,
         which must be big enough or an exception is thrown.
      :type buffer: any buffer type
      :arg format: An optional image format specifier for the image that will be copied to the buffer.
         Only valid values are "RGBA" or "BGRA"
      :type format: str

   .. attribute:: scale

      Fast scale of image (near neighbour).
      
      :type: bool
      
   .. attribute:: size

      Image size. (readonly)
      
      :type: tuple of two ints

   .. method:: setSource(id, image)

      Set image source - all sources must have the same size.
      
      :arg id: Identifier of the source to set.
      :type id: str
      :arg image: Image source of type...
      
         * :class:`VideoFFmpeg`
         * :class:`ImageFFmpeg`
         * :class:`ImageBuff`
         * :class:`ImageMirror`
         * :class:`ImageMix`
         * :class:`ImageRender`
         * :class:`ImageViewport`

   .. method:: setWeight(id, weight)

      Set image source weight - the sum of the weights should be 256 to get full color intensity in the output.
      
      :arg id: Identifier of the source.
      :type id: str
      :arg weight: Weight of the source.
      :type weight: int

   .. attribute:: valid

      Tells if an image is available. (readonly)

      :type: bool

.. class:: ImageRender(scene, camera, fbo=None)

   Image source from render.
   The render is done on a custom framebuffer object if fbo is specified,
   otherwise on the default framebuffer.
   
   :arg scene: Scene in which the image has to be taken.
   :type scene: :class:`~bge.types.KX_Scene`
   :arg camera: Camera from which the image has to be taken.
   :type camera: :class:`~bge.types.KX_Camera`
   :arg fbo: Off-screen render buffer object (optional)
   :type fbo: :class:`~bge.render.RASOffScreen`

   .. attribute:: alpha

      Use alpha in texture.
      
      :type: bool

   .. attribute:: background

      Background color.
      
      :type: int or float list [r, g, b, a] in [0.0, 255.0]

   .. attribute:: capsize

      Size of render area.
      
      :type: sequence of two ints

   .. attribute:: filter

      Pixel filter.
      
      :type: one of...
      
         * :class:`FilterBGR24`
         * :class:`FilterBlueScreen`
         * :class:`FilterColor`
         * :class:`FilterGray`
         * :class:`FilterLevel`
         * :class:`FilterNormal`
         * :class:`FilterRGB24`
         * :class:`FilterRGBA32`

   .. attribute:: flip

      Flip image vertically.
      
      :type: bool

   .. attribute:: image

      Image data. (readonly)
      
      :type: :class:`~bgl.Buffer` or None

   .. attribute:: scale

      Fast scale of image (near neighbour).
      
      :type: bool

   .. attribute:: size

      Image size. (readonly)
      
      :type: tuple of two ints

   .. attribute:: valid

      Tells if an image is available. (readonly)

      :type: bool

   .. attribute:: whole

      Use whole viewport to render.
      
      :type: bool

   .. attribute:: depth

      Use depth component of render as array of float - not suitable for texture source,
      should only be used with bge.texture.imageToArray(mode='F').
      
      :type: bool

   .. attribute:: zbuff

      Use depth component of render as grey scale color -  suitable for texture source.
      
      :type: bool

   .. method:: render()

      Render the scene but do not extract the pixels yet.
      The function returns as soon as the render commands have been send to the GPU.
      The render will proceed asynchronously in the GPU while the host can perform other tasks.
      To complete the render, you can either call :func:`refresh`
      directly of refresh the texture of which this object is the source.
      This method is useful to implement asynchronous render for optimal performance: call render()
      on frame n and refresh() on frame n+1 to give as much as time as possible to the GPU
      to render the frame while the game engine can perform other tasks.

      :return: True if the render was initiated, False if the render cannot be performed (e.g. the camera is active)
      :rtype: bool

   .. method:: refresh()
   .. method:: refresh(buffer, format="RGBA")

      Refresh video - render and optionally copy the image to an external buffer then invalidate its current content.
      The render may have been started earlier with the :func:`render` method,
      in which case this function simply waits for the render operations to complete.
      When called without argument, the pixels are not extracted but the render is guaranteed
      to be completed when the function returns.
      This only makes sense with offscreen render on texture target (see :func:`~bge.render.offScreenCreate`).

      :arg buffer: An object that implements the buffer protocol.
         If specified, the image is copied to the buffer, which must be big enough or an exception is thrown.
         The transfer to the buffer is optimal if no processing of the image is needed.
         This is the case if ``flip=False, alpha=True, scale=False, whole=True, depth=False, zbuff=False``
         and no filter is set.
      :type buffer: any buffer type of sufficient size
      :arg format: An optional image format specifier for the image that will be copied to the buffer.
         Only valid values are "RGBA" or "BGRA"
      :type format: str
      :return: True if the render is complete, False if the render cannot be performed (e.g. the camera is active)
      :rtype: bool

.. class:: ImageViewport

   Image source from viewport.

   .. attribute:: alpha

      Use alpha in texture.
      
      :type: bool

   .. attribute:: capsize

      Size of viewport area being captured.
      
      :type: sequence of two ints

   .. attribute:: filter

      Pixel filter.
      
      :type: one of...
      
         * :class:`FilterBGR24`
         * :class:`FilterBlueScreen`
         * :class:`FilterColor`
         * :class:`FilterGray`
         * :class:`FilterLevel`
         * :class:`FilterNormal`
         * :class:`FilterRGB24`
         * :class:`FilterRGBA32`

   .. attribute:: flip

      Flip image vertically.
      
      :type: bool

   .. attribute:: image

      Image data. (readonly)
      
      :type: :class:`~bgl.Buffer` or None

   .. attribute:: position

      Upper left corner of the captured area.
      
      :type: sequence of two ints

   .. method:: refresh(buffer=None, format="RGBA")

      Refresh video - copy the viewport to an external buffer (optional) then invalidate its current content.

      :arg buffer: An optional object that implements the buffer protocol.
         If specified, the image is copied to the buffer, which must be big enough or an exception is thrown.
         The transfer to the buffer is optimal if no processing of the image is needed.
         This is the case if ``flip=False, alpha=True, scale=False, whole=True, depth=False, zbuff=False``
         and no filter is set.
      :type buffer: any buffer type
      :arg format: An optional image format specifier for the image that will be copied to the buffer.
         Only valid values are "RGBA" or "BGRA"
      :type format: str

   .. attribute:: scale

      Fast scale of image (near neighbour).
      
      :type: bool

   .. attribute:: size

      Image size. (readonly)
      
      :type: tuple of two ints

   .. attribute:: valid

      Tells if an image is available. (readonly)

      :type: bool

   .. attribute:: whole

      Use whole viewport to capture.
      
      :type: bool

   .. attribute:: depth

      Use depth component of viewport as array of float - not suitable for texture source,
      should only be used with bge.texture.imageToArray(mode='F').
      
      :type: bool

   .. attribute:: zbuff

      Use depth component of viewport as grey scale color -  suitable for texture source.
      
      :type: bool

   
***************
Texture classes
***************

.. class:: Texture(gameObj, materialID=0, textureID=0, textureObj=None)

   Texture object.
   
   :arg gameObj: Game object to be created a video texture on.
   :type gameObj: :class:`~bge.types.KX_GameObject`
   :arg materialID: Material ID. (optional)
   :type materialID: int
   :arg textureID: Texture ID. (optional)
   :type textureID: int
   :arg textureObj: Texture object with shared bindId. (optional)
   :type textureObj: :class:`Texture`

   .. attribute:: bindId

      OpenGL Bind Name. (readonly)
      
      :type: int

   .. method:: close()

      Close dynamic texture and restore original.

   .. attribute:: mipmap

      Mipmap texture.
      
      :type: bool

   .. method:: refresh(refresh_source=True, ts=-1.0)

      Refresh texture from source.
      
      :arg refresh_source: Whether to also refresh the image source of the texture.
      :type refresh_source: bool
      :arg ts: If the texture controls a VideoFFmpeg object:
               timestamp (in seconds from the start of the movie) of the frame to be loaded; this can be
               used for video-sound synchonization by passing :attr:`~bge.types.KX_SoundActuator.time` to it. (optional)
      :type ts: float

   .. attribute:: source

      Source of texture.
      
      :type: one of...
      
         * :class:`VideoFFmpeg`
         * :class:`ImageFFmpeg`
         * :class:`ImageBuff`
         * :class:`ImageMirror`
         * :class:`ImageMix`
         * :class:`ImageRender`
         * :class:`ImageViewport`

   
**************
Filter classes
**************


.. class:: FilterBGR24

   Source filter BGR24.

.. class:: FilterBlueScreen

   Filter for Blue Screen. The RGB channels of the color are left unchanged, while the output alpha is obtained as follows:
   
   * if the square of the euclidian distance between the RGB color and the filter's reference color is smaller than the filter's lower limit,
     the output alpha is set to 0;
   
   * if that square is bigger than the filter's upper limit, the output alpha is set to 255;
   
   * otherwise the output alpha is linarly extrapoled between 0 and 255 in the interval of the limits.

   .. attribute:: color

      Reference color.
      
      :type: sequence of three ints
      :default: (0, 0, 255)

   .. attribute:: limits

      Reference color limits.
      
      :type: sequence of two ints
      :default: (64, 64)

   .. attribute:: previous

      Previous pixel filter.
      
      :type: one of...
      
         * :class:`FilterBGR24`
         * :class:`FilterBlueScreen`
         * :class:`FilterColor`
         * :class:`FilterGray`
         * :class:`FilterLevel`
         * :class:`FilterNormal`
         * :class:`FilterRGB24`
         * :class:`FilterRGBA32`

.. class:: FilterColor

   Filter for color calculations. The output color is obtained by multiplying the reduced 4x4 matrix with the input color
   and adding the remaining column to the result.

   .. attribute:: matrix

      Matrix [4][5] for color calculation.
      
      :type: sequence of four sequences of five ints
      :default: ((256, 0, 0, 0, 0), (0, 256, 0, 0, 0), (0, 0, 256, 0, 0), (0, 0, 0, 256, 0))

   .. attribute:: previous

      Previous pixel filter.
      
      :type: one of...
      
         * :class:`FilterBGR24`
         * :class:`FilterBlueScreen`
         * :class:`FilterColor`
         * :class:`FilterGray`
         * :class:`FilterLevel`
         * :class:`FilterNormal`
         * :class:`FilterRGB24`
         * :class:`FilterRGBA32`

.. class:: FilterGray

   Filter for gray scale effect. Proportions of R, G and B contributions in the output gray scale are 28:151:77.

   .. attribute:: previous

      Previous pixel filter.
      
      :type: one of...
      
         * :class:`FilterBGR24`
         * :class:`FilterBlueScreen`
         * :class:`FilterColor`
         * :class:`FilterGray`
         * :class:`FilterLevel`
         * :class:`FilterNormal`
         * :class:`FilterRGB24`
         * :class:`FilterRGBA32`

.. class:: FilterLevel

   Filter for levels calculations. Each output color component is obtained as follows:
   
   * if it is smaller than its corresponding min value, it is set to 0;
   
   * if it is bigger than its corresponding max value, it is set to 255;
   
   * Otherwise it is linearly extrapoled between 0 and 255 in the (min, max) interval.

   .. attribute:: levels

      Levels matrix [4] (min, max).
      
      :type: sequence of four sequences of two ints
      :default: ((0, 255), (0, 255), (0, 255), (0, 255))

   .. attribute:: previous

      Previous pixel filter.
      
      :type: one of...
      
         * :class:`FilterBGR24`
         * :class:`FilterBlueScreen`
         * :class:`FilterColor`
         * :class:`FilterGray`
         * :class:`FilterLevel`
         * :class:`FilterNormal`
         * :class:`FilterRGB24`
         * :class:`FilterRGBA32`

.. class:: FilterNormal

   Normal map filter.

   .. attribute:: colorIdx

      Index of color used to calculate normal (0 - red, 1 - green, 2 - blue, 3 - alpha).
      
      :type: int in [0, 3]
      :default: 0

   .. attribute:: depth

      Depth of relief.
      
      :type: float
      :default: 4.0

   .. attribute:: previous

      Previous pixel filter.
      
      :type: one of...
      
         * :class:`FilterBGR24`
         * :class:`FilterBlueScreen`
         * :class:`FilterColor`
         * :class:`FilterGray`
         * :class:`FilterLevel`
         * :class:`FilterNormal`
         * :class:`FilterRGB24`
         * :class:`FilterRGBA32`

.. class:: FilterRGB24

   Returns a new input filter object to be used with :class:`ImageBuff` object when the image passed
   to the :meth:`ImageBuff.load` function has the 3-bytes pixel format BGR.

.. class:: FilterRGBA32

   Source filter RGBA32.


*********
Functions
*********

.. function:: getLastError()

   Last error that occurred in a bge.texture function.

   :return: The description of the last error occurred in a bge.texture function.
   :rtype: str

.. function:: imageToArray(image, mode)

   Returns a :class:`~bgl.Buffer` corresponding to the current image stored in a texture source object.

   :arg image: Image source object of type ...

      * :class:`VideoFFmpeg`
      * :class:`ImageFFmpeg`
      * :class:`ImageBuff`
      * :class:`ImageMirror`
      * :class:`ImageMix`
      * :class:`ImageRender`
      * :class:`ImageViewport`
   
   :arg mode: Optional argument representing the pixel format.
              
      * You can use the characters R, G, B for the 3 color channels, A for the alpha channel,
        0 to force a fixed 0 color channel and 1 to force a fixed 255 color channel.
        
        Examples:
            * "BGR" will return 3 bytes per pixel with the Blue, Green and Red channels in that order.
            * "RGB1" will return 4 bytes per pixel with the Red, Green, Blue channels in that order and the alpha channel forced to 255.
      
      * A special mode "F" allows to return the image as an array of float. This mode should only be used to retrieve
        the depth buffer of the class:`ImageViewport` and :class:`ImageRender` objects.
        The default mode is "RGBA".
   
   :type mode: str
   
   :return: An object representing the image as one dimensional array of bytes of size (pixel_size*width*height),
            line by line starting from the bottom of the image. The pixel size and format is determined by the mode
            parameter. For mode 'F', the array is a one dimensional array of float of size (width*height).
   :rtype: :class:`~bgl.Buffer`

.. function:: materialID(object, name)

   Returns a numeric value that can be used in :class:`Texture` to create a dynamic texture.

   The value corresponds to an internal material number that uses the texture identified
   by name. name is a string representing a texture name with ``IM`` prefix if you want to
   identify the texture directly. This method works for basic tex face and for material,
   provided the material has a texture channel using that particular texture in first
   position of the texture stack. name can also have ``MA`` prefix if you want to identify
   the texture by material. In that case the material must have a texture channel in first
   position.

   If the object has no material that matches name, it generates a runtime error. Use try/except to catch the exception.

   Ex: ``bge.texture.materialID(obj, 'IMvideo.png')``

   :arg object: The game object that uses the texture you want to make dynamic.
   :type object: :class:`~bge.types.KX_GameObject`
   :arg name: Name of the texture/material you want to make dynamic.
   :type name: str
   
   :return: The internal material number.
   :rtype: int

.. function:: setLogFile(filename)

   Sets the name of a text file in which runtime error messages will be written, in addition to the printing
   of the messages on the Python console. Only the runtime errors specific to the VideoTexture module
   are written in that file, ordinary runtime time errors are not written.

   :arg filename: Name of the error log file.
   :type filename: str
   
   :return: -1 if the parameter name is invalid (not of type string), else 0.
   :rtype: int

   
*********
Constants
*********

FFmpeg Video and Image Status
+++++++++++++++++++++++++++++


.. data:: SOURCE_ERROR

.. data:: SOURCE_EMPTY

.. data:: SOURCE_READY

.. data:: SOURCE_PLAYING

.. data:: SOURCE_STOPPED


Image Blending Modes
++++++++++++++++++++

See Wikipedia's `Blend Modes <https://en.wikipedia.org/wiki/Blend_modes>`_ for reference.

.. data:: IMB_BLEND_MIX

.. data:: IMB_BLEND_ADD

.. data:: IMB_BLEND_SUB

.. data:: IMB_BLEND_MUL

.. data:: IMB_BLEND_LIGHTEN

.. data:: IMB_BLEND_DARKEN

.. data:: IMB_BLEND_ERASE_ALPHA

.. data:: IMB_BLEND_ADD_ALPHA

.. data:: IMB_BLEND_OVERLAY

.. data:: IMB_BLEND_HARDLIGHT

.. data:: IMB_BLEND_COLORBURN

.. data:: IMB_BLEND_LINEARBURN

.. data:: IMB_BLEND_COLORDODGE

.. data:: IMB_BLEND_SCREEN

.. data:: IMB_BLEND_SOFTLIGHT

.. data:: IMB_BLEND_PINLIGHT

.. data:: IMB_BLEND_VIVIDLIGHT

.. data:: IMB_BLEND_LINEARLIGHT

.. data:: IMB_BLEND_DIFFERENCE

.. data:: IMB_BLEND_EXCLUSION

.. data:: IMB_BLEND_HUE

.. data:: IMB_BLEND_SATURATION

.. data:: IMB_BLEND_LUMINOSITY

.. data:: IMB_BLEND_COLOR

.. data:: IMB_BLEND_COPY

.. data:: IMB_BLEND_COPY_RGB

.. data:: IMB_BLEND_COPY_ALPHA

