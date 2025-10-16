# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
from ....io.com import gltf2_io
from ....io.com.gltf2_io_extensions import Extension
from ....io.exp.user_extensions import export_user_extensions
from ..sampler import detect_manual_uv_wrapping
from ..cache import cached
from . import texture as gltf2_blender_gather_texture
from .search_node_tree import \
    get_texture_node_from_socket, \
    from_socket, \
    FilterByType, \
    previous_node, \
    get_const_from_socket, \
    NodeSocket, \
    get_texture_transform_from_mapping_node

# blender_shader_sockets determine the texture and primary_socket determines
# the textransform and UVMap. Ex: when combining an ORM texture, for
# occlusion the primary_socket would be the occlusion socket, and
# blender_shader_sockets would be the (O,R,M) sockets.


def gather_texture_info(primary_socket, blender_shader_sockets, export_settings, filter_type='ALL'):
    export_settings['current_texture_transform'] = {}  # For KHR_animation_pointer
    return __gather_texture_info_helper(primary_socket, blender_shader_sockets, 'DEFAULT', filter_type, export_settings)


def gather_material_normal_texture_info_class(
        primary_socket,
        blender_shader_sockets,
        export_settings,
        filter_type='ALL'):
    export_settings['current_texture_transform'] = {}  # For KHR_animation_pointer
    export_settings['current_normal_scale'] = {}  # For KHR_animation_pointer
    return __gather_texture_info_helper(primary_socket, blender_shader_sockets, 'NORMAL', filter_type, export_settings)


def gather_material_occlusion_texture_info_class(
        primary_socket,
        blender_shader_sockets,
        export_settings,
        filter_type='ALL'):
    export_settings['current_texture_transform'] = {}  # For KHR_animation_pointer
    export_settings['current_occlusion_strength'] = {}  # For KHR_animation_pointer
    return __gather_texture_info_helper(
        primary_socket,
        blender_shader_sockets,
        'OCCLUSION',
        filter_type,
        export_settings)


@cached
def __gather_texture_info_helper(
        primary_socket: bpy.types.NodeSocket,
        blender_shader_sockets: typing.Tuple[bpy.types.NodeSocket],
        kind: str,
        filter_type: str,
        export_settings):
    if not __filter_texture_info(primary_socket, blender_shader_sockets, filter_type, export_settings):
        return None, {}, {}, None

    tex_transform, uvmap_info = __gather_texture_transform_and_tex_coord(primary_socket, export_settings)

    index, factor, udim_image = __gather_index(blender_shader_sockets, None, export_settings)
    if udim_image is not None:
        udim_info = {'udim': udim_image is not None, 'image': udim_image, 'sockets': blender_shader_sockets}
    else:
        udim_info = {}

    fields = {
        'extensions': __gather_extensions(tex_transform, export_settings),
        'extras': __gather_extras(blender_shader_sockets, export_settings),
        'index': index,
        'tex_coord': None  # This will be set later, as some data are dependant of mesh or object
    }

    if kind == 'DEFAULT':
        texture_info = gltf2_io.TextureInfo(**fields)

    elif kind == 'NORMAL':
        fields['scale'] = __gather_normal_scale(primary_socket, export_settings)
        texture_info = gltf2_io.MaterialNormalTextureInfoClass(**fields)

    elif kind == 'OCCLUSION':
        fields['strength'] = __gather_occlusion_strength(primary_socket, export_settings)
        texture_info = gltf2_io.MaterialOcclusionTextureInfoClass(**fields)

    if texture_info.index is None:
        return None, {} if udim_image is None else uvmap_info, udim_info, None

    export_user_extensions('gather_texture_info_hook', export_settings, texture_info, blender_shader_sockets)

    return texture_info, uvmap_info, udim_info, factor


def gather_udim_texture_info(
        primary_socket: bpy.types.NodeSocket,
        blender_shader_sockets: typing.Tuple[bpy.types.NodeSocket],
        udim_info,
        tex,
        export_settings):

    tex_transform, _ = __gather_texture_transform_and_tex_coord(primary_socket, export_settings)
    export_settings['current_udim_info'] = udim_info
    index, _, _ = __gather_index(blender_shader_sockets,
                                 udim_info['image'].name + str(udim_info['tile']), export_settings)
    export_settings['current_udim_info'] = {}

    fields = {
        'extensions': __gather_extensions(tex_transform, export_settings),
        'extras': __gather_extras(blender_shader_sockets, export_settings),
        'index': index,
        'tex_coord': None  # This will be set later, as some data are dependant of mesh or object
    }

    if tex in ["normalTexture", "clearcoatNormalTexture"]:
        fields['scale'] = __gather_normal_scale(primary_socket, export_settings)
        texture_info = gltf2_io.MaterialNormalTextureInfoClass(**fields)
    elif tex == "occlusionTexture":
        fields['strength'] = __gather_occlusion_strength(primary_socket, export_settings)
        texture_info = gltf2_io.MaterialOcclusionTextureInfoClass(**fields)
    else:
        texture_info = gltf2_io.TextureInfo(**fields)

    export_user_extensions('gather_udim_texture_info_hook', export_settings, texture_info, blender_shader_sockets)

    return texture_info


def __filter_texture_info(primary_socket, blender_shader_sockets, filter_type, export_settings):
    if primary_socket is None:
        return False
    if get_texture_node_from_socket(primary_socket, export_settings) is None:
        return False
    if not blender_shader_sockets:
        return False
    if not all([elem is not None for elem in blender_shader_sockets]):
        return False
    if filter_type == "ALL":
        # Check that all sockets link to texture
        if any([get_texture_node_from_socket(socket, export_settings) is None for socket in blender_shader_sockets]):
            # sockets do not lead to a texture --> discard
            return False
    elif filter_type == "ANY":
        # Check that at least one socket link to texture
        if all([get_texture_node_from_socket(socket, export_settings) is None for socket in blender_shader_sockets]):
            return False
    elif filter_type == "NONE":
        # No check
        pass

    return True


def __gather_extensions(texture_transform, export_settings):
    if texture_transform is None:
        return None
    extension = Extension("KHR_texture_transform", texture_transform)
    return {"KHR_texture_transform": extension}


def __gather_extras(blender_shader_sockets, export_settings):
    return None


# MaterialNormalTextureInfo only
def __gather_normal_scale(primary_socket, export_settings):
    result = from_socket(
        primary_socket,
        FilterByType(bpy.types.ShaderNodeNormalMap))
    if not result:
        return None
    strengthInput = result[0].shader_node.inputs['Strength']
    normal_scale = None
    if not strengthInput.is_linked and strengthInput.default_value != 1:
        normal_scale = strengthInput.default_value

    # Storing path for KHR_animation_pointer
    path_ = {}
    path_['length'] = 1
    path_['path'] = "/materials/XXX/YYY/scale"
    export_settings['current_normal_scale']["node_tree." + strengthInput.path_from_id() + ".default_value"] = path_

    return normal_scale


# MaterialOcclusionTextureInfo only
def __gather_occlusion_strength(primary_socket, export_settings):
    # Look for a MixRGB node that mixes with pure white in front of
    # primary_socket. The mix factor gives the occlusion strength.
    nav = primary_socket.to_node_nav()
    nav.move_back()

    reverse = False
    strength = None

    if nav.moved and nav.node.type == 'MIX' and nav.node.blend_type == 'MIX':
        fac, path = nav.get_constant('Factor')
        if fac is not None:
            col1, _ = nav.get_constant('#A_Color')
            col2, _ = nav.get_constant('#B_Color')
            if col1 == [1.0, 1.0, 1.0] and col2 is None:
                strength = fac
            if col1 is None and col2 == [1.0, 1.0, 1.0]:
                strength = 1.0 - fac  # reversed for reversed inputs
                reverse = True

        # Storing path for KHR_animation_pointer
        path_ = {}
        path_['length'] = 1
        path_['path'] = "/materials/XXX/occlusionTexture/strength"
        path_['reverse'] = reverse
        export_settings['current_occlusion_strength'][path] = path_

    return strength


def __gather_index(blender_shader_sockets, use_tile, export_settings):
    # We just put the actual shader into the 'index' member
    return gltf2_blender_gather_texture.gather_texture(blender_shader_sockets, use_tile, export_settings)


def __gather_texture_transform_and_tex_coord(primary_socket, export_settings):
    # We're expecting
    #
    #     [UV Map] => [Mapping] => [UV Wrapping] => [Texture Node] => ... => primary_socket
    #
    # The [UV Wrapping] is for wrap modes like MIRROR that use nodes,
    # [Mapping] is for KHR_texture_transform, and [UV Map] is for texCoord.
    result_tex = get_texture_node_from_socket(primary_socket, export_settings)
    blender_shader_node = result_tex.shader_node

    nodes_used = export_settings.get('nodes_used', {})
    nodes_used[blender_shader_node.name] = True

    # Skip over UV wrapping stuff (it goes in the sampler)
    result = detect_manual_uv_wrapping(blender_shader_node, result_tex.group_path)
    if result:
        node = previous_node(result['next_socket'])
    else:
        node = previous_node(NodeSocket(blender_shader_node.inputs['Vector'], result_tex.group_path))

    texture_transform = None
    if node.node and node.node.type == 'MAPPING':
        texture_transform = get_texture_transform_from_mapping_node(node, export_settings)
        node = previous_node(NodeSocket(node.node.inputs['Vector'], node.group_path))

    uvmap_info = {}

    if node.node and node.node.type == 'UVMAP' and node.node.uv_map:
        uvmap_info['type'] = "Fixed"
        uvmap_info['value'] = node.node.uv_map

    elif node and node.node and node.node.type == 'ATTRIBUTE' \
            and node.node.attribute_type == "GEOMETRY" \
            and node.node.attribute_name:
        # If this attribute is Face Corner / 2D Vector, this is a UV map
        # So we can use it as a UV map Fixed
        # But this will be checked later, when we know the mesh
        uvmap_info['type'] = 'Attribute'
        uvmap_info['value'] = node.node.attribute_name

    else:
        uvmap_info['type'] = 'Active'

    return texture_transform, uvmap_info


def check_same_size_images(
    blender_shader_sockets: typing.Tuple[bpy.types.NodeSocket],
    export_settings
) -> bool:
    """Check that all sockets leads to images of the same size."""
    if not blender_shader_sockets or not all(blender_shader_sockets):
        return False

    sizes = set()
    for socket in blender_shader_sockets:
        tex = get_texture_node_from_socket(socket, export_settings)
        if tex is None:
            return False
        size = tex.shader_node.image.size
        sizes.add((size[0], size[1]))

    return len(sizes) == 1
