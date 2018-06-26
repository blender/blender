"""
Hello World Text Example
++++++++++++++++++++++++

Example of using the blf module. For this module to work we
need to use the OpenGL wrapper :class:`~bgl` as well.
"""
# import stand alone modules
import bgl
import blf
import bpy

font_info = {
    "font_id": 0,
    "handler": None,
}


def init():
    """init function - runs once"""
    import os
    # Create a new font object, use external ttf file.
    font_path = bpy.path.abspath('//Zeyada.ttf')
    # Store the font indice - to use later.
    if os.path.exists(font_path):
        font_info["font_id"] = blf.load(font_path)
    else:
        # Default font.
        font_info["font_id"] = 0

    # set the font drawing routine to run every frame
    font_info["handler"] = bpy.types.SpaceView3D.draw_handler_add(
        draw_callback_px, (None, None), 'WINDOW', 'POST_PIXEL')


def draw_callback_px(self, context):
    """Draw on the viewports"""
    # BLF drawing routine
    font_id = font_info["font_id"]
    blf.position(font_id, 2, 80, 0)
    blf.size(font_id, 50, 72)
    blf.draw(font_id, "Hello World")


if __name__ == '__main__':
    init()
