# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Menu
from bl_ui import node_add_menu
from bpy.app.translations import (
    pgettext_iface as iface_,
    contexts as i18n_contexts,
)


class NODE_MT_category_compositor_input(Menu):
    bl_idname = "NODE_MT_category_compositor_input"
    bl_label = "Input"

    def draw(self, context):
        snode = context.space_data
        is_group = (len(snode.path) > 1)

        layout = self.layout
        layout.menu("NODE_MT_category_compositor_input_constant")
        layout.separator()
        node_add_menu.add_node_type(layout, "CompositorNodeBokehImage")
        node_add_menu.add_node_type(layout, "CompositorNodeImage")
        node_add_menu.add_node_type(layout, "CompositorNodeMask")
        node_add_menu.add_node_type(layout, "CompositorNodeMovieClip")
        node_add_menu.add_node_type(layout, "CompositorNodeTexture")

        if is_group:
            layout.separator()
            node_add_menu.add_node_type(layout, "NodeGroupInput")
        layout.separator()
        layout.menu("NODE_MT_category_compositor_input_scene")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_compositor_input_constant(Menu):
    bl_idname = "NODE_MT_category_compositor_input_constant"
    bl_label = "Constant"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "CompositorNodeRGB")
        node_add_menu.add_node_type(layout, "CompositorNodeValue")

        node_add_menu.draw_assets_for_catalog(layout, "Input/Constant")


class NODE_MT_category_compositor_input_scene(Menu):
    bl_idname = "NODE_MT_category_compositor_input_scene"
    bl_label = "Scene"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "CompositorNodeRLayers")
        node_add_menu.add_node_type(layout, "CompositorNodeSceneTime")
        node_add_menu.add_node_type(layout, "CompositorNodeTime")

        node_add_menu.draw_assets_for_catalog(layout, "Input/Scene")


class NODE_MT_category_compositor_output(Menu):
    bl_idname = "NODE_MT_category_compositor_output"
    bl_label = "Output"

    def draw(self, context):
        snode = context.space_data
        is_group = (len(snode.path) > 1)

        layout = self.layout
        node_add_menu.add_node_type(layout, "CompositorNodeComposite")
        node_add_menu.add_node_type(layout, "CompositorNodeViewer")
        layout.separator()
        node_add_menu.add_node_type(layout, "CompositorNodeOutputFile")

        if is_group:
            layout.separator()
            node_add_menu.add_node_type(layout, "NodeGroupOutput")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_compositor_color(Menu):
    bl_idname = "NODE_MT_category_compositor_color"
    bl_label = "Color"

    def draw(self, _context):
        layout = self.layout
        layout.menu("NODE_MT_category_compositor_color_adjust")
        layout.separator()
        layout.menu("NODE_MT_category_compositor_color_mix")
        layout.separator()
        node_add_menu.add_node_type(layout, "CompositorNodePremulKey")
        node_add_menu.add_node_type(layout, "CompositorNodeValToRGB")
        node_add_menu.add_node_type(layout, "CompositorNodeConvertColorSpace")
        node_add_menu.add_node_type(layout, "CompositorNodeSetAlpha")
        layout.separator()
        node_add_menu.add_node_type(layout, "CompositorNodeInvert")
        node_add_menu.add_node_type(layout, "CompositorNodeRGBToBW")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_compositor_color_adjust(Menu):
    bl_idname = "NODE_MT_category_compositor_color_adjust"
    bl_label = "Adjust"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "CompositorNodeBrightContrast")
        node_add_menu.add_node_type(layout, "CompositorNodeColorBalance")
        node_add_menu.add_node_type(layout, "CompositorNodeColorCorrection")
        node_add_menu.add_node_type(layout, "CompositorNodeExposure")
        node_add_menu.add_node_type(layout, "CompositorNodeGamma")
        node_add_menu.add_node_type(layout, "CompositorNodeHueCorrect")
        node_add_menu.add_node_type(layout, "CompositorNodeHueSat")
        node_add_menu.add_node_type(layout, "CompositorNodeCurveRGB")
        node_add_menu.add_node_type(layout, "CompositorNodeTonemap")

        node_add_menu.draw_assets_for_catalog(layout, "Color/Adjust")


class NODE_MT_category_compositor_color_mix(Menu):
    bl_idname = "NODE_MT_category_compositor_color_mix"
    bl_label = "Mix"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "CompositorNodeAlphaOver")
        layout.separator()
        node_add_menu.add_node_type(layout, "CompositorNodeCombineColor")
        node_add_menu.add_node_type(layout, "CompositorNodeSeparateColor")
        layout.separator()
        node_add_menu.add_node_type(
            layout, "CompositorNodeMixRGB",
            label=iface_("Mix Color"))
        node_add_menu.add_node_type(layout, "CompositorNodeZcombine")
        node_add_menu.draw_assets_for_catalog(layout, "Color/Mix")


class NODE_MT_category_compositor_filter(Menu):
    bl_idname = "NODE_MT_category_compositor_filter"
    bl_label = "Filter"

    def draw(self, _context):
        layout = self.layout
        layout.menu("NODE_MT_category_compositor_filter_blur")
        layout.separator()
        node_add_menu.add_node_type(layout, "CompositorNodeAntiAliasing")
        node_add_menu.add_node_type(layout, "CompositorNodeDenoise")
        node_add_menu.add_node_type(layout, "CompositorNodeDespeckle")
        layout.separator()
        node_add_menu.add_node_type(layout, "CompositorNodeDilateErode")
        node_add_menu.add_node_type(layout, "CompositorNodeInpaint")
        layout.separator()
        node_add_menu.add_node_type(layout, "CompositorNodeFilter")
        node_add_menu.add_node_type(layout, "CompositorNodeGlare")
        node_add_menu.add_node_type(layout, "CompositorNodeKuwahara")
        node_add_menu.add_node_type(layout, "CompositorNodePixelate")
        node_add_menu.add_node_type(layout, "CompositorNodePosterize")
        node_add_menu.add_node_type(layout, "CompositorNodeSunBeams")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_compositor_filter_blur(Menu):
    bl_idname = "NODE_MT_category_compositor_filter_blur"
    bl_label = "Blur"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "CompositorNodeBilateralblur")
        node_add_menu.add_node_type(layout, "CompositorNodeBlur")
        node_add_menu.add_node_type(layout, "CompositorNodeBokehBlur")
        node_add_menu.add_node_type(layout, "CompositorNodeDefocus")
        node_add_menu.add_node_type(layout, "CompositorNodeDBlur")
        node_add_menu.add_node_type(layout, "CompositorNodeVecBlur")

        node_add_menu.draw_assets_for_catalog(layout, "Filter/Blur")


class NODE_MT_category_compositor_group(Menu):
    bl_idname = "NODE_MT_category_compositor_group"
    bl_label = "Group"

    def draw(self, context):
        layout = self.layout
        node_add_menu.draw_node_group_add_menu(context, layout)
        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_compositor_keying(Menu):
    bl_idname = "NODE_MT_category_compositor_keying"
    bl_label = "Keying"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "CompositorNodeChannelMatte")
        node_add_menu.add_node_type(layout, "CompositorNodeChromaMatte")
        node_add_menu.add_node_type(layout, "CompositorNodeColorMatte")
        node_add_menu.add_node_type(layout, "CompositorNodeColorSpill")
        node_add_menu.add_node_type(layout, "CompositorNodeDiffMatte")
        node_add_menu.add_node_type(layout, "CompositorNodeDistanceMatte")
        node_add_menu.add_node_type(layout, "CompositorNodeKeying")
        node_add_menu.add_node_type(layout, "CompositorNodeKeyingScreen")
        node_add_menu.add_node_type(layout, "CompositorNodeLumaMatte")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_compositor_mask(Menu):
    bl_idname = "NODE_MT_category_compositor_mask"
    bl_label = "Mask"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "CompositorNodeCryptomatteV2")
        node_add_menu.add_node_type(layout, "CompositorNodeCryptomatte")
        layout.separator()
        node_add_menu.add_node_type(layout, "CompositorNodeBoxMask")
        node_add_menu.add_node_type(layout, "CompositorNodeEllipseMask")
        layout.separator()
        node_add_menu.add_node_type(layout, "CompositorNodeDoubleEdgeMask")
        node_add_menu.add_node_type(layout, "CompositorNodeIDMask")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_compositor_tracking(Menu):
    bl_idname = "NODE_MT_category_compositor_tracking"
    bl_label = "Tracking"
    bl_translation_context = i18n_contexts.id_movieclip

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "CompositorNodePlaneTrackDeform")
        node_add_menu.add_node_type(layout, "CompositorNodeStabilize")
        node_add_menu.add_node_type(layout, "CompositorNodeTrackPos")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_compositor_transform(Menu):
    bl_idname = "NODE_MT_category_compositor_transform"
    bl_label = "Transform"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "CompositorNodeRotate")
        node_add_menu.add_node_type(layout, "CompositorNodeScale")
        node_add_menu.add_node_type(layout, "CompositorNodeTransform")
        node_add_menu.add_node_type(layout, "CompositorNodeTranslate")
        layout.separator()
        node_add_menu.add_node_type(layout, "CompositorNodeCornerPin")
        node_add_menu.add_node_type(layout, "CompositorNodeCrop")
        layout.separator()
        node_add_menu.add_node_type(layout, "CompositorNodeDisplace")
        node_add_menu.add_node_type(layout, "CompositorNodeFlip")
        node_add_menu.add_node_type(layout, "CompositorNodeMapUV")
        layout.separator()
        node_add_menu.add_node_type(layout, "CompositorNodeLensdist")
        node_add_menu.add_node_type(layout, "CompositorNodeMovieDistortion")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_compositor_utilities(Menu):
    bl_idname = "NODE_MT_category_compositor_utilities"
    bl_label = "Utilities"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "CompositorNodeMapRange")
        node_add_menu.add_node_type(layout, "CompositorNodeMapValue")
        node_add_menu.add_node_type(layout, "CompositorNodeMath")
        layout.separator()
        node_add_menu.add_node_type(layout, "CompositorNodeLevels")
        node_add_menu.add_node_type(layout, "CompositorNodeNormalize")
        layout.separator()
        node_add_menu.add_node_type(layout, "CompositorNodeSplit")
        node_add_menu.add_node_type(layout, "CompositorNodeSwitch")
        node_add_menu.add_node_type(
            layout, "CompositorNodeSwitchView",
            label=iface_("Switch Stereo View"))

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_compositor_vector(Menu):
    bl_idname = "NODE_MT_category_compositor_vector"
    bl_label = "Vector"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "CompositorNodeCombineXYZ")
        node_add_menu.add_node_type(layout, "CompositorNodeSeparateXYZ")
        layout.separator()
        node_add_menu.add_node_type(layout, "CompositorNodeNormal")
        node_add_menu.add_node_type(layout, "CompositorNodeCurveVec")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_compositor_LAYOUT(Menu):
    bl_idname = "NODE_MT_category_compositor_LAYOUT"
    bl_label = "Layout"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "NodeFrame")
        node_add_menu.add_node_type(layout, "NodeReroute")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_compositor_node_add_all(Menu):
    bl_idname = "NODE_MT_compositor_node_add_all"
    bl_label = ""

    def draw(self, context):
        layout = self.layout
        layout.menu("NODE_MT_category_compositor_input")
        layout.menu("NODE_MT_category_compositor_output")
        layout.separator()
        layout.menu("NODE_MT_category_compositor_color")
        layout.menu("NODE_MT_category_compositor_filter")
        layout.separator()
        layout.menu("NODE_MT_category_compositor_keying")
        layout.menu("NODE_MT_category_compositor_mask")
        layout.separator()
        layout.menu("NODE_MT_category_compositor_tracking")
        layout.separator()
        layout.menu("NODE_MT_category_compositor_transform")
        layout.menu("NODE_MT_category_compositor_utilities")
        layout.menu("NODE_MT_category_compositor_vector")
        layout.separator()
        layout.menu("NODE_MT_category_compositor_group")
        layout.menu("NODE_MT_category_layout")

        node_add_menu.draw_root_assets(layout)


classes = (
    NODE_MT_compositor_node_add_all,
    NODE_MT_category_compositor_input,
    NODE_MT_category_compositor_input_constant,
    NODE_MT_category_compositor_input_scene,
    NODE_MT_category_compositor_output,
    NODE_MT_category_compositor_color,
    NODE_MT_category_compositor_color_adjust,
    NODE_MT_category_compositor_color_mix,
    NODE_MT_category_compositor_filter,
    NODE_MT_category_compositor_filter_blur,
    NODE_MT_category_compositor_keying,
    NODE_MT_category_compositor_mask,
    NODE_MT_category_compositor_tracking,
    NODE_MT_category_compositor_transform,
    NODE_MT_category_compositor_utilities,
    NODE_MT_category_compositor_vector,
    NODE_MT_category_compositor_group,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
