# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Menu
from bl_ui import node_add_menu


class NODE_MT_category_COMP_INPUT(Menu):
    bl_idname = "NODE_MT_category_COMP_INPUT"
    bl_label = "Input"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "CompositorNodeBokehImage")
        node_add_menu.add_node_type(layout, "CompositorNodeImage")
        node_add_menu.add_node_type(layout, "CompositorNodeMask")
        node_add_menu.add_node_type(layout, "CompositorNodeMovieClip")
        node_add_menu.add_node_type(layout, "CompositorNodeRLayers")
        node_add_menu.add_node_type(layout, "CompositorNodeRGB")
        node_add_menu.add_node_type(layout, "CompositorNodeSceneTime")
        node_add_menu.add_node_type(layout, "CompositorNodeTexture")
        node_add_menu.add_node_type(layout, "CompositorNodeTime")
        node_add_menu.add_node_type(layout, "CompositorNodeTrackPos")
        node_add_menu.add_node_type(layout, "CompositorNodeValue")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_COMP_OUTPUT(Menu):
    bl_idname = "NODE_MT_category_COMP_OUTPUT"
    bl_label = "Output"

    def draw(self, _context):
        layout = self.layout

        node_add_menu.add_node_type(layout, "CompositorNodeComposite")
        node_add_menu.add_node_type(layout, "CompositorNodeOutputFile")
        node_add_menu.add_node_type(layout, "CompositorNodeLevels")
        node_add_menu.add_node_type(layout, "CompositorNodeSplitViewer")
        node_add_menu.add_node_type(layout, "CompositorNodeViewer")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_COMP_COLOR(Menu):
    bl_idname = "NODE_MT_category_COMP_COLOR"
    bl_label = "Color"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "CompositorNodeAlphaOver")
        node_add_menu.add_node_type(layout, "CompositorNodeBrightContrast")
        node_add_menu.add_node_type(layout, "CompositorNodeColorBalance")
        node_add_menu.add_node_type(layout, "CompositorNodeColorCorrection")
        node_add_menu.add_node_type(layout, "CompositorNodeExposure")
        node_add_menu.add_node_type(layout, "CompositorNodeGamma")
        node_add_menu.add_node_type(layout, "CompositorNodeHueCorrect")
        node_add_menu.add_node_type(layout, "CompositorNodeHueSat")
        node_add_menu.add_node_type(layout, "CompositorNodeInvert")
        node_add_menu.add_node_type(layout, "CompositorNodeMixRGB")
        node_add_menu.add_node_type(layout, "CompositorNodePosterize")
        node_add_menu.add_node_type(layout, "CompositorNodeCurveRGB")
        node_add_menu.add_node_type(layout, "CompositorNodeTonemap")
        node_add_menu.add_node_type(layout, "CompositorNodeZcombine")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_COMP_CONVERTER(Menu):
    bl_idname = "NODE_MT_category_COMP_CONVERTER"
    bl_label = "Converter"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "CompositorNodePremulKey")
        node_add_menu.add_node_type(layout, "CompositorNodeValToRGB")
        node_add_menu.add_node_type(layout, "CompositorNodeCombineColor")
        node_add_menu.add_node_type(layout, "CompositorNodeCombineXYZ")
        node_add_menu.add_node_type(layout, "CompositorNodeConvertColorSpace")
        node_add_menu.add_node_type(layout, "CompositorNodeIDMask")
        node_add_menu.add_node_type(layout, "CompositorNodeMath")
        node_add_menu.add_node_type(layout, "CompositorNodeRGBToBW")
        node_add_menu.add_node_type(layout, "CompositorNodeSeparateColor")
        node_add_menu.add_node_type(layout, "CompositorNodeSeparateXYZ")
        node_add_menu.add_node_type(layout, "CompositorNodeSetAlpha")
        node_add_menu.add_node_type(layout, "CompositorNodeSwitchView")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_COMP_FILTER(Menu):
    bl_idname = "NODE_MT_category_COMP_FILTER"
    bl_label = "Filter"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "CompositorNodeAntiAliasing")
        node_add_menu.add_node_type(layout, "CompositorNodeBilateralblur")
        node_add_menu.add_node_type(layout, "CompositorNodeBlur")
        node_add_menu.add_node_type(layout, "CompositorNodeBokehBlur")
        node_add_menu.add_node_type(layout, "CompositorNodeDefocus")
        node_add_menu.add_node_type(layout, "CompositorNodeDenoise")
        node_add_menu.add_node_type(layout, "CompositorNodeDespeckle")
        node_add_menu.add_node_type(layout, "CompositorNodeDilateErode")
        node_add_menu.add_node_type(layout, "CompositorNodeDBlur")
        node_add_menu.add_node_type(layout, "CompositorNodeFilter")
        node_add_menu.add_node_type(layout, "CompositorNodeGlare")
        node_add_menu.add_node_type(layout, "CompositorNodeInpaint")
        node_add_menu.add_node_type(layout, "CompositorNodeKuwahara")
        node_add_menu.add_node_type(layout, "CompositorNodePixelate")
        node_add_menu.add_node_type(layout, "CompositorNodeSunBeams")
        node_add_menu.add_node_type(layout, "CompositorNodeVecBlur")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_COMP_VECTOR(Menu):
    bl_idname = "NODE_MT_category_COMP_VECTOR"
    bl_label = "Vector"

    def draw(self, _context):
        layout = self.layout

        node_add_menu.add_node_type(layout, "CompositorNodeMapRange")
        node_add_menu.add_node_type(layout, "CompositorNodeMapValue")
        node_add_menu.add_node_type(layout, "CompositorNodeNormal")
        node_add_menu.add_node_type(layout, "CompositorNodeNormalize")
        node_add_menu.add_node_type(layout, "CompositorNodeCurveVec")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_COMP_MATTE(Menu):
    bl_idname = "NODE_MT_category_COMP_MATTE"
    bl_label = "Matte"

    def draw(self, _context):
        layout = self.layout

        node_add_menu.add_node_type(layout, "CompositorNodeBoxMask")
        node_add_menu.add_node_type(layout, "CompositorNodeChannelMatte")
        node_add_menu.add_node_type(layout, "CompositorNodeChromaMatte")
        node_add_menu.add_node_type(layout, "CompositorNodeColorMatte")
        node_add_menu.add_node_type(layout, "CompositorNodeColorSpill")
        node_add_menu.add_node_type(layout, "CompositorNodeCryptomatteV2")
        node_add_menu.add_node_type(layout, "CompositorNodeCryptomatte")
        node_add_menu.add_node_type(layout, "CompositorNodeDiffMatte")
        node_add_menu.add_node_type(layout, "CompositorNodeDistanceMatte")
        node_add_menu.add_node_type(layout, "CompositorNodeDoubleEdgeMask")
        node_add_menu.add_node_type(layout, "CompositorNodeEllipseMask")
        node_add_menu.add_node_type(layout, "CompositorNodeKeying")
        node_add_menu.add_node_type(layout, "CompositorNodeKeyingScreen")
        node_add_menu.add_node_type(layout, "CompositorNodeLumaMatte")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_COMP_DISTORT(Menu):
    bl_idname = "NODE_MT_category_COMP_DISTORT"
    bl_label = "Distort"

    def draw(self, _context):
        layout = self.layout

        node_add_menu.add_node_type(layout, "CompositorNodeCornerPin")
        node_add_menu.add_node_type(layout, "CompositorNodeCrop")
        node_add_menu.add_node_type(layout, "CompositorNodeDisplace")
        node_add_menu.add_node_type(layout, "CompositorNodeFlip")
        node_add_menu.add_node_type(layout, "CompositorNodeLensdist")
        node_add_menu.add_node_type(layout, "CompositorNodeMapUV")
        node_add_menu.add_node_type(layout, "CompositorNodeMovieDistortion")
        node_add_menu.add_node_type(layout, "CompositorNodePlaneTrackDeform")
        node_add_menu.add_node_type(layout, "CompositorNodeRotate")
        node_add_menu.add_node_type(layout, "CompositorNodeScale")
        node_add_menu.add_node_type(layout, "CompositorNodeStabilize")
        node_add_menu.add_node_type(layout, "CompositorNodeTransform")
        node_add_menu.add_node_type(layout, "CompositorNodeTranslate")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_COMP_GROUP(Menu):
    bl_idname = "NODE_MT_category_COMP_GROUP"
    bl_label = "Group"

    def draw(self, context):
        layout = self.layout
        node_add_menu.draw_node_group_add_menu(context, layout)
        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_COMP_LAYOUT(Menu):
    bl_idname = "NODE_MT_category_COMP_LAYOUT"
    bl_label = "Layout"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "NodeFrame")
        node_add_menu.add_node_type(layout, "NodeReroute")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_compositing_node_add_all(Menu):
    bl_idname = "NODE_MT_compositing_node_add_all"
    bl_label = ""

    def draw(self, context):
        layout = self.layout
        layout.menu("NODE_MT_category_COMP_INPUT")
        layout.menu("NODE_MT_category_COMP_OUTPUT")
        layout.separator()
        layout.menu("NODE_MT_category_COMP_COLOR")
        layout.menu("NODE_MT_category_COMP_CONVERTER")
        layout.menu("NODE_MT_category_COMP_DISTORT")
        layout.menu("NODE_MT_category_COMP_FILTER")
        layout.menu("NODE_MT_category_COMP_MATTE")
        layout.menu("NODE_MT_category_COMP_VECTOR")
        layout.separator()
        layout.menu("NODE_MT_category_COMP_GROUP")
        layout.menu("NODE_MT_category_COMP_LAYOUT")

        node_add_menu.draw_root_assets(layout)


classes = (
    NODE_MT_compositing_node_add_all,
    NODE_MT_category_COMP_INPUT,
    NODE_MT_category_COMP_OUTPUT,
    NODE_MT_category_COMP_COLOR,
    NODE_MT_category_COMP_CONVERTER,
    NODE_MT_category_COMP_FILTER,
    NODE_MT_category_COMP_VECTOR,
    NODE_MT_category_COMP_MATTE,
    NODE_MT_category_COMP_DISTORT,
    NODE_MT_category_COMP_GROUP,
    NODE_MT_category_COMP_LAYOUT,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
