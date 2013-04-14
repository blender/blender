# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy
import nodeitems_utils
from nodeitems_utils import NodeCategory, NodeItem


# Subclasses for standard node types

class CompositorNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        return context.space_data.tree_type == 'CompositorNodeTree'

class ShaderNewNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        return context.space_data.tree_type == 'ShaderNodeTree' and \
               context.scene.render.use_shading_nodes

class ShaderOldNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        return context.space_data.tree_type == 'ShaderNodeTree' and \
               not context.scene.render.use_shading_nodes

class TextureNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        return context.space_data.tree_type == 'TextureNodeTree'


def compositor_node_group_items(self):
    return [NodeItem('CompositorNodeGroup', group.name, { "node_tree" : "bpy.data.node_groups['%s']" % group.name })
            for group in bpy.data.node_groups if group.bl_idname == 'CompositorNodeTree']

# Note: node groups not distinguished by old/new shader nodes
def shader_node_group_items(self):
    return [NodeItem('ShaderNodeGroup', group.name, { "node_tree" : "bpy.data.node_groups['%s']" % group.name })
            for group in bpy.data.node_groups if group.bl_idname == 'ShaderNodeTree']

def texture_node_group_items(self):
    return [NodeItem('TextureNodeGroup', group.name, { "node_tree" : "bpy.data.node_groups['%s']" % group.name })
            for group in bpy.data.node_groups if group.bl_idname == 'TextureNodeTree']


# All standard node categories currently used in nodes.

std_node_categories = [
    # Shader Nodes
    ShaderOldNodeCategory("SH_INPUT", "Input", items=[
        NodeItem("ShaderNodeMaterial"),
        NodeItem("ShaderNodeCameraData"),
        NodeItem("ShaderNodeValue"),
        NodeItem("ShaderNodeRGB"),
        NodeItem("ShaderNodeTexture"),
        NodeItem("ShaderNodeGeometry"),
        NodeItem("ShaderNodeExtendedMaterial"),
        ]),
    ShaderOldNodeCategory("SH_OUTPUT", "Output", items=[
        NodeItem("ShaderNodeOutput"),
        ]),
    ShaderOldNodeCategory("SH_OP_COLOR", "Color", items=[
        NodeItem("ShaderNodeMixRGB"),
        NodeItem("ShaderNodeRGBCurve"),
        NodeItem("ShaderNodeInvert"),
        NodeItem("ShaderNodeHueSaturation"),
        ]),
    ShaderOldNodeCategory("SH_OP_VECTOR", "Vector", items=[
        NodeItem("ShaderNodeNormal"),
        NodeItem("ShaderNodeMapping"),
        NodeItem("ShaderNodeVectorCurve"),
        ]),
    ShaderOldNodeCategory("SH_CONVERTOR", "Converter", items=[
        NodeItem("ShaderNodeValToRGB"),
        NodeItem("ShaderNodeRGBToBW"),
        NodeItem("ShaderNodeMath"),
        NodeItem("ShaderNodeVectorMath"),
        NodeItem("ShaderNodeSqueeze"),
        NodeItem("ShaderNodeSeparateRGB"),
        NodeItem("ShaderNodeCombineRGB"),
        ]),
    ShaderOldNodeCategory("SH_SCRIPT", "Script", items=[
        ]),
    ShaderOldNodeCategory("SH_GROUP", "Group", items=shader_node_group_items),
    ShaderOldNodeCategory("SH_LAYOUT", "Layout", items=[
        ]),

    # New Shader Nodes (Cycles)
    ShaderNewNodeCategory("SH_NEW_INPUT", "Input", items=[
        NodeItem("ShaderNodeTexCoord"),
        NodeItem("ShaderNodeAttribute"),
        NodeItem("ShaderNodeLightPath"),
        NodeItem("ShaderNodeFresnel"),
        NodeItem("ShaderNodeLayerWeight"),
        NodeItem("ShaderNodeRGB"),
        NodeItem("ShaderNodeValue"),
        NodeItem("ShaderNodeTangent"),
        NodeItem("ShaderNodeNewGeometry"),
        NodeItem("ShaderNodeObjectInfo"),
        NodeItem("ShaderNodeHairInfo"),
        NodeItem("ShaderNodeParticleInfo"),
        NodeItem("ShaderNodeCameraData"),
        ]),
    ShaderNewNodeCategory("SH_NEW_OUTPUT", "Output", items=[
        NodeItem("ShaderNodeOutputMaterial"),
        NodeItem("ShaderNodeOutputLamp"),
        NodeItem("ShaderNodeOutputWorld"),
        ]),
    ShaderNewNodeCategory("SH_NEW_SHADER", "Shader", items=[
        NodeItem("ShaderNodeMixShader"),
        NodeItem("ShaderNodeAddShader"),
        NodeItem("ShaderNodeBsdfDiffuse"),
        NodeItem("ShaderNodeBsdfGlossy"),
        NodeItem("ShaderNodeBsdfTransparent"),
        NodeItem("ShaderNodeBsdfRefraction"),
        NodeItem("ShaderNodeBsdfGlass"),
        NodeItem("ShaderNodeBsdfTranslucent"),
        NodeItem("ShaderNodeBsdfAnisotropic"),
        NodeItem("ShaderNodeBsdfVelvet"),
        NodeItem("ShaderNodeSubsurfaceScattering"),
        NodeItem("ShaderNodeEmission"),
        NodeItem("ShaderNodeBackground"),
        NodeItem("ShaderNodeAmbientOcclusion"),
        NodeItem("ShaderNodeHoldout"),
        ]),
    ShaderNewNodeCategory("SH_NEW_TEXTURE", "Texture", items=[
        NodeItem("ShaderNodeTexImage"),
        NodeItem("ShaderNodeTexEnvironment"),
        NodeItem("ShaderNodeTexSky"),
        NodeItem("ShaderNodeTexNoise"),
        NodeItem("ShaderNodeTexWave"),
        NodeItem("ShaderNodeTexVoronoi"),
        NodeItem("ShaderNodeTexMusgrave"),
        NodeItem("ShaderNodeTexGradient"),
        NodeItem("ShaderNodeTexMagic"),
        NodeItem("ShaderNodeTexChecker"),
        NodeItem("ShaderNodeTexBrick"),
        ]),
    ShaderNewNodeCategory("SH_NEW_OP_COLOR", "Color", items=[
        NodeItem("ShaderNodeMixRGB"),
        NodeItem("ShaderNodeRGBCurve"),
        NodeItem("ShaderNodeInvert"),
        NodeItem("ShaderNodeLightFalloff"),
        NodeItem("ShaderNodeHueSaturation"),
        NodeItem("ShaderNodeGamma"),
        NodeItem("ShaderNodeBrightContrast"),
        ]),
    ShaderNewNodeCategory("SH_NEW_OP_VECTOR", "Vector", items=[
        NodeItem("ShaderNodeMapping"),
        NodeItem("ShaderNodeBump"),
        NodeItem("ShaderNodeNormalMap"),
        NodeItem("ShaderNodeNormal"),
        NodeItem("ShaderNodeVectorCurve"),
        ]),
    ShaderNewNodeCategory("SH_NEW_CONVERTOR", "Converter", items=[
        NodeItem("ShaderNodeMath"),
        NodeItem("ShaderNodeValToRGB"),
        NodeItem("ShaderNodeRGBToBW"),
        NodeItem("ShaderNodeVectorMath"),
        NodeItem("ShaderNodeSeparateRGB"),
        NodeItem("ShaderNodeCombineRGB"),
        ]),
    ShaderNewNodeCategory("SH_NEW_SCRIPT", "Script", items=[
        NodeItem("ShaderNodeScript"),
        ]),
    ShaderNewNodeCategory("SH_NEW_GROUP", "Group", items=shader_node_group_items),
    ShaderNewNodeCategory("SH_NEW_LAYOUT", "Layout", items=[
        ]),

     # Compositor Nodes
     CompositorNodeCategory("CMP_INPUT", "Input", items = [
        NodeItem("CompositorNodeRLayers"),
        NodeItem("CompositorNodeImage"),
        NodeItem("CompositorNodeMovieClip"),
        NodeItem("CompositorNodeMask"),
        NodeItem("CompositorNodeRGB"),
        NodeItem("CompositorNodeValue"),
        NodeItem("CompositorNodeTexture"),
        NodeItem("CompositorNodeBokehImage"),
        NodeItem("CompositorNodeTime"),
        NodeItem("CompositorNodeTrackPos"),
        ]),
    CompositorNodeCategory("CMP_OUTPUT", "Output", items = [
        NodeItem("CompositorNodeComposite"),
        NodeItem("CompositorNodeViewer"),
        NodeItem("CompositorNodeSplitViewer"),
        NodeItem("CompositorNodeOutputFile"),
        NodeItem("CompositorNodeLevels"),
        ]),
    CompositorNodeCategory("CMP_OP_COLOR", "Color", items = [
        NodeItem("CompositorNodeMixRGB"),
        NodeItem("CompositorNodeAlphaOver"),
        NodeItem("CompositorNodeInvert"),
        NodeItem("CompositorNodeCurveRGB"),
        NodeItem("CompositorNodeHueSat"),
        NodeItem("CompositorNodeColorBalance"),
        NodeItem("CompositorNodeHueCorrect"),
        NodeItem("CompositorNodeBrightContrast"),
        NodeItem("CompositorNodeGamma"),
        NodeItem("CompositorNodeColorCorrection"),
        NodeItem("CompositorNodeTonemap"),
        NodeItem("CompositorNodeZcombine"),
        ]),
    CompositorNodeCategory("CMP_CONVERTOR", "Converter", items = [
        NodeItem("CompositorNodeMath"),
        NodeItem("CompositorNodeValToRGB"),
        NodeItem("CompositorNodeSetAlpha"),
        NodeItem("CompositorNodePremulKey"),
        NodeItem("CompositorNodeIDMask"),
        NodeItem("CompositorNodeRGBToBW"),
        NodeItem("CompositorNodeSepRGBA"),
        NodeItem("CompositorNodeCombRGBA"),
        NodeItem("CompositorNodeSepHSVA"),
        NodeItem("CompositorNodeCombHSVA"),
        NodeItem("CompositorNodeSepYUVA"),
        NodeItem("CompositorNodeCombYUVA"),
        NodeItem("CompositorNodeSepYCCA"),
        NodeItem("CompositorNodeCombYCCA"),
        ]),
    CompositorNodeCategory("CMP_OP_FILTER", "Filter", items = [
        NodeItem("CompositorNodeBlur"),
        NodeItem("CompositorNodeBilateralblur"),
        NodeItem("CompositorNodeDilateErode"),
        NodeItem("CompositorNodeDespeckle"),
        NodeItem("CompositorNodeFilter"),
        NodeItem("CompositorNodeBokehBlur"),
        NodeItem("CompositorNodeVecBlur"),
        NodeItem("CompositorNodeDefocus"),
        NodeItem("CompositorNodeGlare"),
        NodeItem("CompositorNodeInpaint"),
        NodeItem("CompositorNodeDBlur"),
        NodeItem("CompositorNodePixelate"),
        ]),
    CompositorNodeCategory("CMP_OP_VECTOR", "Vector", items = [
        NodeItem("CompositorNodeNormal"),
        NodeItem("CompositorNodeMapValue"),
        NodeItem("CompositorNodeMapRange"),
        NodeItem("CompositorNodeNormalize"),
        NodeItem("CompositorNodeCurveVec"),
        ]),
    CompositorNodeCategory("CMP_MATTE", "Matte", items = [
        NodeItem("CompositorNodeKeying"),
        NodeItem("CompositorNodeKeyingScreen"),
        NodeItem("CompositorNodeChannelMatte"),
        NodeItem("CompositorNodeColorSpill"),
        NodeItem("CompositorNodeBoxMask"),
        NodeItem("CompositorNodeEllipseMask"),
        NodeItem("CompositorNodeLumaMatte"),
        NodeItem("CompositorNodeDiffMatte"),
        NodeItem("CompositorNodeDistanceMatte"),
        NodeItem("CompositorNodeChromaMatte"),
        NodeItem("CompositorNodeColorMatte"),
        NodeItem("CompositorNodeDoubleEdgeMask"),
        ]),
    CompositorNodeCategory("CMP_DISTORT", "Distort", items = [
        NodeItem("CompositorNodeScale"),
        NodeItem("CompositorNodeLensdist"),
        NodeItem("CompositorNodeMovieDistortion"),
        NodeItem("CompositorNodeTranslate"),
        NodeItem("CompositorNodeRotate"),
        NodeItem("CompositorNodeFlip"),
        NodeItem("CompositorNodeCrop"),
        NodeItem("CompositorNodeDisplace"),
        NodeItem("CompositorNodeMapUV"),
        NodeItem("CompositorNodeTransform"),
        NodeItem("CompositorNodeStabilize"),
        ]),
    CompositorNodeCategory("CMP_GROUP", "Group", items=compositor_node_group_items),
    CompositorNodeCategory("CMP_LAYOUT", "Layout", items = [
        NodeItem("CompositorNodeSwitch"),
        ]),

    # Texture Nodes
    TextureNodeCategory("TEX_INPUT", "Input", items = [
        NodeItem("TextureNodeCurveTime"),
        NodeItem("TextureNodeCoordinates"),
        NodeItem("TextureNodeTexture"),
        NodeItem("TextureNodeImage"),
        ]),
    TextureNodeCategory("TEX_OUTPUT", "Output", items = [
        NodeItem("TextureNodeOutput"),
        NodeItem("TextureNodeViewer"),
        ]),
    TextureNodeCategory("TEX_OP_COLOR", "Color", items = [
        NodeItem("TextureNodeMixRGB"),
        NodeItem("TextureNodeCurveRGB"),
        NodeItem("TextureNodeInvert"),
        NodeItem("TextureNodeHueSaturation"),
        NodeItem("TextureNodeCompose"),
        NodeItem("TextureNodeDecompose"),
        ]),
    TextureNodeCategory("TEX_PATTERN", "Pattern", items = [
        NodeItem("TextureNodeChecker"),
        NodeItem("TextureNodeBricks"),
        ]),
    TextureNodeCategory("TEX_TEXTURE", "Textures", items = [
        NodeItem("TextureNodeTexNoise"),
        NodeItem("TextureNodeTexDistNoise"),
        NodeItem("TextureNodeTexClouds"),
        NodeItem("TextureNodeTexBlend"),
        NodeItem("TextureNodeTexVoronoi"),
        NodeItem("TextureNodeTexMagic"),
        NodeItem("TextureNodeTexMarble"),
        NodeItem("TextureNodeTexWood"),
        NodeItem("TextureNodeTexMusgrave"),
        NodeItem("TextureNodeTexStucci"),
        ]),
    TextureNodeCategory("TEX_CONVERTOR", "Converter", items = [
        NodeItem("TextureNodeMath"),
        NodeItem("TextureNodeValToRGB"),
        NodeItem("TextureNodeRGBToBW"),
        NodeItem("TextureNodeValToNor"),
        NodeItem("TextureNodeDistance"),
        ]),
    TextureNodeCategory("TEX_DISTORT", "Distort", items = [
        NodeItem("TextureNodeScale"),
        NodeItem("TextureNodeTranslate"),
        NodeItem("TextureNodeRotate"),
        ]),
    TextureNodeCategory("TEX_GROUP", "Group", items=texture_node_group_items),
    TextureNodeCategory("TEX_LAYOUT", "Layout", items = [
        ]),
    ]


def register():
    # XXX can be made a lot nicer, just get it working for now
    nodeitems_utils.node_categories = std_node_categories
    nodeitems_utils.register_node_ui()


def unregister():
    nodeitems_utils.unregister_node_ui()
    nodeitems_utils.node_categories = []


if __name__ == "__main__":
    register()
