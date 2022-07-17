"""
Image Data
++++++++++

The Image data-block is a shallow wrapper around image or video file(s)
(on disk, as packed data, or generated).

All actual data like the pixel buffer, size, resolution etc. is
cached in an :class:`imbuf.types.ImBuf` image buffer (or several buffers
in some cases, like UDIM textures, multi-views, animations...).

Several properties and functions of the Image data-block are then actually
using/modifying its image buffer, and not the Image data-block itself.

.. warning::

   One key limitation is that image buffers are not shared between different
   Image data-blocks, and they are not duplicated when copying an image.

   So until a modified image buffer is saved on disk, duplicating its Image
   data-block will not propagate the underlying buffer changes to the new Image.


This example script generates an Image data-block with a given size,
change its first pixel, rescale it, and duplicates the image.

The duplicated image still has the same size and colors as the original image
at its creation, all editing in the original image's buffer is 'lost' in its copy.
"""

import bpy

image_src = bpy.data.images.new('src', 1024, 102)
print(image_src.size)
print(image_src.pixels[0:4])

image_src.scale(1024, 720)
image_src.pixels[0:4] = (0.5, 0.5, 0.5, 0.5)
image_src.update()
print(image_src.size)
print(image_src.pixels[0:4])

image_dest = image_src.copy()
image_dest.update()
print(image_dest.size)
print(image_dest.pixels[0:4])
