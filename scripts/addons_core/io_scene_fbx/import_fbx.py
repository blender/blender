# SPDX-FileCopyrightText: 2013-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

# FBX 7.1.0 -> 7.4.0 loader for Blender

# Not totally pep8 compliant.
#   pep8 import_fbx.py --ignore=E501,E123,E702,E125

if "bpy" in locals():
    import importlib
    if "parse_fbx" in locals():
        importlib.reload(parse_fbx)
    if "fbx_utils" in locals():
        importlib.reload(fbx_utils)

import bpy
from bpy.app.translations import pgettext_rpt as rpt_
from mathutils import Matrix, Euler, Vector, Quaternion
from bpy_extras import anim_utils

# Also imported in .fbx_utils, so importing here is unlikely to further affect Blender startup time.
import numpy as np

# -----
# Utils
from . import parse_fbx, fbx_utils

from .parse_fbx import (
    data_types,
    FBXElem,
)
from .fbx_utils import (
    PerfMon,
    units_blender_to_fbx_factor,
    units_convertor_iter,
    array_to_matrix4,
    similar_values,
    similar_values_iter,
    FBXImportSettings,
    vcos_transformed,
    nors_transformed,
    parray_as_ndarray,
    astype_view_signedness,
    MESH_ATTRIBUTE_MATERIAL_INDEX,
    MESH_ATTRIBUTE_POSITION,
    MESH_ATTRIBUTE_EDGE_VERTS,
    MESH_ATTRIBUTE_CORNER_VERT,
    MESH_ATTRIBUTE_SHARP_FACE,
    MESH_ATTRIBUTE_SHARP_EDGE,
    expand_shape_key_range,
    FBX_KTIME_V7,
    FBX_KTIME_V8,
    FBX_TIMECODE_DEFINITION_TO_KTIME_PER_SECOND,
)

LINEAR_INTERPOLATION_VALUE = bpy.types.Keyframe.bl_rna.properties['interpolation'].enum_items['LINEAR'].value

# global singleton, assign on execution
fbx_elem_nil = None

# Units converters...
convert_deg_to_rad_iter = units_convertor_iter("degree", "radian")

MAT_CONVERT_BONE = fbx_utils.MAT_CONVERT_BONE.inverted()
MAT_CONVERT_LIGHT = fbx_utils.MAT_CONVERT_LIGHT.inverted()
MAT_CONVERT_CAMERA = fbx_utils.MAT_CONVERT_CAMERA.inverted()


def validate_blend_names(name):
    assert(type(name) == bytes)
    # Blender typically does not accept names over 63 bytes...
    if len(name) > 63:
        import hashlib
        h = hashlib.sha1(name).hexdigest()
        n = 55
        name_utf8 = name[:n].decode('utf-8', 'replace') + "_" + h[:7]
        while len(name_utf8.encode()) > 63:
            n -= 1
            name_utf8 = name[:n].decode('utf-8', 'replace') + "_" + h[:7]
        return name_utf8
    else:
        # We use 'replace' even though FBX 'specs' say it should always be utf8, see T53841.
        return name.decode('utf-8', 'replace')


def elem_find_first(elem, id_search, default=None):
    for fbx_item in elem.elems:
        if fbx_item.id == id_search:
            return fbx_item
    return default


def elem_find_iter(elem, id_search):
    for fbx_item in elem.elems:
        if fbx_item.id == id_search:
            yield fbx_item


def elem_find_first_string(elem, id_search):
    fbx_item = elem_find_first(elem, id_search)
    if fbx_item is not None and fbx_item.props:  # Do not error on complete empty properties (see T45291).
        assert(len(fbx_item.props) == 1)
        assert(fbx_item.props_type[0] == data_types.STRING)
        return fbx_item.props[0].decode('utf-8', 'replace')
    return None


def elem_find_first_string_as_bytes(elem, id_search):
    fbx_item = elem_find_first(elem, id_search)
    if fbx_item is not None and fbx_item.props:  # Do not error on complete empty properties (see T45291).
        assert(len(fbx_item.props) == 1)
        assert(fbx_item.props_type[0] == data_types.STRING)
        return fbx_item.props[0]  # Keep it as bytes as requested...
    return None


def elem_find_first_bytes(elem, id_search, decode=True):
    fbx_item = elem_find_first(elem, id_search)
    if fbx_item is not None and fbx_item.props:  # Do not error on complete empty properties (see T45291).
        assert(len(fbx_item.props) == 1)
        assert(fbx_item.props_type[0] == data_types.BYTES)
        return fbx_item.props[0]
    return None


def elem_repr(elem):
    return "%s: props[%d=%r], elems=(%r)" % (
        elem.id,
        len(elem.props),
        ", ".join([repr(p) for p in elem.props]),
        # elem.props_type,
        b", ".join([e.id for e in elem.elems]),
    )


def elem_split_name_class(elem):
    assert(elem.props_type[-2] == data_types.STRING)
    elem_name, elem_class = elem.props[-2].split(b'\x00\x01')
    return elem_name, elem_class


def elem_name_ensure_class(elem, clss=...):
    elem_name, elem_class = elem_split_name_class(elem)
    if clss is not ...:
        assert(elem_class == clss)
    return validate_blend_names(elem_name)


def elem_name_ensure_classes(elem, clss=...):
    elem_name, elem_class = elem_split_name_class(elem)
    if clss is not ...:
        assert(elem_class in clss)
    return validate_blend_names(elem_name)


def elem_split_name_class_nodeattr(elem):
    assert(elem.props_type[-2] == data_types.STRING)
    elem_name, elem_class = elem.props[-2].split(b'\x00\x01')
    assert(elem_class == b'NodeAttribute')
    assert(elem.props_type[-1] == data_types.STRING)
    elem_class = elem.props[-1]
    return elem_name, elem_class


def elem_uuid(elem):
    assert(elem.props_type[0] == data_types.INT64)
    return elem.props[0]


def elem_prop_first(elem, default=None):
    return elem.props[0] if (elem is not None) and elem.props else default


# ----
# Support for
# Properties70: { ... P:
# Custom properties ("user properties" in FBX) are ignored here and get handled separately (see #104773).
def elem_props_find_first(elem, elem_prop_id):
    if elem is None:
        # When properties are not found... Should never happen, but happens - as usual.
        return None
    # support for templates (tuple of elems)
    if type(elem) is not FBXElem:
        assert(type(elem) is tuple)
        for e in elem:
            result = elem_props_find_first(e, elem_prop_id)
            if result is not None:
                return result
        assert(len(elem) > 0)
        return None

    for subelem in elem.elems:
        assert(subelem.id == b'P')
        # 'U' flag indicates that the property has been defined by the user.
        if subelem.props[0] == elem_prop_id and b'U' not in subelem.props[3]:
            return subelem
    return None


def elem_props_get_color_rgb(elem, elem_prop_id, default=None):
    elem_prop = elem_props_find_first(elem, elem_prop_id)
    if elem_prop is not None:
        assert(elem_prop.props[0] == elem_prop_id)
        if elem_prop.props[1] == b'Color':
            # FBX version 7300
            assert(elem_prop.props[1] == b'Color')
            assert(elem_prop.props[2] == b'')
        else:
            assert(elem_prop.props[1] == b'ColorRGB')
            assert(elem_prop.props[2] == b'Color')
        assert(elem_prop.props_type[4:7] == bytes((data_types.FLOAT64,)) * 3)
        return elem_prop.props[4:7]
    return default


def elem_props_get_vector_3d(elem, elem_prop_id, default=None):
    elem_prop = elem_props_find_first(elem, elem_prop_id)
    if elem_prop is not None:
        assert(elem_prop.props_type[4:7] == bytes((data_types.FLOAT64,)) * 3)
        return elem_prop.props[4:7]
    return default


def elem_props_get_number(elem, elem_prop_id, default=None):
    elem_prop = elem_props_find_first(elem, elem_prop_id)
    if elem_prop is not None:
        assert(elem_prop.props[0] == elem_prop_id)
        if elem_prop.props[1] == b'double':
            assert(elem_prop.props[1] == b'double')
            assert(elem_prop.props[2] == b'Number')
        else:
            assert(elem_prop.props[1] == b'Number')
            assert(elem_prop.props[2] == b'')

        # we could allow other number types
        assert(elem_prop.props_type[4] == data_types.FLOAT64)

        return elem_prop.props[4]
    return default


def elem_props_get_integer(elem, elem_prop_id, default=None):
    elem_prop = elem_props_find_first(elem, elem_prop_id)
    if elem_prop is not None:
        assert(elem_prop.props[0] == elem_prop_id)
        if elem_prop.props[1] == b'int':
            assert(elem_prop.props[1] == b'int')
            assert(elem_prop.props[2] == b'Integer')
        elif elem_prop.props[1] == b'ULongLong':
            assert(elem_prop.props[1] == b'ULongLong')
            assert(elem_prop.props[2] == b'')

        # we could allow other number types
        assert(elem_prop.props_type[4] in {data_types.INT32, data_types.INT64})

        return elem_prop.props[4]
    return default


def elem_props_get_bool(elem, elem_prop_id, default=None):
    elem_prop = elem_props_find_first(elem, elem_prop_id)
    if elem_prop is not None:
        assert(elem_prop.props[0] == elem_prop_id)
        # b'Bool' with a capital seems to be used for animated property... go figure...
        assert(elem_prop.props[1] in {b'bool', b'Bool'})
        assert(elem_prop.props[2] == b'')

        # we could allow other number types
        assert(elem_prop.props_type[4] == data_types.INT32)
        assert(elem_prop.props[4] in {0, 1})

        return bool(elem_prop.props[4])
    return default


def elem_props_get_enum(elem, elem_prop_id, default=None):
    elem_prop = elem_props_find_first(elem, elem_prop_id)
    if elem_prop is not None:
        assert(elem_prop.props[0] == elem_prop_id)
        assert(elem_prop.props[1] == b'enum')
        assert(elem_prop.props[2] == b'')
        assert(elem_prop.props[3] == b'')

        # we could allow other number types
        assert(elem_prop.props_type[4] == data_types.INT32)

        return elem_prop.props[4]
    return default


def elem_props_get_visibility(elem, elem_prop_id, default=None):
    elem_prop = elem_props_find_first(elem, elem_prop_id)
    if elem_prop is not None:
        assert(elem_prop.props[0] == elem_prop_id)
        assert(elem_prop.props[1] == b'Visibility')
        assert(elem_prop.props[2] == b'')

        # we could allow other number types
        assert(elem_prop.props_type[4] == data_types.FLOAT64)

        return elem_prop.props[4]
    return default


# ----------------------------------------------------------------------------
# Blender

# ------
# Object
from collections import namedtuple


FBXTransformData = namedtuple("FBXTransformData", (
    "loc", "geom_loc",
    "rot", "rot_ofs", "rot_piv", "pre_rot", "pst_rot", "rot_ord", "rot_alt_mat", "geom_rot",
    "sca", "sca_ofs", "sca_piv", "geom_sca",
))


def blen_read_custom_properties(fbx_obj, blen_obj, settings):
    # There doesn't seem to be a way to put user properties into templates, so this only get the object properties:
    fbx_obj_props = elem_find_first(fbx_obj, b'Properties70')
    if fbx_obj_props:
        for fbx_prop in fbx_obj_props.elems:
            assert(fbx_prop.id == b'P')

            if b'U' in fbx_prop.props[3]:
                if fbx_prop.props[0] == b'UDP3DSMAX':
                    # Special case for 3DS Max user properties:
                    try:
                        assert(fbx_prop.props[1] == b'KString')
                    except AssertionError as exc:
                        print(exc)
                    assert(fbx_prop.props_type[4] == data_types.STRING)
                    items = fbx_prop.props[4].decode('utf-8', 'replace')
                    for item in items.split('\r\n'):
                        if item:
                            split_item = item.split('=', 1)
                            if len(split_item) != 2:
                                split_item = item.split(':', 1)
                            if len(split_item) != 2:
                                print("cannot parse UDP3DSMAX custom property '%s', ignoring..." % item)
                            else:
                                prop_name, prop_value = split_item
                                prop_name = validate_blend_names(prop_name.strip().encode('utf-8'))
                                blen_obj[prop_name] = prop_value.strip()
                else:
                    prop_name = validate_blend_names(fbx_prop.props[0])
                    prop_type = fbx_prop.props[1]
                    if prop_type in {b'Vector', b'Vector3D', b'Color', b'ColorRGB'}:
                        assert(fbx_prop.props_type[4:7] == bytes((data_types.FLOAT64,)) * 3)
                        blen_obj[prop_name] = fbx_prop.props[4:7]
                    elif prop_type in {b'Vector4', b'ColorRGBA'}:
                        assert(fbx_prop.props_type[4:8] == bytes((data_types.FLOAT64,)) * 4)
                        blen_obj[prop_name] = fbx_prop.props[4:8]
                    elif prop_type == b'Vector2D':
                        assert(fbx_prop.props_type[4:6] == bytes((data_types.FLOAT64,)) * 2)
                        blen_obj[prop_name] = fbx_prop.props[4:6]
                    elif prop_type in {b'Integer', b'int'}:
                        assert(fbx_prop.props_type[4] == data_types.INT32)
                        blen_obj[prop_name] = fbx_prop.props[4]
                    elif prop_type == b'KString':
                        assert(fbx_prop.props_type[4] == data_types.STRING)
                        blen_obj[prop_name] = fbx_prop.props[4].decode('utf-8', 'replace')
                    elif prop_type in {b'Number', b'double', b'Double'}:
                        assert(fbx_prop.props_type[4] == data_types.FLOAT64)
                        blen_obj[prop_name] = fbx_prop.props[4]
                    elif prop_type in {b'Float', b'float'}:
                        assert(fbx_prop.props_type[4] == data_types.FLOAT32)
                        blen_obj[prop_name] = fbx_prop.props[4]
                    elif prop_type in {b'Bool', b'bool'}:
                        assert(fbx_prop.props_type[4] == data_types.INT32)
                        blen_obj[prop_name] = fbx_prop.props[4] != 0
                    elif prop_type in {b'Enum', b'enum'}:
                        assert(fbx_prop.props_type[4:6] == bytes((data_types.INT32, data_types.STRING)))
                        val = fbx_prop.props[4]
                        if settings.use_custom_props_enum_as_string and fbx_prop.props[5]:
                            enum_items = fbx_prop.props[5].decode('utf-8', 'replace').split('~')
                            if val >= 0 and val < len(enum_items):
                                blen_obj[prop_name] = enum_items[val]
                            else:
                                print("WARNING: User property '%s' has wrong enum value, skipped" % prop_name)
                        else:
                            blen_obj[prop_name] = val
                    else:
                        print(
                            "WARNING: User property type '%s' is not supported" %
                            prop_type.decode(
                                'utf-8', 'replace'))


def blen_read_object_transform_do(transform_data):
    # This is a nightmare. FBX SDK uses Maya way to compute the transformation matrix of a node - utterly simple:
    #
    #     WorldTransform = ParentWorldTransform @ T @ Roff @ Rp @ Rpre @ R @ Rpost-1 @ Rp-1 @ Soff @ Sp @ S @ Sp-1
    #
    # Where all those terms are 4 x 4 matrices that contain:
    #     WorldTransform: Transformation matrix of the node in global space.
    #     ParentWorldTransform: Transformation matrix of the parent node in global space.
    #     T: Translation
    #     Roff: Rotation offset
    #     Rp: Rotation pivot
    #     Rpre: Pre-rotation
    #     R: Rotation
    #     Rpost-1: Inverse of the post-rotation (FBX 2011 documentation incorrectly specifies this without inversion)
    #     Rp-1: Inverse of the rotation pivot
    #     Soff: Scaling offset
    #     Sp: Scaling pivot
    #     S: Scaling
    #     Sp-1: Inverse of the scaling pivot
    #
    # But it was still too simple, and FBX notion of compatibility is... quite specific. So we also have to
    # support 3DSMax way:
    #
    #     WorldTransform = ParentWorldTransform @ T @ R @ S @ OT @ OR @ OS
    #
    # Where all those terms are 4 x 4 matrices that contain:
    #     WorldTransform: Transformation matrix of the node in global space
    #     ParentWorldTransform: Transformation matrix of the parent node in global space
    #     T: Translation
    #     R: Rotation
    #     S: Scaling
    #     OT: Geometric transform translation
    #     OR: Geometric transform rotation
    #     OS: Geometric transform scale
    #
    # Notes:
    #     Geometric transformations ***are not inherited***: ParentWorldTransform does not contain the OT, OR, OS
    #     of WorldTransform's parent node.
    #     The R matrix takes into account the rotation order. Other rotation matrices are always 'XYZ' order.
    #
    # Taken from https://help.autodesk.com/view/FBX/2020/ENU/
    #            ?guid=FBX_Developer_Help_nodes_and_scene_graph_fbx_nodes_computing_transformation_matrix_html

    # translation
    lcl_translation = Matrix.Translation(transform_data.loc)
    geom_loc = Matrix.Translation(transform_data.geom_loc)

    # rotation
    def to_rot(rot, rot_ord): return Euler(convert_deg_to_rad_iter(rot), rot_ord).to_matrix().to_4x4()
    lcl_rot = to_rot(transform_data.rot, transform_data.rot_ord) @ transform_data.rot_alt_mat
    pre_rot = to_rot(transform_data.pre_rot, 'XYZ')
    pst_rot = to_rot(transform_data.pst_rot, 'XYZ')
    geom_rot = to_rot(transform_data.geom_rot, 'XYZ')

    rot_ofs = Matrix.Translation(transform_data.rot_ofs)
    rot_piv = Matrix.Translation(transform_data.rot_piv)
    sca_ofs = Matrix.Translation(transform_data.sca_ofs)
    sca_piv = Matrix.Translation(transform_data.sca_piv)

    # scale
    lcl_scale = Matrix()
    lcl_scale[0][0], lcl_scale[1][1], lcl_scale[2][2] = transform_data.sca
    geom_scale = Matrix()
    geom_scale[0][0], geom_scale[1][1], geom_scale[2][2] = transform_data.geom_sca

    base_mat = (
        lcl_translation @
        rot_ofs @
        rot_piv @
        pre_rot @
        lcl_rot @
        pst_rot.inverted_safe() @
        rot_piv.inverted_safe() @
        sca_ofs @
        sca_piv @
        lcl_scale @
        sca_piv.inverted_safe()
    )
    geom_mat = geom_loc @ geom_rot @ geom_scale
    # We return mat without 'geometric transforms' too, because it is to be used for children, sigh...
    return (base_mat @ geom_mat, base_mat, geom_mat)


# XXX This might be weak, now that we can add vgroups from both bones and shapes, name collisions become
#     more likely, will have to make this more robust!!!
def add_vgroup_to_objects(vg_indices, vg_weights, vg_name, objects):
    assert(len(vg_indices) == len(vg_weights))
    if vg_indices:
        for obj in objects:
            # We replace/override here...
            vg = obj.vertex_groups.get(vg_name)
            if vg is None:
                vg = obj.vertex_groups.new(name=vg_name)
            vg_add = vg.add
            for i, w in zip(vg_indices, vg_weights):
                vg_add((i,), w, 'REPLACE')


def blen_read_object_transform_preprocess(fbx_props, fbx_obj, rot_alt_mat, use_prepost_rot):
    # This is quite involved, 'fbxRNode.cpp' from openscenegraph used as a reference
    const_vector_zero_3d = 0.0, 0.0, 0.0
    const_vector_one_3d = 1.0, 1.0, 1.0

    loc = list(elem_props_get_vector_3d(fbx_props, b'Lcl Translation', const_vector_zero_3d))
    rot = list(elem_props_get_vector_3d(fbx_props, b'Lcl Rotation', const_vector_zero_3d))
    sca = list(elem_props_get_vector_3d(fbx_props, b'Lcl Scaling', const_vector_one_3d))

    geom_loc = list(elem_props_get_vector_3d(fbx_props, b'GeometricTranslation', const_vector_zero_3d))
    geom_rot = list(elem_props_get_vector_3d(fbx_props, b'GeometricRotation', const_vector_zero_3d))
    geom_sca = list(elem_props_get_vector_3d(fbx_props, b'GeometricScaling', const_vector_one_3d))

    rot_ofs = elem_props_get_vector_3d(fbx_props, b'RotationOffset', const_vector_zero_3d)
    rot_piv = elem_props_get_vector_3d(fbx_props, b'RotationPivot', const_vector_zero_3d)
    sca_ofs = elem_props_get_vector_3d(fbx_props, b'ScalingOffset', const_vector_zero_3d)
    sca_piv = elem_props_get_vector_3d(fbx_props, b'ScalingPivot', const_vector_zero_3d)

    is_rot_act = elem_props_get_bool(fbx_props, b'RotationActive', False)

    if is_rot_act:
        if use_prepost_rot:
            pre_rot = elem_props_get_vector_3d(fbx_props, b'PreRotation', const_vector_zero_3d)
            pst_rot = elem_props_get_vector_3d(fbx_props, b'PostRotation', const_vector_zero_3d)
        else:
            pre_rot = const_vector_zero_3d
            pst_rot = const_vector_zero_3d
        rot_ord = {
            0: 'XYZ',
            1: 'XZY',
            2: 'YZX',
            3: 'YXZ',
            4: 'ZXY',
            5: 'ZYX',
            6: 'XYZ',  # XXX eSphericXYZ, not really supported...
        }.get(elem_props_get_enum(fbx_props, b'RotationOrder', 0))
    else:
        pre_rot = const_vector_zero_3d
        pst_rot = const_vector_zero_3d
        rot_ord = 'XYZ'

    return FBXTransformData(loc, geom_loc,
                            rot, rot_ofs, rot_piv, pre_rot, pst_rot, rot_ord, rot_alt_mat, geom_rot,
                            sca, sca_ofs, sca_piv, geom_sca)


# ---------
# Animation
def _blen_read_object_transform_do_anim(transform_data, lcl_translation_mat, lcl_rot_euler, lcl_scale_mat,
                                        extra_pre_matrix, extra_post_matrix):
    """Specialized version of blen_read_object_transform_do for animation that pre-calculates the non-animated matrices
    and returns a function that calculates (base_mat @ geom_mat). See the comments in blen_read_object_transform_do for
    a full description of what this function is doing.

    The lcl_translation_mat, lcl_rot_euler and lcl_scale_mat arguments should have their values updated each frame and
    then calling the returned function will calculate the matrix for the current frame.

    extra_pre_matrix and extra_post_matrix are any extra matrices to multiply first/last."""
    # Translation
    geom_loc = Matrix.Translation(transform_data.geom_loc)

    # Rotation
    def to_rot_xyz(rot):
        # All the rotations that can be precalculated have a fixed XYZ order.
        return Euler(convert_deg_to_rad_iter(rot), 'XYZ').to_matrix().to_4x4()
    pre_rot = to_rot_xyz(transform_data.pre_rot)
    pst_rot_inv = to_rot_xyz(transform_data.pst_rot).inverted_safe()
    geom_rot = to_rot_xyz(transform_data.geom_rot)

    # Offsets and pivots
    rot_ofs = Matrix.Translation(transform_data.rot_ofs)
    rot_piv = Matrix.Translation(transform_data.rot_piv)
    rot_piv_inv = rot_piv.inverted_safe()
    sca_ofs = Matrix.Translation(transform_data.sca_ofs)
    sca_piv = Matrix.Translation(transform_data.sca_piv)
    sca_piv_inv = sca_piv.inverted_safe()

    # Scale
    geom_scale = Matrix()
    geom_scale[0][0], geom_scale[1][1], geom_scale[2][2] = transform_data.geom_sca

    # Some matrices can be combined in advance, using the associative property of matrix multiplication, so that less
    # matrix multiplication is required each frame.
    geom_mat = geom_loc @ geom_rot @ geom_scale
    post_lcl_translation = rot_ofs @ rot_piv @ pre_rot
    post_lcl_rotation = transform_data.rot_alt_mat @ pst_rot_inv @ rot_piv_inv @ sca_ofs @ sca_piv
    post_lcl_scaling = sca_piv_inv @ geom_mat @ extra_post_matrix

    # Get the bound to_matrix method to avoid re-binding it on each call.
    lcl_rot_euler_to_matrix_3x3 = lcl_rot_euler.to_matrix
    # Get the unbound Matrix.to_4x4 method to avoid having to look it up again on each call.
    matrix_to_4x4 = Matrix.to_4x4

    if extra_pre_matrix == Matrix():
        # There aren't any other matrices that must be multiplied before lcl_translation_mat that extra_pre_matrix can
        # be combined with, so skip extra_pre_matrix when it's the identity matrix.
        return lambda: (lcl_translation_mat @
                        post_lcl_translation @
                        matrix_to_4x4(lcl_rot_euler_to_matrix_3x3()) @
                        post_lcl_rotation @
                        lcl_scale_mat @
                        post_lcl_scaling)
    else:
        return lambda: (extra_pre_matrix @
                        lcl_translation_mat @
                        post_lcl_translation @
                        matrix_to_4x4(lcl_rot_euler_to_matrix_3x3()) @
                        post_lcl_rotation @
                        lcl_scale_mat @
                        post_lcl_scaling)


def _transformation_curves_gen(item, values_arrays, channel_keys):
    """Yields flattened location/rotation/scaling values for imported PoseBone/Object Lcl Translation/Rotation/Scaling
    animation curve values.

    The value arrays must have the same lengths, where each index of each array corresponds to a single keyframe.

    Each value array must have a corresponding channel key tuple that identifies the fbx property
    (b'Lcl Translation'/b'Lcl Rotation'/b'Lcl Scaling') and the channel (x/y/z as 0/1/2) of that property."""
    from operator import setitem
    from functools import partial

    if item.is_bone:
        bl_obj = item.bl_obj.pose.bones[item.bl_bone]
    else:
        bl_obj = item.bl_obj

    rot_mode = bl_obj.rotation_mode
    transform_data = item.fbx_transform_data
    rot_eul_prev = bl_obj.rotation_euler.copy()
    rot_quat_prev = bl_obj.rotation_quaternion.copy()

    # Pre-compute combined pre-matrix
    # Remove that rest pose matrix from current matrix (also in parent space) by computing the inverted local rest
    # matrix of the bone, if relevant.
    combined_pre_matrix = item.get_bind_matrix().inverted_safe() if item.is_bone else Matrix()
    # item.pre_matrix will contain any correction for a parent's correction matrix or the global matrix
    if item.pre_matrix:
        combined_pre_matrix @= item.pre_matrix

    # Pre-compute combined post-matrix
    # Compensate for changes in the local matrix during processing
    combined_post_matrix = item.anim_compensation_matrix.copy() if item.anim_compensation_matrix else Matrix()
    # item.post_matrix will contain any correction for lights, camera and bone orientation
    if item.post_matrix:
        combined_post_matrix @= item.post_matrix

    # Create matrices/euler from the initial transformation values of this item.
    # These variables will be updated in-place as we iterate through each frame.
    lcl_translation_mat = Matrix.Translation(transform_data.loc)
    lcl_rotation_eul = Euler(convert_deg_to_rad_iter(transform_data.rot), transform_data.rot_ord)
    lcl_scaling_mat = Matrix()
    lcl_scaling_mat[0][0], lcl_scaling_mat[1][1], lcl_scaling_mat[2][2] = transform_data.sca

    # Create setters into lcl_translation_mat, lcl_rotation_eul and lcl_scaling_mat for each values_array and convert
    # any rotation values into radians.
    lcl_setters = []
    values_arrays_converted = []
    for values_array, (fbx_prop, channel) in zip(values_arrays, channel_keys):
        if fbx_prop == b'Lcl Translation':
            # lcl_translation_mat.translation[channel] = value
            setter = partial(setitem, lcl_translation_mat.translation, channel)
        elif fbx_prop == b'Lcl Rotation':
            # FBX rotations are in degrees, but Blender uses radians, so convert all rotation values in advance.
            values_array = np.deg2rad(values_array)
            # lcl_rotation_eul[channel] = value
            setter = partial(setitem, lcl_rotation_eul, channel)
        else:
            assert(fbx_prop == b'Lcl Scaling')
            # lcl_scaling_mat[channel][channel] = value
            setter = partial(setitem, lcl_scaling_mat[channel], channel)
        lcl_setters.append(setter)
        values_arrays_converted.append(values_array)

    # Create an iterator that gets one value from each array. Each iterated tuple will be all the imported
    # Lcl Translation/Lcl Rotation/Lcl Scaling values for a single frame, in that order.
    # Note that an FBX animation does not have to animate all the channels, so only the animated channels of each
    # property will be present.
    # .data, the memoryview of an np.ndarray, is faster to iterate than the ndarray itself.
    frame_values_it = zip(*(arr.data for arr in values_arrays_converted))

    # Getting the unbound methods in advance avoids having to look them up again on each call within the loop.
    mat_decompose = Matrix.decompose
    quat_to_axis_angle = Quaternion.to_axis_angle
    quat_to_euler = Quaternion.to_euler
    quat_dot = Quaternion.dot

    calc_mat = _blen_read_object_transform_do_anim(transform_data,
                                                   lcl_translation_mat, lcl_rotation_eul, lcl_scaling_mat,
                                                   combined_pre_matrix, combined_post_matrix)

    # Iterate through the values for each frame.
    for frame_values in frame_values_it:
        # Set each value into its corresponding lcl matrix/euler.
        for lcl_setter, value in zip(lcl_setters, frame_values):
            lcl_setter(value)

        # Calculate the updated matrix for this frame.
        mat = calc_mat()

        # Now we have a virtual matrix of transform from AnimCurves, we can yield keyframe values!
        loc, rot, sca = mat_decompose(mat)
        if rot_mode == 'QUATERNION':
            if quat_dot(rot_quat_prev, rot) < 0.0:
                rot = -rot
            rot_quat_prev = rot
        elif rot_mode == 'AXIS_ANGLE':
            vec, ang = quat_to_axis_angle(rot)
            rot = ang, vec.x, vec.y, vec.z
        else:  # Euler
            rot = quat_to_euler(rot, rot_mode, rot_eul_prev)
            rot_eul_prev = rot

        # Yield order matches the order that the location/rotation/scale FCurves are created in.
        yield from loc
        yield from rot
        yield from sca


def _combine_curve_keyframe_times(times_and_values_tuples, initial_values):
    """Combine multiple parsed animation curves, that affect different channels, such that every animation curve
    contains the keyframes from every other curve, interpolating the values for the newly inserted keyframes in each
    curve.

    Currently, linear interpolation is assumed, but FBX does store how keyframes should be interpolated, so correctly
    interpolating the keyframe values is a TODO."""
    if len(times_and_values_tuples) == 1:
        # Nothing to do when there is only a single curve.
        times, values = times_and_values_tuples[0]
        return times, [values]

    all_times = [t[0] for t in times_and_values_tuples]

    # Get the combined sorted unique times of all the curves.
    sorted_all_times = np.unique(np.concatenate(all_times))

    values_arrays = []
    for (times, values), initial_value in zip(times_and_values_tuples, initial_values):
        if sorted_all_times.size == times.size:
            # `sorted_all_times` will always contain all values in `times` and both `times` and `sorted_all_times` must
            # be strictly increasing, so if both arrays have the same size, they must be identical.
            extended_values = values
        else:
            # For now, linear interpolation is assumed. NumPy conveniently has a fast C-compiled function for this.
            # Efficiently implementing other FBX supported interpolation will most likely be much more complicated.
            extended_values = np.interp(sorted_all_times, times, values, left=initial_value)
        values_arrays.append(extended_values)
    return sorted_all_times, values_arrays


def blen_read_invalid_animation_curve(key_times, key_values):
    """FBX will parse animation curves even when their keyframe times are invalid (not strictly increasing). It's
    unclear exactly how FBX handles invalid curves, but this matches in some cases and is how the FBX IO addon has been
    handling invalid keyframe times for a long time.

    Notably, this function will also correctly parse valid animation curves, though is much slower than the trivial,
    regular way.

    The returned keyframe times are guaranteed to be strictly increasing."""
    sorted_unique_times = np.unique(key_times)

    # Unsure if this can be vectorized with numpy, so using iteration for now.
    def index_gen():
        idx = 0
        key_times_data = key_times.data
        key_times_len = len(key_times)
        # Iterating .data, the memoryview of the array, is faster than iterating the array directly.
        for curr_fbxktime in sorted_unique_times.data:
            if key_times_data[idx] < curr_fbxktime:
                if idx >= 0:
                    idx += 1
                    if idx >= key_times_len:
                        # We have reached our last element for this curve, stay on it from now on...
                        idx = -1
            yield idx

    indices = np.fromiter(index_gen(), dtype=np.int64, count=len(sorted_unique_times))
    indexed_times = key_times[indices]
    indexed_values = key_values[indices]

    # Linear interpolate the value for each time in sorted_unique_times according to the times and values at each index
    # and the previous index.
    interpolated_values = np.empty_like(indexed_values)

    # Where the index is 0, there's no previous value to interpolate from, so we set the value without interpolating.
    # Because the indices are in increasing order, all zeroes must be at the start, so we can find the index of the last
    # zero and use that to index with a slice instead of a boolean array for performance.
    # Equivalent to, but as a slice:
    # idx_zero_mask = indices == 0
    # idx_nonzero_mask = ~idx_zero_mask
    first_nonzero_idx = np.searchsorted(indices, 0, side='right')
    idx_zero_slice = slice(0, first_nonzero_idx)  # [:first_nonzero_idx]
    idx_nonzero_slice = slice(first_nonzero_idx, None)  # [first_nonzero_idx:]

    interpolated_values[idx_zero_slice] = indexed_values[idx_zero_slice]

    indexed_times_nonzero_idx = indexed_times[idx_nonzero_slice]
    indexed_values_nonzero_idx = indexed_values[idx_nonzero_slice]
    indices_nonzero = indices[idx_nonzero_slice]

    prev_indices_nonzero = indices_nonzero - 1
    prev_indexed_times_nonzero_idx = key_times[prev_indices_nonzero]
    prev_indexed_values_nonzero_idx = key_values[prev_indices_nonzero]

    ifac_a = sorted_unique_times[idx_nonzero_slice] - prev_indexed_times_nonzero_idx
    ifac_b = indexed_times_nonzero_idx - prev_indexed_times_nonzero_idx
    # If key_times contains two (or more) duplicate times in a row, then values in `ifac_b` can be zero which would
    # result in division by zero.
    # Use the `np.errstate` context manager to suppress printing the RuntimeWarning to the system console.
    with np.errstate(divide='ignore'):
        ifac = ifac_a / ifac_b
    interpolated_values[idx_nonzero_slice] = ((indexed_values_nonzero_idx - prev_indexed_values_nonzero_idx) * ifac
                                              + prev_indexed_values_nonzero_idx)

    # If the time to interpolate at is larger than the time in indexed_times, then the value has been extrapolated.
    # Extrapolated values are excluded.
    valid_mask = indexed_times >= sorted_unique_times

    key_times = sorted_unique_times[valid_mask]
    key_values = interpolated_values[valid_mask]

    return key_times, key_values


def _convert_fbx_time_to_blender_time(key_times, blen_start_offset, fbx_start_offset, fps, fbx_ktime):
    timefac = fps / fbx_ktime

    # Convert from FBX timing to Blender timing.
    # Cannot subtract in-place because key_times could be read directly from FBX and could be used by multiple Actions.
    key_times = key_times - fbx_start_offset
    # FBX times are integers and timefac is a Python float, so the new array will be a np.float64 array.
    key_times = key_times * timefac

    key_times += blen_start_offset

    return key_times


def blen_read_animation_curve(fbx_curve):
    """Read an animation curve from FBX data.

    The parsed keyframe times are guaranteed to be strictly increasing."""
    key_times = parray_as_ndarray(elem_prop_first(elem_find_first(fbx_curve, b'KeyTime')))
    key_values = parray_as_ndarray(elem_prop_first(elem_find_first(fbx_curve, b'KeyValueFloat')))

    assert(len(key_values) == len(key_times))

    # The FBX SDK specifies that only one key per time is allowed and that the keys are sorted in time order.
    # https://help.autodesk.com/view/FBX/2020/ENU/?guid=FBX_Developer_Help_cpp_ref_class_fbx_anim_curve_html
    all_times_strictly_increasing = (key_times[1:] > key_times[:-1]).all()

    if all_times_strictly_increasing:
        return key_times, key_values
    else:
        # FBX will still read animation curves even if they are invalid.
        return blen_read_invalid_animation_curve(key_times, key_values)


def blen_store_keyframes(fbx_key_times, blen_fcurve, key_values, blen_start_offset, fps, fbx_ktime, fbx_start_offset=0):
    """Set all keyframe times and values for a newly created FCurve.
    Linear interpolation is currently assumed.

    This is a convenience function for calling blen_store_keyframes_multi with only a single fcurve and values array."""
    blen_store_keyframes_multi(fbx_key_times, [(blen_fcurve, key_values)], blen_start_offset, fps, fbx_ktime,
                               fbx_start_offset)


def blen_store_keyframes_multi(fbx_key_times, fcurve_and_key_values_pairs, blen_start_offset, fps, fbx_ktime,
                               fbx_start_offset=0):
    """Set all keyframe times and values for multiple pairs of newly created FCurves and keyframe values arrays, where
    each pair has the same keyframe times.
    Linear interpolation is currently assumed."""
    bl_key_times = _convert_fbx_time_to_blender_time(fbx_key_times, blen_start_offset, fbx_start_offset, fps, fbx_ktime)
    num_keys = len(bl_key_times)

    # Compatible with C float type
    bl_keyframe_dtype = np.single
    # Compatible with C char type
    bl_enum_dtype = np.ubyte

    # The keyframe_points 'co' are accessed as flattened pairs of (time, value).
    # The key times are the same for each (blen_fcurve, key_values) pair, so only the values need to be updated for each
    # array of values.
    keyframe_points_co = np.empty(len(bl_key_times) * 2, dtype=bl_keyframe_dtype)
    # Even indices are times.
    keyframe_points_co[0::2] = bl_key_times

    interpolation_array = np.full(num_keys, LINEAR_INTERPOLATION_VALUE, dtype=bl_enum_dtype)

    for blen_fcurve, key_values in fcurve_and_key_values_pairs:
        # The fcurve must be newly created and thus have no keyframe_points.
        assert(len(blen_fcurve.keyframe_points) == 0)

        # Odd indices are values.
        keyframe_points_co[1::2] = key_values

        # Add the keyframe points to the FCurve and then set the 'co' and 'interpolation' of each point.
        blen_fcurve.keyframe_points.add(num_keys)
        blen_fcurve.keyframe_points.foreach_set('co', keyframe_points_co)
        blen_fcurve.keyframe_points.foreach_set('interpolation', interpolation_array)

        # Since we inserted our keyframes in 'ultra-fast' mode, we have to update the fcurves now.
        blen_fcurve.update()


def blen_read_animations_action_item(channelbag, item, cnodes, fps, anim_offset, global_scale, shape_key_deforms,
                                     fbx_ktime):
    """
    'Bake' loc/rot/scale into the channelbag,
    taking any pre_ and post_ matrix into account to transform from fbx into blender space.
    """
    from bpy.types import ShapeKey, Material, Camera

    fbx_curves: dict[bytes, dict[int, FBXElem]] = {}
    for curves, fbxprop in cnodes.values():
        channels_dict = fbx_curves.setdefault(fbxprop, {})
        for (fbx_acdata, _blen_data), channel in curves.values():
            if channel in channels_dict:
                # Ignore extra curves when one has already been found for this channel because FBX's default animation
                # system implementation only uses the first curve assigned to a channel.
                # Additional curves per channel are allowed by the FBX specification, but the handling of these curves
                # is considered the responsibility of the application that created them. Note that each curve node is
                # expected to have a unique set of channels, so these additional curves with the same channel would have
                # to belong to separate curve nodes. See the FBX SDK documentation for FbxAnimCurveNode.
                continue
            channels_dict[channel] = fbx_acdata

    # Leave if no curves are attached (if a blender curve is attached to scale but without keys it defaults to 0).
    if len(fbx_curves) == 0:
        return

    if isinstance(item, Material):
        grpname = item.name
        props = [("diffuse_color", 3, grpname or "Diffuse Color")]
    elif isinstance(item, ShapeKey):
        props = [(item.path_from_id("value"), 1, "Key")]
    elif isinstance(item, Camera):
        props = [(item.path_from_id("lens"), 1, "Camera"), (item.dof.path_from_id("focus_distance"), 1, "Camera")]
    else:  # Object or PoseBone:
        if item.is_bone:
            bl_obj = item.bl_obj.pose.bones[item.bl_bone]
        else:
            bl_obj = item.bl_obj

        # We want to create actions for objects, but for bones we 'reuse' armatures' actions!
        grpname = bl_obj.name

        # Since we might get other channels animated in the end, due to all FBX transform magic,
        # we need to add curves for whole loc/rot/scale in any case.
        props = [(bl_obj.path_from_id("location"), 3, grpname or "Location"),
                 None,
                 (bl_obj.path_from_id("scale"), 3, grpname or "Scale")]
        rot_mode = bl_obj.rotation_mode
        if rot_mode == 'QUATERNION':
            props[1] = (bl_obj.path_from_id("rotation_quaternion"), 4, grpname or "Quaternion Rotation")
        elif rot_mode == 'AXIS_ANGLE':
            props[1] = (bl_obj.path_from_id("rotation_axis_angle"), 4, grpname or "Axis Angle Rotation")
        else:  # Euler
            props[1] = (bl_obj.path_from_id("rotation_euler"), 3, grpname or "Euler Rotation")

    blen_curves = [channelbag.fcurves.new(prop, index=channel, group_name=grpname)
                   for prop, nbr_channels, grpname in props for channel in range(nbr_channels)]

    if isinstance(item, Material):
        for fbxprop, channel_to_curve in fbx_curves.items():
            assert(fbxprop == b'DiffuseColor')
            for channel, curve in channel_to_curve.items():
                assert(channel in {0, 1, 2})
                blen_curve = blen_curves[channel]
                fbx_key_times, values = blen_read_animation_curve(curve)
                blen_store_keyframes(fbx_key_times, blen_curve, values, anim_offset, fps, fbx_ktime)

    elif isinstance(item, ShapeKey):
        for fbxprop, channel_to_curve in fbx_curves.items():
            assert(fbxprop == b'DeformPercent')
            for channel, curve in channel_to_curve.items():
                assert(channel == 0)
                blen_curve = blen_curves[channel]

                fbx_key_times, values = blen_read_animation_curve(curve)
                # A fully activated shape key in FBX DeformPercent is 100.0 whereas it is 1.0 in Blender.
                values = values / 100.0
                blen_store_keyframes(fbx_key_times, blen_curve, values, anim_offset, fps, fbx_ktime)

                # Store the minimum and maximum shape key values, so that the shape key's slider range can be expanded
                # if necessary after reading all animations.
                if values.size:
                    deform_values = shape_key_deforms.setdefault(item, [])
                    deform_values.append(values.min())
                    deform_values.append(values.max())

    elif isinstance(item, Camera):
        for fbxprop, channel_to_curve in fbx_curves.items():
            is_focus_distance = fbxprop == b'FocusDistance'
            assert(fbxprop == b'FocalLength' or is_focus_distance)
            for channel, curve in channel_to_curve.items():
                assert(channel == 0)
                # The indices are determined by the creation of the `props` list above.
                blen_curve = blen_curves[1 if is_focus_distance else 0]

                fbx_key_times, values = blen_read_animation_curve(curve)
                if is_focus_distance:
                    # Remap the imported values from FBX to Blender.
                    values = values / 1000.0
                    values *= global_scale
                blen_store_keyframes(fbx_key_times, blen_curve, values, anim_offset, fps, fbx_ktime)

    else:  # Object or PoseBone:
        transform_data = item.fbx_transform_data

        # Each transformation curve needs to have keyframes at the times of every other transformation curve
        # (interpolating missing values), so that we can construct a matrix at every keyframe.
        transform_prop_to_attr = {
            b'Lcl Translation': transform_data.loc,
            b'Lcl Rotation': transform_data.rot,
            b'Lcl Scaling': transform_data.sca,
        }

        times_and_values_tuples = []
        initial_values = []
        channel_keys = []
        for fbxprop, channel_to_curve in fbx_curves.items():
            if fbxprop not in transform_prop_to_attr:
                # Currently, we only care about transformation curves.
                continue
            for channel, curve in channel_to_curve.items():
                assert(channel in {0, 1, 2})
                fbx_key_times, values = blen_read_animation_curve(curve)

                channel_keys.append((fbxprop, channel))

                initial_values.append(transform_prop_to_attr[fbxprop][channel])

                times_and_values_tuples.append((fbx_key_times, values))
        if not times_and_values_tuples:
            # If `times_and_values_tuples` is empty, all the imported animation curves are for properties other than
            # transformation (e.g. animated custom properties), so there is nothing to do until support for those other
            # properties is added.
            return

        # Combine the keyframe times of all the transformation curves so that each curve has a value at every time.
        combined_fbx_times, values_arrays = _combine_curve_keyframe_times(times_and_values_tuples, initial_values)

        # Convert from FBX Lcl Translation/Lcl Rotation/Lcl Scaling to the Blender location/rotation/scaling properties
        # of this Object/PoseBone.
        # The number of fcurves for the Blender properties varies depending on the rotation mode.
        num_loc_channels = 3
        num_rot_channels = 4 if rot_mode in {'QUATERNION', 'AXIS_ANGLE'} else 3  # Variations of EULER are all 3
        num_sca_channels = 3
        num_channels = num_loc_channels + num_rot_channels + num_sca_channels
        num_frames = len(combined_fbx_times)
        full_length = num_channels * num_frames

        # Do the conversion.
        flattened_channel_values_gen = _transformation_curves_gen(item, values_arrays, channel_keys)
        flattened_channel_values = np.fromiter(flattened_channel_values_gen, dtype=np.single, count=full_length)

        # Reshape to one row per frame and then view the transpose so that each row corresponds to a single channel.
        # e.g.
        # loc_channels = channel_values[:num_loc_channels]
        # rot_channels = channel_values[num_loc_channels:num_loc_channels + num_rot_channels]
        # sca_channels = channel_values[num_loc_channels + num_rot_channels:]
        channel_values = flattened_channel_values.reshape(num_frames, num_channels).T

        # Each channel has the same keyframe times, so the combined times can be passed once along with all the curves
        # and values arrays.
        blen_store_keyframes_multi(combined_fbx_times, zip(blen_curves, channel_values), anim_offset, fps, fbx_ktime)


def blen_read_animations(fbx_tmpl_astack, fbx_tmpl_alayer, stacks, scene, anim_offset, global_scale, fbx_ktime):
    """
    Recreate an action per stack/layer/object combinations.
    Only the first found action is linked to objects, more complex setups are not handled,
    it's up to user to reproduce them!
    """
    from bpy.types import ShapeKey, Material, Camera

    shape_key_values = {}
    actions = {}
    for as_uuid, ((fbx_asdata, _blen_data), alayers) in stacks.items():
        stack_name = elem_name_ensure_class(fbx_asdata, b'AnimStack')
        for al_uuid, ((fbx_aldata, _blen_data), items) in alayers.items():
            layer_name = elem_name_ensure_class(fbx_aldata, b'AnimLayer')
            for item, cnodes in items.items():
                if isinstance(item, Material):
                    id_data = item
                elif isinstance(item, ShapeKey):
                    id_data = item.id_data
                elif isinstance(item, Camera):
                    id_data = item
                else:
                    id_data = item.bl_obj
                    # XXX Ignore rigged mesh animations - those are a nightmare to handle, see note about it in
                    #     FbxImportHelperNode class definition.
                    if id_data and id_data.type == 'MESH' and id_data.parent and id_data.parent.type == 'ARMATURE':
                        continue
                if id_data is None:
                    continue

                # Create new action if needed (should always be needed, except for keyblocks from shapekeys cases).
                key = (as_uuid, al_uuid, id_data)
                action = actions.get(key)
                if action is None:
                    if stack_name == layer_name:
                        action_name = "|".join((id_data.name, stack_name))
                    else:
                        action_name = "|".join((id_data.name, stack_name, layer_name))
                    actions[key] = action = bpy.data.actions.new(action_name)
                    action.use_fake_user = True

                    # Always use the same name for the slot. It should be simple
                    # to switch between imported Actions while keeping Slot
                    # auto-assignment, which means that all Actions should use
                    # the same slot name. As long as there's no separate
                    # indicator for the "intended object name" for this FBX
                    # animation, this is the best Blender can do. Maybe the
                    # 'stack name' would be a better choice?
                    action.slots.new(id_data.id_type, "Slot")

                # If none yet assigned, assign this action to id_data.
                if not id_data.animation_data:
                    id_data.animation_data_create()
                if not id_data.animation_data.action:
                    id_data.animation_data.action = action
                    id_data.animation_data.action_slot = action.slots[0]

                # And actually populate the action!
                channelbag = anim_utils.action_ensure_channelbag_for_slot(action, action.slots[0])
                blen_read_animations_action_item(channelbag, item, cnodes, scene.render.fps, anim_offset, global_scale,
                                                 shape_key_values, fbx_ktime)

    # If the minimum/maximum animated value is outside the slider range of the shape key, attempt to expand the slider
    # range until the animated range fits and has extra room to be decreased or increased further.
    # Shape key slider_min and slider_max have hard min/max values, if an imported animation uses a value outside that
    # range, a warning message will be printed to the console and the slider_min/slider_max values will end up clamped.
    shape_key_values_in_range = True
    for shape_key, deform_values in shape_key_values.items():
        min_animated_deform = min(deform_values)
        max_animated_deform = max(deform_values)
        shape_key_values_in_range &= expand_shape_key_range(shape_key, min_animated_deform)
        shape_key_values_in_range &= expand_shape_key_range(shape_key, max_animated_deform)
    if not shape_key_values_in_range:
        print("WARNING: The imported animated Value of a Shape Key is beyond the minimum/maximum allowed and will be"
              " clamped during playback.")


# ----
# Mesh

def blen_read_geom_layerinfo(fbx_layer):
    return (
        validate_blend_names(elem_find_first_string_as_bytes(fbx_layer, b'Name')),
        elem_find_first_string_as_bytes(fbx_layer, b'MappingInformationType'),
        elem_find_first_string_as_bytes(fbx_layer, b'ReferenceInformationType'),
    )


def blen_read_geom_validate_blen_data(blen_data, blen_dtype, item_size):
    """Validate blen_data when it's not a bpy_prop_collection.
    Returns whether blen_data is a bpy_prop_collection"""
    blen_data_is_collection = isinstance(blen_data, bpy.types.bpy_prop_collection)
    if not blen_data_is_collection:
        if item_size > 1:
            assert(len(blen_data.shape) == 2)
            assert(blen_data.shape[1] == item_size)
        assert(blen_data.dtype == blen_dtype)
    return blen_data_is_collection


def blen_read_geom_parse_fbx_data(fbx_data, stride, item_size):
    """Parse fbx_data as an array.array into a 2d np.ndarray that shares the same memory, where each row is a single
    item"""
    # Technically stride < item_size could be supported, but there's probably not a use case for it since it would
    # result in a view of the data with self-overlapping memory.
    assert(stride >= item_size)
    # View the array.array as an np.ndarray.
    fbx_data_np = parray_as_ndarray(fbx_data)

    if stride == item_size:
        if item_size > 1:
            # Need to make sure fbx_data_np has a whole number of items to be able to view item_size elements per row.
            items_remainder = len(fbx_data_np) % item_size
            if items_remainder:
                print("ERROR: not a whole number of items in this FBX layer, skipping the partial item!")
                fbx_data_np = fbx_data_np[:-items_remainder]
        fbx_data_np = fbx_data_np.reshape(-1, item_size)
    else:
        # Create a view of fbx_data_np that is only the first item_size elements of each stride. Note that the view will
        # not be C-contiguous.
        stride_remainder = len(fbx_data_np) % stride
        if stride_remainder:
            if stride_remainder < item_size:
                print("ERROR: not a whole number of items in this FBX layer, skipping the partial item!")
                # Not enough in the remainder for a full item, so cut off the partial stride
                fbx_data_np = fbx_data_np[:-stride_remainder]
                # Reshape to one stride per row and then create a view that includes only the first item_size elements
                # of each stride.
                fbx_data_np = fbx_data_np.reshape(-1, stride)[:, :item_size]
            else:
                print("ERROR: not a whole number of strides in this FBX layer! There are a whole number of items, but"
                      " this could indicate an error!")
                # There is not a whole number of strides, but there is a whole number of items.
                # This is a pain to deal with because fbx_data_np.reshape(-1, stride) is not possible.
                # A view of just the items can be created using stride_tricks.as_strided by specifying the shape and
                # strides of the view manually.
                # Extreme care must be taken when using stride_tricks.as_strided because improper usage can result in
                # a view that gives access to memory outside the array.
                from numpy.lib import stride_tricks

                # fbx_data_np should always start off as flat and C-contiguous.
                assert(fbx_data_np.strides == (fbx_data_np.itemsize,))

                num_whole_strides = len(fbx_data_np) // stride
                # Plus the one partial stride that is enough elements for a complete item.
                num_items = num_whole_strides + 1
                shape = (num_items, item_size)

                # strides are the number of bytes to step to get to the next element, for each axis.
                step_per_item = fbx_data_np.itemsize * stride
                step_per_item_element = fbx_data_np.itemsize
                strides = (step_per_item, step_per_item_element)

                fbx_data_np = stride_tricks.as_strided(fbx_data_np, shape, strides)
        else:
            # There's a whole number of strides, so first reshape to one stride per row and then create a view that
            # includes only the first item_size elements of each stride.
            fbx_data_np = fbx_data_np.reshape(-1, stride)[:, :item_size]

    return fbx_data_np


def blen_read_geom_check_fbx_data_length(blen_data, fbx_data_np, is_indices=False):
    """Check that there are the same number of items in blen_data and fbx_data_np.

    Returns a tuple of two elements:
        0: fbx_data_np or, if fbx_data_np contains more items than blen_data, a view of fbx_data_np with the excess
           items removed
        1: Whether the returned fbx_data_np contains enough items to completely fill blen_data"""
    bl_num_items = len(blen_data)
    fbx_num_items = len(fbx_data_np)
    enough_data = fbx_num_items >= bl_num_items
    if not enough_data:
        if is_indices:
            print("ERROR: not enough indices in this FBX layer, missing data will be left as default!")
        else:
            print("ERROR: not enough data in this FBX layer, missing data will be left as default!")
    elif fbx_num_items > bl_num_items:
        if is_indices:
            print("ERROR: too many indices in this FBX layer, skipping excess!")
        else:
            print("ERROR: too much data in this FBX layer, skipping excess!")
        fbx_data_np = fbx_data_np[:bl_num_items]

    return fbx_data_np, enough_data


def blen_read_geom_xform(fbx_data_np, xform):
    """xform is either None, or a function that takes fbx_data_np as its only positional argument and returns an
    np.ndarray with the same total number of elements as fbx_data_np.
    It is acceptable for xform to return an array with a different dtype to fbx_data_np.

    Returns xform(fbx_data_np) when xform is not None and ensures the result of xform(fbx_data_np) has the same shape as
    fbx_data_np before returning it.
    When xform is None, fbx_data_np is returned as is."""
    if xform is not None:
        item_size = fbx_data_np.shape[1]
        fbx_total_data = fbx_data_np.size
        fbx_data_np = xform(fbx_data_np)
        # The amount of data should not be changed by xform
        assert(fbx_data_np.size == fbx_total_data)
        # Ensure fbx_data_np is still item_size elements per row
        if len(fbx_data_np.shape) != 2 or fbx_data_np.shape[1] != item_size:
            fbx_data_np = fbx_data_np.reshape(-1, item_size)
    return fbx_data_np


def blen_read_geom_array_foreach_set_direct(blen_data, blen_attr, blen_dtype, fbx_data, stride, item_size, descr,
                                            xform):
    """Generic fbx_layer to blen_data foreach setter for Direct layers.
    blen_data must be a bpy_prop_collection or 2d np.ndarray whose second axis length is item_size.
    fbx_data must be an array.array."""
    fbx_data_np = blen_read_geom_parse_fbx_data(fbx_data, stride, item_size)
    fbx_data_np, enough_data = blen_read_geom_check_fbx_data_length(blen_data, fbx_data_np)
    fbx_data_np = blen_read_geom_xform(fbx_data_np, xform)

    blen_data_is_collection = blen_read_geom_validate_blen_data(blen_data, blen_dtype, item_size)

    if blen_data_is_collection:
        if not enough_data:
            blen_total_data = len(blen_data) * item_size
            buffer = np.empty(blen_total_data, dtype=blen_dtype)
            # It's not clear what values should be used for the missing data, so read the current values into a buffer.
            blen_data.foreach_get(blen_attr, buffer)

            # Change the buffer shape to one item per row
            buffer.shape = (-1, item_size)

            # Copy the fbx data into the start of the buffer
            buffer[:len(fbx_data_np)] = fbx_data_np
        else:
            # Convert the buffer to the Blender C type of blen_attr
            buffer = astype_view_signedness(fbx_data_np, blen_dtype)

        # Set blen_attr of blen_data. The buffer must be flat and C-contiguous, which ravel() ensures
        blen_data.foreach_set(blen_attr, buffer.ravel())
    else:
        assert(blen_data.size % item_size == 0)
        blen_data = blen_data.view()
        blen_data.shape = (-1, item_size)
        blen_data[:len(fbx_data_np)] = fbx_data_np


def blen_read_geom_array_foreach_set_indexed(blen_data, blen_attr, blen_dtype, fbx_data, fbx_layer_index, stride,
                                             item_size, descr, xform):
    """Generic fbx_layer to blen_data foreach setter for IndexToDirect layers.
    blen_data must be a bpy_prop_collection or 2d np.ndarray whose second axis length is item_size.
    fbx_data must be an array.array or a 1d np.ndarray."""
    fbx_data_np = blen_read_geom_parse_fbx_data(fbx_data, stride, item_size)
    fbx_data_np = blen_read_geom_xform(fbx_data_np, xform)

    # fbx_layer_index is allowed to be a 1d np.ndarray for use with blen_read_geom_array_foreach_set_looptovert.
    if not isinstance(fbx_layer_index, np.ndarray):
        fbx_layer_index = parray_as_ndarray(fbx_layer_index)

    fbx_layer_index, enough_indices = blen_read_geom_check_fbx_data_length(blen_data, fbx_layer_index, is_indices=True)

    blen_data_is_collection = blen_read_geom_validate_blen_data(blen_data, blen_dtype, item_size)

    blen_data_items_len = len(blen_data)
    blen_data_len = blen_data_items_len * item_size
    fbx_num_items = len(fbx_data_np)

    # Find all indices that are out of bounds of fbx_data_np.
    min_index_inclusive = -fbx_num_items
    max_index_inclusive = fbx_num_items - 1
    valid_index_mask = np.equal(fbx_layer_index, fbx_layer_index.clip(min_index_inclusive, max_index_inclusive))
    indices_invalid = not valid_index_mask.all()

    fbx_data_items = fbx_data_np.reshape(-1, item_size)

    if indices_invalid or not enough_indices:
        if blen_data_is_collection:
            buffer = np.empty(blen_data_len, dtype=blen_dtype)
            buffer_item_view = buffer.view()
            buffer_item_view.shape = (-1, item_size)
            # Since we don't know what the default values should be for the missing data, read the current values into a
            # buffer.
            blen_data.foreach_get(blen_attr, buffer)
        else:
            buffer_item_view = blen_data

        if not enough_indices:
            # Reduce the length of the view to the same length as the number of indices.
            buffer_item_view = buffer_item_view[:len(fbx_layer_index)]

        # Copy the result of indexing fbx_data_items by each element in fbx_layer_index into the buffer.
        if indices_invalid:
            print("ERROR: indices in this FBX layer out of bounds of the FBX data, skipping invalid indices!")
            buffer_item_view[valid_index_mask] = fbx_data_items[fbx_layer_index[valid_index_mask]]
        else:
            buffer_item_view[:] = fbx_data_items[fbx_layer_index]

        if blen_data_is_collection:
            blen_data.foreach_set(blen_attr, buffer.ravel())
    else:
        if blen_data_is_collection:
            # Cast the buffer to the Blender C type of blen_attr
            fbx_data_items = astype_view_signedness(fbx_data_items, blen_dtype)
            buffer_items = fbx_data_items[fbx_layer_index]
            blen_data.foreach_set(blen_attr, buffer_items.ravel())
        else:
            blen_data[:] = fbx_data_items[fbx_layer_index]


def blen_read_geom_array_foreach_set_allsame(blen_data, blen_attr, blen_dtype, fbx_data, stride, item_size, descr,
                                             xform):
    """Generic fbx_layer to blen_data foreach setter for AllSame layers.
    blen_data must be a bpy_prop_collection or 2d np.ndarray whose second axis length is item_size.
    fbx_data must be an array.array."""
    fbx_data_np = blen_read_geom_parse_fbx_data(fbx_data, stride, item_size)
    fbx_data_np = blen_read_geom_xform(fbx_data_np, xform)
    blen_data_is_collection = blen_read_geom_validate_blen_data(blen_data, blen_dtype, item_size)
    fbx_items_len = len(fbx_data_np)
    blen_items_len = len(blen_data)

    if fbx_items_len < 1:
        print("ERROR: not enough data in this FBX layer, skipping!")
        return

    if blen_data_is_collection:
        # Create an array filled with the value from fbx_data_np
        buffer = np.full((blen_items_len, item_size), fbx_data_np[0], dtype=blen_dtype)

        blen_data.foreach_set(blen_attr, buffer.ravel())
    else:
        blen_data[:] = fbx_data_np[0]


def blen_read_geom_array_foreach_set_looptovert(mesh, blen_data, blen_attr, blen_dtype, fbx_data, stride, item_size,
                                                descr, xform):
    """Generic fbx_layer to blen_data foreach setter for face corner ByVertice layers.
    blen_data must be a bpy_prop_collection or 2d np.ndarray whose second axis length is item_size.
    fbx_data must be an array.array"""
    # The fbx_data is mapped to vertices. To expand fbx_data to face corners, get an array of the vertex index of each
    # face corner that will then be used to index fbx_data.
    corner_vertex_indices = MESH_ATTRIBUTE_CORNER_VERT.to_ndarray(mesh.attributes)
    blen_read_geom_array_foreach_set_indexed(blen_data, blen_attr, blen_dtype, fbx_data, corner_vertex_indices, stride,
                                             item_size, descr, xform)


# generic error printers.
def blen_read_geom_array_error_mapping(descr, fbx_layer_mapping, quiet=False):
    if not quiet:
        print("warning layer %r mapping type unsupported: %r" % (descr, fbx_layer_mapping))


def blen_read_geom_array_error_ref(descr, fbx_layer_ref, quiet=False):
    if not quiet:
        print("warning layer %r ref type unsupported: %r" % (descr, fbx_layer_ref))


def blen_read_geom_array_mapped_vert(
        mesh, blen_data, blen_attr, blen_dtype,
        fbx_layer_data, fbx_layer_index,
        fbx_layer_mapping, fbx_layer_ref,
        stride, item_size, descr,
        xform=None, quiet=False,
):
    if fbx_layer_mapping == b'ByVertice':
        if fbx_layer_ref == b'IndexToDirect':
            # XXX Looks like we often get no fbx_layer_index in this case, shall not happen but happens...
            #     We fallback to 'Direct' mapping in this case.
            # ~ assert(fbx_layer_index is not None)
            if fbx_layer_index is None:
                blen_read_geom_array_foreach_set_direct(blen_data, blen_attr, blen_dtype, fbx_layer_data, stride,
                                                        item_size, descr, xform)
            else:
                blen_read_geom_array_foreach_set_indexed(blen_data, blen_attr, blen_dtype, fbx_layer_data,
                                                         fbx_layer_index, stride, item_size, descr, xform)
            return True
        elif fbx_layer_ref == b'Direct':
            assert(fbx_layer_index is None)
            blen_read_geom_array_foreach_set_direct(blen_data, blen_attr, blen_dtype, fbx_layer_data, stride, item_size,
                                                    descr, xform)
            return True
        blen_read_geom_array_error_ref(descr, fbx_layer_ref, quiet)
    elif fbx_layer_mapping == b'AllSame':
        if fbx_layer_ref == b'IndexToDirect':
            assert(fbx_layer_index is None)
            blen_read_geom_array_foreach_set_allsame(blen_data, blen_attr, blen_dtype, fbx_layer_data, stride,
                                                     item_size, descr, xform)
            return True
        blen_read_geom_array_error_ref(descr, fbx_layer_ref, quiet)
    else:
        blen_read_geom_array_error_mapping(descr, fbx_layer_mapping, quiet)

    return False


def blen_read_geom_array_mapped_edge(
        mesh, blen_data, blen_attr, blen_dtype,
        fbx_layer_data, fbx_layer_index,
        fbx_layer_mapping, fbx_layer_ref,
        stride, item_size, descr,
        xform=None, quiet=False,
):
    if fbx_layer_mapping == b'ByEdge':
        if fbx_layer_ref == b'Direct':
            blen_read_geom_array_foreach_set_direct(blen_data, blen_attr, blen_dtype, fbx_layer_data, stride, item_size,
                                                    descr, xform)
            return True
        blen_read_geom_array_error_ref(descr, fbx_layer_ref, quiet)
    elif fbx_layer_mapping == b'AllSame':
        if fbx_layer_ref == b'IndexToDirect':
            assert(fbx_layer_index is None)
            blen_read_geom_array_foreach_set_allsame(blen_data, blen_attr, blen_dtype, fbx_layer_data, stride,
                                                     item_size, descr, xform)
            return True
        blen_read_geom_array_error_ref(descr, fbx_layer_ref, quiet)
    else:
        blen_read_geom_array_error_mapping(descr, fbx_layer_mapping, quiet)

    return False


def blen_read_geom_array_mapped_polygon(
        mesh, blen_data, blen_attr, blen_dtype,
        fbx_layer_data, fbx_layer_index,
        fbx_layer_mapping, fbx_layer_ref,
        stride, item_size, descr,
        xform=None, quiet=False,
):
    if fbx_layer_mapping == b'ByPolygon':
        if fbx_layer_ref == b'IndexToDirect':
            # XXX Looks like we often get no fbx_layer_index in this case, shall not happen but happens...
            #     We fallback to 'Direct' mapping in this case.
            # ~ assert(fbx_layer_index is not None)
            if fbx_layer_index is None:
                blen_read_geom_array_foreach_set_direct(blen_data, blen_attr, blen_dtype, fbx_layer_data, stride,
                                                        item_size, descr, xform)
            else:
                blen_read_geom_array_foreach_set_indexed(blen_data, blen_attr, blen_dtype, fbx_layer_data,
                                                         fbx_layer_index, stride, item_size, descr, xform)
            return True
        elif fbx_layer_ref == b'Direct':
            blen_read_geom_array_foreach_set_direct(blen_data, blen_attr, blen_dtype, fbx_layer_data, stride, item_size,
                                                    descr, xform)
            return True
        blen_read_geom_array_error_ref(descr, fbx_layer_ref, quiet)
    elif fbx_layer_mapping == b'AllSame':
        if fbx_layer_ref == b'IndexToDirect':
            assert(fbx_layer_index is None)
            blen_read_geom_array_foreach_set_allsame(blen_data, blen_attr, blen_dtype, fbx_layer_data, stride,
                                                     item_size, descr, xform)
            return True
        blen_read_geom_array_error_ref(descr, fbx_layer_ref, quiet)
    else:
        blen_read_geom_array_error_mapping(descr, fbx_layer_mapping, quiet)

    return False


def blen_read_geom_array_mapped_polyloop(
        mesh, blen_data, blen_attr, blen_dtype,
        fbx_layer_data, fbx_layer_index,
        fbx_layer_mapping, fbx_layer_ref,
        stride, item_size, descr,
        xform=None, quiet=False,
):
    if fbx_layer_mapping == b'ByPolygonVertex':
        if fbx_layer_ref == b'IndexToDirect':
            # XXX Looks like we often get no fbx_layer_index in this case, shall not happen but happens...
            #     We fallback to 'Direct' mapping in this case.
            # ~ assert(fbx_layer_index is not None)
            if fbx_layer_index is None:
                blen_read_geom_array_foreach_set_direct(blen_data, blen_attr, blen_dtype, fbx_layer_data, stride,
                                                        item_size, descr, xform)
            else:
                blen_read_geom_array_foreach_set_indexed(blen_data, blen_attr, blen_dtype, fbx_layer_data,
                                                         fbx_layer_index, stride, item_size, descr, xform)
            return True
        elif fbx_layer_ref == b'Direct':
            blen_read_geom_array_foreach_set_direct(blen_data, blen_attr, blen_dtype, fbx_layer_data, stride, item_size,
                                                    descr, xform)
            return True
        blen_read_geom_array_error_ref(descr, fbx_layer_ref, quiet)
    elif fbx_layer_mapping == b'ByVertice':
        if fbx_layer_ref == b'Direct':
            assert(fbx_layer_index is None)
            blen_read_geom_array_foreach_set_looptovert(mesh, blen_data, blen_attr, blen_dtype, fbx_layer_data, stride,
                                                        item_size, descr, xform)
            return True
        blen_read_geom_array_error_ref(descr, fbx_layer_ref, quiet)
    elif fbx_layer_mapping == b'AllSame':
        if fbx_layer_ref == b'IndexToDirect':
            assert(fbx_layer_index is None)
            blen_read_geom_array_foreach_set_allsame(blen_data, blen_attr, blen_dtype, fbx_layer_data, stride,
                                                     item_size, descr, xform)
            return True
        blen_read_geom_array_error_ref(descr, fbx_layer_ref, quiet)
    else:
        blen_read_geom_array_error_mapping(descr, fbx_layer_mapping, quiet)

    return False


def blen_read_geom_layer_material(fbx_obj, mesh):
    fbx_layer = elem_find_first(fbx_obj, b'LayerElementMaterial')

    if fbx_layer is None:
        return

    (fbx_layer_name,
     fbx_layer_mapping,
     fbx_layer_ref,
     ) = blen_read_geom_layerinfo(fbx_layer)

    layer_id = b'Materials'
    fbx_layer_data = elem_prop_first(elem_find_first(fbx_layer, layer_id))

    blen_data = MESH_ATTRIBUTE_MATERIAL_INDEX.ensure(mesh.attributes).data
    fbx_item_size = 1
    assert(fbx_item_size == MESH_ATTRIBUTE_MATERIAL_INDEX.item_size)
    blen_read_geom_array_mapped_polygon(
        mesh, blen_data, MESH_ATTRIBUTE_MATERIAL_INDEX.foreach_attribute, MESH_ATTRIBUTE_MATERIAL_INDEX.dtype,
        fbx_layer_data, None,
        fbx_layer_mapping, fbx_layer_ref,
        1, fbx_item_size, layer_id,
    )


def blen_read_geom_layer_uv(fbx_obj, mesh):
    for layer_id in (b'LayerElementUV',):
        for fbx_layer in elem_find_iter(fbx_obj, layer_id):
            # all should be valid
            (fbx_layer_name,
             fbx_layer_mapping,
             fbx_layer_ref,
             ) = blen_read_geom_layerinfo(fbx_layer)

            fbx_layer_data = elem_prop_first(elem_find_first(fbx_layer, b'UV'))
            fbx_layer_index = elem_prop_first(elem_find_first(fbx_layer, b'UVIndex'))

            # Always init our new layers with (0, 0) UVs.
            uv_lay = mesh.uv_layers.new(name=fbx_layer_name, do_init=False)
            if uv_lay is None:
                print("Failed to add {%r %r} UVLayer to %r (probably too many of them?)"
                      "" % (layer_id, fbx_layer_name, mesh.name))
                continue

            blen_data = uv_lay.uv

            # some valid files omit this data
            if fbx_layer_data is None:
                print("%r %r missing data" % (layer_id, fbx_layer_name))
                continue

            blen_read_geom_array_mapped_polyloop(
                mesh, blen_data, "vector", np.single,
                fbx_layer_data, fbx_layer_index,
                fbx_layer_mapping, fbx_layer_ref,
                2, 2, layer_id,
            )


def blen_read_geom_layer_color(fbx_obj, mesh, colors_type):
    if colors_type == 'NONE':
        return
    use_srgb = colors_type == 'SRGB'
    layer_type = 'BYTE_COLOR' if use_srgb else 'FLOAT_COLOR'
    color_prop_name = "color_srgb" if use_srgb else "color"
    # almost same as UVs
    for layer_id in (b'LayerElementColor',):
        for fbx_layer in elem_find_iter(fbx_obj, layer_id):
            # all should be valid
            (fbx_layer_name,
             fbx_layer_mapping,
             fbx_layer_ref,
             ) = blen_read_geom_layerinfo(fbx_layer)

            fbx_layer_data = elem_prop_first(elem_find_first(fbx_layer, b'Colors'))
            fbx_layer_index = elem_prop_first(elem_find_first(fbx_layer, b'ColorIndex'))

            color_lay = mesh.color_attributes.new(name=fbx_layer_name, type=layer_type, domain='CORNER')

            if color_lay is None:
                print("Failed to add {%r %r} vertex color layer to %r (probably too many of them?)"
                      "" % (layer_id, fbx_layer_name, mesh.name))
                continue

            blen_data = color_lay.data

            # some valid files omit this data
            if fbx_layer_data is None:
                print("%r %r missing data" % (layer_id, fbx_layer_name))
                continue

            blen_read_geom_array_mapped_polyloop(
                mesh, blen_data, color_prop_name, np.single,
                fbx_layer_data, fbx_layer_index,
                fbx_layer_mapping, fbx_layer_ref,
                4, 4, layer_id,
            )


def blen_read_geom_layer_smooth(fbx_obj, mesh):
    fbx_layer = elem_find_first(fbx_obj, b'LayerElementSmoothing')

    if fbx_layer is None:
        return

    # all should be valid
    (fbx_layer_name,
     fbx_layer_mapping,
     fbx_layer_ref,
     ) = blen_read_geom_layerinfo(fbx_layer)

    layer_id = b'Smoothing'
    fbx_layer_data = elem_prop_first(elem_find_first(fbx_layer, layer_id))

    # udk has 'Direct' mapped, with no Smoothing, not sure why, but ignore these
    if fbx_layer_data is None:
        return

    if fbx_layer_mapping == b'ByEdge':
        # some models have bad edge data, we can't use this info...
        if not mesh.edges:
            print("warning skipping sharp edges data, no valid edges...")
            return

        blen_data = MESH_ATTRIBUTE_SHARP_EDGE.ensure(mesh.attributes).data
        fbx_item_size = 1
        assert(fbx_item_size == MESH_ATTRIBUTE_SHARP_EDGE.item_size)
        blen_read_geom_array_mapped_edge(
            mesh, blen_data, MESH_ATTRIBUTE_SHARP_EDGE.foreach_attribute, MESH_ATTRIBUTE_SHARP_EDGE.dtype,
            fbx_layer_data, None,
            fbx_layer_mapping, fbx_layer_ref,
            1, fbx_item_size, layer_id,
            xform=np.logical_not,  # in FBX, 0 (False) is sharp, but in Blender True is sharp.
        )
    elif fbx_layer_mapping == b'ByPolygon':
        sharp_face = MESH_ATTRIBUTE_SHARP_FACE.ensure(mesh.attributes)
        blen_data = sharp_face.data
        fbx_item_size = 1
        assert(fbx_item_size == MESH_ATTRIBUTE_SHARP_FACE.item_size)
        sharp_face_set_successfully = blen_read_geom_array_mapped_polygon(
            mesh, blen_data, MESH_ATTRIBUTE_SHARP_FACE.foreach_attribute, MESH_ATTRIBUTE_SHARP_FACE.dtype,
            fbx_layer_data, None,
            fbx_layer_mapping, fbx_layer_ref,
            1, fbx_item_size, layer_id,
            xform=lambda s: (s == 0),  # smoothgroup bitflags, treat as booleans for now
        )
        if not sharp_face_set_successfully:
            mesh.attributes.remove(sharp_face)
    else:
        print("warning layer %r mapping type unsupported: %r" % (fbx_layer.id, fbx_layer_mapping))


def blen_read_geom_layer_edge_crease(fbx_obj, mesh):
    fbx_layer = elem_find_first(fbx_obj, b'LayerElementEdgeCrease')

    if fbx_layer is None:
        return False

    # all should be valid
    (fbx_layer_name,
     fbx_layer_mapping,
     fbx_layer_ref,
     ) = blen_read_geom_layerinfo(fbx_layer)

    if fbx_layer_mapping != b'ByEdge':
        return False

    layer_id = b'EdgeCrease'
    fbx_layer_data = elem_prop_first(elem_find_first(fbx_layer, layer_id))

    # some models have bad edge data, we can't use this info...
    if not mesh.edges:
        print("warning skipping edge crease data, no valid edges...")
        return False

    if fbx_layer_mapping == b'ByEdge':
        # some models have bad edge data, we can't use this info...
        if not mesh.edges:
            print("warning skipping edge crease data, no valid edges...")
            return False

        blen_data = mesh.edge_creases_ensure().data
        return blen_read_geom_array_mapped_edge(
            mesh, blen_data, "value", np.single,
            fbx_layer_data, None,
            fbx_layer_mapping, fbx_layer_ref,
            1, 1, layer_id,
            # Blender squares those values before sending them to OpenSubdiv, when other software don't,
            # so we need to compensate that to get similar results through FBX...
            xform=np.sqrt,
        )
    else:
        print("warning layer %r mapping type unsupported: %r" % (fbx_layer.id, fbx_layer_mapping))
        return False


def blen_read_geom_layer_normal(fbx_obj, mesh, xform=None):
    fbx_layer = elem_find_first(fbx_obj, b'LayerElementNormal')

    if fbx_layer is None:
        return False

    (fbx_layer_name,
     fbx_layer_mapping,
     fbx_layer_ref,
     ) = blen_read_geom_layerinfo(fbx_layer)

    layer_id = b'Normals'
    fbx_layer_data = elem_prop_first(elem_find_first(fbx_layer, layer_id))
    fbx_layer_index = elem_prop_first(elem_find_first(fbx_layer, b'NormalsIndex'))

    if fbx_layer_data is None:
        print("warning %r %r missing data" % (layer_id, fbx_layer_name))
        return False

    # Normals are temporarily set here so that they can be retrieved again after a call to Mesh.validate().
    bl_norm_dtype = np.single
    item_size = 3
    # try loops, then polygons, then vertices.
    tries = ((mesh.attributes["temp_custom_normals"].data, "Loops", False, blen_read_geom_array_mapped_polyloop),
             (mesh.polygons, "Polygons", True, blen_read_geom_array_mapped_polygon),
             (mesh.vertices, "Vertices", True, blen_read_geom_array_mapped_vert))
    for blen_data, blen_data_type, is_fake, func in tries:
        bdata = np.zeros((len(blen_data), item_size), dtype=bl_norm_dtype) if is_fake else blen_data
        if func(mesh, bdata, "vector", bl_norm_dtype,
                fbx_layer_data, fbx_layer_index, fbx_layer_mapping, fbx_layer_ref, 3, item_size, layer_id, xform, True):
            if blen_data_type == "Polygons":
                # To expand to per-loop normals, repeat each per-polygon normal by the number of loops of each polygon.
                poly_loop_totals = np.empty(len(mesh.polygons), dtype=np.uintc)
                mesh.polygons.foreach_get("loop_total", poly_loop_totals)
                loop_normals = np.repeat(bdata, poly_loop_totals, axis=0)
                mesh.attributes["temp_custom_normals"].data.foreach_set("vector", loop_normals.ravel())
            elif blen_data_type == "Vertices":
                # We have to copy vnors to lnors! Far from elegant, but simple.
                loop_vertex_indices = MESH_ATTRIBUTE_CORNER_VERT.to_ndarray(mesh.attributes)
                mesh.attributes["temp_custom_normals"].data.foreach_set("vector", bdata[loop_vertex_indices].ravel())
            return True

    blen_read_geom_array_error_mapping("normal", fbx_layer_mapping)
    blen_read_geom_array_error_ref("normal", fbx_layer_ref)
    return False


def blen_read_geom(fbx_tmpl, fbx_obj, settings):
    # Vertices are in object space, but we are post-multiplying all transforms with the inverse of the
    # global matrix, so we need to apply the global matrix to the vertices to get the correct result.
    geom_mat_co = settings.global_matrix if settings.bake_space_transform else None
    # We need to apply the inverse transpose of the global matrix when transforming normals.
    geom_mat_no = Matrix(settings.global_matrix_inv_transposed) if settings.bake_space_transform else None
    if geom_mat_no is not None:
        # Remove translation & scaling!
        geom_mat_no.translation = Vector()
        geom_mat_no.normalize()

    # TODO, use 'fbx_tmpl'
    elem_name_utf8 = elem_name_ensure_class(fbx_obj, b'Geometry')

    fbx_verts = elem_prop_first(elem_find_first(fbx_obj, b'Vertices'))
    fbx_polys = elem_prop_first(elem_find_first(fbx_obj, b'PolygonVertexIndex'))
    fbx_edges = elem_prop_first(elem_find_first(fbx_obj, b'Edges'))

    # The dtypes when empty don't matter, but are set to what the fbx arrays are expected to be.
    fbx_verts = parray_as_ndarray(fbx_verts) if fbx_verts else np.empty(0, dtype=data_types.ARRAY_FLOAT64)
    fbx_polys = parray_as_ndarray(fbx_polys) if fbx_polys else np.empty(0, dtype=data_types.ARRAY_INT32)
    fbx_edges = parray_as_ndarray(fbx_edges) if fbx_edges else np.empty(0, dtype=data_types.ARRAY_INT32)

    # Each vert is a 3d vector so is made of 3 components.
    tot_verts = len(fbx_verts) // 3
    if tot_verts * 3 != len(fbx_verts):
        print("ERROR: Not a whole number of vertices. Ignoring the partial vertex!")
        # Remove any remainder.
        fbx_verts = fbx_verts[:tot_verts * 3]

    tot_loops = len(fbx_polys)
    tot_edges = len(fbx_edges)

    mesh = bpy.data.meshes.new(name=elem_name_utf8)
    attributes = mesh.attributes

    if tot_verts:
        if geom_mat_co is not None:
            fbx_verts = vcos_transformed(fbx_verts, geom_mat_co, MESH_ATTRIBUTE_POSITION.dtype)
        else:
            fbx_verts = fbx_verts.astype(MESH_ATTRIBUTE_POSITION.dtype, copy=False)

        mesh.vertices.add(tot_verts)
        MESH_ATTRIBUTE_POSITION.foreach_set(attributes, fbx_verts.ravel())

    if tot_loops:
        bl_loop_start_dtype = np.uintc

        mesh.loops.add(tot_loops)
        # The end of each polygon is specified by an inverted index.
        fbx_loop_end_idx = np.flatnonzero(fbx_polys < 0)

        tot_polys = len(fbx_loop_end_idx)

        # Un-invert the loop ends.
        fbx_polys[fbx_loop_end_idx] ^= -1
        # Set loop vertex indices, casting to the Blender C type first for performance.
        MESH_ATTRIBUTE_CORNER_VERT.foreach_set(
            attributes, astype_view_signedness(fbx_polys, MESH_ATTRIBUTE_CORNER_VERT.dtype))

        poly_loop_starts = np.empty(tot_polys, dtype=bl_loop_start_dtype)
        # The first loop is always a loop start.
        poly_loop_starts[0] = 0
        # Ignoring the last loop end, the indices after every loop end are the remaining loop starts.
        poly_loop_starts[1:] = fbx_loop_end_idx[:-1] + 1

        mesh.polygons.add(tot_polys)
        mesh.polygons.foreach_set("loop_start", poly_loop_starts)

        blen_read_geom_layer_material(fbx_obj, mesh)
        blen_read_geom_layer_uv(fbx_obj, mesh)
        blen_read_geom_layer_color(fbx_obj, mesh, settings.colors_type)

        if tot_edges:
            # edges in fact index the polygons (NOT the vertices)

            # The first vertex index of each edge is the vertex index of the corresponding loop in fbx_polys.
            edges_a = fbx_polys[fbx_edges]

            # The second vertex index of each edge is the vertex index of the next loop in the same polygon. The
            # complexity here is that if the first vertex index was the last loop of that polygon in fbx_polys, the next
            # loop in the polygon is the first loop of that polygon, which is not the next loop in fbx_polys.

            # Copy fbx_polys, but rolled backwards by 1 so that indexing the result by [fbx_edges] will get the next
            # loop of the same polygon unless the first vertex index was the last loop of the polygon.
            fbx_polys_next = np.roll(fbx_polys, -1)
            # Get the first loop of each polygon and set them into fbx_polys_next at the same indices as the last loop
            # of each polygon in fbx_polys.
            fbx_polys_next[fbx_loop_end_idx] = fbx_polys[poly_loop_starts]

            # Indexing fbx_polys_next by fbx_edges now gets the vertex index of the next loop in fbx_polys.
            edges_b = fbx_polys_next[fbx_edges]

            # edges_a and edges_b need to be combined so that the first vertex index of each edge is immediately
            # followed by the second vertex index of that same edge.
            # Stack edges_a and edges_b as individual columns like np.column_stack((edges_a, edges_b)).
            # np.concatenate is used because np.column_stack doesn't allow specifying the dtype of the returned array.
            edges_conv = np.concatenate((edges_a.reshape(-1, 1), edges_b.reshape(-1, 1)),
                                        axis=1, dtype=MESH_ATTRIBUTE_EDGE_VERTS.dtype, casting='unsafe')

            # Add the edges and set their vertex indices.
            mesh.edges.add(len(edges_conv))
            # ravel() because edges_conv must be flat and C-contiguous when passed to foreach_set.
            MESH_ATTRIBUTE_EDGE_VERTS.foreach_set(attributes, edges_conv.ravel())
    elif tot_edges:
        print("ERROR: No polygons, but edges exist. Ignoring the edges!")

    # must be after edge, face loading.
    blen_read_geom_layer_smooth(fbx_obj, mesh)

    blen_read_geom_layer_edge_crease(fbx_obj, mesh)

    ok_normals = False
    if settings.use_custom_normals:
        # Note: we store 'temp' normals in loops, since validate() may alter final mesh,
        #       we can only set custom lnors *after* calling it.
        mesh.attributes.new("temp_custom_normals", 'FLOAT_VECTOR', 'CORNER')
        if geom_mat_no is None:
            ok_normals = blen_read_geom_layer_normal(fbx_obj, mesh)
        else:
            ok_normals = blen_read_geom_layer_normal(fbx_obj, mesh,
                                                     lambda v_array: nors_transformed(v_array, geom_mat_no))

    mesh.validate(clean_customdata=False)  # *Very* important to not remove lnors here!

    if ok_normals:
        bl_nors_dtype = np.single
        clnors = np.empty(len(mesh.loops) * 3, dtype=bl_nors_dtype)
        mesh.attributes["temp_custom_normals"].data.foreach_get("vector", clnors)

        # Iterating clnors into a nested tuple first is faster than passing clnors.reshape(-1, 3) directly into
        # normals_split_custom_set. We use clnors.data since it is a memoryview, which is faster to iterate than clnors.
        mesh.normals_split_custom_set(tuple(zip(*(iter(clnors.data),) * 3)))
    if settings.use_custom_normals:
        mesh.attributes.remove(mesh.attributes["temp_custom_normals"])

    if settings.use_custom_props:
        blen_read_custom_properties(fbx_obj, mesh, settings)

    return mesh


def blen_read_shapes(fbx_tmpl, fbx_data, objects, me, scene):
    if not fbx_data:
        # No shape key data. Nothing to do.
        return

    me_vcos = MESH_ATTRIBUTE_POSITION.to_ndarray(me.attributes)
    me_vcos_vector_view = me_vcos.reshape(-1, 3)

    objects = list({node.bl_obj for node in objects})
    assert(objects)

    # Blender has a hard minimum and maximum shape key Value. If an imported shape key has a value outside this range it
    # will be clamped, and we'll print a warning message to the console.
    shape_key_values_in_range = True
    bc_uuid_to_keyblocks = {}
    for bc_uuid, fbx_sdata, fbx_bcdata, shapes_assigned_to_channel in fbx_data:
        num_shapes_assigned_to_channel = len(shapes_assigned_to_channel)
        if num_shapes_assigned_to_channel > 1:
            # Relevant design task: #104698
            raise RuntimeError("FBX in-between Shapes are not currently supported")  # See bug report #84111
        elem_name_utf8 = elem_name_ensure_class(fbx_sdata, b'Geometry')
        indices = elem_prop_first(elem_find_first(fbx_sdata, b'Indexes'))
        dvcos = elem_prop_first(elem_find_first(fbx_sdata, b'Vertices'))

        indices = parray_as_ndarray(indices) if indices else np.empty(0, dtype=data_types.ARRAY_INT32)
        dvcos = parray_as_ndarray(dvcos) if dvcos else np.empty(0, dtype=data_types.ARRAY_FLOAT64)

        # If there's not a whole number of vectors, trim off the remainder.
        # 3 components per vector.
        remainder = len(dvcos) % 3
        if remainder:
            dvcos = dvcos[:-remainder]
        dvcos = dvcos.reshape(-1, 3)

        # There must be the same number of indices as vertex coordinate differences.
        assert(len(indices) == len(dvcos))

        # We completely ignore normals here!
        weight = elem_prop_first(elem_find_first(fbx_bcdata, b'DeformPercent'), default=100.0) / 100.0

        # The FullWeights array stores the deformation percentages of the BlendShapeChannel that fully activate each
        # Shape assigned to the BlendShapeChannel. Blender also uses this array to store Vertex Group weights, but this
        # is not part of the FBX standard.
        full_weights = elem_prop_first(elem_find_first(fbx_bcdata, b'FullWeights'))
        full_weights = parray_as_ndarray(full_weights) if full_weights else np.empty(0, dtype=data_types.ARRAY_FLOAT64)

        # Special case for Blender exported Shape Keys with a Vertex Group assigned. The Vertex Group weights are stored
        # in the FullWeights array.
        # XXX - It's possible, though very rare, to get a false positive here and create a Vertex Group when we
        #       shouldn't. This should only be possible when there are extraneous FullWeights or when there is a single
        #       FullWeight and its value is not 100.0.
        if (
                # Blender exported Shape Keys only ever export as 1 Shape per BlendShapeChannel.
                num_shapes_assigned_to_channel == 1
                # There should be one vertex weight for each vertex moved by the Shape.
                and len(full_weights) == len(indices)
                # Skip creating a Vertex Group when all the weights are 100.0 because such a Vertex Group has no effect.
                # This also avoids creating a Vertex Group for imported Shapes that only move a single vertex because
                # their BlendShapeChannel's singular FullWeight is expected to always be 100.0.
                and not np.all(full_weights == 100.0)
                # Blender vertex weights are always within the [0.0, 1.0] range (scaled to [0.0, 100.0] when saving to
                # FBX). This can eliminate imported BlendShapeChannels from Unreal that have extraneous FullWeights
                # because the extraneous values are usually negative.
                and np.all((full_weights >= 0.0) & (full_weights <= 100.0))
        ):
            # Not doing the division in-place because it's technically possible for FBX BlendShapeChannels to be used by
            # more than one FBX BlendShape, though this shouldn't be the case for Blender exported Shape Keys.
            vgweights = full_weights / 100.0
        else:
            vgweights = None
            # There must be a FullWeight for each Shape. Any extra FullWeights are ignored.
            assert(len(full_weights) >= num_shapes_assigned_to_channel)

        # To add shape keys to the mesh, an Object using the mesh is needed.
        if me.shape_keys is None:
            objects[0].shape_key_add(name="Basis", from_mix=False)
        kb = objects[0].shape_key_add(name=elem_name_utf8, from_mix=False)
        kb.value = 0.0
        me.shape_keys.use_relative = True  # Should already be set as such.

        # Only need to set the shape key co if there are any non-zero dvcos.
        if dvcos.any():
            shape_cos = me_vcos_vector_view.copy()
            shape_cos[indices] += dvcos
            kb.points.foreach_set("co", shape_cos.ravel())

        shape_key_values_in_range &= expand_shape_key_range(kb, weight)

        kb.value = weight

        # Add vgroup if necessary.
        if vgweights is not None:
            # VertexGroup.add only allows sequences of int indices, but iterating the indices array directly would
            # produce numpy scalars of types such as np.int32. The underlying memoryview of the indices array, however,
            # does produce standard Python ints when iterated, so pass indices.data to add_vgroup_to_objects instead of
            # indices.
            # memoryviews tend to be faster to iterate than numpy arrays anyway, so vgweights.data is passed too.
            add_vgroup_to_objects(indices.data, vgweights.data, kb.name, objects)
            kb.vertex_group = kb.name

        bc_uuid_to_keyblocks.setdefault(bc_uuid, []).append(kb)

    if not shape_key_values_in_range:
        print("WARNING: The imported Value of a Shape Key on the Mesh '%s' is beyond the minimum/maximum allowed and"
              " has been clamped." % me.name)

    return bc_uuid_to_keyblocks


# --------
# Material

def blen_read_material(fbx_tmpl, fbx_obj, settings):
    from bpy_extras import node_shader_utils
    from math import sqrt

    elem_name_utf8 = elem_name_ensure_class(fbx_obj, b'Material')

    if settings.mtl_name_collision_mode == "REFERENCE_EXISTING":
        if (ma := bpy.data.materials.get(elem_name_utf8)):
            return ma

    nodal_material_wrap_map = settings.nodal_material_wrap_map
    ma = bpy.data.materials.new(name=elem_name_utf8)

    const_color_white = 1.0, 1.0, 1.0
    const_color_black = 0.0, 0.0, 0.0

    fbx_props = (elem_find_first(fbx_obj, b'Properties70'),
                 elem_find_first(fbx_tmpl, b'Properties70', fbx_elem_nil))
    fbx_props_no_template = (fbx_props[0], fbx_elem_nil)

    ma_wrap = node_shader_utils.PrincipledBSDFWrapper(ma, is_readonly=False)
    ma_wrap.base_color = elem_props_get_color_rgb(fbx_props, b'DiffuseColor', const_color_white)
    # No specular color in Principled BSDF shader, assumed to be either white or take some tint from diffuse one...
    # TODO: add way to handle tint option (guesstimate from spec color + intensity...)?
    ma_wrap.specular = elem_props_get_number(fbx_props, b'SpecularFactor', 0.25) * 2.0
    # XXX Totally empirical conversion, trying to adapt it (and protect against invalid negative values, see T96076):
    #     From [1.0 - 0.0] Principled BSDF range to [0.0 - 100.0] FBX shininess range)...
    fbx_shininess = max(elem_props_get_number(fbx_props, b'Shininess', 20.0), 0.0)
    ma_wrap.roughness = 1.0 - (sqrt(fbx_shininess) / 10.0)
    # Sweetness... Looks like we are not the only ones to not know exactly how FBX is supposed to work (see T59850).
    # According to one of its developers, Unity uses that formula to extract alpha value:
    #
    #   alpha = 1 - TransparencyFactor
    #   if (alpha == 1 or alpha == 0):
    #       alpha = 1 - TransparentColor.r
    #
    # Until further info, let's assume this is correct way to do, hence the following code for TransparentColor.
    # However, there are some cases (from 3DSMax, see T65065), where we do have TransparencyFactor only defined
    # in the template to 0.0, and then materials defining TransparentColor to pure white (1.0, 1.0, 1.0),
    # and setting alpha value in Opacity... try to cope with that too. :((((
    alpha = 1.0 - elem_props_get_number(fbx_props, b'TransparencyFactor', 0.0)
    if (alpha == 1.0 or alpha == 0.0):
        alpha = elem_props_get_number(fbx_props_no_template, b'Opacity', None)
        if alpha is None:
            alpha = 1.0 - elem_props_get_color_rgb(fbx_props, b'TransparentColor', const_color_black)[0]
    ma_wrap.alpha = alpha
    ma_wrap.metallic = elem_props_get_number(fbx_props, b'ReflectionFactor', 0.0)
    # We have no metallic (a.k.a. reflection) color...
    # elem_props_get_color_rgb(fbx_props, b'ReflectionColor', const_color_white)
    ma_wrap.normalmap_strength = elem_props_get_number(fbx_props, b'BumpFactor', 1.0)
    # Emission strength and color
    ma_wrap.emission_strength = elem_props_get_number(fbx_props, b'EmissiveFactor', 1.0)
    ma_wrap.emission_color = elem_props_get_color_rgb(fbx_props, b'EmissiveColor', const_color_black)

    nodal_material_wrap_map[ma] = ma_wrap

    if settings.use_custom_props:
        blen_read_custom_properties(fbx_obj, ma, settings)

    return ma


# -------
# Image & Texture

def blen_read_texture_image(fbx_tmpl, fbx_obj, basedir, settings):
    import os
    from bpy_extras import image_utils

    def pack_data_from_content(image, fbx_obj):
        data = elem_find_first_bytes(fbx_obj, b'Content')
        if (data):
            data_len = len(data)
            if (data_len):
                image.pack(data=data, data_len=data_len)

    elem_name_utf8 = elem_name_ensure_classes(fbx_obj, {b'Texture', b'Video'})

    image_cache = settings.image_cache

    # Yet another beautiful logic demonstration by Master FBX:
    # * RelativeFilename in both Video and Texture nodes.
    # * FileName in texture nodes.
    # * Filename in video nodes.
    # Aaaaaaaarrrrrrrrgggggggggggg!!!!!!!!!!!!!!
    filepath = elem_find_first_string(fbx_obj, b'RelativeFilename')
    if filepath:
        # Make sure we do handle a relative path, and not an absolute one (see D5143).
        filepath = filepath.lstrip(os.path.sep).lstrip(os.path.altsep)
        filepath = os.path.join(basedir, filepath)
    else:
        filepath = elem_find_first_string(fbx_obj, b'FileName')
    if not filepath:
        filepath = elem_find_first_string(fbx_obj, b'Filename')
    if not filepath:
        print("Error, could not find any file path in ", fbx_obj)
        print("       Falling back to: ", elem_name_utf8)
        filepath = elem_name_utf8
    else:
        filepath = filepath.replace('\\', '/') if (os.sep == '/') else filepath.replace('/', '\\')

    image = image_cache.get(filepath)
    if image is not None:
        # Data is only embedded once, we may have already created the image but still be missing its data!
        if not image.has_data:
            pack_data_from_content(image, fbx_obj)
        return image

    image = image_utils.load_image(
        filepath,
        dirname=basedir,
        place_holder=True,
        recursive=settings.use_image_search,
    )

    # Try to use embedded data, if available!
    pack_data_from_content(image, fbx_obj)

    image_cache[filepath] = image
    # name can be ../a/b/c
    image.name = os.path.basename(elem_name_utf8)

    if settings.use_custom_props:
        blen_read_custom_properties(fbx_obj, image, settings)

    return image


def blen_read_camera(fbx_tmpl, fbx_obj, settings):
    # meters to inches
    M2I = 0.0393700787

    global_scale = settings.global_scale

    elem_name_utf8 = elem_name_ensure_class(fbx_obj, b'NodeAttribute')

    fbx_props = (elem_find_first(fbx_obj, b'Properties70'),
                 elem_find_first(fbx_tmpl, b'Properties70', fbx_elem_nil))

    camera = bpy.data.cameras.new(name=elem_name_utf8)

    camera.type = 'ORTHO' if elem_props_get_enum(fbx_props, b'CameraProjectionType', 0) == 1 else 'PERSP'

    camera.dof.focus_distance = elem_props_get_number(fbx_props, b'FocusDistance', 10) * global_scale
    if (elem_props_get_bool(fbx_props, b'UseDepthOfField', False)):
        camera.dof.use_dof = True

    camera.lens = elem_props_get_number(fbx_props, b'FocalLength', 35.0)
    camera.sensor_width = elem_props_get_number(fbx_props, b'FilmWidth', 32.0 * M2I) / M2I
    camera.sensor_height = elem_props_get_number(fbx_props, b'FilmHeight', 32.0 * M2I) / M2I

    camera.ortho_scale = elem_props_get_number(fbx_props, b'OrthoZoom', 1.0)

    filmaspect = camera.sensor_width / camera.sensor_height
    # film offset
    camera.shift_x = elem_props_get_number(fbx_props, b'FilmOffsetX', 0.0) / (M2I * camera.sensor_width)
    camera.shift_y = elem_props_get_number(fbx_props, b'FilmOffsetY', 0.0) / (M2I * camera.sensor_height * filmaspect)

    camera.clip_start = elem_props_get_number(fbx_props, b'NearPlane', 0.01) * global_scale
    camera.clip_end = elem_props_get_number(fbx_props, b'FarPlane', 100.0) * global_scale

    if settings.use_custom_props:
        blen_read_custom_properties(fbx_obj, camera, settings)

    return camera


def blen_read_light(fbx_tmpl, fbx_obj, settings):
    import math
    elem_name_utf8 = elem_name_ensure_class(fbx_obj, b'NodeAttribute')

    fbx_props = (elem_find_first(fbx_obj, b'Properties70'),
                 elem_find_first(fbx_tmpl, b'Properties70', fbx_elem_nil))

    light_type = {
        0: 'POINT',
        1: 'SUN',
        2: 'SPOT'}.get(elem_props_get_enum(fbx_props, b'LightType', 0), 'POINT')

    lamp = bpy.data.lights.new(name=elem_name_utf8, type=light_type)

    if light_type == 'SPOT':
        spot_size = elem_props_get_number(fbx_props, b'OuterAngle', None)
        if spot_size is None:
            # Deprecated.
            spot_size = elem_props_get_number(fbx_props, b'Cone angle', 45.0)
        lamp.spot_size = math.radians(spot_size)

        spot_blend = elem_props_get_number(fbx_props, b'InnerAngle', None)
        if spot_blend is None:
            # Deprecated.
            spot_blend = elem_props_get_number(fbx_props, b'HotSpot', 45.0)
        lamp.spot_blend = 1.0 - (spot_blend / spot_size)

    # TODO, cycles nodes???
    lamp.color = elem_props_get_color_rgb(fbx_props, b'Color', (1.0, 1.0, 1.0))
    lamp.energy = elem_props_get_number(fbx_props, b'Intensity', 100.0) / 100.0
    lamp.exposure = elem_props_get_number(fbx_props, b'Exposure', 0.0)
    lamp.use_shadow = elem_props_get_bool(fbx_props, b'CastShadow', True)
    if hasattr(lamp, "cycles"):
        lamp.cycles.cast_shadow = lamp.use_shadow
    # Removed but could be restored if the value can be applied.
    # `lamp.shadow_color = elem_props_get_color_rgb(fbx_props, b'ShadowColor', (0.0, 0.0, 0.0))`

    if settings.use_custom_props:
        blen_read_custom_properties(fbx_obj, lamp, settings)

    return lamp


# ### Import Utility class
class FbxImportHelperNode:
    """
    Temporary helper node to store a hierarchy of fbxNode objects before building Objects, Armatures and Bones.
    It tries to keep the correction data in one place so it can be applied consistently to the imported data.
    """

    __slots__ = (
        '_parent', 'anim_compensation_matrix', 'is_global_animation', 'armature_setup', 'armature', 'bind_matrix',
        'bl_bone', 'bl_data', 'bl_obj', 'bone_child_matrix', 'children', 'clusters',
        'fbx_elem', 'fbx_data_elem', 'fbx_name', 'fbx_transform_data', 'fbx_type',
        'is_armature', 'has_bone_children', 'is_bone', 'is_root', 'is_leaf',
        'matrix', 'matrix_as_parent', 'matrix_geom', 'meshes', 'post_matrix', 'pre_matrix')

    def __init__(self, fbx_elem, bl_data, fbx_transform_data, is_bone):
        self.fbx_name = elem_name_ensure_class(fbx_elem, b'Model') if fbx_elem else 'Unknown'
        self.fbx_type = fbx_elem.props[2] if fbx_elem else None
        self.fbx_elem = fbx_elem
        # FBX elem of a connected NodeAttribute/Geometry for helpers whose bl_data
        # does not exist or is yet to be created.
        self.fbx_data_elem = None
        self.bl_obj = None
        self.bl_data = bl_data
        # Name of bone if this is a bone (this may be different to fbx_name if there was a name conflict in Blender!)
        self.bl_bone = None
        self.fbx_transform_data = fbx_transform_data
        self.is_root = False
        self.is_bone = is_bone
        self.is_armature = False
        self.armature = None                    # For bones only, relevant armature node.
        # True if the hierarchy below this node contains bones, important to support mixed hierarchies.
        self.has_bone_children = False
        # True for leaf-bones added to the end of some bone chains to set the lengths.
        self.is_leaf = False
        self.pre_matrix = None                  # correction matrix that needs to be applied before the FBX transform
        self.bind_matrix = None                 # for bones this is the matrix used to bind to the skin
        if fbx_transform_data:
            self.matrix, self.matrix_as_parent, self.matrix_geom = blen_read_object_transform_do(fbx_transform_data)
        else:
            self.matrix, self.matrix_as_parent, self.matrix_geom = (None, None, None)
        self.post_matrix = None                 # correction matrix that needs to be applied after the FBX transform
        self.bone_child_matrix = None           # Objects attached to a bone end not the beginning, this matrix corrects for that

        # XXX Those two are to handle the fact that rigged meshes are not linked to their armature in FBX, which implies
        #     that their animation is in global space (afaik...).
        #     This is actually not really solvable currently, since anim_compensation_matrix is not valid if armature
        #     itself is animated (we'd have to recompute global-to-local anim_compensation_matrix for each frame,
        #     and for each armature action... beyond being an insane work).
        #     Solution for now: do not read rigged meshes animations at all! sic...
        # a mesh moved in the hierarchy may have a different local matrix. This compensates animations for this.
        self.anim_compensation_matrix = None
        self.is_global_animation = False

        self.meshes = None                      # List of meshes influenced by this bone.
        self.clusters = []                      # Deformer Cluster nodes
        self.armature_setup = {}                # mesh and armature matrix when the mesh was bound

        self._parent = None
        self.children = []

    @property
    def parent(self):
        return self._parent

    @parent.setter
    def parent(self, value):
        if self._parent is not None:
            self._parent.children.remove(self)
        self._parent = value
        if self._parent is not None:
            self._parent.children.append(self)

    @property
    def ignore(self):
        # Separating leaf status from ignore status itself.
        # Currently they are equivalent, but this may change in future.
        return self.is_leaf

    def __repr__(self):
        if self.fbx_elem:
            return self.fbx_elem.props[1].decode()
        else:
            return "None"

    def print_info(self, indent=0):
        print(" " * indent + (self.fbx_name if self.fbx_name else "(Null)")
              + ("[root]" if self.is_root else "")
              + ("[leaf]" if self.is_leaf else "")
              + ("[ignore]" if self.ignore else "")
              + ("[armature]" if self.is_armature else "")
              + ("[bone]" if self.is_bone else "")
              + ("[HBC]" if self.has_bone_children else "")
              )
        for c in self.children:
            c.print_info(indent + 1)

    def mark_leaf_bones(self):
        if self.is_bone and len(self.children) == 1:
            child = self.children[0]
            if child.is_bone and len(child.children) == 0:
                child.is_leaf = True
        for child in self.children:
            child.mark_leaf_bones()

    def do_bake_transform(self, settings):
        return (settings.bake_space_transform and self.fbx_type in (b'Mesh', b'Null') and
                not self.is_armature and not self.is_bone)

    def find_correction_matrix(self, settings, parent_correction_inv=None):
        from bpy_extras.io_utils import axis_conversion

        if self.parent and (self.parent.is_root or self.parent.do_bake_transform(settings)):
            self.pre_matrix = settings.global_matrix

        if parent_correction_inv:
            self.pre_matrix = parent_correction_inv @ (self.pre_matrix if self.pre_matrix else Matrix())

        correction_matrix = None

        if self.is_bone:
            if settings.automatic_bone_orientation:
                # find best orientation to align bone with
                bone_children = tuple(child for child in self.children if child.is_bone)
                if len(bone_children) == 0:
                    # no children, inherit the correction from parent (if possible)
                    if self.parent and self.parent.is_bone:
                        correction_matrix = parent_correction_inv.inverted() if parent_correction_inv else None
                else:
                    # else find how best to rotate the bone to align the Y axis with the children
                    best_axis = (1, 0, 0)
                    if len(bone_children) == 1:
                        vec = bone_children[0].get_bind_matrix().to_translation()
                        best_axis = Vector((0, 0, 1 if vec[2] >= 0 else -1))
                        if abs(vec[0]) > abs(vec[1]):
                            if abs(vec[0]) > abs(vec[2]):
                                best_axis = Vector((1 if vec[0] >= 0 else -1, 0, 0))
                        elif abs(vec[1]) > abs(vec[2]):
                            best_axis = Vector((0, 1 if vec[1] >= 0 else -1, 0))
                    else:
                        # get the child directions once because they may be checked several times
                        child_locs = (child.get_bind_matrix().to_translation() for child in bone_children)
                        child_locs = tuple(loc.normalized() for loc in child_locs if loc.magnitude > 0.0)

                        # I'm not sure which one I like better...
                        if False:
                            best_angle = -1.0
                            for i in range(6):
                                a = i // 2
                                s = -1 if i % 2 == 1 else 1
                                test_axis = Vector((s if a == 0 else 0, s if a == 1 else 0, s if a == 2 else 0))

                                # find max angle to children
                                max_angle = 1.0
                                for loc in child_locs:
                                    max_angle = min(max_angle, test_axis.dot(loc))

                                # is it better than the last one?
                                if best_angle < max_angle:
                                    best_angle = max_angle
                                    best_axis = test_axis
                        else:
                            best_angle = -1.0
                            for vec in child_locs:
                                test_axis = Vector((0, 0, 1 if vec[2] >= 0 else -1))
                                if abs(vec[0]) > abs(vec[1]):
                                    if abs(vec[0]) > abs(vec[2]):
                                        test_axis = Vector((1 if vec[0] >= 0 else -1, 0, 0))
                                elif abs(vec[1]) > abs(vec[2]):
                                    test_axis = Vector((0, 1 if vec[1] >= 0 else -1, 0))

                                # find max angle to children
                                max_angle = 1.0
                                for loc in child_locs:
                                    max_angle = min(max_angle, test_axis.dot(loc))

                                # is it better than the last one?
                                if best_angle < max_angle:
                                    best_angle = max_angle
                                    best_axis = test_axis

                    # convert best_axis to axis string
                    to_up = 'Z' if best_axis[2] >= 0 else '-Z'
                    if abs(best_axis[0]) > abs(best_axis[1]):
                        if abs(best_axis[0]) > abs(best_axis[2]):
                            to_up = 'X' if best_axis[0] >= 0 else '-X'
                    elif abs(best_axis[1]) > abs(best_axis[2]):
                        to_up = 'Y' if best_axis[1] >= 0 else '-Y'
                    to_forward = 'X' if to_up not in {'X', '-X'} else 'Y'

                    # Build correction matrix
                    if (to_up, to_forward) != ('Y', 'X'):
                        correction_matrix = axis_conversion(from_forward='X',
                                                            from_up='Y',
                                                            to_forward=to_forward,
                                                            to_up=to_up,
                                                            ).to_4x4()
            else:
                correction_matrix = settings.bone_correction_matrix
        else:
            # camera and light can be hard wired
            if self.fbx_type == b'Camera':
                correction_matrix = MAT_CONVERT_CAMERA
            elif self.fbx_type == b'Light':
                correction_matrix = MAT_CONVERT_LIGHT

        self.post_matrix = correction_matrix

        if self.do_bake_transform(settings):
            self.post_matrix = settings.global_matrix_inv @ (self.post_matrix if self.post_matrix else Matrix())

        # process children
        correction_matrix_inv = correction_matrix.inverted_safe() if correction_matrix else None
        for child in self.children:
            child.find_correction_matrix(settings, correction_matrix_inv)

    def find_armature_bones(self, armature):
        for child in self.children:
            if child.is_bone:
                child.armature = armature
                child.find_armature_bones(armature)

    def find_armatures(self):
        needs_armature = False
        for child in self.children:
            if child.is_bone:
                needs_armature = True
                break
        if needs_armature:
            if self.fbx_type in {b'Null', b'Root'}:
                # if empty then convert into armature
                self.is_armature = True
                armature = self
            else:
                # otherwise insert a new node
                # XXX Maybe in case self is virtual FBX root node, we should instead add one armature per bone child?
                armature = FbxImportHelperNode(None, None, None, False)
                armature.fbx_name = "Armature"
                armature.is_armature = True

                for child in tuple(self.children):
                    if child.is_bone:
                        child.parent = armature

                armature.parent = self

            armature.find_armature_bones(armature)

        for child in self.children:
            if child.is_armature or child.is_bone:
                continue
            child.find_armatures()

    def find_bone_children(self):
        has_bone_children = False
        for child in self.children:
            has_bone_children |= child.find_bone_children()
        self.has_bone_children = has_bone_children
        return self.is_bone or has_bone_children

    def find_fake_bones(self, in_armature=False):
        if in_armature and not self.is_bone and self.has_bone_children:
            self.is_bone = True
            # if we are not a null node we need an intermediate node for the data
            if self.fbx_type not in {b'Null', b'Root'}:
                node = FbxImportHelperNode(self.fbx_elem, self.bl_data, None, False)
                self.fbx_elem = None
                self.bl_data = None

                # transfer children
                for child in self.children:
                    if child.is_bone or child.has_bone_children:
                        continue
                    child.parent = node

                # attach to parent
                node.parent = self

        if self.is_armature:
            in_armature = True
        for child in self.children:
            child.find_fake_bones(in_armature)

    def get_world_matrix_as_parent(self):
        matrix = self.parent.get_world_matrix_as_parent() if self.parent else Matrix()
        if self.matrix_as_parent:
            matrix = matrix @ self.matrix_as_parent
        return matrix

    def get_world_matrix(self):
        matrix = self.parent.get_world_matrix_as_parent() if self.parent else Matrix()
        if self.matrix:
            matrix = matrix @ self.matrix
        return matrix

    def get_matrix(self):
        matrix = self.matrix if self.matrix else Matrix()
        if self.pre_matrix:
            matrix = self.pre_matrix @ matrix
        if self.post_matrix:
            matrix = matrix @ self.post_matrix
        return matrix

    def get_bind_matrix(self):
        matrix = self.bind_matrix if self.bind_matrix else Matrix()
        if self.pre_matrix:
            matrix = self.pre_matrix @ matrix
        if self.post_matrix:
            matrix = matrix @ self.post_matrix
        return matrix

    def make_bind_pose_local(self, parent_matrix=None):
        if parent_matrix is None:
            parent_matrix = Matrix()

        if self.bind_matrix:
            bind_matrix = parent_matrix.inverted_safe() @ self.bind_matrix
        else:
            bind_matrix = self.matrix.copy() if self.matrix else None

        self.bind_matrix = bind_matrix
        if bind_matrix:
            parent_matrix = parent_matrix @ bind_matrix

        for child in self.children:
            child.make_bind_pose_local(parent_matrix)

    def collect_skeleton_meshes(self, meshes):
        for _, m in self.clusters:
            meshes.update(m)
        for child in self.children:
            if not child.meshes:
                child.collect_skeleton_meshes(meshes)

    def collect_armature_meshes(self):
        if self.is_armature:
            armature_matrix_inv = self.get_world_matrix().inverted_safe()

            meshes = set()
            for child in self.children:
                # Children meshes may be linked to children armatures, in which case we do not want to link them
                # to a parent one. See T70244.
                child.collect_armature_meshes()
                if not child.meshes:
                    child.collect_skeleton_meshes(meshes)
            for m in meshes:
                old_matrix = m.matrix
                m.matrix = armature_matrix_inv @ m.get_world_matrix()
                m.anim_compensation_matrix = old_matrix.inverted_safe() @ m.matrix
                m.is_global_animation = True
                m.parent = self
            self.meshes = meshes
        else:
            for child in self.children:
                child.collect_armature_meshes()

    def build_skeleton(self, arm, parent_matrix, settings, parent_bone_size=1):
        def child_connect(par_bone, child_bone, child_head, connect_ctx):
            # child_bone or child_head may be None.
            force_connect_children, connected = connect_ctx
            if child_bone is not None:
                child_bone.parent = par_bone
                child_head = child_bone.head

            if similar_values_iter(par_bone.tail, child_head):
                if child_bone is not None:
                    child_bone.use_connect = True
                # Disallow any force-connection at this level from now on, since that child was 'really'
                # connected, we do not want to move current bone's tail anymore!
                connected = None
            elif force_connect_children and connected is not None:
                # We only store position where tail of par_bone should be in the end.
                # Actual tail moving and force connection of compatible child bones will happen
                # once all have been checked.
                if connected is ...:
                    connected = ([child_head.copy(), 1], [child_bone] if child_bone is not None else [])
                else:
                    connected[0][0] += child_head
                    connected[0][1] += 1
                    if child_bone is not None:
                        connected[1].append(child_bone)
            connect_ctx[1] = connected

        def child_connect_finalize(par_bone, connect_ctx):
            force_connect_children, connected = connect_ctx
            # Do nothing if force connection is not enabled!
            if force_connect_children and connected is not None and connected is not ...:
                # Here again we have to be wary about zero-length bones!!!
                par_tail = connected[0][0] / connected[0][1]
                if (par_tail - par_bone.head).magnitude < 1e-2:
                    par_bone_vec = (par_bone.tail - par_bone.head).normalized()
                    par_tail = par_bone.head + par_bone_vec * 0.01
                par_bone.tail = par_tail
                for child_bone in connected[1]:
                    if similar_values_iter(par_tail, child_bone.head):
                        child_bone.use_connect = True

        # Create the (edit)bone.
        bone = arm.bl_data.edit_bones.new(name=self.fbx_name)
        bone.select = True
        self.bl_obj = arm.bl_obj
        self.bl_data = arm.bl_data
        self.bl_bone = bone.name  # Could be different from the FBX name!
        # Read EditBone custom props the NodeAttribute
        if settings.use_custom_props and self.fbx_data_elem:
            blen_read_custom_properties(self.fbx_data_elem, bone, settings)

        # get average distance to children
        bone_size = 0.0
        bone_count = 0
        for child in self.children:
            if child.is_bone:
                bone_size += child.get_bind_matrix().to_translation().magnitude
                bone_count += 1
        if bone_count > 0:
            bone_size /= bone_count
        else:
            bone_size = parent_bone_size

        # So that our bone gets its final length, but still Y-aligned in armature space.
        # 0-length bones are automatically collapsed into their parent when you leave edit mode,
        # so this enforces a minimum length.
        bone_tail = Vector((0.0, 1.0, 0.0)) * max(0.01, bone_size)
        bone.tail = bone_tail

        # And rotate/move it to its final "rest pose".
        bone_matrix = parent_matrix @ self.get_bind_matrix().normalized()

        bone.matrix = bone_matrix

        force_connect_children = settings.force_connect_children

        connect_ctx = [force_connect_children, ...]
        for child in self.children:
            if child.is_leaf and force_connect_children:
                # Arggggggggggggggggg! We do not want to create this bone, but we need its 'virtual head' location
                # to orient current one!!!
                child_head = (bone_matrix @ child.get_bind_matrix().normalized()).translation
                child_connect(bone, None, child_head, connect_ctx)
            elif child.is_bone and not child.ignore:
                child_bone = child.build_skeleton(arm, bone_matrix, settings, bone_size)
                # Connection to parent.
                child_connect(bone, child_bone, None, connect_ctx)

        child_connect_finalize(bone, connect_ctx)

        # Correction for children attached to a bone. FBX expects to attach to the head of a bone, while Blender
        # attaches to the tail.
        if force_connect_children:
            # When forcefully connecting, the bone's tail position may be changed, which can change both the bone's
            # rotation and its length.
            # Set the correction matrix such that it transforms the current tail transformation back to the original
            # head transformation.
            head_to_origin = bone.matrix.inverted_safe()
            tail_to_head = Matrix.Translation(bone.head - bone.tail)
            origin_to_original_head = bone_matrix
            tail_to_original_head = head_to_origin @ tail_to_head @ origin_to_original_head
            self.bone_child_matrix = tail_to_original_head
        else:
            self.bone_child_matrix = Matrix.Translation(-bone_tail)

        return bone

    def build_node_obj(self, fbx_tmpl, settings):
        if self.bl_obj:
            return self.bl_obj

        if self.is_bone or not self.fbx_elem:
            return None

        # create when linking since we need object data
        elem_name_utf8 = self.fbx_name

        # Object data must be created already
        self.bl_obj = obj = bpy.data.objects.new(name=elem_name_utf8, object_data=self.bl_data)

        fbx_props = (elem_find_first(self.fbx_elem, b'Properties70'),
                     elem_find_first(fbx_tmpl, b'Properties70', fbx_elem_nil))

        # ----
        # Misc Attributes

        obj.color[0:3] = elem_props_get_color_rgb(fbx_props, b'Color', (0.8, 0.8, 0.8))
        obj.hide_viewport = not bool(elem_props_get_visibility(fbx_props, b'Visibility', 1.0))

        obj.matrix_basis = self.get_matrix()

        if settings.use_custom_props:
            blen_read_custom_properties(self.fbx_elem, obj, settings)

        return obj

    def build_skeleton_children(self, fbx_tmpl, settings, scene, view_layer):
        if self.is_bone:
            for child in self.children:
                if child.ignore:
                    continue
                child.build_skeleton_children(fbx_tmpl, settings, scene, view_layer)
            return None
        else:
            # child is not a bone
            obj = self.build_node_obj(fbx_tmpl, settings)

            if obj is None:
                return None

            for child in self.children:
                if child.ignore:
                    continue
                child.build_skeleton_children(fbx_tmpl, settings, scene, view_layer)

            # instance in scene
            view_layer.active_layer_collection.collection.objects.link(obj)
            obj.select_set(True)

            return obj

    def link_skeleton_children(self, fbx_tmpl, settings, scene):
        if self.is_bone:
            for child in self.children:
                if child.ignore:
                    continue
                child_obj = child.bl_obj
                if child_obj and child_obj != self.bl_obj:
                    child_obj.parent = self.bl_obj  # get the armature the bone belongs to
                    child_obj.parent_bone = self.bl_bone
                    child_obj.parent_type = 'BONE'
                    child_obj.matrix_parent_inverse = Matrix()

                    # Blender attaches to the end of a bone, while FBX attaches to the start.
                    # bone_child_matrix corrects for that.
                    if child.pre_matrix:
                        child.pre_matrix = self.bone_child_matrix @ child.pre_matrix
                    else:
                        child.pre_matrix = self.bone_child_matrix

                    child_obj.matrix_basis = child.get_matrix()
                child.link_skeleton_children(fbx_tmpl, settings, scene)
            return None
        else:
            obj = self.bl_obj

            for child in self.children:
                if child.ignore:
                    continue
                child_obj = child.link_skeleton_children(fbx_tmpl, settings, scene)
                if child_obj:
                    child_obj.parent = obj

            return obj

    def set_pose_matrix_and_custom_props(self, arm, settings):
        pose_bone = arm.bl_obj.pose.bones[self.bl_bone]
        pose_bone.matrix_basis = self.get_bind_matrix().inverted_safe() @ self.get_matrix()

        # `self.fbx_elem` can be `None` in cases where the imported hierarchy contains a mix of bone and non-bone FBX
        # Nodes parented to one another, e.g. "bone1"->"mesh1"->"bone2". In Blender, an Armature can only consist of
        # bones, so to maintain the imported hierarchy, a placeholder bone with the same name as "mesh1" is inserted
        # into the Armature and then the imported "mesh1" Object is parented to the placeholder bone. The placeholder
        # bone won't have a `self.fbx_elem` because it belongs to the "mesh1" Object instead.
        # See FbxImportHelperNode.find_fake_bones().
        if settings.use_custom_props and self.fbx_elem:
            blen_read_custom_properties(self.fbx_elem, pose_bone, settings)

        for child in self.children:
            if child.ignore:
                continue
            if child.is_bone:
                child.set_pose_matrix_and_custom_props(arm, settings)

    def merge_weights(self, combined_weights, fbx_cluster):
        indices = elem_prop_first(elem_find_first(fbx_cluster, b'Indexes', default=None), default=())
        weights = elem_prop_first(elem_find_first(fbx_cluster, b'Weights', default=None), default=())

        for index, weight in zip(indices, weights):
            w = combined_weights.get(index)
            if w is None:
                combined_weights[index] = [weight]
            else:
                w.append(weight)

    def set_bone_weights(self):
        ignored_children = tuple(child for child in self.children
                                 if child.is_bone and child.ignore and len(child.clusters) > 0)

        if len(ignored_children) > 0:
            # If we have an ignored child bone we need to merge their weights into the current bone weights.
            # This can happen both intentionally and accidentally when skinning a model. Either way, they
            # need to be moved into a parent bone or they cause animation glitches.
            for fbx_cluster, meshes in self.clusters:
                combined_weights = {}
                self.merge_weights(combined_weights, fbx_cluster)

                for child in ignored_children:
                    for child_cluster, child_meshes in child.clusters:
                        if not meshes.isdisjoint(child_meshes):
                            self.merge_weights(combined_weights, child_cluster)

                # combine child weights
                indices = []
                weights = []
                for i, w in combined_weights.items():
                    indices.append(i)
                    if len(w) > 1:
                        # Add ignored child weights to the current bone's weight.
                        # XXX - Weights that sum to more than 1.0 get clamped to 1.0 when set in the vertex group.
                        weights.append(sum(w))
                    else:
                        weights.append(w[0])

                add_vgroup_to_objects(indices, weights, self.bl_bone, [node.bl_obj for node in meshes])

            # clusters that drive meshes not included in a parent don't need to be merged
            all_meshes = set().union(*[meshes for _, meshes in self.clusters])
            for child in ignored_children:
                for child_cluster, child_meshes in child.clusters:
                    if all_meshes.isdisjoint(child_meshes):
                        indices = elem_prop_first(elem_find_first(child_cluster, b'Indexes', default=None), default=())
                        weights = elem_prop_first(elem_find_first(child_cluster, b'Weights', default=None), default=())
                        add_vgroup_to_objects(indices, weights, self.bl_bone, [node.bl_obj for node in child_meshes])
        else:
            # set the vertex weights on meshes
            for fbx_cluster, meshes in self.clusters:
                indices = elem_prop_first(elem_find_first(fbx_cluster, b'Indexes', default=None), default=())
                weights = elem_prop_first(elem_find_first(fbx_cluster, b'Weights', default=None), default=())
                add_vgroup_to_objects(indices, weights, self.bl_bone, [node.bl_obj for node in meshes])

        for child in self.children:
            if child.is_bone and not child.ignore:
                child.set_bone_weights()

    def build_hierarchy(self, fbx_tmpl, settings, scene, view_layer):
        if self.is_armature:
            # create when linking since we need object data
            elem_name_utf8 = self.fbx_name

            self.bl_data = arm_data = bpy.data.armatures.new(name=elem_name_utf8)

            # Object data must be created already
            self.bl_obj = arm = bpy.data.objects.new(name=elem_name_utf8, object_data=arm_data)

            arm.matrix_basis = self.get_matrix()

            if self.fbx_elem:
                fbx_props = (elem_find_first(self.fbx_elem, b'Properties70'),
                             elem_find_first(fbx_tmpl, b'Properties70', fbx_elem_nil))

                if settings.use_custom_props:
                    # Read Armature Object custom props from the Node
                    blen_read_custom_properties(self.fbx_elem, arm, settings)

                    if self.fbx_data_elem:
                        # Read Armature Data custom props from the NodeAttribute
                        blen_read_custom_properties(self.fbx_data_elem, arm_data, settings)

            # instance in scene
            view_layer.active_layer_collection.collection.objects.link(arm)
            arm.select_set(True)

            # Add bones:

            # Switch to Edit mode.
            view_layer.objects.active = arm
            is_hidden = arm.hide_viewport
            arm.hide_viewport = False  # Can't switch to Edit mode hidden objects...
            bpy.ops.object.mode_set(mode='EDIT')

            for child in self.children:
                if child.ignore:
                    continue
                if child.is_bone:
                    child.build_skeleton(self, Matrix(), settings)

            bpy.ops.object.mode_set(mode='OBJECT')

            arm.hide_viewport = is_hidden

            # Set pose matrix and PoseBone custom properties
            for child in self.children:
                if child.ignore:
                    continue
                if child.is_bone:
                    child.set_pose_matrix_and_custom_props(self, settings)

            # Add bone children:
            for child in self.children:
                if child.ignore:
                    continue
                child_obj = child.build_skeleton_children(fbx_tmpl, settings, scene, view_layer)

            return arm
        elif self.fbx_elem and not self.is_bone:
            obj = self.build_node_obj(fbx_tmpl, settings)

            # walk through children
            for child in self.children:
                child.build_hierarchy(fbx_tmpl, settings, scene, view_layer)

            # instance in scene
            view_layer.active_layer_collection.collection.objects.link(obj)
            obj.select_set(True)

            return obj
        else:
            for child in self.children:
                child.build_hierarchy(fbx_tmpl, settings, scene, view_layer)

            return None

    def link_hierarchy(self, fbx_tmpl, settings, scene):
        if self.is_armature:
            arm = self.bl_obj

            # Link bone children:
            for child in self.children:
                if child.ignore:
                    continue
                child_obj = child.link_skeleton_children(fbx_tmpl, settings, scene)
                if child_obj:
                    child_obj.parent = arm

            # Add armature modifiers to the meshes
            if self.meshes:
                for mesh in self.meshes:
                    (mmat, amat) = mesh.armature_setup[self]
                    me_obj = mesh.bl_obj

                    # bring global armature & mesh matrices into *Blender* global space.
                    # Note: Usage of matrix_geom (local 'diff' transform) here is quite brittle.
                    #       Among other things, why in hell isn't it taken into account by bindpose & co???
                    #       Probably because org app (max) handles it completely aside from any parenting stuff,
                    #       which we obviously cannot do in Blender. :/
                    if amat is None:
                        amat = self.bind_matrix
                    amat = settings.global_matrix @ (Matrix() if amat is None else amat)
                    if self.matrix_geom:
                        amat = amat @ self.matrix_geom
                    mmat = settings.global_matrix @ mmat
                    if mesh.matrix_geom:
                        mmat = mmat @ mesh.matrix_geom

                    # Now that we have armature and mesh in there (global) bind 'state' (matrix),
                    # we can compute inverse parenting matrix of the mesh.
                    me_obj.matrix_parent_inverse = amat.inverted_safe() @ mmat @ me_obj.matrix_basis.inverted_safe()

                    mod = mesh.bl_obj.modifiers.new(arm.name, 'ARMATURE')
                    mod.object = arm

            # Add bone weights to the deformers
            for child in self.children:
                if child.ignore:
                    continue
                if child.is_bone:
                    child.set_bone_weights()

            return arm
        elif self.bl_obj:
            obj = self.bl_obj

            # walk through children
            for child in self.children:
                child_obj = child.link_hierarchy(fbx_tmpl, settings, scene)
                if child_obj:
                    child_obj.parent = obj

            return obj
        else:
            for child in self.children:
                child.link_hierarchy(fbx_tmpl, settings, scene)

            return None


def load(operator, context, filepath="",
         use_manual_orientation=False,
         axis_forward='-Z',
         axis_up='Y',
         global_scale=1.0,
         bake_space_transform=False,
         use_custom_normals=True,
         use_image_search=False,
         use_alpha_decals=False,
         decal_offset=0.0,
         use_anim=True,
         anim_offset=1.0,
         use_subsurf=False,
         use_custom_props=True,
         use_custom_props_enum_as_string=True,
         ignore_leaf_bones=False,
         force_connect_children=False,
         automatic_bone_orientation=False,
         primary_bone_axis='Y',
         secondary_bone_axis='X',
         use_prepost_rot=True,
         colors_type='SRGB',
         mtl_name_collision_mode="MAKE_UNIQUE"):

    global fbx_elem_nil
    fbx_elem_nil = FBXElem('', (), (), ())

    import os
    import time
    from bpy_extras.io_utils import axis_conversion

    from . import parse_fbx
    from .fbx_utils import RIGHT_HAND_AXES, FBX_FRAMERATES

    start_time_proc = time.process_time()
    start_time_sys = time.time()

    perfmon = PerfMon()
    perfmon.level_up()
    perfmon.step("FBX Import: start importing %s" % filepath)
    perfmon.level_up()

    # Detect ASCII files.

    # Typically it's bad practice to fail silently on any error,
    # however the file may fail to read for many reasons,
    # and this situation is handled later in the code,
    # right now we only want to know if the file successfully reads as ascii.
    try:
        with open(filepath, 'r', encoding="utf-8") as fh:
            fh.read(24)
        is_ascii = True
    except Exception:
        is_ascii = False

    if is_ascii:
        operator.report({'ERROR'}, rpt_("ASCII FBX files are not supported %r") % filepath)
        return {'CANCELLED'}
    del is_ascii
    # End ascii detection.

    try:
        elem_root, version = parse_fbx.parse(filepath)
    except Exception as e:
        import traceback
        traceback.print_exc()

        operator.report({'ERROR'}, rpt_("Couldn't open file %r (%s)") % (filepath, e))
        return {'CANCELLED'}

    if version < 7100:
        operator.report({'ERROR'}, rpt_("Version %r unsupported, must be %r or later") % (version, 7100))
        return {'CANCELLED'}

    print("FBX version: %r" % version)

    if bpy.ops.object.mode_set.poll():
        bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

    # deselect all
    if bpy.ops.object.select_all.poll():
        bpy.ops.object.select_all(action='DESELECT')

    basedir = os.path.dirname(filepath)

    nodal_material_wrap_map = {}
    image_cache = {}

    # Tables: (FBX_byte_id -> [FBX_data, None or Blender_datablock])
    fbx_table_nodes = {}

    if use_alpha_decals:
        material_decals = set()
    else:
        material_decals = None

    scene = context.scene
    view_layer = context.view_layer

    # #### Get some info from GlobalSettings.

    perfmon.step("FBX import: Prepare...")

    fbx_settings = elem_find_first(elem_root, b'GlobalSettings')
    fbx_settings_props = elem_find_first(fbx_settings, b'Properties70')
    if fbx_settings is None or fbx_settings_props is None:
        operator.report({'ERROR'}, rpt_("No 'GlobalSettings' found in file %r") % filepath)
        return {'CANCELLED'}

    # FBX default base unit seems to be the centimeter, while raw Blender Unit is equivalent to the meter...
    unit_scale = elem_props_get_number(fbx_settings_props, b'UnitScaleFactor', 1.0)
    unit_scale_org = elem_props_get_number(fbx_settings_props, b'OriginalUnitScaleFactor', 1.0)
    global_scale *= (unit_scale / units_blender_to_fbx_factor(context.scene))
    # Compute global matrix and scale.
    if not use_manual_orientation:
        axis_forward = (elem_props_get_integer(fbx_settings_props, b'FrontAxis', 1),
                        elem_props_get_integer(fbx_settings_props, b'FrontAxisSign', 1))
        axis_up = (elem_props_get_integer(fbx_settings_props, b'UpAxis', 2),
                   elem_props_get_integer(fbx_settings_props, b'UpAxisSign', 1))
        axis_coord = (elem_props_get_integer(fbx_settings_props, b'CoordAxis', 0),
                      elem_props_get_integer(fbx_settings_props, b'CoordAxisSign', 1))
        axis_key = (axis_up, axis_forward, axis_coord)
        axis_up, axis_forward = {v: k for k, v in RIGHT_HAND_AXES.items()}.get(axis_key, ('Z', 'Y'))
    global_matrix = (Matrix.Scale(global_scale, 4) @
                     axis_conversion(from_forward=axis_forward, from_up=axis_up).to_4x4())

    # To cancel out unwanted rotation/scale on nodes.
    global_matrix_inv = global_matrix.inverted()
    # For transforming mesh normals.
    global_matrix_inv_transposed = global_matrix_inv.transposed()

    # Compute bone correction matrix
    bone_correction_matrix = None  # None means no correction/identity
    if not automatic_bone_orientation:
        if (primary_bone_axis, secondary_bone_axis) != ('Y', 'X'):
            bone_correction_matrix = axis_conversion(from_forward='X',
                                                     from_up='Y',
                                                     to_forward=secondary_bone_axis,
                                                     to_up=primary_bone_axis,
                                                     ).to_4x4()

    # Compute framerate settings.
    custom_fps = elem_props_get_number(fbx_settings_props, b'CustomFrameRate', 25.0)
    time_mode = elem_props_get_enum(fbx_settings_props, b'TimeMode')
    real_fps = {eid: val for val, eid in FBX_FRAMERATES[1:]}.get(time_mode, custom_fps)
    if real_fps <= 0.0:
        real_fps = 25.0
    scene.render.fps = round(real_fps)
    scene.render.fps_base = scene.render.fps / real_fps

    # store global settings that need to be accessed during conversion
    settings = FBXImportSettings(
        operator.report, (axis_up, axis_forward), global_matrix, global_scale,
        bake_space_transform, global_matrix_inv, global_matrix_inv_transposed,
        use_custom_normals, use_image_search,
        use_alpha_decals, decal_offset,
        use_anim, anim_offset,
        use_subsurf,
        use_custom_props, use_custom_props_enum_as_string,
        nodal_material_wrap_map, image_cache,
        ignore_leaf_bones, force_connect_children, automatic_bone_orientation, bone_correction_matrix,
        use_prepost_rot, colors_type, mtl_name_collision_mode,
    )

    # #### And now, the "real" data.

    perfmon.step("FBX import: Templates...")

    fbx_defs = elem_find_first(elem_root, b'Definitions')  # can be None
    fbx_nodes = elem_find_first(elem_root, b'Objects')
    fbx_connections = elem_find_first(elem_root, b'Connections')

    if fbx_nodes is None:
        operator.report({'ERROR'}, rpt_("No 'Objects' found in file %r") % filepath)
        return {'CANCELLED'}
    if fbx_connections is None:
        operator.report({'ERROR'}, rpt_("No 'Connections' found in file %r") % filepath)
        return {'CANCELLED'}

    # ----
    # First load property templates
    # Load 'PropertyTemplate' values.
    # Key is a tuple, (ObjectType, FBXNodeType)
    # eg, (b'Texture', b'KFbxFileTexture')
    #     (b'Geometry', b'KFbxMesh')
    fbx_templates = {}

    def _():
        if fbx_defs is not None:
            for fbx_def in fbx_defs.elems:
                if fbx_def.id == b'ObjectType':
                    for fbx_subdef in fbx_def.elems:
                        if fbx_subdef.id == b'PropertyTemplate':
                            assert(fbx_def.props_type == b'S')
                            assert(fbx_subdef.props_type == b'S')
                            # (b'Texture', b'KFbxFileTexture') - eg.
                            key = fbx_def.props[0], fbx_subdef.props[0]
                            fbx_templates[key] = fbx_subdef
    _()
    del _

    def fbx_template_get(key):
        ret = fbx_templates.get(key, fbx_elem_nil)
        if ret is fbx_elem_nil:
            # Newest FBX (7.4 and above) use no more 'K' in their type names...
            key = (key[0], key[1][1:])
            return fbx_templates.get(key, fbx_elem_nil)
        return ret

    perfmon.step("FBX import: Nodes...")

    # ----
    # Build FBX node-table
    def _():
        for fbx_obj in fbx_nodes.elems:
            # TODO, investigate what other items after first 3 may be
            assert(fbx_obj.props_type[:3] == b'LSS')
            fbx_uuid = elem_uuid(fbx_obj)
            fbx_table_nodes[fbx_uuid] = [fbx_obj, None]
    _()
    del _

    # ----
    # Load in the data
    # http://download.autodesk.com/us/fbx/20112/FBX_SDK_HELP/index.html?url=
    #        WS73099cc142f487551fea285e1221e4f9ff8-7fda.htm,topicNumber=d0e6388

    perfmon.step("FBX import: Connections...")

    fbx_connection_map = {}
    fbx_connection_map_reverse = {}

    def _():
        for fbx_link in fbx_connections.elems:
            c_type = fbx_link.props[0]
            if fbx_link.props_type[1:3] == b'LL':
                c_src, c_dst = fbx_link.props[1:3]
                fbx_connection_map.setdefault(c_src, []).append((c_dst, fbx_link))
                fbx_connection_map_reverse.setdefault(c_dst, []).append((c_src, fbx_link))
    _()
    del _

    perfmon.step("FBX import: Meshes...")

    # ----
    # Load mesh data
    def _():
        fbx_tmpl = fbx_template_get((b'Geometry', b'KFbxMesh'))

        for fbx_uuid, fbx_item in fbx_table_nodes.items():
            fbx_obj, blen_data = fbx_item
            if fbx_obj.id != b'Geometry':
                continue
            if fbx_obj.props[-1] == b'Mesh':
                assert(blen_data is None)
                fbx_item[1] = blen_read_geom(fbx_tmpl, fbx_obj, settings)
    _()
    del _

    perfmon.step("FBX import: Materials & Textures...")

    # ----
    # Load material data
    def _():
        fbx_tmpl = fbx_template_get((b'Material', b'KFbxSurfacePhong'))
        # b'KFbxSurfaceLambert'

        for fbx_uuid, fbx_item in fbx_table_nodes.items():
            fbx_obj, blen_data = fbx_item
            if fbx_obj.id != b'Material':
                continue
            assert(blen_data is None)
            fbx_item[1] = blen_read_material(fbx_tmpl, fbx_obj, settings)
    _()
    del _

    # ----
    # Load image & textures data
    def _():
        fbx_tmpl_tex = fbx_template_get((b'Texture', b'KFbxFileTexture'))
        fbx_tmpl_img = fbx_template_get((b'Video', b'KFbxVideo'))

        # Important to run all 'Video' ones first, embedded images are stored in those nodes.
        # XXX Note we simplify things here, assuming both matching Video and Texture will use same file path,
        #     this may be a bit weak, if issue arise we'll fallback to plain connection stuff...
        for fbx_uuid, fbx_item in fbx_table_nodes.items():
            fbx_obj, blen_data = fbx_item
            if fbx_obj.id != b'Video':
                continue
            fbx_item[1] = blen_read_texture_image(fbx_tmpl_img, fbx_obj, basedir, settings)
        for fbx_uuid, fbx_item in fbx_table_nodes.items():
            fbx_obj, blen_data = fbx_item
            if fbx_obj.id != b'Texture':
                continue
            fbx_item[1] = blen_read_texture_image(fbx_tmpl_tex, fbx_obj, basedir, settings)
    _()
    del _

    perfmon.step("FBX import: Cameras & Lamps...")

    # ----
    # Load camera data
    def _():
        fbx_tmpl = fbx_template_get((b'NodeAttribute', b'KFbxCamera'))

        for fbx_uuid, fbx_item in fbx_table_nodes.items():
            fbx_obj, blen_data = fbx_item
            if fbx_obj.id != b'NodeAttribute':
                continue
            if fbx_obj.props[-1] == b'Camera':
                assert(blen_data is None)
                fbx_item[1] = blen_read_camera(fbx_tmpl, fbx_obj, settings)
    _()
    del _

    # ----
    # Load lamp data
    def _():
        fbx_tmpl = fbx_template_get((b'NodeAttribute', b'KFbxLight'))

        for fbx_uuid, fbx_item in fbx_table_nodes.items():
            fbx_obj, blen_data = fbx_item
            if fbx_obj.id != b'NodeAttribute':
                continue
            if fbx_obj.props[-1] == b'Light':
                assert(blen_data is None)
                fbx_item[1] = blen_read_light(fbx_tmpl, fbx_obj, settings)
    _()
    del _

    # ----
    # Connections
    def connection_filter_ex(fbx_uuid, fbx_id, dct):
        return [(c_found[0], c_found[1], c_type)
                for (c_uuid, c_type) in dct.get(fbx_uuid, ())
                # 0 is used for the root node, which isn't in fbx_table_nodes
                for c_found in (() if c_uuid == 0 else (fbx_table_nodes.get(c_uuid, (None, None)),))
                if (fbx_id is None) or (c_found[0] and c_found[0].id == fbx_id)]

    def connection_filter_forward(fbx_uuid, fbx_id):
        return connection_filter_ex(fbx_uuid, fbx_id, fbx_connection_map)

    def connection_filter_reverse(fbx_uuid, fbx_id):
        return connection_filter_ex(fbx_uuid, fbx_id, fbx_connection_map_reverse)

    perfmon.step("FBX import: Objects & Armatures...")

    # -- temporary helper hierarchy to build armatures and objects from
    # lookup from uuid to helper node. Used to build parent-child relations and later to look up animated nodes.
    fbx_helper_nodes = {}

    def _():
        # We build an intermediate hierarchy used to:
        # - Calculate and store bone orientation correction matrices. The same matrices will be reused for animation.
        # - Find/insert armature nodes.
        # - Filter leaf bones.

        # create scene root
        fbx_helper_nodes[0] = root_helper = FbxImportHelperNode(None, None, None, False)
        root_helper.is_root = True

        # add fbx nodes
        fbx_tmpl = fbx_template_get((b'Model', b'KFbxNode'))
        for a_uuid, a_item in fbx_table_nodes.items():
            fbx_obj, bl_data = a_item
            if fbx_obj is None or fbx_obj.id != b'Model':
                continue

            fbx_props = (elem_find_first(fbx_obj, b'Properties70'),
                         elem_find_first(fbx_tmpl, b'Properties70', fbx_elem_nil))

            transform_data = blen_read_object_transform_preprocess(fbx_props, fbx_obj, Matrix(), use_prepost_rot)
            # Note: 'Root' "bones" are handled as (armature) objects.
            # Note: See T46912 for first FBX file I ever saw with 'Limb' bones - thought those were totally deprecated.
            is_bone = fbx_obj.props[2] in {b'LimbNode', b'Limb'}
            fbx_helper_nodes[a_uuid] = FbxImportHelperNode(fbx_obj, bl_data, transform_data, is_bone)

        # add parent-child relations and add blender data to the node
        for fbx_link in fbx_connections.elems:
            if fbx_link.props[0] != b'OO':
                continue
            if fbx_link.props_type[1:3] == b'LL':
                c_src, c_dst = fbx_link.props[1:3]
                parent = fbx_helper_nodes.get(c_dst)
                if parent is None:
                    continue

                child = fbx_helper_nodes.get(c_src)
                if child is None:
                    # add blender data (meshes, lights, cameras, etc.) to a helper node
                    fbx_sdata, bl_data = p_item = fbx_table_nodes.get(c_src, (None, None))
                    if fbx_sdata is None:
                        continue
                    if fbx_sdata.id not in {b'Geometry', b'NodeAttribute'}:
                        continue
                    parent.bl_data = bl_data
                    if bl_data is None:
                        # If there's no bl_data, add the fbx_sdata so that it can be read when creating the bl_data/bone
                        parent.fbx_data_elem = fbx_sdata
                else:
                    # set parent
                    child.parent = parent

        # find armatures (either an empty below a bone or a new node inserted at the bone
        root_helper.find_armatures()

        # mark nodes that have bone children
        root_helper.find_bone_children()

        # mark nodes that need a bone to attach child-bones to
        root_helper.find_fake_bones()

        # mark leaf nodes that are only required to mark the end of their parent bone
        if settings.ignore_leaf_bones:
            root_helper.mark_leaf_bones()

        # What a mess! Some bones have several BindPoses, some have none, clusters contain a bind pose as well,
        # and you can have several clusters per bone!
        # Maybe some conversion can be applied to put them all into the same frame of reference?

        # get the bind pose from pose elements
        for a_uuid, a_item in fbx_table_nodes.items():
            fbx_obj, bl_data = a_item
            if fbx_obj is None:
                continue
            if fbx_obj.id != b'Pose':
                continue
            if fbx_obj.props[2] != b'BindPose':
                continue
            for fbx_pose_node in fbx_obj.elems:
                if fbx_pose_node.id != b'PoseNode':
                    continue
                node_elem = elem_find_first(fbx_pose_node, b'Node')
                node = elem_uuid(node_elem)
                matrix_elem = elem_find_first(fbx_pose_node, b'Matrix')
                matrix = array_to_matrix4(matrix_elem.props[0]) if matrix_elem else None
                bone = fbx_helper_nodes.get(node)
                if bone and matrix:
                    # Store the matrix in the helper node.
                    # There may be several bind pose matrices for the same node, but in tests they seem to be identical.
                    bone.bind_matrix = matrix  # global space

        # get clusters and bind pose
        for helper_uuid, helper_node in fbx_helper_nodes.items():
            if not helper_node.is_bone:
                continue
            for cluster_uuid, cluster_link in fbx_connection_map.get(helper_uuid, ()):
                if cluster_link.props[0] != b'OO':
                    continue
                fbx_cluster, _ = fbx_table_nodes.get(cluster_uuid, (None, None))
                if fbx_cluster is None or fbx_cluster.id != b'Deformer' or fbx_cluster.props[2] != b'Cluster':
                    continue

                # Get the bind pose from the cluster:
                tx_mesh_elem = elem_find_first(fbx_cluster, b'Transform', default=None)
                tx_mesh = array_to_matrix4(tx_mesh_elem.props[0]) if tx_mesh_elem else Matrix()

                tx_bone_elem = elem_find_first(fbx_cluster, b'TransformLink', default=None)
                tx_bone = array_to_matrix4(tx_bone_elem.props[0]) if tx_bone_elem else None

                tx_arm_elem = elem_find_first(fbx_cluster, b'TransformAssociateModel', default=None)
                tx_arm = array_to_matrix4(tx_arm_elem.props[0]) if tx_arm_elem else None

                mesh_matrix = tx_mesh
                armature_matrix = tx_arm

                if tx_bone:
                    mesh_matrix = tx_bone @ mesh_matrix
                    helper_node.bind_matrix = tx_bone  # overwrite the bind matrix

                # Get the meshes driven by this cluster: (Shouldn't that be only one?)
                meshes = set()
                for skin_uuid, skin_link in fbx_connection_map.get(cluster_uuid):
                    if skin_link.props[0] != b'OO':
                        continue
                    fbx_skin, _ = fbx_table_nodes.get(skin_uuid, (None, None))
                    if fbx_skin is None or fbx_skin.id != b'Deformer' or fbx_skin.props[2] != b'Skin':
                        continue
                    skin_connection = fbx_connection_map.get(skin_uuid)
                    if skin_connection is None:
                        continue
                    for mesh_uuid, mesh_link in skin_connection:
                        if mesh_link.props[0] != b'OO':
                            continue
                        fbx_mesh, _ = fbx_table_nodes.get(mesh_uuid, (None, None))
                        if fbx_mesh is None or fbx_mesh.id != b'Geometry' or fbx_mesh.props[2] != b'Mesh':
                            continue
                        for object_uuid, object_link in fbx_connection_map.get(mesh_uuid):
                            if object_link.props[0] != b'OO':
                                continue
                            mesh_node = fbx_helper_nodes[object_uuid]
                            if mesh_node:
                                # ----
                                # If we get a valid mesh matrix (in bone space), store armature and
                                # mesh global matrices, we need them to compute mesh's matrix_parent_inverse
                                # when actually binding them via the modifier.
                                # Note we assume all bones were bound with the same mesh/armature (global) matrix,
                                # we do not support otherwise in Blender anyway!
                                mesh_node.armature_setup[helper_node.armature] = (mesh_matrix, armature_matrix)
                                meshes.add(mesh_node)

                helper_node.clusters.append((fbx_cluster, meshes))

        # convert bind poses from global space into local space
        root_helper.make_bind_pose_local()

        # collect armature meshes
        root_helper.collect_armature_meshes()

        # find the correction matrices to align FBX objects with their Blender equivalent
        root_helper.find_correction_matrix(settings)

        # build the Object/Armature/Bone hierarchy
        root_helper.build_hierarchy(fbx_tmpl, settings, scene, view_layer)

        # Link the Object/Armature/Bone hierarchy
        root_helper.link_hierarchy(fbx_tmpl, settings, scene)

        # root_helper.print_info(0)
    _()
    del _

    perfmon.step("FBX import: ShapeKeys...")

    # We can handle shapes.
    blend_shape_channels = {}  # We do not need Shapes themselves, but keyblocks, for anim.

    def _():
        fbx_tmpl = fbx_template_get((b'Geometry', b'KFbxShape'))

        # - FBX             | - Blender equivalent
        # Mesh              | `Mesh`
        # BlendShape        | `Key`
        # BlendShapeChannel | `ShapeKey`, but without its `.data`.
        # Shape             | `ShapeKey.data`, but also includes normals and the values are relative to the base Mesh
        #                   | instead of being absolute. The data is sparse, so each Shape has an "Indexes" array too.
        #                   | FBX 2020 introduced 'Modern Style' Shapes that also support tangents, binormals, vertex
        #                   | colors and UVs, and can be absolute values instead of relative, but 'Modern Style' Shapes
        #                   | are not currently supported.
        #
        # The FBX connections between Shapes and Meshes form multiple many-many relationships:
        # Mesh >-< BlendShape >-< BlendShapeChannel >-< Shape
        # In practice, the relationships are almost never many-many and are more typically 1-many or 1-1:
        #   Mesh --- BlendShape:
        #     usually 1-1 and the FBX SDK might enforce that each BlendShape is connected to at most one Mesh.
        #   BlendShape --< BlendShapeChannel:
        #     usually 1-many.
        #   BlendShapeChannel --- or uncommonly --< Shape:
        #     usually 1-1, but 1-many is a documented feature.

        def connections_gen(c_src_uuid, fbx_id, fbx_type):
            """Helper to reduce duplicate code"""
            # Rarely, an imported FBX file will have duplicate connections. For Shape Key related connections, FBX
            # appears to ignore the duplicates, or overwrite the existing duplicates such that the end result is the
            # same as ignoring them, so keep a set of the seen connections and ignore any duplicates.
            seen_connections = set()
            for c_dst_uuid, ctype in fbx_connection_map.get(c_src_uuid, ()):
                if ctype.props[0] != b'OO':
                    # 'Object-Object' connections only.
                    continue
                fbx_data, bl_data = fbx_table_nodes.get(c_dst_uuid, (None, None))
                if fbx_data is None or fbx_data.id != fbx_id or fbx_data.props[2] != fbx_type:
                    # Either `c_dst_uuid` doesn't exist, or it has a different id or type.
                    continue
                connection_key = (c_src_uuid, c_dst_uuid)
                if connection_key in seen_connections:
                    # The connection is a duplicate, skip it.
                    continue
                seen_connections.add(connection_key)
                yield c_dst_uuid, fbx_data, bl_data

        # XXX - Multiple Shapes can be assigned to a single BlendShapeChannel to create a progressive blend between the
        #       base mesh and the assigned Shapes, with the percentage at which each Shape is fully blended being stored
        #       in the BlendShapeChannel's FullWeights array. This is also known as 'in-between shapes'.
        #       We don't have any support for in-between shapes currently.
        blend_shape_channel_to_shapes = {}
        mesh_to_shapes = {}
        for s_uuid, (fbx_sdata, _bl_sdata) in fbx_table_nodes.items():
            if fbx_sdata is None or fbx_sdata.id != b'Geometry' or fbx_sdata.props[2] != b'Shape':
                continue

            # shape -> blendshapechannel -> blendshape -> mesh.
            for bc_uuid, fbx_bcdata, _bl_bcdata in connections_gen(s_uuid, b'Deformer', b'BlendShapeChannel'):
                # Track the Shapes connected to each BlendShapeChannel.
                shapes_assigned_to_channel = blend_shape_channel_to_shapes.setdefault(bc_uuid, [])
                shapes_assigned_to_channel.append(s_uuid)
                for bs_uuid, _fbx_bsdata, _bl_bsdata in connections_gen(bc_uuid, b'Deformer', b'BlendShape'):
                    for m_uuid, _fbx_mdata, bl_mdata in connections_gen(bs_uuid, b'Geometry', b'Mesh'):
                        # Blenmeshes are assumed already created at that time!
                        assert(isinstance(bl_mdata, bpy.types.Mesh))
                        # Group shapes by mesh so that each mesh only needs to be processed once for all of its shape
                        # keys.
                        if bl_mdata not in mesh_to_shapes:
                            # And we have to find all objects using this mesh!
                            objects = []
                            for o_uuid, o_ctype in fbx_connection_map.get(m_uuid, ()):
                                if o_ctype.props[0] != b'OO':
                                    continue
                                node = fbx_helper_nodes[o_uuid]
                                if node:
                                    objects.append(node)
                            shapes_list = []
                            mesh_to_shapes[bl_mdata] = (objects, shapes_list)
                        else:
                            shapes_list = mesh_to_shapes[bl_mdata][1]
                        # Only the number of shapes assigned to each BlendShapeChannel needs to be passed through to
                        # `blen_read_shapes`, but that number isn't known until all the connections have been
                        # iterated, so pass the `shapes_assigned_to_channel` list instead.
                        shapes_list.append((bc_uuid, fbx_sdata, fbx_bcdata, shapes_assigned_to_channel))
                    # BlendShape deformers are only here to connect BlendShapeChannels to meshes, nothing else to do.

        # Iterate through each mesh and create its shape keys
        for bl_mdata, (objects, shapes) in mesh_to_shapes.items():
            for bc_uuid, keyblocks in blen_read_shapes(fbx_tmpl, shapes, objects, bl_mdata, scene).items():
                # keyblocks is a list of tuples (mesh, keyblock) matching that shape/blendshapechannel, for animation.
                blend_shape_channels.setdefault(bc_uuid, []).extend(keyblocks)
    _()
    del _

    if settings.use_subsurf:
        perfmon.step("FBX import: Subdivision surfaces")

        # Look through connections for subsurf in meshes and add it to the parent object
        def _():
            for fbx_link in fbx_connections.elems:
                if fbx_link.props[0] != b'OO':
                    continue
                if fbx_link.props_type[1:3] == b'LL':
                    c_src, c_dst = fbx_link.props[1:3]
                    parent = fbx_helper_nodes.get(c_dst)
                    if parent is None:
                        continue

                    child = fbx_helper_nodes.get(c_src)
                    if child is None:
                        fbx_sdata, bl_data = fbx_table_nodes.get(c_src, (None, None))
                        if fbx_sdata.id != b'Geometry':
                            continue

                        preview_levels = elem_prop_first(elem_find_first(fbx_sdata, b'PreviewDivisionLevels'))
                        render_levels = elem_prop_first(elem_find_first(fbx_sdata, b'RenderDivisionLevels'))
                        if isinstance(preview_levels, int) and isinstance(render_levels, int):
                            mod = parent.bl_obj.modifiers.new('subsurf', 'SUBSURF')
                            mod.levels = preview_levels
                            mod.render_levels = render_levels
                            boundary_rule = elem_prop_first(elem_find_first(fbx_sdata, b'BoundaryRule'), default=1)
                            if boundary_rule == 1:
                                mod.boundary_smooth = "PRESERVE_CORNERS"
                            else:
                                mod.boundary_smooth = "ALL"

        _()
        del _

    if use_anim:
        perfmon.step("FBX import: Animations...")

        # Animation!
        def _():
            # Find the number of "ktimes" per second for this file.
            # Start with the default for this FBX version.
            fbx_ktime = FBX_KTIME_V8 if version >= 8000 else FBX_KTIME_V7
            # Try to find the value of the nested elem_root->'FBXHeaderExtension'->'OtherFlags'->'TCDefinition' element
            # and look up the "ktimes" per second for its value.
            if header := elem_find_first(elem_root, b'FBXHeaderExtension'):
                # The header version that added TCDefinition support is 1004.
                if elem_prop_first(elem_find_first(header, b'FBXHeaderVersion'), default=0) >= 1004:
                    if other_flags := elem_find_first(header, b'OtherFlags'):
                        if timecode_definition := elem_find_first(other_flags, b'TCDefinition'):
                            timecode_definition_value = elem_prop_first(timecode_definition)
                            # If its value is unknown or missing, default to FBX_KTIME_V8.
                            fbx_ktime = FBX_TIMECODE_DEFINITION_TO_KTIME_PER_SECOND.get(timecode_definition_value,
                                                                                        FBX_KTIME_V8)

            fbx_tmpl_astack = fbx_template_get((b'AnimationStack', b'FbxAnimStack'))
            fbx_tmpl_alayer = fbx_template_get((b'AnimationLayer', b'FbxAnimLayer'))
            stacks = {}

            # AnimationStacks.
            for as_uuid, fbx_asitem in fbx_table_nodes.items():
                fbx_asdata, _blen_data = fbx_asitem
                if fbx_asdata.id != b'AnimationStack' or fbx_asdata.props[2] != b'':
                    continue
                stacks[as_uuid] = (fbx_asitem, {})

            # AnimationLayers
            # (mixing is completely ignored for now, each layer results in an independent set of actions).
            def get_astacks_from_alayer(al_uuid):
                for as_uuid, as_ctype in fbx_connection_map.get(al_uuid, ()):
                    if as_ctype.props[0] != b'OO':
                        continue
                    fbx_asdata, _bl_asdata = fbx_table_nodes.get(as_uuid, (None, None))
                    if (fbx_asdata is None or fbx_asdata.id != b'AnimationStack' or
                            fbx_asdata.props[2] != b'' or as_uuid not in stacks):
                        continue
                    yield as_uuid
            for al_uuid, fbx_alitem in fbx_table_nodes.items():
                fbx_aldata, _blen_data = fbx_alitem
                if fbx_aldata.id != b'AnimationLayer' or fbx_aldata.props[2] != b'':
                    continue
                for as_uuid in get_astacks_from_alayer(al_uuid):
                    _fbx_asitem, alayers = stacks[as_uuid]
                    alayers[al_uuid] = (fbx_alitem, {})

            # AnimationCurveNodes (also the ones linked to actual animated data!).
            curvenodes = {}
            for acn_uuid, fbx_acnitem in fbx_table_nodes.items():
                fbx_acndata, _blen_data = fbx_acnitem
                if fbx_acndata.id != b'AnimationCurveNode' or fbx_acndata.props[2] != b'':
                    continue
                cnode = curvenodes[acn_uuid] = {}
                items = []
                for n_uuid, n_ctype in fbx_connection_map.get(acn_uuid, ()):
                    if n_ctype.props[0] != b'OP':
                        continue
                    lnk_prop = n_ctype.props[3]
                    if lnk_prop in {b'Lcl Translation', b'Lcl Rotation', b'Lcl Scaling'}:
                        # n_uuid can (????) be linked to root '0' node, instead of a mere object node... See T41712.
                        ob = fbx_helper_nodes.get(n_uuid, None)
                        if ob is None or ob.is_root:
                            continue
                        items.append((ob, lnk_prop))
                    elif lnk_prop == b'DeformPercent':  # Shape keys.
                        keyblocks = blend_shape_channels.get(n_uuid, None)
                        if keyblocks is None:
                            continue
                        items += [(kb, lnk_prop) for kb in keyblocks]
                    elif lnk_prop == b'FocalLength':  # Camera lens.
                        from bpy.types import Camera
                        fbx_item = fbx_table_nodes.get(n_uuid, None)
                        if fbx_item is None or not isinstance(fbx_item[1], Camera):
                            continue
                        cam = fbx_item[1]
                        items.append((cam, lnk_prop))
                    elif lnk_prop == b'FocusDistance':  # Camera focus.
                        from bpy.types import Camera
                        fbx_item = fbx_table_nodes.get(n_uuid, None)
                        if fbx_item is None or not isinstance(fbx_item[1], Camera):
                            continue
                        cam = fbx_item[1]
                        items.append((cam, lnk_prop))
                    elif lnk_prop == b'DiffuseColor':
                        from bpy.types import Material
                        fbx_item = fbx_table_nodes.get(n_uuid, None)
                        if fbx_item is None or not isinstance(fbx_item[1], Material):
                            continue
                        mat = fbx_item[1]
                        items.append((mat, lnk_prop))
                        print("WARNING! Importing material's animation is not supported for Nodal materials...")
                for al_uuid, al_ctype in fbx_connection_map.get(acn_uuid, ()):
                    if al_ctype.props[0] != b'OO':
                        continue
                    fbx_aldata, _blen_aldata = fbx_alitem = fbx_table_nodes.get(al_uuid, (None, None))
                    if fbx_aldata is None or fbx_aldata.id != b'AnimationLayer' or fbx_aldata.props[2] != b'':
                        continue
                    for as_uuid in get_astacks_from_alayer(al_uuid):
                        _fbx_alitem, anim_items = stacks[as_uuid][1][al_uuid]
                        assert(_fbx_alitem == fbx_alitem)
                        for item, item_prop in items:
                            # No need to keep curvenode FBX data here, contains nothing useful for us.
                            anim_items.setdefault(item, {})[acn_uuid] = (cnode, item_prop)

            # AnimationCurves (real animation data).
            for ac_uuid, fbx_acitem in fbx_table_nodes.items():
                fbx_acdata, _blen_data = fbx_acitem
                if fbx_acdata.id != b'AnimationCurve' or fbx_acdata.props[2] != b'':
                    continue
                for acn_uuid, acn_ctype in fbx_connection_map.get(ac_uuid, ()):
                    if acn_ctype.props[0] != b'OP':
                        continue
                    fbx_acndata, _bl_acndata = fbx_table_nodes.get(acn_uuid, (None, None))
                    if (fbx_acndata is None or fbx_acndata.id != b'AnimationCurveNode' or
                            fbx_acndata.props[2] != b'' or acn_uuid not in curvenodes):
                        continue
                    # Note this is an infamous simplification of the compound props stuff,
                    # seems to be standard naming but we'll probably have to be smarter to handle more exotic files?
                    channel = {
                        b'd|X': 0, b'd|Y': 1, b'd|Z': 2,
                        b'd|DeformPercent': 0,
                        b'd|FocalLength': 0,
                        b'd|FocusDistance': 0
                    }.get(acn_ctype.props[3], None)
                    if channel is None:
                        continue
                    curvenodes[acn_uuid][ac_uuid] = (fbx_acitem, channel)

            # And now that we have sorted all this, apply animations!
            blen_read_animations(fbx_tmpl_astack, fbx_tmpl_alayer, stacks, scene, settings.anim_offset, global_scale,
                                 fbx_ktime)

        _()
        del _

    perfmon.step("FBX import: Assign materials...")

    def _():
        # link Material's to Geometry (via Model's)
        processed_meshes = set()
        for helper_uuid, helper_node in fbx_helper_nodes.items():
            obj = helper_node.bl_obj
            if not obj or obj.type != 'MESH':
                continue

            # Get the Mesh corresponding to the Geometry used by this Model.
            mesh = obj.data
            processed_meshes.add(mesh)

            # Get the Materials from the Model's connections.
            material_connections = connection_filter_reverse(helper_uuid, b'Material')
            if not material_connections:
                continue

            mesh_mats = mesh.materials
            num_mesh_mats = len(mesh_mats)

            if num_mesh_mats == 0:
                # This is the first (or only) model to use this Geometry. This is the most common case when importing.
                # All the Materials can trivially be appended to the Mesh's Materials.
                mats_to_append = material_connections
                mats_to_compare = ()
            elif num_mesh_mats == len(material_connections):
                # Another Model uses the same Geometry and has already appended its Materials to the Mesh. This is the
                # second most common case when importing.
                # It's also possible that a Model could share the same Geometry and have the same number of Materials,
                # but have different Materials, though this is less common.
                # The Model Materials will need to be compared with the Mesh Materials at the same indices to check if
                # they are different.
                mats_to_append = ()
                mats_to_compare = material_connections
            else:
                # Under the assumption that only used Materials are connected to the Model, the number of Materials of
                # each Model using a specific Geometry should be the same, otherwise the Material Indices of the
                # Geometry will be out-of-bounds of the Materials of at least one of the Models using that Geometry.
                # We wouldn't expect this case to happen, but there's nothing to say it can't.
                # We'll handle a differing number of Materials by appending any extra Materials and comparing the rest.
                mats_to_append = material_connections[num_mesh_mats:]
                mats_to_compare = material_connections[:num_mesh_mats]

            for _fbx_lnk_material, material, _fbx_lnk_material_type in mats_to_append:
                mesh_mats.append(material)

            mats_to_compare_and_slots = zip(mats_to_compare, obj.material_slots)
            for (_fbx_lnk_material, material, _fbx_lnk_material_type), mat_slot in mats_to_compare_and_slots:
                if material != mat_slot.material:
                    # Material Slots default to being linked to the Mesh, so a previously processed Object is also using
                    # this Mesh, but the Mesh uses a different Material for this Material Slot.
                    # To have a different Material for this Material Slot on this Object only, the Material Slot must be
                    # linked to the Object rather than the Mesh.
                    # TODO: add an option to link all materials to objects in Blender instead?
                    mat_slot.link = 'OBJECT'
                    mat_slot.material = material

        # We have to validate mesh polygons' ma_idx, see #41015!
        # Some FBX seem to have an extra 'default' material which is not defined in FBX file.
        for mesh in processed_meshes:
            if mesh.validate_material_indices():
                print("WARNING: mesh '%s' had invalid material indices, those were reset to first material" % mesh.name)
    _()
    del _

    perfmon.step("FBX import: Assign textures...")

    def _():
        material_images = {}

        fbx_tmpl = fbx_template_get((b'Material', b'KFbxSurfacePhong'))
        # b'KFbxSurfaceLambert'

        def texture_mapping_set(fbx_obj, node_texture):
            assert(fbx_obj.id == b'Texture')

            fbx_props = (elem_find_first(fbx_obj, b'Properties70'),
                         elem_find_first(fbx_tmpl, b'Properties70', fbx_elem_nil))
            loc = elem_props_get_vector_3d(fbx_props, b'Translation', (0.0, 0.0, 0.0))
            rot = tuple(-r for r in elem_props_get_vector_3d(fbx_props, b'Rotation', (0.0, 0.0, 0.0)))
            scale = tuple(((1.0 / s) if s != 0.0 else 1.0)
                          for s in elem_props_get_vector_3d(fbx_props, b'Scaling', (1.0, 1.0, 1.0)))
            clamp = (bool(elem_props_get_enum(fbx_props, b'WrapModeU', 0)) or
                     bool(elem_props_get_enum(fbx_props, b'WrapModeV', 0)))

            if (loc == (0.0, 0.0, 0.0) and
                rot == (0.0, 0.0, 0.0) and
                scale == (1.0, 1.0, 1.0) and
                    clamp == False):
                return

            node_texture.translation = loc
            node_texture.rotation = rot
            node_texture.scale = scale
            if clamp:
                node_texture.extension = 'EXTEND'

        for fbx_uuid, fbx_item in fbx_table_nodes.items():
            fbx_obj, blen_data = fbx_item
            if fbx_obj.id != b'Material':
                continue

            material = fbx_table_nodes.get(fbx_uuid, (None, None))[1]
            for (fbx_lnk,
                 image,
                 fbx_lnk_type) in connection_filter_reverse(fbx_uuid, b'Texture'):

                if fbx_lnk_type.props[0] == b'OP':
                    lnk_type = fbx_lnk_type.props[3]

                    ma_wrap = nodal_material_wrap_map[material]

                    if lnk_type in {b'DiffuseColor', b'3dsMax|maps|texmap_diffuse'}:
                        ma_wrap.base_color_texture.image = image
                        texture_mapping_set(fbx_lnk, ma_wrap.base_color_texture)
                    elif lnk_type in {b'SpecularColor', b'SpecularFactor'}:
                        # Intensity actually, not color...
                        ma_wrap.specular_texture.image = image
                        texture_mapping_set(fbx_lnk, ma_wrap.specular_texture)
                    elif lnk_type in {b'ReflectionColor', b'ReflectionFactor', b'3dsMax|maps|texmap_reflection'}:
                        # Intensity actually, not color...
                        ma_wrap.metallic_texture.image = image
                        texture_mapping_set(fbx_lnk, ma_wrap.metallic_texture)
                    elif lnk_type in {b'TransparentColor', b'TransparencyFactor'}:
                        ma_wrap.alpha_texture.image = image
                        texture_mapping_set(fbx_lnk, ma_wrap.alpha_texture)
                        if use_alpha_decals:
                            material_decals.add(material)
                    elif lnk_type == b'ShininessExponent':
                        # That is probably reversed compared to expected results? TODO...
                        ma_wrap.roughness_texture.image = image
                        texture_mapping_set(fbx_lnk, ma_wrap.roughness_texture)
                    # XXX, applications abuse bump!
                    elif lnk_type in {b'NormalMap', b'Bump', b'3dsMax|maps|texmap_bump'}:
                        ma_wrap.normalmap_texture.image = image
                        texture_mapping_set(fbx_lnk, ma_wrap.normalmap_texture)
                        """
                    elif lnk_type == b'Bump':
                        # TODO displacement...
                        """
                    elif lnk_type in {b'EmissiveColor'}:
                        ma_wrap.emission_color_texture.image = image
                        texture_mapping_set(fbx_lnk, ma_wrap.emission_color_texture)
                    elif lnk_type in {b'EmissiveFactor'}:
                        ma_wrap.emission_strength_texture.image = image
                        texture_mapping_set(fbx_lnk, ma_wrap.emission_strength_texture)
                    else:
                        print("WARNING: material link %r ignored" % lnk_type)

                    material_images.setdefault(material, {})[lnk_type] = image

        # Check if the diffuse image has an alpha channel,
        # if so, use the alpha channel.

        # Note: this could be made optional since images may have alpha but be entirely opaque
        for fbx_uuid, fbx_item in fbx_table_nodes.items():
            fbx_obj, blen_data = fbx_item
            if fbx_obj.id != b'Material':
                continue
            material = fbx_table_nodes.get(fbx_uuid, (None, None))[1]
            image = material_images.get(material, {}).get(b'DiffuseColor', None)
            # do we have alpha?
            if image and image.depth == 32:
                if use_alpha_decals:
                    material_decals.add(material)

                ma_wrap = nodal_material_wrap_map[material]
                ma_wrap.alpha_texture.use_alpha = True
                ma_wrap.alpha_texture.copy_from(ma_wrap.base_color_texture)

            # Propagate mapping from diffuse to all other channels which have none defined.
            # XXX Commenting for now, I do not really understand the logic here, why should diffuse mapping
            #     be applied to all others if not defined for them???
            # ~ ma_wrap = nodal_material_wrap_map[material]
            # ~ ma_wrap.mapping_set_from_diffuse()

    _()
    del _

    perfmon.step("FBX import: Cycles z-offset workaround...")

    def _():
        # Annoying workaround for cycles having no z-offset
        if material_decals and use_alpha_decals:
            for fbx_uuid, fbx_item in fbx_table_nodes.items():
                fbx_obj, blen_data = fbx_item
                if fbx_obj.id != b'Geometry':
                    continue
                if fbx_obj.props[-1] == b'Mesh':
                    mesh = fbx_item[1]

                    num_verts = len(mesh.vertices)
                    if decal_offset != 0.0 and num_verts > 0:
                        for material in mesh.materials:
                            if material in material_decals:
                                blen_norm_dtype = np.single
                                vcos = MESH_ATTRIBUTE_POSITION.to_ndarray(mesh.attributes)
                                vnorm = np.empty(num_verts * 3, dtype=blen_norm_dtype)
                                mesh.vertex_normals.foreach_get("vector", vnorm)

                                vcos += vnorm * decal_offset

                                MESH_ATTRIBUTE_POSITION.foreach_set(mesh.attributes, vcos)
                                break

                    for obj in (obj for obj in bpy.data.objects if obj.data == mesh):
                        obj.visible_shadow = False
    _()
    del _

    perfmon.level_down()

    perfmon.level_down("Import finished.")
    return {'FINISHED'}
