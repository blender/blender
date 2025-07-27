# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import Menu
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


class NODE_MT_category_shader_input(Menu):
    bl_idname = "NODE_MT_category_shader_input"
    bl_label = "Input"

    def draw(self, context):
        layout = self.layout

        node_add_menu.add_node_type(layout, "ShaderNodeAmbientOcclusion")
        node_add_menu.add_node_type(layout, "ShaderNodeAttribute")
        node_add_menu.add_node_type(layout, "ShaderNodeBevel")
        node_add_menu.add_node_type_with_outputs(
            context, layout, "ShaderNodeCameraData",
            ["View Vector", "View Z Depth", "View Distance"],
        )
        node_add_menu.add_node_type(layout, "ShaderNodeVertexColor")
        node_add_menu.add_node_type_with_outputs(
            context, layout, "ShaderNodeHairInfo",
            ["Is Strand", "Intercept", "Length", "Thickness", "Tangent Normal", "Random"],
        )
        node_add_menu.add_node_type(layout, "ShaderNodeFresnel")
        node_add_menu.add_node_type_with_outputs(
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
        node_add_menu.add_node_type(layout, "ShaderNodeLayerWeight")
        node_add_menu.add_node_type_with_outputs(
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
        node_add_menu.add_node_type_with_outputs(
            context, layout, "ShaderNodeObjectInfo",
            ["Location", "Color", "Alpha", "Object Index", "Material Index", "Random"],
        )
        node_add_menu.add_node_type_with_outputs(
            context, layout, "ShaderNodeParticleInfo",
            ["Index", "Random", "Age", "Lifetime", "Location", "Size", "Velocity", "Angular Velocity"],
        )
        node_add_menu.add_node_type_with_outputs(
            context, layout, "ShaderNodePointInfo",
            ["Position", "Radius", "Random"],
        )
        node_add_menu.add_node_type(layout, "ShaderNodeRGB")
        node_add_menu.add_node_type(layout, "ShaderNodeTangent")
        node_add_menu.add_node_type_with_outputs(
            context, layout, "ShaderNodeTexCoord",
            ["Normal", "UV", "Object", "Camera", "Window", "Reflection"],
        )
        node_add_menu.add_node_type(layout, "ShaderNodeUVAlongStroke", poll=line_style_shader_nodes_poll(context))
        node_add_menu.add_node_type(layout, "ShaderNodeUVMap")
        node_add_menu.add_node_type(layout, "ShaderNodeValue")
        node_add_menu.add_node_type_with_outputs(
            context, layout, "ShaderNodeVolumeInfo",
            ["Color", "Density", "Flame", "Temperature"],
        )
        node_add_menu.add_node_type(layout, "ShaderNodeWireframe")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_shader_output(Menu):
    bl_idname = "NODE_MT_category_shader_output"
    bl_label = "Output"

    def draw(self, context):
        layout = self.layout

        node_add_menu.add_node_type(
            layout,
            "ShaderNodeOutputAOV",
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeOutputLight",
            poll=object_not_eevee_shader_nodes_poll(context),
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeOutputLineStyle",
            poll=line_style_shader_nodes_poll(context),
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeOutputMaterial",
            poll=object_shader_nodes_poll(context),
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeOutputWorld",
            poll=world_shader_nodes_poll(context),
        )

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_shader_shader(Menu):
    bl_idname = "NODE_MT_category_shader_shader"
    bl_label = "Shader"

    def draw(self, context):
        layout = self.layout

        node_add_menu.add_node_type(
            layout,
            "ShaderNodeAddShader",
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeBackground",
            poll=world_shader_nodes_poll(context),
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeBsdfDiffuse",
            poll=object_shader_nodes_poll(context),
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeEmission",
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeBsdfGlass",
            poll=object_shader_nodes_poll(context),
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeBsdfGlossy",
            poll=object_shader_nodes_poll(context),
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeBsdfHair",
            poll=object_not_eevee_shader_nodes_poll(context),
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeHoldout",
            poll=object_shader_nodes_poll(context),
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeBsdfMetallic",
            poll=object_shader_nodes_poll(context),
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeMixShader",
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeBsdfPrincipled",
            poll=object_shader_nodes_poll(context),
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeBsdfHairPrincipled",
            poll=object_not_eevee_shader_nodes_poll(context),
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeVolumePrincipled"
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeBsdfRayPortal",
            poll=object_not_eevee_shader_nodes_poll(context),
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeBsdfRefraction",
            poll=object_shader_nodes_poll(context),
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeBsdfSheen",
            poll=object_not_eevee_shader_nodes_poll(context),
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeEeveeSpecular",
            poll=object_eevee_shader_nodes_poll(context),
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeSubsurfaceScattering",
            poll=object_shader_nodes_poll(context),
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeBsdfToon",
            poll=object_not_eevee_shader_nodes_poll(context),
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeBsdfTranslucent",
            poll=object_shader_nodes_poll(context),
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeBsdfTransparent",
            poll=object_shader_nodes_poll(context),
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeVolumeAbsorption",
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeVolumeScatter",
        )
        node_add_menu.add_node_type(
            layout,
            "ShaderNodeVolumeCoefficients",
        )

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_shader_color(Menu):
    bl_idname = "NODE_MT_category_shader_color"
    bl_label = "Color"

    def draw(self, context):
        layout = self.layout

        node_add_menu.add_node_type(layout, "ShaderNodeBrightContrast")
        node_add_menu.add_node_type(layout, "ShaderNodeGamma")
        node_add_menu.add_node_type(layout, "ShaderNodeHueSaturation")
        node_add_menu.add_node_type(layout, "ShaderNodeInvert")
        node_add_menu.add_node_type(layout, "ShaderNodeLightFalloff")
        node_add_menu.add_color_mix_node(context, layout)
        node_add_menu.add_node_type(layout, "ShaderNodeRGBCurve")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_shader_converter(Menu):
    bl_idname = "NODE_MT_category_shader_converter"
    bl_label = "Converter"

    def draw(self, context):
        layout = self.layout

        node_add_menu.add_node_type(layout, "ShaderNodeBlackbody")
        node_add_menu.add_node_type(layout, "ShaderNodeClamp")
        node_add_menu.add_node_type(layout, "ShaderNodeValToRGB")
        node_add_menu.add_node_type(layout, "ShaderNodeCombineColor")
        node_add_menu.add_node_type(layout, "ShaderNodeCombineXYZ")
        node_add_menu.add_node_type(layout, "ShaderNodeFloatCurve")
        node_add_menu.add_node_type(layout, "ShaderNodeMapRange")
        node_add_menu.add_node_type_with_searchable_enum(context, layout, "ShaderNodeMath", "operation")
        node_add_menu.add_node_type(layout, "ShaderNodeMix")
        node_add_menu.add_node_type(layout, "ShaderNodeRGBToBW")
        node_add_menu.add_node_type(layout, "ShaderNodeSeparateColor")
        node_add_menu.add_node_type(layout, "ShaderNodeSeparateXYZ")
        node_add_menu.add_node_type(layout, "ShaderNodeShaderToRGB", poll=object_eevee_shader_nodes_poll(context))
        node_add_menu.add_node_type_with_searchable_enum(context, layout, "ShaderNodeVectorMath", "operation")
        node_add_menu.add_node_type(layout, "ShaderNodeWavelength")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_shader_texture(Menu):
    bl_idname = "NODE_MT_category_shader_texture"
    bl_label = "Texture"

    def draw(self, _context):
        layout = self.layout

        node_add_menu.add_node_type(layout, "ShaderNodeTexBrick")
        node_add_menu.add_node_type(layout, "ShaderNodeTexChecker")
        node_add_menu.add_node_type(layout, "ShaderNodeTexEnvironment")
        node_add_menu.add_node_type(layout, "ShaderNodeTexGabor")
        node_add_menu.add_node_type(layout, "ShaderNodeTexGradient")
        node_add_menu.add_node_type(layout, "ShaderNodeTexIES")
        node_add_menu.add_node_type(layout, "ShaderNodeTexImage")
        node_add_menu.add_node_type(layout, "ShaderNodeTexMagic")
        node_add_menu.add_node_type(layout, "ShaderNodeTexNoise")
        node_add_menu.add_node_type(layout, "ShaderNodeTexSky")
        node_add_menu.add_node_type(layout, "ShaderNodeTexVoronoi")
        node_add_menu.add_node_type(layout, "ShaderNodeTexWave")
        node_add_menu.add_node_type(layout, "ShaderNodeTexWhiteNoise")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_shader_vector(Menu):
    bl_idname = "NODE_MT_category_shader_vector"
    bl_label = "Vector"

    def draw(self, _context):
        layout = self.layout

        node_add_menu.add_node_type(layout, "ShaderNodeBump")
        node_add_menu.add_node_type(layout, "ShaderNodeDisplacement")
        node_add_menu.add_node_type(layout, "ShaderNodeMapping")
        node_add_menu.add_node_type(layout, "ShaderNodeNormal")
        node_add_menu.add_node_type(layout, "ShaderNodeNormalMap")
        node_add_menu.add_node_type(layout, "ShaderNodeVectorCurve")
        node_add_menu.add_node_type(layout, "ShaderNodeVectorDisplacement")
        node_add_menu.add_node_type(layout, "ShaderNodeVectorRotate")
        node_add_menu.add_node_type(layout, "ShaderNodeVectorTransform")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_shader_script(Menu):
    bl_idname = "NODE_MT_category_shader_script"
    bl_label = "Script"

    def draw(self, _context):
        layout = self.layout

        node_add_menu.add_node_type(layout, "ShaderNodeScript")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_shader_group(Menu):
    bl_idname = "NODE_MT_category_shader_group"
    bl_label = "Group"

    def draw(self, context):
        layout = self.layout
        node_add_menu.draw_node_group_add_menu(context, layout)
        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_shader_node_add_all(Menu):
    bl_idname = "NODE_MT_shader_node_add_all"
    bl_label = "Add"
    bl_translation_context = i18n_contexts.operator_default

    def draw(self, _context):
        layout = self.layout
        layout.menu("NODE_MT_category_shader_input")
        layout.menu("NODE_MT_category_shader_output")
        layout.separator()
        layout.menu("NODE_MT_category_shader_color")
        layout.menu("NODE_MT_category_shader_converter")
        layout.menu("NODE_MT_category_shader_shader")
        layout.menu("NODE_MT_category_shader_texture")
        layout.menu("NODE_MT_category_shader_vector")
        layout.separator()
        layout.menu("NODE_MT_category_shader_script")
        layout.separator()
        layout.menu("NODE_MT_category_shader_group")
        layout.menu("NODE_MT_category_layout")

        node_add_menu.draw_root_assets(layout)


classes = (
    NODE_MT_shader_node_add_all,
    NODE_MT_category_shader_input,
    NODE_MT_category_shader_output,
    NODE_MT_category_shader_color,
    NODE_MT_category_shader_converter,
    NODE_MT_category_shader_shader,
    NODE_MT_category_shader_texture,
    NODE_MT_category_shader_vector,
    NODE_MT_category_shader_script,
    NODE_MT_category_shader_group,
)


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
