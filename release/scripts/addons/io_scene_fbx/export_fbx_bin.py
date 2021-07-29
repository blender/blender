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

# Script copyright (C) Campbell Barton, Bastien Montagne


import array
import datetime
import math
import os
import time

from collections import OrderedDict
from itertools import zip_longest, chain

if "bpy" in locals():
    import importlib
    if "encode_bin" in locals():
        importlib.reload(encode_bin)
    if "data_types" in locals():
        importlib.reload(data_types)
    if "fbx_utils" in locals():
        importlib.reload(fbx_utils)

import bpy
import bpy_extras
from mathutils import Vector, Matrix

from . import encode_bin, data_types, fbx_utils
from .fbx_utils import (
    # Constants.
    FBX_VERSION, FBX_HEADER_VERSION, FBX_SCENEINFO_VERSION, FBX_TEMPLATES_VERSION,
    FBX_MODELS_VERSION,
    FBX_GEOMETRY_VERSION, FBX_GEOMETRY_NORMAL_VERSION, FBX_GEOMETRY_BINORMAL_VERSION, FBX_GEOMETRY_TANGENT_VERSION,
    FBX_GEOMETRY_SMOOTHING_VERSION, FBX_GEOMETRY_VCOLOR_VERSION, FBX_GEOMETRY_UV_VERSION,
    FBX_GEOMETRY_MATERIAL_VERSION, FBX_GEOMETRY_LAYER_VERSION,
    FBX_GEOMETRY_SHAPE_VERSION, FBX_DEFORMER_SHAPE_VERSION, FBX_DEFORMER_SHAPECHANNEL_VERSION,
    FBX_POSE_BIND_VERSION, FBX_DEFORMER_SKIN_VERSION, FBX_DEFORMER_CLUSTER_VERSION,
    FBX_MATERIAL_VERSION, FBX_TEXTURE_VERSION,
    FBX_ANIM_KEY_VERSION,
    FBX_ANIM_PROPSGROUP_NAME,
    FBX_KTIME,
    BLENDER_OTHER_OBJECT_TYPES, BLENDER_OBJECT_TYPES_MESHLIKE,
    FBX_LIGHT_TYPES, FBX_LIGHT_DECAY_TYPES,
    RIGHT_HAND_AXES, FBX_FRAMERATES,
    # Miscellaneous utils.
    PerfMon,
    units_blender_to_fbx_factor, units_convertor, units_convertor_iter,
    matrix4_to_array, similar_values, similar_values_iter,
    # Mesh transform helpers.
    vcos_transformed_gen, nors_transformed_gen,
    # UUID from key.
    get_fbx_uuid_from_key,
    # Key generators.
    get_blenderID_key, get_blenderID_name,
    get_blender_mesh_shape_key, get_blender_mesh_shape_channel_key,
    get_blender_empty_key, get_blender_bone_key,
    get_blender_bindpose_key, get_blender_armature_skin_key, get_blender_bone_cluster_key,
    get_blender_anim_id_base, get_blender_anim_stack_key, get_blender_anim_layer_key,
    get_blender_anim_curve_node_key, get_blender_anim_curve_key,
    # FBX element data.
    elem_empty,
    elem_data_single_bool, elem_data_single_int16, elem_data_single_int32, elem_data_single_int64,
    elem_data_single_float32, elem_data_single_float64,
    elem_data_single_bytes, elem_data_single_string, elem_data_single_string_unicode,
    elem_data_single_bool_array, elem_data_single_int32_array, elem_data_single_int64_array,
    elem_data_single_float32_array, elem_data_single_float64_array, elem_data_vec_float64,
    # FBX element properties.
    elem_properties, elem_props_set, elem_props_compound,
    # FBX element properties handling templates.
    elem_props_template_init, elem_props_template_set, elem_props_template_finalize,
    # Templates.
    FBXTemplate, fbx_templates_generate,
    # Animation.
    AnimationCurveNodeWrapper,
    # Objects.
    ObjectWrapper, fbx_name_class,
    # Top level.
    FBXExportSettingsMedia, FBXExportSettings, FBXExportData,
)

# Units convertors!
convert_sec_to_ktime = units_convertor("second", "ktime")
convert_sec_to_ktime_iter = units_convertor_iter("second", "ktime")

convert_mm_to_inch = units_convertor("millimeter", "inch")

convert_rad_to_deg = units_convertor("radian", "degree")
convert_rad_to_deg_iter = units_convertor_iter("radian", "degree")


# ##### Templates #####
# TODO: check all those "default" values, they should match Blender's default as much as possible, I guess?

def fbx_template_def_globalsettings(scene, settings, override_defaults=None, nbr_users=0):
    props = OrderedDict()
    if override_defaults is not None:
        props.update(override_defaults)
    return FBXTemplate(b"GlobalSettings", b"", props, nbr_users, [False])


def fbx_template_def_model(scene, settings, override_defaults=None, nbr_users=0):
    gscale = settings.global_scale
    props = OrderedDict((
        # Name,                   Value, Type, Animatable
        (b"QuaternionInterpolate", (0, "p_enum", False)),  # 0 = no quat interpolation.
        (b"RotationOffset", ((0.0, 0.0, 0.0), "p_vector_3d", False)),
        (b"RotationPivot", ((0.0, 0.0, 0.0), "p_vector_3d", False)),
        (b"ScalingOffset", ((0.0, 0.0, 0.0), "p_vector_3d", False)),
        (b"ScalingPivot", ((0.0, 0.0, 0.0), "p_vector_3d", False)),
        (b"TranslationActive", (False, "p_bool", False)),
        (b"TranslationMin", ((0.0, 0.0, 0.0), "p_vector_3d", False)),
        (b"TranslationMax", ((0.0, 0.0, 0.0), "p_vector_3d", False)),
        (b"TranslationMinX", (False, "p_bool", False)),
        (b"TranslationMinY", (False, "p_bool", False)),
        (b"TranslationMinZ", (False, "p_bool", False)),
        (b"TranslationMaxX", (False, "p_bool", False)),
        (b"TranslationMaxY", (False, "p_bool", False)),
        (b"TranslationMaxZ", (False, "p_bool", False)),
        (b"RotationOrder", (0, "p_enum", False)),  # we always use 'XYZ' order.
        (b"RotationSpaceForLimitOnly", (False, "p_bool", False)),
        (b"RotationStiffnessX", (0.0, "p_double", False)),
        (b"RotationStiffnessY", (0.0, "p_double", False)),
        (b"RotationStiffnessZ", (0.0, "p_double", False)),
        (b"AxisLen", (10.0, "p_double", False)),
        (b"PreRotation", ((0.0, 0.0, 0.0), "p_vector_3d", False)),
        (b"PostRotation", ((0.0, 0.0, 0.0), "p_vector_3d", False)),
        (b"RotationActive", (False, "p_bool", False)),
        (b"RotationMin", ((0.0, 0.0, 0.0), "p_vector_3d", False)),
        (b"RotationMax", ((0.0, 0.0, 0.0), "p_vector_3d", False)),
        (b"RotationMinX", (False, "p_bool", False)),
        (b"RotationMinY", (False, "p_bool", False)),
        (b"RotationMinZ", (False, "p_bool", False)),
        (b"RotationMaxX", (False, "p_bool", False)),
        (b"RotationMaxY", (False, "p_bool", False)),
        (b"RotationMaxZ", (False, "p_bool", False)),
        (b"InheritType", (0, "p_enum", False)),  # RrSs
        (b"ScalingActive", (False, "p_bool", False)),
        (b"ScalingMin", ((0.0, 0.0, 0.0), "p_vector_3d", False)),
        (b"ScalingMax", ((1.0, 1.0, 1.0), "p_vector_3d", False)),
        (b"ScalingMinX", (False, "p_bool", False)),
        (b"ScalingMinY", (False, "p_bool", False)),
        (b"ScalingMinZ", (False, "p_bool", False)),
        (b"ScalingMaxX", (False, "p_bool", False)),
        (b"ScalingMaxY", (False, "p_bool", False)),
        (b"ScalingMaxZ", (False, "p_bool", False)),
        (b"GeometricTranslation", ((0.0, 0.0, 0.0), "p_vector_3d", False)),
        (b"GeometricRotation", ((0.0, 0.0, 0.0), "p_vector_3d", False)),
        (b"GeometricScaling", ((1.0, 1.0, 1.0), "p_vector_3d", False)),
        (b"MinDampRangeX", (0.0, "p_double", False)),
        (b"MinDampRangeY", (0.0, "p_double", False)),
        (b"MinDampRangeZ", (0.0, "p_double", False)),
        (b"MaxDampRangeX", (0.0, "p_double", False)),
        (b"MaxDampRangeY", (0.0, "p_double", False)),
        (b"MaxDampRangeZ", (0.0, "p_double", False)),
        (b"MinDampStrengthX", (0.0, "p_double", False)),
        (b"MinDampStrengthY", (0.0, "p_double", False)),
        (b"MinDampStrengthZ", (0.0, "p_double", False)),
        (b"MaxDampStrengthX", (0.0, "p_double", False)),
        (b"MaxDampStrengthY", (0.0, "p_double", False)),
        (b"MaxDampStrengthZ", (0.0, "p_double", False)),
        (b"PreferedAngleX", (0.0, "p_double", False)),
        (b"PreferedAngleY", (0.0, "p_double", False)),
        (b"PreferedAngleZ", (0.0, "p_double", False)),
        (b"LookAtProperty", (None, "p_object", False)),
        (b"UpVectorProperty", (None, "p_object", False)),
        (b"Show", (True, "p_bool", False)),
        (b"NegativePercentShapeSupport", (True, "p_bool", False)),
        (b"DefaultAttributeIndex", (-1, "p_integer", False)),
        (b"Freeze", (False, "p_bool", False)),
        (b"LODBox", (False, "p_bool", False)),
        (b"Lcl Translation", ((0.0, 0.0, 0.0), "p_lcl_translation", True)),
        (b"Lcl Rotation", ((0.0, 0.0, 0.0), "p_lcl_rotation", True)),
        (b"Lcl Scaling", ((1.0, 1.0, 1.0), "p_lcl_scaling", True)),
        (b"Visibility", (1.0, "p_visibility", True)),
        (b"Visibility Inheritance", (1, "p_visibility_inheritance", False)),
    ))
    if override_defaults is not None:
        props.update(override_defaults)
    return FBXTemplate(b"Model", b"FbxNode", props, nbr_users, [False])


def fbx_template_def_null(scene, settings, override_defaults=None, nbr_users=0):
    props = OrderedDict((
        (b"Color", ((0.8, 0.8, 0.8), "p_color_rgb", False)),
        (b"Size", (100.0, "p_double", False)),
        (b"Look", (1, "p_enum", False)),  # Cross (0 is None, i.e. invisible?).
    ))
    if override_defaults is not None:
        props.update(override_defaults)
    return FBXTemplate(b"NodeAttribute", b"FbxNull", props, nbr_users, [False])


def fbx_template_def_light(scene, settings, override_defaults=None, nbr_users=0):
    gscale = settings.global_scale
    props = OrderedDict((
        (b"LightType", (0, "p_enum", False)),  # Point light.
        (b"CastLight", (True, "p_bool", False)),
        (b"Color", ((1.0, 1.0, 1.0), "p_color", True)),
        (b"Intensity", (100.0, "p_number", True)),  # Times 100 compared to Blender values...
        (b"DecayType", (2, "p_enum", False)),  # Quadratic.
        (b"DecayStart", (30.0 * gscale, "p_double", False)),
        (b"CastShadows", (True, "p_bool", False)),
        (b"ShadowColor", ((0.0, 0.0, 0.0), "p_color", True)),
        (b"AreaLightShape", (0, "p_enum", False)),  # Rectangle.
    ))
    if override_defaults is not None:
        props.update(override_defaults)
    return FBXTemplate(b"NodeAttribute", b"FbxLight", props, nbr_users, [False])


def fbx_template_def_camera(scene, settings, override_defaults=None, nbr_users=0):
    r = scene.render
    props = OrderedDict((
        (b"Color", ((0.8, 0.8, 0.8), "p_color_rgb", False)),
        (b"Position", ((0.0, 0.0, -50.0), "p_vector", True)),
        (b"UpVector", ((0.0, 1.0, 0.0), "p_vector", True)),
        (b"InterestPosition", ((0.0, 0.0, 0.0), "p_vector", True)),
        (b"Roll", (0.0, "p_roll", True)),
        (b"OpticalCenterX", (0.0, "p_opticalcenterx", True)),
        (b"OpticalCenterY", (0.0, "p_opticalcentery", True)),
        (b"BackgroundColor", ((0.63, 0.63, 0.63), "p_color", True)),
        (b"TurnTable", (0.0, "p_number", True)),
        (b"DisplayTurnTableIcon", (False, "p_bool", False)),
        (b"UseMotionBlur", (False, "p_bool", False)),
        (b"UseRealTimeMotionBlur", (True, "p_bool", False)),
        (b"Motion Blur Intensity", (1.0, "p_number", True)),
        (b"AspectRatioMode", (0, "p_enum", False)),  # WindowSize.
        (b"AspectWidth", (320.0, "p_double", False)),
        (b"AspectHeight", (200.0, "p_double", False)),
        (b"PixelAspectRatio", (1.0, "p_double", False)),
        (b"FilmOffsetX", (0.0, "p_number", True)),
        (b"FilmOffsetY", (0.0, "p_number", True)),
        (b"FilmWidth", (0.816, "p_double", False)),
        (b"FilmHeight", (0.612, "p_double", False)),
        (b"FilmAspectRatio", (1.3333333333333333, "p_double", False)),
        (b"FilmSqueezeRatio", (1.0, "p_double", False)),
        (b"FilmFormatIndex", (0, "p_enum", False)),  # Assuming this is ApertureFormat, 0 = custom.
        (b"PreScale", (1.0, "p_number", True)),
        (b"FilmTranslateX", (0.0, "p_number", True)),
        (b"FilmTranslateY", (0.0, "p_number", True)),
        (b"FilmRollPivotX", (0.0, "p_number", True)),
        (b"FilmRollPivotY", (0.0, "p_number", True)),
        (b"FilmRollValue", (0.0, "p_number", True)),
        (b"FilmRollOrder", (0, "p_enum", False)),  # 0 = rotate first (default).
        (b"ApertureMode", (2, "p_enum", False)),  # 2 = Vertical.
        (b"GateFit", (0, "p_enum", False)),  # 0 = no resolution gate fit.
        (b"FieldOfView", (25.114999771118164, "p_fov", True)),
        (b"FieldOfViewX", (40.0, "p_fov_x", True)),
        (b"FieldOfViewY", (40.0, "p_fov_y", True)),
        (b"FocalLength", (34.89327621672628, "p_number", True)),
        (b"CameraFormat", (0, "p_enum", False)),  # Custom camera format.
        (b"UseFrameColor", (False, "p_bool", False)),
        (b"FrameColor", ((0.3, 0.3, 0.3), "p_color_rgb", False)),
        (b"ShowName", (True, "p_bool", False)),
        (b"ShowInfoOnMoving", (True, "p_bool", False)),
        (b"ShowGrid", (True, "p_bool", False)),
        (b"ShowOpticalCenter", (False, "p_bool", False)),
        (b"ShowAzimut", (True, "p_bool", False)),
        (b"ShowTimeCode", (False, "p_bool", False)),
        (b"ShowAudio", (False, "p_bool", False)),
        (b"AudioColor", ((0.0, 1.0, 0.0), "p_vector_3d", False)),  # Yep, vector3d, not corlorgb… :cry:
        (b"NearPlane", (10.0, "p_double", False)),
        (b"FarPlane", (4000.0, "p_double", False)),
        (b"AutoComputeClipPanes", (False, "p_bool", False)),
        (b"ViewCameraToLookAt", (True, "p_bool", False)),
        (b"ViewFrustumNearFarPlane", (False, "p_bool", False)),
        (b"ViewFrustumBackPlaneMode", (2, "p_enum", False)),  # 2 = show back plane if texture added.
        (b"BackPlaneDistance", (4000.0, "p_number", True)),
        (b"BackPlaneDistanceMode", (1, "p_enum", False)),  # 1 = relative to camera.
        (b"ViewFrustumFrontPlaneMode", (2, "p_enum", False)),  # 2 = show front plane if texture added.
        (b"FrontPlaneDistance", (10.0, "p_number", True)),
        (b"FrontPlaneDistanceMode", (1, "p_enum", False)),  # 1 = relative to camera.
        (b"LockMode", (False, "p_bool", False)),
        (b"LockInterestNavigation", (False, "p_bool", False)),
        # BackPlate... properties **arggggg!**
        (b"FitImage", (False, "p_bool", False)),
        (b"Crop", (False, "p_bool", False)),
        (b"Center", (True, "p_bool", False)),
        (b"KeepRatio", (True, "p_bool", False)),
        # End of BackPlate...
        (b"BackgroundAlphaTreshold", (0.5, "p_double", False)),
        (b"ShowBackplate", (True, "p_bool", False)),
        (b"BackPlaneOffsetX", (0.0, "p_number", True)),
        (b"BackPlaneOffsetY", (0.0, "p_number", True)),
        (b"BackPlaneRotation", (0.0, "p_number", True)),
        (b"BackPlaneScaleX", (1.0, "p_number", True)),
        (b"BackPlaneScaleY", (1.0, "p_number", True)),
        (b"Background Texture", (None, "p_object", False)),
        (b"FrontPlateFitImage", (True, "p_bool", False)),
        (b"FrontPlateCrop", (False, "p_bool", False)),
        (b"FrontPlateCenter", (True, "p_bool", False)),
        (b"FrontPlateKeepRatio", (True, "p_bool", False)),
        (b"Foreground Opacity", (1.0, "p_double", False)),
        (b"ShowFrontplate", (True, "p_bool", False)),
        (b"FrontPlaneOffsetX", (0.0, "p_number", True)),
        (b"FrontPlaneOffsetY", (0.0, "p_number", True)),
        (b"FrontPlaneRotation", (0.0, "p_number", True)),
        (b"FrontPlaneScaleX", (1.0, "p_number", True)),
        (b"FrontPlaneScaleY", (1.0, "p_number", True)),
        (b"Foreground Texture", (None, "p_object", False)),
        (b"DisplaySafeArea", (False, "p_bool", False)),
        (b"DisplaySafeAreaOnRender", (False, "p_bool", False)),
        (b"SafeAreaDisplayStyle", (1, "p_enum", False)),  # 1 = rounded corners.
        (b"SafeAreaAspectRatio", (1.3333333333333333, "p_double", False)),
        (b"Use2DMagnifierZoom", (False, "p_bool", False)),
        (b"2D Magnifier Zoom", (100.0, "p_number", True)),
        (b"2D Magnifier X", (50.0, "p_number", True)),
        (b"2D Magnifier Y", (50.0, "p_number", True)),
        (b"CameraProjectionType", (0, "p_enum", False)),  # 0 = perspective, 1 = orthogonal.
        (b"OrthoZoom", (1.0, "p_double", False)),
        (b"UseRealTimeDOFAndAA", (False, "p_bool", False)),
        (b"UseDepthOfField", (False, "p_bool", False)),
        (b"FocusSource", (0, "p_enum", False)),  # 0 = camera interest, 1 = distance from camera interest.
        (b"FocusAngle", (3.5, "p_double", False)),  # ???
        (b"FocusDistance", (200.0, "p_double", False)),
        (b"UseAntialiasing", (False, "p_bool", False)),
        (b"AntialiasingIntensity", (0.77777, "p_double", False)),
        (b"AntialiasingMethod", (0, "p_enum", False)),  # 0 = oversampling, 1 = hardware.
        (b"UseAccumulationBuffer", (False, "p_bool", False)),
        (b"FrameSamplingCount", (7, "p_integer", False)),
        (b"FrameSamplingType", (1, "p_enum", False)),  # 0 = uniform, 1 = stochastic.
    ))
    if override_defaults is not None:
        props.update(override_defaults)
    return FBXTemplate(b"NodeAttribute", b"FbxCamera", props, nbr_users, [False])


def fbx_template_def_bone(scene, settings, override_defaults=None, nbr_users=0):
    props = OrderedDict()
    if override_defaults is not None:
        props.update(override_defaults)
    return FBXTemplate(b"NodeAttribute", b"LimbNode", props, nbr_users, [False])


def fbx_template_def_geometry(scene, settings, override_defaults=None, nbr_users=0):
    props = OrderedDict((
        (b"Color", ((0.8, 0.8, 0.8), "p_color_rgb", False)),
        (b"BBoxMin", ((0.0, 0.0, 0.0), "p_vector_3d", False)),
        (b"BBoxMax", ((0.0, 0.0, 0.0), "p_vector_3d", False)),
        (b"Primary Visibility", (True, "p_bool", False)),
        (b"Casts Shadows", (True, "p_bool", False)),
        (b"Receive Shadows", (True, "p_bool", False)),
    ))
    if override_defaults is not None:
        props.update(override_defaults)
    return FBXTemplate(b"Geometry", b"FbxMesh", props, nbr_users, [False])


def fbx_template_def_material(scene, settings, override_defaults=None, nbr_users=0):
    # WIP...
    props = OrderedDict((
        (b"ShadingModel", ("Phong", "p_string", False)),
        (b"MultiLayer", (False, "p_bool", False)),
        # Lambert-specific.
        (b"EmissiveColor", ((0.0, 0.0, 0.0), "p_color", True)),
        (b"EmissiveFactor", (1.0, "p_number", True)),
        (b"AmbientColor", ((0.2, 0.2, 0.2), "p_color", True)),
        (b"AmbientFactor", (1.0, "p_number", True)),
        (b"DiffuseColor", ((0.8, 0.8, 0.8), "p_color", True)),
        (b"DiffuseFactor", (1.0, "p_number", True)),
        (b"TransparentColor", ((0.0, 0.0, 0.0), "p_color", True)),
        (b"TransparencyFactor", (0.0, "p_number", True)),
        (b"Opacity", (1.0, "p_number", True)),
        (b"NormalMap", ((0.0, 0.0, 0.0), "p_vector_3d", False)),
        (b"Bump", ((0.0, 0.0, 0.0), "p_vector_3d", False)),
        (b"BumpFactor", (1.0, "p_double", False)),
        (b"DisplacementColor", ((0.0, 0.0, 0.0), "p_color_rgb", False)),
        (b"DisplacementFactor", (1.0, "p_double", False)),
        (b"VectorDisplacementColor", ((0.0, 0.0, 0.0), "p_color_rgb", False)),
        (b"VectorDisplacementFactor", (1.0, "p_double", False)),
        # Phong-specific.
        (b"SpecularColor", ((0.2, 0.2, 0.2), "p_color", True)),
        (b"SpecularFactor", (1.0, "p_number", True)),
        # Not sure about the name, importer uses this (but ShininessExponent for tex prop name!)
        # And in fbx exported by sdk, you have one in template, the other in actual material!!! :/
        # For now, using both.
        (b"Shininess", (20.0, "p_number", True)),
        (b"ShininessExponent", (20.0, "p_number", True)),
        (b"ReflectionColor", ((0.0, 0.0, 0.0), "p_color", True)),
        (b"ReflectionFactor", (1.0, "p_number", True)),
    ))
    if override_defaults is not None:
        props.update(override_defaults)
    return FBXTemplate(b"Material", b"FbxSurfacePhong", props, nbr_users, [False])


def fbx_template_def_texture_file(scene, settings, override_defaults=None, nbr_users=0):
    # WIP...
    # XXX Not sure about all names!
    props = OrderedDict((
        (b"TextureTypeUse", (0, "p_enum", False)),  # Standard.
        (b"AlphaSource", (2, "p_enum", False)),  # Black (i.e. texture's alpha), XXX name guessed!.
        (b"Texture alpha", (1.0, "p_double", False)),
        (b"PremultiplyAlpha", (True, "p_bool", False)),
        (b"CurrentTextureBlendMode", (1, "p_enum", False)),  # Additive...
        (b"CurrentMappingType", (0, "p_enum", False)),  # UV.
        (b"UVSet", ("default", "p_string", False)),  # UVMap name.
        (b"WrapModeU", (0, "p_enum", False)),  # Repeat.
        (b"WrapModeV", (0, "p_enum", False)),  # Repeat.
        (b"UVSwap", (False, "p_bool", False)),
        (b"Translation", ((0.0, 0.0, 0.0), "p_vector_3d", False)),
        (b"Rotation", ((0.0, 0.0, 0.0), "p_vector_3d", False)),
        (b"Scaling", ((1.0, 1.0, 1.0), "p_vector_3d", False)),
        (b"TextureRotationPivot", ((0.0, 0.0, 0.0), "p_vector_3d", False)),
        (b"TextureScalingPivot", ((0.0, 0.0, 0.0), "p_vector_3d", False)),
        # Not sure about those two...
        (b"UseMaterial", (False, "p_bool", False)),
        (b"UseMipMap", (False, "p_bool", False)),
    ))
    if override_defaults is not None:
        props.update(override_defaults)
    return FBXTemplate(b"Texture", b"FbxFileTexture", props, nbr_users, [False])


def fbx_template_def_video(scene, settings, override_defaults=None, nbr_users=0):
    # WIP...
    props = OrderedDict((
        # All pictures.
        (b"Width", (0, "p_integer", False)),
        (b"Height", (0, "p_integer", False)),
        (b"Path", ("", "p_string_url", False)),
        (b"AccessMode", (0, "p_enum", False)),  # Disk (0=Disk, 1=Mem, 2=DiskAsync).
        # All videos.
        (b"StartFrame", (0, "p_integer", False)),
        (b"StopFrame", (0, "p_integer", False)),
        (b"Offset", (0, "p_timestamp", False)),
        (b"PlaySpeed", (0.0, "p_double", False)),
        (b"FreeRunning", (False, "p_bool", False)),
        (b"Loop", (False, "p_bool", False)),
        (b"InterlaceMode", (0, "p_enum", False)),  # None, i.e. progressive.
        # Image sequences.
        (b"ImageSequence", (False, "p_bool", False)),
        (b"ImageSequenceOffset", (0, "p_integer", False)),
        (b"FrameRate", (0.0, "p_double", False)),
        (b"LastFrame", (0, "p_integer", False)),
    ))
    if override_defaults is not None:
        props.update(override_defaults)
    return FBXTemplate(b"Video", b"FbxVideo", props, nbr_users, [False])


def fbx_template_def_pose(scene, settings, override_defaults=None, nbr_users=0):
    props = OrderedDict()
    if override_defaults is not None:
        props.update(override_defaults)
    return FBXTemplate(b"Pose", b"", props, nbr_users, [False])


def fbx_template_def_deformer(scene, settings, override_defaults=None, nbr_users=0):
    props = OrderedDict()
    if override_defaults is not None:
        props.update(override_defaults)
    return FBXTemplate(b"Deformer", b"", props, nbr_users, [False])


def fbx_template_def_animstack(scene, settings, override_defaults=None, nbr_users=0):
    props = OrderedDict((
        (b"Description", ("", "p_string", False)),
        (b"LocalStart", (0, "p_timestamp", False)),
        (b"LocalStop", (0, "p_timestamp", False)),
        (b"ReferenceStart", (0, "p_timestamp", False)),
        (b"ReferenceStop", (0, "p_timestamp", False)),
    ))
    if override_defaults is not None:
        props.update(override_defaults)
    return FBXTemplate(b"AnimationStack", b"FbxAnimStack", props, nbr_users, [False])


def fbx_template_def_animlayer(scene, settings, override_defaults=None, nbr_users=0):
    props = OrderedDict((
        (b"Weight", (100.0, "p_number", True)),
        (b"Mute", (False, "p_bool", False)),
        (b"Solo", (False, "p_bool", False)),
        (b"Lock", (False, "p_bool", False)),
        (b"Color", ((0.8, 0.8, 0.8), "p_color_rgb", False)),
        (b"BlendMode", (0, "p_enum", False)),
        (b"RotationAccumulationMode", (0, "p_enum", False)),
        (b"ScaleAccumulationMode", (0, "p_enum", False)),
        (b"BlendModeBypass", (0, "p_ulonglong", False)),
    ))
    if override_defaults is not None:
        props.update(override_defaults)
    return FBXTemplate(b"AnimationLayer", b"FbxAnimLayer", props, nbr_users, [False])


def fbx_template_def_animcurvenode(scene, settings, override_defaults=None, nbr_users=0):
    props = OrderedDict((
        (FBX_ANIM_PROPSGROUP_NAME.encode(), (None, "p_compound", False)),
    ))
    if override_defaults is not None:
        props.update(override_defaults)
    return FBXTemplate(b"AnimationCurveNode", b"FbxAnimCurveNode", props, nbr_users, [False])


def fbx_template_def_animcurve(scene, settings, override_defaults=None, nbr_users=0):
    props = OrderedDict()
    if override_defaults is not None:
        props.update(override_defaults)
    return FBXTemplate(b"AnimationCurve", b"", props, nbr_users, [False])


# ##### Generators for connection elements. #####

def elem_connection(elem, c_type, uid_src, uid_dst, prop_dst=None):
    e = elem_data_single_string(elem, b"C", c_type)
    e.add_int64(uid_src)
    e.add_int64(uid_dst)
    if prop_dst is not None:
        e.add_string(prop_dst)


# ##### FBX objects generators. #####

def fbx_data_element_custom_properties(props, bid):
    """
    Store custom properties of blender ID bid (any mapping-like object, in fact) into FBX properties props.
    """
    for k, v in bid.items():
        list_val = getattr(v, "to_list", lambda: None)()

        if isinstance(v, str):
            elem_props_set(props, "p_string", k.encode(), v, custom=True)
        elif isinstance(v, int):
            elem_props_set(props, "p_integer", k.encode(), v, custom=True)
        elif isinstance(v, float):
            elem_props_set(props, "p_double", k.encode(), v, custom=True)
        elif list_val:
            if len(list_val) == 3:
                elem_props_set(props, "p_vector", k.encode(), list_val, custom=True)
            else:
                elem_props_set(props, "p_string", k.encode(), str(list_val), custom=True)
        else:
            elem_props_set(props, "p_string", k.encode(), str(v), custom=True)


def fbx_data_empty_elements(root, empty, scene_data):
    """
    Write the Empty data block (you can control its FBX datatype with the 'fbx_type' string custom property).
    """
    empty_key = scene_data.data_empties[empty]

    null = elem_data_single_int64(root, b"NodeAttribute", get_fbx_uuid_from_key(empty_key))
    null.add_string(fbx_name_class(empty.name.encode(), b"NodeAttribute"))
    val = empty.bdata.get('fbx_type', None)
    null.add_string(val.encode() if val and isinstance(val, str) else b"Null")

    elem_data_single_string(null, b"TypeFlags", b"Null")

    tmpl = elem_props_template_init(scene_data.templates, b"Null")
    props = elem_properties(null)
    elem_props_template_finalize(tmpl, props)

    # No custom properties, already saved with object (Model).


def fbx_data_lamp_elements(root, lamp, scene_data):
    """
    Write the Lamp data block.
    """
    gscale = scene_data.settings.global_scale

    lamp_key = scene_data.data_lamps[lamp]
    do_light = True
    decay_type = FBX_LIGHT_DECAY_TYPES['CONSTANT']
    do_shadow = False
    shadow_color = Vector((0.0, 0.0, 0.0))
    if lamp.type not in {'HEMI'}:
        if lamp.type not in {'SUN', 'AREA'}:
            decay_type = FBX_LIGHT_DECAY_TYPES[lamp.falloff_type]
        do_light = (not lamp.use_only_shadow) and (lamp.use_specular or lamp.use_diffuse)
        do_shadow = lamp.shadow_method not in {'NOSHADOW'}
        shadow_color = lamp.shadow_color

    light = elem_data_single_int64(root, b"NodeAttribute", get_fbx_uuid_from_key(lamp_key))
    light.add_string(fbx_name_class(lamp.name.encode(), b"NodeAttribute"))
    light.add_string(b"Light")

    elem_data_single_int32(light, b"GeometryVersion", FBX_GEOMETRY_VERSION)  # Sic...

    tmpl = elem_props_template_init(scene_data.templates, b"Light")
    props = elem_properties(light)
    elem_props_template_set(tmpl, props, "p_enum", b"LightType", FBX_LIGHT_TYPES[lamp.type])
    elem_props_template_set(tmpl, props, "p_bool", b"CastLight", do_light)
    elem_props_template_set(tmpl, props, "p_color", b"Color", lamp.color)
    elem_props_template_set(tmpl, props, "p_number", b"Intensity", lamp.energy * 100.0)
    elem_props_template_set(tmpl, props, "p_enum", b"DecayType", decay_type)
    elem_props_template_set(tmpl, props, "p_double", b"DecayStart", lamp.distance * gscale)
    elem_props_template_set(tmpl, props, "p_bool", b"CastShadows", do_shadow)
    elem_props_template_set(tmpl, props, "p_color", b"ShadowColor", shadow_color)
    if lamp.type in {'SPOT'}:
        elem_props_template_set(tmpl, props, "p_double", b"OuterAngle", math.degrees(lamp.spot_size))
        elem_props_template_set(tmpl, props, "p_double", b"InnerAngle",
                                math.degrees(lamp.spot_size * (1.0 - lamp.spot_blend)))
    elem_props_template_finalize(tmpl, props)

    # Custom properties.
    if scene_data.settings.use_custom_props:
        fbx_data_element_custom_properties(props, lamp)


def fbx_data_camera_elements(root, cam_obj, scene_data):
    """
    Write the Camera data blocks.
    """
    gscale = scene_data.settings.global_scale

    cam = cam_obj.bdata
    cam_data = cam.data
    cam_key = scene_data.data_cameras[cam_obj]

    # Real data now, good old camera!
    # Object transform info.
    loc, rot, scale, matrix, matrix_rot = cam_obj.fbx_object_tx(scene_data)
    up = matrix_rot * Vector((0.0, 1.0, 0.0))
    to = matrix_rot * Vector((0.0, 0.0, -1.0))
    # Render settings.
    # TODO We could export much more...
    render = scene_data.scene.render
    width = render.resolution_x
    height = render.resolution_y
    aspect = width / height
    # Film width & height from mm to inches
    filmwidth = convert_mm_to_inch(cam_data.sensor_width)
    filmheight = convert_mm_to_inch(cam_data.sensor_height)
    filmaspect = filmwidth / filmheight
    # Film offset
    offsetx = filmwidth * cam_data.shift_x
    offsety = filmaspect * filmheight * cam_data.shift_y

    cam = elem_data_single_int64(root, b"NodeAttribute", get_fbx_uuid_from_key(cam_key))
    cam.add_string(fbx_name_class(cam_data.name.encode(), b"NodeAttribute"))
    cam.add_string(b"Camera")

    tmpl = elem_props_template_init(scene_data.templates, b"Camera")
    props = elem_properties(cam)

    elem_props_template_set(tmpl, props, "p_vector", b"Position", loc)
    elem_props_template_set(tmpl, props, "p_vector", b"UpVector", up)
    elem_props_template_set(tmpl, props, "p_vector", b"InterestPosition", loc + to)  # Point, not vector!
    # Should we use world value?
    elem_props_template_set(tmpl, props, "p_color", b"BackgroundColor", (0.0, 0.0, 0.0))
    elem_props_template_set(tmpl, props, "p_bool", b"DisplayTurnTableIcon", True)

    elem_props_template_set(tmpl, props, "p_enum", b"AspectRatioMode", 2)  # FixedResolution
    elem_props_template_set(tmpl, props, "p_double", b"AspectWidth", float(render.resolution_x))
    elem_props_template_set(tmpl, props, "p_double", b"AspectHeight", float(render.resolution_y))
    elem_props_template_set(tmpl, props, "p_double", b"PixelAspectRatio",
                            float(render.pixel_aspect_x / render.pixel_aspect_y))

    elem_props_template_set(tmpl, props, "p_double", b"FilmWidth", filmwidth)
    elem_props_template_set(tmpl, props, "p_double", b"FilmHeight", filmheight)
    elem_props_template_set(tmpl, props, "p_double", b"FilmAspectRatio", filmaspect)
    elem_props_template_set(tmpl, props, "p_double", b"FilmOffsetX", offsetx)
    elem_props_template_set(tmpl, props, "p_double", b"FilmOffsetY", offsety)

    elem_props_template_set(tmpl, props, "p_enum", b"ApertureMode", 3)  # FocalLength.
    elem_props_template_set(tmpl, props, "p_enum", b"GateFit", 2)  # FitHorizontal.
    elem_props_template_set(tmpl, props, "p_fov", b"FieldOfView", math.degrees(cam_data.angle_x))
    elem_props_template_set(tmpl, props, "p_fov_x", b"FieldOfViewX", math.degrees(cam_data.angle_x))
    elem_props_template_set(tmpl, props, "p_fov_y", b"FieldOfViewY", math.degrees(cam_data.angle_y))
    # No need to convert to inches here...
    elem_props_template_set(tmpl, props, "p_double", b"FocalLength", cam_data.lens)
    elem_props_template_set(tmpl, props, "p_double", b"SafeAreaAspectRatio", aspect)
    # Default to perspective camera.
    elem_props_template_set(tmpl, props, "p_enum", b"CameraProjectionType", 1 if cam_data.type == 'ORTHO' else 0)
    elem_props_template_set(tmpl, props, "p_double", b"OrthoZoom", cam_data.ortho_scale)

    elem_props_template_set(tmpl, props, "p_double", b"NearPlane", cam_data.clip_start * gscale)
    elem_props_template_set(tmpl, props, "p_double", b"FarPlane", cam_data.clip_end * gscale)
    elem_props_template_set(tmpl, props, "p_enum", b"BackPlaneDistanceMode", 1)  # RelativeToCamera.
    elem_props_template_set(tmpl, props, "p_double", b"BackPlaneDistance", cam_data.clip_end * gscale)

    elem_props_template_finalize(tmpl, props)

    # Custom properties.
    if scene_data.settings.use_custom_props:
        fbx_data_element_custom_properties(props, cam_data)

    elem_data_single_string(cam, b"TypeFlags", b"Camera")
    elem_data_single_int32(cam, b"GeometryVersion", 124)  # Sic...
    elem_data_vec_float64(cam, b"Position", loc)
    elem_data_vec_float64(cam, b"Up", up)
    elem_data_vec_float64(cam, b"LookAt", to)
    elem_data_single_int32(cam, b"ShowInfoOnMoving", 1)
    elem_data_single_int32(cam, b"ShowAudio", 0)
    elem_data_vec_float64(cam, b"AudioColor", (0.0, 1.0, 0.0))
    elem_data_single_float64(cam, b"CameraOrthoZoom", 1.0)


def fbx_data_bindpose_element(root, me_obj, me, scene_data, arm_obj=None, mat_world_arm=None, bones=[]):
    """
    Helper, since bindpose are used by both meshes shape keys and armature bones...
    """
    if arm_obj is None:
        arm_obj = me_obj
    # We assume bind pose for our bones are their "Editmode" pose...
    # All matrices are expected in global (world) space.
    bindpose_key = get_blender_bindpose_key(arm_obj.bdata, me)
    fbx_pose = elem_data_single_int64(root, b"Pose", get_fbx_uuid_from_key(bindpose_key))
    fbx_pose.add_string(fbx_name_class(me.name.encode(), b"Pose"))
    fbx_pose.add_string(b"BindPose")

    elem_data_single_string(fbx_pose, b"Type", b"BindPose")
    elem_data_single_int32(fbx_pose, b"Version", FBX_POSE_BIND_VERSION)
    elem_data_single_int32(fbx_pose, b"NbPoseNodes", 1 + (1 if (arm_obj != me_obj) else 0) + len(bones))

    # First node is mesh/object.
    mat_world_obj = me_obj.fbx_object_matrix(scene_data, global_space=True)
    fbx_posenode = elem_empty(fbx_pose, b"PoseNode")
    elem_data_single_int64(fbx_posenode, b"Node", me_obj.fbx_uuid)
    elem_data_single_float64_array(fbx_posenode, b"Matrix", matrix4_to_array(mat_world_obj))
    # Second node is armature object itself.
    if arm_obj != me_obj:
        fbx_posenode = elem_empty(fbx_pose, b"PoseNode")
        elem_data_single_int64(fbx_posenode, b"Node", arm_obj.fbx_uuid)
        elem_data_single_float64_array(fbx_posenode, b"Matrix", matrix4_to_array(mat_world_arm))
    # And all bones of armature!
    mat_world_bones = {}
    for bo_obj in bones:
        bomat = bo_obj.fbx_object_matrix(scene_data, rest=True, global_space=True)
        mat_world_bones[bo_obj] = bomat
        fbx_posenode = elem_empty(fbx_pose, b"PoseNode")
        elem_data_single_int64(fbx_posenode, b"Node", bo_obj.fbx_uuid)
        elem_data_single_float64_array(fbx_posenode, b"Matrix", matrix4_to_array(bomat))

    return mat_world_obj, mat_world_bones


def fbx_data_mesh_shapes_elements(root, me_obj, me, scene_data, fbx_me_tmpl, fbx_me_props):
    """
    Write shape keys related data.
    """
    if me not in scene_data.data_deformers_shape:
        return

    write_normals = True  # scene_data.settings.mesh_smooth_type in {'OFF'}

    # First, write the geometry data itself (i.e. shapes).
    _me_key, shape_key, shapes = scene_data.data_deformers_shape[me]

    channels = []

    for shape, (channel_key, geom_key, shape_verts_co, shape_verts_idx) in shapes.items():
        # Use vgroups as weights, if defined.
        if shape.vertex_group and shape.vertex_group in me_obj.bdata.vertex_groups:
            shape_verts_weights = [0.0] * (len(shape_verts_co) // 3)
            vg_idx = me_obj.bdata.vertex_groups[shape.vertex_group].index
            for sk_idx, v_idx in enumerate(shape_verts_idx):
                for vg in me.vertices[v_idx].groups:
                    if vg.group == vg_idx:
                        shape_verts_weights[sk_idx] = vg.weight * 100.0
        else:
            shape_verts_weights = [100.0] * (len(shape_verts_co) // 3)
        channels.append((channel_key, shape, shape_verts_weights))

        geom = elem_data_single_int64(root, b"Geometry", get_fbx_uuid_from_key(geom_key))
        geom.add_string(fbx_name_class(shape.name.encode(), b"Geometry"))
        geom.add_string(b"Shape")

        tmpl = elem_props_template_init(scene_data.templates, b"Geometry")
        props = elem_properties(geom)
        elem_props_template_finalize(tmpl, props)

        elem_data_single_int32(geom, b"Version", FBX_GEOMETRY_SHAPE_VERSION)

        elem_data_single_int32_array(geom, b"Indexes", shape_verts_idx)
        elem_data_single_float64_array(geom, b"Vertices", shape_verts_co)
        if write_normals:
            elem_data_single_float64_array(geom, b"Normals", [0.0] * len(shape_verts_co))

    # Yiha! BindPose for shapekeys too! Dodecasigh...
    # XXX Not sure yet whether several bindposes on same mesh are allowed, or not... :/
    fbx_data_bindpose_element(root, me_obj, me, scene_data)

    # ...and now, the deformers stuff.
    fbx_shape = elem_data_single_int64(root, b"Deformer", get_fbx_uuid_from_key(shape_key))
    fbx_shape.add_string(fbx_name_class(me.name.encode(), b"Deformer"))
    fbx_shape.add_string(b"BlendShape")

    elem_data_single_int32(fbx_shape, b"Version", FBX_DEFORMER_SHAPE_VERSION)

    for channel_key, shape, shape_verts_weights in channels:
        fbx_channel = elem_data_single_int64(root, b"Deformer", get_fbx_uuid_from_key(channel_key))
        fbx_channel.add_string(fbx_name_class(shape.name.encode(), b"SubDeformer"))
        fbx_channel.add_string(b"BlendShapeChannel")

        elem_data_single_int32(fbx_channel, b"Version", FBX_DEFORMER_SHAPECHANNEL_VERSION)
        elem_data_single_float64(fbx_channel, b"DeformPercent", shape.value * 100.0)  # Percents...
        elem_data_single_float64_array(fbx_channel, b"FullWeights", shape_verts_weights)

        # *WHY* add this in linked mesh properties too? *cry*
        # No idea whether it’s percent here too, or more usual factor (assume percentage for now) :/
        elem_props_template_set(fbx_me_tmpl, fbx_me_props, "p_number", shape.name.encode(), shape.value * 100.0,
                                animatable=True)


def fbx_data_mesh_elements(root, me_obj, scene_data, done_meshes):
    """
    Write the Mesh (Geometry) data block.
    """
    # Ugly helper... :/
    def _infinite_gen(val):
        while 1:
            yield val

    me_key, me, _free = scene_data.data_meshes[me_obj]

    # In case of multiple instances of same mesh, only write it once!
    if me_key in done_meshes:
        return

    # No gscale/gmat here, all data are supposed to be in object space.
    smooth_type = scene_data.settings.mesh_smooth_type
    write_normals = True  # smooth_type in {'OFF'}

    do_bake_space_transform = me_obj.use_bake_space_transform(scene_data)

    # Vertices are in object space, but we are post-multiplying all transforms with the inverse of the
    # global matrix, so we need to apply the global matrix to the vertices to get the correct result.
    geom_mat_co = scene_data.settings.global_matrix if do_bake_space_transform else None
    # We need to apply the inverse transpose of the global matrix when transforming normals.
    geom_mat_no = Matrix(scene_data.settings.global_matrix_inv_transposed) if do_bake_space_transform else None
    if geom_mat_no is not None:
        # Remove translation & scaling!
        geom_mat_no.translation = Vector()
        geom_mat_no.normalize()

    geom = elem_data_single_int64(root, b"Geometry", get_fbx_uuid_from_key(me_key))
    geom.add_string(fbx_name_class(me.name.encode(), b"Geometry"))
    geom.add_string(b"Mesh")

    tmpl = elem_props_template_init(scene_data.templates, b"Geometry")
    props = elem_properties(geom)

    # Custom properties.
    if scene_data.settings.use_custom_props:
        fbx_data_element_custom_properties(props, me)

    elem_data_single_int32(geom, b"GeometryVersion", FBX_GEOMETRY_VERSION)

    # Vertex cos.
    t_co = array.array(data_types.ARRAY_FLOAT64, (0.0,)) * len(me.vertices) * 3
    me.vertices.foreach_get("co", t_co)
    elem_data_single_float64_array(geom, b"Vertices", chain(*vcos_transformed_gen(t_co, geom_mat_co)))
    del t_co

    # Polygon indices.
    #
    # We do loose edges as two-vertices faces, if enabled...
    #
    # Note we have to process Edges in the same time, as they are based on poly's loops...
    loop_nbr = len(me.loops)
    t_pvi = array.array(data_types.ARRAY_INT32, (0,)) * loop_nbr
    t_ls = [None] * len(me.polygons)

    me.loops.foreach_get("vertex_index", t_pvi)
    me.polygons.foreach_get("loop_start", t_ls)

    # Add "fake" faces for loose edges.
    if scene_data.settings.use_mesh_edges:
        t_le = tuple(e.vertices for e in me.edges if e.is_loose)
        t_pvi.extend(chain(*t_le))
        t_ls.extend(range(loop_nbr, loop_nbr + len(t_le), 2))
        del t_le

    # Edges...
    # Note: Edges are represented as a loop here: each edge uses a single index, which refers to the polygon array.
    #       The edge is made by the vertex indexed py this polygon's point and the next one on the same polygon.
    #       Advantage: Only one index per edge.
    #       Drawback: Only polygon's edges can be represented (that's why we have to add fake two-verts polygons
    #                 for loose edges).
    #       We also have to store a mapping from real edges to their indices in this array, for edge-mapped data
    #       (like e.g. crease).
    t_eli = array.array(data_types.ARRAY_INT32)
    edges_map = {}
    edges_nbr = 0
    if t_ls and t_pvi:
        t_ls = set(t_ls)
        todo_edges = [None] * len(me.edges) * 2
        # Sigh, cannot access edge.key through foreach_get... :/
        me.edges.foreach_get("vertices", todo_edges)
        todo_edges = set((v1, v2) if v1 < v2 else (v2, v1) for v1, v2 in zip(*(iter(todo_edges),) * 2))

        li = 0
        vi = vi_start = t_pvi[0]
        for li_next, vi_next in enumerate(t_pvi[1:] + t_pvi[:1], start=1):
            if li_next in t_ls:  # End of a poly's loop.
                vi2 = vi_start
                vi_start = vi_next
            else:
                vi2 = vi_next

            e_key = (vi, vi2) if vi < vi2 else (vi2, vi)
            if e_key in todo_edges:
                t_eli.append(li)
                todo_edges.remove(e_key)
                edges_map[e_key] = edges_nbr
                edges_nbr += 1

            vi = vi_next
            li = li_next
    # End of edges!

    # We have to ^-1 last index of each loop.
    for ls in t_ls:
        t_pvi[ls - 1] ^= -1

    # And finally we can write data!
    elem_data_single_int32_array(geom, b"PolygonVertexIndex", t_pvi)
    elem_data_single_int32_array(geom, b"Edges", t_eli)
    del t_pvi
    del t_ls
    del t_eli

    # And now, layers!

    # Smoothing.
    if smooth_type in {'FACE', 'EDGE'}:
        t_ps = None
        _map = b""
        if smooth_type == 'FACE':
            t_ps = array.array(data_types.ARRAY_INT32, (0,)) * len(me.polygons)
            me.polygons.foreach_get("use_smooth", t_ps)
            _map = b"ByPolygon"
        else:  # EDGE
            # Write Edge Smoothing.
            # Note edge is sharp also if it's used by more than two faces, or one of its faces is flat.
            t_ps = array.array(data_types.ARRAY_INT32, (0,)) * edges_nbr
            sharp_edges = set()
            temp_sharp_edges = {}
            for p in me.polygons:
                if not p.use_smooth:
                    sharp_edges.update(p.edge_keys)
                    continue
                for k in p.edge_keys:
                    if temp_sharp_edges.setdefault(k, 0) > 1:
                        sharp_edges.add(k)
                    else:
                        temp_sharp_edges[k] += 1
            del temp_sharp_edges
            for e in me.edges:
                if e.key not in edges_map:
                    continue  # Only loose edges, in theory!
                t_ps[edges_map[e.key]] = not (e.use_edge_sharp or (e.key in sharp_edges))
            _map = b"ByEdge"
        lay_smooth = elem_data_single_int32(geom, b"LayerElementSmoothing", 0)
        elem_data_single_int32(lay_smooth, b"Version", FBX_GEOMETRY_SMOOTHING_VERSION)
        elem_data_single_string(lay_smooth, b"Name", b"")
        elem_data_single_string(lay_smooth, b"MappingInformationType", _map)
        elem_data_single_string(lay_smooth, b"ReferenceInformationType", b"Direct")
        elem_data_single_int32_array(lay_smooth, b"Smoothing", t_ps)  # Sight, int32 for bool...
        del t_ps

    # TODO: Edge crease (LayerElementCrease).

    # And we are done with edges!
    del edges_map

    # Loop normals.
    tspacenumber = 0
    if write_normals:
        # NOTE: this is not supported by importer currently.
        # XXX Official docs says normals should use IndexToDirect,
        #     but this does not seem well supported by apps currently...
        me.calc_normals_split()

        t_ln = array.array(data_types.ARRAY_FLOAT64, (0.0,)) * len(me.loops) * 3
        me.loops.foreach_get("normal", t_ln)
        t_ln = nors_transformed_gen(t_ln, geom_mat_no)
        if 0:
            t_ln = tuple(t_ln)  # No choice... :/

            lay_nor = elem_data_single_int32(geom, b"LayerElementNormal", 0)
            elem_data_single_int32(lay_nor, b"Version", FBX_GEOMETRY_NORMAL_VERSION)
            elem_data_single_string(lay_nor, b"Name", b"")
            elem_data_single_string(lay_nor, b"MappingInformationType", b"ByPolygonVertex")
            elem_data_single_string(lay_nor, b"ReferenceInformationType", b"IndexToDirect")

            ln2idx = tuple(set(t_ln))
            elem_data_single_float64_array(lay_nor, b"Normals", chain(*ln2idx))
            # Normal weights, no idea what it is.
            # t_lnw = array.array(data_types.ARRAY_FLOAT64, (0.0,)) * len(ln2idx)
            # elem_data_single_float64_array(lay_nor, b"NormalsW", t_lnw)

            ln2idx = {nor: idx for idx, nor in enumerate(ln2idx)}
            elem_data_single_int32_array(lay_nor, b"NormalsIndex", (ln2idx[n] for n in t_ln))

            del ln2idx
            # del t_lnw
        else:
            lay_nor = elem_data_single_int32(geom, b"LayerElementNormal", 0)
            elem_data_single_int32(lay_nor, b"Version", FBX_GEOMETRY_NORMAL_VERSION)
            elem_data_single_string(lay_nor, b"Name", b"")
            elem_data_single_string(lay_nor, b"MappingInformationType", b"ByPolygonVertex")
            elem_data_single_string(lay_nor, b"ReferenceInformationType", b"Direct")
            elem_data_single_float64_array(lay_nor, b"Normals", chain(*t_ln))
            # Normal weights, no idea what it is.
            # t_ln = array.array(data_types.ARRAY_FLOAT64, (0.0,)) * len(me.loops)
            # elem_data_single_float64_array(lay_nor, b"NormalsW", t_ln)
        del t_ln

        # tspace
        if scene_data.settings.use_tspace:
            tspacenumber = len(me.uv_layers)
            if tspacenumber:
                t_ln = array.array(data_types.ARRAY_FLOAT64, (0.0,)) * len(me.loops) * 3
                # t_lnw = array.array(data_types.ARRAY_FLOAT64, (0.0,)) * len(me.loops)
                for idx, uvlayer in enumerate(me.uv_layers):
                    name = uvlayer.name
                    me.calc_tangents(name)
                    # Loop bitangents (aka binormals).
                    # NOTE: this is not supported by importer currently.
                    me.loops.foreach_get("bitangent", t_ln)
                    lay_nor = elem_data_single_int32(geom, b"LayerElementBinormal", idx)
                    elem_data_single_int32(lay_nor, b"Version", FBX_GEOMETRY_BINORMAL_VERSION)
                    elem_data_single_string_unicode(lay_nor, b"Name", name)
                    elem_data_single_string(lay_nor, b"MappingInformationType", b"ByPolygonVertex")
                    elem_data_single_string(lay_nor, b"ReferenceInformationType", b"Direct")
                    elem_data_single_float64_array(lay_nor, b"Binormals",
                                                   chain(*nors_transformed_gen(t_ln, geom_mat_no)))
                    # Binormal weights, no idea what it is.
                    # elem_data_single_float64_array(lay_nor, b"BinormalsW", t_lnw)

                    # Loop tangents.
                    # NOTE: this is not supported by importer currently.
                    me.loops.foreach_get("tangent", t_ln)
                    lay_nor = elem_data_single_int32(geom, b"LayerElementTangent", idx)
                    elem_data_single_int32(lay_nor, b"Version", FBX_GEOMETRY_TANGENT_VERSION)
                    elem_data_single_string_unicode(lay_nor, b"Name", name)
                    elem_data_single_string(lay_nor, b"MappingInformationType", b"ByPolygonVertex")
                    elem_data_single_string(lay_nor, b"ReferenceInformationType", b"Direct")
                    elem_data_single_float64_array(lay_nor, b"Tangents",
                                                   chain(*nors_transformed_gen(t_ln, geom_mat_no)))
                    # Tangent weights, no idea what it is.
                    # elem_data_single_float64_array(lay_nor, b"TangentsW", t_lnw)

                del t_ln
                # del t_lnw
                me.free_tangents()

        me.free_normals_split()

    # Write VertexColor Layers.
    vcolnumber = len(me.vertex_colors)
    if vcolnumber:
        def _coltuples_gen(raw_cols):
            return zip(*(iter(raw_cols),) * 3 + (_infinite_gen(1.0),))  # We need a fake alpha...

        t_lc = array.array(data_types.ARRAY_FLOAT64, (0.0,)) * len(me.loops) * 3
        for colindex, collayer in enumerate(me.vertex_colors):
            collayer.data.foreach_get("color", t_lc)
            lay_vcol = elem_data_single_int32(geom, b"LayerElementColor", colindex)
            elem_data_single_int32(lay_vcol, b"Version", FBX_GEOMETRY_VCOLOR_VERSION)
            elem_data_single_string_unicode(lay_vcol, b"Name", collayer.name)
            elem_data_single_string(lay_vcol, b"MappingInformationType", b"ByPolygonVertex")
            elem_data_single_string(lay_vcol, b"ReferenceInformationType", b"IndexToDirect")

            col2idx = tuple(set(_coltuples_gen(t_lc)))
            elem_data_single_float64_array(lay_vcol, b"Colors", chain(*col2idx))  # Flatten again...

            col2idx = {col: idx for idx, col in enumerate(col2idx)}
            elem_data_single_int32_array(lay_vcol, b"ColorIndex", (col2idx[c] for c in _coltuples_gen(t_lc)))
            del col2idx
        del t_lc
        del _coltuples_gen

    # Write UV layers.
    # Note: LayerElementTexture is deprecated since FBX 2011 - luckily!
    #       Textures are now only related to materials, in FBX!
    uvnumber = len(me.uv_layers)
    if uvnumber:
        def _uvtuples_gen(raw_uvs):
            return zip(*(iter(raw_uvs),) * 2)

        t_luv = array.array(data_types.ARRAY_FLOAT64, (0.0,)) * len(me.loops) * 2
        for uvindex, uvlayer in enumerate(me.uv_layers):
            uvlayer.data.foreach_get("uv", t_luv)
            lay_uv = elem_data_single_int32(geom, b"LayerElementUV", uvindex)
            elem_data_single_int32(lay_uv, b"Version", FBX_GEOMETRY_UV_VERSION)
            elem_data_single_string_unicode(lay_uv, b"Name", uvlayer.name)
            elem_data_single_string(lay_uv, b"MappingInformationType", b"ByPolygonVertex")
            elem_data_single_string(lay_uv, b"ReferenceInformationType", b"IndexToDirect")

            uv2idx = tuple(set(_uvtuples_gen(t_luv)))
            elem_data_single_float64_array(lay_uv, b"UV", chain(*uv2idx))  # Flatten again...

            uv2idx = {uv: idx for idx, uv in enumerate(uv2idx)}
            elem_data_single_int32_array(lay_uv, b"UVIndex", (uv2idx[uv] for uv in _uvtuples_gen(t_luv)))
            del uv2idx
        del t_luv
        del _uvtuples_gen

    # Face's materials.
    me_fbxmats_idx = scene_data.mesh_mat_indices.get(me)
    if me_fbxmats_idx is not None:
        me_blmats = me.materials
        if me_fbxmats_idx and me_blmats:
            lay_mat = elem_data_single_int32(geom, b"LayerElementMaterial", 0)
            elem_data_single_int32(lay_mat, b"Version", FBX_GEOMETRY_MATERIAL_VERSION)
            elem_data_single_string(lay_mat, b"Name", b"")
            nbr_mats = len(me_fbxmats_idx)
            if nbr_mats > 1:
                t_pm = array.array(data_types.ARRAY_INT32, (0,)) * len(me.polygons)
                me.polygons.foreach_get("material_index", t_pm)

                # We have to validate mat indices, and map them to FBX indices.
                # Note a mat might not be in me_fbxmats_idx (e.g. node mats are ignored).
                blmats_to_fbxmats_idxs = [me_fbxmats_idx[m] for m in me_blmats if m in me_fbxmats_idx]
                mat_idx_limit = len(blmats_to_fbxmats_idxs)
                def_mat = blmats_to_fbxmats_idxs[0]
                _gen = (blmats_to_fbxmats_idxs[m] if m < mat_idx_limit else def_mat for m in t_pm)
                t_pm = array.array(data_types.ARRAY_INT32, _gen)

                elem_data_single_string(lay_mat, b"MappingInformationType", b"ByPolygon")
                # XXX Logically, should be "Direct" reference type, since we do not have any index array, and have one
                #     value per polygon...
                #     But looks like FBX expects it to be IndexToDirect here (maybe because materials are already
                #     indices??? *sigh*).
                elem_data_single_string(lay_mat, b"ReferenceInformationType", b"IndexToDirect")
                elem_data_single_int32_array(lay_mat, b"Materials", t_pm)
                del t_pm
            else:
                elem_data_single_string(lay_mat, b"MappingInformationType", b"AllSame")
                elem_data_single_string(lay_mat, b"ReferenceInformationType", b"IndexToDirect")
                elem_data_single_int32_array(lay_mat, b"Materials", [0])

    # And the "layer TOC"...

    layer = elem_data_single_int32(geom, b"Layer", 0)
    elem_data_single_int32(layer, b"Version", FBX_GEOMETRY_LAYER_VERSION)
    if write_normals:
        lay_nor = elem_empty(layer, b"LayerElement")
        elem_data_single_string(lay_nor, b"Type", b"LayerElementNormal")
        elem_data_single_int32(lay_nor, b"TypedIndex", 0)
    if tspacenumber:
        lay_binor = elem_empty(layer, b"LayerElement")
        elem_data_single_string(lay_binor, b"Type", b"LayerElementBinormal")
        elem_data_single_int32(lay_binor, b"TypedIndex", 0)
        lay_tan = elem_empty(layer, b"LayerElement")
        elem_data_single_string(lay_tan, b"Type", b"LayerElementTangent")
        elem_data_single_int32(lay_tan, b"TypedIndex", 0)
    if smooth_type in {'FACE', 'EDGE'}:
        lay_smooth = elem_empty(layer, b"LayerElement")
        elem_data_single_string(lay_smooth, b"Type", b"LayerElementSmoothing")
        elem_data_single_int32(lay_smooth, b"TypedIndex", 0)
    if vcolnumber:
        lay_vcol = elem_empty(layer, b"LayerElement")
        elem_data_single_string(lay_vcol, b"Type", b"LayerElementColor")
        elem_data_single_int32(lay_vcol, b"TypedIndex", 0)
    if uvnumber:
        lay_uv = elem_empty(layer, b"LayerElement")
        elem_data_single_string(lay_uv, b"Type", b"LayerElementUV")
        elem_data_single_int32(lay_uv, b"TypedIndex", 0)
    if me_fbxmats_idx is not None:
        lay_mat = elem_empty(layer, b"LayerElement")
        elem_data_single_string(lay_mat, b"Type", b"LayerElementMaterial")
        elem_data_single_int32(lay_mat, b"TypedIndex", 0)

    # Add other uv and/or vcol layers...
    for vcolidx, uvidx, tspaceidx in zip_longest(range(1, vcolnumber), range(1, uvnumber), range(1, tspacenumber),
                                                 fillvalue=0):
        layer = elem_data_single_int32(geom, b"Layer", max(vcolidx, uvidx))
        elem_data_single_int32(layer, b"Version", FBX_GEOMETRY_LAYER_VERSION)
        if vcolidx:
            lay_vcol = elem_empty(layer, b"LayerElement")
            elem_data_single_string(lay_vcol, b"Type", b"LayerElementColor")
            elem_data_single_int32(lay_vcol, b"TypedIndex", vcolidx)
        if uvidx:
            lay_uv = elem_empty(layer, b"LayerElement")
            elem_data_single_string(lay_uv, b"Type", b"LayerElementUV")
            elem_data_single_int32(lay_uv, b"TypedIndex", uvidx)
        if tspaceidx:
            lay_binor = elem_empty(layer, b"LayerElement")
            elem_data_single_string(lay_binor, b"Type", b"LayerElementBinormal")
            elem_data_single_int32(lay_binor, b"TypedIndex", tspaceidx)
            lay_tan = elem_empty(layer, b"LayerElement")
            elem_data_single_string(lay_tan, b"Type", b"LayerElementTangent")
            elem_data_single_int32(lay_tan, b"TypedIndex", tspaceidx)

    # Shape keys...
    fbx_data_mesh_shapes_elements(root, me_obj, me, scene_data, tmpl, props)

    elem_props_template_finalize(tmpl, props)
    done_meshes.add(me_key)


def check_skip_material(mat):
    """Simple helper to check whether we actually support exporting that material or not"""
    return mat.type not in {'SURFACE'} or mat.use_nodes


def fbx_data_material_elements(root, mat, scene_data):
    """
    Write the Material data block.
    """
    ambient_color = (0.0, 0.0, 0.0)
    if scene_data.data_world:
        ambient_color = next(iter(scene_data.data_world.keys())).ambient_color

    mat_key, _objs = scene_data.data_materials[mat]
    skip_mat = check_skip_material(mat)
    mat_type = b"Phong"
    # Approximation...
    if not skip_mat and mat.specular_shader not in {'COOKTORR', 'PHONG', 'BLINN'}:
        mat_type = b"Lambert"

    fbx_mat = elem_data_single_int64(root, b"Material", get_fbx_uuid_from_key(mat_key))
    fbx_mat.add_string(fbx_name_class(mat.name.encode(), b"Material"))
    fbx_mat.add_string(b"")

    elem_data_single_int32(fbx_mat, b"Version", FBX_MATERIAL_VERSION)
    # those are not yet properties, it seems...
    elem_data_single_string(fbx_mat, b"ShadingModel", mat_type)
    elem_data_single_int32(fbx_mat, b"MultiLayer", 0)  # Should be bool...

    tmpl = elem_props_template_init(scene_data.templates, b"Material")
    props = elem_properties(fbx_mat)

    if not skip_mat:
        elem_props_template_set(tmpl, props, "p_string", b"ShadingModel", mat_type.decode())
        elem_props_template_set(tmpl, props, "p_color", b"EmissiveColor", mat.diffuse_color)
        elem_props_template_set(tmpl, props, "p_number", b"EmissiveFactor", mat.emit)
        elem_props_template_set(tmpl, props, "p_color", b"AmbientColor", ambient_color)
        elem_props_template_set(tmpl, props, "p_number", b"AmbientFactor", mat.ambient)
        elem_props_template_set(tmpl, props, "p_color", b"DiffuseColor", mat.diffuse_color)
        elem_props_template_set(tmpl, props, "p_number", b"DiffuseFactor", mat.diffuse_intensity)
        elem_props_template_set(tmpl, props, "p_color", b"TransparentColor",
                                mat.diffuse_color if mat.use_transparency else (1.0, 1.0, 1.0))
        elem_props_template_set(tmpl, props, "p_number", b"TransparencyFactor",
                                1.0 - mat.alpha if mat.use_transparency else 0.0)
        elem_props_template_set(tmpl, props, "p_number", b"Opacity", mat.alpha if mat.use_transparency else 1.0)
        elem_props_template_set(tmpl, props, "p_vector_3d", b"NormalMap", (0.0, 0.0, 0.0))
        # Not sure about those...
        """
        b"Bump": ((0.0, 0.0, 0.0), "p_vector_3d"),
        b"BumpFactor": (1.0, "p_double"),
        b"DisplacementColor": ((0.0, 0.0, 0.0), "p_color_rgb"),
        b"DisplacementFactor": (0.0, "p_double"),
        """
        if mat_type == b"Phong":
            elem_props_template_set(tmpl, props, "p_color", b"SpecularColor", mat.specular_color)
            elem_props_template_set(tmpl, props, "p_number", b"SpecularFactor", mat.specular_intensity / 2.0)
            # See Material template about those two!
            elem_props_template_set(tmpl, props, "p_number", b"Shininess", (mat.specular_hardness - 1.0) / 5.10)
            elem_props_template_set(tmpl, props, "p_number", b"ShininessExponent", (mat.specular_hardness - 1.0) / 5.10)
            elem_props_template_set(tmpl, props, "p_color", b"ReflectionColor", mat.mirror_color)
            elem_props_template_set(tmpl, props, "p_number", b"ReflectionFactor",
                                    mat.raytrace_mirror.reflect_factor if mat.raytrace_mirror.use else 0.0)

    elem_props_template_finalize(tmpl, props)

    # Custom properties.
    if scene_data.settings.use_custom_props:
        fbx_data_element_custom_properties(props, mat)


def _gen_vid_path(img, scene_data):
    msetts = scene_data.settings.media_settings
    fname_rel = bpy_extras.io_utils.path_reference(img.filepath, msetts.base_src, msetts.base_dst, msetts.path_mode,
                                                   msetts.subdir, msetts.copy_set, img.library)
    fname_abs = os.path.normpath(os.path.abspath(os.path.join(msetts.base_dst, fname_rel)))
    return fname_abs, fname_rel


def fbx_data_texture_file_elements(root, tex, scene_data):
    """
    Write the (file) Texture data block.
    """
    # XXX All this is very fuzzy to me currently...
    #     Textures do not seem to use properties as much as they could.
    #     For now assuming most logical and simple stuff.

    tex_key, _mats = scene_data.data_textures[tex]
    img = tex.texture.image
    fname_abs, fname_rel = _gen_vid_path(img, scene_data)

    fbx_tex = elem_data_single_int64(root, b"Texture", get_fbx_uuid_from_key(tex_key))
    fbx_tex.add_string(fbx_name_class(tex.name.encode(), b"Texture"))
    fbx_tex.add_string(b"")

    elem_data_single_string(fbx_tex, b"Type", b"TextureVideoClip")
    elem_data_single_int32(fbx_tex, b"Version", FBX_TEXTURE_VERSION)
    elem_data_single_string(fbx_tex, b"TextureName", fbx_name_class(tex.name.encode(), b"Texture"))
    elem_data_single_string(fbx_tex, b"Media", fbx_name_class(img.name.encode(), b"Video"))
    elem_data_single_string_unicode(fbx_tex, b"FileName", fname_abs)
    elem_data_single_string_unicode(fbx_tex, b"RelativeFilename", fname_rel)

    alpha_source = 0  # None
    if img.use_alpha:
        if tex.texture.use_calculate_alpha:
            alpha_source = 1  # RGBIntensity as alpha.
        else:
            alpha_source = 2  # Black, i.e. alpha channel.
    # BlendMode not useful for now, only affects layered textures afaics.
    mapping = 0  # UV.
    uvset = None
    if tex.texture_coords in {'ORCO'}:  # XXX Others?
        if tex.mapping in {'FLAT'}:
            mapping = 1  # Planar
        elif tex.mapping in {'CUBE'}:
            mapping = 4  # Box
        elif tex.mapping in {'TUBE'}:
            mapping = 3  # Cylindrical
        elif tex.mapping in {'SPHERE'}:
            mapping = 2  # Spherical
    elif tex.texture_coords in {'UV'}:
        mapping = 0  # UV
        # Yuck, UVs are linked by mere names it seems... :/
        uvset = tex.uv_layer
    wrap_mode = 1  # Clamp
    if tex.texture.extension in {'REPEAT'}:
        wrap_mode = 0  # Repeat

    tmpl = elem_props_template_init(scene_data.templates, b"TextureFile")
    props = elem_properties(fbx_tex)
    elem_props_template_set(tmpl, props, "p_enum", b"AlphaSource", alpha_source)
    elem_props_template_set(tmpl, props, "p_bool", b"PremultiplyAlpha",
                            img.alpha_mode in {'STRAIGHT'})  # Or is it PREMUL?
    elem_props_template_set(tmpl, props, "p_enum", b"CurrentMappingType", mapping)
    if uvset is not None:
        elem_props_template_set(tmpl, props, "p_string", b"UVSet", uvset)
    elem_props_template_set(tmpl, props, "p_enum", b"WrapModeU", wrap_mode)
    elem_props_template_set(tmpl, props, "p_enum", b"WrapModeV", wrap_mode)
    elem_props_template_set(tmpl, props, "p_vector_3d", b"Translation", tex.offset)
    elem_props_template_set(tmpl, props, "p_vector_3d", b"Scaling", tex.scale)
    # UseMaterial should always be ON imho.
    elem_props_template_set(tmpl, props, "p_bool", b"UseMaterial", True)
    elem_props_template_set(tmpl, props, "p_bool", b"UseMipMap", tex.texture.use_mipmap)
    elem_props_template_finalize(tmpl, props)

    # Custom properties.
    if scene_data.settings.use_custom_props:
        fbx_data_element_custom_properties(props, tex.texture)


def fbx_data_video_elements(root, vid, scene_data):
    """
    Write the actual image data block.
    """
    msetts = scene_data.settings.media_settings

    vid_key, _texs = scene_data.data_videos[vid]
    fname_abs, fname_rel = _gen_vid_path(vid, scene_data)

    fbx_vid = elem_data_single_int64(root, b"Video", get_fbx_uuid_from_key(vid_key))
    fbx_vid.add_string(fbx_name_class(vid.name.encode(), b"Video"))
    fbx_vid.add_string(b"Clip")

    elem_data_single_string(fbx_vid, b"Type", b"Clip")
    # XXX No Version???

    tmpl = elem_props_template_init(scene_data.templates, b"Video")
    props = elem_properties(fbx_vid)
    elem_props_template_set(tmpl, props, "p_string_url", b"Path", fname_abs)
    elem_props_template_finalize(tmpl, props)

    elem_data_single_int32(fbx_vid, b"UseMipMap", 0)
    elem_data_single_string_unicode(fbx_vid, b"Filename", fname_abs)
    elem_data_single_string_unicode(fbx_vid, b"RelativeFilename", fname_rel)

    if scene_data.settings.media_settings.embed_textures:
        if vid.packed_file is not None:
            # We only ever embed a given file once!
            if fname_abs not in msetts.embedded_set:
                elem_data_single_bytes(fbx_vid, b"Content", vid.packed_file.data)
                msetts.embedded_set.add(fname_abs)
        else:
            filepath = bpy.path.abspath(vid.filepath)
            # We only ever embed a given file once!
            if filepath not in msetts.embedded_set:
                try:
                    with open(filepath, 'br') as f:
                        elem_data_single_bytes(fbx_vid, b"Content", f.read())
                except Exception as e:
                    print("WARNING: embedding file {} failed ({})".format(filepath, e))
                    elem_data_single_bytes(fbx_vid, b"Content", b"")
                msetts.embedded_set.add(filepath)
    # Looks like we'd rather not write any 'Content' element in this case (see T44442).
    # Sounds suspect, but let's try it!
    #~ else:
        #~ elem_data_single_bytes(fbx_vid, b"Content", b"")


def fbx_data_armature_elements(root, arm_obj, scene_data):
    """
    Write:
        * Bones "data" (NodeAttribute::LimbNode, contains pretty much nothing!).
        * Deformers (i.e. Skin), bind between an armature and a mesh.
        ** SubDeformers (i.e. Cluster), one per bone/vgroup pair.
        * BindPose.
    Note armature itself has no data, it is a mere "Null" Model...
    """
    mat_world_arm = arm_obj.fbx_object_matrix(scene_data, global_space=True)
    bones = tuple(bo_obj for bo_obj in arm_obj.bones if bo_obj in scene_data.objects)

    bone_radius_scale = 33.0

    # Bones "data".
    for bo_obj in bones:
        bo = bo_obj.bdata
        bo_data_key = scene_data.data_bones[bo_obj]
        fbx_bo = elem_data_single_int64(root, b"NodeAttribute", get_fbx_uuid_from_key(bo_data_key))
        fbx_bo.add_string(fbx_name_class(bo.name.encode(), b"NodeAttribute"))
        fbx_bo.add_string(b"LimbNode")
        elem_data_single_string(fbx_bo, b"TypeFlags", b"Skeleton")

        tmpl = elem_props_template_init(scene_data.templates, b"Bone")
        props = elem_properties(fbx_bo)
        elem_props_template_set(tmpl, props, "p_double", b"Size", bo.head_radius * bone_radius_scale)
        elem_props_template_finalize(tmpl, props)

        # Custom properties.
        if scene_data.settings.use_custom_props:
            fbx_data_element_custom_properties(props, bo)

        # Store Blender bone length - XXX Not much useful actually :/
        # (LimbLength can't be used because it is a scale factor 0-1 for the parent-child distance:
        # http://docs.autodesk.com/FBX/2014/ENU/FBX-SDK-Documentation/cpp_ref/class_fbx_skeleton.html#a9bbe2a70f4ed82cd162620259e649f0f )
        # elem_props_set(props, "p_double", "BlenderBoneLength".encode(), (bo.tail_local - bo.head_local).length, custom=True)

    # Skin deformers and BindPoses.
    # Note: we might also use Deformers for our "parent to vertex" stuff???
    deformer = scene_data.data_deformers_skin.get(arm_obj, None)
    if deformer is not None:
        for me, (skin_key, ob_obj, clusters) in deformer.items():
            # BindPose.
            mat_world_obj, mat_world_bones = fbx_data_bindpose_element(root, ob_obj, me, scene_data,
                                                                       arm_obj, mat_world_arm, bones)

            # Deformer.
            fbx_skin = elem_data_single_int64(root, b"Deformer", get_fbx_uuid_from_key(skin_key))
            fbx_skin.add_string(fbx_name_class(arm_obj.name.encode(), b"Deformer"))
            fbx_skin.add_string(b"Skin")

            elem_data_single_int32(fbx_skin, b"Version", FBX_DEFORMER_SKIN_VERSION)
            elem_data_single_float64(fbx_skin, b"Link_DeformAcuracy", 50.0)  # Only vague idea what it is...

            # Pre-process vertex weights (also to check vertices assigned ot more than four bones).
            ob = ob_obj.bdata
            bo_vg_idx = {bo_obj.bdata.name: ob.vertex_groups[bo_obj.bdata.name].index
                         for bo_obj in clusters.keys() if bo_obj.bdata.name in ob.vertex_groups}
            valid_idxs = set(bo_vg_idx.values())
            vgroups = {vg.index: OrderedDict() for vg in ob.vertex_groups}
            verts_vgroups = (sorted(((vg.group, vg.weight) for vg in v.groups if vg.weight and vg.group in valid_idxs),
                                    key=lambda e: e[1], reverse=True)
                             for v in me.vertices)
            for idx, vgs in enumerate(verts_vgroups):
                for vg_idx, w in vgs:
                    vgroups[vg_idx][idx] = w

            for bo_obj, clstr_key in clusters.items():
                bo = bo_obj.bdata
                # Find which vertices are affected by this bone/vgroup pair, and matching weights.
                # Note we still write a cluster for bones not affecting the mesh, to get 'rest pose' data
                # (the TransformBlah matrices).
                vg_idx = bo_vg_idx.get(bo.name, None)
                indices, weights = ((), ()) if vg_idx is None or not vgroups[vg_idx] else zip(*vgroups[vg_idx].items())

                # Create the cluster.
                fbx_clstr = elem_data_single_int64(root, b"Deformer", get_fbx_uuid_from_key(clstr_key))
                fbx_clstr.add_string(fbx_name_class(bo.name.encode(), b"SubDeformer"))
                fbx_clstr.add_string(b"Cluster")

                elem_data_single_int32(fbx_clstr, b"Version", FBX_DEFORMER_CLUSTER_VERSION)
                # No idea what that user data might be...
                fbx_userdata = elem_data_single_string(fbx_clstr, b"UserData", b"")
                fbx_userdata.add_string(b"")
                if indices:
                    elem_data_single_int32_array(fbx_clstr, b"Indexes", indices)
                    elem_data_single_float64_array(fbx_clstr, b"Weights", weights)
                # Transform, TransformLink and TransformAssociateModel matrices...
                # They seem to be doublons of BindPose ones??? Have armature (associatemodel) in addition, though.
                # WARNING! Even though official FBX API presents Transform in global space,
                #          **it is stored in bone space in FBX data!** See:
                #          http://area.autodesk.com/forum/autodesk-fbx/fbx-sdk/why-the-values-return-
                #                 by-fbxcluster-gettransformmatrix-x-not-same-with-the-value-in-ascii-fbx-file/
                elem_data_single_float64_array(fbx_clstr, b"Transform",
                                               matrix4_to_array(mat_world_bones[bo_obj].inverted_safe() * mat_world_obj))
                elem_data_single_float64_array(fbx_clstr, b"TransformLink", matrix4_to_array(mat_world_bones[bo_obj]))
                elem_data_single_float64_array(fbx_clstr, b"TransformAssociateModel", matrix4_to_array(mat_world_arm))


def fbx_data_leaf_bone_elements(root, scene_data):
    # Write a dummy leaf bone that is used by applications to show the length of the last bone in a chain
    for (node_name, _par_uuid, node_uuid, attr_uuid, matrix, hide, size) in scene_data.data_leaf_bones:
        # Bone 'data'...
        fbx_bo = elem_data_single_int64(root, b"NodeAttribute", attr_uuid)
        fbx_bo.add_string(fbx_name_class(node_name.encode(), b"NodeAttribute"))
        fbx_bo.add_string(b"LimbNode")
        elem_data_single_string(fbx_bo, b"TypeFlags", b"Skeleton")

        tmpl = elem_props_template_init(scene_data.templates, b"Bone")
        props = elem_properties(fbx_bo)
        elem_props_template_set(tmpl, props, "p_double", b"Size", size)
        elem_props_template_finalize(tmpl, props)

        # And bone object.
        model = elem_data_single_int64(root, b"Model", node_uuid)
        model.add_string(fbx_name_class(node_name.encode(), b"Model"))
        model.add_string(b"LimbNode")

        elem_data_single_int32(model, b"Version", FBX_MODELS_VERSION)

        # Object transform info.
        loc, rot, scale = matrix.decompose()
        rot = rot.to_euler('XYZ')
        rot = tuple(convert_rad_to_deg_iter(rot))

        tmpl = elem_props_template_init(scene_data.templates, b"Model")
        # For now add only loc/rot/scale...
        props = elem_properties(model)
        # Generated leaf bones are obviously never animated!
        elem_props_template_set(tmpl, props, "p_lcl_translation", b"Lcl Translation", loc)
        elem_props_template_set(tmpl, props, "p_lcl_rotation", b"Lcl Rotation", rot)
        elem_props_template_set(tmpl, props, "p_lcl_scaling", b"Lcl Scaling", scale)
        elem_props_template_set(tmpl, props, "p_visibility", b"Visibility", float(not hide))

        # Absolutely no idea what this is, but seems mandatory for validity of the file, and defaults to
        # invalid -1 value...
        elem_props_template_set(tmpl, props, "p_integer", b"DefaultAttributeIndex", 0)

        elem_props_template_set(tmpl, props, "p_enum", b"InheritType", 1)  # RSrs

        # Those settings would obviously need to be edited in a complete version of the exporter, may depends on
        # object type, etc.
        elem_data_single_int32(model, b"MultiLayer", 0)
        elem_data_single_int32(model, b"MultiTake", 0)
        elem_data_single_bool(model, b"Shading", True)
        elem_data_single_string(model, b"Culling", b"CullingOff")

        elem_props_template_finalize(tmpl, props)


def fbx_data_object_elements(root, ob_obj, scene_data):
    """
    Write the Object (Model) data blocks.
    Note this "Model" can also be bone or dupli!
    """
    obj_type = b"Null"  # default, sort of empty...
    if ob_obj.is_bone:
        obj_type = b"LimbNode"
    elif (ob_obj.type == 'ARMATURE'):
        if scene_data.settings.armature_nodetype == 'ROOT':
            obj_type = b"Root"
        elif scene_data.settings.armature_nodetype == 'LIMBNODE':
            obj_type = b"LimbNode"
        else:  # Default, preferred option...
            obj_type = b"Null"
    elif (ob_obj.type in BLENDER_OBJECT_TYPES_MESHLIKE):
        obj_type = b"Mesh"
    elif (ob_obj.type == 'LAMP'):
        obj_type = b"Light"
    elif (ob_obj.type == 'CAMERA'):
        obj_type = b"Camera"
    model = elem_data_single_int64(root, b"Model", ob_obj.fbx_uuid)
    model.add_string(fbx_name_class(ob_obj.name.encode(), b"Model"))
    model.add_string(obj_type)

    elem_data_single_int32(model, b"Version", FBX_MODELS_VERSION)

    # Object transform info.
    loc, rot, scale, matrix, matrix_rot = ob_obj.fbx_object_tx(scene_data)
    rot = tuple(convert_rad_to_deg_iter(rot))

    tmpl = elem_props_template_init(scene_data.templates, b"Model")
    # For now add only loc/rot/scale...
    props = elem_properties(model)
    elem_props_template_set(tmpl, props, "p_lcl_translation", b"Lcl Translation", loc,
                            animatable=True, animated=((ob_obj.key, "Lcl Translation") in scene_data.animated))
    elem_props_template_set(tmpl, props, "p_lcl_rotation", b"Lcl Rotation", rot,
                            animatable=True, animated=((ob_obj.key, "Lcl Rotation") in scene_data.animated))
    elem_props_template_set(tmpl, props, "p_lcl_scaling", b"Lcl Scaling", scale,
                            animatable=True, animated=((ob_obj.key, "Lcl Scaling") in scene_data.animated))
    elem_props_template_set(tmpl, props, "p_visibility", b"Visibility", float(not ob_obj.hide))

    # Absolutely no idea what this is, but seems mandatory for validity of the file, and defaults to
    # invalid -1 value...
    elem_props_template_set(tmpl, props, "p_integer", b"DefaultAttributeIndex", 0)

    elem_props_template_set(tmpl, props, "p_enum", b"InheritType", 1)  # RSrs

    # Custom properties.
    if scene_data.settings.use_custom_props:
        fbx_data_element_custom_properties(props, ob_obj.bdata)

    # Those settings would obviously need to be edited in a complete version of the exporter, may depends on
    # object type, etc.
    elem_data_single_int32(model, b"MultiLayer", 0)
    elem_data_single_int32(model, b"MultiTake", 0)
    elem_data_single_bool(model, b"Shading", True)
    elem_data_single_string(model, b"Culling", b"CullingOff")

    if obj_type == b"Camera":
        # Why, oh why are FBX cameras such a mess???
        # And WHY add camera data HERE??? Not even sure this is needed...
        render = scene_data.scene.render
        width = render.resolution_x * 1.0
        height = render.resolution_y * 1.0
        elem_props_template_set(tmpl, props, "p_enum", b"ResolutionMode", 0)  # Don't know what it means
        elem_props_template_set(tmpl, props, "p_double", b"AspectW", width)
        elem_props_template_set(tmpl, props, "p_double", b"AspectH", height)
        elem_props_template_set(tmpl, props, "p_bool", b"ViewFrustum", True)
        elem_props_template_set(tmpl, props, "p_enum", b"BackgroundMode", 0)  # Don't know what it means
        elem_props_template_set(tmpl, props, "p_bool", b"ForegroundTransparent", True)

    elem_props_template_finalize(tmpl, props)


def fbx_data_animation_elements(root, scene_data):
    """
    Write animation data.
    """
    animations = scene_data.animations
    if not animations:
        return
    scene = scene_data.scene

    fps = scene.render.fps / scene.render.fps_base

    def keys_to_ktimes(keys):
        return (int(v) for v in convert_sec_to_ktime_iter((f / fps for f, _v in keys)))

    # Animation stacks.
    for astack_key, alayers, alayer_key, name, f_start, f_end in animations:
        astack = elem_data_single_int64(root, b"AnimationStack", get_fbx_uuid_from_key(astack_key))
        astack.add_string(fbx_name_class(name, b"AnimStack"))
        astack.add_string(b"")

        astack_tmpl = elem_props_template_init(scene_data.templates, b"AnimationStack")
        astack_props = elem_properties(astack)
        r = scene_data.scene.render
        fps = r.fps / r.fps_base
        start = int(convert_sec_to_ktime(f_start / fps))
        end = int(convert_sec_to_ktime(f_end / fps))
        elem_props_template_set(astack_tmpl, astack_props, "p_timestamp", b"LocalStart", start)
        elem_props_template_set(astack_tmpl, astack_props, "p_timestamp", b"LocalStop", end)
        elem_props_template_set(astack_tmpl, astack_props, "p_timestamp", b"ReferenceStart", start)
        elem_props_template_set(astack_tmpl, astack_props, "p_timestamp", b"ReferenceStop", end)
        elem_props_template_finalize(astack_tmpl, astack_props)

        # For now, only one layer for all animations.
        alayer = elem_data_single_int64(root, b"AnimationLayer", get_fbx_uuid_from_key(alayer_key))
        alayer.add_string(fbx_name_class(name, b"AnimLayer"))
        alayer.add_string(b"")

        for ob_obj, (alayer_key, acurvenodes) in alayers.items():
            # Animation layer.
            # alayer = elem_data_single_int64(root, b"AnimationLayer", get_fbx_uuid_from_key(alayer_key))
            # alayer.add_string(fbx_name_class(ob_obj.name.encode(), b"AnimLayer"))
            # alayer.add_string(b"")

            for fbx_prop, (acurvenode_key, acurves, acurvenode_name) in acurvenodes.items():
                # Animation curve node.
                acurvenode = elem_data_single_int64(root, b"AnimationCurveNode", get_fbx_uuid_from_key(acurvenode_key))
                acurvenode.add_string(fbx_name_class(acurvenode_name.encode(), b"AnimCurveNode"))
                acurvenode.add_string(b"")

                acn_tmpl = elem_props_template_init(scene_data.templates, b"AnimationCurveNode")
                acn_props = elem_properties(acurvenode)

                for fbx_item, (acurve_key, def_value, keys, _acurve_valid) in acurves.items():
                    elem_props_template_set(acn_tmpl, acn_props, "p_number", fbx_item.encode(),
                                            def_value, animatable=True)

                    # Only create Animation curve if needed!
                    if keys:
                        acurve = elem_data_single_int64(root, b"AnimationCurve", get_fbx_uuid_from_key(acurve_key))
                        acurve.add_string(fbx_name_class(b"", b"AnimCurve"))
                        acurve.add_string(b"")

                        # key attributes...
                        nbr_keys = len(keys)
                        # flags...
                        keyattr_flags = (
                            1 << 2 |   # interpolation mode, 1 = constant, 2 = linear, 3 = cubic.
                            1 << 8 |   # tangent mode, 8 = auto, 9 = TCB, 10 = user, 11 = generic break,
                            1 << 13 |  # tangent mode, 12 = generic clamp, 13 = generic time independent,
                            1 << 14 |  # tangent mode, 13 + 14 = generic clamp progressive.
                            0,
                        )
                        # Maybe values controlling TCB & co???
                        keyattr_datafloat = (0.0, 0.0, 9.419963346924634e-30, 0.0)

                        # And now, the *real* data!
                        elem_data_single_float64(acurve, b"Default", def_value)
                        elem_data_single_int32(acurve, b"KeyVer", FBX_ANIM_KEY_VERSION)
                        elem_data_single_int64_array(acurve, b"KeyTime", keys_to_ktimes(keys))
                        elem_data_single_float32_array(acurve, b"KeyValueFloat", (v for _f, v in keys))
                        elem_data_single_int32_array(acurve, b"KeyAttrFlags", keyattr_flags)
                        elem_data_single_float32_array(acurve, b"KeyAttrDataFloat", keyattr_datafloat)
                        elem_data_single_int32_array(acurve, b"KeyAttrRefCount", (nbr_keys,))

                elem_props_template_finalize(acn_tmpl, acn_props)


# ##### Top-level FBX data container. #####

def fbx_mat_properties_from_texture(tex):
    """
    Returns a set of FBX metarial properties that are affected by the given texture.
    Quite obviously, this is a fuzzy and far-from-perfect mapping! Amounts of influence are completely lost, e.g.
    Note tex is actually expected to be a texture slot.
    """
    # Mapping Blender -> FBX (blend_use_name, blend_fact_name, fbx_name).
    blend_to_fbx = (
        # Lambert & Phong...
        ("diffuse", "diffuse", b"DiffuseFactor"),
        ("color_diffuse", "diffuse_color", b"DiffuseColor"),
        ("alpha", "alpha", b"TransparencyFactor"),
        ("diffuse", "diffuse", b"TransparentColor"),  # Uses diffuse color in Blender!
        ("emit", "emit", b"EmissiveFactor"),
        ("diffuse", "diffuse", b"EmissiveColor"),  # Uses diffuse color in Blender!
        ("ambient", "ambient", b"AmbientFactor"),
        # ("", "", b"AmbientColor"),  # World stuff in Blender, for now ignore...
        ("normal", "normal", b"NormalMap"),
        # Note: unsure about those... :/
        # ("", "", b"Bump"),
        # ("", "", b"BumpFactor"),
        # ("", "", b"DisplacementColor"),
        # ("", "", b"DisplacementFactor"),
        # Phong only.
        ("specular", "specular", b"SpecularFactor"),
        ("color_spec", "specular_color", b"SpecularColor"),
        # See Material template about those two!
        ("hardness", "hardness", b"Shininess"),
        ("hardness", "hardness", b"ShininessExponent"),
        ("mirror", "mirror", b"ReflectionColor"),
        ("raymir", "raymir", b"ReflectionFactor"),
    )

    tex_fbx_props = set()
    for use_map_name, name_factor, fbx_prop_name in blend_to_fbx:
        # Always export enabled textures, even if they have a null influence...
        if getattr(tex, "use_map_" + use_map_name):
            tex_fbx_props.add(fbx_prop_name)

    return tex_fbx_props


def fbx_skeleton_from_armature(scene, settings, arm_obj, objects, data_meshes,
                               data_bones, data_deformers_skin, data_empties, arm_parents):
    """
    Create skeleton from armature/bones (NodeAttribute/LimbNode and Model/LimbNode), and for each deformed mesh,
    create Pose/BindPose(with sub PoseNode) and Deformer/Skin(with Deformer/SubDeformer/Cluster).
    Also supports "parent to bone" (simple parent to Model/LimbNode).
    arm_parents is a set of tuples (armature, object) for all successful armature bindings.
    """
    # We need some data for our armature 'object' too!!!
    data_empties[arm_obj] = get_blender_empty_key(arm_obj.bdata)

    arm_data = arm_obj.bdata.data
    bones = OrderedDict()
    for bo in arm_obj.bones:
        if settings.use_armature_deform_only:
            if bo.bdata.use_deform:
                bones[bo] = True
                bo_par = bo.parent
                while bo_par.is_bone:
                    bones[bo_par] = True
                    bo_par = bo_par.parent
            elif bo not in bones:  # Do not override if already set in the loop above!
                bones[bo] = False
        else:
            bones[bo] = True

    bones = OrderedDict((bo, None) for bo, use in bones.items() if use)

    if not bones:
        return

    data_bones.update((bo, get_blender_bone_key(arm_obj.bdata, bo.bdata)) for bo in bones)

    for ob_obj in objects:
        if not ob_obj.is_deformed_by_armature(arm_obj):
            continue

        # Always handled by an Armature modifier...
        found = False
        for mod in ob_obj.bdata.modifiers:
            if mod.type not in {'ARMATURE'} or not mod.object:
                continue
            # We only support vertex groups binding method, not bone envelopes one!
            if mod.object in {arm_obj.bdata, arm_obj.bdata.proxy} and mod.use_vertex_groups:
                found = True
                break

        if not found:
            continue

        # Now we have a mesh using this armature.
        # Note: bindpose have no relations at all (no connections), so no need for any preprocess for them.
        # Create skin & clusters relations (note skins are connected to geometry, *not* model!).
        _key, me, _free = data_meshes[ob_obj]
        clusters = OrderedDict((bo, get_blender_bone_cluster_key(arm_obj.bdata, me, bo.bdata)) for bo in bones)
        data_deformers_skin.setdefault(arm_obj, OrderedDict())[me] = (get_blender_armature_skin_key(arm_obj.bdata, me),
                                                                      ob_obj, clusters)

        # We don't want a regular parent relationship for those in FBX...
        arm_parents.add((arm_obj, ob_obj))
        # Needed to handle matrices/spaces (since we do not parent them to 'armature' in FBX :/ ).
        ob_obj.parented_to_armature = True

    objects.update(bones)


def fbx_generate_leaf_bones(settings, data_bones):
    # find which bons have no children
    child_count = {bo: 0 for bo in data_bones.keys()}
    for bo in data_bones.keys():
        if bo.parent and bo.parent.is_bone:
            child_count[bo.parent] += 1

    bone_radius_scale = settings.global_scale * 33.0

    # generate bone data
    leaf_parents = [bo for bo, count in child_count.items() if count == 0]
    leaf_bones = []
    for parent in leaf_parents:
        node_name = parent.name + "_end"
        parent_uuid = parent.fbx_uuid
        parent_key = parent.key
        node_uuid = get_fbx_uuid_from_key(parent_key + "_end_node")
        attr_uuid = get_fbx_uuid_from_key(parent_key + "_end_nodeattr")

        hide = parent.hide
        size = parent.bdata.head_radius * bone_radius_scale
        bone_length = (parent.bdata.tail_local - parent.bdata.head_local).length
        matrix = Matrix.Translation((0, bone_length, 0))
        if settings.bone_correction_matrix_inv:
            matrix = settings.bone_correction_matrix_inv * matrix
        if settings.bone_correction_matrix:
            matrix = matrix * settings.bone_correction_matrix
        leaf_bones.append((node_name, parent_uuid, node_uuid, attr_uuid, matrix, hide, size))

    return leaf_bones


def fbx_animations_do(scene_data, ref_id, f_start, f_end, start_zero, objects=None, force_keep=False):
    """
    Generate animation data (a single AnimStack) from objects, for a given frame range.
    """
    bake_step = scene_data.settings.bake_anim_step
    simplify_fac = scene_data.settings.bake_anim_simplify_factor
    scene = scene_data.scene
    force_keying = scene_data.settings.bake_anim_use_all_bones
    force_sek = scene_data.settings.bake_anim_force_startend_keying

    if objects is not None:
        # Add bones and duplis!
        for ob_obj in tuple(objects):
            if not ob_obj.is_object:
                continue
            if ob_obj.type == 'ARMATURE':
                objects |= {bo_obj for bo_obj in ob_obj.bones if bo_obj in scene_data.objects}
            ob_obj.dupli_list_create(scene, 'RENDER')
            for dp_obj in ob_obj.dupli_list:
                if dp_obj in scene_data.objects:
                    objects.add(dp_obj)
            ob_obj.dupli_list_clear()
    else:
        objects = scene_data.objects

    back_currframe = scene.frame_current
    animdata_ob = OrderedDict()
    p_rots = {}

    for ob_obj in objects:
        if ob_obj.parented_to_armature:
            continue
        ACNW = AnimationCurveNodeWrapper
        loc, rot, scale, _m, _mr = ob_obj.fbx_object_tx(scene_data)
        rot_deg = tuple(convert_rad_to_deg_iter(rot))
        force_key = (simplify_fac == 0.0) or (ob_obj.is_bone and force_keying)
        animdata_ob[ob_obj] = (ACNW(ob_obj.key, 'LCL_TRANSLATION', force_key, force_sek, loc),
                               ACNW(ob_obj.key, 'LCL_ROTATION', force_key, force_sek, rot_deg),
                               ACNW(ob_obj.key, 'LCL_SCALING', force_key, force_sek, scale))
        p_rots[ob_obj] = rot

    animdata_shapes = OrderedDict()
    force_key = (simplify_fac == 0.0)
    for me, (me_key, _shapes_key, shapes) in scene_data.data_deformers_shape.items():
        # Ignore absolute shape keys for now!
        if not me.shape_keys.use_relative:
            continue
        for shape, (channel_key, geom_key, _shape_verts_co, _shape_verts_idx) in shapes.items():
            acnode = AnimationCurveNodeWrapper(channel_key, 'SHAPE_KEY', force_key, force_sek, (0.0,))
            # Sooooo happy to have to twist again like a mad snake... Yes, we need to write those curves twice. :/
            acnode.add_group(me_key, shape.name, shape.name, (shape.name,))
            animdata_shapes[channel_key] = (acnode, me, shape)

    currframe = f_start
    while currframe <= f_end:
        real_currframe = currframe - f_start if start_zero else currframe
        scene.frame_set(int(currframe), currframe - int(currframe))

        for ob_obj in animdata_ob:
            ob_obj.dupli_list_create(scene, 'RENDER')
        for ob_obj, (anim_loc, anim_rot, anim_scale) in animdata_ob.items():
            # We compute baked loc/rot/scale for all objects (rot being euler-compat with previous value!).
            p_rot = p_rots.get(ob_obj, None)
            loc, rot, scale, _m, _mr = ob_obj.fbx_object_tx(scene_data, rot_euler_compat=p_rot)
            p_rots[ob_obj] = rot
            anim_loc.add_keyframe(real_currframe, loc)
            anim_rot.add_keyframe(real_currframe, tuple(convert_rad_to_deg_iter(rot)))
            anim_scale.add_keyframe(real_currframe, scale)
        for ob_obj in objects:
            ob_obj.dupli_list_clear()
        for anim_shape, me, shape in animdata_shapes.values():
            anim_shape.add_keyframe(real_currframe, (shape.value * 100.0,))
        currframe += bake_step

    scene.frame_set(back_currframe, 0.0)

    animations = OrderedDict()

    # And now, produce final data (usable by FBX export code)
    # Objects-like loc/rot/scale...
    for ob_obj, anims in animdata_ob.items():
        for anim in anims:
            anim.simplify(simplify_fac, bake_step, force_keep)
            if not anim:
                continue
            for obj_key, group_key, group, fbx_group, fbx_gname in anim.get_final_data(scene, ref_id, force_keep):
                anim_data = animations.get(obj_key)
                if anim_data is None:
                    anim_data = animations[obj_key] = ("dummy_unused_key", OrderedDict())
                anim_data[1][fbx_group] = (group_key, group, fbx_gname)

    # And meshes' shape keys.
    for channel_key, (anim_shape, me, shape) in animdata_shapes.items():
        final_keys = OrderedDict()
        anim_shape.simplify(simplify_fac, bake_step, force_keep)
        if not anim_shape:
            continue
        for elem_key, group_key, group, fbx_group, fbx_gname in anim_shape.get_final_data(scene, ref_id, force_keep):
                anim_data = animations.get(elem_key)
                if anim_data is None:
                    anim_data = animations[elem_key] = ("dummy_unused_key", OrderedDict())
                anim_data[1][fbx_group] = (group_key, group, fbx_gname)

    astack_key = get_blender_anim_stack_key(scene, ref_id)
    alayer_key = get_blender_anim_layer_key(scene, ref_id)
    name = (get_blenderID_name(ref_id) if ref_id else scene.name).encode()

    if start_zero:
        f_end -= f_start
        f_start = 0.0

    return (astack_key, animations, alayer_key, name, f_start, f_end) if animations else None


def fbx_animations(scene_data):
    """
    Generate global animation data from objects.
    """
    scene = scene_data.scene
    animations = []
    animated = set()
    frame_start = 1e100
    frame_end = -1e100

    def add_anim(animations, animated, anim):
        nonlocal frame_start, frame_end
        if anim is not None:
            animations.append(anim)
            f_start, f_end = anim[4:6]
            if f_start < frame_start:
                frame_start = f_start
            if f_end > frame_end:
                frame_end = f_end

            _astack_key, astack, _alayer_key, _name, _fstart, _fend = anim
            for elem_key, (alayer_key, acurvenodes) in astack.items():
                for fbx_prop, (acurvenode_key, acurves, acurvenode_name) in acurvenodes.items():
                    animated.add((elem_key, fbx_prop))

    # Per-NLA strip animstacks.
    if scene_data.settings.bake_anim_use_nla_strips:
        strips = []
        ob_actions = []
        for ob_obj in scene_data.objects:
            # NLA tracks only for objects, not bones!
            if not ob_obj.is_object:
                continue
            ob = ob_obj.bdata  # Back to real Blender Object.
            if not ob.animation_data:
                continue
            # We have to remove active action from objects, it overwrites strips actions otherwise...
            ob_actions.append((ob, ob.animation_data.action))
            ob.animation_data.action = None
            for track in ob.animation_data.nla_tracks:
                if track.mute:
                    continue
                for strip in track.strips:
                    if strip.mute:
                        continue
                    strips.append(strip)
                    strip.mute = True

        for strip in strips:
            strip.mute = False
            add_anim(animations, animated,
                     fbx_animations_do(scene_data, strip, strip.frame_start, strip.frame_end, True, force_keep=True))
            strip.mute = True
            scene.frame_set(scene.frame_current, 0.0)

        for strip in strips:
            strip.mute = False

        for ob, ob_act in ob_actions:
            ob.animation_data.action = ob_act

    # All actions.
    if scene_data.settings.bake_anim_use_all_actions:
        def validate_actions(act, path_resolve):
            for fc in act.fcurves:
                data_path = fc.data_path
                if fc.array_index:
                    data_path = data_path + "[%d]" % fc.array_index
                try:
                    path_resolve(data_path)
                except ValueError:
                    return False  # Invalid.
            return True  # Valid.

        def restore_object(ob_to, ob_from):
            # Restore org state of object (ugh :/ ).
            props = (
                'location', 'rotation_quaternion', 'rotation_axis_angle', 'rotation_euler', 'rotation_mode', 'scale',
                'delta_location', 'delta_rotation_euler', 'delta_rotation_quaternion', 'delta_scale',
                'lock_location', 'lock_rotation', 'lock_rotation_w', 'lock_rotations_4d', 'lock_scale',
                'tag', 'layers', 'select', 'track_axis', 'up_axis', 'active_material', 'active_material_index',
                'matrix_parent_inverse', 'empty_draw_type', 'empty_draw_size', 'empty_image_offset', 'pass_index',
                'color', 'hide', 'hide_select', 'hide_render', 'use_slow_parent', 'slow_parent_offset',
                'use_extra_recalc_object', 'use_extra_recalc_data', 'dupli_type', 'use_dupli_frames_speed',
                'use_dupli_vertices_rotation', 'use_dupli_faces_scale', 'dupli_faces_scale', 'dupli_group',
                'dupli_frames_start', 'dupli_frames_end', 'dupli_frames_on', 'dupli_frames_off',
                'draw_type', 'show_bounds', 'draw_bounds_type', 'show_name', 'show_axis', 'show_texture_space',
                'show_wire', 'show_all_edges', 'show_transparent', 'show_x_ray',
                'show_only_shape_key', 'use_shape_key_edit_mode', 'active_shape_key_index',
            )
            for p in props:
                if not ob_to.is_property_readonly(p):
                    setattr(ob_to, p, getattr(ob_from, p))

        for ob_obj in scene_data.objects:
            # Actions only for objects, not bones!
            if not ob_obj.is_object:
                continue

            ob = ob_obj.bdata  # Back to real Blender Object.

            if not ob.animation_data:
                continue  # Do not export animations for objects that are absolutely not animated, see T44386.

            if ob.animation_data.is_property_readonly('action'):
                continue  # Cannot re-assign 'active action' to this object (usually related to NLA usage, see T48089).

            # We can't play with animdata and actions and get back to org state easily.
            # So we have to add a temp copy of the object to the scene, animate it, and remove it... :/
            ob_copy = ob.copy()
            # Great, have to handle bones as well if needed...
            pbones_matrices = [pbo.matrix_basis.copy() for pbo in ob.pose.bones] if ob.type == 'ARMATURE' else ...

            org_act = ob.animation_data.action
            path_resolve = ob.path_resolve

            for act in bpy.data.actions:
                # For now, *all* paths in the action must be valid for the object, to validate the action.
                # Unless that action was already assigned to the object!
                if act != org_act and not validate_actions(act, path_resolve):
                    continue
                ob.animation_data.action = act
                frame_start, frame_end = act.frame_range  # sic!
                add_anim(animations, animated,
                         fbx_animations_do(scene_data, (ob, act), frame_start, frame_end, True,
                                           objects={ob_obj}, force_keep=True))
                # Ugly! :/
                if pbones_matrices is not ...:
                    for pbo, mat in zip(ob.pose.bones, pbones_matrices):
                        pbo.matrix_basis = mat.copy()
                ob.animation_data.action = org_act
                restore_object(ob, ob_copy)
                scene.frame_set(scene.frame_current, 0.0)

            if pbones_matrices is not ...:
                for pbo, mat in zip(ob.pose.bones, pbones_matrices):
                    pbo.matrix_basis = mat.copy()
            ob.animation_data.action = org_act

            bpy.data.objects.remove(ob_copy)
            scene.frame_set(scene.frame_current, 0.0)

    # Global (containing everything) animstack, only if not exporting NLA strips and/or all actions.
    if not scene_data.settings.bake_anim_use_nla_strips and not scene_data.settings.bake_anim_use_all_actions:
        add_anim(animations, animated, fbx_animations_do(scene_data, None, scene.frame_start, scene.frame_end, False))

    # Be sure to update all matrices back to org state!
    scene.frame_set(scene.frame_current, 0.0)

    return animations, animated, frame_start, frame_end


def fbx_data_from_scene(scene, settings):
    """
    Do some pre-processing over scene's data...
    """
    objtypes = settings.object_types
    dp_objtypes = objtypes - {'ARMATURE'}  # Armatures are not supported as dupli instances currently...
    perfmon = PerfMon()
    perfmon.level_up()

    # ##### Gathering data...

    perfmon.step("FBX export prepare: Wrapping Objects...")

    # This is rather simple for now, maybe we could end generating templates with most-used values
    # instead of default ones?
    objects = OrderedDict()  # Because we do not have any ordered set...
    for ob in settings.context_objects:
        if ob.type not in objtypes:
            continue
        ob_obj = ObjectWrapper(ob)
        objects[ob_obj] = None
        # Duplis...
        ob_obj.dupli_list_create(scene, 'RENDER')
        for dp_obj in ob_obj.dupli_list:
            if dp_obj.type not in dp_objtypes:
                continue
            objects[dp_obj] = None
        ob_obj.dupli_list_clear()

    perfmon.step("FBX export prepare: Wrapping Data (lamps, cameras, empties)...")

    data_lamps = OrderedDict((ob_obj.bdata.data, get_blenderID_key(ob_obj.bdata.data))
                             for ob_obj in objects if ob_obj.type == 'LAMP')
    # Unfortunately, FBX camera data contains object-level data (like position, orientation, etc.)...
    data_cameras = OrderedDict((ob_obj, get_blenderID_key(ob_obj.bdata.data))
                               for ob_obj in objects if ob_obj.type == 'CAMERA')
    # Yep! Contains nothing, but needed!
    data_empties = OrderedDict((ob_obj, get_blender_empty_key(ob_obj.bdata))
                               for ob_obj in objects if ob_obj.type == 'EMPTY')

    perfmon.step("FBX export prepare: Wrapping Meshes...")

    data_meshes = OrderedDict()
    for ob_obj in objects:
        if ob_obj.type not in BLENDER_OBJECT_TYPES_MESHLIKE:
            continue
        ob = ob_obj.bdata
        use_org_data = True
        org_ob_obj = None

        # Do not want to systematically recreate a new mesh for dupliobject instances, kind of break purpose of those.
        if ob_obj.is_dupli:
            org_ob_obj = ObjectWrapper(ob)  # We get the "real" object wrapper from that dupli instance.
            if org_ob_obj in data_meshes:
                data_meshes[ob_obj] = data_meshes[org_ob_obj]
                continue

        is_ob_material = any(ms.link == 'OBJECT' for ms in ob.material_slots)

        if settings.use_mesh_modifiers or ob.type in BLENDER_OTHER_OBJECT_TYPES or is_ob_material:
            # We cannot use default mesh in that case, or material would not be the right ones...
            use_org_data = not (is_ob_material or ob.type in BLENDER_OTHER_OBJECT_TYPES)
            tmp_mods = []
            if use_org_data and ob.type == 'MESH':
                # No need to create a new mesh in this case, if no modifier is active!
                for mod in ob.modifiers:
                    # For meshes, when armature export is enabled, disable Armature modifiers here!
                    if mod.type == 'ARMATURE' and 'ARMATURE' in settings.object_types:
                        tmp_mods.append((mod, mod.show_render))
                        mod.show_render = False
                    if mod.show_render:
                        use_org_data = False
            if not use_org_data:
                tmp_me = ob.to_mesh(scene, apply_modifiers=True,
                                    settings='RENDER' if settings.use_mesh_modifiers_render else 'PREVIEW')
                data_meshes[ob_obj] = (get_blenderID_key(tmp_me), tmp_me, True)
            # Re-enable temporary disabled modifiers.
            for mod, show_render in tmp_mods:
                mod.show_render = show_render
        if use_org_data:
            data_meshes[ob_obj] = (get_blenderID_key(ob.data), ob.data, False)

        # In case "real" source object of that dupli did not yet still existed in data_meshes, create it now!
        if org_ob_obj is not None:
            data_meshes[org_ob_obj] = data_meshes[ob_obj]

    perfmon.step("FBX export prepare: Wrapping ShapeKeys...")

    # ShapeKeys.
    data_deformers_shape = OrderedDict()
    geom_mat_co = settings.global_matrix if settings.bake_space_transform else None
    for me_key, me, _free in data_meshes.values():
        if not (me.shape_keys and len(me.shape_keys.key_blocks) > 1):  # We do not want basis-only relative skeys...
            continue
        if me in data_deformers_shape:
            continue

        shapes_key = get_blender_mesh_shape_key(me)
        # We gather all vcos first, since some skeys may be based on others...
        _cos = array.array(data_types.ARRAY_FLOAT64, (0.0,)) * len(me.vertices) * 3
        me.vertices.foreach_get("co", _cos)
        v_cos = tuple(vcos_transformed_gen(_cos, geom_mat_co))
        sk_cos = {}
        for shape in me.shape_keys.key_blocks[1:]:
            shape.data.foreach_get("co", _cos)
            sk_cos[shape] = tuple(vcos_transformed_gen(_cos, geom_mat_co))
        sk_base = me.shape_keys.key_blocks[0]

        for shape in me.shape_keys.key_blocks[1:]:
            # Only write vertices really different from org coordinates!
            # XXX FBX does not like empty shapes (makes Unity crash e.g.), so we have to do this here... :/
            shape_verts_co = []
            shape_verts_idx = []

            sv_cos = sk_cos[shape]
            ref_cos = v_cos if shape.relative_key == sk_base else sk_cos[shape.relative_key]
            for idx, (sv_co, ref_co) in enumerate(zip(sv_cos, ref_cos)):
                if similar_values_iter(sv_co, ref_co):
                    # Note: Maybe this is a bit too simplistic, should we use real shape base here? Though FBX does not
                    #       have this at all... Anyway, this should cover most common cases imho.
                    continue
                shape_verts_co.extend(Vector(sv_co) - Vector(ref_co))
                shape_verts_idx.append(idx)
            if not shape_verts_co:
                continue
            channel_key, geom_key = get_blender_mesh_shape_channel_key(me, shape)
            data = (channel_key, geom_key, shape_verts_co, shape_verts_idx)
            data_deformers_shape.setdefault(me, (me_key, shapes_key, OrderedDict()))[2][shape] = data

    perfmon.step("FBX export prepare: Wrapping Armatures...")

    # Armatures!
    data_deformers_skin = OrderedDict()
    data_bones = OrderedDict()
    arm_parents = set()
    for ob_obj in tuple(objects):
        if not (ob_obj.is_object and ob_obj.type in {'ARMATURE'}):
            continue
        fbx_skeleton_from_armature(scene, settings, ob_obj, objects, data_meshes,
                                   data_bones, data_deformers_skin, data_empties, arm_parents)

    # Generate leaf bones
    data_leaf_bones = []
    if settings.add_leaf_bones:
        data_leaf_bones = fbx_generate_leaf_bones(settings, data_bones)

    perfmon.step("FBX export prepare: Wrapping World...")

    # Some world settings are embedded in FBX materials...
    if scene.world:
        data_world = OrderedDict(((scene.world, get_blenderID_key(scene.world)),))
    else:
        data_world = OrderedDict()

    perfmon.step("FBX export prepare: Wrapping Materials...")

    # TODO: Check all the mat stuff works even when mats are linked to Objects
    #       (we can then have the same mesh used with different materials...).
    #       *Should* work, as FBX always links its materials to Models (i.e. objects).
    #       XXX However, material indices would probably break...
    data_materials = OrderedDict()
    for ob_obj in objects:
        # If obj is not a valid object for materials, wrapper will just return an empty tuple...
        for mat_s in ob_obj.material_slots:
            mat = mat_s.material
            if mat is None:
                continue  # Empty slots!
            # Note theoretically, FBX supports any kind of materials, even GLSL shaders etc.
            # However, I doubt anything else than Lambert/Phong is really portable!
            # We support any kind of 'surface' shader though, better to have some kind of default Lambert than nothing.
            # Note we want to keep a 'dummy' empty mat even when we can't really support it, see T41396.
            mat_data = data_materials.get(mat)
            if mat_data is not None:
                mat_data[1].append(ob_obj)
            else:
                data_materials[mat] = (get_blenderID_key(mat), [ob_obj])

    perfmon.step("FBX export prepare: Wrapping Textures...")

    # Note FBX textures also hold their mapping info.
    # TODO: Support layers?
    data_textures = OrderedDict()
    # FbxVideo also used to store static images...
    data_videos = OrderedDict()
    # For now, do not use world textures, don't think they can be linked to anything FBX wise...
    for mat in data_materials.keys():
        if check_skip_material(mat):
            continue
        for tex, use_tex in zip(mat.texture_slots, mat.use_textures):
            if tex is None or tex.texture is None or not use_tex:
                continue
            # For now, only consider image textures.
            # Note FBX does has support for procedural, but this is not portable at all (opaque blob),
            # so not useful for us.
            # TODO I think ENVIRONMENT_MAP should be usable in FBX as well, but for now let it aside.
            # if tex.texture.type not in {'IMAGE', 'ENVIRONMENT_MAP'}:
            if tex.texture.type not in {'IMAGE'}:
                continue
            img = tex.texture.image
            if img is None:
                continue
            # Find out whether we can actually use this texture for this material, in FBX context.
            tex_fbx_props = fbx_mat_properties_from_texture(tex)
            if not tex_fbx_props:
                continue
            tex_data = data_textures.get(tex)
            if tex_data is not None:
                tex_data[1][mat] = tex_fbx_props
            else:
                data_textures[tex] = (get_blenderID_key(tex), OrderedDict(((mat, tex_fbx_props),)))
            vid_data = data_videos.get(img)
            if vid_data is not None:
                vid_data[1].append(tex)
            else:
                data_videos[img] = (get_blenderID_key(img), [tex])

    perfmon.step("FBX export prepare: Wrapping Animations...")

    # Animation...
    animations = ()
    animated = set()
    frame_start = scene.frame_start
    frame_end = scene.frame_end
    if settings.bake_anim:
        # From objects & bones only for a start.
        # Kind of hack, we need a temp scene_data for object's space handling to bake animations...
        tmp_scdata = FBXExportData(
            None, None, None,
            settings, scene, objects, None, None, 0.0, 0.0,
            data_empties, data_lamps, data_cameras, data_meshes, None,
            data_bones, data_leaf_bones, data_deformers_skin, data_deformers_shape,
            data_world, data_materials, data_textures, data_videos,
        )
        animations, animated, frame_start, frame_end = fbx_animations(tmp_scdata)

    # ##### Creation of templates...

    perfmon.step("FBX export prepare: Generating templates...")

    templates = OrderedDict()
    templates[b"GlobalSettings"] = fbx_template_def_globalsettings(scene, settings, nbr_users=1)

    if data_empties:
        templates[b"Null"] = fbx_template_def_null(scene, settings, nbr_users=len(data_empties))

    if data_lamps:
        templates[b"Light"] = fbx_template_def_light(scene, settings, nbr_users=len(data_lamps))

    if data_cameras:
        templates[b"Camera"] = fbx_template_def_camera(scene, settings, nbr_users=len(data_cameras))

    if data_bones:
        templates[b"Bone"] = fbx_template_def_bone(scene, settings, nbr_users=len(data_bones))

    if data_meshes:
        nbr = len({me_key for me_key, _me, _free in data_meshes.values()})
        if data_deformers_shape:
            nbr += sum(len(shapes[2]) for shapes in data_deformers_shape.values())
        templates[b"Geometry"] = fbx_template_def_geometry(scene, settings, nbr_users=nbr)

    if objects:
        templates[b"Model"] = fbx_template_def_model(scene, settings, nbr_users=len(objects))

    if arm_parents:
        # Number of Pose|BindPose elements should be the same as number of meshes-parented-to-armatures
        templates[b"BindPose"] = fbx_template_def_pose(scene, settings, nbr_users=len(arm_parents))

    if data_deformers_skin or data_deformers_shape:
        nbr = 0
        if data_deformers_skin:
            nbr += len(data_deformers_skin)
            nbr += sum(len(clusters) for def_me in data_deformers_skin.values() for a, b, clusters in def_me.values())
        if data_deformers_shape:
            nbr += len(data_deformers_shape)
            nbr += sum(len(shapes[2]) for shapes in data_deformers_shape.values())
        assert(nbr != 0)
        templates[b"Deformers"] = fbx_template_def_deformer(scene, settings, nbr_users=nbr)

    # No world support in FBX...
    """
    if data_world:
        templates[b"World"] = fbx_template_def_world(scene, settings, nbr_users=len(data_world))
    """

    if data_materials:
        templates[b"Material"] = fbx_template_def_material(scene, settings, nbr_users=len(data_materials))

    if data_textures:
        templates[b"TextureFile"] = fbx_template_def_texture_file(scene, settings, nbr_users=len(data_textures))

    if data_videos:
        templates[b"Video"] = fbx_template_def_video(scene, settings, nbr_users=len(data_videos))

    if animations:
        nbr_astacks = len(animations)
        nbr_acnodes = 0
        nbr_acurves = 0
        for _astack_key, astack, _al, _n, _fs, _fe in animations:
            for _alayer_key, alayer in astack.values():
                for _acnode_key, acnode, _acnode_name in alayer.values():
                    nbr_acnodes += 1
                    for _acurve_key, _dval, acurve, acurve_valid in acnode.values():
                        if acurve:
                            nbr_acurves += 1

        templates[b"AnimationStack"] = fbx_template_def_animstack(scene, settings, nbr_users=nbr_astacks)
        # Would be nice to have one layer per animated object, but this seems tricky and not that well supported.
        # So for now, only one layer per anim stack.
        templates[b"AnimationLayer"] = fbx_template_def_animlayer(scene, settings, nbr_users=nbr_astacks)
        templates[b"AnimationCurveNode"] = fbx_template_def_animcurvenode(scene, settings, nbr_users=nbr_acnodes)
        templates[b"AnimationCurve"] = fbx_template_def_animcurve(scene, settings, nbr_users=nbr_acurves)

    templates_users = sum(tmpl.nbr_users for tmpl in templates.values())

    # ##### Creation of connections...

    perfmon.step("FBX export prepare: Generating Connections...")

    connections = []

    # Objects (with classical parenting).
    for ob_obj in objects:
        # Bones are handled later.
        if not ob_obj.is_bone:
            par_obj = ob_obj.parent
            # Meshes parented to armature are handled separately, yet we want the 'no parent' connection (0).
            if par_obj and ob_obj.has_valid_parent(objects) and (par_obj, ob_obj) not in arm_parents:
                connections.append((b"OO", ob_obj.fbx_uuid, par_obj.fbx_uuid, None))
            else:
                connections.append((b"OO", ob_obj.fbx_uuid, 0, None))

    # Armature & Bone chains.
    for bo_obj in data_bones.keys():
        par_obj = bo_obj.parent
        if par_obj not in objects:
            continue
        connections.append((b"OO", bo_obj.fbx_uuid, par_obj.fbx_uuid, None))

    # Object data.
    for ob_obj in objects:
        if ob_obj.is_bone:
            bo_data_key = data_bones[ob_obj]
            connections.append((b"OO", get_fbx_uuid_from_key(bo_data_key), ob_obj.fbx_uuid, None))
        else:
            if ob_obj.type == 'LAMP':
                lamp_key = data_lamps[ob_obj.bdata.data]
                connections.append((b"OO", get_fbx_uuid_from_key(lamp_key), ob_obj.fbx_uuid, None))
            elif ob_obj.type == 'CAMERA':
                cam_key = data_cameras[ob_obj]
                connections.append((b"OO", get_fbx_uuid_from_key(cam_key), ob_obj.fbx_uuid, None))
            elif ob_obj.type == 'EMPTY' or ob_obj.type == 'ARMATURE':
                empty_key = data_empties[ob_obj]
                connections.append((b"OO", get_fbx_uuid_from_key(empty_key), ob_obj.fbx_uuid, None))
            elif ob_obj.type in BLENDER_OBJECT_TYPES_MESHLIKE:
                mesh_key, _me, _free = data_meshes[ob_obj]
                connections.append((b"OO", get_fbx_uuid_from_key(mesh_key), ob_obj.fbx_uuid, None))

    # Leaf Bones
    for (_node_name, par_uuid, node_uuid, attr_uuid, _matrix, _hide, _size) in data_leaf_bones:
        connections.append((b"OO", node_uuid, par_uuid, None))
        connections.append((b"OO", attr_uuid, node_uuid, None))

    # 'Shape' deformers (shape keys, only for meshes currently)...
    for me_key, shapes_key, shapes in data_deformers_shape.values():
        # shape -> geometry
        connections.append((b"OO", get_fbx_uuid_from_key(shapes_key), get_fbx_uuid_from_key(me_key), None))
        for channel_key, geom_key, _shape_verts_co, _shape_verts_idx in shapes.values():
            # shape channel -> shape
            connections.append((b"OO", get_fbx_uuid_from_key(channel_key), get_fbx_uuid_from_key(shapes_key), None))
            # geometry (keys) -> shape channel
            connections.append((b"OO", get_fbx_uuid_from_key(geom_key), get_fbx_uuid_from_key(channel_key), None))

    # 'Skin' deformers (armature-to-geometry, only for meshes currently)...
    for arm, deformed_meshes in data_deformers_skin.items():
        for me, (skin_key, ob_obj, clusters) in deformed_meshes.items():
            # skin -> geometry
            mesh_key, _me, _free = data_meshes[ob_obj]
            assert(me == _me)
            connections.append((b"OO", get_fbx_uuid_from_key(skin_key), get_fbx_uuid_from_key(mesh_key), None))
            for bo_obj, clstr_key in clusters.items():
                # cluster -> skin
                connections.append((b"OO", get_fbx_uuid_from_key(clstr_key), get_fbx_uuid_from_key(skin_key), None))
                # bone -> cluster
                connections.append((b"OO", bo_obj.fbx_uuid, get_fbx_uuid_from_key(clstr_key), None))

    # Materials
    mesh_mat_indices = OrderedDict()
    _objs_indices = {}
    for mat, (mat_key, ob_objs) in data_materials.items():
        for ob_obj in ob_objs:
            connections.append((b"OO", get_fbx_uuid_from_key(mat_key), ob_obj.fbx_uuid, None))
            # Get index of this mat for this object (or dupliobject).
            # Mat indices for mesh faces are determined by their order in 'mat to ob' connections.
            # Only mats for meshes currently...
            # Note in case of dupliobjects a same me/mat idx will be generated several times...
            # Should not be an issue in practice, and it's needed in case we export duplis but not the original!
            if ob_obj.type not in BLENDER_OBJECT_TYPES_MESHLIKE:
                continue
            _mesh_key, me, _free = data_meshes[ob_obj]
            idx = _objs_indices[ob_obj] = _objs_indices.get(ob_obj, -1) + 1
            mesh_mat_indices.setdefault(me, OrderedDict())[mat] = idx
    del _objs_indices

    # Textures
    for tex, (tex_key, mats) in data_textures.items():
        for mat, fbx_mat_props in mats.items():
            mat_key, _ob_objs = data_materials[mat]
            for fbx_prop in fbx_mat_props:
                # texture -> material properties
                connections.append((b"OP", get_fbx_uuid_from_key(tex_key), get_fbx_uuid_from_key(mat_key), fbx_prop))

    # Images
    for vid, (vid_key, texs) in data_videos.items():
        for tex in texs:
            tex_key, _texs = data_textures[tex]
            connections.append((b"OO", get_fbx_uuid_from_key(vid_key), get_fbx_uuid_from_key(tex_key), None))

    # Animations
    for astack_key, astack, alayer_key, _name, _fstart, _fend in animations:
        # Animstack itself is linked nowhere!
        astack_id = get_fbx_uuid_from_key(astack_key)
        # For now, only one layer!
        alayer_id = get_fbx_uuid_from_key(alayer_key)
        connections.append((b"OO", alayer_id, astack_id, None))
        for elem_key, (alayer_key, acurvenodes) in astack.items():
            elem_id = get_fbx_uuid_from_key(elem_key)
            # Animlayer -> animstack.
            # alayer_id = get_fbx_uuid_from_key(alayer_key)
            # connections.append((b"OO", alayer_id, astack_id, None))
            for fbx_prop, (acurvenode_key, acurves, acurvenode_name) in acurvenodes.items():
                # Animcurvenode -> animalayer.
                acurvenode_id = get_fbx_uuid_from_key(acurvenode_key)
                connections.append((b"OO", acurvenode_id, alayer_id, None))
                # Animcurvenode -> object property.
                connections.append((b"OP", acurvenode_id, elem_id, fbx_prop.encode()))
                for fbx_item, (acurve_key, default_value, acurve, acurve_valid) in acurves.items():
                    if acurve:
                        # Animcurve -> Animcurvenode.
                        connections.append((b"OP", get_fbx_uuid_from_key(acurve_key), acurvenode_id, fbx_item.encode()))

    perfmon.level_down()

    # ##### And pack all this!

    return FBXExportData(
        templates, templates_users, connections,
        settings, scene, objects, animations, animated, frame_start, frame_end,
        data_empties, data_lamps, data_cameras, data_meshes, mesh_mat_indices,
        data_bones, data_leaf_bones, data_deformers_skin, data_deformers_shape,
        data_world, data_materials, data_textures, data_videos,
    )


def fbx_scene_data_cleanup(scene_data):
    """
    Some final cleanup...
    """
    # Delete temp meshes.
    done_meshes = set()
    for me_key, me, free in scene_data.data_meshes.values():
        if free and me_key not in done_meshes:
            bpy.data.meshes.remove(me)
            done_meshes.add(me_key)


# ##### Top-level FBX elements generators. #####

def fbx_header_elements(root, scene_data, time=None):
    """
    Write boiling code of FBX root.
    time is expected to be a datetime.datetime object, or None (using now() in this case).
    """
    app_vendor = "Blender Foundation"
    app_name = "Blender (stable FBX IO)"
    app_ver = bpy.app.version_string

    import addon_utils
    import sys
    addon_ver = addon_utils.module_bl_info(sys.modules[__package__])['version']

    # ##### Start of FBXHeaderExtension element.
    header_ext = elem_empty(root, b"FBXHeaderExtension")

    elem_data_single_int32(header_ext, b"FBXHeaderVersion", FBX_HEADER_VERSION)

    elem_data_single_int32(header_ext, b"FBXVersion", FBX_VERSION)

    # No encryption!
    elem_data_single_int32(header_ext, b"EncryptionType", 0)

    if time is None:
        time = datetime.datetime.now()
    elem = elem_empty(header_ext, b"CreationTimeStamp")
    elem_data_single_int32(elem, b"Version", 1000)
    elem_data_single_int32(elem, b"Year", time.year)
    elem_data_single_int32(elem, b"Month", time.month)
    elem_data_single_int32(elem, b"Day", time.day)
    elem_data_single_int32(elem, b"Hour", time.hour)
    elem_data_single_int32(elem, b"Minute", time.minute)
    elem_data_single_int32(elem, b"Second", time.second)
    elem_data_single_int32(elem, b"Millisecond", time.microsecond // 1000)

    elem_data_single_string_unicode(header_ext, b"Creator", "%s - %s - %d.%d.%d"
                                                % (app_name, app_ver, addon_ver[0], addon_ver[1], addon_ver[2]))

    # 'SceneInfo' seems mandatory to get a valid FBX file...
    # TODO use real values!
    # XXX Should we use scene.name.encode() here?
    scene_info = elem_data_single_string(header_ext, b"SceneInfo", fbx_name_class(b"GlobalInfo", b"SceneInfo"))
    scene_info.add_string(b"UserData")
    elem_data_single_string(scene_info, b"Type", b"UserData")
    elem_data_single_int32(scene_info, b"Version", FBX_SCENEINFO_VERSION)
    meta_data = elem_empty(scene_info, b"MetaData")
    elem_data_single_int32(meta_data, b"Version", FBX_SCENEINFO_VERSION)
    elem_data_single_string(meta_data, b"Title", b"")
    elem_data_single_string(meta_data, b"Subject", b"")
    elem_data_single_string(meta_data, b"Author", b"")
    elem_data_single_string(meta_data, b"Keywords", b"")
    elem_data_single_string(meta_data, b"Revision", b"")
    elem_data_single_string(meta_data, b"Comment", b"")

    props = elem_properties(scene_info)
    elem_props_set(props, "p_string_url", b"DocumentUrl", "/foobar.fbx")
    elem_props_set(props, "p_string_url", b"SrcDocumentUrl", "/foobar.fbx")
    original = elem_props_compound(props, b"Original")
    original("p_string", b"ApplicationVendor", app_vendor)
    original("p_string", b"ApplicationName", app_name)
    original("p_string", b"ApplicationVersion", app_ver)
    original("p_datetime", b"DateTime_GMT", "01/01/1970 00:00:00.000")
    original("p_string", b"FileName", "/foobar.fbx")
    lastsaved = elem_props_compound(props, b"LastSaved")
    lastsaved("p_string", b"ApplicationVendor", app_vendor)
    lastsaved("p_string", b"ApplicationName", app_name)
    lastsaved("p_string", b"ApplicationVersion", app_ver)
    lastsaved("p_datetime", b"DateTime_GMT", "01/01/1970 00:00:00.000")

    # ##### End of FBXHeaderExtension element.

    # FileID is replaced by dummy value currently...
    elem_data_single_bytes(root, b"FileId", b"FooBar")

    # CreationTime is replaced by dummy value currently, but anyway...
    elem_data_single_string_unicode(root, b"CreationTime",
                                    "{:04}-{:02}-{:02} {:02}:{:02}:{:02}:{:03}"
                                    "".format(time.year, time.month, time.day, time.hour, time.minute, time.second,
                                              time.microsecond * 1000))

    elem_data_single_string_unicode(root, b"Creator", "%s - %s - %d.%d.%d"
                                          % (app_name, app_ver, addon_ver[0], addon_ver[1], addon_ver[2]))

    # ##### Start of GlobalSettings element.
    global_settings = elem_empty(root, b"GlobalSettings")
    scene = scene_data.scene

    elem_data_single_int32(global_settings, b"Version", 1000)

    props = elem_properties(global_settings)
    up_axis, front_axis, coord_axis = RIGHT_HAND_AXES[scene_data.settings.to_axes]
    #~ # DO NOT take into account global scale here! That setting is applied to object transformations during export
    #~ # (in other words, this is pure blender-exporter feature, and has nothing to do with FBX data).
    #~ if scene_data.settings.apply_unit_scale:
        #~ # Unit scaling is applied to objects' scale, so our unit is effectively FBX one (centimeter).
        #~ scale_factor_org = 1.0
        #~ scale_factor = 1.0 / units_blender_to_fbx_factor(scene)
    #~ else:
        #~ scale_factor_org = units_blender_to_fbx_factor(scene)
        #~ scale_factor = scale_factor_org
    scale_factor = scale_factor_org = scene_data.settings.unit_scale
    elem_props_set(props, "p_integer", b"UpAxis", up_axis[0])
    elem_props_set(props, "p_integer", b"UpAxisSign", up_axis[1])
    elem_props_set(props, "p_integer", b"FrontAxis", front_axis[0])
    elem_props_set(props, "p_integer", b"FrontAxisSign", front_axis[1])
    elem_props_set(props, "p_integer", b"CoordAxis", coord_axis[0])
    elem_props_set(props, "p_integer", b"CoordAxisSign", coord_axis[1])
    elem_props_set(props, "p_integer", b"OriginalUpAxis", -1)
    elem_props_set(props, "p_integer", b"OriginalUpAxisSign", 1)
    elem_props_set(props, "p_double", b"UnitScaleFactor", scale_factor)
    elem_props_set(props, "p_double", b"OriginalUnitScaleFactor", scale_factor_org)
    elem_props_set(props, "p_color_rgb", b"AmbientColor", (0.0, 0.0, 0.0))
    elem_props_set(props, "p_string", b"DefaultCamera", "Producer Perspective")

    # Global timing data.
    r = scene.render
    _, fbx_fps_mode = FBX_FRAMERATES[0]  # Custom framerate.
    fbx_fps = fps = r.fps / r.fps_base
    for ref_fps, fps_mode in FBX_FRAMERATES:
        if similar_values(fps, ref_fps):
            fbx_fps = ref_fps
            fbx_fps_mode = fps_mode
    elem_props_set(props, "p_enum", b"TimeMode", fbx_fps_mode)
    elem_props_set(props, "p_timestamp", b"TimeSpanStart", 0)
    elem_props_set(props, "p_timestamp", b"TimeSpanStop", FBX_KTIME)
    elem_props_set(props, "p_double", b"CustomFrameRate", fbx_fps)

    # ##### End of GlobalSettings element.


def fbx_documents_elements(root, scene_data):
    """
    Write 'Document' part of FBX root.
    Seems like FBX support multiple documents, but until I find examples of such, we'll stick to single doc!
    time is expected to be a datetime.datetime object, or None (using now() in this case).
    """
    name = scene_data.scene.name

    # ##### Start of Documents element.
    docs = elem_empty(root, b"Documents")

    elem_data_single_int32(docs, b"Count", 1)

    doc_uid = get_fbx_uuid_from_key("__FBX_Document__" + name)
    doc = elem_data_single_int64(docs, b"Document", doc_uid)
    doc.add_string_unicode(name)
    doc.add_string_unicode(name)

    props = elem_properties(doc)
    elem_props_set(props, "p_object", b"SourceObject")
    elem_props_set(props, "p_string", b"ActiveAnimStackName", "")

    # XXX Some kind of ID? Offset?
    #     Anyway, as long as we have only one doc, probably not an issue.
    elem_data_single_int64(doc, b"RootNode", 0)


def fbx_references_elements(root, scene_data):
    """
    Have no idea what references are in FBX currently... Just writing empty element.
    """
    docs = elem_empty(root, b"References")


def fbx_definitions_elements(root, scene_data):
    """
    Templates definitions. Only used by Objects data afaik (apart from dummy GlobalSettings one).
    """
    definitions = elem_empty(root, b"Definitions")

    elem_data_single_int32(definitions, b"Version", FBX_TEMPLATES_VERSION)
    elem_data_single_int32(definitions, b"Count", scene_data.templates_users)

    fbx_templates_generate(definitions, scene_data.templates)


def fbx_objects_elements(root, scene_data):
    """
    Data (objects, geometry, material, textures, armatures, etc.).
    """
    perfmon = PerfMon()
    perfmon.level_up()
    objects = elem_empty(root, b"Objects")

    perfmon.step("FBX export fetch empties (%d)..." % len(scene_data.data_empties))

    for empty in scene_data.data_empties:
        fbx_data_empty_elements(objects, empty, scene_data)

    perfmon.step("FBX export fetch lamps (%d)..." % len(scene_data.data_lamps))

    for lamp in scene_data.data_lamps:
        fbx_data_lamp_elements(objects, lamp, scene_data)

    perfmon.step("FBX export fetch cameras (%d)..." % len(scene_data.data_cameras))

    for cam in scene_data.data_cameras:
        fbx_data_camera_elements(objects, cam, scene_data)

    perfmon.step("FBX export fetch meshes (%d)..."
                 % len({me_key for me_key, _me, _free in scene_data.data_meshes.values()}))

    done_meshes = set()
    for me_obj in scene_data.data_meshes:
        fbx_data_mesh_elements(objects, me_obj, scene_data, done_meshes)
    del done_meshes

    perfmon.step("FBX export fetch objects (%d)..." % len(scene_data.objects))

    for ob_obj in scene_data.objects:
        if ob_obj.is_dupli:
            continue
        fbx_data_object_elements(objects, ob_obj, scene_data)
        ob_obj.dupli_list_create(scene_data.scene, 'RENDER')
        for dp_obj in ob_obj.dupli_list:
            if dp_obj not in scene_data.objects:
                continue
            fbx_data_object_elements(objects, dp_obj, scene_data)
        ob_obj.dupli_list_clear()

    perfmon.step("FBX export fetch remaining...")

    for ob_obj in scene_data.objects:
        if not (ob_obj.is_object and ob_obj.type == 'ARMATURE'):
            continue
        fbx_data_armature_elements(objects, ob_obj, scene_data)

    if scene_data.data_leaf_bones:
        fbx_data_leaf_bone_elements(objects, scene_data)

    for mat in scene_data.data_materials:
        fbx_data_material_elements(objects, mat, scene_data)

    for tex in scene_data.data_textures:
        fbx_data_texture_file_elements(objects, tex, scene_data)

    for vid in scene_data.data_videos:
        fbx_data_video_elements(objects, vid, scene_data)

    perfmon.step("FBX export fetch animations...")
    start_time = time.process_time()

    fbx_data_animation_elements(objects, scene_data)

    perfmon.level_down()


def fbx_connections_elements(root, scene_data):
    """
    Relations between Objects (which material uses which texture, and so on).
    """
    connections = elem_empty(root, b"Connections")

    for c in scene_data.connections:
        elem_connection(connections, *c)


def fbx_takes_elements(root, scene_data):
    """
    Animations.
    """
    # XXX Pretty sure takes are no more needed...
    takes = elem_empty(root, b"Takes")
    elem_data_single_string(takes, b"Current", b"")

    animations = scene_data.animations
    for astack_key, animations, alayer_key, name, f_start, f_end in animations:
        scene = scene_data.scene
        fps = scene.render.fps / scene.render.fps_base
        start_ktime = int(convert_sec_to_ktime(f_start / fps))
        end_ktime = int(convert_sec_to_ktime(f_end / fps))

        take = elem_data_single_string(takes, b"Take", name)
        elem_data_single_string(take, b"FileName", name + b".tak")
        take_loc_time = elem_data_single_int64(take, b"LocalTime", start_ktime)
        take_loc_time.add_int64(end_ktime)
        take_ref_time = elem_data_single_int64(take, b"ReferenceTime", start_ktime)
        take_ref_time.add_int64(end_ktime)


# ##### "Main" functions. #####

# This func can be called with just the filepath
def save_single(operator, scene, filepath="",
                global_matrix=Matrix(),
                apply_unit_scale=False,
                global_scale=1.0,
                apply_scale_options='FBX_SCALE_NONE',
                axis_up="Z",
                axis_forward="Y",
                context_objects=None,
                object_types=None,
                use_mesh_modifiers=True,
                use_mesh_modifiers_render=True,
                mesh_smooth_type='FACE',
                use_armature_deform_only=False,
                bake_anim=True,
                bake_anim_use_all_bones=True,
                bake_anim_use_nla_strips=True,
                bake_anim_use_all_actions=True,
                bake_anim_step=1.0,
                bake_anim_simplify_factor=1.0,
                bake_anim_force_startend_keying=True,
                add_leaf_bones=False,
                primary_bone_axis='Y',
                secondary_bone_axis='X',
                use_metadata=True,
                path_mode='AUTO',
                use_mesh_edges=True,
                use_tspace=True,
                embed_textures=False,
                use_custom_props=False,
                bake_space_transform=False,
                armature_nodetype='NULL',
                **kwargs
                ):

    # Clear cached ObjectWrappers (just in case...).
    ObjectWrapper.cache_clear()

    if object_types is None:
        object_types = {'EMPTY', 'CAMERA', 'LAMP', 'ARMATURE', 'MESH', 'OTHER'}

    if 'OTHER' in object_types:
        object_types |= BLENDER_OTHER_OBJECT_TYPES

    # Default Blender unit is equivalent to meter, while FBX one is centimeter...
    unit_scale = units_blender_to_fbx_factor(scene) if apply_unit_scale else 100.0
    if apply_scale_options == 'FBX_SCALE_NONE':
        global_matrix = Matrix.Scale(unit_scale * global_scale, 4) * global_matrix
        unit_scale = 1.0
    elif apply_scale_options == 'FBX_SCALE_UNITS':
        global_matrix = Matrix.Scale(global_scale, 4) * global_matrix
    elif apply_scale_options == 'FBX_SCALE_CUSTOM':
        global_matrix = Matrix.Scale(unit_scale, 4) * global_matrix
        unit_scale = global_scale
    else: # if apply_scale_options == 'FBX_SCALE_ALL':
        unit_scale = global_scale * unit_scale

    global_scale = global_matrix.median_scale
    global_matrix_inv = global_matrix.inverted()
    # For transforming mesh normals.
    global_matrix_inv_transposed = global_matrix_inv.transposed()

    # Only embed textures in COPY mode!
    if embed_textures and path_mode != 'COPY':
        embed_textures = False

    # Calcuate bone correction matrix
    bone_correction_matrix = None  # Default is None = no change
    bone_correction_matrix_inv = None
    if (primary_bone_axis, secondary_bone_axis) != ('Y', 'X'):
        from bpy_extras.io_utils import axis_conversion
        bone_correction_matrix = axis_conversion(from_forward=secondary_bone_axis,
                                                 from_up=primary_bone_axis,
                                                 to_forward='X',
                                                 to_up='Y',
                                                 ).to_4x4()
        bone_correction_matrix_inv = bone_correction_matrix.inverted()


    media_settings = FBXExportSettingsMedia(
        path_mode,
        os.path.dirname(bpy.data.filepath),  # base_src
        os.path.dirname(filepath),  # base_dst
        # Local dir where to put images (medias), using FBX conventions.
        os.path.splitext(os.path.basename(filepath))[0] + ".fbm",  # subdir
        embed_textures,
        set(),  # copy_set
        set(),  # embedded_set
    )

    settings = FBXExportSettings(
        operator.report, (axis_up, axis_forward), global_matrix, global_scale, apply_unit_scale, unit_scale,
        bake_space_transform, global_matrix_inv, global_matrix_inv_transposed,
        context_objects, object_types, use_mesh_modifiers, use_mesh_modifiers_render,
        mesh_smooth_type, use_mesh_edges, use_tspace,
        armature_nodetype, use_armature_deform_only,
        add_leaf_bones, bone_correction_matrix, bone_correction_matrix_inv,
        bake_anim, bake_anim_use_all_bones, bake_anim_use_nla_strips, bake_anim_use_all_actions,
        bake_anim_step, bake_anim_simplify_factor, bake_anim_force_startend_keying,
        False, media_settings, use_custom_props,
    )

    import bpy_extras.io_utils

    print('\nFBX export starting... %r' % filepath)
    start_time = time.process_time()

    # Generate some data about exported scene...
    scene_data = fbx_data_from_scene(scene, settings)

    root = elem_empty(None, b"")  # Root element has no id, as it is not saved per se!

    # Mostly FBXHeaderExtension and GlobalSettings.
    fbx_header_elements(root, scene_data)

    # Documents and References are pretty much void currently.
    fbx_documents_elements(root, scene_data)
    fbx_references_elements(root, scene_data)

    # Templates definitions.
    fbx_definitions_elements(root, scene_data)

    # Actual data.
    fbx_objects_elements(root, scene_data)

    # How data are inter-connected.
    fbx_connections_elements(root, scene_data)

    # Animation.
    fbx_takes_elements(root, scene_data)

    # Cleanup!
    fbx_scene_data_cleanup(scene_data)

    # And we are down, we can write the whole thing!
    encode_bin.write(filepath, root, FBX_VERSION)

    # Clear cached ObjectWrappers!
    ObjectWrapper.cache_clear()

    # copy all collected files, if we did not embed them.
    if not media_settings.embed_textures:
        bpy_extras.io_utils.path_reference_copy(media_settings.copy_set)

    print('export finished in %.4f sec.' % (time.process_time() - start_time))
    return {'FINISHED'}


# defaults for applications, currently only unity but could add others.
def defaults_unity3d():
    return {
        # These options seem to produce the same result as the old Ascii exporter in Unity3D:
        "version": 'BIN7400',
        "axis_up": 'Y',
        "axis_forward": '-Z',
        "global_matrix": Matrix.Rotation(-math.pi / 2.0, 4, 'X'),
        # Should really be True, but it can cause problems if a model is already in a scene or prefab
        # with the old transforms.
        "bake_space_transform": False,

        "use_selection": False,

        "object_types": {'ARMATURE', 'EMPTY', 'MESH', 'OTHER'},
        "use_mesh_modifiers": True,
        "use_mesh_modifiers_render": True,
        "use_mesh_edges": False,
        "mesh_smooth_type": 'FACE',
        "use_tspace": False,  # XXX Why? Unity is expected to support tspace import...

        "use_armature_deform_only": True,

        "use_custom_props": True,

        "bake_anim": True,
        "bake_anim_simplify_factor": 1.0,
        "bake_anim_step": 1.0,
        "bake_anim_use_nla_strips": True,
        "bake_anim_use_all_actions": True,
        "add_leaf_bones": False,  # Avoid memory/performance cost for something only useful for modelling
        "primary_bone_axis": 'Y',  # Doesn't really matter for Unity, so leave unchanged
        "secondary_bone_axis": 'X',

        "path_mode": 'AUTO',
        "embed_textures": False,
        "batch_mode": 'OFF',
    }


def save(operator, context,
         filepath="",
         use_selection=False,
         batch_mode='OFF',
         use_batch_own_dir=False,
         **kwargs
         ):
    """
    This is a wrapper around save_single, which handles multi-scenes (or groups) cases, when batch-exporting a whole
    .blend file.
    """

    ret = None

    active_object = context.scene.objects.active

    org_mode = None
    if active_object and active_object.mode != 'OBJECT' and bpy.ops.object.mode_set.poll():
        org_mode = active_object.mode
        bpy.ops.object.mode_set(mode='OBJECT')

    if batch_mode == 'OFF':
        kwargs_mod = kwargs.copy()
        if use_selection:
            kwargs_mod["context_objects"] = context.selected_objects
        else:
            kwargs_mod["context_objects"] = context.scene.objects

        ret = save_single(operator, context.scene, filepath, **kwargs_mod)
    else:
        fbxpath = filepath

        prefix = os.path.basename(fbxpath)
        if prefix:
            fbxpath = os.path.dirname(fbxpath)

        if batch_mode == 'GROUP':
            data_seq = tuple(grp for grp in bpy.data.groups if grp.objects)
        else:
            data_seq = bpy.data.scenes

        # call this function within a loop with BATCH_ENABLE == False
        # no scene switching done at the moment.
        # orig_sce = context.scene

        new_fbxpath = fbxpath  # own dir option modifies, we need to keep an original
        for data in data_seq:  # scene or group
            newname = "_".join((prefix, bpy.path.clean_name(data.name))) if prefix else bpy.path.clean_name(data.name)

            if use_batch_own_dir:
                new_fbxpath = os.path.join(fbxpath, newname)
                # path may already exist
                # TODO - might exist but be a file. unlikely but should probably account for it.

                if not os.path.exists(new_fbxpath):
                    os.makedirs(new_fbxpath)

            filepath = os.path.join(new_fbxpath, newname + '.fbx')

            print('\nBatch exporting %s as...\n\t%r' % (data, filepath))

            if batch_mode == 'GROUP':  # group
                # group, so objects update properly, add a dummy scene.
                scene = bpy.data.scenes.new(name="FBX_Temp")
                scene.layers = [True] * 20
                # bpy.data.scenes.active = scene # XXX, cant switch
                src_scenes = {}  # Count how much each 'source' scenes are used.
                for ob_base in data.objects:
                    for src_sce in ob_base.users_scene:
                        if src_sce not in src_scenes:
                            src_scenes[src_sce] = 0
                        src_scenes[src_sce] += 1
                    scene.objects.link(ob_base)

                # Find the 'most used' source scene, and use its unit settings. This is somewhat weak, but should work
                # fine in most cases, and avoids stupid issues like T41931.
                best_src_scene = None
                best_src_scene_users = -1
                for sce, nbr_users in src_scenes.items():
                    if (nbr_users) > best_src_scene_users:
                        best_src_scene_users = nbr_users
                        best_src_scene = sce
                scene.unit_settings.system = best_src_scene.unit_settings.system
                scene.unit_settings.system_rotation = best_src_scene.unit_settings.system_rotation
                scene.unit_settings.scale_length = best_src_scene.unit_settings.scale_length

                scene.update()
                # TODO - BUMMER! Armatures not in the group wont animate the mesh
            else:
                scene = data

            kwargs_batch = kwargs.copy()
            kwargs_batch["context_objects"] = data.objects

            save_single(operator, scene, filepath, **kwargs_batch)

            if batch_mode == 'GROUP':
                # remove temp group scene
                bpy.data.scenes.remove(scene)

        # no active scene changing!
        # bpy.data.scenes.active = orig_sce

        ret = {'FINISHED'}  # so the script wont run after we have batch exported.

    if active_object and org_mode and bpy.ops.object.mode_set.poll():
        bpy.ops.object.mode_set(mode=org_mode)

    return ret
