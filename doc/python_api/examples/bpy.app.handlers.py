"""
Basic Handler Example
+++++++++++++++++++++
This script shows the most simple example of adding a handler.
"""

import bpy


def my_handler(scene):
    print("Frame Change", scene.frame_current)

bpy.app.handlers.frame_change_pre.append(my_handler)
