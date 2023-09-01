# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Menu
from bl_ui import node_add_menu
from nodeitems_builtins import (
    eevee_cycles_shader_nodes_poll,
    line_style_shader_nodes_poll,
    object_cycles_shader_nodes_poll,
    object_eevee_cycles_shader_nodes_poll,
    object_eevee_shader_nodes_poll,
    world_shader_nodes_poll,
)
from bpy.app.translations import (
    pgettext_iface as iface_,
)


class NODE_MT_category_shader_input(Menu):
    bl_idname = "NODE_MT_category_shader_input"
    bl_label = "Input"

    def draw(self, context):
        layout = self.layout

        node_add_menu.add_node_type(layout, "ShaderNodeAmbientOcclusion")
        node_add_menu.add_node_type(layout, "ShaderNodeAttribute")
        node_add_menu.add_node_type(layout, "ShaderNodeBevel")
        node_add_menu.add_node_type(layout, "ShaderNodeCameraData")
        node_add_menu.add_node_type(layout, "ShaderNodeVertexColor")
        node_add_menu.add_node_type(layout, "ShaderNodeHairInfo")
        node_add_menu.add_node_type(layout, "ShaderNodeFresnel")
        node_add_menu.add_node_type(layout, "ShaderNodeNewGeometry")
        node_add_menu.add_node_type(layout, "ShaderNodeLayerWeight")
        node_add_menu.add_node_type(layout, "ShaderNodeLightPath")
        node_add_menu.add_node_type(layout, "ShaderNodeObjectInfo")
        node_add_menu.add_node_type(layout, "ShaderNodeParticleInfo")
        node_add_menu.add_node_type(layout, "ShaderNodePointInfo")
        node_add_menu.add_node_type(layout, "ShaderNodeRGB")
        node_add_menu.add_node_type(layout, "ShaderNodeTangent")
        node_add_menu.add_node_type(layout, "ShaderNodeTexCoord")
        node_add_menu.add_node_type(layout, "ShaderNodeUVAlongStroke", poll=line_style_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeUVMap")
        node_add_menu.add_node_type(layout, "ShaderNodeValue")
        node_add_menu.add_node_type(layout, "ShaderNodeVolumeInfo")
        node_add_menu.add_node_type(layout, "ShaderNodeWireframe")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_shader_output(Menu):
    bl_idname = "NODE_MT_category_shader_output"
    bl_label = "Output"

    def draw(self, context):
        layout = self.layout

        node_add_menu.add_node_type(layout, "ShaderNodeOutputMaterial", poll=object_eevee_cycles_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeOutputLight", poll=object_cycles_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeOutputAOV"),
        node_add_menu.add_node_type(layout, "ShaderNodeOutputWorld", poll=world_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeOutputLineStyle", poll=line_style_shader_nodes_poll(context)),

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_shader_shader(Menu):
    bl_idname = "NODE_MT_category_shader_shader"
    bl_label = "Shader"

    def draw(self, context):
        layout = self.layout

        node_add_menu.add_node_type(layout, "ShaderNodeAddShader", poll=eevee_cycles_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeBackground", poll=world_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeBsdfDiffuse", poll=object_eevee_cycles_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeEmission", poll=eevee_cycles_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeBsdfGlass", poll=object_eevee_cycles_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeBsdfGlossy", poll=object_eevee_cycles_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeBsdfHair", poll=object_cycles_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeHoldout", poll=object_eevee_cycles_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeMixShader", poll=eevee_cycles_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeBsdfPrincipled", poll=object_eevee_cycles_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeBsdfHairPrincipled", poll=object_cycles_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeVolumePrincipled"),
        node_add_menu.add_node_type(layout, "ShaderNodeBsdfRefraction", poll=object_eevee_cycles_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeBsdfSheen", poll=object_cycles_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeEeveeSpecular", poll=object_eevee_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeSubsurfaceScattering", poll=object_eevee_cycles_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeBsdfToon", poll=object_cycles_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeBsdfTranslucent", poll=object_eevee_cycles_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeBsdfTransparent", poll=object_eevee_cycles_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeVolumeAbsorption", poll=eevee_cycles_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeVolumeScatter", poll=eevee_cycles_shader_nodes_poll(context)),

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_category_shader_color(Menu):
    bl_idname = "NODE_MT_category_shader_color"
    bl_label = "Color"

    def draw(self, _context):
        layout = self.layout

        node_add_menu.add_node_type(layout, "ShaderNodeBrightContrast")
        node_add_menu.add_node_type(layout, "ShaderNodeGamma")
        node_add_menu.add_node_type(layout, "ShaderNodeHueSaturation")
        node_add_menu.add_node_type(layout, "ShaderNodeInvert")
        node_add_menu.add_node_type(layout, "ShaderNodeLightFalloff")
        props = node_add_menu.add_node_type(layout, "ShaderNodeMix", label=iface_("Mix Color"))
        ops = props.settings.add()
        ops.name = "data_type"
        ops.value = "'RGBA'"
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
        node_add_menu.add_node_type(layout, "ShaderNodeMath")
        node_add_menu.add_node_type(layout, "ShaderNodeMix")
        node_add_menu.add_node_type(layout, "ShaderNodeRGBToBW")
        node_add_menu.add_node_type(layout, "ShaderNodeSeparateColor")
        node_add_menu.add_node_type(layout, "ShaderNodeSeparateXYZ")
        node_add_menu.add_node_type(layout, "ShaderNodeShaderToRGB", poll=object_eevee_shader_nodes_poll(context)),
        node_add_menu.add_node_type(layout, "ShaderNodeVectorMath")
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
        node_add_menu.add_node_type(layout, "ShaderNodeTexGradient")
        node_add_menu.add_node_type(layout, "ShaderNodeTexIES")
        node_add_menu.add_node_type(layout, "ShaderNodeTexImage")
        node_add_menu.add_node_type(layout, "ShaderNodeTexMagic")
        node_add_menu.add_node_type(layout, "ShaderNodeTexMusgrave")
        node_add_menu.add_node_type(layout, "ShaderNodeTexNoise")
        node_add_menu.add_node_type(layout, "ShaderNodeTexPointDensity")
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
