# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bl_ui import node_add_menu
from bpy.app.translations import (
    contexts as i18n_contexts,
)


class NODE_MT_compositor_node_input_base(node_add_menu.NodeMenu):
    bl_label = "Input"

    def draw(self, context):
        layout = self.layout
        self.draw_menu(layout, path="Input/Constant")
        layout.separator()
        self.node_operator(layout, "NodeGroupInput")
        self.node_operator(layout, "CompositorNodeBokehImage")
        self.node_operator(layout, "CompositorNodeImage")
        self.node_operator(layout, "CompositorNodeImageInfo")
        self.node_operator(layout, "CompositorNodeImageCoordinates")
        self.node_operator(layout, "CompositorNodeMask")
        self.node_operator(layout, "CompositorNodeMovieClip")
        if context.space_data.node_tree_sub_type == 'SEQUENCER':
            self.node_operator(layout, "CompositorNodeSequencerStripInfo")

        layout.separator()
        self.draw_menu(layout, path="Input/Scene")

        self.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_compositor_node_input_constant_base(node_add_menu.NodeMenu):
    bl_label = "Constant"
    menu_path = "Input/Constant"

    def draw(self, _context):
        layout = self.layout
        self.node_operator(layout, "CompositorNodeRGB")
        self.node_operator(layout, "ShaderNodeValue")
        self.node_operator(layout, "CompositorNodeNormal")

        self.draw_assets_for_catalog(layout, self.menu_path)


class NODE_MT_compositor_node_input_scene_base(node_add_menu.NodeMenu):
    bl_label = "Scene"
    menu_path = "Input/Scene"

    def draw(self, context):
        layout = self.layout
        if context.space_data.node_tree_sub_type == 'SCENE':
            self.node_operator(layout, "CompositorNodeRLayers")
        self.node_operator_with_outputs(context, layout, "CompositorNodeSceneTime", ["Frame", "Seconds"])
        self.node_operator(layout, "CompositorNodeTime")

        self.draw_assets_for_catalog(layout, self.menu_path)


class NODE_MT_compositor_node_output_base(node_add_menu.NodeMenu):
    bl_label = "Output"

    def draw(self, context):
        layout = self.layout
        self.node_operator(layout, "NodeEnableOutput")
        self.node_operator(layout, "NodeGroupOutput")
        self.node_operator(layout, "CompositorNodeViewer")
        if context.space_data.node_tree_sub_type == 'SCENE':
            layout.separator()
            self.node_operator(layout, "CompositorNodeOutputFile")

        self.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_compositor_node_color_base(node_add_menu.NodeMenu):
    bl_label = "Color"

    def draw(self, context):
        layout = self.layout
        self.draw_menu(layout, path="Color/Adjust")
        layout.separator()
        self.node_operator(layout, "CompositorNodePremulKey")
        self.node_operator(layout, "CompositorNodeAlphaOver")
        self.node_operator(layout, "CompositorNodeSetAlpha")
        layout.separator()
        self.node_operator(layout, "CompositorNodeCombineColor")
        self.node_operator(layout, "CompositorNodeSeparateColor")
        layout.separator()
        self.node_operator(layout, "CompositorNodeZcombine")
        self.color_mix_node(context, layout)
        layout.separator()
        self.node_operator(layout, "ShaderNodeBlackbody")
        self.node_operator(layout, "ShaderNodeValToRGB")
        self.node_operator(layout, "CompositorNodeConvertColorSpace")
        self.node_operator(layout, "CompositorNodeConvertToDisplay")
        layout.separator()
        self.node_operator(layout, "CompositorNodeInvert")
        self.node_operator(layout, "CompositorNodeRGBToBW")

        self.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_compositor_node_color_adjust_base(node_add_menu.NodeMenu):
    bl_label = "Adjust"
    menu_path = "Color/Adjust"

    def draw(self, _context):
        layout = self.layout
        self.node_operator(layout, "CompositorNodeBrightContrast")
        self.node_operator(layout, "CompositorNodeColorBalance")
        self.node_operator(layout, "CompositorNodeColorCorrection")
        self.node_operator(layout, "CompositorNodeExposure")
        self.node_operator(layout, "ShaderNodeGamma")
        self.node_operator(layout, "CompositorNodeHueCorrect")
        self.node_operator(layout, "CompositorNodeHueSat")
        self.node_operator(layout, "CompositorNodeCurveRGB")
        self.node_operator(layout, "CompositorNodeTonemap")

        self.draw_assets_for_catalog(layout, self.menu_path)


class NODE_MT_compositor_node_color_mix_base(node_add_menu.NodeMenu):
    bl_label = "Mix"
    menu_path = "Color/Mix"

    def draw(self, context):
        del context
        layout = self.layout

        self.draw_assets_for_catalog(layout, self.menu_path)


class NODE_MT_compositor_node_filter_base(node_add_menu.NodeMenu):
    bl_label = "Filter"

    def draw(self, context):
        layout = self.layout
        self.draw_menu(layout, path="Filter/Blur")
        layout.separator()
        self.node_operator(layout, "CompositorNodeAntiAliasing")
        self.node_operator(layout, "CompositorNodeConvolve")
        self.node_operator(layout, "CompositorNodeDenoise")
        self.node_operator(layout, "CompositorNodeDespeckle")
        layout.separator()
        self.node_operator(layout, "CompositorNodeDilateErode")
        self.node_operator(layout, "CompositorNodeInpaint")
        layout.separator()
        self.node_operator_with_searchable_enum_socket(
            context, layout, "CompositorNodeFilter", "Type", [
                "Soften", "Box Sharpen", "Diamond Sharpen", "Laplace", "Sobel", "Prewitt", "Kirsch", "Shadow",
            ],
        )
        self.node_operator_with_searchable_enum_socket(
            context, layout, "CompositorNodeGlare", "Type", [
                "Bloom", "Ghosts", "Streaks", "Fog Glow", "Simple Star", "Sun Beams", "Kernel",
            ],
        )

        self.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_compositor_node_filter_blur_base(node_add_menu.NodeMenu):
    bl_label = "Blur"
    menu_path = "Filter/Blur"

    def draw(self, _context):
        layout = self.layout
        self.node_operator(layout, "CompositorNodeBilateralblur")
        self.node_operator(layout, "CompositorNodeBlur")
        self.node_operator(layout, "CompositorNodeBokehBlur")
        self.node_operator(layout, "CompositorNodeDefocus")
        self.node_operator(layout, "CompositorNodeDBlur")
        self.node_operator(layout, "CompositorNodeVecBlur")

        self.draw_assets_for_catalog(layout, self.menu_path)


class NODE_MT_compositor_node_keying_base(node_add_menu.NodeMenu):
    bl_label = "Keying"

    def draw(self, _context):
        layout = self.layout
        self.node_operator(layout, "CompositorNodeChannelMatte")
        self.node_operator(layout, "CompositorNodeChromaMatte")
        self.node_operator(layout, "CompositorNodeColorMatte")
        self.node_operator(layout, "CompositorNodeColorSpill")
        self.node_operator(layout, "CompositorNodeDiffMatte")
        self.node_operator(layout, "CompositorNodeDistanceMatte")
        self.node_operator(layout, "CompositorNodeKeying")
        self.node_operator(layout, "CompositorNodeKeyingScreen")
        self.node_operator(layout, "CompositorNodeLumaMatte")

        self.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_compositor_node_mask_base(node_add_menu.NodeMenu):
    bl_label = "Mask"

    def draw(self, _context):
        layout = self.layout
        self.node_operator(layout, "CompositorNodeCryptomatteV2")
        self.node_operator(layout, "CompositorNodeCryptomatte")
        layout.separator()
        self.node_operator(layout, "CompositorNodeBoxMask")
        self.node_operator(layout, "CompositorNodeEllipseMask")
        layout.separator()
        self.node_operator(layout, "CompositorNodeDoubleEdgeMask")
        self.node_operator(layout, "CompositorNodeIDMask")

        self.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_compositor_node_tracking_base(node_add_menu.NodeMenu):
    bl_label = "Tracking"
    bl_translation_context = i18n_contexts.id_movieclip

    def draw(self, _context):
        layout = self.layout
        self.node_operator(layout, "CompositorNodePlaneTrackDeform")
        self.node_operator(layout, "CompositorNodeStabilize")
        self.node_operator(layout, "CompositorNodeTrackPos")

        self.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_compositor_node_transform_base(node_add_menu.NodeMenu):
    bl_label = "Transform"

    def draw(self, _context):
        layout = self.layout
        self.node_operator(layout, "CompositorNodeRotate")
        self.node_operator(layout, "CompositorNodeScale")
        self.node_operator(layout, "CompositorNodeTransform")
        self.node_operator(layout, "CompositorNodeTranslate")
        layout.separator()
        self.node_operator(layout, "CompositorNodeCornerPin")
        self.node_operator(layout, "CompositorNodeCrop")
        layout.separator()
        self.node_operator(layout, "CompositorNodeDisplace")
        self.node_operator(layout, "CompositorNodeFlip")
        self.node_operator(layout, "CompositorNodeMapUV")
        layout.separator()
        self.node_operator(layout, "CompositorNodeLensdist")
        self.node_operator(layout, "CompositorNodeMovieDistortion")

        self.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_compositor_node_texture_base(node_add_menu.NodeMenu):
    bl_label = "Texture"

    def draw(self, _context):
        layout = self.layout

        self.node_operator(layout, "ShaderNodeTexBrick")
        self.node_operator(layout, "ShaderNodeTexChecker")
        self.node_operator(layout, "ShaderNodeTexGabor")
        self.node_operator(layout, "ShaderNodeTexGradient")
        self.node_operator(layout, "ShaderNodeTexMagic")
        self.node_operator(layout, "ShaderNodeTexNoise")
        self.node_operator(layout, "ShaderNodeTexVoronoi")
        self.node_operator(layout, "ShaderNodeTexWave")
        self.node_operator(layout, "ShaderNodeTexWhiteNoise")

        self.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_compositor_node_utilities_base(node_add_menu.NodeMenu):
    bl_label = "Utilities"

    def draw(self, context):
        del context
        layout = self.layout
        self.draw_menu(layout, path="Utilities/Math")
        self.draw_menu(layout, path="Utilities/Vector")
        layout.separator()
        self.node_operator(layout, "CompositorNodeLevels")
        self.node_operator(layout, "CompositorNodeNormalize")
        layout.separator()
        self.node_operator(layout, "CompositorNodeSplit")
        self.node_operator(layout, "CompositorNodeSwitch")
        self.node_operator(layout, "GeometryNodeIndexSwitch")
        self.node_operator(layout, "GeometryNodeMenuSwitch")
        self.node_operator(
            layout, "CompositorNodeSwitchView",
            label="Switch Stereo View")
        layout.separator()
        self.node_operator(layout, "CompositorNodeRelativeToPixel")

        self.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_compositor_node_vector_base(node_add_menu.NodeMenu):
    bl_label = "Vector"
    menu_path = "Utilities/Vector"

    def draw(self, context):
        layout = self.layout
        self.node_operator(layout, "ShaderNodeCombineXYZ")
        props = self.node_operator(layout, "ShaderNodeMapRange")
        ops = props.settings.add()
        ops.name = "data_type"
        ops.value = "'FLOAT_VECTOR'"
        props = self.node_operator(layout, "ShaderNodeMix", label="Mix Vector")
        ops = props.settings.add()
        ops.name = "data_type"
        ops.value = "'VECTOR'"
        self.node_operator(layout, "ShaderNodeSeparateXYZ")
        layout.separator()
        self.node_operator(layout, "ShaderNodeRadialTiling")
        self.node_operator(layout, "ShaderNodeVectorCurve")
        self.node_operator_with_searchable_enum(context, layout, "ShaderNodeVectorMath", "operation")
        self.node_operator(layout, "ShaderNodeVectorRotate")

        self.draw_assets_for_catalog(layout, self.menu_path)


class NODE_MT_compositor_node_math_base(node_add_menu.NodeMenu):
    bl_label = "Math"
    menu_path = "Utilities/Math"

    def draw(self, context):
        layout = self.layout

        self.node_operator(layout, "ShaderNodeClamp")
        self.node_operator(layout, "ShaderNodeFloatCurve")
        self.node_operator(layout, "ShaderNodeMapRange")
        self.node_operator_with_searchable_enum(context, layout, "ShaderNodeMath", "operation")
        self.node_operator(layout, "ShaderNodeMix")

        self.draw_assets_for_catalog(layout, self.menu_path)


class NODE_MT_compositor_node_creative_base(node_add_menu.NodeMenu):
    bl_label = "Creative"

    def draw(self, _context):
        layout = self.layout
        self.node_operator(layout, "CompositorNodeKuwahara")
        self.node_operator(layout, "CompositorNodePixelate")
        self.node_operator(layout, "CompositorNodePosterize")

        self.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_compositor_node_all_base(node_add_menu.NodeMenu):
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
        self.draw_menu(layout, "Creative")
        self.draw_menu(layout, "Filter")
        layout.separator()
        self.draw_menu(layout, "Keying")
        self.draw_menu(layout, "Mask")
        layout.separator()
        self.draw_menu(layout, "Tracking")
        layout.separator()
        self.draw_menu(layout, "Texture")
        self.draw_menu(layout, "Transform")
        self.draw_menu(layout, "Utilities")
        layout.separator()
        self.draw_root_assets(layout)
        layout.separator()
        self.draw_menu(layout, "Group")
        self.draw_menu(layout, "Layout")


add_menus = {
    # menu bl_idname: baseclass
    "NODE_MT_category_compositor_input": NODE_MT_compositor_node_input_base,
    "NODE_MT_category_compositor_input_constant": NODE_MT_compositor_node_input_constant_base,
    "NODE_MT_category_compositor_input_scene": NODE_MT_compositor_node_input_scene_base,
    "NODE_MT_category_compositor_output": NODE_MT_compositor_node_output_base,
    "NODE_MT_category_compositor_color": NODE_MT_compositor_node_color_base,
    "NODE_MT_category_compositor_color_adjust": NODE_MT_compositor_node_color_adjust_base,
    "NODE_MT_category_compositor_filter": NODE_MT_compositor_node_filter_base,
    "NODE_MT_category_compositor_filter_blur": NODE_MT_compositor_node_filter_blur_base,
    "NODE_MT_category_compositor_creative": NODE_MT_compositor_node_creative_base,
    "NODE_MT_category_compositor_texture": NODE_MT_compositor_node_texture_base,
    "NODE_MT_category_compositor_keying": NODE_MT_compositor_node_keying_base,
    "NODE_MT_category_compositor_mask": NODE_MT_compositor_node_mask_base,
    "NODE_MT_category_compositor_tracking": NODE_MT_compositor_node_tracking_base,
    "NODE_MT_category_compositor_transform": NODE_MT_compositor_node_transform_base,
    "NODE_MT_category_compositor_utilities": NODE_MT_compositor_node_utilities_base,
    "NODE_MT_category_compositor_vector": NODE_MT_compositor_node_vector_base,
    "NODE_MT_category_compositor_math": NODE_MT_compositor_node_math_base,
    "NODE_MT_compositor_node_add_all": NODE_MT_compositor_node_all_base,
}
add_menus = node_add_menu.generate_menus(
    add_menus,
    template=node_add_menu.AddNodeMenu,
    base_dict=node_add_menu.add_base_pathing_dict
)


swap_menus = {
    # menu bl_idname: baseclass
    "NODE_MT_compositor_node_input_swap": NODE_MT_compositor_node_input_base,
    "NODE_MT_compositor_node_input_constant_swap": NODE_MT_compositor_node_input_constant_base,
    "NODE_MT_compositor_node_input_scene_swap": NODE_MT_compositor_node_input_scene_base,
    "NODE_MT_compositor_node_output_swap": NODE_MT_compositor_node_output_base,
    "NODE_MT_compositor_node_color_swap": NODE_MT_compositor_node_color_base,
    "NODE_MT_compositor_node_color_adjust_swap": NODE_MT_compositor_node_color_adjust_base,
    "NODE_MT_compositor_node_filter_swap": NODE_MT_compositor_node_filter_base,
    "NODE_MT_compositor_node_filter_blur_swap": NODE_MT_compositor_node_filter_blur_base,
    "NODE_MT_category_compositor_creative_swap": NODE_MT_compositor_node_creative_base,
    "NODE_MT_compositor_node_texture_swap": NODE_MT_compositor_node_texture_base,
    "NODE_MT_compositor_node_keying_swap": NODE_MT_compositor_node_keying_base,
    "NODE_MT_compositor_node_mask_swap": NODE_MT_compositor_node_mask_base,
    "NODE_MT_compositor_node_tracking_swap": NODE_MT_compositor_node_tracking_base,
    "NODE_MT_compositor_node_transform_swap": NODE_MT_compositor_node_transform_base,
    "NODE_MT_compositor_node_utilities_swap": NODE_MT_compositor_node_utilities_base,
    "NODE_MT_compositor_node_vector_swap": NODE_MT_compositor_node_vector_base,
    "NODE_MT_compositor_node_math_swap": NODE_MT_compositor_node_math_base,
    "NODE_MT_compositor_node_swap_all": NODE_MT_compositor_node_all_base,
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
