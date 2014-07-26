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
from nodeitems_utils import NodeCategory, NodeItem, NodeItemCustom


# Subclasses for standard node types

class CompositorNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        return (context.space_data.tree_type == 'CompositorNodeTree')


class ShaderNewNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        return (context.space_data.tree_type == 'ShaderNodeTree' and
                context.scene.render.use_shading_nodes)


class ShaderOldNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        return (context.space_data.tree_type == 'ShaderNodeTree' and
                not context.scene.render.use_shading_nodes)


class TextureNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        return context.space_data.tree_type == 'TextureNodeTree'


# menu entry for node group tools
def group_tools_draw(self, layout, context):
    layout.operator("node.group_make")
    layout.operator("node.group_ungroup")
    layout.separator()

# maps node tree type to group node type
node_tree_group_type = {
    'CompositorNodeTree': 'CompositorNodeGroup',
    'ShaderNodeTree': 'ShaderNodeGroup',
    'TextureNodeTree': 'TextureNodeGroup',
    }


# generic node group items generator for shader, compositor and texture node groups
def node_group_items(context):
    space = context.space_data
    if not space:
        return
    ntree = space.edit_tree
    if not ntree:
        return

    yield NodeItemCustom(draw=group_tools_draw)

    def contains_group(nodetree, group):
        if nodetree == group:
            return True
        else:
            for node in nodetree.nodes:
                if node.bl_idname in node_tree_group_type.values() and node.node_tree is not None:
                    if contains_group(node.node_tree, group):
                        return True
        return False

    for group in context.blend_data.node_groups:
        if group.bl_idname != ntree.bl_idname:
            continue
        # filter out recursive groups
        if contains_group(group, ntree):
            continue

        yield NodeItem(node_tree_group_type[group.bl_idname],
                       group.name,
                       {"node_tree": "bpy.data.node_groups[%r]" % group.name})


# only show input/output nodes inside node groups
def group_input_output_item_poll(context):
    space = context.space_data
    if space.edit_tree in bpy.data.node_groups.values():
        return True
    return False


# All standard node categories currently used in nodes.

shader_node_categories = [
    # Shader Nodes
    ShaderOldNodeCategory("SH_INPUT", "Input", items=[
        NodeItem("ShaderNodeMaterial"),
        NodeItem("ShaderNodeCameraData"),
        NodeItem("ShaderNodeLampData"),
        NodeItem("ShaderNodeValue"),
        NodeItem("ShaderNodeRGB"),
        NodeItem("ShaderNodeTexture"),
        NodeItem("ShaderNodeGeometry"),
        NodeItem("ShaderNodeExtendedMaterial"),
        NodeItem("NodeGroupInput", poll=group_input_output_item_poll),
        ]),
    ShaderOldNodeCategory("SH_OUTPUT", "Output", items=[
        NodeItem("ShaderNodeOutput"),
        NodeItem("NodeGroupOutput", poll=group_input_output_item_poll),
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
        NodeItem("ShaderNodeSeparateHSV"),
        NodeItem("ShaderNodeCombineHSV"),
        ]),
    ShaderOldNodeCategory("SH_GROUP", "Group", items=node_group_items),
    ShaderOldNodeCategory("SH_LAYOUT", "Layout", items=[
        NodeItem("NodeFrame"),
        NodeItem("NodeReroute"),
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
        NodeItem("ShaderNodeWireframe"),
        NodeItem("ShaderNodeObjectInfo"),
        NodeItem("ShaderNodeHairInfo"),
        NodeItem("ShaderNodeParticleInfo"),
        NodeItem("ShaderNodeCameraData"),
        NodeItem("ShaderNodeUVMap"),
        NodeItem("NodeGroupInput", poll=group_input_output_item_poll),
        ]),
    ShaderNewNodeCategory("SH_NEW_OUTPUT", "Output", items=[
        NodeItem("ShaderNodeOutputMaterial"),
        NodeItem("ShaderNodeOutputLamp"),
        NodeItem("ShaderNodeOutputWorld"),
        NodeItem("NodeGroupOutput", poll=group_input_output_item_poll),
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
        NodeItem("ShaderNodeBsdfToon"),
        NodeItem("ShaderNodeSubsurfaceScattering"),
        NodeItem("ShaderNodeEmission"),
        NodeItem("ShaderNodeBsdfHair"),
        NodeItem("ShaderNodeBackground"),
        NodeItem("ShaderNodeAmbientOcclusion"),
        NodeItem("ShaderNodeHoldout"),
        NodeItem("ShaderNodeVolumeAbsorption"),
        NodeItem("ShaderNodeVolumeScatter"),
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
        NodeItem("ShaderNodeVectorTransform"),
        ]),
    ShaderNewNodeCategory("SH_NEW_CONVERTOR", "Converter", items=[
        NodeItem("ShaderNodeMath"),
        NodeItem("ShaderNodeValToRGB"),
        NodeItem("ShaderNodeRGBToBW"),
        NodeItem("ShaderNodeVectorMath"),
        NodeItem("ShaderNodeSeparateRGB"),
        NodeItem("ShaderNodeCombineRGB"),
        NodeItem("ShaderNodeSeparateXYZ"),
        NodeItem("ShaderNodeCombineXYZ"),
        NodeItem("ShaderNodeSeparateHSV"),
        NodeItem("ShaderNodeCombineHSV"),
        NodeItem("ShaderNodeWavelength"),
        NodeItem("ShaderNodeBlackbody"),
        ]),
    ShaderNewNodeCategory("SH_NEW_SCRIPT", "Script", items=[
        NodeItem("ShaderNodeScript"),
        ]),
    ShaderNewNodeCategory("SH_NEW_GROUP", "Group", items=node_group_items),
    ShaderNewNodeCategory("SH_NEW_LAYOUT", "Layout", items=[
        NodeItem("NodeFrame"),
        NodeItem("NodeReroute"),
        ]),
    ]

compositor_node_categories = [
    # Compositor Nodes
    CompositorNodeCategory("CMP_INPUT", "Input", items=[
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
        NodeItem("NodeGroupInput", poll=group_input_output_item_poll),
        ]),
    CompositorNodeCategory("CMP_OUTPUT", "Output", items=[
        NodeItem("CompositorNodeComposite"),
        NodeItem("CompositorNodeViewer"),
        NodeItem("CompositorNodeSplitViewer"),
        NodeItem("CompositorNodeOutputFile"),
        NodeItem("CompositorNodeLevels"),
        NodeItem("NodeGroupOutput", poll=group_input_output_item_poll),
        ]),
    CompositorNodeCategory("CMP_OP_COLOR", "Color", items=[
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
    CompositorNodeCategory("CMP_CONVERTOR", "Converter", items=[
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
    CompositorNodeCategory("CMP_OP_FILTER", "Filter", items=[
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
        NodeItem("CompositorNodeSunBeams"),
        ]),
    CompositorNodeCategory("CMP_OP_VECTOR", "Vector", items=[
        NodeItem("CompositorNodeNormal"),
        NodeItem("CompositorNodeMapValue"),
        NodeItem("CompositorNodeMapRange"),
        NodeItem("CompositorNodeNormalize"),
        NodeItem("CompositorNodeCurveVec"),
        ]),
    CompositorNodeCategory("CMP_MATTE", "Matte", items=[
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
    CompositorNodeCategory("CMP_DISTORT", "Distort", items=[
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
        NodeItem("CompositorNodePlaneTrackDeform"),
        NodeItem("CompositorNodeCornerPin"),
        ]),
    CompositorNodeCategory("CMP_GROUP", "Group", items=node_group_items),
    CompositorNodeCategory("CMP_LAYOUT", "Layout", items=[
        NodeItem("NodeFrame"),
        NodeItem("NodeReroute"),
        NodeItem("CompositorNodeSwitch"),
        ]),
    ]

texture_node_categories = [
    # Texture Nodes
    TextureNodeCategory("TEX_INPUT", "Input", items=[
        NodeItem("TextureNodeCurveTime"),
        NodeItem("TextureNodeCoordinates"),
        NodeItem("TextureNodeTexture"),
        NodeItem("TextureNodeImage"),
        NodeItem("NodeGroupInput", poll=group_input_output_item_poll),
        ]),
    TextureNodeCategory("TEX_OUTPUT", "Output", items=[
        NodeItem("TextureNodeOutput"),
        NodeItem("TextureNodeViewer"),
        NodeItem("NodeGroupOutput", poll=group_input_output_item_poll),
        ]),
    TextureNodeCategory("TEX_OP_COLOR", "Color", items=[
        NodeItem("TextureNodeMixRGB"),
        NodeItem("TextureNodeCurveRGB"),
        NodeItem("TextureNodeInvert"),
        NodeItem("TextureNodeHueSaturation"),
        NodeItem("TextureNodeCompose"),
        NodeItem("TextureNodeDecompose"),
        ]),
    TextureNodeCategory("TEX_PATTERN", "Pattern", items=[
        NodeItem("TextureNodeChecker"),
        NodeItem("TextureNodeBricks"),
        ]),
    TextureNodeCategory("TEX_TEXTURE", "Textures", items=[
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
    TextureNodeCategory("TEX_CONVERTOR", "Converter", items=[
        NodeItem("TextureNodeMath"),
        NodeItem("TextureNodeValToRGB"),
        NodeItem("TextureNodeRGBToBW"),
        NodeItem("TextureNodeValToNor"),
        NodeItem("TextureNodeDistance"),
        ]),
    TextureNodeCategory("TEX_DISTORT", "Distort", items=[
        NodeItem("TextureNodeScale"),
        NodeItem("TextureNodeTranslate"),
        NodeItem("TextureNodeRotate"),
        NodeItem("TextureNodeAt"),
        ]),
    TextureNodeCategory("TEX_GROUP", "Group", items=node_group_items),
    TextureNodeCategory("TEX_LAYOUT", "Layout", items=[
        NodeItem("NodeFrame"),
        NodeItem("NodeReroute"),
        ]),
    ]


def register():
    nodeitems_utils.register_node_categories('SHADER', shader_node_categories)
    nodeitems_utils.register_node_categories('COMPOSITING', compositor_node_categories)
    nodeitems_utils.register_node_categories('TEXTURE', texture_node_categories)


def unregister():
    nodeitems_utils.unregister_node_categories('SHADER')
    nodeitems_utils.unregister_node_categories('COMPOSITING')
    nodeitems_utils.unregister_node_categories('TEXTURE')


if __name__ == "__main__":
    register()
