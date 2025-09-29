# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import Menu
from bpy.app.translations import (
    contexts as i18n_contexts,
)
from bl_ui import node_add_menu


class NODE_MT_texture_node_input_base(Menu):
    bl_label = "Input"

    def draw(self, _context):
        layout = self.layout
        self.node_operator(layout, "TextureNodeCoordinates")
        self.node_operator(layout, "TextureNodeCurveTime")
        self.node_operator(layout, "TextureNodeImage")
        self.node_operator(layout, "TextureNodeTexture")


class NODE_MT_texture_node_output_base(Menu):
    bl_label = "Output"

    def draw(self, _context):
        layout = self.layout
        self.node_operator(layout, "TextureNodeOutput")
        self.node_operator(layout, "TextureNodeViewer")


class NODE_MT_texture_node_color_base(Menu):
    bl_label = "Color"

    def draw(self, _context):
        layout = self.layout
        self.node_operator(layout, "TextureNodeHueSaturation")
        self.node_operator(layout, "TextureNodeInvert")
        self.node_operator(layout, "TextureNodeMixRGB")
        self.node_operator(layout, "TextureNodeCurveRGB")
        layout.separator()
        self.node_operator(layout, "TextureNodeCombineColor")
        self.node_operator(layout, "TextureNodeSeparateColor")


class NODE_MT_texture_node_converter_base(Menu):
    bl_label = "Converter"

    def draw(self, _context):
        layout = self.layout
        self.node_operator(layout, "TextureNodeValToRGB")
        self.node_operator(layout, "TextureNodeDistance")
        self.node_operator(layout, "TextureNodeMath")
        self.node_operator(layout, "TextureNodeRGBToBW")
        self.node_operator(layout, "TextureNodeValToNor")


class NODE_MT_texture_node_distort_base(Menu):
    bl_label = "Distort"

    def draw(self, _context):
        layout = self.layout
        self.node_operator(layout, "TextureNodeAt")
        self.node_operator(layout, "TextureNodeRotate")
        self.node_operator(layout, "TextureNodeScale")
        self.node_operator(layout, "TextureNodeTranslate")


class NODE_MT_texture_node_pattern_base(Menu):
    bl_label = "Pattern"
    bl_translation_context = i18n_contexts.id_texture

    def draw(self, _context):
        layout = self.layout
        self.node_operator(layout, "TextureNodeBricks")
        self.node_operator(layout, "TextureNodeChecker")


class NODE_MT_texture_node_texture_base(Menu):
    bl_label = "Texture"

    def draw(self, _context):
        layout = self.layout
        self.node_operator(layout, "TextureNodeTexBlend")
        self.node_operator(layout, "TextureNodeTexClouds")
        self.node_operator(layout, "TextureNodeTexDistNoise")
        self.node_operator(layout, "TextureNodeTexMagic")
        self.node_operator(layout, "TextureNodeTexMarble")
        self.node_operator(layout, "TextureNodeTexMusgrave")
        self.node_operator(layout, "TextureNodeTexNoise")
        self.node_operator(layout, "TextureNodeTexStucci")
        self.node_operator(layout, "TextureNodeTexVoronoi")
        self.node_operator(layout, "TextureNodeTexWood")


class NODE_MT_texture_node_all_base(node_add_menu.NodeMenu):
    bl_label = ""
    menu_path = "Root"
    bl_translation_context = i18n_contexts.operator_default

    # NOTE: Menus are looked up via their label, this is so that both the Add
    # & Swap menus can share the same layout while each using their
    # corresponding menus
    def draw(self, context):
        del context
        layout = self.layout
        self.draw_menu(layout, "Input")
        self.draw_menu(layout, "Output")
        layout.separator()
        self.draw_menu(layout, "Color")
        self.draw_menu(layout, "Converter")
        self.draw_menu(layout, "Distort")
        self.draw_menu(layout, "Pattern")
        self.draw_menu(layout, "Texture")
        layout.separator()
        self.draw_menu(layout, "Group")
        self.draw_menu(layout, "Layout")

        self.draw_root_assets(layout)


add_menus = {
    # menu bl_idname: baseclass
    "NODE_MT_category_texture_input": NODE_MT_texture_node_input_base,
    "NODE_MT_category_texture_output": NODE_MT_texture_node_output_base,
    "NODE_MT_category_texture_color": NODE_MT_texture_node_color_base,
    "NODE_MT_category_texture_converter": NODE_MT_texture_node_converter_base,
    "NODE_MT_category_texture_distort": NODE_MT_texture_node_distort_base,
    "NODE_MT_category_texture_pattern": NODE_MT_texture_node_pattern_base,
    "NODE_MT_category_texture_texture": NODE_MT_texture_node_texture_base,
    "NODE_MT_texture_node_add_all": NODE_MT_texture_node_all_base,
}
add_menus = node_add_menu.generate_menus(
    add_menus,
    template=node_add_menu.AddNodeMenu,
    base_dict=node_add_menu.add_base_pathing_dict
)


swap_menus = {
    # menu bl_idname: baseclass
    "NODE_MT_texture_node_input_swap": NODE_MT_texture_node_input_base,
    "NODE_MT_texture_node_output_swap": NODE_MT_texture_node_output_base,
    "NODE_MT_texture_node_color_swap": NODE_MT_texture_node_color_base,
    "NODE_MT_texture_node_converter_swap": NODE_MT_texture_node_converter_base,
    "NODE_MT_texture_node_distort_swap": NODE_MT_texture_node_distort_base,
    "NODE_MT_texture_node_pattern_swap": NODE_MT_texture_node_pattern_base,
    "NODE_MT_texture_node_texture_swap": NODE_MT_texture_node_texture_base,
    "NODE_MT_texture_node_swap_all": NODE_MT_texture_node_all_base,
}
swap_menus = node_add_menu.generate_menus(
    swap_menus,
    template=node_add_menu.SwapNodeMenu,
    base_dict=node_add_menu.swap_base_pathing_dict
)


classes = (
    *add_menus,
    *swap_menus,
)


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
