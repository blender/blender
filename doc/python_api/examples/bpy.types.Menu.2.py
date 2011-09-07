"""
Extending Menus
+++++++++++++++
When creating menus for addons you can't reference menus in blenders default
scripts.

Instead the addon can add menu items to existing menus.

The function menu_draw acts like Menu.draw
"""
import bpy


def menu_draw(self, context):
    self.layout.operator("wm.save_homefile")

bpy.types.INFO_MT_file.append(menu_draw)
