"""
Save 3D Viewport to a PNG
+++++++++++++++++++++++++

Capture the 3D viewport's main region from the current window
and write it to a PNG file using :mod:`imbuf`.
"""

import bpy
import imbuf

window = bpy.context.window

# Locate the 3D viewport (if any).
region = None
for area in window.screen.areas:
    if area.type == 'VIEW_3D':
        for region_iter in area.regions:
            if region_iter.type == 'WINDOW':
                region = region_iter
                break
        break

if region is not None:
    # The end coordinate is not inclusive, like Python slicing.
    region_rect = (
        (region.x, region.y),
        (region.x + region.width, region.y + region.height),
    )
    pixels = window.screenshot(region=region_rect)
    height, width = pixels.shape[0], pixels.shape[1]

    ibuf = imbuf.new((width, height))
    ibuf.file_type = 'PNG'
    with ibuf.with_buffer('BYTE', write=True) as buf:
        # The cast produces a zero-copy 1-D view of the same bytes.
        # Currently only 1-D copies are supported by Python.
        buf.cast('B')[:] = pixels.cast('B')

    imbuf.write(ibuf, filepath="/tmp/viewport.png")
