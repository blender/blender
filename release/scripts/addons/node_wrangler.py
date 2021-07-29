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

bl_info = {
    "name": "Node Wrangler",
    "author": "Bartek Skorupa, Greg Zaal, Sebastian Koenig, Christian Brinkmann, Florian Meyer",
    "version": (3, 35),
    "blender": (2, 78, 0),
    "location": "Node Editor Toolbar or Ctrl-Space",
    "description": "Various tools to enhance and speed up node-based workflow",
    "warning": "",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Nodes/Nodes_Efficiency_Tools",
    "category": "Node",
}

import bpy, blf, bgl
from bpy.types import Operator, Panel, Menu
from bpy.props import FloatProperty, EnumProperty, BoolProperty, IntProperty, StringProperty, FloatVectorProperty, CollectionProperty
from bpy_extras.io_utils import ImportHelper, ExportHelper
from mathutils import Vector
from math import cos, sin, pi, hypot
from os import path
from glob import glob
from copy import copy
from itertools import chain
import re
from collections import namedtuple

#################
# rl_outputs:
# list of outputs of Input Render Layer
# with attributes determinig if pass is used,
# and MultiLayer EXR outputs names and corresponding render engines
#
# rl_outputs entry = (render_pass, rl_output_name, exr_output_name, in_internal, in_cycles)
RL_entry = namedtuple('RL_Entry', ['render_pass', 'output_name', 'exr_output_name', 'in_internal', 'in_cycles'])
rl_outputs = (
    RL_entry('use_pass_ambient_occlusion', 'AO', 'AO', True, True),
    RL_entry('use_pass_color', 'Color', 'Color', True, False),
    RL_entry('use_pass_combined', 'Image', 'Combined', True, True),
    RL_entry('use_pass_diffuse', 'Diffuse', 'Diffuse', True, False),
    RL_entry('use_pass_diffuse_color', 'Diffuse Color', 'DiffCol', False, True),
    RL_entry('use_pass_diffuse_direct', 'Diffuse Direct', 'DiffDir', False, True),
    RL_entry('use_pass_diffuse_indirect', 'Diffuse Indirect', 'DiffInd', False, True),
    RL_entry('use_pass_emit', 'Emit', 'Emit', True, False),
    RL_entry('use_pass_environment', 'Environment', 'Env', True, False),
    RL_entry('use_pass_glossy_color', 'Glossy Color', 'GlossCol', False, True),
    RL_entry('use_pass_glossy_direct', 'Glossy Direct', 'GlossDir', False, True),
    RL_entry('use_pass_glossy_indirect', 'Glossy Indirect', 'GlossInd', False, True),
    RL_entry('use_pass_indirect', 'Indirect', 'Indirect', True, False),
    RL_entry('use_pass_material_index', 'IndexMA', 'IndexMA', True, True),
    RL_entry('use_pass_mist', 'Mist', 'Mist', True, False),
    RL_entry('use_pass_normal', 'Normal', 'Normal', True, True),
    RL_entry('use_pass_object_index', 'IndexOB', 'IndexOB', True, True),
    RL_entry('use_pass_reflection', 'Reflect', 'Reflect', True, False),
    RL_entry('use_pass_refraction', 'Refract', 'Refract', True, False),
    RL_entry('use_pass_shadow', 'Shadow', 'Shadow', True, True),
    RL_entry('use_pass_specular', 'Specular', 'Spec', True, False),
    RL_entry('use_pass_subsurface_color', 'Subsurface Color', 'SubsurfaceCol', False, True),
    RL_entry('use_pass_subsurface_direct', 'Subsurface Direct', 'SubsurfaceDir', False, True),
    RL_entry('use_pass_subsurface_indirect', 'Subsurface Indirect', 'SubsurfaceInd', False, True),
    RL_entry('use_pass_transmission_color', 'Transmission Color', 'TransCol', False, True),
    RL_entry('use_pass_transmission_direct', 'Transmission Direct', 'TransDir', False, True),
    RL_entry('use_pass_transmission_indirect', 'Transmission Indirect', 'TransInd', False, True),
    RL_entry('use_pass_uv', 'UV', 'UV', True, True),
    RL_entry('use_pass_vector', 'Speed', 'Vector', True, True),
    RL_entry('use_pass_z', 'Z', 'Depth', True, True),
    )

# shader nodes
# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
shaders_input_nodes_props = (
    ('ShaderNodeTexCoord', 'TEX_COORD', 'Texture Coordinate'),
    ('ShaderNodeAttribute', 'ATTRIBUTE', 'Attribute'),
    ('ShaderNodeLightPath', 'LIGHT_PATH', 'Light Path'),
    ('ShaderNodeFresnel', 'FRESNEL', 'Fresnel'),
    ('ShaderNodeLayerWeight', 'LAYER_WEIGHT', 'Layer Weight'),
    ('ShaderNodeRGB', 'RGB', 'RGB'),
    ('ShaderNodeValue', 'VALUE', 'Value'),
    ('ShaderNodeTangent', 'TANGENT', 'Tangent'),
    ('ShaderNodeNewGeometry', 'NEW_GEOMETRY', 'Geometry'),
    ('ShaderNodeWireframe', 'WIREFRAME', 'Wireframe'),
    ('ShaderNodeObjectInfo', 'OBJECT_INFO', 'Object Info'),
    ('ShaderNodeHairInfo', 'HAIR_INFO', 'Hair Info'),
    ('ShaderNodeParticleInfo', 'PARTICLE_INFO', 'Particle Info'),
    ('ShaderNodeCameraData', 'CAMERA', 'Camera Data'),
    ('ShaderNodeUVMap', 'UVMAP', 'UV Map'),
)
# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
shaders_output_nodes_props = (
    ('ShaderNodeOutputMaterial', 'OUTPUT_MATERIAL', 'Material Output'),
    ('ShaderNodeOutputLamp', 'OUTPUT_LAMP', 'Lamp Output'),
    ('ShaderNodeOutputWorld', 'OUTPUT_WORLD', 'World Output'),
)
# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
shaders_shader_nodes_props = (
    ('ShaderNodeMixShader', 'MIX_SHADER', 'Mix Shader'),
    ('ShaderNodeAddShader', 'ADD_SHADER', 'Add Shader'),
    ('ShaderNodeBsdfDiffuse', 'BSDF_DIFFUSE', 'Diffuse BSDF'),
    ('ShaderNodeBsdfGlossy', 'BSDF_GLOSSY', 'Glossy BSDF'),
    ('ShaderNodeBsdfTransparent', 'BSDF_TRANSPARENT', 'Transparent BSDF'),
    ('ShaderNodeBsdfRefraction', 'BSDF_REFRACTION', 'Refraction BSDF'),
    ('ShaderNodeBsdfGlass', 'BSDF_GLASS', 'Glass BSDF'),
    ('ShaderNodeBsdfTranslucent', 'BSDF_TRANSLUCENT', 'Translucent BSDF'),
    ('ShaderNodeBsdfAnisotropic', 'BSDF_ANISOTROPIC', 'Anisotropic BSDF'),
    ('ShaderNodeBsdfVelvet', 'BSDF_VELVET', 'Velvet BSDF'),
    ('ShaderNodeBsdfToon', 'BSDF_TOON', 'Toon BSDF'),
    ('ShaderNodeSubsurfaceScattering', 'SUBSURFACE_SCATTERING', 'Subsurface Scattering'),
    ('ShaderNodeEmission', 'EMISSION', 'Emission'),
    ('ShaderNodeBsdfHair', 'BSDF_HAIR', 'Hair BSDF'),
    ('ShaderNodeBackground', 'BACKGROUND', 'Background'),
    ('ShaderNodeAmbientOcclusion', 'AMBIENT_OCCLUSION', 'Ambient Occlusion'),
    ('ShaderNodeHoldout', 'HOLDOUT', 'Holdout'),
    ('ShaderNodeVolumeAbsorption', 'VOLUME_ABSORPTION', 'Volume Absorption'),
    ('ShaderNodeVolumeScatter', 'VOLUME_SCATTER', 'Volume Scatter'),
    ('ShaderNodeBsdfPrincipled', 'BSDF_PRINCIPLED', 'Principled BSDF'),
)
# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
shaders_texture_nodes_props = (
    ('ShaderNodeTexBrick', 'TEX_BRICK', 'Brick Texture'),
    ('ShaderNodeTexChecker', 'TEX_CHECKER', 'Checker Texture'),
    ('ShaderNodeTexEnvironment', 'TEX_ENVIRONMENT', 'Environment Texture'),
    ('ShaderNodeTexGradient', 'TEX_GRADIENT', 'Gradient Texture'),
    ('ShaderNodeTexImage', 'TEX_IMAGE', 'Image Texture'),
    ('ShaderNodeTexMagic', 'TEX_MAGIC', 'Magic Texture'),
    ('ShaderNodeTexMusgrave', 'TEX_MUSGRAVE', 'Musgrave Texture'),
    ('ShaderNodeTexNoise', 'TEX_NOISE', 'Noise Texture'),
    ('ShaderNodeTexPointDensity', 'TEX_POINTDENSITY', 'Point Density'),
    ('ShaderNodeTexSky', 'TEX_SKY', 'Sky Texture'),
    ('ShaderNodeTexVoronoi', 'TEX_VORONOI', 'Voronoi Texture'),
    ('ShaderNodeTexWave', 'TEX_WAVE', 'Wave Texture'),
)
# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
shaders_color_nodes_props = (
    ('ShaderNodeMixRGB', 'MIX_RGB', 'MixRGB'),
    ('ShaderNodeRGBCurve', 'CURVE_RGB', 'RGB Curves'),
    ('ShaderNodeInvert', 'INVERT', 'Invert'),
    ('ShaderNodeLightFalloff', 'LIGHT_FALLOFF', 'Light Falloff'),
    ('ShaderNodeHueSaturation', 'HUE_SAT', 'Hue/Saturation'),
    ('ShaderNodeGamma', 'GAMMA', 'Gamma'),
    ('ShaderNodeBrightContrast', 'BRIGHTCONTRAST', 'Bright Contrast'),
)
# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
shaders_vector_nodes_props = (
    ('ShaderNodeMapping', 'MAPPING', 'Mapping'),
    ('ShaderNodeBump', 'BUMP', 'Bump'),
    ('ShaderNodeNormalMap', 'NORMAL_MAP', 'Normal Map'),
    ('ShaderNodeNormal', 'NORMAL', 'Normal'),
    ('ShaderNodeVectorCurve', 'CURVE_VEC', 'Vector Curves'),
    ('ShaderNodeVectorTransform', 'VECT_TRANSFORM', 'Vector Transform'),
)
# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
shaders_converter_nodes_props = (
    ('ShaderNodeMath', 'MATH', 'Math'),
    ('ShaderNodeValToRGB', 'VALTORGB', 'ColorRamp'),
    ('ShaderNodeRGBToBW', 'RGBTOBW', 'RGB to BW'),
    ('ShaderNodeVectorMath', 'VECT_MATH', 'Vector Math'),
    ('ShaderNodeSeparateRGB', 'SEPRGB', 'Separate RGB'),
    ('ShaderNodeCombineRGB', 'COMBRGB', 'Combine RGB'),
    ('ShaderNodeSeparateXYZ', 'SEPXYZ', 'Separate XYZ'),
    ('ShaderNodeCombineXYZ', 'COMBXYZ', 'Combine XYZ'),
    ('ShaderNodeSeparateHSV', 'SEPHSV', 'Separate HSV'),
    ('ShaderNodeCombineHSV', 'COMBHSV', 'Combine HSV'),
    ('ShaderNodeWavelength', 'WAVELENGTH', 'Wavelength'),
    ('ShaderNodeBlackbody', 'BLACKBODY', 'Blackbody'),
)
# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
shaders_layout_nodes_props = (
    ('NodeFrame', 'FRAME', 'Frame'),
    ('NodeReroute', 'REROUTE', 'Reroute'),
)

# compositing nodes
# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
compo_input_nodes_props = (
    ('CompositorNodeRLayers', 'R_LAYERS', 'Render Layers'),
    ('CompositorNodeImage', 'IMAGE', 'Image'),
    ('CompositorNodeMovieClip', 'MOVIECLIP', 'Movie Clip'),
    ('CompositorNodeMask', 'MASK', 'Mask'),
    ('CompositorNodeRGB', 'RGB', 'RGB'),
    ('CompositorNodeValue', 'VALUE', 'Value'),
    ('CompositorNodeTexture', 'TEXTURE', 'Texture'),
    ('CompositorNodeBokehImage', 'BOKEHIMAGE', 'Bokeh Image'),
    ('CompositorNodeTime', 'TIME', 'Time'),
    ('CompositorNodeTrackPos', 'TRACKPOS', 'Track Position'),
)
# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
compo_output_nodes_props = (
    ('CompositorNodeComposite', 'COMPOSITE', 'Composite'),
    ('CompositorNodeViewer', 'VIEWER', 'Viewer'),
    ('CompositorNodeSplitViewer', 'SPLITVIEWER', 'Split Viewer'),
    ('CompositorNodeOutputFile', 'OUTPUT_FILE', 'File Output'),
    ('CompositorNodeLevels', 'LEVELS', 'Levels'),
)
# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
compo_color_nodes_props = (
    ('CompositorNodeMixRGB', 'MIX_RGB', 'Mix'),
    ('CompositorNodeAlphaOver', 'ALPHAOVER', 'Alpha Over'),
    ('CompositorNodeInvert', 'INVERT', 'Invert'),
    ('CompositorNodeCurveRGB', 'CURVE_RGB', 'RGB Curves'),
    ('CompositorNodeHueSat', 'HUE_SAT', 'Hue Saturation Value'),
    ('CompositorNodeColorBalance', 'COLORBALANCE', 'Color Balance'),
    ('CompositorNodeHueCorrect', 'HUECORRECT', 'Hue Correct'),
    ('CompositorNodeBrightContrast', 'BRIGHTCONTRAST', 'Bright/Contrast'),
    ('CompositorNodeGamma', 'GAMMA', 'Gamma'),
    ('CompositorNodeColorCorrection', 'COLORCORRECTION', 'Color Correction'),
    ('CompositorNodeTonemap', 'TONEMAP', 'Tonemap'),
    ('CompositorNodeZcombine', 'ZCOMBINE', 'Z Combine'),
)
# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
compo_converter_nodes_props = (
    ('CompositorNodeMath', 'MATH', 'Math'),
    ('CompositorNodeValToRGB', 'VALTORGB', 'ColorRamp'),
    ('CompositorNodeSetAlpha', 'SETALPHA', 'Set Alpha'),
    ('CompositorNodePremulKey', 'PREMULKEY', 'Alpha Convert'),
    ('CompositorNodeIDMask', 'ID_MASK', 'ID Mask'),
    ('CompositorNodeRGBToBW', 'RGBTOBW', 'RGB to BW'),
    ('CompositorNodeSepRGBA', 'SEPRGBA', 'Separate RGBA'),
    ('CompositorNodeCombRGBA', 'COMBRGBA', 'Combine RGBA'),
    ('CompositorNodeSepHSVA', 'SEPHSVA', 'Separate HSVA'),
    ('CompositorNodeCombHSVA', 'COMBHSVA', 'Combine HSVA'),
    ('CompositorNodeSepYUVA', 'SEPYUVA', 'Separate YUVA'),
    ('CompositorNodeCombYUVA', 'COMBYUVA', 'Combine YUVA'),
    ('CompositorNodeSepYCCA', 'SEPYCCA', 'Separate YCbCrA'),
    ('CompositorNodeCombYCCA', 'COMBYCCA', 'Combine YCbCrA'),
)
# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
compo_filter_nodes_props = (
    ('CompositorNodeBlur', 'BLUR', 'Blur'),
    ('CompositorNodeBilateralblur', 'BILATERALBLUR', 'Bilateral Blur'),
    ('CompositorNodeDilateErode', 'DILATEERODE', 'Dilate/Erode'),
    ('CompositorNodeDespeckle', 'DESPECKLE', 'Despeckle'),
    ('CompositorNodeFilter', 'FILTER', 'Filter'),
    ('CompositorNodeBokehBlur', 'BOKEHBLUR', 'Bokeh Blur'),
    ('CompositorNodeVecBlur', 'VECBLUR', 'Vector Blur'),
    ('CompositorNodeDefocus', 'DEFOCUS', 'Defocus'),
    ('CompositorNodeGlare', 'GLARE', 'Glare'),
    ('CompositorNodeInpaint', 'INPAINT', 'Inpaint'),
    ('CompositorNodeDBlur', 'DBLUR', 'Directional Blur'),
    ('CompositorNodePixelate', 'PIXELATE', 'Pixelate'),
    ('CompositorNodeSunBeams', 'SUNBEAMS', 'Sun Beams'),
)
# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
compo_vector_nodes_props = (
    ('CompositorNodeNormal', 'NORMAL', 'Normal'),
    ('CompositorNodeMapValue', 'MAP_VALUE', 'Map Value'),
    ('CompositorNodeMapRange', 'MAP_RANGE', 'Map Range'),
    ('CompositorNodeNormalize', 'NORMALIZE', 'Normalize'),
    ('CompositorNodeCurveVec', 'CURVE_VEC', 'Vector Curves'),
)
# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
compo_matte_nodes_props = (
    ('CompositorNodeKeying', 'KEYING', 'Keying'),
    ('CompositorNodeKeyingScreen', 'KEYINGSCREEN', 'Keying Screen'),
    ('CompositorNodeChannelMatte', 'CHANNEL_MATTE', 'Channel Key'),
    ('CompositorNodeColorSpill', 'COLOR_SPILL', 'Color Spill'),
    ('CompositorNodeBoxMask', 'BOXMASK', 'Box Mask'),
    ('CompositorNodeEllipseMask', 'ELLIPSEMASK', 'Ellipse Mask'),
    ('CompositorNodeLumaMatte', 'LUMA_MATTE', 'Luminance Key'),
    ('CompositorNodeDiffMatte', 'DIFF_MATTE', 'Difference Key'),
    ('CompositorNodeDistanceMatte', 'DISTANCE_MATTE', 'Distance Key'),
    ('CompositorNodeChromaMatte', 'CHROMA_MATTE', 'Chroma Key'),
    ('CompositorNodeColorMatte', 'COLOR_MATTE', 'Color Key'),
    ('CompositorNodeDoubleEdgeMask', 'DOUBLEEDGEMASK', 'Double Edge Mask'),
)
# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
compo_distort_nodes_props = (
    ('CompositorNodeScale', 'SCALE', 'Scale'),
    ('CompositorNodeLensdist', 'LENSDIST', 'Lens Distortion'),
    ('CompositorNodeMovieDistortion', 'MOVIEDISTORTION', 'Movie Distortion'),
    ('CompositorNodeTranslate', 'TRANSLATE', 'Translate'),
    ('CompositorNodeRotate', 'ROTATE', 'Rotate'),
    ('CompositorNodeFlip', 'FLIP', 'Flip'),
    ('CompositorNodeCrop', 'CROP', 'Crop'),
    ('CompositorNodeDisplace', 'DISPLACE', 'Displace'),
    ('CompositorNodeMapUV', 'MAP_UV', 'Map UV'),
    ('CompositorNodeTransform', 'TRANSFORM', 'Transform'),
    ('CompositorNodeStabilize', 'STABILIZE2D', 'Stabilize 2D'),
    ('CompositorNodePlaneTrackDeform', 'PLANETRACKDEFORM', 'Plane Track Deform'),
    ('CompositorNodeCornerPin', 'CORNERPIN', 'Corner Pin'),
)
# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
compo_layout_nodes_props = (
    ('NodeFrame', 'FRAME', 'Frame'),
    ('NodeReroute', 'REROUTE', 'Reroute'),
    ('CompositorNodeSwitch', 'SWITCH', 'Switch'),
)
# Blender Render material nodes
# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
blender_mat_input_nodes_props = (
    ('ShaderNodeMaterial', 'MATERIAL', 'Material'),
    ('ShaderNodeCameraData', 'CAMERA', 'Camera Data'),
    ('ShaderNodeLampData', 'LAMP', 'Lamp Data'),
    ('ShaderNodeValue', 'VALUE', 'Value'),
    ('ShaderNodeRGB', 'RGB', 'RGB'),
    ('ShaderNodeTexture', 'TEXTURE', 'Texture'),
    ('ShaderNodeGeometry', 'GEOMETRY', 'Geometry'),
    ('ShaderNodeExtendedMaterial', 'MATERIAL_EXT', 'Extended Material'),
)

# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
blender_mat_output_nodes_props = (
    ('ShaderNodeOutput', 'OUTPUT', 'Output'),
)

# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
blender_mat_color_nodes_props = (
    ('ShaderNodeMixRGB', 'MIX_RGB', 'MixRGB'),
    ('ShaderNodeRGBCurve', 'CURVE_RGB', 'RGB Curves'),
    ('ShaderNodeInvert', 'INVERT', 'Invert'),
    ('ShaderNodeHueSaturation', 'HUE_SAT', 'Hue/Saturation'),
)

# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
blender_mat_vector_nodes_props = (
    ('ShaderNodeNormal', 'NORMAL', 'Normal'),
    ('ShaderNodeMapping', 'MAPPING', 'Mapping'),
    ('ShaderNodeVectorCurve', 'CURVE_VEC', 'Vector Curves'),
)

# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
blender_mat_converter_nodes_props = (
    ('ShaderNodeValToRGB', 'VALTORGB', 'ColorRamp'),
    ('ShaderNodeRGBToBW', 'RGBTOBW', 'RGB to BW'),
    ('ShaderNodeMath', 'MATH', 'Math'),
    ('ShaderNodeVectorMath', 'VECT_MATH', 'Vector Math'),
    ('ShaderNodeSqueeze', 'SQUEEZE', 'Squeeze Value'),
    ('ShaderNodeSeparateRGB', 'SEPRGB', 'Separate RGB'),
    ('ShaderNodeCombineRGB', 'COMBRGB', 'Combine RGB'),
    ('ShaderNodeSeparateHSV', 'SEPHSV', 'Separate HSV'),
    ('ShaderNodeCombineHSV', 'COMBHSV', 'Combine HSV'),
)

# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
blender_mat_layout_nodes_props = (
    ('NodeReroute', 'REROUTE', 'Reroute'),
)

# Texture Nodes
# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
texture_input_nodes_props = (
    ('TextureNodeCurveTime', 'CURVE_TIME', 'Curve Time'),
    ('TextureNodeCoordinates', 'COORD', 'Coordinates'),
    ('TextureNodeTexture', 'TEXTURE', 'Texture'),
    ('TextureNodeImage', 'IMAGE', 'Image'),
)

# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
texture_output_nodes_props = (
    ('TextureNodeOutput', 'OUTPUT', 'Output'),
    ('TextureNodeViewer', 'VIEWER', 'Viewer'),
)

# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
texture_color_nodes_props = (
    ('TextureNodeMixRGB', 'MIX_RGB', 'Mix RGB'),
    ('TextureNodeCurveRGB', 'CURVE_RGB', 'RGB Curves'),
    ('TextureNodeInvert', 'INVERT', 'Invert'),
    ('TextureNodeHueSaturation', 'HUE_SAT', 'Hue/Saturation'),
    ('TextureNodeCompose', 'COMPOSE', 'Combine RGBA'),
    ('TextureNodeDecompose', 'DECOMPOSE', 'Separate RGBA'),
)

# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
texture_pattern_nodes_props = (
    ('TextureNodeChecker', 'CHECKER', 'Checker'),
    ('TextureNodeBricks', 'BRICKS', 'Bricks'),
)

# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
texture_textures_nodes_props = (
    ('TextureNodeTexNoise', 'TEX_NOISE', 'Noise'),
    ('TextureNodeTexDistNoise', 'TEX_DISTNOISE', 'Distorted Noise'),
    ('TextureNodeTexClouds', 'TEX_CLOUDS', 'Clouds'),
    ('TextureNodeTexBlend', 'TEX_BLEND', 'Blend'),
    ('TextureNodeTexVoronoi', 'TEX_VORONOI', 'Voronoi'),
    ('TextureNodeTexMagic', 'TEX_MAGIC', 'Magic'),
    ('TextureNodeTexMarble', 'TEX_MARBLE', 'Marble'),
    ('TextureNodeTexWood', 'TEX_WOOD', 'Wood'),
    ('TextureNodeTexMusgrave', 'TEX_MUSGRAVE', 'Musgrave'),
    ('TextureNodeTexStucci', 'TEX_STUCCI', 'Stucci'),
)

# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
texture_converter_nodes_props = (
    ('TextureNodeMath', 'MATH', 'Math'),
    ('TextureNodeValToRGB', 'VALTORGB', 'ColorRamp'),
    ('TextureNodeRGBToBW', 'RGBTOBW', 'RGB to BW'),
    ('TextureNodeValToNor', 'VALTONOR', 'Value to Normal'),
    ('TextureNodeDistance', 'DISTANCE', 'Distance'),
)

# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
texture_distort_nodes_props = (
    ('TextureNodeScale', 'SCALE', 'Scale'),
    ('TextureNodeTranslate', 'TRANSLATE', 'Translate'),
    ('TextureNodeRotate', 'ROTATE', 'Rotate'),
    ('TextureNodeAt', 'AT', 'At'),
)

# (rna_type.identifier, type, rna_type.name)
# Keeping mixed case to avoid having to translate entries when adding new nodes in operators.
texture_layout_nodes_props = (
    ('NodeReroute', 'REROUTE', 'Reroute'),
)

# list of blend types of "Mix" nodes in a form that can be used as 'items' for EnumProperty.
# used list, not tuple for easy merging with other lists.
blend_types = [
    ('MIX', 'Mix', 'Mix Mode'),
    ('ADD', 'Add', 'Add Mode'),
    ('MULTIPLY', 'Multiply', 'Multiply Mode'),
    ('SUBTRACT', 'Subtract', 'Subtract Mode'),
    ('SCREEN', 'Screen', 'Screen Mode'),
    ('DIVIDE', 'Divide', 'Divide Mode'),
    ('DIFFERENCE', 'Difference', 'Difference Mode'),
    ('DARKEN', 'Darken', 'Darken Mode'),
    ('LIGHTEN', 'Lighten', 'Lighten Mode'),
    ('OVERLAY', 'Overlay', 'Overlay Mode'),
    ('DODGE', 'Dodge', 'Dodge Mode'),
    ('BURN', 'Burn', 'Burn Mode'),
    ('HUE', 'Hue', 'Hue Mode'),
    ('SATURATION', 'Saturation', 'Saturation Mode'),
    ('VALUE', 'Value', 'Value Mode'),
    ('COLOR', 'Color', 'Color Mode'),
    ('SOFT_LIGHT', 'Soft Light', 'Soft Light Mode'),
    ('LINEAR_LIGHT', 'Linear Light', 'Linear Light Mode'),
]

# list of operations of "Math" nodes in a form that can be used as 'items' for EnumProperty.
# used list, not tuple for easy merging with other lists.
operations = [
    ('ADD', 'Add', 'Add Mode'),
    ('SUBTRACT', 'Subtract', 'Subtract Mode'),
    ('MULTIPLY', 'Multiply', 'Multiply Mode'),
    ('DIVIDE', 'Divide', 'Divide Mode'),
    ('SINE', 'Sine', 'Sine Mode'),
    ('COSINE', 'Cosine', 'Cosine Mode'),
    ('TANGENT', 'Tangent', 'Tangent Mode'),
    ('ARCSINE', 'Arcsine', 'Arcsine Mode'),
    ('ARCCOSINE', 'Arccosine', 'Arccosine Mode'),
    ('ARCTANGENT', 'Arctangent', 'Arctangent Mode'),
    ('POWER', 'Power', 'Power Mode'),
    ('LOGARITHM', 'Logatithm', 'Logarithm Mode'),
    ('MINIMUM', 'Minimum', 'Minimum Mode'),
    ('MAXIMUM', 'Maximum', 'Maximum Mode'),
    ('ROUND', 'Round', 'Round Mode'),
    ('LESS_THAN', 'Less Than', 'Less Than Mode'),
    ('GREATER_THAN', 'Greater Than', 'Greater Than Mode'),
    ('MODULO', 'Modulo', 'Modulo Mode'),
    ('ABSOLUTE', 'Absolute', 'Absolute Mode'),
]

# in NWBatchChangeNodes additional types/operations. Can be used as 'items' for EnumProperty.
# used list, not tuple for easy merging with other lists.
navs = [
    ('CURRENT', 'Current', 'Leave at current state'),
    ('NEXT', 'Next', 'Next blend type/operation'),
    ('PREV', 'Prev', 'Previous blend type/operation'),
]

draw_color_sets = {
    "red_white": (
        (1.0, 1.0, 1.0, 0.7),
        (1.0, 0.0, 0.0, 0.7),
        (0.8, 0.2, 0.2, 1.0)
    ),
    "green": (
        (0.0, 0.0, 0.0, 1.0),
        (0.38, 0.77, 0.38, 1.0),
        (0.38, 0.77, 0.38, 1.0)
    ),
    "yellow": (
        (0.0, 0.0, 0.0, 1.0),
        (0.77, 0.77, 0.16, 1.0),
        (0.77, 0.77, 0.16, 1.0)
    ),
    "purple": (
        (0.0, 0.0, 0.0, 1.0),
        (0.38, 0.38, 0.77, 1.0),
        (0.38, 0.38, 0.77, 1.0)
    ),
    "grey": (
        (0.0, 0.0, 0.0, 1.0),
        (0.63, 0.63, 0.63, 1.0),
        (0.63, 0.63, 0.63, 1.0)
    ),
    "black": (
        (1.0, 1.0, 1.0, 0.7),
        (0.0, 0.0, 0.0, 0.7),
        (0.2, 0.2, 0.2, 1.0)
    )
}


def nice_hotkey_name(punc):
    # convert the ugly string name into the actual character
    pairs = (
        ('LEFTMOUSE', "LMB"),
        ('MIDDLEMOUSE', "MMB"),
        ('RIGHTMOUSE', "RMB"),
        ('SELECTMOUSE', "Select"),
        ('WHEELUPMOUSE', "Wheel Up"),
        ('WHEELDOWNMOUSE', "Wheel Down"),
        ('WHEELINMOUSE', "Wheel In"),
        ('WHEELOUTMOUSE', "Wheel Out"),
        ('ZERO', "0"),
        ('ONE', "1"),
        ('TWO', "2"),
        ('THREE', "3"),
        ('FOUR', "4"),
        ('FIVE', "5"),
        ('SIX', "6"),
        ('SEVEN', "7"),
        ('EIGHT', "8"),
        ('NINE', "9"),
        ('OSKEY', "Super"),
        ('RET', "Enter"),
        ('LINE_FEED', "Enter"),
        ('SEMI_COLON', ";"),
        ('PERIOD', "."),
        ('COMMA', ","),
        ('QUOTE', '"'),
        ('MINUS', "-"),
        ('SLASH', "/"),
        ('BACK_SLASH', "\\"),
        ('EQUAL', "="),
        ('NUMPAD_1', "Numpad 1"),
        ('NUMPAD_2', "Numpad 2"),
        ('NUMPAD_3', "Numpad 3"),
        ('NUMPAD_4', "Numpad 4"),
        ('NUMPAD_5', "Numpad 5"),
        ('NUMPAD_6', "Numpad 6"),
        ('NUMPAD_7', "Numpad 7"),
        ('NUMPAD_8', "Numpad 8"),
        ('NUMPAD_9', "Numpad 9"),
        ('NUMPAD_0', "Numpad 0"),
        ('NUMPAD_PERIOD', "Numpad ."),
        ('NUMPAD_SLASH', "Numpad /"),
        ('NUMPAD_ASTERIX', "Numpad *"),
        ('NUMPAD_MINUS', "Numpad -"),
        ('NUMPAD_ENTER', "Numpad Enter"),
        ('NUMPAD_PLUS', "Numpad +"),
    )
    nice_punc = False
    for (ugly, nice) in pairs:
        if punc == ugly:
            nice_punc = nice
            break
    if not nice_punc:
        nice_punc = punc.replace("_", " ").title()
    return nice_punc


def force_update(context):
    context.space_data.node_tree.update_tag()


def dpifac():
    prefs = bpy.context.user_preferences.system
    return prefs.dpi * prefs.pixel_size / 72


def node_mid_pt(node, axis):
    if axis == 'x':
        d = node.location.x + (node.dimensions.x / 2)
    elif axis == 'y':
        d = node.location.y - (node.dimensions.y / 2)
    else:
        d = 0
    return d


def autolink(node1, node2, links):
    link_made = False

    for outp in node1.outputs:
        for inp in node2.inputs:
            if not inp.is_linked and inp.name == outp.name:
                link_made = True
                links.new(outp, inp)
                return True

    for outp in node1.outputs:
        for inp in node2.inputs:
            if not inp.is_linked and inp.type == outp.type:
                link_made = True
                links.new(outp, inp)
                return True

    # force some connection even if the type doesn't match
    for outp in node1.outputs:
        for inp in node2.inputs:
            if not inp.is_linked:
                link_made = True
                links.new(outp, inp)
                return True

    # even if no sockets are open, force one of matching type
    for outp in node1.outputs:
        for inp in node2.inputs:
            if inp.type == outp.type:
                link_made = True
                links.new(outp, inp)
                return True

    # do something!
    for outp in node1.outputs:
        for inp in node2.inputs:
            link_made = True
            links.new(outp, inp)
            return True

    print("Could not make a link from " + node1.name + " to " + node2.name)
    return link_made


def node_at_pos(nodes, context, event):
    nodes_near_mouse = []
    nodes_under_mouse = []
    target_node = None

    store_mouse_cursor(context, event)
    x, y = context.space_data.cursor_location
    x = x
    y = y

    # Make a list of each corner (and middle of border) for each node.
    # Will be sorted to find nearest point and thus nearest node
    node_points_with_dist = []
    for node in nodes:
        skipnode = False
        if node.type != 'FRAME':  # no point trying to link to a frame node
            locx = node.location.x
            locy = node.location.y
            dimx = node.dimensions.x/dpifac()
            dimy = node.dimensions.y/dpifac()
            if node.parent:
                locx += node.parent.location.x
                locy += node.parent.location.y
                if node.parent.parent:
                    locx += node.parent.parent.location.x
                    locy += node.parent.parent.location.y
                    if node.parent.parent.parent:
                        locx += node.parent.parent.parent.location.x
                        locy += node.parent.parent.parent.location.y
                        if node.parent.parent.parent.parent:
                            # Support three levels or parenting
                            # There's got to be a better way to do this...
                            skipnode = True
            if not skipnode:
                node_points_with_dist.append([node, hypot(x - locx, y - locy)])  # Top Left
                node_points_with_dist.append([node, hypot(x - (locx + dimx), y - locy)])  # Top Right
                node_points_with_dist.append([node, hypot(x - locx, y - (locy - dimy))])  # Bottom Left
                node_points_with_dist.append([node, hypot(x - (locx + dimx), y - (locy - dimy))])  # Bottom Right

                node_points_with_dist.append([node, hypot(x - (locx + (dimx / 2)), y - locy)])  # Mid Top
                node_points_with_dist.append([node, hypot(x - (locx + (dimx / 2)), y - (locy - dimy))])  # Mid Bottom
                node_points_with_dist.append([node, hypot(x - locx, y - (locy - (dimy / 2)))])  # Mid Left
                node_points_with_dist.append([node, hypot(x - (locx + dimx), y - (locy - (dimy / 2)))])  # Mid Right

    nearest_node = sorted(node_points_with_dist, key=lambda k: k[1])[0][0]

    for node in nodes:
        if node.type != 'FRAME' and skipnode == False:
            locx = node.location.x
            locy = node.location.y
            dimx = node.dimensions.x/dpifac()
            dimy = node.dimensions.y/dpifac()
            if node.parent:
                locx += node.parent.location.x
                locy += node.parent.location.y
            if (locx <= x <= locx + dimx) and \
               (locy - dimy <= y <= locy):
                nodes_under_mouse.append(node)

    if len(nodes_under_mouse) == 1:
        if nodes_under_mouse[0] != nearest_node:
            target_node = nodes_under_mouse[0]  # use the node under the mouse if there is one and only one
        else:
            target_node = nearest_node  # else use the nearest node
    else:
        target_node = nearest_node
    return target_node


def store_mouse_cursor(context, event):
    space = context.space_data
    v2d = context.region.view2d
    tree = space.edit_tree

    # convert mouse position to the View2D for later node placement
    if context.region.type == 'WINDOW':
        space.cursor_location_from_region(event.mouse_region_x, event.mouse_region_y)
    else:
        space.cursor_location = tree.view_center


def draw_line(x1, y1, x2, y2, size, colour=[1.0, 1.0, 1.0, 0.7]):
    shademodel_state = bgl.Buffer(bgl.GL_INT, 1)
    bgl.glGetIntegerv(bgl.GL_SHADE_MODEL, shademodel_state)

    bgl.glEnable(bgl.GL_BLEND)
    bgl.glLineWidth(size * dpifac())
    bgl.glShadeModel(bgl.GL_SMOOTH)
    bgl.glEnable(bgl.GL_LINE_SMOOTH)

    bgl.glBegin(bgl.GL_LINE_STRIP)
    try:
        bgl.glColor4f(colour[0]+(1.0-colour[0])/4, colour[1]+(1.0-colour[1])/4, colour[2]+(1.0-colour[2])/4, colour[3]+(1.0-colour[3])/4)
        bgl.glVertex2f(x1, y1)
        bgl.glColor4f(colour[0], colour[1], colour[2], colour[3])
        bgl.glVertex2f(x2, y2)
    except:
        pass
    bgl.glEnd()

    bgl.glShadeModel(shademodel_state[0])
    bgl.glDisable(bgl.GL_LINE_SMOOTH)


def draw_circle(mx, my, radius, colour=[1.0, 1.0, 1.0, 0.7]):
    bgl.glEnable(bgl.GL_LINE_SMOOTH)
    bgl.glBegin(bgl.GL_TRIANGLE_FAN)
    bgl.glColor4f(colour[0], colour[1], colour[2], colour[3])
    radius = radius * dpifac()
    sides = 12
    for i in range(sides + 1):
        cosine = radius * cos(i * 2 * pi / sides) + mx
        sine = radius * sin(i * 2 * pi / sides) + my
        bgl.glVertex2f(cosine, sine)
    bgl.glEnd()
    bgl.glDisable(bgl.GL_LINE_SMOOTH)


def draw_rounded_node_border(node, radius=8, colour=[1.0, 1.0, 1.0, 0.7]):
    bgl.glEnable(bgl.GL_BLEND)
    bgl.glEnable(bgl.GL_LINE_SMOOTH)

    area_width = bpy.context.area.width - (16*dpifac()) - 1
    bottom_bar = (16*dpifac()) + 1
    sides = 16
    radius = radius*dpifac()
    bgl.glColor4f(colour[0], colour[1], colour[2], colour[3])

    nlocx = (node.location.x+1)*dpifac()
    nlocy = (node.location.y+1)*dpifac()
    ndimx = node.dimensions.x
    ndimy = node.dimensions.y
    # This is a stupid way to do this... TODO use while loop
    if node.parent:
        nlocx += node.parent.location.x
        nlocy += node.parent.location.y
        if node.parent.parent:
            nlocx += node.parent.parent.location.x
            nlocy += node.parent.parent.location.y
            if node.parent.parent.parent:
                nlocx += node.parent.parent.parent.location.x
                nlocy += node.parent.parent.parent.location.y

    if node.hide:
        nlocx += -1
        nlocy += 5
    if node.type == 'REROUTE':
        #nlocx += 1
        nlocy -= 1
        ndimx = 0
        ndimy = 0
        radius += 6

    # Top left corner
    bgl.glBegin(bgl.GL_TRIANGLE_FAN)
    mx, my = bpy.context.region.view2d.view_to_region(nlocx, nlocy, clip=False)
    bgl.glVertex2f(mx,my)
    for i in range(sides+1):
        if (4<=i<=8):
            if my > bottom_bar and mx < area_width:
                cosine = radius * cos(i * 2 * pi / sides) + mx
                sine = radius * sin(i * 2 * pi / sides) + my
                bgl.glVertex2f(cosine, sine)
    bgl.glEnd()

    # Top right corner
    bgl.glBegin(bgl.GL_TRIANGLE_FAN)
    mx, my = bpy.context.region.view2d.view_to_region(nlocx + ndimx, nlocy, clip=False)
    bgl.glVertex2f(mx,my)
    for i in range(sides+1):
        if (0<=i<=4):
            if my > bottom_bar and mx < area_width:
                cosine = radius * cos(i * 2 * pi / sides) + mx
                sine = radius * sin(i * 2 * pi / sides) + my
                bgl.glVertex2f(cosine, sine)
    bgl.glEnd()

    # Bottom left corner
    bgl.glBegin(bgl.GL_TRIANGLE_FAN)
    mx, my = bpy.context.region.view2d.view_to_region(nlocx, nlocy - ndimy, clip=False)
    bgl.glVertex2f(mx,my)
    for i in range(sides+1):
        if (8<=i<=12):
            if my > bottom_bar and mx < area_width:
                cosine = radius * cos(i * 2 * pi / sides) + mx
                sine = radius * sin(i * 2 * pi / sides) + my
                bgl.glVertex2f(cosine, sine)
    bgl.glEnd()

    # Bottom right corner
    bgl.glBegin(bgl.GL_TRIANGLE_FAN)
    mx, my = bpy.context.region.view2d.view_to_region(nlocx + ndimx, nlocy - ndimy, clip=False)
    bgl.glVertex2f(mx,my)
    for i in range(sides+1):
        if (12<=i<=16):
            if my > bottom_bar and mx < area_width:
                cosine = radius * cos(i * 2 * pi / sides) + mx
                sine = radius * sin(i * 2 * pi / sides) + my
                bgl.glVertex2f(cosine, sine)
    bgl.glEnd()


    # Left edge
    bgl.glBegin(bgl.GL_QUADS)
    m1x, m1y = bpy.context.region.view2d.view_to_region(nlocx, nlocy, clip=False)
    m2x, m2y = bpy.context.region.view2d.view_to_region(nlocx, nlocy - ndimy, clip=False)
    m1y = max(m1y, bottom_bar)
    m2y = max(m2y, bottom_bar)
    if m1x < area_width and m2x < area_width:
        bgl.glVertex2f(m2x-radius,m2y)  # draw order is important, start with bottom left and go anti-clockwise
        bgl.glVertex2f(m2x,m2y)
        bgl.glVertex2f(m1x,m1y)
        bgl.glVertex2f(m1x-radius,m1y)
    bgl.glEnd()

    # Top edge
    bgl.glBegin(bgl.GL_QUADS)
    m1x, m1y = bpy.context.region.view2d.view_to_region(nlocx, nlocy, clip=False)
    m2x, m2y = bpy.context.region.view2d.view_to_region(nlocx + ndimx, nlocy, clip=False)
    m1x = min(m1x, area_width)
    m2x = min(m2x, area_width)
    if m1y > bottom_bar and m2y > bottom_bar:
        bgl.glVertex2f(m1x,m2y)  # draw order is important, start with bottom left and go anti-clockwise
        bgl.glVertex2f(m2x,m2y)
        bgl.glVertex2f(m2x,m1y+radius)
        bgl.glVertex2f(m1x,m1y+radius)
    bgl.glEnd()

    # Right edge
    bgl.glBegin(bgl.GL_QUADS)
    m1x, m1y = bpy.context.region.view2d.view_to_region(nlocx + ndimx, nlocy, clip=False)
    m2x, m2y = bpy.context.region.view2d.view_to_region(nlocx + ndimx, nlocy - ndimy, clip=False)
    m1y = max(m1y, bottom_bar)
    m2y = max(m2y, bottom_bar)
    if m1x < area_width and m2x < area_width:
        bgl.glVertex2f(m2x,m2y)  # draw order is important, start with bottom left and go anti-clockwise
        bgl.glVertex2f(m2x+radius,m2y)
        bgl.glVertex2f(m1x+radius,m1y)
        bgl.glVertex2f(m1x,m1y)
    bgl.glEnd()

    # Bottom edge
    bgl.glBegin(bgl.GL_QUADS)
    m1x, m1y = bpy.context.region.view2d.view_to_region(nlocx, nlocy-ndimy, clip=False)
    m2x, m2y = bpy.context.region.view2d.view_to_region(nlocx + ndimx, nlocy-ndimy, clip=False)
    m1x = min(m1x, area_width)
    m2x = min(m2x, area_width)
    if m1y > bottom_bar and m2y > bottom_bar:
        bgl.glVertex2f(m1x,m2y)  # draw order is important, start with bottom left and go anti-clockwise
        bgl.glVertex2f(m2x,m2y)
        bgl.glVertex2f(m2x,m1y-radius)
        bgl.glVertex2f(m1x,m1y-radius)
    bgl.glEnd()


    # Restore defaults
    bgl.glDisable(bgl.GL_BLEND)
    bgl.glDisable(bgl.GL_LINE_SMOOTH)


def draw_callback_nodeoutline(self, context, mode):
    if self.mouse_path:
        nodes, links = get_nodes_links(context)
        bgl.glEnable(bgl.GL_LINE_SMOOTH)

        if mode == "LINK":
            col_outer = [1.0, 0.2, 0.2, 0.4]
            col_inner = [0.0, 0.0, 0.0, 0.5]
            col_circle_inner = [0.3, 0.05, 0.05, 1.0]
        elif mode == "LINKMENU":
            col_outer = [0.4, 0.6, 1.0, 0.4]
            col_inner = [0.0, 0.0, 0.0, 0.5]
            col_circle_inner = [0.08, 0.15, .3, 1.0]
        elif mode == "MIX":
            col_outer = [0.2, 1.0, 0.2, 0.4]
            col_inner = [0.0, 0.0, 0.0, 0.5]
            col_circle_inner = [0.05, 0.3, 0.05, 1.0]

        m1x = self.mouse_path[0][0]
        m1y = self.mouse_path[0][1]
        m2x = self.mouse_path[-1][0]
        m2y = self.mouse_path[-1][1]

        n1 = nodes[context.scene.NWLazySource]
        n2 = nodes[context.scene.NWLazyTarget]

        if n1 == n2:
            col_outer = [0.4, 0.4, 0.4, 0.4]
            col_inner = [0.0, 0.0, 0.0, 0.5]
            col_circle_inner = [0.2, 0.2, 0.2, 1.0]

        draw_rounded_node_border(n1, radius=6, colour=col_outer)  # outline
        draw_rounded_node_border(n1, radius=5, colour=col_inner)  # inner
        draw_rounded_node_border(n2, radius=6, colour=col_outer)  # outline
        draw_rounded_node_border(n2, radius=5, colour=col_inner)  # inner

        draw_line(m1x, m1y, m2x, m2y, 5, col_outer)  # line outline
        draw_line(m1x, m1y, m2x, m2y, 2, col_inner)  # line inner

        # circle outline
        draw_circle(m1x, m1y, 7, col_outer)
        draw_circle(m2x, m2y, 7, col_outer)

        # circle inner
        draw_circle(m1x, m1y, 5, col_circle_inner)
        draw_circle(m2x, m2y, 5, col_circle_inner)

        # restore opengl defaults
        bgl.glLineWidth(1)
        bgl.glDisable(bgl.GL_BLEND)
        bgl.glColor4f(0.0, 0.0, 0.0, 1.0)

        bgl.glDisable(bgl.GL_LINE_SMOOTH)


def get_nodes_links(context):
    tree = context.space_data.node_tree

    # Get nodes from currently edited tree.
    # If user is editing a group, space_data.node_tree is still the base level (outside group).
    # context.active_node is in the group though, so if space_data.node_tree.nodes.active is not
    # the same as context.active_node, the user is in a group.
    # Check recursively until we find the real active node_tree:
    if tree.nodes.active:
        while tree.nodes.active != context.active_node:
            tree = tree.nodes.active.node_tree

    return tree.nodes, tree.links

# Principled prefs
class NWPrincipledPreferences(bpy.types.PropertyGroup):
    base_color = StringProperty(
        name='Base Color',
        default='diffuse diff albedo base col color',
        description='Naming Components for Base Color maps')
    sss_color = StringProperty(
        name='Subsurface Color',
        default='sss subsurface',
        description='Naming Components for Subsurface Color maps')
    metallic = StringProperty(
        name='Metallic',
        default='metallic metalness metal mtl',
        description='Naming Components for metallness maps')
    specular = StringProperty(
        name='Specular',
        default='specularity specular spec spc',
        description='Naming Components for Specular maps')
    normal = StringProperty(
        name='Normal',
        default='normal nor nrm nrml norm',
        description='Naming Components for Normal maps')
    bump = StringProperty(
        name='Bump',
        default='bump bmp',
        description='Naming Components for bump maps')
    rough = StringProperty(
        name='Roughness',
        default='roughness rough rgh',
        description='Naming Components for roughness maps')
    gloss = StringProperty(
        name='Gloss',
        default='gloss glossy glossyness',
        description='Naming Components for glossy maps')
    displacement = StringProperty(
        name='Displacement',
        default='displacement displace disp dsp height heightmap',
        description='Naming Components for displacement maps')

# Addon prefs
class NWNodeWrangler(bpy.types.AddonPreferences):
    bl_idname = __name__

    merge_hide = EnumProperty(
        name="Hide Mix nodes",
        items=(
            ("ALWAYS", "Always", "Always collapse the new merge nodes"),
            ("NON_SHADER", "Non-Shader", "Collapse in all cases except for shaders"),
            ("NEVER", "Never", "Never collapse the new merge nodes")
        ),
        default='NON_SHADER',
        description="When merging nodes with the Ctrl+Numpad0 hotkey (and similar) specifiy whether to collapse them or show the full node with options expanded")
    merge_position = EnumProperty(
        name="Mix Node Position",
        items=(
            ("CENTER", "Center", "Place the Mix node between the two nodes"),
            ("BOTTOM", "Bottom", "Place the Mix node at the same height as the lowest node")
        ),
        default='CENTER',
        description="When merging nodes with the Ctrl+Numpad0 hotkey (and similar) specifiy the position of the new nodes")

    show_hotkey_list = BoolProperty(
        name="Show Hotkey List",
        default=False,
        description="Expand this box into a list of all the hotkeys for functions in this addon"
    )
    hotkey_list_filter = StringProperty(
        name="        Filter by Name",
        default="",
        description="Show only hotkeys that have this text in their name"
    )
    show_principled_lists = BoolProperty(
        name="Show Principled naming tags",
        default=False,
        description="Expand this box into a list of all naming tags for principled texture setup"
    )
    principled_tags = bpy.props.PointerProperty(type=NWPrincipledPreferences)

    def draw(self, context):
        layout = self.layout
        col = layout.column()
        col.prop(self, "merge_position")
        col.prop(self, "merge_hide")

        box = layout.box()
        col = box.column(align=True)
        col.prop(self, "show_principled_lists", text='Edit tags for auto texture detection in Principled BSDF setup', toggle=True)
        if self.show_principled_lists:
            tags = self.principled_tags

            col.prop(tags, "base_color")
            col.prop(tags, "sss_color")
            col.prop(tags, "metallic")
            col.prop(tags, "specular")
            col.prop(tags, "rough")
            col.prop(tags, "gloss")
            col.prop(tags, "normal")
            col.prop(tags, "bump")
            col.prop(tags, "displacement")

        box = layout.box()
        col = box.column(align=True)
        hotkey_button_name = "Show Hotkey List"
        if self.show_hotkey_list:
            hotkey_button_name = "Hide Hotkey List"
        col.prop(self, "show_hotkey_list", text=hotkey_button_name, toggle=True)
        if self.show_hotkey_list:
            col.prop(self, "hotkey_list_filter", icon="VIEWZOOM")
            col.separator()
            for hotkey in kmi_defs:
                if hotkey[7]:
                    hotkey_name = hotkey[7]

                    if self.hotkey_list_filter.lower() in hotkey_name.lower():
                        row = col.row(align=True)
                        row.label(hotkey_name)
                        keystr = nice_hotkey_name(hotkey[1])
                        if hotkey[4]:
                            keystr = "Shift " + keystr
                        if hotkey[5]:
                            keystr = "Alt " + keystr
                        if hotkey[3]:
                            keystr = "Ctrl " + keystr
                        row.label(keystr)



def nw_check(context):
    space = context.space_data
    valid_trees = ["ShaderNodeTree", "CompositorNodeTree", "TextureNodeTree"]

    valid = False
    if space.type == 'NODE_EDITOR' and space.node_tree is not None and space.tree_type in valid_trees:
        valid = True

    return valid

class NWBase:
    @classmethod
    def poll(cls, context):
        return nw_check(context)


# OPERATORS
class NWLazyMix(Operator, NWBase):
    """Add a Mix RGB/Shader node by interactively drawing lines between nodes"""
    bl_idname = "node.nw_lazy_mix"
    bl_label = "Mix Nodes"
    bl_options = {'REGISTER', 'UNDO'}

    def modal(self, context, event):
        context.area.tag_redraw()
        nodes, links = get_nodes_links(context)
        cont = True

        start_pos = [event.mouse_region_x, event.mouse_region_y]

        node1 = None
        if not context.scene.NWBusyDrawing:
            node1 = node_at_pos(nodes, context, event)
            if node1:
                context.scene.NWBusyDrawing = node1.name
        else:
            if context.scene.NWBusyDrawing != 'STOP':
                node1 = nodes[context.scene.NWBusyDrawing]

        context.scene.NWLazySource = node1.name
        context.scene.NWLazyTarget = node_at_pos(nodes, context, event).name

        if event.type == 'MOUSEMOVE':
            self.mouse_path.append((event.mouse_region_x, event.mouse_region_y))

        elif event.type == 'RIGHTMOUSE':
            end_pos = [event.mouse_region_x, event.mouse_region_y]
            bpy.types.SpaceNodeEditor.draw_handler_remove(self._handle, 'WINDOW')

            node2 = None
            node2 = node_at_pos(nodes, context, event)
            if node2:
                context.scene.NWBusyDrawing = node2.name

            if node1 == node2:
                cont = False

            if cont:
                if node1 and node2:
                    for node in nodes:
                        node.select = False
                    node1.select = True
                    node2.select = True

                    bpy.ops.node.nw_merge_nodes(mode="MIX", merge_type="AUTO")

            context.scene.NWBusyDrawing = ""
            return {'FINISHED'}

        elif event.type == 'ESC':
            print('cancelled')
            bpy.types.SpaceNodeEditor.draw_handler_remove(self._handle, 'WINDOW')
            return {'CANCELLED'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        if context.area.type == 'NODE_EDITOR':
            # the arguments we pass the the callback
            args = (self, context, 'MIX')
            # Add the region OpenGL drawing callback
            # draw in view space with 'POST_VIEW' and 'PRE_VIEW'
            self._handle = bpy.types.SpaceNodeEditor.draw_handler_add(draw_callback_nodeoutline, args, 'WINDOW', 'POST_PIXEL')

            self.mouse_path = []

            context.window_manager.modal_handler_add(self)
            return {'RUNNING_MODAL'}
        else:
            self.report({'WARNING'}, "View3D not found, cannot run operator")
            return {'CANCELLED'}


class NWLazyConnect(Operator, NWBase):
    """Connect two nodes without clicking a specific socket (automatically determined"""
    bl_idname = "node.nw_lazy_connect"
    bl_label = "Lazy Connect"
    bl_options = {'REGISTER', 'UNDO'}
    with_menu = BoolProperty()

    def modal(self, context, event):
        context.area.tag_redraw()
        nodes, links = get_nodes_links(context)
        cont = True

        start_pos = [event.mouse_region_x, event.mouse_region_y]

        node1 = None
        if not context.scene.NWBusyDrawing:
            node1 = node_at_pos(nodes, context, event)
            if node1:
                context.scene.NWBusyDrawing = node1.name
        else:
            if context.scene.NWBusyDrawing != 'STOP':
                node1 = nodes[context.scene.NWBusyDrawing]

        context.scene.NWLazySource = node1.name
        context.scene.NWLazyTarget = node_at_pos(nodes, context, event).name

        if event.type == 'MOUSEMOVE':
            self.mouse_path.append((event.mouse_region_x, event.mouse_region_y))

        elif event.type == 'RIGHTMOUSE':
            end_pos = [event.mouse_region_x, event.mouse_region_y]
            bpy.types.SpaceNodeEditor.draw_handler_remove(self._handle, 'WINDOW')

            node2 = None
            node2 = node_at_pos(nodes, context, event)
            if node2:
                context.scene.NWBusyDrawing = node2.name

            if node1 == node2:
                cont = False

            link_success = False
            if cont:
                if node1 and node2:
                    original_sel = []
                    original_unsel = []
                    for node in nodes:
                        if node.select == True:
                            node.select = False
                            original_sel.append(node)
                        else:
                            original_unsel.append(node)
                    node1.select = True
                    node2.select = True

                    #link_success = autolink(node1, node2, links)
                    if self.with_menu:
                        if len(node1.outputs) > 1 and node2.inputs:
                            bpy.ops.wm.call_menu("INVOKE_DEFAULT", name=NWConnectionListOutputs.bl_idname)
                        elif len(node1.outputs) == 1:
                            bpy.ops.node.nw_call_inputs_menu(from_socket=0)
                    else:
                        link_success = autolink(node1, node2, links)

                    for node in original_sel:
                        node.select = True
                    for node in original_unsel:
                        node.select = False

            if link_success:
                force_update(context)
            context.scene.NWBusyDrawing = ""
            return {'FINISHED'}

        elif event.type == 'ESC':
            bpy.types.SpaceNodeEditor.draw_handler_remove(self._handle, 'WINDOW')
            return {'CANCELLED'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        if context.area.type == 'NODE_EDITOR':
            nodes, links = get_nodes_links(context)
            node = node_at_pos(nodes, context, event)
            if node:
                context.scene.NWBusyDrawing = node.name

            # the arguments we pass the the callback
            mode = "LINK"
            if self.with_menu:
                mode = "LINKMENU"
            args = (self, context, mode)
            # Add the region OpenGL drawing callback
            # draw in view space with 'POST_VIEW' and 'PRE_VIEW'
            self._handle = bpy.types.SpaceNodeEditor.draw_handler_add(draw_callback_nodeoutline, args, 'WINDOW', 'POST_PIXEL')

            self.mouse_path = []

            context.window_manager.modal_handler_add(self)
            return {'RUNNING_MODAL'}
        else:
            self.report({'WARNING'}, "View3D not found, cannot run operator")
            return {'CANCELLED'}


class NWDeleteUnused(Operator, NWBase):
    """Delete all nodes whose output is not used"""
    bl_idname = 'node.nw_del_unused'
    bl_label = 'Delete Unused Nodes'
    bl_options = {'REGISTER', 'UNDO'}

    delete_muted = BoolProperty(name="Delete Muted", description="Delete (but reconnect, like Ctrl-X) all muted nodes", default=True)
    delete_frames = BoolProperty(name="Delete Empty Frames", description="Delete all frames that have no nodes inside them", default=True)

    def is_unused_node(self, node):
        end_types = ['OUTPUT_MATERIAL', 'OUTPUT', 'VIEWER', 'COMPOSITE', \
                'SPLITVIEWER', 'OUTPUT_FILE', 'LEVELS', 'OUTPUT_LAMP', \
                'OUTPUT_WORLD', 'GROUP_INPUT', 'GROUP_OUTPUT', 'FRAME']
        if node.type in end_types:
            return False

        for output in node.outputs:
            if output.links:
                return False
        return True

    @classmethod
    def poll(cls, context):
        valid = False
        if nw_check(context):
            if context.space_data.node_tree.nodes:
                valid = True
        return valid

    def execute(self, context):
        nodes, links = get_nodes_links(context)

        # Store selection
        selection = []
        for node in nodes:
            if node.select == True:
                selection.append(node.name)

        for node in nodes:
            node.select = False

        deleted_nodes = []
        temp_deleted_nodes = []
        del_unused_iterations = len(nodes)
        for it in range(0, del_unused_iterations):
            temp_deleted_nodes = list(deleted_nodes)  # keep record of last iteration
            for node in nodes:
                if self.is_unused_node(node):
                    node.select = True
                    deleted_nodes.append(node.name)
                    bpy.ops.node.delete()

            if temp_deleted_nodes == deleted_nodes:  # stop iterations when there are no more nodes to be deleted
                break

        if self.delete_frames:
            repeat = True
            while repeat:
                frames_in_use = []
                frames = []
                repeat = False
                for node in nodes:
                    if node.parent:
                        frames_in_use.append(node.parent)
                for node in nodes:
                    if node.type == 'FRAME' and node not in frames_in_use:
                        frames.append(node)
                        if node.parent:
                            repeat = True  # repeat for nested frames
                for node in frames:
                    if node not in frames_in_use:
                        node.select = True
                        deleted_nodes.append(node.name)
                bpy.ops.node.delete()

        if self.delete_muted:
            for node in nodes:
                if node.mute:
                    node.select = True
                    deleted_nodes.append(node.name)
            bpy.ops.node.delete_reconnect()

        # get unique list of deleted nodes (iterations would count the same node more than once)
        deleted_nodes = list(set(deleted_nodes))
        for n in deleted_nodes:
            self.report({'INFO'}, "Node " + n + " deleted")
        num_deleted = len(deleted_nodes)
        n = ' node'
        if num_deleted > 1:
            n += 's'
        if num_deleted:
            self.report({'INFO'}, "Deleted " + str(num_deleted) + n)
        else:
            self.report({'INFO'}, "Nothing deleted")

        # Restore selection
        nodes, links = get_nodes_links(context)
        for node in nodes:
            if node.name in selection:
                node.select = True
        return {'FINISHED'}

    def invoke(self, context, event):
        return context.window_manager.invoke_confirm(self, event)


class NWSwapLinks(Operator, NWBase):
    """Swap the output connections of the two selected nodes, or two similar inputs of a single node"""
    bl_idname = 'node.nw_swap_links'
    bl_label = 'Swap Links'
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        valid = False
        if nw_check(context):
            if context.selected_nodes:
                valid = len(context.selected_nodes) <= 2
        return valid

    def execute(self, context):
        nodes, links = get_nodes_links(context)
        selected_nodes = context.selected_nodes
        n1 = selected_nodes[0]

        # Swap outputs
        if len(selected_nodes) == 2:
            n2 = selected_nodes[1]
            if n1.outputs and n2.outputs:
                n1_outputs = []
                n2_outputs = []

                out_index = 0
                for output in n1.outputs:
                    if output.links:
                        for link in output.links:
                            n1_outputs.append([out_index, link.to_socket])
                            links.remove(link)
                    out_index += 1

                out_index = 0
                for output in n2.outputs:
                    if output.links:
                        for link in output.links:
                            n2_outputs.append([out_index, link.to_socket])
                            links.remove(link)
                    out_index += 1

                for connection in n1_outputs:
                    try:
                        links.new(n2.outputs[connection[0]], connection[1])
                    except:
                        self.report({'WARNING'}, "Some connections have been lost due to differing numbers of output sockets")
                for connection in n2_outputs:
                    try:
                        links.new(n1.outputs[connection[0]], connection[1])
                    except:
                        self.report({'WARNING'}, "Some connections have been lost due to differing numbers of output sockets")
            else:
                if n1.outputs or n2.outputs:
                    self.report({'WARNING'}, "One of the nodes has no outputs!")
                else:
                    self.report({'WARNING'}, "Neither of the nodes have outputs!")

        # Swap Inputs
        elif len(selected_nodes) == 1:
            if n1.inputs:
                types = []
                i=0
                for i1 in n1.inputs:
                    if i1.is_linked:
                        similar_types = 0
                        for i2 in n1.inputs:
                            if i1.type == i2.type and i2.is_linked:
                                similar_types += 1
                        types.append ([i1, similar_types, i])
                    i += 1
                types.sort(key=lambda k: k[1], reverse=True)

                if types:
                    t = types[0]
                    if t[1] == 2:
                        for i2 in n1.inputs:
                            if t[0].type == i2.type == t[0].type and t[0] != i2 and i2.is_linked:
                                pair = [t[0], i2]
                        i1f = pair[0].links[0].from_socket
                        i1t = pair[0].links[0].to_socket
                        i2f = pair[1].links[0].from_socket
                        i2t = pair[1].links[0].to_socket
                        links.new(i1f, i2t)
                        links.new(i2f, i1t)
                    if t[1] == 1:
                        if len(types) == 1:
                            fs = t[0].links[0].from_socket
                            i = t[2]
                            links.remove(t[0].links[0])
                            if i+1 == len(n1.inputs):
                                i = -1
                            i += 1
                            while n1.inputs[i].is_linked:
                                i += 1
                            links.new(fs, n1.inputs[i])
                        elif len(types) == 2:
                            i1f = types[0][0].links[0].from_socket
                            i1t = types[0][0].links[0].to_socket
                            i2f = types[1][0].links[0].from_socket
                            i2t = types[1][0].links[0].to_socket
                            links.new(i1f, i2t)
                            links.new(i2f, i1t)

                else:
                    self.report({'WARNING'}, "This node has no input connections to swap!")
            else:
                self.report({'WARNING'}, "This node has no inputs to swap!")

        force_update(context)
        return {'FINISHED'}


class NWResetBG(Operator, NWBase):
    """Reset the zoom and position of the background image"""
    bl_idname = 'node.nw_bg_reset'
    bl_label = 'Reset Backdrop'
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        valid = False
        if nw_check(context):
            snode = context.space_data
            valid = snode.tree_type == 'CompositorNodeTree'
        return valid

    def execute(self, context):
        context.space_data.backdrop_zoom = 1
        context.space_data.backdrop_x = 0
        context.space_data.backdrop_y = 0
        return {'FINISHED'}


class NWAddAttrNode(Operator, NWBase):
    """Add an Attribute node with this name"""
    bl_idname = 'node.nw_add_attr_node'
    bl_label = 'Add UV map'
    attr_name = StringProperty()
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        bpy.ops.node.add_node('INVOKE_DEFAULT', use_transform=True, type="ShaderNodeAttribute")
        nodes, links = get_nodes_links(context)
        nodes.active.attribute_name = self.attr_name
        return {'FINISHED'}


class NWEmissionViewer(Operator, NWBase):
    bl_idname = "node.nw_emission_viewer"
    bl_label = "Emission Viewer"
    bl_description = "Connect active node to Emission Shader for shadeless previews"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        is_cycles = context.scene.render.engine == 'CYCLES'
        if nw_check(context):
            space = context.space_data
            if space.tree_type == 'ShaderNodeTree' and is_cycles:
                if context.active_node:
                    if context.active_node.type != "OUTPUT_MATERIAL" or context.active_node.type != "OUTPUT_WORLD":
                        return True
                else:
                    return True
        return False

    def invoke(self, context, event):
        space = context.space_data
        shader_type = space.shader_type
        if shader_type == 'OBJECT':
            if space.id not in [lamp for lamp in bpy.data.lamps]:  # cannot use bpy.data.lamps directly as iterable
                shader_output_type = "OUTPUT_MATERIAL"
                shader_output_ident = "ShaderNodeOutputMaterial"
                shader_viewer_ident = "ShaderNodeEmission"
            else:
                shader_output_type = "OUTPUT_LAMP"
                shader_output_ident = "ShaderNodeOutputLamp"
                shader_viewer_ident = "ShaderNodeEmission"

        elif shader_type == 'WORLD':
            shader_output_type = "OUTPUT_WORLD"
            shader_output_ident = "ShaderNodeOutputWorld"
            shader_viewer_ident = "ShaderNodeBackground"
        shader_types = [x[1] for x in shaders_shader_nodes_props]
        mlocx = event.mouse_region_x
        mlocy = event.mouse_region_y
        select_node = bpy.ops.node.select(mouse_x=mlocx, mouse_y=mlocy, extend=False)
        if 'FINISHED' in select_node:  # only run if mouse click is on a node
            nodes, links = get_nodes_links(context)
            in_group = context.active_node != space.node_tree.nodes.active
            active = nodes.active
            output_types = [x[1] for x in shaders_output_nodes_props]
            valid = False
            if active:
                if (active.name != "Emission Viewer") and (active.type not in output_types) and not in_group:
                    for out in active.outputs:
                        if not out.hide:
                            valid = True
                            break
            if valid:
                # get material_output node, store selection, deselect all
                materialout = None  # placeholder node
                selection = []
                for node in nodes:
                    if node.type == shader_output_type:
                        materialout = node
                    if node.select:
                        selection.append(node.name)
                    node.select = False
                if not materialout:
                    # get right-most location
                    sorted_by_xloc = (sorted(nodes, key=lambda x: x.location.x))
                    max_xloc_node = sorted_by_xloc[-1]
                    if max_xloc_node.name == 'Emission Viewer':
                        max_xloc_node = sorted_by_xloc[-2]

                    # get average y location
                    sum_yloc = 0
                    for node in nodes:
                        sum_yloc += node.location.y

                    new_locx = max_xloc_node.location.x + max_xloc_node.dimensions.x + 80
                    new_locy = sum_yloc / len(nodes)

                    materialout = nodes.new(shader_output_ident)
                    materialout.location.x = new_locx
                    materialout.location.y = new_locy
                    materialout.select = False
                # Analyze outputs, add "Emission Viewer" if needed, make links
                out_i = None
                valid_outputs = []
                for i, out in enumerate(active.outputs):
                    if not out.hide:
                        valid_outputs.append(i)
                if valid_outputs:
                    out_i = valid_outputs[0]  # Start index of node's outputs
                for i, valid_i in enumerate(valid_outputs):
                    for out_link in active.outputs[valid_i].links:
                        if "Emission Viewer" in out_link.to_node.name or (out_link.to_node == materialout and out_link.to_socket == materialout.inputs[0]):
                            if i < len(valid_outputs) - 1:
                                out_i = valid_outputs[i + 1]
                            else:
                                out_i = valid_outputs[0]
                make_links = []  # store sockets for new links
                if active.outputs:
                    # If output type not 'SHADER' - "Emission Viewer" needed
                    if active.outputs[out_i].type != 'SHADER':
                        # get Emission Viewer node
                        emission_exists = False
                        emission_placeholder = nodes[0]
                        for node in nodes:
                            if "Emission Viewer" in node.name:
                                emission_exists = True
                                emission_placeholder = node
                        if not emission_exists:
                            emission = nodes.new(shader_viewer_ident)
                            emission.hide = True
                            emission.location = [materialout.location.x, (materialout.location.y + 40)]
                            emission.label = "Viewer"
                            emission.name = "Emission Viewer"
                            emission.use_custom_color = True
                            emission.color = (0.6, 0.5, 0.4)
                            emission.select = False
                        else:
                            emission = emission_placeholder
                        make_links.append((active.outputs[out_i], emission.inputs[0]))

                        # If Viewer is connected to output by user, don't change those connections (patch by gandalf3)
                        if emission.outputs[0].links.__len__() > 0:
                            if not emission.outputs[0].links[0].to_node == materialout:
                                make_links.append((emission.outputs[0], materialout.inputs[0]))
                        else:
                            make_links.append((emission.outputs[0], materialout.inputs[0]))

                        # Set brightness of viewer to compensate for Film and CM exposure
                        intensity = 1/context.scene.cycles.film_exposure  # Film exposure is a multiplier
                        intensity /= pow(2, (context.scene.view_settings.exposure))  # CM exposure is measured in stops/EVs (2^x)
                        emission.inputs[1].default_value = intensity

                    else:
                        # Output type is 'SHADER', no Viewer needed. Delete Viewer if exists.
                        make_links.append((active.outputs[out_i], materialout.inputs[1 if active.outputs[out_i].name == "Volume" else 0]))
                        for node in nodes:
                            if node.name == 'Emission Viewer':
                                node.select = True
                                bpy.ops.node.delete()
                    for li_from, li_to in make_links:
                        links.new(li_from, li_to)
                # Restore selection
                nodes.active = active
                for node in nodes:
                    if node.name in selection:
                        node.select = True
                force_update(context)
            return {'FINISHED'}
        else:
            return {'CANCELLED'}


class NWFrameSelected(Operator, NWBase):
    bl_idname = "node.nw_frame_selected"
    bl_label = "Frame Selected"
    bl_description = "Add a frame node and parent the selected nodes to it"
    bl_options = {'REGISTER', 'UNDO'}
    label_prop = StringProperty(name='Label', default=' ', description='The visual name of the frame node')
    color_prop = FloatVectorProperty(name="Color", description="The color of the frame node", default=(0.6, 0.6, 0.6),
                                     min=0, max=1, step=1, precision=3, subtype='COLOR_GAMMA', size=3)

    def execute(self, context):
        nodes, links = get_nodes_links(context)
        selected = []
        for node in nodes:
            if node.select == True:
                selected.append(node)

        bpy.ops.node.add_node(type='NodeFrame')
        frm = nodes.active
        frm.label = self.label_prop
        frm.use_custom_color = True
        frm.color = self.color_prop

        for node in selected:
            node.parent = frm

        return {'FINISHED'}


class NWReloadImages(Operator, NWBase):
    bl_idname = "node.nw_reload_images"
    bl_label = "Reload Images"
    bl_description = "Update all the image nodes to match their files on disk"

    def execute(self, context):
        nodes, links = get_nodes_links(context)
        image_types = ["IMAGE", "TEX_IMAGE", "TEX_ENVIRONMENT", "TEXTURE"]
        num_reloaded = 0
        for node in nodes:
            if node.type in image_types:
                if node.type == "TEXTURE":
                    if node.texture:  # node has texture assigned
                        if node.texture.type in ['IMAGE', 'ENVIRONMENT_MAP']:
                            if node.texture.image:  # texture has image assigned
                                node.texture.image.reload()
                                num_reloaded += 1
                else:
                    if node.image:
                        node.image.reload()
                        num_reloaded += 1

        if num_reloaded:
            self.report({'INFO'}, "Reloaded images")
            print("Reloaded " + str(num_reloaded) + " images")
            force_update(context)
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "No images found to reload in this node tree")
            return {'CANCELLED'}


class NWSwitchNodeType(Operator, NWBase):
    """Switch type of selected nodes """
    bl_idname = "node.nw_swtch_node_type"
    bl_label = "Switch Node Type"
    bl_options = {'REGISTER', 'UNDO'}

    to_type = EnumProperty(
        name="Switch to type",
        items=list(shaders_input_nodes_props) +
        list(shaders_output_nodes_props) +
        list(shaders_shader_nodes_props) +
        list(shaders_texture_nodes_props) +
        list(shaders_color_nodes_props) +
        list(shaders_vector_nodes_props) +
        list(shaders_converter_nodes_props) +
        list(shaders_layout_nodes_props) +
        list(compo_input_nodes_props) +
        list(compo_output_nodes_props) +
        list(compo_color_nodes_props) +
        list(compo_converter_nodes_props) +
        list(compo_filter_nodes_props) +
        list(compo_vector_nodes_props) +
        list(compo_matte_nodes_props) +
        list(compo_distort_nodes_props) +
        list(compo_layout_nodes_props) +
        list(blender_mat_input_nodes_props) +
        list(blender_mat_output_nodes_props) +
        list(blender_mat_color_nodes_props) +
        list(blender_mat_vector_nodes_props) +
        list(blender_mat_converter_nodes_props) +
        list(blender_mat_layout_nodes_props) +
        list(texture_input_nodes_props) +
        list(texture_output_nodes_props) +
        list(texture_color_nodes_props) +
        list(texture_pattern_nodes_props) +
        list(texture_textures_nodes_props) +
        list(texture_converter_nodes_props) +
        list(texture_distort_nodes_props) +
        list(texture_layout_nodes_props)
    )

    def execute(self, context):
        nodes, links = get_nodes_links(context)
        to_type = self.to_type
        # Those types of nodes will not swap.
        src_excludes = ('NodeFrame')
        # Those attributes of nodes will be copied if possible
        attrs_to_pass = ('color', 'hide', 'label', 'mute', 'parent',
                         'show_options', 'show_preview', 'show_texture',
                         'use_alpha', 'use_clamp', 'use_custom_color', 'location'
                         )
        selected = [n for n in nodes if n.select]
        reselect = []
        for node in [n for n in selected if
                     n.rna_type.identifier not in src_excludes and
                     n.rna_type.identifier != to_type]:
            new_node = nodes.new(to_type)
            for attr in attrs_to_pass:
                if hasattr(node, attr) and hasattr(new_node, attr):
                    setattr(new_node, attr, getattr(node, attr))
            # set image datablock of dst to image of src
            if hasattr(node, 'image') and hasattr(new_node, 'image'):
                if node.image:
                    new_node.image = node.image
            # Special cases
            if new_node.type == 'SWITCH':
                new_node.hide = True
            # Dictionaries: src_sockets and dst_sockets:
            # 'INPUTS': input sockets ordered by type (entry 'MAIN' main type of inputs).
            # 'OUTPUTS': output sockets ordered by type (entry 'MAIN' main type of outputs).
            # in 'INPUTS' and 'OUTPUTS':
            # 'SHADER', 'RGBA', 'VECTOR', 'VALUE' - sockets of those types.
            # socket entry:
            # (index_in_type, socket_index, socket_name, socket_default_value, socket_links)
            src_sockets = {
                'INPUTS': {'SHADER': [], 'RGBA': [], 'VECTOR': [], 'VALUE': [], 'MAIN': None},
                'OUTPUTS': {'SHADER': [], 'RGBA': [], 'VECTOR': [], 'VALUE': [], 'MAIN': None},
            }
            dst_sockets = {
                'INPUTS': {'SHADER': [], 'RGBA': [], 'VECTOR': [], 'VALUE': [], 'MAIN': None},
                'OUTPUTS': {'SHADER': [], 'RGBA': [], 'VECTOR': [], 'VALUE': [], 'MAIN': None},
            }
            types_order_one = 'SHADER', 'RGBA', 'VECTOR', 'VALUE'
            types_order_two = 'SHADER', 'VECTOR', 'RGBA', 'VALUE'
            # check src node to set src_sockets values and dst node to set dst_sockets dict values
            for sockets, nd in ((src_sockets, node), (dst_sockets, new_node)):
                # Check node's inputs and outputs and fill proper entries in "sockets" dict
                for in_out, in_out_name in ((nd.inputs, 'INPUTS'), (nd.outputs, 'OUTPUTS')):
                    # enumerate in inputs, then in outputs
                    # find name, default value and links of socket
                    for i, socket in enumerate(in_out):
                        the_name = socket.name
                        dval = None
                        # Not every socket, especially in outputs has "default_value"
                        if hasattr(socket, 'default_value'):
                            dval = socket.default_value
                        socket_links = []
                        for lnk in socket.links:
                            socket_links.append(lnk)
                        # check type of socket to fill proper keys.
                        for the_type in types_order_one:
                            if socket.type == the_type:
                                # create values for sockets['INPUTS'][the_type] and sockets['OUTPUTS'][the_type]
                                # entry structure: (index_in_type, socket_index, socket_name, socket_default_value, socket_links)
                                sockets[in_out_name][the_type].append((len(sockets[in_out_name][the_type]), i, the_name, dval, socket_links))
                    # Check which of the types in inputs/outputs is considered to be "main".
                    # Set values of sockets['INPUTS']['MAIN'] and sockets['OUTPUTS']['MAIN']
                    for type_check in types_order_one:
                        if sockets[in_out_name][type_check]:
                            sockets[in_out_name]['MAIN'] = type_check
                            break

            matches = {
                'INPUTS': {'SHADER': [], 'RGBA': [], 'VECTOR': [], 'VALUE_NAME': [], 'VALUE': [], 'MAIN': []},
                'OUTPUTS': {'SHADER': [], 'RGBA': [], 'VECTOR': [], 'VALUE_NAME': [], 'VALUE': [], 'MAIN': []},
            }

            for inout, soctype in (
                    ('INPUTS', 'MAIN',),
                    ('INPUTS', 'SHADER',),
                    ('INPUTS', 'RGBA',),
                    ('INPUTS', 'VECTOR',),
                    ('INPUTS', 'VALUE',),
                    ('OUTPUTS', 'MAIN',),
                    ('OUTPUTS', 'SHADER',),
                    ('OUTPUTS', 'RGBA',),
                    ('OUTPUTS', 'VECTOR',),
                    ('OUTPUTS', 'VALUE',),
            ):
                if src_sockets[inout][soctype] and dst_sockets[inout][soctype]:
                    if soctype == 'MAIN':
                        sc = src_sockets[inout][src_sockets[inout]['MAIN']]
                        dt = dst_sockets[inout][dst_sockets[inout]['MAIN']]
                    else:
                        sc = src_sockets[inout][soctype]
                        dt = dst_sockets[inout][soctype]
                    # start with 'dt' to determine number of possibilities.
                    for i, soc in enumerate(dt):
                        # if src main has enough entries - match them with dst main sockets by indexes.
                        if len(sc) > i:
                            matches[inout][soctype].append(((sc[i][1], sc[i][3]), (soc[1], soc[3])))
                        # add 'VALUE_NAME' criterion to inputs.
                        if inout == 'INPUTS' and soctype == 'VALUE':
                            for s in sc:
                                if s[2] == soc[2]:  # if names match
                                    # append src (index, dval), dst (index, dval)
                                    matches['INPUTS']['VALUE_NAME'].append(((s[1], s[3]), (soc[1], soc[3])))

            # When src ['INPUTS']['MAIN'] is 'VECTOR' replace 'MAIN' with matches VECTOR if possible.
            # This creates better links when relinking textures.
            if src_sockets['INPUTS']['MAIN'] == 'VECTOR' and matches['INPUTS']['VECTOR']:
                matches['INPUTS']['MAIN'] = matches['INPUTS']['VECTOR']

            # Pass default values and RELINK:
            for tp in ('MAIN', 'SHADER', 'RGBA', 'VECTOR', 'VALUE_NAME', 'VALUE'):
                # INPUTS: Base on matches in proper order.
                for (src_i, src_dval), (dst_i, dst_dval) in matches['INPUTS'][tp]:
                    # pass dvals
                    if src_dval and dst_dval and tp in {'RGBA', 'VALUE_NAME'}:
                        new_node.inputs[dst_i].default_value = src_dval
                    # Special case: switch to math
                    if node.type in {'MIX_RGB', 'ALPHAOVER', 'ZCOMBINE'} and\
                            new_node.type == 'MATH' and\
                            tp == 'MAIN':
                        new_dst_dval = max(src_dval[0], src_dval[1], src_dval[2])
                        new_node.inputs[dst_i].default_value = new_dst_dval
                        if node.type == 'MIX_RGB':
                            if node.blend_type in [o[0] for o in operations]:
                                new_node.operation = node.blend_type
                    # Special case: switch from math to some types
                    if node.type == 'MATH' and\
                            new_node.type in {'MIX_RGB', 'ALPHAOVER', 'ZCOMBINE'} and\
                            tp == 'MAIN':
                        for i in range(3):
                            new_node.inputs[dst_i].default_value[i] = src_dval
                        if new_node.type == 'MIX_RGB':
                            if node.operation in [t[0] for t in blend_types]:
                                new_node.blend_type = node.operation
                            # Set Fac of MIX_RGB to 1.0
                            new_node.inputs[0].default_value = 1.0
                    # make link only when dst matching input is not linked already.
                    if node.inputs[src_i].links and not new_node.inputs[dst_i].links:
                        in_src_link = node.inputs[src_i].links[0]
                        in_dst_socket = new_node.inputs[dst_i]
                        links.new(in_src_link.from_socket, in_dst_socket)
                        links.remove(in_src_link)
                # OUTPUTS: Base on matches in proper order.
                for (src_i, src_dval), (dst_i, dst_dval) in matches['OUTPUTS'][tp]:
                    for out_src_link in node.outputs[src_i].links:
                        out_dst_socket = new_node.outputs[dst_i]
                        links.new(out_dst_socket, out_src_link.to_socket)
            # relink rest inputs if possible, no criteria
            for src_inp in node.inputs:
                for dst_inp in new_node.inputs:
                    if src_inp.links and not dst_inp.links:
                        src_link = src_inp.links[0]
                        links.new(src_link.from_socket, dst_inp)
                        links.remove(src_link)
            # relink rest outputs if possible, base on node kind if any left.
            for src_o in node.outputs:
                for out_src_link in src_o.links:
                    for dst_o in new_node.outputs:
                        if src_o.type == dst_o.type:
                            links.new(dst_o, out_src_link.to_socket)
            # relink rest outputs no criteria if any left. Link all from first output.
            for src_o in node.outputs:
                for out_src_link in src_o.links:
                    if new_node.outputs:
                        links.new(new_node.outputs[0], out_src_link.to_socket)
            nodes.remove(node)
        force_update(context)
        return {'FINISHED'}


class NWMergeNodes(Operator, NWBase):
    bl_idname = "node.nw_merge_nodes"
    bl_label = "Merge Nodes"
    bl_description = "Merge Selected Nodes"
    bl_options = {'REGISTER', 'UNDO'}

    mode = EnumProperty(
        name="mode",
        description="All possible blend types and math operations",
        items=blend_types + [op for op in operations if op not in blend_types],
    )
    merge_type = EnumProperty(
        name="merge type",
        description="Type of Merge to be used",
        items=(
            ('AUTO', 'Auto', 'Automatic Output Type Detection'),
            ('SHADER', 'Shader', 'Merge using ADD or MIX Shader'),
            ('MIX', 'Mix Node', 'Merge using Mix Nodes'),
            ('MATH', 'Math Node', 'Merge using Math Nodes'),
            ('ZCOMBINE', 'Z-Combine Node', 'Merge using Z-Combine Nodes'),
            ('ALPHAOVER', 'Alpha Over Node', 'Merge using Alpha Over Nodes'),
        ),
    )

    def execute(self, context):
        settings = context.user_preferences.addons[__name__].preferences
        merge_hide = settings.merge_hide
        merge_position = settings.merge_position  # 'center' or 'bottom'

        do_hide = False
        do_hide_shader = False
        if merge_hide == 'ALWAYS':
            do_hide = True
            do_hide_shader = True
        elif merge_hide == 'NON_SHADER':
            do_hide = True

        tree_type = context.space_data.node_tree.type
        if tree_type == 'COMPOSITING':
            node_type = 'CompositorNode'
        elif tree_type == 'SHADER':
            node_type = 'ShaderNode'
        elif tree_type == 'TEXTURE':
            node_type = 'TextureNode'
        nodes, links = get_nodes_links(context)
        mode = self.mode
        merge_type = self.merge_type
        # Prevent trying to add Z-Combine in not 'COMPOSITING' node tree.
        # 'ZCOMBINE' works only if mode == 'MIX'
        # Setting mode to None prevents trying to add 'ZCOMBINE' node.
        if (merge_type == 'ZCOMBINE' or merge_type == 'ALPHAOVER') and tree_type != 'COMPOSITING':
            merge_type = 'MIX'
            mode = 'MIX'
        selected_mix = []  # entry = [index, loc]
        selected_shader = []  # entry = [index, loc]
        selected_math = []  # entry = [index, loc]
        selected_z = []  # entry = [index, loc]
        selected_alphaover = []  # entry = [index, loc]

        for i, node in enumerate(nodes):
            if node.select and node.outputs:
                if merge_type == 'AUTO':
                    for (type, types_list, dst) in (
                            ('SHADER', ('MIX', 'ADD'), selected_shader),
                            ('RGBA', [t[0] for t in blend_types], selected_mix),
                            ('VALUE', [t[0] for t in operations], selected_math),
                    ):
                        output_type = node.outputs[0].type
                        valid_mode = mode in types_list
                        # When mode is 'MIX' use mix node for both 'RGBA' and 'VALUE' output types.
                        # Cheat that output type is 'RGBA',
                        # and that 'MIX' exists in math operations list.
                        # This way when selected_mix list is analyzed:
                        # Node data will be appended even though it doesn't meet requirements.
                        if output_type != 'SHADER' and mode == 'MIX':
                            output_type = 'RGBA'
                            valid_mode = True
                        if output_type == type and valid_mode:
                            dst.append([i, node.location.x, node.location.y, node.dimensions.x, node.hide])
                else:
                    for (type, types_list, dst) in (
                            ('SHADER', ('MIX', 'ADD'), selected_shader),
                            ('MIX', [t[0] for t in blend_types], selected_mix),
                            ('MATH', [t[0] for t in operations], selected_math),
                            ('ZCOMBINE', ('MIX', ), selected_z),
                            ('ALPHAOVER', ('MIX', ), selected_alphaover),
                    ):
                        if merge_type == type and mode in types_list:
                            dst.append([i, node.location.x, node.location.y, node.dimensions.x, node.hide])
        # When nodes with output kinds 'RGBA' and 'VALUE' are selected at the same time
        # use only 'Mix' nodes for merging.
        # For that we add selected_math list to selected_mix list and clear selected_math.
        if selected_mix and selected_math and merge_type == 'AUTO':
            selected_mix += selected_math
            selected_math = []

        for nodes_list in [selected_mix, selected_shader, selected_math, selected_z, selected_alphaover]:
            if nodes_list:
                count_before = len(nodes)
                # sort list by loc_x - reversed
                nodes_list.sort(key=lambda k: k[1], reverse=True)
                # get maximum loc_x
                loc_x = nodes_list[0][1] + nodes_list[0][3] + 70
                nodes_list.sort(key=lambda k: k[2], reverse=True)
                if merge_position == 'CENTER':
                    loc_y = ((nodes_list[len(nodes_list) - 1][2]) + (nodes_list[len(nodes_list) - 2][2])) / 2  # average yloc of last two nodes (lowest two)
                    if nodes_list[len(nodes_list) - 1][-1] == True:  # if last node is hidden, mix should be shifted up a bit
                        if do_hide:
                            loc_y += 40
                        else:
                            loc_y += 80
                else:
                    loc_y = nodes_list[len(nodes_list) - 1][2]
                offset_y = 100
                if not do_hide:
                    offset_y = 200
                if nodes_list == selected_shader and not do_hide_shader:
                    offset_y = 150.0
                the_range = len(nodes_list) - 1
                if len(nodes_list) == 1:
                    the_range = 1
                for i in range(the_range):
                    if nodes_list == selected_mix:
                        add_type = node_type + 'MixRGB'
                        add = nodes.new(add_type)
                        add.blend_type = mode
                        if mode != 'MIX':
                            add.inputs[0].default_value = 1.0
                        add.show_preview = False
                        add.hide = do_hide
                        if do_hide:
                            loc_y = loc_y - 50
                        first = 1
                        second = 2
                        add.width_hidden = 100.0
                    elif nodes_list == selected_math:
                        add_type = node_type + 'Math'
                        add = nodes.new(add_type)
                        add.operation = mode
                        add.hide = do_hide
                        if do_hide:
                            loc_y = loc_y - 50
                        first = 0
                        second = 1
                        add.width_hidden = 100.0
                    elif nodes_list == selected_shader:
                        if mode == 'MIX':
                            add_type = node_type + 'MixShader'
                            add = nodes.new(add_type)
                            add.hide = do_hide_shader
                            if do_hide_shader:
                                loc_y = loc_y - 50
                            first = 1
                            second = 2
                            add.width_hidden = 100.0
                        elif mode == 'ADD':
                            add_type = node_type + 'AddShader'
                            add = nodes.new(add_type)
                            add.hide = do_hide_shader
                            if do_hide_shader:
                                loc_y = loc_y - 50
                            first = 0
                            second = 1
                            add.width_hidden = 100.0
                    elif nodes_list == selected_z:
                        add = nodes.new('CompositorNodeZcombine')
                        add.show_preview = False
                        add.hide = do_hide
                        if do_hide:
                            loc_y = loc_y - 50
                        first = 0
                        second = 2
                        add.width_hidden = 100.0
                    elif nodes_list == selected_alphaover:
                        add = nodes.new('CompositorNodeAlphaOver')
                        add.show_preview = False
                        add.hide = do_hide
                        if do_hide:
                            loc_y = loc_y - 50
                        first = 1
                        second = 2
                        add.width_hidden = 100.0
                    add.location = loc_x, loc_y
                    loc_y += offset_y
                    add.select = True
                count_adds = i + 1
                count_after = len(nodes)
                index = count_after - 1
                first_selected = nodes[nodes_list[0][0]]
                # "last" node has been added as first, so its index is count_before.
                last_add = nodes[count_before]
                # Special case:
                # Two nodes were selected and first selected has no output links, second selected has output links.
                # Then add links from last add to all links 'to_socket' of out links of second selected.
                if len(nodes_list) == 2:
                    if not first_selected.outputs[0].links:
                        second_selected = nodes[nodes_list[1][0]]
                        for ss_link in second_selected.outputs[0].links:
                            # Prevent cyclic dependencies when nodes to be marged are linked to one another.
                            # Create list of invalid indexes.
                            invalid_i = [n[0] for n in (selected_mix + selected_math + selected_shader + selected_z)]
                            # Link only if "to_node" index not in invalid indexes list.
                            if ss_link.to_node not in [nodes[i] for i in invalid_i]:
                                links.new(last_add.outputs[0], ss_link.to_socket)
                # add links from last_add to all links 'to_socket' of out links of first selected.
                for fs_link in first_selected.outputs[0].links:
                    # Prevent cyclic dependencies when nodes to be marged are linked to one another.
                    # Create list of invalid indexes.
                    invalid_i = [n[0] for n in (selected_mix + selected_math + selected_shader + selected_z)]
                    # Link only if "to_node" index not in invalid indexes list.
                    if fs_link.to_node not in [nodes[i] for i in invalid_i]:
                        links.new(last_add.outputs[0], fs_link.to_socket)
                # add link from "first" selected and "first" add node
                node_to = nodes[count_after - 1]
                links.new(first_selected.outputs[0], node_to.inputs[first])
                if node_to.type == 'ZCOMBINE':
                    for fs_out in first_selected.outputs:
                        if fs_out != first_selected.outputs[0] and fs_out.name in ('Z', 'Depth'):
                            links.new(fs_out, node_to.inputs[1])
                            break
                # add links between added ADD nodes and between selected and ADD nodes
                for i in range(count_adds):
                    if i < count_adds - 1:
                        node_from = nodes[index]
                        node_to = nodes[index - 1]
                        node_to_input_i = first
                        node_to_z_i = 1  # if z combine - link z to first z input
                        links.new(node_from.outputs[0], node_to.inputs[node_to_input_i])
                        if node_to.type == 'ZCOMBINE':
                            for from_out in node_from.outputs:
                                if from_out != node_from.outputs[0] and from_out.name in ('Z', 'Depth'):
                                    links.new(from_out, node_to.inputs[node_to_z_i])
                    if len(nodes_list) > 1:
                        node_from = nodes[nodes_list[i + 1][0]]
                        node_to = nodes[index]
                        node_to_input_i = second
                        node_to_z_i = 3  # if z combine - link z to second z input
                        links.new(node_from.outputs[0], node_to.inputs[node_to_input_i])
                        if node_to.type == 'ZCOMBINE':
                            for from_out in node_from.outputs:
                                if from_out != node_from.outputs[0] and from_out.name in ('Z', 'Depth'):
                                    links.new(from_out, node_to.inputs[node_to_z_i])
                    index -= 1
                # set "last" of added nodes as active
                nodes.active = last_add
                for i, x, y, dx, h in nodes_list:
                    nodes[i].select = False

        return {'FINISHED'}


class NWBatchChangeNodes(Operator, NWBase):
    bl_idname = "node.nw_batch_change"
    bl_label = "Batch Change"
    bl_description = "Batch Change Blend Type and Math Operation"
    bl_options = {'REGISTER', 'UNDO'}

    blend_type = EnumProperty(
        name="Blend Type",
        items=blend_types + navs,
    )
    operation = EnumProperty(
        name="Operation",
        items=operations + navs,
    )

    def execute(self, context):

        nodes, links = get_nodes_links(context)
        blend_type = self.blend_type
        operation = self.operation
        for node in context.selected_nodes:
            if node.type == 'MIX_RGB':
                if not blend_type in [nav[0] for nav in navs]:
                    node.blend_type = blend_type
                else:
                    if blend_type == 'NEXT':
                        index = [i for i, entry in enumerate(blend_types) if node.blend_type in entry][0]
                        #index = blend_types.index(node.blend_type)
                        if index == len(blend_types) - 1:
                            node.blend_type = blend_types[0][0]
                        else:
                            node.blend_type = blend_types[index + 1][0]

                    if blend_type == 'PREV':
                        index = [i for i, entry in enumerate(blend_types) if node.blend_type in entry][0]
                        if index == 0:
                            node.blend_type = blend_types[len(blend_types) - 1][0]
                        else:
                            node.blend_type = blend_types[index - 1][0]

            if node.type == 'MATH':
                if not operation in [nav[0] for nav in navs]:
                    node.operation = operation
                else:
                    if operation == 'NEXT':
                        index = [i for i, entry in enumerate(operations) if node.operation in entry][0]
                        #index = operations.index(node.operation)
                        if index == len(operations) - 1:
                            node.operation = operations[0][0]
                        else:
                            node.operation = operations[index + 1][0]

                    if operation == 'PREV':
                        index = [i for i, entry in enumerate(operations) if node.operation in entry][0]
                        #index = operations.index(node.operation)
                        if index == 0:
                            node.operation = operations[len(operations) - 1][0]
                        else:
                            node.operation = operations[index - 1][0]

        return {'FINISHED'}


class NWChangeMixFactor(Operator, NWBase):
    bl_idname = "node.nw_factor"
    bl_label = "Change Factor"
    bl_description = "Change Factors of Mix Nodes and Mix Shader Nodes"
    bl_options = {'REGISTER', 'UNDO'}

    # option: Change factor.
    # If option is 1.0 or 0.0 - set to 1.0 or 0.0
    # Else - change factor by option value.
    option = FloatProperty()

    def execute(self, context):
        nodes, links = get_nodes_links(context)
        option = self.option
        selected = []  # entry = index
        for si, node in enumerate(nodes):
            if node.select:
                if node.type in {'MIX_RGB', 'MIX_SHADER'}:
                    selected.append(si)

        for si in selected:
            fac = nodes[si].inputs[0]
            nodes[si].hide = False
            if option in {0.0, 1.0}:
                fac.default_value = option
            else:
                fac.default_value += option

        return {'FINISHED'}


class NWCopySettings(Operator, NWBase):
    bl_idname = "node.nw_copy_settings"
    bl_label = "Copy Settings"
    bl_description = "Copy Settings of Active Node to Selected Nodes"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        valid = False
        if nw_check(context):
            if context.active_node is not None and context.active_node.type is not 'FRAME':
                valid = True
        return valid

    def execute(self, context):
        node_active = context.active_node
        node_selected = context.selected_nodes

        # Error handling
        if not (len(node_selected) > 1):
            self.report({'ERROR'}, "2 nodes must be selected at least")
            return {'CANCELLED'}

        # Check if active node is in the selection
        selected_node_names = [n.name for n in node_selected]
        if node_active.name not in selected_node_names:
            self.report({'ERROR'}, "No active node")
            return {'CANCELLED'}

        # Get nodes in selection by type
        valid_nodes = [n for n in node_selected if n.type == node_active.type]

        if not (len(valid_nodes) > 1) and node_active:
            self.report({'ERROR'}, "Selected nodes are not of the same type as {}".format(node_active.name))
            return {'CANCELLED'}

        if len(valid_nodes) != len(node_selected):
            # Report nodes that are not valid
            valid_node_names = [n.name for n in valid_nodes]
            not_valid_names = list(set(selected_node_names) - set(valid_node_names))
            self.report({'INFO'}, "Ignored {} (not of the same type as {})".format(", ".join(not_valid_names), node_active.name))

        # Reference original
        orig = node_active
        #node_selected_names = [n.name for n in node_selected]

        # Output list
        success_names = []

        # Deselect all nodes
        for i in node_selected:
            i.select = False

        # Code by zeffii from http://blender.stackexchange.com/a/42338/3710
        # Run through all other nodes
        for node in valid_nodes[1:]:

            # Check for frame node
            parent = node.parent if node.parent else None
            node_loc = [node.location.x, node.location.y]

            # Select original to duplicate
            orig.select = True

            # Duplicate selected node
            bpy.ops.node.duplicate()
            new_node = context.selected_nodes[0]

            # Deselect copy
            new_node.select = False

            # Properties to copy
            node_tree = node.id_data
            props_to_copy = 'bl_idname name location height width'.split(' ')

            # Input and outputs
            reconnections = []
            mappings = chain.from_iterable([node.inputs, node.outputs])
            for i in (i for i in mappings if i.is_linked):
                for L in i.links:
                    reconnections.append([L.from_socket.path_from_id(), L.to_socket.path_from_id()])

            # Properties
            props = {j: getattr(node, j) for j in props_to_copy}
            props_to_copy.pop(0)

            for prop in props_to_copy:
                setattr(new_node, prop, props[prop])

            # Get the node tree to remove the old node
            nodes = node_tree.nodes
            nodes.remove(node)
            new_node.name = props['name']

            if parent:
                new_node.parent = parent
                new_node.location = node_loc

            for str_from, str_to in reconnections:
                node_tree.links.new(eval(str_from), eval(str_to))

            success_names.append(new_node.name)

        orig.select = True
        node_tree.nodes.active = orig
        self.report({'INFO'}, "Successfully copied attributes from {} to: {}".format(orig.name, ", ".join(success_names)))
        return {'FINISHED'}


class NWCopyLabel(Operator, NWBase):
    bl_idname = "node.nw_copy_label"
    bl_label = "Copy Label"
    bl_options = {'REGISTER', 'UNDO'}

    option = EnumProperty(
        name="option",
        description="Source of name of label",
        items=(
            ('FROM_ACTIVE', 'from active', 'from active node',),
            ('FROM_NODE', 'from node', 'from node linked to selected node'),
            ('FROM_SOCKET', 'from socket', 'from socket linked to selected node'),
        )
    )

    def execute(self, context):
        nodes, links = get_nodes_links(context)
        option = self.option
        active = nodes.active
        if option == 'FROM_ACTIVE':
            if active:
                src_label = active.label
                for node in [n for n in nodes if n.select and nodes.active != n]:
                    node.label = src_label
        elif option == 'FROM_NODE':
            selected = [n for n in nodes if n.select]
            for node in selected:
                for input in node.inputs:
                    if input.links:
                        src = input.links[0].from_node
                        node.label = src.label
                        break
        elif option == 'FROM_SOCKET':
            selected = [n for n in nodes if n.select]
            for node in selected:
                for input in node.inputs:
                    if input.links:
                        src = input.links[0].from_socket
                        node.label = src.name
                        break

        return {'FINISHED'}


class NWClearLabel(Operator, NWBase):
    bl_idname = "node.nw_clear_label"
    bl_label = "Clear Label"
    bl_options = {'REGISTER', 'UNDO'}

    option = BoolProperty()

    def execute(self, context):
        nodes, links = get_nodes_links(context)
        for node in [n for n in nodes if n.select]:
            node.label = ''

        return {'FINISHED'}

    def invoke(self, context, event):
        if self.option:
            return self.execute(context)
        else:
            return context.window_manager.invoke_confirm(self, event)


class NWModifyLabels(Operator, NWBase):
    """Modify Labels of all selected nodes"""
    bl_idname = "node.nw_modify_labels"
    bl_label = "Modify Labels"
    bl_options = {'REGISTER', 'UNDO'}

    prepend = StringProperty(
        name="Add to Beginning"
    )
    append = StringProperty(
        name="Add to End"
    )
    replace_from = StringProperty(
        name="Text to Replace"
    )
    replace_to = StringProperty(
        name="Replace with"
    )

    def execute(self, context):
        nodes, links = get_nodes_links(context)
        for node in [n for n in nodes if n.select]:
            node.label = self.prepend + node.label.replace(self.replace_from, self.replace_to) + self.append

        return {'FINISHED'}

    def invoke(self, context, event):
        self.prepend = ""
        self.append = ""
        self.remove = ""
        return context.window_manager.invoke_props_dialog(self)


class NWAddTextureSetup(Operator, NWBase):
    bl_idname = "node.nw_add_texture"
    bl_label = "Texture Setup"
    bl_description = "Add Texture Node Setup to Selected Shaders"
    bl_options = {'REGISTER', 'UNDO'}

    add_mapping = BoolProperty(name="Add Mapping Nodes", description="Create coordinate and mapping nodes for the texture (ignored for selected texture nodes)", default=True)

    @classmethod
    def poll(cls, context):
        valid = False
        if nw_check(context):
            space = context.space_data
            if space.tree_type == 'ShaderNodeTree' and context.scene.render.engine == 'CYCLES':
                valid = True
        return valid

    def execute(self, context):
        nodes, links = get_nodes_links(context)
        shader_types = [x[1] for x in shaders_shader_nodes_props if x[1] not in {'MIX_SHADER', 'ADD_SHADER'}]
        texture_types = [x[1] for x in shaders_texture_nodes_props]
        selected_nodes = [n for n in nodes if n.select]
        for t_node in selected_nodes:
            valid = False
            input_index = 0
            if t_node.inputs:
                for index, i in enumerate(t_node.inputs):
                    if not i.is_linked:
                        valid = True
                        input_index = index
                        break
            if valid:
                locx = t_node.location.x
                locy = t_node.location.y - t_node.dimensions.y/2

                xoffset = [500, 700]
                is_texture = False
                if t_node.type in texture_types + ['MAPPING']:
                    xoffset = [290, 500]
                    is_texture = True

                coordout = 2
                image_type = 'ShaderNodeTexImage'

                if (t_node.type in texture_types and t_node.type != 'TEX_IMAGE') or (t_node.type == 'BACKGROUND'):
                    coordout = 0  # image texture uses UVs, procedural textures and Background shader use Generated
                    if t_node.type == 'BACKGROUND':
                        image_type = 'ShaderNodeTexEnvironment'

                if not is_texture:
                    tex = nodes.new(image_type)
                    tex.location = [locx - 200, locy + 112]
                    nodes.active = tex
                    links.new(tex.outputs[0], t_node.inputs[input_index])

                t_node.select = False
                if self.add_mapping or is_texture:
                    if t_node.type != 'MAPPING':
                        m = nodes.new('ShaderNodeMapping')
                        m.location = [locx - xoffset[0], locy + 141]
                        m.width = 240
                    else:
                        m = t_node
                    coord = nodes.new('ShaderNodeTexCoord')
                    coord.location = [locx - (200 if t_node.type == 'MAPPING' else xoffset[1]), locy + 124]

                    if not is_texture:
                        links.new(m.outputs[0], tex.inputs[0])
                        links.new(coord.outputs[coordout], m.inputs[0])
                    else:
                        nodes.active = m
                        links.new(m.outputs[0], t_node.inputs[input_index])
                        links.new(coord.outputs[coordout], m.inputs[0])
            else:
                self.report({'WARNING'}, "No free inputs for node: "+t_node.name)
        return {'FINISHED'}


class NWAddPrincipledSetup(Operator, NWBase, ImportHelper):
    bl_idname = "node.nw_add_textures_for_principled"
    bl_label = "Principled Texture Setup"
    bl_description = "Add Texture Node Setup for Principled BSDF"
    bl_options = {'REGISTER', 'UNDO'}

    directory = StringProperty(
                    name='Directory',
                    subtype='DIR_PATH',
                    default='',
                    description='Folder to search in for image files')
    files = CollectionProperty(
                    type=bpy.types.OperatorFileListElement,
                    options={'HIDDEN', 'SKIP_SAVE'})

    order = [
        "filepath",
        "files",
        ]

    @classmethod
    def poll(cls, context):
        valid = False
        if nw_check(context):
            space = context.space_data
            if space.tree_type == 'ShaderNodeTree' and context.scene.render.engine == 'CYCLES':
                valid = True
        return valid

    def execute(self, context):
        # Check if everything is ok
        if not self.directory:
            self.report({'INFO'}, 'No Folder Selected')
            return {'CANCELLED'}
        if not self.files[:]:
            self.report({'INFO'}, 'No Files Selected')
            return {'CANCELLED'}

        nodes, links = get_nodes_links(context)
        active_node = nodes.active
        if not active_node.bl_idname == 'ShaderNodeBsdfPrincipled':
            self.report({'INFO'}, 'Select Principled BSDF')
            return {'CANCELLED'}

        # Helper_functions
        def split_into__components(fname):
            # Split filename into components
            # 'WallTexture_diff_2k.002.jpg' -> ['Wall', 'Texture', 'diff', 'k']
            # Remove extension
            fname = path.splitext(fname)[0]
            # Remove digits
            fname = ''.join(i for i in fname if not i.isdigit())
            # Seperate CamelCase by space
            fname = re.sub("([a-z])([A-Z])","\g<1> \g<2>",fname)
            # Replace common separators with SPACE
            seperators = ['_', '.', '-', '__', '--', '#']
            for sep in seperators:
                fname = fname.replace(sep, ' ')

            components = fname.split(' ')
            components = [c.lower() for c in components]
            return components

        # Filter textures names for texturetypes in filenames
        # [Socket Name, [abbreviations and keyword list], Filename placeholder]
        tags = context.user_preferences.addons[__name__].preferences.principled_tags
        normal_abbr = tags.normal.split(' ')
        bump_abbr = tags.bump.split(' ')
        gloss_abbr = tags.gloss.split(' ')
        rough_abbr = tags.rough.split(' ')
        socketnames = [
        ['Displacement', tags.displacement.split(' '), None],
        ['Base Color', tags.base_color.split(' '), None],
        ['Subsurface Color', tags.sss_color.split(' '), None],
        ['Metallic', tags.metallic.split(' '), None],
        ['Specular', tags.specular.split(' '), None],
        ['Roughness', rough_abbr + gloss_abbr, None],
        ['Normal', normal_abbr + bump_abbr, None],
        ]

        # Look through texture_types and set value as filename of first matched file
        def match_files_to_socket_names():
            for sname in socketnames:
                for file in self.files:
                    fname = file.name
                    filenamecomponents = split_into__components(fname)
                    matches = set(sname[1]).intersection(set(filenamecomponents))
                    # TODO: ignore basename (if texture is named "fancy_metal_nor", it will be detected as metallic map, not normal map)
                    if matches:
                        sname[2] = fname
                        break

        match_files_to_socket_names()
        # Remove socketnames without found files
        socketnames = [s for s in socketnames if s[2]
                       and path.exists(self.directory+s[2])]
        if not socketnames:
            self.report({'INFO'}, 'No matching images found')
            print('No matching images found')
            return {'CANCELLED'}

        # Add found images
        print('\nMatched Textures:')
        texture_nodes = []
        disp_texture = None
        normal_node = None
        roughness_node = None
        for i, sname in enumerate(socketnames):
            print(i, sname[0], sname[2])

            # DISPLACEMENT NODES
            if sname[0] == 'Displacement':
                disp_texture = nodes.new(type='ShaderNodeTexImage')
                img = bpy.data.images.load(self.directory+sname[2])
                disp_texture.image = img
                disp_texture.label = 'Displacement'
                disp_texture.color_space = 'NONE'

                # Add displacement offset nodes
                math_sub = nodes.new(type='ShaderNodeMath')
                math_sub.operation = 'SUBTRACT'
                math_sub.label = 'Offset'
                math_sub.location = active_node.location + Vector((0, -560))
                math_mul = nodes.new(type='ShaderNodeMath')
                math_mul.operation = 'MULTIPLY'
                math_mul.label = 'Strength'
                math_mul.location = math_sub.location + Vector((200, 0))
                link = links.new(math_mul.inputs[0], math_sub.outputs[0])
                link = links.new(math_sub.inputs[0], disp_texture.outputs[0])

                # Turn on true displacement in the material
                # Too complicated for now

                '''
                # Frame. Does not update immediatly
                # Seems to need an editor redraw
                frame = nodes.new(type='NodeFrame')
                frame.label = 'Displacement'
                math_sub.parent = frame
                math_mul.parent = frame
                frame.update()
                '''

                #find ouput node
                output_node = [n for n in nodes if n.bl_idname == 'ShaderNodeOutputMaterial']
                if output_node:
                    if not output_node[0].inputs[2].is_linked:
                        link = links.new(output_node[0].inputs[2], math_mul.outputs[0])

                continue

            if not active_node.inputs[sname[0]].is_linked:
                # No texture node connected -> add texture node with new image
                texture_node = nodes.new(type='ShaderNodeTexImage')
                img = bpy.data.images.load(self.directory+sname[2])
                texture_node.image = img

                # NORMAL NODES
                if sname[0] == 'Normal':
                    # Test if new texture node is normal or bump map
                    fname_components = split_into__components(sname[2])
                    match_normal = set(normal_abbr).intersection(set(fname_components))
                    match_bump = set(bump_abbr).intersection(set(fname_components))
                    if match_normal:
                        # If Normal add normal node in between
                        normal_node = nodes.new(type='ShaderNodeNormalMap')
                        link = links.new(normal_node.inputs[1], texture_node.outputs[0])
                    elif match_bump:
                        # If Bump add bump node in between
                        normal_node = nodes.new(type='ShaderNodeBump')
                        link = links.new(normal_node.inputs[2], texture_node.outputs[0])

                    link = links.new(active_node.inputs[sname[0]], normal_node.outputs[0])
                    normal_node_texture = texture_node

                elif sname[0] == 'Roughness':
                    # Test if glossy or roughness map
                    fname_components = split_into__components(sname[2])
                    match_rough = set(rough_abbr).intersection(set(fname_components))
                    match_gloss = set(gloss_abbr).intersection(set(fname_components))

                    if match_rough:
                        # If Roughness nothing to to
                        link = links.new(active_node.inputs[sname[0]], texture_node.outputs[0])

                    elif match_gloss:
                        # If Gloss Map add invert node
                        invert_node = nodes.new(type='ShaderNodeInvert')
                        link = links.new(invert_node.inputs[1], texture_node.outputs[0])

                        link = links.new(active_node.inputs[sname[0]], invert_node.outputs[0])
                        roughness_node = texture_node

                else:
                    # This is a simple connection Texture --> Input slot
                    link = links.new(active_node.inputs[sname[0]], texture_node.outputs[0])

                # Use non-color for all but 'Base Color' Textures
                if not sname[0] in ['Base Color']:
                    texture_node.color_space = 'NONE'

            else:
                # If already texture connected. add to node list for alignment
                texture_node = active_node.inputs[sname[0]].links[0].from_node

            # This are all connected texture nodes
            texture_nodes.append(texture_node)
            texture_node.label = sname[0]

        if disp_texture:
            texture_nodes.append(disp_texture)

        # Alignment
        for i, texture_node in enumerate(texture_nodes):
            offset = Vector((-400, (i * -260) + 200))
            texture_node.location = active_node.location + offset

        if normal_node:
            # Extra alignment if normal node was added
            normal_node.location = normal_node_texture.location + Vector((200, 0))

        if roughness_node:
            # Alignment of invert node if glossy map
            invert_node.location = roughness_node.location + Vector((200, 0))

        # Add texture input + mapping
        mapping = nodes.new(type='ShaderNodeMapping')
        mapping.location = active_node.location + Vector((-900, 0))
        if len(texture_nodes) > 1:
            # If more than one texture add reroute node in between
            reroute = nodes.new(type='NodeReroute')
            tex_coords = Vector((texture_nodes[0].location.x, sum(n.location.y for n in texture_nodes)/len(texture_nodes)))
            reroute.location = tex_coords + Vector((-50, -120))
            for texture_node in texture_nodes:
                link = links.new(texture_node.inputs[0], reroute.outputs[0])
            link = links.new(reroute.inputs[0], mapping.outputs[0])
        else:
            link = links.new(texture_nodes[0].inputs[0], mapping.outputs[0])

        # Connect texture_coordiantes to mapping node
        texture_input = nodes.new(type='ShaderNodeTexCoord')
        texture_input.location = mapping.location + Vector((-200, 0))
        link = links.new(mapping.inputs[0], texture_input.outputs[2])

        # Just to be sure
        active_node.select = False
        nodes.update()
        links.update()
        force_update(context)
        return {'FINISHED'}


class NWAddReroutes(Operator, NWBase):
    """Add Reroute Nodes and link them to outputs of selected nodes"""
    bl_idname = "node.nw_add_reroutes"
    bl_label = "Add Reroutes"
    bl_description = "Add Reroutes to Outputs"
    bl_options = {'REGISTER', 'UNDO'}

    option = EnumProperty(
        name="option",
        items=[
            ('ALL', 'to all', 'Add to all outputs'),
            ('LOOSE', 'to loose', 'Add only to loose outputs'),
            ('LINKED', 'to linked', 'Add only to linked outputs'),
        ]
    )

    def execute(self, context):
        tree_type = context.space_data.node_tree.type
        option = self.option
        nodes, links = get_nodes_links(context)
        # output valid when option is 'all' or when 'loose' output has no links
        valid = False
        post_select = []  # nodes to be selected after execution
        # create reroutes and recreate links
        for node in [n for n in nodes if n.select]:
            if node.outputs:
                x = node.location.x
                y = node.location.y
                width = node.width
                # unhide 'REROUTE' nodes to avoid issues with location.y
                if node.type == 'REROUTE':
                    node.hide = False
                # When node is hidden - width_hidden not usable.
                # Hack needed to calculate real width
                if node.hide:
                    bpy.ops.node.select_all(action='DESELECT')
                    helper = nodes.new('NodeReroute')
                    helper.select = True
                    node.select = True
                    # resize node and helper to zero. Then check locations to calculate width
                    bpy.ops.transform.resize(value=(0.0, 0.0, 0.0))
                    width = 2.0 * (helper.location.x - node.location.x)
                    # restore node location
                    node.location = x, y
                    # delete helper
                    node.select = False
                    # only helper is selected now
                    bpy.ops.node.delete()
                x = node.location.x + width + 20.0
                if node.type != 'REROUTE':
                    y -= 35.0
                y_offset = -22.0
                loc = x, y
            reroutes_count = 0  # will be used when aligning reroutes added to hidden nodes
            for out_i, output in enumerate(node.outputs):
                pass_used = False  # initial value to be analyzed if 'R_LAYERS'
                # if node is not 'R_LAYERS' - "pass_used" not needed, so set it to True
                if node.type != 'R_LAYERS':
                    pass_used = True
                else:  # if 'R_LAYERS' check if output represent used render pass
                    node_scene = node.scene
                    node_layer = node.layer
                    # If output - "Alpha" is analyzed - assume it's used. Not represented in passes.
                    if output.name == 'Alpha':
                        pass_used = True
                    else:
                        # check entries in global 'rl_outputs' variable
                        #for render_pass, output_name, exr_name, in_internal, in_cycles in rl_outputs:
                        for rlo in rl_outputs:
                            if output.name == rlo.output_name or output.name == rlo.exr_output_name:
                                pass_used = getattr(node_scene.render.layers[node_layer], rlo.render_pass)
                                break
                if pass_used:
                    valid = ((option == 'ALL') or
                             (option == 'LOOSE' and not output.links) or
                             (option == 'LINKED' and output.links))
                    # Add reroutes only if valid, but offset location in all cases.
                    if valid:
                        n = nodes.new('NodeReroute')
                        nodes.active = n
                        for link in output.links:
                            links.new(n.outputs[0], link.to_socket)
                        links.new(output, n.inputs[0])
                        n.location = loc
                        post_select.append(n)
                    reroutes_count += 1
                    y += y_offset
                    loc = x, y
            # disselect the node so that after execution of script only newly created nodes are selected
            node.select = False
            # nicer reroutes distribution along y when node.hide
            if node.hide:
                y_translate = reroutes_count * y_offset / 2.0 - y_offset - 35.0
                for reroute in [r for r in nodes if r.select]:
                    reroute.location.y -= y_translate
            for node in post_select:
                node.select = True

        return {'FINISHED'}


class NWLinkActiveToSelected(Operator, NWBase):
    """Link active node to selected nodes basing on various criteria"""
    bl_idname = "node.nw_link_active_to_selected"
    bl_label = "Link Active Node to Selected"
    bl_options = {'REGISTER', 'UNDO'}

    replace = BoolProperty()
    use_node_name = BoolProperty()
    use_outputs_names = BoolProperty()

    @classmethod
    def poll(cls, context):
        valid = False
        if nw_check(context):
            if context.active_node is not None:
                if context.active_node.select:
                    valid = True
        return valid

    def execute(self, context):
        nodes, links = get_nodes_links(context)
        replace = self.replace
        use_node_name = self.use_node_name
        use_outputs_names = self.use_outputs_names
        active = nodes.active
        selected = [node for node in nodes if node.select and node != active]
        outputs = []  # Only usable outputs of active nodes will be stored here.
        for out in active.outputs:
            if active.type != 'R_LAYERS':
                outputs.append(out)
            else:
                # 'R_LAYERS' node type needs special handling.
                # outputs of 'R_LAYERS' are callable even if not seen in UI.
                # Only outputs that represent used passes should be taken into account
                # Check if pass represented by output is used.
                # global 'rl_outputs' list will be used for that
                for render_pass, out_name, exr_name, in_internal, in_cycles in rl_outputs:
                    pass_used = False  # initial value. Will be set to True if pass is used
                    if out.name == 'Alpha':
                        # Alpha output is always present. Doesn't have representation in render pass. Assume it's used.
                        pass_used = True
                    elif out.name == out_name:
                        # example 'render_pass' entry: 'use_pass_uv' Check if True in scene render layers
                        pass_used = getattr(active.scene.render.layers[active.layer], render_pass)
                        break
                if pass_used:
                    outputs.append(out)
        doit = True  # Will be changed to False when links successfully added to previous output.
        for out in outputs:
            if doit:
                for node in selected:
                    dst_name = node.name  # Will be compared with src_name if needed.
                    # When node has label - use it as dst_name
                    if node.label:
                        dst_name = node.label
                    valid = True  # Initial value. Will be changed to False if names don't match.
                    src_name = dst_name  # If names not used - this asignment will keep valid = True.
                    if use_node_name:
                        # Set src_name to source node name or label
                        src_name = active.name
                        if active.label:
                            src_name = active.label
                    elif use_outputs_names:
                        src_name = (out.name, )
                        for render_pass, out_name, exr_name, in_internal, in_cycles in rl_outputs:
                            if out.name in {out_name, exr_name}:
                                src_name = (out_name, exr_name)
                    if dst_name not in src_name:
                        valid = False
                    if valid:
                        for input in node.inputs:
                            if input.type == out.type or node.type == 'REROUTE':
                                if replace or not input.is_linked:
                                    links.new(out, input)
                                    if not use_node_name and not use_outputs_names:
                                        doit = False
                                    break

        return {'FINISHED'}


class NWAlignNodes(Operator, NWBase):
    '''Align the selected nodes neatly in a row/column'''
    bl_idname = "node.nw_align_nodes"
    bl_label = "Align Nodes"
    bl_options = {'REGISTER', 'UNDO'}
    margin = IntProperty(name='Margin', default=50, description='The amount of space between nodes')

    def execute(self, context):
        nodes, links = get_nodes_links(context)
        margin = self.margin

        selection = []
        for node in nodes:
            if node.select and node.type != 'FRAME':
                selection.append(node)

        # If no nodes are selected, align all nodes
        active_loc = None
        if not selection:
            selection = nodes
        elif nodes.active in selection:
            active_loc = copy(nodes.active.location)  # make a copy, not a reference

        # Check if nodes should be layed out horizontally or vertically
        x_locs = [n.location.x + (n.dimensions.x / 2) for n in selection]  # use dimension to get center of node, not corner
        y_locs = [n.location.y - (n.dimensions.y / 2) for n in selection]
        x_range = max(x_locs) - min(x_locs)
        y_range = max(y_locs) - min(y_locs)
        mid_x = (max(x_locs) + min(x_locs)) / 2
        mid_y = (max(y_locs) + min(y_locs)) / 2
        horizontal = x_range > y_range

        # Sort selection by location of node mid-point
        if horizontal:
            selection = sorted(selection, key=lambda n: n.location.x + (n.dimensions.x / 2))
        else:
            selection = sorted(selection, key=lambda n: n.location.y - (n.dimensions.y / 2), reverse=True)

        # Alignment
        current_pos = 0
        for node in selection:
            current_margin = margin
            current_margin = current_margin * 0.5 if node.hide else current_margin  # use a smaller margin for hidden nodes

            if horizontal:
                node.location.x = current_pos
                current_pos += current_margin + node.dimensions.x
                node.location.y = mid_y + (node.dimensions.y / 2)
            else:
                node.location.y = current_pos
                current_pos -= (current_margin * 0.3) + node.dimensions.y  # use half-margin for vertical alignment
                node.location.x = mid_x - (node.dimensions.x / 2)

        # If active node is selected, center nodes around it
        if active_loc is not None:
            active_loc_diff = active_loc - nodes.active.location
            for node in selection:
                node.location += active_loc_diff
        else:  # Position nodes centered around where they used to be
            locs = ([n.location.x + (n.dimensions.x / 2) for n in selection]) if horizontal else ([n.location.y - (n.dimensions.y / 2) for n in selection])
            new_mid = (max(locs) + min(locs)) / 2
            for node in selection:
                if horizontal:
                    node.location.x += (mid_x - new_mid)
                else:
                    node.location.y += (mid_y - new_mid)

        return {'FINISHED'}


class NWSelectParentChildren(Operator, NWBase):
    bl_idname = "node.nw_select_parent_child"
    bl_label = "Select Parent or Children"
    bl_options = {'REGISTER', 'UNDO'}

    option = EnumProperty(
        name="option",
        items=(
            ('PARENT', 'Select Parent', 'Select Parent Frame'),
            ('CHILD', 'Select Children', 'Select members of selected frame'),
        )
    )

    def execute(self, context):
        nodes, links = get_nodes_links(context)
        option = self.option
        selected = [node for node in nodes if node.select]
        if option == 'PARENT':
            for sel in selected:
                parent = sel.parent
                if parent:
                    parent.select = True
        else:  # option == 'CHILD'
            for sel in selected:
                children = [node for node in nodes if node.parent == sel]
                for kid in children:
                    kid.select = True

        return {'FINISHED'}


class NWDetachOutputs(Operator, NWBase):
    """Detach outputs of selected node leaving inluts liked"""
    bl_idname = "node.nw_detach_outputs"
    bl_label = "Detach Outputs"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        nodes, links = get_nodes_links(context)
        selected = context.selected_nodes
        bpy.ops.node.duplicate_move_keep_inputs()
        new_nodes = context.selected_nodes
        bpy.ops.node.select_all(action="DESELECT")
        for node in selected:
            node.select = True
        bpy.ops.node.delete_reconnect()
        for new_node in new_nodes:
            new_node.select = True
        bpy.ops.transform.translate('INVOKE_DEFAULT')

        return {'FINISHED'}


class NWLinkToOutputNode(Operator, NWBase):
    """Link to Composite node or Material Output node"""
    bl_idname = "node.nw_link_out"
    bl_label = "Connect to Output"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        valid = False
        if nw_check(context):
            if context.active_node is not None:
                for out in context.active_node.outputs:
                    if not out.hide:
                        valid = True
                        break
        return valid

    def execute(self, context):
        nodes, links = get_nodes_links(context)
        active = nodes.active
        output_node = None
        output_index = None
        tree_type = context.space_data.tree_type
        output_types_shaders = [x[1] for x in shaders_output_nodes_props]
        output_types_compo = ['COMPOSITE']
        output_types_blender_mat = ['OUTPUT']
        output_types_textures = ['OUTPUT']
        output_types = output_types_shaders + output_types_compo + output_types_blender_mat
        for node in nodes:
            if node.type in output_types:
                output_node = node
                break
        if not output_node:
            bpy.ops.node.select_all(action="DESELECT")
            if tree_type == 'ShaderNodeTree':
                if context.scene.render.engine == 'CYCLES':
                    output_node = nodes.new('ShaderNodeOutputMaterial')
                else:
                    output_node = nodes.new('ShaderNodeOutput')
            elif tree_type == 'CompositorNodeTree':
                output_node = nodes.new('CompositorNodeComposite')
            elif tree_type == 'TextureNodeTree':
                output_node = nodes.new('TextureNodeOutput')
            output_node.location.x = active.location.x + active.dimensions.x + 80
            output_node.location.y = active.location.y
        if (output_node and active.outputs):
            for i, output in enumerate(active.outputs):
                if not output.hide:
                    output_index = i
                    break
            for i, output in enumerate(active.outputs):
                if output.type == output_node.inputs[0].type and not output.hide:
                    output_index = i
                    break

            out_input_index = 0
            if tree_type == 'ShaderNodeTree' and context.scene.render.engine == 'CYCLES':
                if active.outputs[output_index].name == 'Volume':
                    out_input_index = 1
                elif active.outputs[output_index].type != 'SHADER':  # connect to displacement if not a shader
                    out_input_index = 2
            links.new(active.outputs[output_index], output_node.inputs[out_input_index])

        force_update(context)  # viewport render does not update

        return {'FINISHED'}


class NWMakeLink(Operator, NWBase):
    """Make a link from one socket to another"""
    bl_idname = 'node.nw_make_link'
    bl_label = 'Make Link'
    bl_options = {'REGISTER', 'UNDO'}
    from_socket = IntProperty()
    to_socket = IntProperty()

    def execute(self, context):
        nodes, links = get_nodes_links(context)

        n1 = nodes[context.scene.NWLazySource]
        n2 = nodes[context.scene.NWLazyTarget]

        links.new(n1.outputs[self.from_socket], n2.inputs[self.to_socket])

        force_update(context)

        return {'FINISHED'}


class NWCallInputsMenu(Operator, NWBase):
    """Link from this output"""
    bl_idname = 'node.nw_call_inputs_menu'
    bl_label = 'Make Link'
    bl_options = {'REGISTER', 'UNDO'}
    from_socket = IntProperty()

    def execute(self, context):
        nodes, links = get_nodes_links(context)

        context.scene.NWSourceSocket = self.from_socket

        n1 = nodes[context.scene.NWLazySource]
        n2 = nodes[context.scene.NWLazyTarget]
        if len(n2.inputs) > 1:
            bpy.ops.wm.call_menu("INVOKE_DEFAULT", name=NWConnectionListInputs.bl_idname)
        elif len(n2.inputs) == 1:
            links.new(n1.outputs[self.from_socket], n2.inputs[0])
        return {'FINISHED'}


class NWAddSequence(Operator, ImportHelper):
    """Add an Image Sequence"""
    bl_idname = 'node.nw_add_sequence'
    bl_label = 'Import Image Sequence'
    bl_options = {'REGISTER', 'UNDO'}
    directory = StringProperty(subtype="DIR_PATH")
    filename = StringProperty(subtype="FILE_NAME")
    files = CollectionProperty(type=bpy.types.OperatorFileListElement, options={'HIDDEN', 'SKIP_SAVE'})

    def execute(self, context):
        nodes, links = get_nodes_links(context)
        directory = self.directory
        filename = self.filename
        files = self.files
        tree = context.space_data.node_tree

        # DEBUG
        # print ("\nDIR:", directory)
        # print ("FN:", filename)
        # print ("Fs:", list(f.name for f in files), '\n')

        if tree.type == 'SHADER':
            node_type = "ShaderNodeTexImage"
        elif tree.type == 'COMPOSITING':
            node_type = "CompositorNodeImage"
        else:
            self.report({'ERROR'}, "Unsupported Node Tree type!")
            return {'CANCELLED'}

        if not files[0].name and not filename:
            self.report({'ERROR'}, "No file chosen")
            return {'CANCELLED'}
        elif files[0].name and (not filename or not path.exists(directory+filename)):
            # User has selected multiple files without an active one, or the active one is non-existant
            filename = files[0].name

        if not path.exists(directory+filename):
            self.report({'ERROR'}, filename+" does not exist!")
            return {'CANCELLED'}

        without_ext = '.'.join(filename.split('.')[:-1])

        # if last digit isn't a number, it's not a sequence
        if not without_ext[-1].isdigit():
            self.report({'ERROR'}, filename+" does not seem to be part of a sequence")
            return {'CANCELLED'}


        extension = filename.split('.')[-1]
        reverse = without_ext[::-1] # reverse string

        count_numbers = 0
        for char in reverse:
            if char.isdigit():
                count_numbers += 1
            else:
                break

        without_num = without_ext[:count_numbers*-1]

        files = sorted(glob(directory + without_num + "[0-9]"*count_numbers + "." + extension))

        num_frames = len(files)

        nodes_list = [node for node in nodes]
        if nodes_list:
            nodes_list.sort(key=lambda k: k.location.x)
            xloc = nodes_list[0].location.x - 220  # place new nodes at far left
            yloc = 0
            for node in nodes:
                node.select = False
                yloc += node_mid_pt(node, 'y')
            yloc = yloc/len(nodes)
        else:
            xloc = 0
            yloc = 0

        name_with_hashes = without_num + "#"*count_numbers + '.' + extension

        bpy.ops.node.add_node('INVOKE_DEFAULT', use_transform=True, type=node_type)
        node = nodes.active
        node.label = name_with_hashes

        img = bpy.data.images.load(directory+(without_ext+'.'+extension))
        img.source = 'SEQUENCE'
        img.name = name_with_hashes
        node.image = img
        image_user = node.image_user if tree.type == 'SHADER' else node
        image_user.frame_offset = int(files[0][len(without_num)+len(directory):-1*(len(extension)+1)]) - 1  # separate the number from the file name of the first  file
        image_user.frame_duration = num_frames

        return {'FINISHED'}


class NWAddMultipleImages(Operator, ImportHelper):
    """Add multiple images at once"""
    bl_idname = 'node.nw_add_multiple_images'
    bl_label = 'Open Selected Images'
    bl_options = {'REGISTER', 'UNDO'}
    directory = StringProperty(subtype="DIR_PATH")
    files = CollectionProperty(type=bpy.types.OperatorFileListElement, options={'HIDDEN', 'SKIP_SAVE'})

    def execute(self, context):
        nodes, links = get_nodes_links(context)

        xloc, yloc = context.region.view2d.region_to_view(context.area.width/2, context.area.height/2)

        if context.space_data.node_tree.type == 'SHADER':
            node_type = "ShaderNodeTexImage"
        elif context.space_data.node_tree.type == 'COMPOSITING':
            node_type = "CompositorNodeImage"
        else:
            self.report({'ERROR'}, "Unsupported Node Tree type!")
            return {'CANCELLED'}

        new_nodes = []
        for f in self.files:
            fname = f.name

            node = nodes.new(node_type)
            new_nodes.append(node)
            node.label = fname
            node.hide = True
            node.width_hidden = 100
            node.location.x = xloc
            node.location.y = yloc
            yloc -= 40

            img = bpy.data.images.load(self.directory+fname)
            node.image = img

        # shift new nodes up to center of tree
        list_size = new_nodes[0].location.y - new_nodes[-1].location.y
        for node in nodes:
            if node in new_nodes:
                node.select = True
                node.location.y += (list_size/2)
            else:
                node.select = False
        return {'FINISHED'}


class NWViewerFocus(bpy.types.Operator):
    """Set the viewer tile center to the mouse position"""
    bl_idname = "node.nw_viewer_focus"
    bl_label = "Viewer Focus"

    x = bpy.props.IntProperty()
    y = bpy.props.IntProperty()

    @classmethod
    def poll(cls, context):
        return nw_check(context) and context.space_data.tree_type == 'CompositorNodeTree'

    def execute(self, context):
        return {'FINISHED'}

    def invoke(self, context, event):
        render = context.scene.render
        space = context.space_data
        percent = render.resolution_percentage*0.01

        nodes, links = get_nodes_links(context)
        viewers = [n for n in nodes if n.type == 'VIEWER']

        if viewers:
            mlocx = event.mouse_region_x
            mlocy = event.mouse_region_y
            select_node = bpy.ops.node.select(mouse_x=mlocx, mouse_y=mlocy, extend=False)

            if not 'FINISHED' in select_node:  # only run if we're not clicking on a node
                region_x = context.region.width
                region_y = context.region.height

                region_center_x = context.region.width  / 2
                region_center_y = context.region.height / 2

                bd_x = render.resolution_x * percent * space.backdrop_zoom
                bd_y = render.resolution_y * percent * space.backdrop_zoom

                backdrop_center_x = (bd_x / 2) - space.backdrop_x
                backdrop_center_y = (bd_y / 2) - space.backdrop_y

                margin_x = region_center_x - backdrop_center_x
                margin_y = region_center_y - backdrop_center_y

                abs_mouse_x = (mlocx - margin_x) / bd_x
                abs_mouse_y = (mlocy - margin_y) / bd_y

                for node in viewers:
                    node.center_x = abs_mouse_x
                    node.center_y = abs_mouse_y
            else:
                return {'PASS_THROUGH'}

        return self.execute(context)


class NWSaveViewer(bpy.types.Operator, ExportHelper):
    """Save the current viewer node to an image file"""
    bl_idname = "node.nw_save_viewer"
    bl_label = "Save This Image"
    filepath = StringProperty(subtype="FILE_PATH")
    filename_ext = EnumProperty(
            name="Format",
            description="Choose the file format to save to",
            items=(('.bmp', "PNG", ""),
                   ('.rgb', 'IRIS', ""),
                   ('.png', 'PNG', ""),
                   ('.jpg', 'JPEG', ""),
                   ('.jp2', 'JPEG2000', ""),
                   ('.tga', 'TARGA', ""),
                   ('.cin', 'CINEON', ""),
                   ('.dpx', 'DPX', ""),
                   ('.exr', 'OPEN_EXR', ""),
                   ('.hdr', 'HDR', ""),
                   ('.tif', 'TIFF', "")),
            default='.png',
            )

    @classmethod
    def poll(cls, context):
        valid = False
        if nw_check(context):
            if context.space_data.tree_type == 'CompositorNodeTree':
                if "Viewer Node" in [i.name for i in bpy.data.images]:
                    if sum(bpy.data.images["Viewer Node"].size) > 0:  # False if not connected or connected but no image
                        valid = True
        return valid

    def execute(self, context):
        fp = self.filepath
        if fp:
            formats = {
                       '.bmp': 'BMP',
                       '.rgb': 'IRIS',
                       '.png': 'PNG',
                       '.jpg': 'JPEG',
                       '.jpeg': 'JPEG',
                       '.jp2': 'JPEG2000',
                       '.tga': 'TARGA',
                       '.cin': 'CINEON',
                       '.dpx': 'DPX',
                       '.exr': 'OPEN_EXR',
                       '.hdr': 'HDR',
                       '.tiff': 'TIFF',
                       '.tif': 'TIFF'}
            basename, ext = path.splitext(fp)
            old_render_format = context.scene.render.image_settings.file_format
            context.scene.render.image_settings.file_format = formats[self.filename_ext]
            context.area.type = "IMAGE_EDITOR"
            context.area.spaces[0].image = bpy.data.images['Viewer Node']
            context.area.spaces[0].image.save_render(fp)
            context.area.type = "NODE_EDITOR"
            context.scene.render.image_settings.file_format = old_render_format
            return {'FINISHED'}


class NWResetNodes(bpy.types.Operator):
    """Reset Nodes in Selection"""
    bl_idname = "node.nw_reset_nodes"
    bl_label = "Reset Nodes"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        space = context.space_data
        return space.type == 'NODE_EDITOR'

    def execute(self, context):
        node_active = context.active_node
        node_selected = context.selected_nodes
        node_ignore = ["FRAME","REROUTE", "GROUP"]

        # Check if one node is selected at least
        if not (len(node_selected) > 0):
            self.report({'ERROR'}, "1 node must be selected at least")
            return {'CANCELLED'}

        active_node_name = node_active.name if node_active.select else None
        valid_nodes = [n for n in node_selected if n.type not in node_ignore]

        # Create output lists
        selected_node_names = [n.name for n in node_selected]
        success_names = []

        # Reset all valid children in a frame
        node_active_is_frame = False
        if len(node_selected) == 1 and node_active.type == "FRAME":
            node_tree = node_active.id_data
            children = [n for n in node_tree.nodes if n.parent == node_active]
            if children:
                valid_nodes = [n for n in children if n.type not in node_ignore]
                selected_node_names = [n.name for n in children if n.type not in node_ignore]
                node_active_is_frame = True

        # Check if valid nodes in selection
        if not (len(valid_nodes) > 0):
            # Check for frames only
            frames_selected = [n for n in node_selected if n.type == "FRAME"]
            if (len(frames_selected) > 1 and len(frames_selected) == len(node_selected)):
                self.report({'ERROR'}, "Please select only 1 frame to reset")
            else:
                self.report({'ERROR'}, "No valid node(s) in selection")
            return {'CANCELLED'}

        # Report nodes that are not valid
        if len(valid_nodes) != len(node_selected) and node_active_is_frame is False:
            valid_node_names = [n.name for n in valid_nodes]
            not_valid_names = list(set(selected_node_names) - set(valid_node_names))
            self.report({'INFO'}, "Ignored {}".format(", ".join(not_valid_names)))

        # Deselect all nodes
        for i in node_selected:
            i.select = False

        # Run through all valid nodes
        for node in valid_nodes:

            parent = node.parent if node.parent else None
            node_loc = [node.location.x, node.location.y]

            node_tree = node.id_data
            props_to_copy = 'bl_idname name location height width'.split(' ')

            reconnections = []
            mappings = chain.from_iterable([node.inputs, node.outputs])
            for i in (i for i in mappings if i.is_linked):
                for L in i.links:
                    reconnections.append([L.from_socket.path_from_id(), L.to_socket.path_from_id()])

            props = {j: getattr(node, j) for j in props_to_copy}

            new_node = node_tree.nodes.new(props['bl_idname'])
            props_to_copy.pop(0)

            for prop in props_to_copy:
                setattr(new_node, prop, props[prop])

            nodes = node_tree.nodes
            nodes.remove(node)
            new_node.name = props['name']

            if parent:
                new_node.parent = parent
                new_node.location = node_loc

            for str_from, str_to in reconnections:
                node_tree.links.new(eval(str_from), eval(str_to))

            new_node.select = False
            success_names.append(new_node.name)

        # Reselect all nodes
        if selected_node_names and node_active_is_frame is False:
            for i in selected_node_names:
                node_tree.nodes[i].select = True

        if active_node_name is not None:
            node_tree.nodes[active_node_name].select = True
            node_tree.nodes.active = node_tree.nodes[active_node_name]

        self.report({'INFO'}, "Successfully reset {}".format(", ".join(success_names)))
        return {'FINISHED'}


#
#  P A N E L
#

def drawlayout(context, layout, mode='non-panel'):
    tree_type = context.space_data.tree_type

    col = layout.column(align=True)
    col.menu(NWMergeNodesMenu.bl_idname)
    col.separator()

    col = layout.column(align=True)
    col.menu(NWSwitchNodeTypeMenu.bl_idname, text="Switch Node Type")
    col.separator()

    if tree_type == 'ShaderNodeTree' and context.scene.render.engine == 'CYCLES':
        col = layout.column(align=True)
        col.operator(NWAddTextureSetup.bl_idname, text="Add Texture Setup", icon='NODE_SEL')
        col.operator(NWAddPrincipledSetup.bl_idname, text="Add Principled Setup", icon='NODE_SEL')
        col.separator()

    col = layout.column(align=True)
    col.operator(NWDetachOutputs.bl_idname, icon='UNLINKED')
    col.operator(NWSwapLinks.bl_idname)
    col.menu(NWAddReroutesMenu.bl_idname, text="Add Reroutes", icon='LAYER_USED')
    col.separator()

    col = layout.column(align=True)
    col.menu(NWLinkActiveToSelectedMenu.bl_idname, text="Link Active To Selected", icon='LINKED')
    col.operator(NWLinkToOutputNode.bl_idname, icon='DRIVER')
    col.separator()

    col = layout.column(align=True)
    if mode == 'panel':
        row = col.row(align=True)
        row.operator(NWClearLabel.bl_idname).option = True
        row.operator(NWModifyLabels.bl_idname)
    else:
        col.operator(NWClearLabel.bl_idname).option = True
        col.operator(NWModifyLabels.bl_idname)
    col.menu(NWBatchChangeNodesMenu.bl_idname, text="Batch Change")
    col.separator()
    col.menu(NWCopyToSelectedMenu.bl_idname, text="Copy to Selected")
    col.separator()

    col = layout.column(align=True)
    if tree_type == 'CompositorNodeTree':
        col.operator(NWResetBG.bl_idname, icon='ZOOM_PREVIOUS')
    col.operator(NWReloadImages.bl_idname, icon='FILE_REFRESH')
    col.separator()

    col = layout.column(align=True)
    col.operator(NWFrameSelected.bl_idname, icon='STICKY_UVS_LOC')
    col.separator()

    col = layout.column(align=True)
    col.operator(NWAlignNodes.bl_idname, icon='ALIGN')
    col.separator()

    col = layout.column(align=True)
    col.operator(NWDeleteUnused.bl_idname, icon='CANCEL')
    col.separator()


class NodeWranglerPanel(Panel, NWBase):
    bl_idname = "NODE_PT_nw_node_wrangler"
    bl_space_type = 'NODE_EDITOR'
    bl_label = "Node Wrangler"
    bl_region_type = "TOOLS"
    bl_category = "Node Wrangler"

    prepend = StringProperty(
        name='prepend',
    )
    append = StringProperty()
    remove = StringProperty()

    def draw(self, context):
        self.layout.label(text="(Quick access: Ctrl+Space)")
        drawlayout(context, self.layout, mode='panel')


#
#  M E N U S
#
class NodeWranglerMenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_node_wrangler_menu"
    bl_label = "Node Wrangler"

    def draw(self, context):
        drawlayout(context, self.layout)


class NWMergeNodesMenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_merge_nodes_menu"
    bl_label = "Merge Selected Nodes"

    def draw(self, context):
        type = context.space_data.tree_type
        layout = self.layout
        if type == 'ShaderNodeTree' and context.scene.render.engine == 'CYCLES':
            layout.menu(NWMergeShadersMenu.bl_idname, text="Use Shaders")
        layout.menu(NWMergeMixMenu.bl_idname, text="Use Mix Nodes")
        layout.menu(NWMergeMathMenu.bl_idname, text="Use Math Nodes")
        props = layout.operator(NWMergeNodes.bl_idname, text="Use Z-Combine Nodes")
        props.mode = 'MIX'
        props.merge_type = 'ZCOMBINE'
        props = layout.operator(NWMergeNodes.bl_idname, text="Use Alpha Over Nodes")
        props.mode = 'MIX'
        props.merge_type = 'ALPHAOVER'


class NWMergeShadersMenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_merge_shaders_menu"
    bl_label = "Merge Selected Nodes using Shaders"

    def draw(self, context):
        layout = self.layout
        for type in ('MIX', 'ADD'):
            props = layout.operator(NWMergeNodes.bl_idname, text=type)
            props.mode = type
            props.merge_type = 'SHADER'


class NWMergeMixMenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_merge_mix_menu"
    bl_label = "Merge Selected Nodes using Mix"

    def draw(self, context):
        layout = self.layout
        for type, name, description in blend_types:
            props = layout.operator(NWMergeNodes.bl_idname, text=name)
            props.mode = type
            props.merge_type = 'MIX'


class NWConnectionListOutputs(Menu, NWBase):
    bl_idname = "NODE_MT_nw_connection_list_out"
    bl_label = "From:"

    def draw(self, context):
        layout = self.layout
        nodes, links = get_nodes_links(context)

        n1 = nodes[context.scene.NWLazySource]

        if n1.type == "R_LAYERS":
            index=0
            for o in n1.outputs:
                if o.enabled:  # Check which passes the render layer has enabled
                    layout.operator(NWCallInputsMenu.bl_idname, text=o.name, icon="RADIOBUT_OFF").from_socket=index
                index+=1
        else:
            index=0
            for o in n1.outputs:
                layout.operator(NWCallInputsMenu.bl_idname, text=o.name, icon="RADIOBUT_OFF").from_socket=index
                index+=1


class NWConnectionListInputs(Menu, NWBase):
    bl_idname = "NODE_MT_nw_connection_list_in"
    bl_label = "To:"

    def draw(self, context):
        layout = self.layout
        nodes, links = get_nodes_links(context)

        n2 = nodes[context.scene.NWLazyTarget]

        index = 0
        for i in n2.inputs:
            op = layout.operator(NWMakeLink.bl_idname, text=i.name, icon="FORWARD")
            op.from_socket = context.scene.NWSourceSocket
            op.to_socket = index
            index+=1


class NWMergeMathMenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_merge_math_menu"
    bl_label = "Merge Selected Nodes using Math"

    def draw(self, context):
        layout = self.layout
        for type, name, description in operations:
            props = layout.operator(NWMergeNodes.bl_idname, text=name)
            props.mode = type
            props.merge_type = 'MATH'


class NWBatchChangeNodesMenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_batch_change_nodes_menu"
    bl_label = "Batch Change Selected Nodes"

    def draw(self, context):
        layout = self.layout
        layout.menu(NWBatchChangeBlendTypeMenu.bl_idname)
        layout.menu(NWBatchChangeOperationMenu.bl_idname)


class NWBatchChangeBlendTypeMenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_batch_change_blend_type_menu"
    bl_label = "Batch Change Blend Type"

    def draw(self, context):
        layout = self.layout
        for type, name, description in blend_types:
            props = layout.operator(NWBatchChangeNodes.bl_idname, text=name)
            props.blend_type = type
            props.operation = 'CURRENT'


class NWBatchChangeOperationMenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_batch_change_operation_menu"
    bl_label = "Batch Change Math Operation"

    def draw(self, context):
        layout = self.layout
        for type, name, description in operations:
            props = layout.operator(NWBatchChangeNodes.bl_idname, text=name)
            props.blend_type = 'CURRENT'
            props.operation = type


class NWCopyToSelectedMenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_copy_node_properties_menu"
    bl_label = "Copy to Selected"

    def draw(self, context):
        layout = self.layout
        layout.operator(NWCopySettings.bl_idname, text="Settings from Active")
        layout.menu(NWCopyLabelMenu.bl_idname)


class NWCopyLabelMenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_copy_label_menu"
    bl_label = "Copy Label"

    def draw(self, context):
        layout = self.layout
        layout.operator(NWCopyLabel.bl_idname, text="from Active Node's Label").option = 'FROM_ACTIVE'
        layout.operator(NWCopyLabel.bl_idname, text="from Linked Node's Label").option = 'FROM_NODE'
        layout.operator(NWCopyLabel.bl_idname, text="from Linked Output's Name").option = 'FROM_SOCKET'


class NWAddReroutesMenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_add_reroutes_menu"
    bl_label = "Add Reroutes"
    bl_description = "Add Reroute Nodes to Selected Nodes' Outputs"

    def draw(self, context):
        layout = self.layout
        layout.operator(NWAddReroutes.bl_idname, text="to All Outputs").option = 'ALL'
        layout.operator(NWAddReroutes.bl_idname, text="to Loose Outputs").option = 'LOOSE'
        layout.operator(NWAddReroutes.bl_idname, text="to Linked Outputs").option = 'LINKED'


class NWLinkActiveToSelectedMenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_link_active_to_selected_menu"
    bl_label = "Link Active to Selected"

    def draw(self, context):
        layout = self.layout
        layout.menu(NWLinkStandardMenu.bl_idname)
        layout.menu(NWLinkUseNodeNameMenu.bl_idname)
        layout.menu(NWLinkUseOutputsNamesMenu.bl_idname)


class NWLinkStandardMenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_link_standard_menu"
    bl_label = "To All Selected"

    def draw(self, context):
        layout = self.layout
        props = layout.operator(NWLinkActiveToSelected.bl_idname, text="Don't Replace Links")
        props.replace = False
        props.use_node_name = False
        props.use_outputs_names = False
        props = layout.operator(NWLinkActiveToSelected.bl_idname, text="Replace Links")
        props.replace = True
        props.use_node_name = False
        props.use_outputs_names = False


class NWLinkUseNodeNameMenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_link_use_node_name_menu"
    bl_label = "Use Node Name/Label"

    def draw(self, context):
        layout = self.layout
        props = layout.operator(NWLinkActiveToSelected.bl_idname, text="Don't Replace Links")
        props.replace = False
        props.use_node_name = True
        props.use_outputs_names = False
        props = layout.operator(NWLinkActiveToSelected.bl_idname, text="Replace Links")
        props.replace = True
        props.use_node_name = True
        props.use_outputs_names = False


class NWLinkUseOutputsNamesMenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_link_use_outputs_names_menu"
    bl_label = "Use Outputs Names"

    def draw(self, context):
        layout = self.layout
        props = layout.operator(NWLinkActiveToSelected.bl_idname, text="Don't Replace Links")
        props.replace = False
        props.use_node_name = False
        props.use_outputs_names = True
        props = layout.operator(NWLinkActiveToSelected.bl_idname, text="Replace Links")
        props.replace = True
        props.use_node_name = False
        props.use_outputs_names = True


class NWVertColMenu(bpy.types.Menu):
    bl_idname = "NODE_MT_nw_node_vertex_color_menu"
    bl_label = "Vertex Colors"

    @classmethod
    def poll(cls, context):
        valid = False
        if nw_check(context):
            snode = context.space_data
            valid = snode.tree_type == 'ShaderNodeTree' and context.scene.render.engine == 'CYCLES'
        return valid

    def draw(self, context):
        l = self.layout
        nodes, links = get_nodes_links(context)
        mat = context.object.active_material

        objs = []
        for obj in bpy.data.objects:
            for slot in obj.material_slots:
                if slot.material == mat:
                    objs.append(obj)
        vcols = []
        for obj in objs:
            if obj.data.vertex_colors:
                for vcol in obj.data.vertex_colors:
                    vcols.append(vcol.name)
        vcols = list(set(vcols))  # get a unique list

        if vcols:
            for vcol in vcols:
                l.operator(NWAddAttrNode.bl_idname, text=vcol).attr_name = vcol
        else:
            l.label("No Vertex Color layers on objects with this material")


class NWSwitchNodeTypeMenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_node_type_menu"
    bl_label = "Switch Type to..."

    def draw(self, context):
        layout = self.layout
        tree = context.space_data.node_tree
        if tree.type == 'SHADER':
            if context.scene.render.engine == 'CYCLES':
                layout.menu(NWSwitchShadersInputSubmenu.bl_idname)
                layout.menu(NWSwitchShadersOutputSubmenu.bl_idname)
                layout.menu(NWSwitchShadersShaderSubmenu.bl_idname)
                layout.menu(NWSwitchShadersTextureSubmenu.bl_idname)
                layout.menu(NWSwitchShadersColorSubmenu.bl_idname)
                layout.menu(NWSwitchShadersVectorSubmenu.bl_idname)
                layout.menu(NWSwitchShadersConverterSubmenu.bl_idname)
                layout.menu(NWSwitchShadersLayoutSubmenu.bl_idname)
            if context.scene.render.engine != 'CYCLES':
                layout.menu(NWSwitchMatInputSubmenu.bl_idname)
                layout.menu(NWSwitchMatOutputSubmenu.bl_idname)
                layout.menu(NWSwitchMatColorSubmenu.bl_idname)
                layout.menu(NWSwitchMatVectorSubmenu.bl_idname)
                layout.menu(NWSwitchMatConverterSubmenu.bl_idname)
                layout.menu(NWSwitchMatLayoutSubmenu.bl_idname)
        if tree.type == 'COMPOSITING':
            layout.menu(NWSwitchCompoInputSubmenu.bl_idname)
            layout.menu(NWSwitchCompoOutputSubmenu.bl_idname)
            layout.menu(NWSwitchCompoColorSubmenu.bl_idname)
            layout.menu(NWSwitchCompoConverterSubmenu.bl_idname)
            layout.menu(NWSwitchCompoFilterSubmenu.bl_idname)
            layout.menu(NWSwitchCompoVectorSubmenu.bl_idname)
            layout.menu(NWSwitchCompoMatteSubmenu.bl_idname)
            layout.menu(NWSwitchCompoDistortSubmenu.bl_idname)
            layout.menu(NWSwitchCompoLayoutSubmenu.bl_idname)
        if tree.type == 'TEXTURE':
            layout.menu(NWSwitchTexInputSubmenu.bl_idname)
            layout.menu(NWSwitchTexOutputSubmenu.bl_idname)
            layout.menu(NWSwitchTexColorSubmenu.bl_idname)
            layout.menu(NWSwitchTexPatternSubmenu.bl_idname)
            layout.menu(NWSwitchTexTexturesSubmenu.bl_idname)
            layout.menu(NWSwitchTexConverterSubmenu.bl_idname)
            layout.menu(NWSwitchTexDistortSubmenu.bl_idname)
            layout.menu(NWSwitchTexLayoutSubmenu.bl_idname)


class NWSwitchShadersInputSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_shaders_input_submenu"
    bl_label = "Input"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(shaders_input_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchShadersOutputSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_shaders_output_submenu"
    bl_label = "Output"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(shaders_output_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchShadersShaderSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_shaders_shader_submenu"
    bl_label = "Shader"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(shaders_shader_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchShadersTextureSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_shaders_texture_submenu"
    bl_label = "Texture"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(shaders_texture_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchShadersColorSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_shaders_color_submenu"
    bl_label = "Color"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(shaders_color_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchShadersVectorSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_shaders_vector_submenu"
    bl_label = "Vector"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(shaders_vector_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchShadersConverterSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_shaders_converter_submenu"
    bl_label = "Converter"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(shaders_converter_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchShadersLayoutSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_shaders_layout_submenu"
    bl_label = "Layout"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(shaders_layout_nodes_props, key=lambda k: k[2]):
            if node_type != 'FRAME':
                props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
                props.to_type = ident


class NWSwitchCompoInputSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_compo_input_submenu"
    bl_label = "Input"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(compo_input_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchCompoOutputSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_compo_output_submenu"
    bl_label = "Output"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(compo_output_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchCompoColorSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_compo_color_submenu"
    bl_label = "Color"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(compo_color_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchCompoConverterSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_compo_converter_submenu"
    bl_label = "Converter"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(compo_converter_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchCompoFilterSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_compo_filter_submenu"
    bl_label = "Filter"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(compo_filter_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchCompoVectorSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_compo_vector_submenu"
    bl_label = "Vector"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(compo_vector_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchCompoMatteSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_compo_matte_submenu"
    bl_label = "Matte"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(compo_matte_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchCompoDistortSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_compo_distort_submenu"
    bl_label = "Distort"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(compo_distort_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchCompoLayoutSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_compo_layout_submenu"
    bl_label = "Layout"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(compo_layout_nodes_props, key=lambda k: k[2]):
            if node_type != 'FRAME':
                props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
                props.to_type = ident


class NWSwitchMatInputSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_mat_input_submenu"
    bl_label = "Input"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(blender_mat_input_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchMatOutputSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_mat_output_submenu"
    bl_label = "Output"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(blender_mat_output_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchMatColorSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_mat_color_submenu"
    bl_label = "Color"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(blender_mat_color_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchMatVectorSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_mat_vector_submenu"
    bl_label = "Vector"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(blender_mat_vector_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchMatConverterSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_mat_converter_submenu"
    bl_label = "Converter"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(blender_mat_converter_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchMatLayoutSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_mat_layout_submenu"
    bl_label = "Layout"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(blender_mat_layout_nodes_props, key=lambda k: k[2]):
            if node_type != 'FRAME':
                props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
                props.to_type = ident


class NWSwitchTexInputSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_tex_input_submenu"
    bl_label = "Input"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(texture_input_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchTexOutputSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_tex_output_submenu"
    bl_label = "Output"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(texture_output_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchTexColorSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_tex_color_submenu"
    bl_label = "Color"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(texture_color_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchTexPatternSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_tex_pattern_submenu"
    bl_label = "Pattern"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(texture_pattern_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchTexTexturesSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_tex_textures_submenu"
    bl_label = "Textures"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(texture_textures_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchTexConverterSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_tex_converter_submenu"
    bl_label = "Converter"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(texture_converter_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchTexDistortSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_tex_distort_submenu"
    bl_label = "Distort"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(texture_distort_nodes_props, key=lambda k: k[2]):
            props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
            props.to_type = ident


class NWSwitchTexLayoutSubmenu(Menu, NWBase):
    bl_idname = "NODE_MT_nw_switch_tex_layout_submenu"
    bl_label = "Layout"

    def draw(self, context):
        layout = self.layout
        for ident, node_type, rna_name in sorted(texture_layout_nodes_props, key=lambda k: k[2]):
            if node_type != 'FRAME':
                props = layout.operator(NWSwitchNodeType.bl_idname, text=rna_name)
                props.to_type = ident


#
#  APPENDAGES TO EXISTING UI
#


def select_parent_children_buttons(self, context):
    layout = self.layout
    layout.operator(NWSelectParentChildren.bl_idname, text="Select frame's members (children)").option = 'CHILD'
    layout.operator(NWSelectParentChildren.bl_idname, text="Select parent frame").option = 'PARENT'


def attr_nodes_menu_func(self, context):
    col = self.layout.column(align=True)
    col.menu("NODE_MT_nw_node_vertex_color_menu")
    col.separator()


def multipleimages_menu_func(self, context):
    col = self.layout.column(align=True)
    col.operator(NWAddMultipleImages.bl_idname, text="Multiple Images")
    col.operator(NWAddSequence.bl_idname, text="Image Sequence")
    col.separator()


def bgreset_menu_func(self, context):
    self.layout.operator(NWResetBG.bl_idname)


def save_viewer_menu_func(self, context):
    if nw_check(context):
        if context.space_data.tree_type == 'CompositorNodeTree':
            if context.scene.node_tree.nodes.active:
                if context.scene.node_tree.nodes.active.type == "VIEWER":
                    self.layout.operator(NWSaveViewer.bl_idname, icon='FILE_IMAGE')


def reset_nodes_button(self, context):
    node_active = context.active_node
    node_selected = context.selected_nodes
    node_ignore = ["FRAME","REROUTE", "GROUP"]

    # Check if active node is in the selection and respective type
    if (len(node_selected) == 1) and node_active.select and node_active.type not in node_ignore:
        row = self.layout.row()
        row.operator("node.nw_reset_nodes", text="Reset Node", icon="FILE_REFRESH")
        self.layout.separator()

    elif (len(node_selected) == 1) and node_active.select and node_active.type == "FRAME":
        row = self.layout.row()
        row.operator("node.nw_reset_nodes", text="Reset Nodes in Frame", icon="FILE_REFRESH")
        self.layout.separator()


#
#  REGISTER/UNREGISTER CLASSES AND KEYMAP ITEMS
#

addon_keymaps = []
# kmi_defs entry: (identifier, key, action, CTRL, SHIFT, ALT, props, nice name)
# props entry: (property name, property value)
kmi_defs = (
    # MERGE NODES
    # NWMergeNodes with Ctrl (AUTO).
    (NWMergeNodes.bl_idname, 'NUMPAD_0', 'PRESS', True, False, False,
        (('mode', 'MIX'), ('merge_type', 'AUTO'),), "Merge Nodes (Automatic)"),
    (NWMergeNodes.bl_idname, 'ZERO', 'PRESS', True, False, False,
        (('mode', 'MIX'), ('merge_type', 'AUTO'),), "Merge Nodes (Automatic)"),
    (NWMergeNodes.bl_idname, 'NUMPAD_PLUS', 'PRESS', True, False, False,
        (('mode', 'ADD'), ('merge_type', 'AUTO'),), "Merge Nodes (Add)"),
    (NWMergeNodes.bl_idname, 'EQUAL', 'PRESS', True, False, False,
        (('mode', 'ADD'), ('merge_type', 'AUTO'),), "Merge Nodes (Add)"),
    (NWMergeNodes.bl_idname, 'NUMPAD_ASTERIX', 'PRESS', True, False, False,
        (('mode', 'MULTIPLY'), ('merge_type', 'AUTO'),), "Merge Nodes (Multiply)"),
    (NWMergeNodes.bl_idname, 'EIGHT', 'PRESS', True, False, False,
        (('mode', 'MULTIPLY'), ('merge_type', 'AUTO'),), "Merge Nodes (Multiply)"),
    (NWMergeNodes.bl_idname, 'NUMPAD_MINUS', 'PRESS', True, False, False,
        (('mode', 'SUBTRACT'), ('merge_type', 'AUTO'),), "Merge Nodes (Subtract)"),
    (NWMergeNodes.bl_idname, 'MINUS', 'PRESS', True, False, False,
        (('mode', 'SUBTRACT'), ('merge_type', 'AUTO'),), "Merge Nodes (Subtract)"),
    (NWMergeNodes.bl_idname, 'NUMPAD_SLASH', 'PRESS', True, False, False,
        (('mode', 'DIVIDE'), ('merge_type', 'AUTO'),), "Merge Nodes (Divide)"),
    (NWMergeNodes.bl_idname, 'SLASH', 'PRESS', True, False, False,
        (('mode', 'DIVIDE'), ('merge_type', 'AUTO'),), "Merge Nodes (Divide)"),
    (NWMergeNodes.bl_idname, 'COMMA', 'PRESS', True, False, False,
        (('mode', 'LESS_THAN'), ('merge_type', 'MATH'),), "Merge Nodes (Less than)"),
    (NWMergeNodes.bl_idname, 'PERIOD', 'PRESS', True, False, False,
        (('mode', 'GREATER_THAN'), ('merge_type', 'MATH'),), "Merge Nodes (Greater than)"),
    (NWMergeNodes.bl_idname, 'NUMPAD_PERIOD', 'PRESS', True, False, False,
        (('mode', 'MIX'), ('merge_type', 'ZCOMBINE'),), "Merge Nodes (Z-Combine)"),
    # NWMergeNodes with Ctrl Alt (MIX or ALPHAOVER)
    (NWMergeNodes.bl_idname, 'NUMPAD_0', 'PRESS', True, False, True,
        (('mode', 'MIX'), ('merge_type', 'ALPHAOVER'),), "Merge Nodes (Alpha Over)"),
    (NWMergeNodes.bl_idname, 'ZERO', 'PRESS', True, False, True,
        (('mode', 'MIX'), ('merge_type', 'ALPHAOVER'),), "Merge Nodes (Alpha Over)"),
    (NWMergeNodes.bl_idname, 'NUMPAD_PLUS', 'PRESS', True, False, True,
        (('mode', 'ADD'), ('merge_type', 'MIX'),), "Merge Nodes (Color, Add)"),
    (NWMergeNodes.bl_idname, 'EQUAL', 'PRESS', True, False, True,
        (('mode', 'ADD'), ('merge_type', 'MIX'),), "Merge Nodes (Color, Add)"),
    (NWMergeNodes.bl_idname, 'NUMPAD_ASTERIX', 'PRESS', True, False, True,
        (('mode', 'MULTIPLY'), ('merge_type', 'MIX'),), "Merge Nodes (Color, Multiply)"),
    (NWMergeNodes.bl_idname, 'EIGHT', 'PRESS', True, False, True,
        (('mode', 'MULTIPLY'), ('merge_type', 'MIX'),), "Merge Nodes (Color, Multiply)"),
    (NWMergeNodes.bl_idname, 'NUMPAD_MINUS', 'PRESS', True, False, True,
        (('mode', 'SUBTRACT'), ('merge_type', 'MIX'),), "Merge Nodes (Color, Subtract)"),
    (NWMergeNodes.bl_idname, 'MINUS', 'PRESS', True, False, True,
        (('mode', 'SUBTRACT'), ('merge_type', 'MIX'),), "Merge Nodes (Color, Subtract)"),
    (NWMergeNodes.bl_idname, 'NUMPAD_SLASH', 'PRESS', True, False, True,
        (('mode', 'DIVIDE'), ('merge_type', 'MIX'),), "Merge Nodes (Color, Divide)"),
    (NWMergeNodes.bl_idname, 'SLASH', 'PRESS', True, False, True,
        (('mode', 'DIVIDE'), ('merge_type', 'MIX'),), "Merge Nodes (Color, Divide)"),
    # NWMergeNodes with Ctrl Shift (MATH)
    (NWMergeNodes.bl_idname, 'NUMPAD_PLUS', 'PRESS', True, True, False,
        (('mode', 'ADD'), ('merge_type', 'MATH'),), "Merge Nodes (Math, Add)"),
    (NWMergeNodes.bl_idname, 'EQUAL', 'PRESS', True, True, False,
        (('mode', 'ADD'), ('merge_type', 'MATH'),), "Merge Nodes (Math, Add)"),
    (NWMergeNodes.bl_idname, 'NUMPAD_ASTERIX', 'PRESS', True, True, False,
        (('mode', 'MULTIPLY'), ('merge_type', 'MATH'),), "Merge Nodes (Math, Multiply)"),
    (NWMergeNodes.bl_idname, 'EIGHT', 'PRESS', True, True, False,
        (('mode', 'MULTIPLY'), ('merge_type', 'MATH'),), "Merge Nodes (Math, Multiply)"),
    (NWMergeNodes.bl_idname, 'NUMPAD_MINUS', 'PRESS', True, True, False,
        (('mode', 'SUBTRACT'), ('merge_type', 'MATH'),), "Merge Nodes (Math, Subtract)"),
    (NWMergeNodes.bl_idname, 'MINUS', 'PRESS', True, True, False,
        (('mode', 'SUBTRACT'), ('merge_type', 'MATH'),), "Merge Nodes (Math, Subtract)"),
    (NWMergeNodes.bl_idname, 'NUMPAD_SLASH', 'PRESS', True, True, False,
        (('mode', 'DIVIDE'), ('merge_type', 'MATH'),), "Merge Nodes (Math, Divide)"),
    (NWMergeNodes.bl_idname, 'SLASH', 'PRESS', True, True, False,
        (('mode', 'DIVIDE'), ('merge_type', 'MATH'),), "Merge Nodes (Math, Divide)"),
    (NWMergeNodes.bl_idname, 'COMMA', 'PRESS', True, True, False,
        (('mode', 'LESS_THAN'), ('merge_type', 'MATH'),), "Merge Nodes (Math, Less than)"),
    (NWMergeNodes.bl_idname, 'PERIOD', 'PRESS', True, True, False,
        (('mode', 'GREATER_THAN'), ('merge_type', 'MATH'),), "Merge Nodes (Math, Greater than)"),
    # BATCH CHANGE NODES
    # NWBatchChangeNodes with Alt
    (NWBatchChangeNodes.bl_idname, 'NUMPAD_0', 'PRESS', False, False, True,
        (('blend_type', 'MIX'), ('operation', 'CURRENT'),), "Batch change blend type (Mix)"),
    (NWBatchChangeNodes.bl_idname, 'ZERO', 'PRESS', False, False, True,
        (('blend_type', 'MIX'), ('operation', 'CURRENT'),), "Batch change blend type (Mix)"),
    (NWBatchChangeNodes.bl_idname, 'NUMPAD_PLUS', 'PRESS', False, False, True,
        (('blend_type', 'ADD'), ('operation', 'ADD'),), "Batch change blend type (Add)"),
    (NWBatchChangeNodes.bl_idname, 'EQUAL', 'PRESS', False, False, True,
        (('blend_type', 'ADD'), ('operation', 'ADD'),), "Batch change blend type (Add)"),
    (NWBatchChangeNodes.bl_idname, 'NUMPAD_ASTERIX', 'PRESS', False, False, True,
        (('blend_type', 'MULTIPLY'), ('operation', 'MULTIPLY'),), "Batch change blend type (Multiply)"),
    (NWBatchChangeNodes.bl_idname, 'EIGHT', 'PRESS', False, False, True,
        (('blend_type', 'MULTIPLY'), ('operation', 'MULTIPLY'),), "Batch change blend type (Multiply)"),
    (NWBatchChangeNodes.bl_idname, 'NUMPAD_MINUS', 'PRESS', False, False, True,
        (('blend_type', 'SUBTRACT'), ('operation', 'SUBTRACT'),), "Batch change blend type (Subtract)"),
    (NWBatchChangeNodes.bl_idname, 'MINUS', 'PRESS', False, False, True,
        (('blend_type', 'SUBTRACT'), ('operation', 'SUBTRACT'),), "Batch change blend type (Subtract)"),
    (NWBatchChangeNodes.bl_idname, 'NUMPAD_SLASH', 'PRESS', False, False, True,
        (('blend_type', 'DIVIDE'), ('operation', 'DIVIDE'),), "Batch change blend type (Divide)"),
    (NWBatchChangeNodes.bl_idname, 'SLASH', 'PRESS', False, False, True,
        (('blend_type', 'DIVIDE'), ('operation', 'DIVIDE'),), "Batch change blend type (Divide)"),
    (NWBatchChangeNodes.bl_idname, 'COMMA', 'PRESS', False, False, True,
        (('blend_type', 'CURRENT'), ('operation', 'LESS_THAN'),), "Batch change blend type (Current)"),
    (NWBatchChangeNodes.bl_idname, 'PERIOD', 'PRESS', False, False, True,
        (('blend_type', 'CURRENT'), ('operation', 'GREATER_THAN'),), "Batch change blend type (Current)"),
    (NWBatchChangeNodes.bl_idname, 'DOWN_ARROW', 'PRESS', False, False, True,
        (('blend_type', 'NEXT'), ('operation', 'NEXT'),), "Batch change blend type (Next)"),
    (NWBatchChangeNodes.bl_idname, 'UP_ARROW', 'PRESS', False, False, True,
        (('blend_type', 'PREV'), ('operation', 'PREV'),), "Batch change blend type (Previous)"),
    # LINK ACTIVE TO SELECTED
    # Don't use names, don't replace links (K)
    (NWLinkActiveToSelected.bl_idname, 'K', 'PRESS', False, False, False,
        (('replace', False), ('use_node_name', False), ('use_outputs_names', False),), "Link active to selected (Don't replace links)"),
    # Don't use names, replace links (Shift K)
    (NWLinkActiveToSelected.bl_idname, 'K', 'PRESS', False, True, False,
        (('replace', True), ('use_node_name', False), ('use_outputs_names', False),), "Link active to selected (Replace links)"),
    # Use node name, don't replace links (')
    (NWLinkActiveToSelected.bl_idname, 'QUOTE', 'PRESS', False, False, False,
        (('replace', False), ('use_node_name', True), ('use_outputs_names', False),), "Link active to selected (Don't replace links, node names)"),
    # Use node name, replace links (Shift ')
    (NWLinkActiveToSelected.bl_idname, 'QUOTE', 'PRESS', False, True, False,
        (('replace', True), ('use_node_name', True), ('use_outputs_names', False),), "Link active to selected (Replace links, node names)"),
    # Don't use names, don't replace links (;)
    (NWLinkActiveToSelected.bl_idname, 'SEMI_COLON', 'PRESS', False, False, False,
        (('replace', False), ('use_node_name', False), ('use_outputs_names', True),), "Link active to selected (Don't replace links, output names)"),
    # Don't use names, replace links (')
    (NWLinkActiveToSelected.bl_idname, 'SEMI_COLON', 'PRESS', False, True, False,
        (('replace', True), ('use_node_name', False), ('use_outputs_names', True),), "Link active to selected (Replace links, output names)"),
    # CHANGE MIX FACTOR
    (NWChangeMixFactor.bl_idname, 'LEFT_ARROW', 'PRESS', False, False, True, (('option', -0.1),), "Reduce Mix Factor by 0.1"),
    (NWChangeMixFactor.bl_idname, 'RIGHT_ARROW', 'PRESS', False, False, True, (('option', 0.1),), "Increase Mix Factor by 0.1"),
    (NWChangeMixFactor.bl_idname, 'LEFT_ARROW', 'PRESS', False, True, True, (('option', -0.01),), "Reduce Mix Factor by 0.01"),
    (NWChangeMixFactor.bl_idname, 'RIGHT_ARROW', 'PRESS', False, True, True, (('option', 0.01),), "Increase Mix Factor by 0.01"),
    (NWChangeMixFactor.bl_idname, 'LEFT_ARROW', 'PRESS', True, True, True, (('option', 0.0),), "Set Mix Factor to 0.0"),
    (NWChangeMixFactor.bl_idname, 'RIGHT_ARROW', 'PRESS', True, True, True, (('option', 1.0),), "Set Mix Factor to 1.0"),
    (NWChangeMixFactor.bl_idname, 'NUMPAD_0', 'PRESS', True, True, True, (('option', 0.0),), "Set Mix Factor to 0.0"),
    (NWChangeMixFactor.bl_idname, 'ZERO', 'PRESS', True, True, True, (('option', 0.0),), "Set Mix Factor to 0.0"),
    (NWChangeMixFactor.bl_idname, 'NUMPAD_1', 'PRESS', True, True, True, (('option', 1.0),), "Mix Factor to 1.0"),
    (NWChangeMixFactor.bl_idname, 'ONE', 'PRESS', True, True, True, (('option', 1.0),), "Set Mix Factor to 1.0"),
    # CLEAR LABEL (Alt L)
    (NWClearLabel.bl_idname, 'L', 'PRESS', False, False, True, (('option', False),), "Clear node labels"),
    # MODIFY LABEL (Alt Shift L)
    (NWModifyLabels.bl_idname, 'L', 'PRESS', False, True, True, None, "Modify node labels"),
    # Copy Label from active to selected
    (NWCopyLabel.bl_idname, 'V', 'PRESS', False, True, False, (('option', 'FROM_ACTIVE'),), "Copy label from active to selected"),
    # DETACH OUTPUTS (Alt Shift D)
    (NWDetachOutputs.bl_idname, 'D', 'PRESS', False, True, True, None, "Detach outputs"),
    # LINK TO OUTPUT NODE (O)
    (NWLinkToOutputNode.bl_idname, 'O', 'PRESS', False, False, False, None, "Link to output node"),
    # SELECT PARENT/CHILDREN
    # Select Children
    (NWSelectParentChildren.bl_idname, 'RIGHT_BRACKET', 'PRESS', False, False, False, (('option', 'CHILD'),), "Select children"),
    # Select Parent
    (NWSelectParentChildren.bl_idname, 'LEFT_BRACKET', 'PRESS', False, False, False, (('option', 'PARENT'),), "Select Parent"),
    # Add Texture Setup
    (NWAddTextureSetup.bl_idname, 'T', 'PRESS', True, False, False, None, "Add texture setup"),
    # Add Principled BSDF Texture Setup
    (NWAddPrincipledSetup.bl_idname, 'T', 'PRESS', True, True, False, None, "Add Principled texture setup"),
    # Reset backdrop
    (NWResetBG.bl_idname, 'Z', 'PRESS', False, False, False, None, "Reset backdrop image zoom"),
    # Delete unused
    (NWDeleteUnused.bl_idname, 'X', 'PRESS', False, False, True, None, "Delete unused nodes"),
    # Frame Seleted
    (NWFrameSelected.bl_idname, 'P', 'PRESS', False, True, False, None, "Frame selected nodes"),
    # Swap Outputs
    (NWSwapLinks.bl_idname, 'S', 'PRESS', False, False, True, None, "Swap Outputs"),
    # Emission Viewer
    (NWEmissionViewer.bl_idname, 'LEFTMOUSE', 'PRESS', True, True, False, None, "Connect to Cycles Viewer node"),
    # Reload Images
    (NWReloadImages.bl_idname, 'R', 'PRESS', False, False, True, None, "Reload images"),
    # Lazy Mix
    (NWLazyMix.bl_idname, 'RIGHTMOUSE', 'PRESS', False, False, True, None, "Lazy Mix"),
    # Lazy Connect
    (NWLazyConnect.bl_idname, 'RIGHTMOUSE', 'PRESS', True, False, False, None, "Lazy Connect"),
    # Lazy Connect with Menu
    (NWLazyConnect.bl_idname, 'RIGHTMOUSE', 'PRESS', True, True, False, (('with_menu', True),), "Lazy Connect with Socket Menu"),
    # Viewer Tile Center
    (NWViewerFocus.bl_idname, 'LEFTMOUSE', 'DOUBLE_CLICK', False, False, False, None, "Set Viewers Tile Center"),
    # Align Nodes
    (NWAlignNodes.bl_idname, 'EQUAL', 'PRESS', False, True, False, None, "Align selected nodes neatly in a row/column"),
    # Reset Nodes (Back Space)
    (NWResetNodes.bl_idname, 'BACK_SPACE', 'PRESS', False, False, False, None, "Revert node back to default state, but keep connections"),
    # MENUS
    ('wm.call_menu', 'SPACE', 'PRESS', True, False, False, (('name', NodeWranglerMenu.bl_idname),), "Node Wranger menu"),
    ('wm.call_menu', 'SLASH', 'PRESS', False, False, False, (('name', NWAddReroutesMenu.bl_idname),), "Add Reroutes menu"),
    ('wm.call_menu', 'NUMPAD_SLASH', 'PRESS', False, False, False, (('name', NWAddReroutesMenu.bl_idname),), "Add Reroutes menu"),
    ('wm.call_menu', 'BACK_SLASH', 'PRESS', False, False, False, (('name', NWLinkActiveToSelectedMenu.bl_idname),), "Link active to selected (menu)"),
    ('wm.call_menu', 'C', 'PRESS', False, True, False, (('name', NWCopyToSelectedMenu.bl_idname),), "Copy to selected (menu)"),
    ('wm.call_menu', 'S', 'PRESS', False, True, False, (('name', NWSwitchNodeTypeMenu.bl_idname),), "Switch node type menu"),
)


def register():
    # props
    bpy.types.Scene.NWBusyDrawing = StringProperty(
        name="Busy Drawing!",
        default="",
        description="An internal property used to store only the first mouse position")
    bpy.types.Scene.NWLazySource = StringProperty(
        name="Lazy Source!",
        default="x",
        description="An internal property used to store the first node in a Lazy Connect operation")
    bpy.types.Scene.NWLazyTarget = StringProperty(
        name="Lazy Target!",
        default="x",
        description="An internal property used to store the last node in a Lazy Connect operation")
    bpy.types.Scene.NWSourceSocket = IntProperty(
        name="Source Socket!",
        default=0,
        description="An internal property used to store the source socket in a Lazy Connect operation")

    bpy.utils.register_module(__name__)

    # keymaps
    addon_keymaps.clear()
    kc = bpy.context.window_manager.keyconfigs.addon
    if kc:
        km = kc.keymaps.new(name='Node Editor', space_type="NODE_EDITOR")
        for (identifier, key, action, CTRL, SHIFT, ALT, props, nicename) in kmi_defs:
            kmi = km.keymap_items.new(identifier, key, action, ctrl=CTRL, shift=SHIFT, alt=ALT)
            if props:
                for prop, value in props:
                    setattr(kmi.properties, prop, value)
            addon_keymaps.append((km, kmi))

    # menu items
    bpy.types.NODE_MT_select.append(select_parent_children_buttons)
    bpy.types.NODE_MT_category_SH_NEW_INPUT.prepend(attr_nodes_menu_func)
    bpy.types.NODE_PT_category_SH_NEW_INPUT.prepend(attr_nodes_menu_func)
    bpy.types.NODE_PT_backdrop.append(bgreset_menu_func)
    bpy.types.NODE_PT_active_node_generic.append(save_viewer_menu_func)
    bpy.types.NODE_MT_category_SH_NEW_TEXTURE.prepend(multipleimages_menu_func)
    bpy.types.NODE_PT_category_SH_NEW_TEXTURE.prepend(multipleimages_menu_func)
    bpy.types.NODE_MT_category_CMP_INPUT.prepend(multipleimages_menu_func)
    bpy.types.NODE_PT_category_CMP_INPUT.prepend(multipleimages_menu_func)
    bpy.types.NODE_PT_active_node_generic.prepend(reset_nodes_button)
    bpy.types.NODE_MT_node.prepend(reset_nodes_button)


def unregister():
    # props
    del bpy.types.Scene.NWBusyDrawing
    del bpy.types.Scene.NWLazySource
    del bpy.types.Scene.NWLazyTarget
    del bpy.types.Scene.NWSourceSocket

    # keymaps
    for km, kmi in addon_keymaps:
        km.keymap_items.remove(kmi)
    addon_keymaps.clear()

    # menuitems
    bpy.types.NODE_MT_select.remove(select_parent_children_buttons)
    bpy.types.NODE_MT_category_SH_NEW_INPUT.remove(attr_nodes_menu_func)
    bpy.types.NODE_PT_category_SH_NEW_INPUT.remove(attr_nodes_menu_func)
    bpy.types.NODE_PT_backdrop.remove(bgreset_menu_func)
    bpy.types.NODE_PT_active_node_generic.remove(save_viewer_menu_func)
    bpy.types.NODE_MT_category_SH_NEW_TEXTURE.remove(multipleimages_menu_func)
    bpy.types.NODE_PT_category_SH_NEW_TEXTURE.remove(multipleimages_menu_func)
    bpy.types.NODE_MT_category_CMP_INPUT.remove(multipleimages_menu_func)
    bpy.types.NODE_PT_category_CMP_INPUT.remove(multipleimages_menu_func)
    bpy.types.NODE_PT_active_node_generic.remove(reset_nodes_button)
    bpy.types.NODE_MT_node.remove(reset_nodes_button)

    bpy.utils.unregister_module(__name__)

if __name__ == "__main__":
    register()
