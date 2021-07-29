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

# Script copyright (C) Blender Foundation

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
from mathutils import Matrix, Euler, Vector

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
)

# global singleton, assign on execution
fbx_elem_nil = None

# Units convertors...
convert_deg_to_rad_iter = units_convertor_iter("degree", "radian")

MAT_CONVERT_BONE = fbx_utils.MAT_CONVERT_BONE.inverted()
MAT_CONVERT_LAMP = fbx_utils.MAT_CONVERT_LAMP.inverted()
MAT_CONVERT_CAMERA = fbx_utils.MAT_CONVERT_CAMERA.inverted()


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
    return elem_name.decode('utf-8', 'replace')


def elem_name_ensure_classes(elem, clss=...):
    elem_name, elem_class = elem_split_name_class(elem)
    if clss is not ...:
        assert(elem_class in clss)
    return elem_name.decode('utf-8', 'replace')


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
        if subelem.props[0] == elem_prop_id:
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
        assert(elem_prop.props[1] == b'bool')
        assert(elem_prop.props[2] == b'')
        assert(elem_prop.props[3] == b'')

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
                    assert(fbx_prop.props[1] == b'KString')
                    assert(fbx_prop.props_type[4] == data_types.STRING)
                    items = fbx_prop.props[4].decode('utf-8', 'replace')
                    for item in items.split('\r\n'):
                        if item:
                            prop_name, prop_value = item.split('=', 1)
                            blen_obj[prop_name.strip()] = prop_value.strip()
                else:
                    prop_name = fbx_prop.props[0].decode('utf-8', 'replace')
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
                            assert(val >= 0 and val < len(enum_items))
                            blen_obj[prop_name] = enum_items[val]
                        else:
                            blen_obj[prop_name] = val
                    else:
                        print ("WARNING: User property type '%s' is not supported" % prop_type.decode('utf-8', 'replace'))


def blen_read_object_transform_do(transform_data):
    # This is a nightmare. FBX SDK uses Maya way to compute the transformation matrix of a node - utterly simple:
    #
    #     WorldTransform = ParentWorldTransform * T * Roff * Rp * Rpre * R * Rpost * Rp-1 * Soff * Sp * S * Sp-1
    #
    # Where all those terms are 4 x 4 matrices that contain:
    #     WorldTransform: Transformation matrix of the node in global space.
    #     ParentWorldTransform: Transformation matrix of the parent node in global space.
    #     T: Translation
    #     Roff: Rotation offset
    #     Rp: Rotation pivot
    #     Rpre: Pre-rotation
    #     R: Rotation
    #     Rpost: Post-rotation
    #     Rp-1: Inverse of the rotation pivot
    #     Soff: Scaling offset
    #     Sp: Scaling pivot
    #     S: Scaling
    #     Sp-1: Inverse of the scaling pivot
    #
    # But it was still too simple, and FBX notion of compatibility is... quite specific. So we also have to
    # support 3DSMax way:
    #
    #     WorldTransform = ParentWorldTransform * T * R * S * OT * OR * OS
    #
    # Where all those terms are 4 x 4 matrices that contain:
    #     WorldTransform: Transformation matrix of the node in global space
    #     ParentWorldTransform: Transformation matrix of the parent node in global space
    #     T: Translation
    #     R: Rotation
    #     S: Scaling
    #     OT: Geometric transform translation
    #     OR: Geometric transform rotation
    #     OS: Geometric transform translation
    #
    # Notes:
    #     Geometric transformations ***are not inherited***: ParentWorldTransform does not contain the OT, OR, OS
    #     of WorldTransform's parent node.
    #
    # Taken from http://download.autodesk.com/us/fbx/20112/FBX_SDK_HELP/
    #            index.html?url=WS1a9193826455f5ff1f92379812724681e696651.htm,topicNumber=d0e7429

    # translation
    lcl_translation = Matrix.Translation(transform_data.loc)
    geom_loc = Matrix.Translation(transform_data.geom_loc)

    # rotation
    to_rot = lambda rot, rot_ord: Euler(convert_deg_to_rad_iter(rot), rot_ord).to_matrix().to_4x4()
    lcl_rot = to_rot(transform_data.rot, transform_data.rot_ord) * transform_data.rot_alt_mat
    pre_rot = to_rot(transform_data.pre_rot, transform_data.rot_ord)
    pst_rot = to_rot(transform_data.pst_rot, transform_data.rot_ord)
    geom_rot = to_rot(transform_data.geom_rot, transform_data.rot_ord)

    rot_ofs = Matrix.Translation(transform_data.rot_ofs)
    rot_piv = Matrix.Translation(transform_data.rot_piv)
    sca_ofs = Matrix.Translation(transform_data.sca_ofs)
    sca_piv = Matrix.Translation(transform_data.sca_piv)

    # scale
    lcl_scale = Matrix()
    lcl_scale[0][0], lcl_scale[1][1], lcl_scale[2][2] = transform_data.sca
    geom_scale = Matrix();
    geom_scale[0][0], geom_scale[1][1], geom_scale[2][2] = transform_data.geom_sca

    base_mat = (
        lcl_translation *
        rot_ofs *
        rot_piv *
        pre_rot *
        lcl_rot *
        pst_rot *
        rot_piv.inverted_safe() *
        sca_ofs *
        sca_piv *
        lcl_scale *
        sca_piv.inverted_safe()
    )
    geom_mat = geom_loc * geom_rot * geom_scale
    # We return mat without 'geometric transforms' too, because it is to be used for children, sigh...
    return (base_mat * geom_mat, base_mat, geom_mat)


# XXX This might be weak, now that we can add vgroups from both bones and shapes, name collisions become
#     more likely, will have to make this more robust!!!
def add_vgroup_to_objects(vg_indices, vg_weights, vg_name, objects):
    assert(len(vg_indices) == len(vg_weights))
    if vg_indices:
        for obj in objects:
            # We replace/override here...
            vg = obj.vertex_groups.get(vg_name)
            if vg is None:
                vg = obj.vertex_groups.new(vg_name)
            for i, w in zip(vg_indices, vg_weights):
                vg.add((i,), w, 'REPLACE')


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
def blen_read_animations_curves_iter(fbx_curves, blen_start_offset, fbx_start_offset, fps):
    """
    Get raw FBX AnimCurve list, and yield values for all curves at each singular curves' keyframes,
    together with (blender) timing, in frames.
    blen_start_offset is expected in frames, while fbx_start_offset is expected in FBX ktime.
    """
    # As a first step, assume linear interpolation between key frames, we'll (try to!) handle more
    # of FBX curves later.
    from .fbx_utils import FBX_KTIME
    timefac = fps / FBX_KTIME

    curves = tuple([0,
                    elem_prop_first(elem_find_first(c[2], b'KeyTime')),
                    elem_prop_first(elem_find_first(c[2], b'KeyValueFloat')),
                    c]
                   for c in fbx_curves)

    allkeys = sorted({item for sublist in curves for item in sublist[1]})
    for curr_fbxktime in allkeys:
        curr_values = []
        for item in curves:
            idx, times, values, fbx_curve = item

            if times[idx] < curr_fbxktime:
                if idx >= 0:
                    idx += 1
                    if idx >= len(times):
                        # We have reached our last element for this curve, stay on it from now on...
                        idx = -1
                    item[0] = idx

            if times[idx] >= curr_fbxktime:
                if idx == 0:
                    curr_values.append((values[idx], fbx_curve))
                else:
                    # Interpolate between this key and the previous one.
                    ifac = (curr_fbxktime - times[idx - 1]) / (times[idx] - times[idx - 1])
                    curr_values.append(((values[idx] - values[idx - 1]) * ifac + values[idx - 1], fbx_curve))
        curr_blenkframe = (curr_fbxktime - fbx_start_offset) * timefac + blen_start_offset
        yield (curr_blenkframe, curr_values)


def blen_read_animations_action_item(action, item, cnodes, fps, anim_offset):
    """
    'Bake' loc/rot/scale into the action,
    taking any pre_ and post_ matrix into account to transform from fbx into blender space.
    """
    from bpy.types import Object, PoseBone, ShapeKey
    from itertools import chain

    fbx_curves = []
    for curves, fbxprop in cnodes.values():
        for (fbx_acdata, _blen_data), channel in curves.values():
            fbx_curves.append((fbxprop, channel, fbx_acdata))

    # Leave if no curves are attached (if a blender curve is attached to scale but without keys it defaults to 0).
    if len(fbx_curves) == 0:
        return

    blen_curves = []
    props = []

    if isinstance(item, ShapeKey):
        props = [(item.path_from_id("value"), 1, "Key")]
    else:  # Object or PoseBone:
        if item.is_bone:
            bl_obj = item.bl_obj.pose.bones[item.bl_bone]
        else:
            bl_obj = item.bl_obj

        # We want to create actions for objects, but for bones we 'reuse' armatures' actions!
        grpname = item.bl_obj.name

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

    blen_curves = [action.fcurves.new(prop, channel, grpname)
                   for prop, nbr_channels, grpname in props for channel in range(nbr_channels)]

    if isinstance(item, ShapeKey):
        for frame, values in blen_read_animations_curves_iter(fbx_curves, anim_offset, 0, fps):
            value = 0.0
            for v, (fbxprop, channel, _fbx_acdata) in values:
                assert(fbxprop == b'DeformPercent')
                assert(channel == 0)
                value = v / 100.0

            for fc, v in zip(blen_curves, (value,)):
                fc.keyframe_points.insert(frame, v, {'NEEDED', 'FAST'}).interpolation = 'LINEAR'

    else:  # Object or PoseBone:
        if item.is_bone:
            bl_obj = item.bl_obj.pose.bones[item.bl_bone]
        else:
            bl_obj = item.bl_obj

        transform_data = item.fbx_transform_data
        rot_prev = bl_obj.rotation_euler.copy()

        # Pre-compute inverted local rest matrix of the bone, if relevant.
        restmat_inv = item.get_bind_matrix().inverted_safe() if item.is_bone else None

        for frame, values in blen_read_animations_curves_iter(fbx_curves, anim_offset, 0, fps):
            for v, (fbxprop, channel, _fbx_acdata) in values:
                if fbxprop == b'Lcl Translation':
                    transform_data.loc[channel] = v
                elif fbxprop == b'Lcl Rotation':
                    transform_data.rot[channel] = v
                elif fbxprop == b'Lcl Scaling':
                    transform_data.sca[channel] = v
            mat, _, _ = blen_read_object_transform_do(transform_data)

            # compensate for changes in the local matrix during processing
            if item.anim_compensation_matrix:
                mat = mat * item.anim_compensation_matrix

            # apply pre- and post matrix
            # post-matrix will contain any correction for lights, camera and bone orientation
            # pre-matrix will contain any correction for a parent's correction matrix or the global matrix
            if item.pre_matrix:
                mat = item.pre_matrix * mat
            if item.post_matrix:
                mat = mat * item.post_matrix

            # And now, remove that rest pose matrix from current mat (also in parent space).
            if restmat_inv:
                mat = restmat_inv * mat

            # Now we have a virtual matrix of transform from AnimCurves, we can insert keyframes!
            loc, rot, sca = mat.decompose()
            if rot_mode == 'QUATERNION':
                pass  # nothing to do!
            elif rot_mode == 'AXIS_ANGLE':
                vec, ang = rot.to_axis_angle()
                rot = ang, vec.x, vec.y, vec.z
            else:  # Euler
                rot = rot.to_euler(rot_mode, rot_prev)
                rot_prev = rot
            for fc, value in zip(blen_curves, chain(loc, rot, sca)):
                fc.keyframe_points.insert(frame, value, {'NEEDED', 'FAST'}).interpolation = 'LINEAR'

    # Since we inserted our keyframes in 'FAST' mode, we have to update the fcurves now.
    for fc in blen_curves:
        fc.update()


def blen_read_animations(fbx_tmpl_astack, fbx_tmpl_alayer, stacks, scene, anim_offset):
    """
    Recreate an action per stack/layer/object combinations.
    Only the first found action is linked to objects, more complex setups are not handled,
    it's up to user to reproduce them!
    """
    from bpy.types import ShapeKey

    actions = {}
    for as_uuid, ((fbx_asdata, _blen_data), alayers) in stacks.items():
        stack_name = elem_name_ensure_class(fbx_asdata, b'AnimStack')
        for al_uuid, ((fbx_aldata, _blen_data), items) in alayers.items():
            layer_name = elem_name_ensure_class(fbx_aldata, b'AnimLayer')
            for item, cnodes in items.items():
                if isinstance(item, ShapeKey):
                    id_data = item.id_data
                else:
                    id_data = item.bl_obj
                    # XXX Ignore rigged mesh animations - those are a nightmare to handle, see note about it in
                    #     FbxImportHelperNode class definition.
                    if id_data.type == 'MESH' and id_data.parent and id_data.parent.type == 'ARMATURE':
                        continue
                if id_data is None:
                    continue

                # Create new action if needed (should always be needed!
                key = (as_uuid, al_uuid, id_data)
                action = actions.get(key)
                if action is None:
                    action_name = "|".join((id_data.name, stack_name, layer_name))
                    actions[key] = action = bpy.data.actions.new(action_name)
                    action.use_fake_user = True
                # If none yet assigned, assign this action to id_data.
                if not id_data.animation_data:
                    id_data.animation_data_create()
                if not id_data.animation_data.action:
                    id_data.animation_data.action = action
                # And actually populate the action!
                blen_read_animations_action_item(action, item, cnodes, scene.render.fps, anim_offset)


# ----
# Mesh

def blen_read_geom_layerinfo(fbx_layer):
    return (
        elem_find_first_string(fbx_layer, b'Name'),
        elem_find_first_string_as_bytes(fbx_layer, b'MappingInformationType'),
        elem_find_first_string_as_bytes(fbx_layer, b'ReferenceInformationType'),
        )


def blen_read_geom_array_setattr(generator, blen_data, blen_attr, fbx_data, stride, item_size, descr, xform):
    """Generic fbx_layer to blen_data setter, generator is expected to yield tuples (ble_idx, fbx_idx)."""
    max_idx = len(blen_data) - 1
    print_error = True

    def check_skip(blen_idx, fbx_idx):
        nonlocal print_error
        if fbx_idx < 0:  # Negative values mean 'skip'.
            return True
        if blen_idx > max_idx:
            if print_error:
                print("ERROR: too much data in this layer, compared to elements in mesh, skipping!")
                print_error = False
            return True
        return False

    if xform is not None:
        if isinstance(blen_data, list):
            if item_size == 1:
                def _process(blend_data, blen_attr, fbx_data, xform, item_size, blen_idx, fbx_idx):
                    blen_data[blen_idx] = xform(fbx_data[fbx_idx])
            else:
                def _process(blend_data, blen_attr, fbx_data, xform, item_size, blen_idx, fbx_idx):
                    blen_data[blen_idx] = xform(fbx_data[fbx_idx:fbx_idx + item_size])
        else:
            if item_size == 1:
                def _process(blend_data, blen_attr, fbx_data, xform, item_size, blen_idx, fbx_idx):
                    setattr(blen_data[blen_idx], blen_attr, xform(fbx_data[fbx_idx]))
            else:
                def _process(blend_data, blen_attr, fbx_data, xform, item_size, blen_idx, fbx_idx):
                    setattr(blen_data[blen_idx], blen_attr, xform(fbx_data[fbx_idx:fbx_idx + item_size]))
    else:
        if isinstance(blen_data, list):
            if item_size == 1:
                def _process(blend_data, blen_attr, fbx_data, xform, item_size, blen_idx, fbx_idx):
                    blen_data[blen_idx] = fbx_data[fbx_idx]
            else:
                def _process(blend_data, blen_attr, fbx_data, xform, item_size, blen_idx, fbx_idx):
                    blen_data[blen_idx] = fbx_data[fbx_idx:fbx_idx + item_size]
        else:
            if item_size == 1:
                def _process(blend_data, blen_attr, fbx_data, xform, item_size, blen_idx, fbx_idx):
                    setattr(blen_data[blen_idx], blen_attr, fbx_data[fbx_idx])
            else:
                def _process(blend_data, blen_attr, fbx_data, xform, item_size, blen_idx, fbx_idx):
                    setattr(blen_data[blen_idx], blen_attr, fbx_data[fbx_idx:fbx_idx + item_size])

    for blen_idx, fbx_idx in generator:
        if check_skip(blen_idx, fbx_idx):
            continue
        _process(blen_data, blen_attr, fbx_data, xform, item_size, blen_idx, fbx_idx)


# generic generators.
def blen_read_geom_array_gen_allsame(data_len):
    return zip(*(range(data_len), (0,) * data_len))


def blen_read_geom_array_gen_direct(fbx_data, stride):
    fbx_data_len = len(fbx_data)
    return zip(*(range(fbx_data_len // stride), range(0, fbx_data_len, stride)))


def blen_read_geom_array_gen_indextodirect(fbx_layer_index, stride):
    return ((bi, fi * stride) for bi, fi in enumerate(fbx_layer_index))


def blen_read_geom_array_gen_direct_looptovert(mesh, fbx_data, stride):
    fbx_data_len = len(fbx_data) // stride
    loops = mesh.loops
    for p in mesh.polygons:
        for lidx in p.loop_indices:
            vidx = loops[lidx].vertex_index
            if vidx < fbx_data_len:
                yield lidx, vidx * stride


# generic error printers.
def blen_read_geom_array_error_mapping(descr, fbx_layer_mapping, quiet=False):
    if not quiet:
        print("warning layer %r mapping type unsupported: %r" % (descr, fbx_layer_mapping))


def blen_read_geom_array_error_ref(descr, fbx_layer_ref, quiet=False):
    if not quiet:
        print("warning layer %r ref type unsupported: %r" % (descr, fbx_layer_ref))


def blen_read_geom_array_mapped_vert(
        mesh, blen_data, blen_attr,
        fbx_layer_data, fbx_layer_index,
        fbx_layer_mapping, fbx_layer_ref,
        stride, item_size, descr,
        xform=None, quiet=False,
        ):
    if fbx_layer_mapping == b'ByVertice':
        if fbx_layer_ref == b'Direct':
            assert(fbx_layer_index is None)
            blen_read_geom_array_setattr(blen_read_geom_array_gen_direct(fbx_layer_data, stride),
                                         blen_data, blen_attr, fbx_layer_data, stride, item_size, descr, xform)
            return True
        blen_read_geom_array_error_ref(descr, fbx_layer_ref, quiet)
    elif fbx_layer_mapping == b'AllSame':
        if fbx_layer_ref == b'IndexToDirect':
            assert(fbx_layer_index is None)
            blen_read_geom_array_setattr(blen_read_geom_array_gen_allsame(len(blen_data)),
                                         blen_data, blen_attr, fbx_layer_data, stride, item_size, descr, xform)
            return True
        blen_read_geom_array_error_ref(descr, fbx_layer_ref, quiet)
    else:
        blen_read_geom_array_error_mapping(descr, fbx_layer_mapping, quiet)

    return False


def blen_read_geom_array_mapped_edge(
        mesh, blen_data, blen_attr,
        fbx_layer_data, fbx_layer_index,
        fbx_layer_mapping, fbx_layer_ref,
        stride, item_size, descr,
        xform=None, quiet=False,
        ):
    if fbx_layer_mapping == b'ByEdge':
        if fbx_layer_ref == b'Direct':
            blen_read_geom_array_setattr(blen_read_geom_array_gen_direct(fbx_layer_data, stride),
                                         blen_data, blen_attr, fbx_layer_data, stride, item_size, descr, xform)
            return True
        blen_read_geom_array_error_ref(descr, fbx_layer_ref, quiet)
    elif fbx_layer_mapping == b'AllSame':
        if fbx_layer_ref == b'IndexToDirect':
            assert(fbx_layer_index is None)
            blen_read_geom_array_setattr(blen_read_geom_array_gen_allsame(len(blen_data)),
                                         blen_data, blen_attr, fbx_layer_data, stride, item_size, descr, xform)
            return True
        blen_read_geom_array_error_ref(descr, fbx_layer_ref, quiet)
    else:
        blen_read_geom_array_error_mapping(descr, fbx_layer_mapping, quiet)

    return False


def blen_read_geom_array_mapped_polygon(
        mesh, blen_data, blen_attr,
        fbx_layer_data, fbx_layer_index,
        fbx_layer_mapping, fbx_layer_ref,
        stride, item_size, descr,
        xform=None, quiet=False,
        ):
    if fbx_layer_mapping == b'ByPolygon':
        if fbx_layer_ref == b'IndexToDirect':
            # XXX Looks like we often get no fbx_layer_index in this case, shall not happen but happens...
            #     We fallback to 'Direct' mapping in this case.
            #~ assert(fbx_layer_index is not None)
            if fbx_layer_index is None:
                blen_read_geom_array_setattr(blen_read_geom_array_gen_direct(fbx_layer_data, stride),
                                             blen_data, blen_attr, fbx_layer_data, stride, item_size, descr, xform)
            else:
                blen_read_geom_array_setattr(blen_read_geom_array_gen_indextodirect(fbx_layer_index, stride),
                                             blen_data, blen_attr, fbx_layer_data, stride, item_size, descr, xform)
            return True
        elif fbx_layer_ref == b'Direct':
            blen_read_geom_array_setattr(blen_read_geom_array_gen_direct(fbx_layer_data, stride),
                                         blen_data, blen_attr, fbx_layer_data, stride, item_size, descr, xform)
            return True
        blen_read_geom_array_error_ref(descr, fbx_layer_ref, quiet)
    elif fbx_layer_mapping == b'AllSame':
        if fbx_layer_ref == b'IndexToDirect':
            assert(fbx_layer_index is None)
            blen_read_geom_array_setattr(blen_read_geom_array_gen_allsame(len(blen_data)),
                                         blen_data, blen_attr, fbx_layer_data, stride, item_size, descr, xform)
            return True
        blen_read_geom_array_error_ref(descr, fbx_layer_ref, quiet)
    else:
        blen_read_geom_array_error_mapping(descr, fbx_layer_mapping, quiet)

    return False


def blen_read_geom_array_mapped_polyloop(
        mesh, blen_data, blen_attr,
        fbx_layer_data, fbx_layer_index,
        fbx_layer_mapping, fbx_layer_ref,
        stride, item_size, descr,
        xform=None, quiet=False,
        ):
    if fbx_layer_mapping == b'ByPolygonVertex':
        if fbx_layer_ref == b'IndexToDirect':
            # XXX Looks like we often get no fbx_layer_index in this case, shall not happen but happens...
            #     We fallback to 'Direct' mapping in this case.
            #~ assert(fbx_layer_index is not None)
            if fbx_layer_index is None:
                blen_read_geom_array_setattr(blen_read_geom_array_gen_direct(fbx_layer_data, stride),
                                             blen_data, blen_attr, fbx_layer_data, stride, item_size, descr, xform)
            else:
                blen_read_geom_array_setattr(blen_read_geom_array_gen_indextodirect(fbx_layer_index, stride),
                                             blen_data, blen_attr, fbx_layer_data, stride, item_size, descr, xform)
            return True
        elif fbx_layer_ref == b'Direct':
            blen_read_geom_array_setattr(blen_read_geom_array_gen_direct(fbx_layer_data, stride),
                                         blen_data, blen_attr, fbx_layer_data, stride, item_size, descr, xform)
            return True
        blen_read_geom_array_error_ref(descr, fbx_layer_ref, quiet)
    elif fbx_layer_mapping == b'ByVertice':
        if fbx_layer_ref == b'Direct':
            assert(fbx_layer_index is None)
            blen_read_geom_array_setattr(blen_read_geom_array_gen_direct_looptovert(mesh, fbx_layer_data, stride),
                                         blen_data, blen_attr, fbx_layer_data, stride, item_size, descr, xform)
            return True
        blen_read_geom_array_error_ref(descr, fbx_layer_ref, quiet)
    elif fbx_layer_mapping == b'AllSame':
        if fbx_layer_ref == b'IndexToDirect':
            assert(fbx_layer_index is None)
            blen_read_geom_array_setattr(blen_read_geom_array_gen_allsame(len(blen_data)),
                                         blen_data, blen_attr, fbx_layer_data, stride, item_size, descr, xform)
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

    blen_data = mesh.polygons
    blen_read_geom_array_mapped_polygon(
        mesh, blen_data, "material_index",
        fbx_layer_data, None,
        fbx_layer_mapping, fbx_layer_ref,
        1, 1, layer_id,
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

            uv_tex = mesh.uv_textures.new(name=fbx_layer_name)
            uv_lay = mesh.uv_layers[-1]
            blen_data = uv_lay.data

            # some valid files omit this data
            if fbx_layer_data is None:
                print("%r %r missing data" % (layer_id, fbx_layer_name))
                continue

            blen_read_geom_array_mapped_polyloop(
                mesh, blen_data, "uv",
                fbx_layer_data, fbx_layer_index,
                fbx_layer_mapping, fbx_layer_ref,
                2, 2, layer_id,
                )


def blen_read_geom_layer_color(fbx_obj, mesh):
    # almost same as UV's
    for layer_id in (b'LayerElementColor',):
        for fbx_layer in elem_find_iter(fbx_obj, layer_id):
            # all should be valid
            (fbx_layer_name,
             fbx_layer_mapping,
             fbx_layer_ref,
             ) = blen_read_geom_layerinfo(fbx_layer)

            fbx_layer_data = elem_prop_first(elem_find_first(fbx_layer, b'Colors'))
            fbx_layer_index = elem_prop_first(elem_find_first(fbx_layer, b'ColorIndex'))

            color_lay = mesh.vertex_colors.new(name=fbx_layer_name)
            blen_data = color_lay.data

            # some valid files omit this data
            if fbx_layer_data is None:
                print("%r %r missing data" % (layer_id, fbx_layer_name))
                continue

            # ignore alpha layer (read 4 items into 3)
            blen_read_geom_array_mapped_polyloop(
                mesh, blen_data, "color",
                fbx_layer_data, fbx_layer_index,
                fbx_layer_mapping, fbx_layer_ref,
                4, 3, layer_id,
                )


def blen_read_geom_layer_smooth(fbx_obj, mesh):
    fbx_layer = elem_find_first(fbx_obj, b'LayerElementSmoothing')

    if fbx_layer is None:
        return False

    # all should be valid
    (fbx_layer_name,
     fbx_layer_mapping,
     fbx_layer_ref,
     ) = blen_read_geom_layerinfo(fbx_layer)

    layer_id = b'Smoothing'
    fbx_layer_data = elem_prop_first(elem_find_first(fbx_layer, layer_id))

    # udk has 'Direct' mapped, with no Smoothing, not sure why, but ignore these
    if fbx_layer_data is None:
        return False

    if fbx_layer_mapping == b'ByEdge':
        # some models have bad edge data, we cant use this info...
        if not mesh.edges:
            print("warning skipping sharp edges data, no valid edges...")
            return False

        blen_data = mesh.edges
        blen_read_geom_array_mapped_edge(
            mesh, blen_data, "use_edge_sharp",
            fbx_layer_data, None,
            fbx_layer_mapping, fbx_layer_ref,
            1, 1, layer_id,
            xform=lambda s: not s,
            )
        # We only set sharp edges here, not face smoothing itself...
        mesh.use_auto_smooth = True
        mesh.show_edge_sharp = True
        return False
    elif fbx_layer_mapping == b'ByPolygon':
        blen_data = mesh.polygons
        return blen_read_geom_array_mapped_polygon(
            mesh, blen_data, "use_smooth",
            fbx_layer_data, None,
            fbx_layer_mapping, fbx_layer_ref,
            1, 1, layer_id,
            xform=lambda s: (s != 0),  # smoothgroup bitflags, treat as booleans for now
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

    # try loops, then vertices.
    tries = ((mesh.loops, "Loops", False, blen_read_geom_array_mapped_polyloop),
             (mesh.polygons, "Polygons", True, blen_read_geom_array_mapped_polygon),
             (mesh.vertices, "Vertices", True, blen_read_geom_array_mapped_vert))
    for blen_data, blen_data_type, is_fake, func in tries:
        bdata = [None] * len(blen_data) if is_fake else blen_data
        if func(mesh, bdata, "normal",
                fbx_layer_data, fbx_layer_index, fbx_layer_mapping, fbx_layer_ref, 3, 3, layer_id, xform, True):
            if blen_data_type is "Polygons":
                for pidx, p in enumerate(mesh.polygons):
                    for lidx in range(p.loop_start, p.loop_start + p.loop_total):
                        mesh.loops[lidx].normal[:] = bdata[pidx]
            elif blen_data_type is "Vertices":
                # We have to copy vnors to lnors! Far from elegant, but simple.
                for l in mesh.loops:
                    l.normal[:] = bdata[l.vertex_index]
            return True

    blen_read_geom_array_error_mapping("normal", fbx_layer_mapping)
    blen_read_geom_array_error_ref("normal", fbx_layer_ref)
    return False


def blen_read_geom(fbx_tmpl, fbx_obj, settings):
    from itertools import chain
    import array

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

    if geom_mat_co is not None:
        def _vcos_transformed_gen(raw_cos, m=None):
            # Note: we could most likely get much better performances with numpy, but will leave this as TODO for now.
            return chain(*(m * Vector(v) for v in zip(*(iter(raw_cos),) * 3)))
        fbx_verts = array.array(fbx_verts.typecode, _vcos_transformed_gen(fbx_verts, geom_mat_co))

    if fbx_verts is None:
        fbx_verts = ()
    if fbx_polys is None:
        fbx_polys = ()

    mesh = bpy.data.meshes.new(name=elem_name_utf8)
    mesh.vertices.add(len(fbx_verts) // 3)
    mesh.vertices.foreach_set("co", fbx_verts)

    if fbx_polys:
        mesh.loops.add(len(fbx_polys))
        poly_loop_starts = []
        poly_loop_totals = []
        poly_loop_prev = 0
        for i, l in enumerate(mesh.loops):
            index = fbx_polys[i]
            if index < 0:
                poly_loop_starts.append(poly_loop_prev)
                poly_loop_totals.append((i - poly_loop_prev) + 1)
                poly_loop_prev = i + 1
                index ^= -1
            l.vertex_index = index

        mesh.polygons.add(len(poly_loop_starts))
        mesh.polygons.foreach_set("loop_start", poly_loop_starts)
        mesh.polygons.foreach_set("loop_total", poly_loop_totals)

        blen_read_geom_layer_material(fbx_obj, mesh)
        blen_read_geom_layer_uv(fbx_obj, mesh)
        blen_read_geom_layer_color(fbx_obj, mesh)

    if fbx_edges:
        # edges in fact index the polygons (NOT the vertices)
        import array
        tot_edges = len(fbx_edges)
        edges_conv = array.array('i', [0]) * (tot_edges * 2)

        edge_index = 0
        for i in fbx_edges:
            e_a = fbx_polys[i]
            if e_a >= 0:
                e_b = fbx_polys[i + 1]
                if e_b < 0:
                    e_b ^= -1
            else:
                # Last index of polygon, wrap back to the start.

                # ideally we wouldn't have to search back,
                # but it should only be 2-3 iterations.
                j = i - 1
                while j >= 0 and fbx_polys[j] >= 0:
                    j -= 1
                e_a ^= -1
                e_b = fbx_polys[j + 1]

            edges_conv[edge_index] = e_a
            edges_conv[edge_index + 1] = e_b
            edge_index += 2

        mesh.edges.add(tot_edges)
        mesh.edges.foreach_set("vertices", edges_conv)

    # must be after edge, face loading.
    ok_smooth = blen_read_geom_layer_smooth(fbx_obj, mesh)

    ok_normals = False
    if settings.use_custom_normals:
        # Note: we store 'temp' normals in loops, since validate() may alter final mesh,
        #       we can only set custom lnors *after* calling it.
        mesh.create_normals_split()
        if geom_mat_no is None:
            ok_normals = blen_read_geom_layer_normal(fbx_obj, mesh)
        else:
            def nortrans(v):
                return geom_mat_no * Vector(v)
            ok_normals = blen_read_geom_layer_normal(fbx_obj, mesh, nortrans)

    mesh.validate(clean_customdata=False)  # *Very* important to not remove lnors here!

    if ok_normals:
        clnors = array.array('f', [0.0] * (len(mesh.loops) * 3))
        mesh.loops.foreach_get("normal", clnors)

        if not ok_smooth:
            mesh.polygons.foreach_set("use_smooth", [True] * len(mesh.polygons))
            ok_smooth = True

        mesh.normals_split_custom_set(tuple(zip(*(iter(clnors),) * 3)))
        mesh.use_auto_smooth = True
        mesh.show_edge_sharp = True
    else:
        mesh.calc_normals()

    if settings.use_custom_normals:
        mesh.free_normals_split()

    if not ok_smooth:
        mesh.polygons.foreach_set("use_smooth", [True] * len(mesh.polygons))

    if settings.use_custom_props:
        blen_read_custom_properties(fbx_obj, mesh, settings)

    return mesh


def blen_read_shape(fbx_tmpl, fbx_sdata, fbx_bcdata, meshes, scene):
    elem_name_utf8 = elem_name_ensure_class(fbx_sdata, b'Geometry')
    indices = elem_prop_first(elem_find_first(fbx_sdata, b'Indexes'), default=())
    dvcos = tuple(co for co in zip(*[iter(elem_prop_first(elem_find_first(fbx_sdata, b'Vertices'), default=()))] * 3))
    # We completely ignore normals here!
    weight = elem_prop_first(elem_find_first(fbx_bcdata, b'DeformPercent'), default=100.0) / 100.0
    vgweights = tuple(vgw / 100.0 for vgw in elem_prop_first(elem_find_first(fbx_bcdata, b'FullWeights'), default=()))

    # Special case, in case all weights are the same, FullWeight can have only one element - *sigh!*
    nbr_indices = len(indices)
    if len(vgweights) == 1 and nbr_indices > 1:
        vgweights = (vgweights[0],) * nbr_indices

    assert(len(vgweights) == nbr_indices == len(dvcos))
    create_vg = bool(set(vgweights) - {1.0})

    keyblocks = []

    for me, objects in meshes:
        vcos = tuple((idx, me.vertices[idx].co + Vector(dvco)) for idx, dvco in zip(indices, dvcos))
        objects = list({node.bl_obj for node in objects})
        assert(objects)

        if me.shape_keys is None:
            objects[0].shape_key_add(name="Basis", from_mix=False)
        objects[0].shape_key_add(name=elem_name_utf8, from_mix=False)
        me.shape_keys.use_relative = True  # Should already be set as such.

        kb = me.shape_keys.key_blocks[elem_name_utf8]
        for idx, co in vcos:
            kb.data[idx].co[:] = co
        kb.value = weight

        # Add vgroup if necessary.
        if create_vg:
            add_vgroup_to_objects(indices, vgweights, elem_name_utf8, objects)
            kb.vertex_group = elem_name_utf8

        keyblocks.append(kb)

    return keyblocks


# --------
# Material

def blen_read_material(fbx_tmpl, fbx_obj, settings):
    elem_name_utf8 = elem_name_ensure_class(fbx_obj, b'Material')

    cycles_material_wrap_map = settings.cycles_material_wrap_map
    ma = bpy.data.materials.new(name=elem_name_utf8)

    const_color_white = 1.0, 1.0, 1.0

    fbx_props = (elem_find_first(fbx_obj, b'Properties70'),
                 elem_find_first(fbx_tmpl, b'Properties70', fbx_elem_nil))
    #~ assert(fbx_props[0] is not None)  # Some Material may be missing that one, it seems... see T50566.

    ma_diff = elem_props_get_color_rgb(fbx_props, b'DiffuseColor', const_color_white)
    ma_spec = elem_props_get_color_rgb(fbx_props, b'SpecularColor', const_color_white)
    ma_alpha = elem_props_get_number(fbx_props, b'Opacity', 1.0)
    ma_spec_intensity = ma.specular_intensity = elem_props_get_number(fbx_props, b'SpecularFactor', 0.25) * 2.0
    ma_spec_hardness = elem_props_get_number(fbx_props, b'Shininess', 9.6)
    ma_refl_factor = elem_props_get_number(fbx_props, b'ReflectionFactor', 0.0)
    ma_refl_color = elem_props_get_color_rgb(fbx_props, b'ReflectionColor', const_color_white)

    if settings.use_cycles:
        from modules import cycles_shader_compat
        # viewport color
        ma.diffuse_color = ma_diff

        ma_wrap = cycles_shader_compat.CyclesShaderWrapper(ma)
        ma_wrap.diffuse_color_set(ma_diff)
        ma_wrap.specular_color_set([c * ma_spec_intensity for c in ma_spec])
        ma_wrap.hardness_value_set(((ma_spec_hardness + 3.0) / 5.0) - 0.65)
        ma_wrap.alpha_value_set(ma_alpha)
        ma_wrap.reflect_factor_set(ma_refl_factor)
        ma_wrap.reflect_color_set(ma_refl_color)

        cycles_material_wrap_map[ma] = ma_wrap
    else:
        # TODO, number BumpFactor isnt used yet
        ma.diffuse_color = ma_diff
        ma.specular_color = ma_spec
        ma.alpha = ma_alpha
        if ma_alpha < 1.0:
            ma.use_transparency = True
            ma.transparency_method = 'RAYTRACE'
        ma.specular_intensity = ma_spec_intensity
        ma.specular_hardness = ma_spec_hardness * 5.10 + 1.0

        if ma_refl_factor != 0.0:
            ma.raytrace_mirror.use = True
            ma.raytrace_mirror.reflect_factor = ma_refl_factor
            ma.mirror_color = ma_refl_color

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
        filepath = os.path.join(basedir, filepath)
    else:
        filepath = elem_find_first_string(fbx_obj, b'FileName')
    if not filepath:
        filepath = elem_find_first_string(fbx_obj, b'Filename')
    if not filepath:
        print("Error, could not find any file path in ", fbx_obj)
        print("       Falling back to: ", elem_name_utf8)
        filepath = elem_name_utf8
    else :
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


def blen_read_camera(fbx_tmpl, fbx_obj, global_scale):
    # meters to inches
    M2I = 0.0393700787

    elem_name_utf8 = elem_name_ensure_class(fbx_obj, b'NodeAttribute')

    fbx_props = (elem_find_first(fbx_obj, b'Properties70'),
                 elem_find_first(fbx_tmpl, b'Properties70', fbx_elem_nil))
    assert(fbx_props[0] is not None)

    camera = bpy.data.cameras.new(name=elem_name_utf8)

    camera.type = 'ORTHO' if elem_props_get_enum(fbx_props, b'CameraProjectionType', 0) == 1 else 'PERSP'

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

    return camera


def blen_read_light(fbx_tmpl, fbx_obj, global_scale):
    import math
    elem_name_utf8 = elem_name_ensure_class(fbx_obj, b'NodeAttribute')

    fbx_props = (elem_find_first(fbx_obj, b'Properties70'),
                 elem_find_first(fbx_tmpl, b'Properties70', fbx_elem_nil))
    # rare
    if fbx_props[0] is None:
        lamp = bpy.data.lamps.new(name=elem_name_utf8, type='POINT')
        return lamp

    light_type = {
        0: 'POINT',
        1: 'SUN',
        2: 'SPOT'}.get(elem_props_get_enum(fbx_props, b'LightType', 0), 'POINT')

    lamp = bpy.data.lamps.new(name=elem_name_utf8, type=light_type)

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

    # TODO, cycles
    lamp.color = elem_props_get_color_rgb(fbx_props, b'Color', (1.0, 1.0, 1.0))
    lamp.energy = elem_props_get_number(fbx_props, b'Intensity', 100.0) / 100.0
    lamp.distance = elem_props_get_number(fbx_props, b'DecayStart', 25.0) * global_scale
    lamp.shadow_method = ('RAY_SHADOW' if elem_props_get_bool(fbx_props, b'CastShadow', True) else 'NOSHADOW')
    lamp.shadow_color = elem_props_get_color_rgb(fbx_props, b'ShadowColor', (0.0, 0.0, 0.0))

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
        'fbx_elem', 'fbx_name', 'fbx_transform_data', 'fbx_type',
        'is_armature', 'has_bone_children', 'is_bone', 'is_root', 'is_leaf',
        'matrix', 'matrix_as_parent', 'matrix_geom', 'meshes', 'post_matrix', 'pre_matrix')

    def __init__(self, fbx_elem, bl_data, fbx_transform_data, is_bone):
        self.fbx_name = elem_name_ensure_class(fbx_elem, b'Model') if fbx_elem else 'Unknown'
        self.fbx_type = fbx_elem.props[2] if fbx_elem else None
        self.fbx_elem = fbx_elem
        self.bl_obj = None
        self.bl_data = bl_data
        self.bl_bone = None                     # Name of bone if this is a bone (this may be different to fbx_name if there was a name conflict in Blender!)
        self.fbx_transform_data = fbx_transform_data
        self.is_root = False
        self.is_bone = is_bone
        self.is_armature = False
        self.armature = None                    # For bones only, relevant armature node.
        self.has_bone_children = False          # True if the hierarchy below this node contains bones, important to support mixed hierarchies.
        self.is_leaf = False                    # True for leaf-bones added to the end of some bone chains to set the lengths.
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
        self.anim_compensation_matrix = None    # a mesh moved in the hierarchy may have a different local matrix. This compensates animations for this.
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
            self.pre_matrix = parent_correction_inv * (self.pre_matrix if self.pre_matrix else Matrix())

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
                correction_matrix = MAT_CONVERT_LAMP

        self.post_matrix = correction_matrix

        if self.do_bake_transform(settings):
            self.post_matrix = settings.global_matrix_inv * (self.post_matrix if self.post_matrix else Matrix())

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
            matrix = matrix * self.matrix_as_parent
        return matrix

    def get_world_matrix(self):
        matrix = self.parent.get_world_matrix_as_parent() if self.parent else Matrix()
        if self.matrix:
            matrix = matrix * self.matrix
        return matrix

    def get_matrix(self):
        matrix = self.matrix if self.matrix else Matrix()
        if self.pre_matrix:
            matrix = self.pre_matrix * matrix
        if self.post_matrix:
            matrix = matrix * self.post_matrix
        return matrix

    def get_bind_matrix(self):
        matrix = self.bind_matrix if self.bind_matrix else Matrix()
        if self.pre_matrix:
            matrix = self.pre_matrix * matrix
        if self.post_matrix:
            matrix = matrix * self.post_matrix
        return matrix

    def make_bind_pose_local(self, parent_matrix=None):
        if parent_matrix is None:
            parent_matrix = Matrix()

        if self.bind_matrix:
            bind_matrix = parent_matrix.inverted_safe() * self.bind_matrix
        else:
            bind_matrix = self.matrix.copy() if self.matrix else None

        self.bind_matrix = bind_matrix
        if bind_matrix:
            parent_matrix = parent_matrix * bind_matrix

        for child in self.children:
            child.make_bind_pose_local(parent_matrix)

    def collect_skeleton_meshes(self, meshes):
        for _, m in self.clusters:
            meshes.update(m)
        for child in self.children:
            child.collect_skeleton_meshes(meshes)

    def collect_armature_meshes(self):
        if self.is_armature:
            armature_matrix_inv = self.get_world_matrix().inverted_safe()

            meshes = set()
            for child in self.children:
                child.collect_skeleton_meshes(meshes)
            for m in meshes:
                old_matrix = m.matrix
                m.matrix = armature_matrix_inv * m.get_world_matrix()
                m.anim_compensation_matrix = old_matrix.inverted_safe() * m.matrix
                m.is_global_animation = True
                m.parent = self
            self.meshes = meshes
        else:
            for child in self.children:
                child.collect_armature_meshes()

    def build_skeleton(self, arm, parent_matrix, parent_bone_size=1, force_connect_children=False):
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
        bone_matrix = parent_matrix * self.get_bind_matrix().normalized()

        bone.matrix = bone_matrix

        # Correction for children attached to a bone. FBX expects to attach to the head of a bone,
        # while Blender attaches to the tail.
        self.bone_child_matrix = Matrix.Translation(-bone_tail)

        connect_ctx = [force_connect_children, ...]
        for child in self.children:
            if child.is_leaf and force_connect_children:
                # Arggggggggggggggggg! We do not want to create this bone, but we need its 'virtual head' location
                # to orient current one!!!
                child_head = (bone_matrix * child.get_bind_matrix().normalized()).translation
                child_connect(bone, None, child_head, connect_ctx)
            elif child.is_bone and not child.ignore:
                child_bone = child.build_skeleton(arm, bone_matrix, bone_size,
                                                  force_connect_children=force_connect_children)
                # Connection to parent.
                child_connect(bone, child_bone, None, connect_ctx)

        child_connect_finalize(bone, connect_ctx)
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
        assert(fbx_props[0] is not None)

        # ----
        # Misc Attributes

        obj.color[0:3] = elem_props_get_color_rgb(fbx_props, b'Color', (0.8, 0.8, 0.8))
        obj.hide = not bool(elem_props_get_visibility(fbx_props, b'Visibility', 1.0))

        obj.matrix_basis = self.get_matrix()

        if settings.use_custom_props:
            blen_read_custom_properties(self.fbx_elem, obj, settings)

        return obj

    def build_skeleton_children(self, fbx_tmpl, settings, scene):
        if self.is_bone:
            for child in self.children:
                if child.ignore:
                    continue
                child.build_skeleton_children(fbx_tmpl, settings, scene)
            return None
        else:
            # child is not a bone
            obj = self.build_node_obj(fbx_tmpl, settings)

            if obj is None:
                return None

            for child in self.children:
                if child.ignore:
                    continue
                child.build_skeleton_children(fbx_tmpl, settings, scene)

            # instance in scene
            obj_base = scene.objects.link(obj)
            obj_base.select = True

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
                        child.pre_matrix = self.bone_child_matrix * child.pre_matrix
                    else:
                        child.pre_matrix = self.bone_child_matrix

                    child_obj.matrix_basis = child.get_matrix()
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

    def set_pose_matrix(self, arm):
        pose_bone = arm.bl_obj.pose.bones[self.bl_bone]
        pose_bone.matrix_basis = self.get_bind_matrix().inverted_safe() * self.get_matrix()

        for child in self.children:
            if child.ignore:
                continue
            if child.is_bone:
                child.set_pose_matrix(arm)

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
                        weights.append(sum(w) / len(w))
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

    def build_hierarchy(self, fbx_tmpl, settings, scene):
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
                assert(fbx_props[0] is not None)

                if settings.use_custom_props:
                    blen_read_custom_properties(self.fbx_elem, arm, settings)

            # instance in scene
            obj_base = scene.objects.link(arm)
            obj_base.select = True

            # Add bones:

            # Switch to Edit mode.
            scene.objects.active = arm
            is_hidden = arm.hide
            arm.hide = False  # Can't switch to Edit mode hidden objects...
            bpy.ops.object.mode_set(mode='EDIT')

            for child in self.children:
                if child.ignore:
                    continue
                if child.is_bone:
                    child.build_skeleton(self, Matrix(), force_connect_children=settings.force_connect_children)

            bpy.ops.object.mode_set(mode='OBJECT')

            arm.hide = is_hidden

            # Set pose matrix
            for child in self.children:
                if child.ignore:
                    continue
                if child.is_bone:
                    child.set_pose_matrix(self)

            # Add bone children:
            for child in self.children:
                if child.ignore:
                    continue
                child_obj = child.build_skeleton_children(fbx_tmpl, settings, scene)

            return arm
        elif self.fbx_elem and not self.is_bone:
            obj = self.build_node_obj(fbx_tmpl, settings)

            # walk through children
            for child in self.children:
                child.build_hierarchy(fbx_tmpl, settings, scene)

            # instance in scene
            obj_base = scene.objects.link(obj)
            obj_base.select = True

            return obj
        else:
            for child in self.children:
                child.build_hierarchy(fbx_tmpl, settings, scene)

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
                    amat = settings.global_matrix * (Matrix() if amat is None else amat)
                    if self.matrix_geom:
                        amat = amat * self.matrix_geom
                    mmat = settings.global_matrix * mmat
                    if mesh.matrix_geom:
                        mmat = mmat * mesh.matrix_geom

                    # Now that we have armature and mesh in there (global) bind 'state' (matrix),
                    # we can compute inverse parenting matrix of the mesh.
                    me_obj.matrix_parent_inverse = amat.inverted_safe() * mmat * me_obj.matrix_basis.inverted_safe()

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


def is_ascii(filepath, size):
    with open(filepath, 'r', encoding="utf-8") as f:
        try:
            f.read(size)
            return True
        except UnicodeDecodeError:
            pass

    return False


def load(operator, context, filepath="",
         use_manual_orientation=False,
         axis_forward='-Z',
         axis_up='Y',
         global_scale=1.0,
         bake_space_transform=False,
         use_custom_normals=True,
         use_cycles=True,
         use_image_search=False,
         use_alpha_decals=False,
         decal_offset=0.0,
         use_anim=True,
         anim_offset=1.0,
         use_custom_props=True,
         use_custom_props_enum_as_string=True,
         ignore_leaf_bones=False,
         force_connect_children=False,
         automatic_bone_orientation=False,
         primary_bone_axis='Y',
         secondary_bone_axis='X',
         use_prepost_rot=True):

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

    # detect ascii files
    if is_ascii(filepath, 24):
        operator.report({'ERROR'}, "ASCII FBX files are not supported %r" % filepath)
        return {'CANCELLED'}

    try:
        elem_root, version = parse_fbx.parse(filepath)
    except Exception as e:
        import traceback
        traceback.print_exc()

        operator.report({'ERROR'}, "Couldn't open file %r (%s)" % (filepath, e))
        return {'CANCELLED'}

    if version < 7100:
        operator.report({'ERROR'}, "Version %r unsupported, must be %r or later" % (version, 7100))
        return {'CANCELLED'}

    print("FBX version: %r" % version)

    if bpy.ops.object.mode_set.poll():
        bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

    # deselect all
    if bpy.ops.object.select_all.poll():
        bpy.ops.object.select_all(action='DESELECT')

    basedir = os.path.dirname(filepath)

    cycles_material_wrap_map = {}
    image_cache = {}
    if not use_cycles:
        texture_cache = {}

    # Tables: (FBX_byte_id -> [FBX_data, None or Blender_datablock])
    fbx_table_nodes = {}

    if use_alpha_decals:
        material_decals = set()
    else:
        material_decals = None

    scene = context.scene

    # #### Get some info from GlobalSettings.

    perfmon.step("FBX import: Prepare...")

    fbx_settings = elem_find_first(elem_root, b'GlobalSettings')
    fbx_settings_props = elem_find_first(fbx_settings, b'Properties70')
    if fbx_settings is None or fbx_settings_props is None:
        operator.report({'ERROR'}, "No 'GlobalSettings' found in file %r" % filepath)
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
    global_matrix = (Matrix.Scale(global_scale, 4) *
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
        use_custom_normals, use_cycles, use_image_search,
        use_alpha_decals, decal_offset,
        use_anim, anim_offset,
        use_custom_props, use_custom_props_enum_as_string,
        cycles_material_wrap_map, image_cache,
        ignore_leaf_bones, force_connect_children, automatic_bone_orientation, bone_correction_matrix,
        use_prepost_rot,
    )

    # #### And now, the "real" data.

    perfmon.step("FBX import: Templates...")

    fbx_defs = elem_find_first(elem_root, b'Definitions')  # can be None
    fbx_nodes = elem_find_first(elem_root, b'Objects')
    fbx_connections = elem_find_first(elem_root, b'Connections')

    if fbx_nodes is None:
        operator.report({'ERROR'}, "No 'Objects' found in file %r" % filepath)
        return {'CANCELLED'}
    if fbx_connections is None:
        operator.report({'ERROR'}, "No 'Connections' found in file %r" % filepath)
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
    _(); del _

    def fbx_template_get(key):
        ret = fbx_templates.get(key, fbx_elem_nil)
        if ret is None:
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
    _(); del _

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
    _(); del _

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
    _(); del _

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
    _(); del _

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
    _(); del _

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
                fbx_item[1] = blen_read_camera(fbx_tmpl, fbx_obj, global_scale)
    _(); del _

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
                fbx_item[1] = blen_read_light(fbx_tmpl, fbx_obj, global_scale)
    _(); del _

    # ----
    # Connections
    def connection_filter_ex(fbx_uuid, fbx_id, dct):
        return [(c_found[0], c_found[1], c_type)
                for (c_uuid, c_type) in dct.get(fbx_uuid, ())
                # 0 is used for the root node, which isnt in fbx_table_nodes
                for c_found in (() if c_uuid is 0 else (fbx_table_nodes.get(c_uuid, (None, None)),))
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
            assert(fbx_props[0] is not None)

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
                    mesh_matrix = tx_bone * mesh_matrix
                    helper_node.bind_matrix = tx_bone  # overwrite the bind matrix

                # Get the meshes driven by this cluster: (Shouldn't that be only one?)
                meshes = set()
                for skin_uuid, skin_link in fbx_connection_map.get(cluster_uuid):
                    if skin_link.props[0] != b'OO':
                        continue
                    fbx_skin, _ = fbx_table_nodes.get(skin_uuid, (None, None))
                    if fbx_skin is None or fbx_skin.id != b'Deformer' or fbx_skin.props[2] != b'Skin':
                        continue
                    for mesh_uuid, mesh_link in fbx_connection_map.get(skin_uuid):
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
        root_helper.build_hierarchy(fbx_tmpl, settings, scene)

        # Link the Object/Armature/Bone hierarchy
        root_helper.link_hierarchy(fbx_tmpl, settings, scene)

        # root_helper.print_info(0)
    _(); del _

    perfmon.step("FBX import: ShapeKeys...")

    # We can handle shapes.
    blend_shape_channels = {}  # We do not need Shapes themselves, but keyblocks, for anim.

    def _():
        fbx_tmpl = fbx_template_get((b'Geometry', b'KFbxShape'))

        for s_uuid, s_item in fbx_table_nodes.items():
            fbx_sdata, bl_sdata = s_item = fbx_table_nodes.get(s_uuid, (None, None))
            if fbx_sdata is None or fbx_sdata.id != b'Geometry' or fbx_sdata.props[2] != b'Shape':
                continue

            # shape -> blendshapechannel -> blendshape -> mesh.
            for bc_uuid, bc_ctype in fbx_connection_map.get(s_uuid, ()):
                if bc_ctype.props[0] != b'OO':
                    continue
                fbx_bcdata, _bl_bcdata = fbx_table_nodes.get(bc_uuid, (None, None))
                if fbx_bcdata is None or fbx_bcdata.id != b'Deformer' or fbx_bcdata.props[2] != b'BlendShapeChannel':
                    continue
                meshes = []
                objects = []
                for bs_uuid, bs_ctype in fbx_connection_map.get(bc_uuid, ()):
                    if bs_ctype.props[0] != b'OO':
                        continue
                    fbx_bsdata, _bl_bsdata = fbx_table_nodes.get(bs_uuid, (None, None))
                    if fbx_bsdata is None or fbx_bsdata.id != b'Deformer' or fbx_bsdata.props[2] != b'BlendShape':
                        continue
                    for m_uuid, m_ctype in fbx_connection_map.get(bs_uuid, ()):
                        if m_ctype.props[0] != b'OO':
                            continue
                        fbx_mdata, bl_mdata = fbx_table_nodes.get(m_uuid, (None, None))
                        if fbx_mdata is None or fbx_mdata.id != b'Geometry' or fbx_mdata.props[2] != b'Mesh':
                            continue
                        # Blenmeshes are assumed already created at that time!
                        assert(isinstance(bl_mdata, bpy.types.Mesh))
                        # And we have to find all objects using this mesh!
                        objects = []
                        for o_uuid, o_ctype in fbx_connection_map.get(m_uuid, ()):
                            if o_ctype.props[0] != b'OO':
                                continue
                            node = fbx_helper_nodes[o_uuid]
                            if node:
                                objects.append(node)
                        meshes.append((bl_mdata, objects))
                    # BlendShape deformers are only here to connect BlendShapeChannels to meshes, nothing else to do.

                # keyblocks is a list of tuples (mesh, keyblock) matching that shape/blendshapechannel, for animation.
                keyblocks = blen_read_shape(fbx_tmpl, fbx_sdata, fbx_bcdata, meshes, scene)
                blend_shape_channels[bc_uuid] = keyblocks
    _(); del _

    if use_anim:
        perfmon.step("FBX import: Animations...")

        # Animation!
        def _():
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
                        keyblocks = blend_shape_channels.get(n_uuid)
                        if keyblocks is None:
                            continue
                        items += [(kb, lnk_prop) for kb in keyblocks]
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
                    channel = {b'd|X': 0, b'd|Y': 1, b'd|Z': 2, b'd|DeformPercent': 0}.get(acn_ctype.props[3], None)
                    if channel is None:
                        continue
                    curvenodes[acn_uuid][ac_uuid] = (fbx_acitem, channel)

            # And now that we have sorted all this, apply animations!
            blen_read_animations(fbx_tmpl_astack, fbx_tmpl_alayer, stacks, scene, settings.anim_offset)

        _(); del _

    perfmon.step("FBX import: Assign materials...")

    def _():
        # link Material's to Geometry (via Model's)
        for fbx_uuid, fbx_item in fbx_table_nodes.items():
            fbx_obj, blen_data = fbx_item
            if fbx_obj.id != b'Geometry':
                continue

            mesh = fbx_table_nodes.get(fbx_uuid, (None, None))[1]

            # can happen in rare cases
            if mesh is None:
                continue

            # In Blender, we link materials to data, typically (meshes), while in FBX they are linked to objects...
            # So we have to be careful not to re-add endlessly the same material to a mesh!
            # This can easily happen with 'baked' dupliobjects, see T44386.
            # TODO: add an option to link materials to objects in Blender instead?
            done_mats = set()

            for (fbx_lnk, fbx_lnk_item, fbx_lnk_type) in connection_filter_forward(fbx_uuid, b'Model'):
                # link materials
                fbx_lnk_uuid = elem_uuid(fbx_lnk)
                for (fbx_lnk_material, material, fbx_lnk_material_type) in connection_filter_reverse(fbx_lnk_uuid, b'Material'):
                    if material not in done_mats:
                        mesh.materials.append(material)
                        done_mats.add(material)

            # We have to validate mesh polygons' mat_idx, see T41015!
            # Some FBX seem to have an extra 'default' material which is not defined in FBX file.
            if mesh.validate_material_indices():
                print("WARNING: mesh '%s' had invalid material indices, those were reset to first material" % mesh.name)
    _(); del _

    perfmon.step("FBX import: Assign textures...")

    def _():
        material_images = {}

        fbx_tmpl = fbx_template_get((b'Material', b'KFbxSurfacePhong'))
        # b'KFbxSurfaceLambert'

        # textures that use this material
        def texture_bumpfac_get(fbx_obj):
            assert(fbx_obj.id == b'Material')
            fbx_props = (elem_find_first(fbx_obj, b'Properties70'),
                         elem_find_first(fbx_tmpl, b'Properties70', fbx_elem_nil))
            # Do not assert, it can be None actually, sigh...
            #~ assert(fbx_props[0] is not None)
            # (x / 7.142) is only a guess, cycles usable range is (0.0 -> 0.5)
            return elem_props_get_number(fbx_props, b'BumpFactor', 2.5) / 7.142

        def texture_mapping_get(fbx_obj):
            assert(fbx_obj.id == b'Texture')

            fbx_props = (elem_find_first(fbx_obj, b'Properties70'),
                         elem_find_first(fbx_tmpl, b'Properties70', fbx_elem_nil))
            # Do not assert, it can be None actually, sigh...
            #~ assert(fbx_props[0] is not None)
            return (elem_props_get_vector_3d(fbx_props, b'Translation', (0.0, 0.0, 0.0)),
                    elem_props_get_vector_3d(fbx_props, b'Rotation', (0.0, 0.0, 0.0)),
                    elem_props_get_vector_3d(fbx_props, b'Scaling', (1.0, 1.0, 1.0)),
                    (bool(elem_props_get_enum(fbx_props, b'WrapModeU', 0)),
                     bool(elem_props_get_enum(fbx_props, b'WrapModeV', 0))))

        if not use_cycles:
            # Simple function to make a new mtex and set defaults
            def material_mtex_new(material, image, tex_map):
                tex = texture_cache.get(image)
                if tex is None:
                    tex = bpy.data.textures.new(name=image.name, type='IMAGE')
                    tex.image = image
                    texture_cache[image] = tex

                    # copy custom properties from image object to texture
                    for key, value in image.items():
                        tex[key] = value

                    # delete custom properties on the image object
                    for key in image.keys():
                        del image[key]

                mtex = material.texture_slots.add()
                mtex.texture = tex
                mtex.texture_coords = 'UV'
                mtex.use_map_color_diffuse = False

                # No rotation here...
                mtex.offset[:] = tex_map[0]
                mtex.scale[:] = tex_map[2]
                return mtex

        for fbx_uuid, fbx_item in fbx_table_nodes.items():
            fbx_obj, blen_data = fbx_item
            if fbx_obj.id != b'Material':
                continue

            material = fbx_table_nodes.get(fbx_uuid, (None, None))[1]
            for (fbx_lnk,
                 image,
                 fbx_lnk_type) in connection_filter_reverse(fbx_uuid, b'Texture'):

                if use_cycles:
                    if fbx_lnk_type.props[0] == b'OP':
                        lnk_type = fbx_lnk_type.props[3]

                        ma_wrap = cycles_material_wrap_map[material]

                        # tx/rot/scale
                        tex_map = texture_mapping_get(fbx_lnk)
                        if (tex_map[0] == (0.0, 0.0, 0.0) and
                                tex_map[1] == (0.0, 0.0, 0.0) and
                                tex_map[2] == (1.0, 1.0, 1.0) and
                                tex_map[3] == (False, False)):
                            use_mapping = False
                        else:
                            use_mapping = True
                            tex_map_kw = {
                                "translation": tex_map[0],
                                "rotation": [-i for i in tex_map[1]],
                                "scale": [((1.0 / i) if i != 0.0 else 1.0) for i in tex_map[2]],
                                "clamp": tex_map[3],
                                }

                        if lnk_type in {b'DiffuseColor', b'3dsMax|maps|texmap_diffuse'}:
                            ma_wrap.diffuse_image_set(image)
                            if use_mapping:
                                ma_wrap.diffuse_mapping_set(**tex_map_kw)
                        elif lnk_type == b'SpecularColor':
                            ma_wrap.specular_image_set(image)
                            if use_mapping:
                                ma_wrap.specular_mapping_set(**tex_map_kw)
                        elif lnk_type in {b'ReflectionColor', b'3dsMax|maps|texmap_reflection'}:
                            ma_wrap.reflect_image_set(image)
                            if use_mapping:
                                ma_wrap.reflect_mapping_set(**tex_map_kw)
                        elif lnk_type == b'TransparentColor':  # alpha
                            ma_wrap.alpha_image_set(image)
                            if use_mapping:
                                ma_wrap.alpha_mapping_set(**tex_map_kw)
                            if use_alpha_decals:
                                material_decals.add(material)
                        elif lnk_type == b'DiffuseFactor':
                            pass  # TODO
                        elif lnk_type == b'ShininessExponent':
                            ma_wrap.hardness_image_set(image)
                            if use_mapping:
                                ma_wrap.hardness_mapping_set(**tex_map_kw)
                        # XXX, applications abuse bump!
                        elif lnk_type in {b'NormalMap', b'Bump', b'3dsMax|maps|texmap_bump'}:
                            ma_wrap.normal_image_set(image)
                            ma_wrap.normal_factor_set(texture_bumpfac_get(fbx_obj))
                            if use_mapping:
                                ma_wrap.normal_mapping_set(**tex_map_kw)
                            """
                        elif lnk_type == b'Bump':
                            ma_wrap.bump_image_set(image)
                            ma_wrap.bump_factor_set(texture_bumpfac_get(fbx_obj))
                            if use_mapping:
                                ma_wrap.bump_mapping_set(**tex_map_kw)
                            """
                        else:
                            print("WARNING: material link %r ignored" % lnk_type)

                        material_images.setdefault(material, {})[lnk_type] = (image, tex_map)
                else:
                    if fbx_lnk_type.props[0] == b'OP':
                        lnk_type = fbx_lnk_type.props[3]

                        # tx/rot/scale (rot is ignored here!).
                        tex_map = texture_mapping_get(fbx_lnk)

                        mtex = material_mtex_new(material, image, tex_map)

                        if lnk_type in {b'DiffuseColor', b'3dsMax|maps|texmap_diffuse'}:
                            mtex.use_map_color_diffuse = True
                            mtex.blend_type = 'MULTIPLY'
                        elif lnk_type == b'SpecularColor':
                            mtex.use_map_color_spec = True
                            mtex.blend_type = 'MULTIPLY'
                        elif lnk_type in {b'ReflectionColor', b'3dsMax|maps|texmap_reflection'}:
                            mtex.use_map_raymir = True
                        elif lnk_type == b'TransparentColor':  # alpha
                            material.use_transparency = True
                            material.transparency_method = 'RAYTRACE'
                            material.alpha = 0.0
                            mtex.use_map_alpha = True
                            mtex.alpha_factor = 1.0
                            if use_alpha_decals:
                                material_decals.add(material)
                        elif lnk_type == b'DiffuseFactor':
                            mtex.use_map_diffuse = True
                        elif lnk_type == b'ShininessExponent':
                            mtex.use_map_hardness = True
                        # XXX, applications abuse bump!
                        elif lnk_type in {b'NormalMap', b'Bump', b'3dsMax|maps|texmap_bump'}:
                            mtex.texture.use_normal_map = True  # not ideal!
                            mtex.use_map_normal = True
                            mtex.normal_factor = texture_bumpfac_get(fbx_obj)
                            """
                        elif lnk_type == b'Bump':
                            mtex.use_map_normal = True
                            mtex.normal_factor = texture_bumpfac_get(fbx_obj)
                            """
                        else:
                            print("WARNING: material link %r ignored" % lnk_type)

                        material_images.setdefault(material, {})[lnk_type] = (image, tex_map)

        # Check if the diffuse image has an alpha channel,
        # if so, use the alpha channel.

        # Note: this could be made optional since images may have alpha but be entirely opaque
        for fbx_uuid, fbx_item in fbx_table_nodes.items():
            fbx_obj, blen_data = fbx_item
            if fbx_obj.id != b'Material':
                continue
            material = fbx_table_nodes.get(fbx_uuid, (None, None))[1]
            image, tex_map = material_images.get(material, {}).get(b'DiffuseColor', (None, None))
            # do we have alpha?
            if image and image.depth == 32:
                if use_alpha_decals:
                    material_decals.add(material)

                if use_cycles:
                    ma_wrap = cycles_material_wrap_map[material]
                    if ma_wrap.node_bsdf_alpha.mute:
                        ma_wrap.alpha_image_set_from_diffuse()
                else:
                    if not any((True for mtex in material.texture_slots if mtex and mtex.use_map_alpha)):
                        mtex = material_mtex_new(material, image, tex_map)

                        material.use_transparency = True
                        material.transparency_method = 'RAYTRACE'
                        material.alpha = 0.0
                        mtex.use_map_alpha = True
                        mtex.alpha_factor = 1.0

            # propagate mapping from diffuse to all other channels which have none defined.
            if use_cycles:
                ma_wrap = cycles_material_wrap_map[material]
                ma_wrap.mapping_set_from_diffuse()

    _(); del _

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

                    if decal_offset != 0.0:
                        for material in mesh.materials:
                            if material in material_decals:
                                for v in mesh.vertices:
                                    v.co += v.normal * decal_offset
                                break

                    if use_cycles:
                        for obj in (obj for obj in bpy.data.objects if obj.data == mesh):
                            obj.cycles_visibility.shadow = False
                    else:
                        for material in mesh.materials:
                            if material in material_decals:
                                # recieve but dont cast shadows
                                material.use_raytrace = False
    _(); del _

    perfmon.level_down()

    perfmon.level_down("Import finished.")
    return {'FINISHED'}
