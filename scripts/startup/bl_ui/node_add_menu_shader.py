# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bl_ui import node_add_menu
from bpy.app.translations import (
    contexts as i18n_contexts,
)


# only show input/output nodes when editing line style node trees
def line_style_shader_nodes_poll(context):
    snode = context.space_data
    return (snode.tree_type == 'ShaderNodeTree' and
            snode.shader_type == 'LINESTYLE')


# only show nodes working in world node trees
def world_shader_nodes_poll(context):
    snode = context.space_data
    return (snode.tree_type == 'ShaderNodeTree' and
            snode.shader_type == 'WORLD')


# only show nodes working in object node trees
def object_shader_nodes_poll(context):
    snode = context.space_data
    return (snode.tree_type == 'ShaderNodeTree' and
            snode.shader_type == 'OBJECT')


def cycles_shader_nodes_poll(context):
    return context.engine == 'CYCLES'


def eevee_shader_nodes_poll(context):
    return context.engine == 'BLENDER_EEVEE'


def object_not_eevee_shader_nodes_poll(context):
    return (object_shader_nodes_poll(context) and
            not eevee_shader_nodes_poll(context))


def object_eevee_shader_nodes_poll(context):
    return (object_shader_nodes_poll(context) and
            eevee_shader_nodes_poll(context))


class NODE_MT_shader_node_input_base(node_add_menu.NodeMenu):
    bl_label = "Input"

    def draw(self, context):
        layout = self.layout

        self.node_operator(layout, "ShaderNodeAmbientOcclusion")
        self.node_operator(layout, "ShaderNodeAttribute")
        self.node_operator(layout, "ShaderNodeBevel")
        self.node_operator_with_outputs(
            context, layout, "ShaderNodeCameraData",
            ["View Vector", "View Z Depth", "View Distance"],
        )
        self.node_operator(layout, "ShaderNodeVertexColor")
        self.node_operator_with_outputs(
            context, layout, "ShaderNodeHairInfo",
            ["Is Strand", "Intercept", "Length", "Thickness", "Tangent Normal", "Random"],
        )
        self.node_operator(layout, "ShaderNodeFresnel")
        self.node_operator_with_outputs(
            context,
            layout,
            "ShaderNodeNewGeometry",
            [
                "Position",
                "Normal",
                "Tangent",
                "True Normal",
                "Incoming",
                "Parametric",
                "Backfacing",
                "Pointiness",
                "Random Per Island",
            ],
        )
        self.node_operator(layout, "ShaderNodeLayerWeight")
        self.node_operator_with_outputs(
            context,
            layout,
            "ShaderNodeLightPath",
            [
                "Is Camera Ray",
                "Is Shadow Ray",
                "Is Diffuse Ray",
                "Is Glossy Ray",
                "Is Singular Ray",
                "Is Reflection Ray",
                "Is Transmission Ray",
                "Is Volume Scatter Ray",
                "Ray Length",
                "Ray Depth",
                "Diffuse Depth",
                "Glossy Depth",
                "Transparent Depth",
                "Transmission Depth",
                "Portal Depth"
            ],
        )
        self.node_operator_with_outputs(
            context, layout, "ShaderNodeObjectInfo",
            ["Location", "Color", "Alpha", "Object Index", "Material Index", "Random"],
        )
        self.node_operator_with_outputs(
            context, layout, "ShaderNodeParticleInfo",
            ["Index", "Random", "Age", "Lifetime", "Location", "Size", "Velocity", "Angular Velocity"],
        )
        self.node_operator_with_outputs(
            context, layout, "ShaderNodePointInfo",
            ["Position", "Radius", "Random"],
        )
        self.node_operator(layout, "ShaderNodeRGB")
        self.node_operator(layout, "ShaderNodeTangent")
        self.node_operator_with_outputs(
            context, layout, "ShaderNodeTexCoord",
            ["Normal", "UV", "Object", "Camera", "Window", "Reflection"],
        )
        self.node_operator(layout, "ShaderNodeUVAlongStroke", poll=line_style_shader_nodes_poll(context))
        self.node_operator(layout, "ShaderNodeUVMap")
        self.node_operator(layout, "ShaderNodeValue")
        self.node_operator_with_outputs(
            context, layout, "ShaderNodeVolumeInfo",
            ["Color", "Density", "Flame", "Temperature"],
        )
        self.node_operator(layout, "ShaderNodeWireframe")

        self.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_shader_node_output_base(node_add_menu.NodeMenu):
    bl_label = "Output"

    def draw(self, context):
        layout = self.layout

        self.node_operator(
            layout,
            "ShaderNodeOutputAOV",
        )
        self.node_operator(
            layout,
            "ShaderNodeOutputLight",
            poll=object_not_eevee_shader_nodes_poll(context),
        )
        self.node_operator(
            layout,
            "ShaderNodeOutputLineStyle",
            poll=line_style_shader_nodes_poll(context),
        )
        self.node_operator(
            layout,
            "ShaderNodeOutputMaterial",
            poll=object_shader_nodes_poll(context),
        )
        self.node_operator(
            layout,
            "ShaderNodeOutputWorld",
            poll=world_shader_nodes_poll(context),
        )

        self.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_shader_node_shader_base(node_add_menu.NodeMenu):
    bl_label = "Shader"

    def draw(self, context):
        layout = self.layout

        self.node_operator(
            layout,
            "ShaderNodeAddShader",
        )
        self.node_operator(
            layout,
            "ShaderNodeMixShader",
        )

        layout.separator()

        self.node_operator(
            layout,
            "ShaderNodeBackground",
            poll=world_shader_nodes_poll(context),
        )
        self.node_operator(
            layout,
            "ShaderNodeBsdfDiffuse",
            poll=object_shader_nodes_poll(context),
        )
        self.node_operator(
            layout,
            "ShaderNodeEmission",
        )
        self.node_operator(
            layout,
            "ShaderNodeBsdfGlass",
            poll=object_shader_nodes_poll(context),
        )
        self.node_operator(
            layout,
            "ShaderNodeBsdfGlossy",
            poll=object_shader_nodes_poll(context),
        )
        self.node_operator(
            layout,
            "ShaderNodeBsdfHair",
            poll=object_not_eevee_shader_nodes_poll(context),
        )
        self.node_operator(
            layout,
            "ShaderNodeHoldout",
            poll=object_shader_nodes_poll(context),
        )
        self.node_operator(
            layout,
            "ShaderNodeBsdfMetallic",
            poll=object_shader_nodes_poll(context),
        )
        self.node_operator(
            layout,
            "ShaderNodeBsdfPrincipled",
            poll=object_shader_nodes_poll(context),
        )
        self.node_operator(
            layout,
            "ShaderNodeBsdfHairPrincipled",
            poll=object_not_eevee_shader_nodes_poll(context),
        )
        self.node_operator(
            layout,
            "ShaderNodeBsdfRayPortal",
            poll=object_not_eevee_shader_nodes_poll(context),
        )
        self.node_operator(
            layout,
            "ShaderNodeBsdfRefraction",
            poll=object_shader_nodes_poll(context),
        )
        self.node_operator(
            layout,
            "ShaderNodeBsdfSheen",
            poll=object_not_eevee_shader_nodes_poll(context),
        )
        self.node_operator(
            layout,
            "ShaderNodeEeveeSpecular",
            poll=object_eevee_shader_nodes_poll(context),
        )
        self.node_operator(
            layout,
            "ShaderNodeSubsurfaceScattering",
            poll=object_shader_nodes_poll(context),
        )
        self.node_operator(
            layout,
            "ShaderNodeBsdfToon",
            poll=object_not_eevee_shader_nodes_poll(context),
        )
        self.node_operator(
            layout,
            "ShaderNodeBsdfTranslucent",
            poll=object_shader_nodes_poll(context),
        )
        self.node_operator(
            layout,
            "ShaderNodeBsdfTransparent",
            poll=object_shader_nodes_poll(context),
        )

        layout.separator()

        self.node_operator(
            layout,
            "ShaderNodeVolumePrincipled"
        )
        self.node_operator(
            layout,
            "ShaderNodeVolumeAbsorption",
        )
        self.node_operator(
            layout,
            "ShaderNodeVolumeScatter",
        )
        self.node_operator(
            layout,
            "ShaderNodeVolumeCoefficients",
        )

        self.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_shader_node_color_base(node_add_menu.NodeMenu):
    bl_label = "Color"

    def draw(self, context):
        layout = self.layout

        layout.separator()
        self.node_operator(layout, "ShaderNodeBlackbody")
        self.node_operator(layout, "ShaderNodeBrightContrast")
        self.node_operator(layout, "ShaderNodeValToRGB")
        self.node_operator(layout, "ShaderNodeGamma")
        self.node_operator(layout, "ShaderNodeHueSaturation")
        self.node_operator(layout, "ShaderNodeInvert")
        self.node_operator(layout, "ShaderNodeLightFalloff")
        self.color_mix_node(context, layout)
        self.node_operator(layout, "ShaderNodeRGBCurve")
        self.node_operator(layout, "ShaderNodeWavelength")
        layout.separator()
        self.node_operator(layout, "ShaderNodeCombineColor")
        self.node_operator(layout, "ShaderNodeSeparateColor")
        layout.separator()
        self.node_operator(layout, "ShaderNodeRGBToBW")
        self.node_operator(layout, "ShaderNodeShaderToRGB", poll=object_eevee_shader_nodes_poll(context))

        self.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_shader_node_texture_base(node_add_menu.NodeMenu):
    bl_label = "Texture"

    def draw(self, _context):
        layout = self.layout

        self.node_operator(layout, "ShaderNodeTexBrick")
        self.node_operator(layout, "ShaderNodeTexChecker")
        self.node_operator(layout, "ShaderNodeTexEnvironment")
        self.node_operator(layout, "ShaderNodeTexGabor")
        self.node_operator(layout, "ShaderNodeTexGradient")
        self.node_operator(layout, "ShaderNodeTexIES")
        self.node_operator(layout, "ShaderNodeTexImage")
        self.node_operator(layout, "ShaderNodeTexMagic")
        self.node_operator(layout, "ShaderNodeTexNoise")
        self.node_operator(layout, "ShaderNodeTexSky")
        self.node_operator(layout, "ShaderNodeTexVoronoi")
        self.node_operator(layout, "ShaderNodeTexWave")
        self.node_operator(layout, "ShaderNodeTexWhiteNoise")

        self.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_shader_node_vector_base(node_add_menu.NodeMenu):
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
        self.node_operator(layout, "ShaderNodeMapping")
        self.node_operator(layout, "ShaderNodeNormal")
        self.node_operator(layout, "ShaderNodeRadialTiling")
        self.node_operator(layout, "ShaderNodeVectorCurve")
        self.node_operator_with_searchable_enum(context, layout, "ShaderNodeVectorMath", "operation")
        self.node_operator(layout, "ShaderNodeVectorRotate")
        self.node_operator(layout, "ShaderNodeVectorTransform")

        self.draw_assets_for_catalog(layout, self.menu_path)


class NODE_MT_shader_node_math_base(node_add_menu.NodeMenu):
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


class NODE_MT_shader_node_displacement_base(node_add_menu.NodeMenu):
    bl_label = "Displacement"

    def draw(self, _context):
        layout = self.layout

        self.node_operator(layout, "ShaderNodeBump")
        self.node_operator(layout, "ShaderNodeDisplacement")
        self.node_operator(layout, "ShaderNodeNormalMap")
        self.node_operator(layout, "ShaderNodeVectorDisplacement")

        self.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_shader_node_utilities_base(node_add_menu.NodeMenu):
    bl_label = "Utilities"

    def draw(self, context):
        layout = self.layout

        self.draw_menu(layout, "Utilities/Math")
        self.draw_menu(layout, "Utilities/Vector")
        layout.separator()
        self.repeat_zone(layout, label="Repeat")
        layout.separator()
        self.closure_zone(layout, label="Closure")
        self.node_operator(layout, "NodeEvaluateClosure")
        self.node_operator(layout, "NodeCombineBundle")
        self.node_operator(layout, "NodeSeparateBundle")
        layout.separator()
        self.node_operator(layout, "GeometryNodeMenuSwitch")
        if cycles_shader_nodes_poll(context):
            layout.separator()
            self.node_operator(layout, "ShaderNodeScript")

        self.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_shader_node_all_base(node_add_menu.NodeMenu):
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
        # Do not order this alphabetically, we are matching the order in the output node.
        self.draw_menu(layout, "Shader")
        self.draw_menu(layout, "Displacement")
        layout.separator()
        self.draw_menu(layout, "Color")
        self.draw_menu(layout, "Texture")
        self.draw_menu(layout, "Utilities")
        layout.separator()
        self.draw_root_assets(layout)
        layout.separator()
        self.draw_menu(layout, "Group")
        self.draw_menu(layout, "Layout")


add_menus = {
    # menu bl_idname: baseclass
    "NODE_MT_category_shader_input": NODE_MT_shader_node_input_base,
    "NODE_MT_category_shader_output": NODE_MT_shader_node_output_base,
    "NODE_MT_category_shader_color": NODE_MT_shader_node_color_base,
    "NODE_MT_category_shader_shader": NODE_MT_shader_node_shader_base,
    "NODE_MT_category_shader_texture": NODE_MT_shader_node_texture_base,
    "NODE_MT_category_shader_displacement": NODE_MT_shader_node_displacement_base,
    "NODE_MT_category_shader_vector": NODE_MT_shader_node_vector_base,
    "NODE_MT_category_shader_math": NODE_MT_shader_node_math_base,
    "NODE_MT_category_shader_utilities": NODE_MT_shader_node_utilities_base,
    "NODE_MT_shader_node_add_all": NODE_MT_shader_node_all_base,
}
add_menus = node_add_menu.generate_menus(
    add_menus,
    template=node_add_menu.AddNodeMenu,
    base_dict=node_add_menu.add_base_pathing_dict
)


swap_menus = {
    # menu bl_idname: baseclass
    "NODE_MT_shader_node_input_swap": NODE_MT_shader_node_input_base,
    "NODE_MT_shader_node_output_swap": NODE_MT_shader_node_output_base,
    "NODE_MT_shader_node_color_swap": NODE_MT_shader_node_color_base,
    "NODE_MT_shader_node_shader_swap": NODE_MT_shader_node_shader_base,
    "NODE_MT_shader_node_texture_swap": NODE_MT_shader_node_texture_base,
    "NODE_MT_shader_node_displacement_swap": NODE_MT_shader_node_displacement_base,
    "NODE_MT_shader_node_vector_swap": NODE_MT_shader_node_vector_base,
    "NODE_MT_shader_node_math_swap": NODE_MT_shader_node_math_base,
    "NODE_MT_shader_node_utilities_swap": NODE_MT_shader_node_utilities_base,
    "NODE_MT_shader_node_swap_all": NODE_MT_shader_node_all_base,
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
