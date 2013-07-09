"""
Hello World Text Example
++++++++++++++++++++++++

Blender Game Engine example of using the blf module. For this module to work we
need to use the OpenGL wrapper :class:`~bgl` as well.
"""
# import game engine modules
from bge import render
from bge import logic
# import stand alone modules
import bgl
import blf


def init():
    """init function - runs once"""
    # create a new font object, use external ttf file
    font_path = logic.expandPath('//Zeyada.ttf')
    # store the font indice - to use later
    logic.font_id = blf.load(font_path)

    # set the font drawing routine to run every frame
    scene = logic.getCurrentScene()
    scene.post_draw = [write]


def write():
    """write on screen"""
    width = render.getWindowWidth()
    height = render.getWindowHeight()

    # OpenGL setup
    bgl.glMatrixMode(bgl.GL_PROJECTION)
    bgl.glLoadIdentity()
    bgl.gluOrtho2D(0, width, 0, height)
    bgl.glMatrixMode(bgl.GL_MODELVIEW)
    bgl.glLoadIdentity()

    # BLF drawing routine
    font_id = logic.font_id
    blf.position(font_id, (width * 0.2), (height * 0.3), 0)
    blf.size(font_id, 50, 72)
    blf.draw(font_id, "Hello World")
