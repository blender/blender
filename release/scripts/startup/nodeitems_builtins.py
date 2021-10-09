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
from nodeitems_utils import (
    NodeCategory,
    NodeItem,
    NodeItemCustom,
)


# Subclasses for standard node types

class SortedNodeCategory(NodeCategory):
    def __init__(self, identifier, name, description="", items=None):
        # for builtin nodes the convention is to sort by name
        if isinstance(items, list):
            items = sorted(items, key=lambda item: item.label.lower())

        super().__init__(identifier, name, description=description, items=items)


class CompositorNodeCategory(SortedNodeCategory):
    @classmethod
    def poll(cls, context):
        return (context.space_data.type == 'NODE_EDITOR' and
                context.space_data.tree_type == 'CompositorNodeTree')


class ShaderNodeCategory(SortedNodeCategory):
    @classmethod
    def poll(cls, context):
        return (context.space_data.type == 'NODE_EDITOR' and
                context.space_data.tree_type == 'ShaderNodeTree')


class TextureNodeCategory(SortedNodeCategory):
    @classmethod
    def poll(cls, context):
        return (context.space_data.type == 'NODE_EDITOR' and
                context.space_data.tree_type == 'TextureNodeTree')


class GeometryNodeCategory(SortedNodeCategory):
    @classmethod
    def poll(cls, context):
        return (context.space_data.type == 'NODE_EDITOR' and
                context.space_data.tree_type == 'GeometryNodeTree')


# menu entry for node group tools
def group_tools_draw(self, layout, _context):
    layout.operator("node.group_make")
    layout.operator("node.group_ungroup")
    layout.separator()


# maps node tree type to group node type
node_tree_group_type = {
    'CompositorNodeTree': 'CompositorNodeGroup',
    'ShaderNodeTree': 'ShaderNodeGroup',
    'TextureNodeTree': 'TextureNodeGroup',
    'GeometryNodeTree': 'GeometryNodeGroup',
}


# generic node group items generator for shader, compositor, geometry and texture node groups
def node_group_items(context):
    if context is None:
        return
    space = context.space_data
    if not space:
        return
    ntree = space.edit_tree
    if not ntree:
        return

    yield NodeItemCustom(draw=group_tools_draw)

    yield NodeItem("NodeGroupInput", poll=group_input_output_item_poll)
    yield NodeItem("NodeGroupOutput", poll=group_input_output_item_poll)

    yield NodeItemCustom(draw=lambda self, layout, context: layout.separator())

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
        # filter out hidden nodetrees
        if group.name.startswith('.'):
            continue
        yield NodeItem(node_tree_group_type[group.bl_idname],
                       label=group.name,
                       settings={"node_tree": "bpy.data.node_groups[%r]" % group.name})


# only show input/output nodes inside node groups
def group_input_output_item_poll(context):
    space = context.space_data
    if space.edit_tree in bpy.data.node_groups.values():
        return True
    return False


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


def eevee_cycles_shader_nodes_poll(context):
    return (cycles_shader_nodes_poll(context) or
            eevee_shader_nodes_poll(context))


def object_cycles_shader_nodes_poll(context):
    return (object_shader_nodes_poll(context) and
            cycles_shader_nodes_poll(context))


def object_eevee_shader_nodes_poll(context):
    return (object_shader_nodes_poll(context) and
            eevee_shader_nodes_poll(context))


def object_eevee_cycles_shader_nodes_poll(context):
    return (object_shader_nodes_poll(context) and
            eevee_cycles_shader_nodes_poll(context))


def geometry_nodes_legacy_poll(context):
    return context.preferences.experimental.use_geometry_nodes_legacy


# All standard node categories currently used in nodes.

shader_node_categories = [
    # Shader Nodes (Cycles and Eevee)
    ShaderNodeCategory("SH_NEW_INPUT", "Input", items=[
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
        NodeItem("ShaderNodeBevel"),
        NodeItem("ShaderNodeAmbientOcclusion"),
        NodeItem("ShaderNodeObjectInfo"),
        NodeItem("ShaderNodeHairInfo"),
        NodeItem("ShaderNodeVolumeInfo"),
        NodeItem("ShaderNodeParticleInfo"),
        NodeItem("ShaderNodeCameraData"),
        NodeItem("ShaderNodeUVMap"),
        NodeItem("ShaderNodeVertexColor"),
        NodeItem("ShaderNodeUVAlongStroke", poll=line_style_shader_nodes_poll),
    ]),
    ShaderNodeCategory("SH_NEW_OUTPUT", "Output", items=[
        NodeItem("ShaderNodeOutputMaterial", poll=object_eevee_cycles_shader_nodes_poll),
        NodeItem("ShaderNodeOutputLight", poll=object_cycles_shader_nodes_poll),
        NodeItem("ShaderNodeOutputAOV"),
        NodeItem("ShaderNodeOutputWorld", poll=world_shader_nodes_poll),
        NodeItem("ShaderNodeOutputLineStyle", poll=line_style_shader_nodes_poll),
    ]),
    ShaderNodeCategory("SH_NEW_SHADER", "Shader", items=[
        NodeItem("ShaderNodeMixShader", poll=eevee_cycles_shader_nodes_poll),
        NodeItem("ShaderNodeAddShader", poll=eevee_cycles_shader_nodes_poll),
        NodeItem("ShaderNodeBsdfDiffuse", poll=object_eevee_cycles_shader_nodes_poll),
        NodeItem("ShaderNodeBsdfPrincipled", poll=object_eevee_cycles_shader_nodes_poll),
        NodeItem("ShaderNodeBsdfGlossy", poll=object_eevee_cycles_shader_nodes_poll),
        NodeItem("ShaderNodeBsdfTransparent", poll=object_eevee_cycles_shader_nodes_poll),
        NodeItem("ShaderNodeBsdfRefraction", poll=object_eevee_cycles_shader_nodes_poll),
        NodeItem("ShaderNodeBsdfGlass", poll=object_eevee_cycles_shader_nodes_poll),
        NodeItem("ShaderNodeBsdfTranslucent", poll=object_eevee_cycles_shader_nodes_poll),
        NodeItem("ShaderNodeBsdfAnisotropic", poll=object_cycles_shader_nodes_poll),
        NodeItem("ShaderNodeBsdfVelvet", poll=object_cycles_shader_nodes_poll),
        NodeItem("ShaderNodeBsdfToon", poll=object_cycles_shader_nodes_poll),
        NodeItem("ShaderNodeSubsurfaceScattering", poll=object_eevee_cycles_shader_nodes_poll),
        NodeItem("ShaderNodeEmission", poll=eevee_cycles_shader_nodes_poll),
        NodeItem("ShaderNodeBsdfHair", poll=object_cycles_shader_nodes_poll),
        NodeItem("ShaderNodeBackground", poll=world_shader_nodes_poll),
        NodeItem("ShaderNodeHoldout", poll=object_eevee_cycles_shader_nodes_poll),
        NodeItem("ShaderNodeVolumeAbsorption", poll=eevee_cycles_shader_nodes_poll),
        NodeItem("ShaderNodeVolumeScatter", poll=eevee_cycles_shader_nodes_poll),
        NodeItem("ShaderNodeVolumePrincipled"),
        NodeItem("ShaderNodeEeveeSpecular", poll=object_eevee_shader_nodes_poll),
        NodeItem("ShaderNodeBsdfHairPrincipled", poll=object_cycles_shader_nodes_poll)
    ]),
    ShaderNodeCategory("SH_NEW_TEXTURE", "Texture", items=[
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
        NodeItem("ShaderNodeTexPointDensity"),
        NodeItem("ShaderNodeTexIES"),
        NodeItem("ShaderNodeTexWhiteNoise"),
    ]),
    ShaderNodeCategory("SH_NEW_OP_COLOR", "Color", items=[
        NodeItem("ShaderNodeMixRGB"),
        NodeItem("ShaderNodeRGBCurve"),
        NodeItem("ShaderNodeInvert"),
        NodeItem("ShaderNodeLightFalloff"),
        NodeItem("ShaderNodeHueSaturation"),
        NodeItem("ShaderNodeGamma"),
        NodeItem("ShaderNodeBrightContrast"),
    ]),
    ShaderNodeCategory("SH_NEW_OP_VECTOR", "Vector", items=[
        NodeItem("ShaderNodeMapping"),
        NodeItem("ShaderNodeBump"),
        NodeItem("ShaderNodeDisplacement"),
        NodeItem("ShaderNodeVectorDisplacement"),
        NodeItem("ShaderNodeNormalMap"),
        NodeItem("ShaderNodeNormal"),
        NodeItem("ShaderNodeVectorCurve"),
        NodeItem("ShaderNodeVectorRotate"),
        NodeItem("ShaderNodeVectorTransform"),
    ]),
    ShaderNodeCategory("SH_NEW_CONVERTOR", "Converter", items=[
        NodeItem("ShaderNodeMapRange"),
        NodeItem("ShaderNodeFloatCurve"),
        NodeItem("ShaderNodeClamp"),
        NodeItem("ShaderNodeMath"),
        NodeItem("ShaderNodeValToRGB"),
        NodeItem("ShaderNodeRGBToBW"),
        NodeItem("ShaderNodeShaderToRGB", poll=object_eevee_shader_nodes_poll),
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
    ShaderNodeCategory("SH_NEW_SCRIPT", "Script", items=[
        NodeItem("ShaderNodeScript"),
    ]),
    ShaderNodeCategory("SH_NEW_GROUP", "Group", items=node_group_items),
    ShaderNodeCategory("SH_NEW_LAYOUT", "Layout", items=[
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
    ]),
    CompositorNodeCategory("CMP_OUTPUT", "Output", items=[
        NodeItem("CompositorNodeComposite"),
        NodeItem("CompositorNodeViewer"),
        NodeItem("CompositorNodeSplitViewer"),
        NodeItem("CompositorNodeOutputFile"),
        NodeItem("CompositorNodeLevels"),
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
        NodeItem("CompositorNodeExposure"),
        NodeItem("CompositorNodeColorCorrection"),
        NodeItem("CompositorNodePosterize"),
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
        NodeItem("CompositorNodeSwitchView"),
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
        NodeItem("CompositorNodeDenoise"),
        NodeItem("CompositorNodeAntiAliasing"),
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
        NodeItem("CompositorNodeCryptomatte"),
        NodeItem("CompositorNodeCryptomatteV2"),
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
    ]),
    TextureNodeCategory("TEX_OUTPUT", "Output", items=[
        NodeItem("TextureNodeOutput"),
        NodeItem("TextureNodeViewer"),
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

geometry_node_categories = [
    # Geometry Nodes
    GeometryNodeCategory("GEO_ATTRIBUTE", "Attribute", items=[
        NodeItem("GeometryNodeLegacyAttributeRandomize", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyAttributeMath", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyAttributeClamp", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyAttributeCompare", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyAttributeConvert", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyAttributeCurveMap", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyAttributeFill", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyAttributeMix", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyAttributeProximity", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyAttributeColorRamp", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyAttributeVectorMath", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyAttributeVectorRotate", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyAttributeSampleTexture", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyAttributeCombineXYZ", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyAttributeSeparateXYZ", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyAttributeMapRange", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyAttributeTransfer", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeAttributeRemove", poll=geometry_nodes_legacy_poll),

        NodeItem("GeometryNodeAttributeCapture"),
        NodeItem("GeometryNodeAttributeStatistic"),
    ]),
    GeometryNodeCategory("GEO_COLOR", "Color", items=[
        NodeItem("ShaderNodeMixRGB"),
        NodeItem("ShaderNodeRGBCurve"),
        NodeItem("ShaderNodeValToRGB"),
        NodeItem("ShaderNodeSeparateRGB"),
        NodeItem("ShaderNodeCombineRGB"),
    ]),
    GeometryNodeCategory("GEO_CURVE", "Curve", items=[
        NodeItem("GeometryNodeLegacyCurveSubdivide", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyCurveReverse", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyCurveSplineType", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyCurveSetHandles", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyCurveSelectHandles", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyMeshToCurve", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyCurveToPoints", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyCurveEndpoints", poll=geometry_nodes_legacy_poll),

        NodeItem("GeometryNodeCurveToMesh"),
        NodeItem("GeometryNodeCurveResample"),
        NodeItem("GeometryNodeCurveFill"),
        NodeItem("GeometryNodeCurveTrim"),
        NodeItem("GeometryNodeCurveLength"),
        NodeItem("GeometryNodeCurveSplineType"),
        NodeItem("GeometryNodeSplineLength"),
        NodeItem("GeometryNodeCurveSubdivide"),
        NodeItem("GeometryNodeCurveParameter"),
        NodeItem("GeometryNodeCurveSetHandles"),
        NodeItem("GeometryNodeInputTangent"),
        NodeItem("GeometryNodeCurveSample"),
        NodeItem("GeometryNodeCurveHandleTypeSelection"),
        NodeItem("GeometryNodeCurveFillet"),
        NodeItem("GeometryNodeCurveReverse"),
    ]),
    GeometryNodeCategory("GEO_PRIMITIVES_CURVE", "Curve Primitives", items=[
        NodeItem("GeometryNodeCurvePrimitiveLine"),
        NodeItem("GeometryNodeCurvePrimitiveCircle"),
        NodeItem("GeometryNodeCurveStar"),
        NodeItem("GeometryNodeCurveSpiral"),
        NodeItem("GeometryNodeCurveQuadraticBezier"),
        NodeItem("GeometryNodeCurvePrimitiveQuadrilateral"),
        NodeItem("GeometryNodeCurvePrimitiveBezierSegment"),
    ]),
    GeometryNodeCategory("GEO_GEOMETRY", "Geometry", items=[
        NodeItem("GeometryNodeLegacyDeleteGeometry", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyRaycast", poll=geometry_nodes_legacy_poll),

        NodeItem("GeometryNodeProximity"),
        NodeItem("GeometryNodeBoundBox"),
        NodeItem("GeometryNodeConvexHull"),
        NodeItem("GeometryNodeTransform"),
        NodeItem("GeometryNodeJoinGeometry"),
        NodeItem("GeometryNodeSeparateComponents"),
        NodeItem("GeometryNodeSetPosition"),
        NodeItem("GeometryNodeRealizeInstances"),
    ]),
    GeometryNodeCategory("GEO_INPUT", "Input", items=[
        NodeItem("FunctionNodeLegacyRandomFloat", poll=geometry_nodes_legacy_poll),

        NodeItem("GeometryNodeObjectInfo"),
        NodeItem("GeometryNodeCollectionInfo"),
        NodeItem("ShaderNodeValue"),
        NodeItem("FunctionNodeInputString"),
        NodeItem("FunctionNodeInputVector"),
        NodeItem("GeometryNodeInputMaterial"),
        NodeItem("GeometryNodeIsViewport"),
        NodeItem("GeometryNodeInputPosition"),
        NodeItem("GeometryNodeInputIndex"),
        NodeItem("GeometryNodeInputNormal"),
    ]),
    GeometryNodeCategory("GEO_MATERIAL", "Material", items=[
        NodeItem("GeometryNodeLegacyMaterialAssign", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacySelectByMaterial", poll=geometry_nodes_legacy_poll),

        NodeItem("GeometryNodeMaterialAssign"),
        NodeItem("GeometryNodeMaterialSelection"),
        NodeItem("GeometryNodeMaterialReplace"),
    ]),
    GeometryNodeCategory("GEO_MESH", "Mesh", items=[
        NodeItem("GeometryNodeLegacyEdgeSplit", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacySubdivisionSurface", poll=geometry_nodes_legacy_poll),

        NodeItem("GeometryNodeBoolean"),
        NodeItem("GeometryNodeTriangulate"),
        NodeItem("GeometryNodeMeshSubdivide"),
        NodeItem("GeometryNodePointsToVertices"),
    ]),
    GeometryNodeCategory("GEO_PRIMITIVES_MESH", "Mesh Primitives", items=[
        NodeItem("GeometryNodeMeshCircle"),
        NodeItem("GeometryNodeMeshCone"),
        NodeItem("GeometryNodeMeshCube"),
        NodeItem("GeometryNodeMeshCylinder"),
        NodeItem("GeometryNodeMeshGrid"),
        NodeItem("GeometryNodeMeshIcoSphere"),
        NodeItem("GeometryNodeMeshLine"),
        NodeItem("GeometryNodeMeshUVSphere"),
    ]),
    GeometryNodeCategory("GEO_POINT", "Point", items=[
        NodeItem("GeometryNodeMeshToPoints"),
        NodeItem("GeometryNodeInstanceOnPoints"),
        NodeItem("GeometryNodeDistributePointsOnFaces"),
        NodeItem("GeometryNodeLegacyPointDistribute", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyPointInstance", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyPointSeparate", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyPointScale", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyPointTranslate", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyRotatePoints", poll=geometry_nodes_legacy_poll),
        NodeItem("GeometryNodeLegacyAlignRotationToVector", poll=geometry_nodes_legacy_poll),
    ]),
    GeometryNodeCategory("GEO_TEXT", "Text", items=[
        NodeItem("FunctionNodeStringLength"),
        NodeItem("FunctionNodeStringSubstring"),
        NodeItem("FunctionNodeValueToString"),
        NodeItem("GeometryNodeStringJoin"),
        NodeItem("FunctionNodeInputSpecialCharacters"),
        NodeItem("GeometryNodeStringToCurves"),
    ]),
    GeometryNodeCategory("GEO_UTILITIES", "Utilities", items=[
        NodeItem("ShaderNodeMapRange"),
        NodeItem("ShaderNodeFloatCurve"),
        NodeItem("ShaderNodeClamp"),
        NodeItem("ShaderNodeMath"),
        NodeItem("FunctionNodeBooleanMath"),
        NodeItem("FunctionNodeRotateEuler"),
        NodeItem("FunctionNodeFloatCompare"),
        NodeItem("FunctionNodeFloatToInt"),
        NodeItem("GeometryNodeSwitch"),
        NodeItem("FunctionNodeRandomValue"),
        NodeItem("FunctionNodeAlignEulerToVector"),
    ]),
    GeometryNodeCategory("GEO_TEXTURE", "Texture", items=[
        NodeItem("ShaderNodeTexNoise"),
    ]),
    GeometryNodeCategory("GEO_VECTOR", "Vector", items=[
        NodeItem("ShaderNodeVectorCurve"),
        NodeItem("ShaderNodeSeparateXYZ"),
        NodeItem("ShaderNodeCombineXYZ"),
        NodeItem("ShaderNodeVectorMath"),
        NodeItem("ShaderNodeVectorRotate"),
    ]),
    GeometryNodeCategory("GEO_OUTPUT", "Output", items=[
        NodeItem("GeometryNodeViewer"),
    ]),
    GeometryNodeCategory("GEO_VOLUME", "Volume", items=[
        NodeItem("GeometryNodeLegacyPointsToVolume", poll=geometry_nodes_legacy_poll),

        NodeItem("GeometryNodePointsToVolume"),
        NodeItem("GeometryNodeVolumeToMesh"),
    ]),
    GeometryNodeCategory("GEO_GROUP", "Group", items=node_group_items),
    GeometryNodeCategory("GEO_LAYOUT", "Layout", items=[
        NodeItem("NodeFrame"),
        NodeItem("NodeReroute"),
    ]),
]


def register():
    nodeitems_utils.register_node_categories('SHADER', shader_node_categories)
    nodeitems_utils.register_node_categories('COMPOSITING', compositor_node_categories)
    nodeitems_utils.register_node_categories('TEXTURE', texture_node_categories)
    nodeitems_utils.register_node_categories('GEOMETRY', geometry_node_categories)


def unregister():
    nodeitems_utils.unregister_node_categories('SHADER')
    nodeitems_utils.unregister_node_categories('COMPOSITING')
    nodeitems_utils.unregister_node_categories('TEXTURE')
    nodeitems_utils.unregister_node_categories('GEOMETRY')


if __name__ == "__main__":
    register()
