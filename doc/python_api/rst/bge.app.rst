
Application Data (bge.app)
==========================

Module to access application values that remain unchanged during runtime.

.. module:: bge.app

.. data:: version

   The Blender/BGE version as a tuple of 3 ints, eg. (2, 75, 1).

   .. note::

      Version tuples can be compared simply with (in)equality symbols;
      for example, ``(2, 74, 5) <= (2, 75, 0)`` returns True (lexical order).

   :type: tuple of three ints

.. data:: version_string

   The Blender/BGE version formatted as a string, eg. "2.75 (sub 1)".

   :type: str

.. data:: version_char

   The Blender/BGE version character (for minor releases).

   :type: str

.. data:: has_texture_ffmpeg

   True if the BGE has been built with FFmpeg support,
   enabling use of :class:`~bge.texture.ImageFFmpeg` and :class:`~bge.texture.VideoFFmpeg`.

   :type: bool

.. data:: has_joystick

   True if the BGE has been built with joystick support.

   :type: bool

.. data:: has_physics

   True if the BGE has been built with physics support.

   :type: bool
