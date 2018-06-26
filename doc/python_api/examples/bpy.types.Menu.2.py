"""
Extending Menus
+++++++++++++++

When creating menus for add-ons you can't reference menus
in Blender's default scripts.
Instead, the add-on can add menu items to existing menus.

The function menu_draw acts like :class:`Menu.draw`.
"""
import bpy


def menu_draw(self, context):
    self.layout.operator("wm.save_homefile")


bpy.types.INFO_MT_file.append(menu_draw)
