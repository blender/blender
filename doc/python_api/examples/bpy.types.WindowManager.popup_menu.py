"""
Popup menus can be useful for creating menus without having to register menu classes.

Note that they will not block the scripts execution, so the caller can't wait for user input.
"""

import bpy


def draw(self, context):
    self.layout.label("Hello World")

bpy.context.window_manager.popup_menu(draw, title="Greeting", icon='INFO')
