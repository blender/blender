# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Menu
from bl_ui import node_add_menu


class NODE_MT_category_texture_input(Menu):
    bl_idname = "NODE_MT_category_texture_input"
    bl_label = "Input"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "TextureNodeCoordinates")
        node_add_menu.add_node_type(layout, "TextureNodeCurveTime")
        node_add_menu.add_node_type(layout, "TextureNodeImage")
        node_add_menu.add_node_type(layout, "TextureNodeTexture")


class NODE_MT_category_texture_output(Menu):
    bl_idname = "NODE_MT_category_texture_output"
    bl_label = "Output"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "TextureNodeOutput")
        node_add_menu.add_node_type(layout, "TextureNodeViewer")


class NODE_MT_category_texture_color(Menu):
    bl_idname = "NODE_MT_category_texture_color"
    bl_label = "Color"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "TextureNodeHueSaturation")
        node_add_menu.add_node_type(layout, "TextureNodeInvert")
        node_add_menu.add_node_type(layout, "TextureNodeMixRGB")
        node_add_menu.add_node_type(layout, "TextureNodeCurveRGB")
        layout.separator()
        node_add_menu.add_node_type(layout, "TextureNodeCombineColor")
        node_add_menu.add_node_type(layout, "TextureNodeSeparateColor")


class NODE_MT_category_texture_converter(Menu):
    bl_idname = "NODE_MT_category_texture_converter"
    bl_label = "Converter"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "TextureNodeValToRGB")
        node_add_menu.add_node_type(layout, "TextureNodeDistance")
        node_add_menu.add_node_type(layout, "TextureNodeMath")
        node_add_menu.add_node_type(layout, "TextureNodeRGBToBW")
        node_add_menu.add_node_type(layout, "TextureNodeValToNor")


class NODE_MT_category_texture_distort(Menu):
    bl_idname = "NODE_MT_category_texture_distort"
    bl_label = "Distort"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "TextureNodeAt")
        node_add_menu.add_node_type(layout, "TextureNodeRotate")
        node_add_menu.add_node_type(layout, "TextureNodeScale")
        node_add_menu.add_node_type(layout, "TextureNodeTranslate")


class NODE_MT_category_texture_pattern(Menu):
    bl_idname = "NODE_MT_category_texture_pattern"
    bl_label = "Pattern"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "TextureNodeBricks")
        node_add_menu.add_node_type(layout, "TextureNodeChecker")


class NODE_MT_category_texture_texture(Menu):
    bl_idname = "NODE_MT_category_texture_texture"
    bl_label = "Texture"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "TextureNodeTexBlend")
        node_add_menu.add_node_type(layout, "TextureNodeTexClouds")
        node_add_menu.add_node_type(layout, "TextureNodeTexDistNoise")
        node_add_menu.add_node_type(layout, "TextureNodeTexMagic")
        node_add_menu.add_node_type(layout, "TextureNodeTexMarble")
        node_add_menu.add_node_type(layout, "TextureNodeTexMusgrave")
        node_add_menu.add_node_type(layout, "TextureNodeTexNoise")
        node_add_menu.add_node_type(layout, "TextureNodeTexStucci")
        node_add_menu.add_node_type(layout, "TextureNodeTexVoronoi")
        node_add_menu.add_node_type(layout, "TextureNodeTexWood")


class NODE_MT_category_texture_group(Menu):
    bl_idname = "NODE_MT_category_texture_group"
    bl_label = "Group"

    def draw(self, context):
        layout = self.layout
        node_add_menu.draw_node_group_add_menu(context, layout)


class NODE_MT_texture_node_add_all(Menu):
    bl_idname = "NODE_MT_texture_node_add_all"
    bl_label = "Add"

    def draw(self, _context):
        layout = self.layout
        layout.menu("NODE_MT_category_texture_input")
        layout.menu("NODE_MT_category_texture_output")
        layout.separator()
        layout.menu("NODE_MT_category_texture_color")
        layout.menu("NODE_MT_category_texture_converter")
        layout.menu("NODE_MT_category_texture_distort")
        layout.menu("NODE_MT_category_texture_pattern")
        layout.menu("NODE_MT_category_texture_texture")
        layout.separator()
        layout.menu("NODE_MT_category_texture_group")
        layout.menu("NODE_MT_category_layout")


classes = (
    NODE_MT_texture_node_add_all,
    NODE_MT_category_texture_input,
    NODE_MT_category_texture_output,
    NODE_MT_category_texture_color,
    NODE_MT_category_texture_converter,
    NODE_MT_category_texture_distort,
    NODE_MT_category_texture_pattern,
    NODE_MT_category_texture_texture,
    NODE_MT_category_texture_group,
)


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
