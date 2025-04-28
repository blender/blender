"""
Drawing Text to an Image
++++++++++++++++++++++++

Example showing how text can be draw into an image.
This can be done by binding an image buffer (:mod:`imbuf`) to the font's ID.
"""

import blf
import imbuf

image_size = 512, 512
font_size = 20

ibuf = imbuf.new(image_size)

font_id = blf.load("/path/to/font.ttf")

blf.color(font_id, 1.0, 1.0, 1.0, 1.0)
blf.size(font_id, font_size)
blf.position(font_id, 0, image_size[0] - font_size, 0)

blf.enable(font_id, blf.WORD_WRAP)
blf.word_wrap(font_id, image_size[0])

with blf.bind_imbuf(font_id, ibuf, display_name="sRGB"):
    blf.draw_buffer(font_id, "Lots of wrapped text. " * 50)

imbuf.write(ibuf, filepath="/path/to/image.png")
