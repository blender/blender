# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import typing
import bpy

from ....io.exp.user_extensions import export_user_extensions
from ....io.com.gltf2_io_extensions import Extension
from ....io.exp.image_data import ImageData
from ....io.exp.binary_data import BinaryData
from ....io.com import gltf2_io
from ..sampler import gather_sampler
from ..cache import cached
from .search_node_tree import get_texture_node_from_socket
from . import image


@cached
def gather_texture(
        blender_shader_sockets: typing.Tuple[bpy.types.NodeSocket],
        use_tile: bool,
        is_data: bool,
        export_settings):
    """
    Gather texture sampling information and image channels from a blender shader texture attached to a shader socket.

    :param blender_shader_sockets: The sockets of the material which should contribute to the texture
    :param export_settings: configuration of the export
    :return: a glTF 2.0 texture with sampler and source embedded (will be converted to references by the exporter)
    """

    if not __filter_texture(blender_shader_sockets, export_settings):
        return None, None, None

    source, webp_image, ktx2_image, image_data, factor, udim_image = __gather_source(
        blender_shader_sockets, use_tile, is_data, export_settings)

    exts, remove_source = __gather_extensions(blender_shader_sockets, source,
                                              webp_image, ktx2_image, image_data, export_settings)

    texture = gltf2_io.Texture(
        extensions=exts,
        extras=__gather_extras(blender_shader_sockets, export_settings),
        name=__gather_name(blender_shader_sockets, export_settings),
        sampler=__gather_sampler(blender_shader_sockets, export_settings),
        source=source if remove_source is False else None
    )

    # although valid, most viewers can't handle missing source properties
    # This can have None source for "keep original", when original can't be found
    if texture.source is None and remove_source is False:
        return None, None, udim_image

    export_user_extensions('gather_texture_hook', export_settings, texture, blender_shader_sockets)

    return texture, factor, udim_image


def __filter_texture(blender_shader_sockets, export_settings):
    # User doesn't want to export textures
    if export_settings['gltf_image_format'] == "NONE":
        return None
    return True


def __gather_extensions(blender_shader_sockets, source, webp_image, ktx2_image, image_data, export_settings):

    extensions = {}

    remove_source = False
    required_webp = False
    required_ktx2 = False

    ext_webp = {}
    ext_ktx2 = {}

    # If user want to keep original textures, and these textures are WebP, we need to remove source from
    # gltf2_io.Texture, and populate extension
    if export_settings['gltf_keep_original_textures'] is True \
            and source is not None \
            and source.mime_type == "image/webp":
        ext_webp["source"] = source
        remove_source = True
        required_webp = True

    # If user want to keep original textures, and these textures are KTX2, we need to remove source from
    # gltf2_io.Texture, and populate extension
    if export_settings['gltf_keep_original_textures'] is True \
            and source is not None \
            and source.mime_type == "image/ktx2":
        ext_ktx2["source"] = source
        remove_source = True
        required_ktx2 = True

# If user want to export in WebP format (so without fallback in png/jpg)
    if export_settings['gltf_image_format'] == "WEBP":
        # We create all image without fallback
        ext_webp["source"] = source
        remove_source = True
        required_webp = True

# If user want to export in KTX2 format (so without fallback in png/jpg)
    if export_settings['gltf_image_format'] == "KTX2":
        # We create all image without fallback
        ext_ktx2["source"] = source
        remove_source = True
        required_ktx2 = True

# If user doesn't want to export in WebP format, but want WebP too. Texture is not WebP
    if export_settings['gltf_image_format'] != "WEBP" \
            and export_settings['gltf_add_compressed_images'] is True \
            and export_settings['gltf_compressed_images_type'] == "WEBP" \
            and source is not None \
            and source.mime_type != "image/webp":
        # We need here to create some WebP textures

        new_mime_type = "image/webp"
        new_data, _ = image_data.encode(new_mime_type, export_settings)
        if len(new_data) == 0:
            export_settings['log'].warning("Image data is empty, not exporting image")
            return None, False

        if export_settings['gltf_format'] == 'GLTF_SEPARATE':

            uri = ImageData(
                data=new_data,
                mime_type=new_mime_type,
                name=source.uri.name
            )
            buffer_view = None
            name = source.uri.name
            image.set_real_uri(uri, export_settings)  # Note: image, here, is the imported image python file

        else:
            buffer_view = BinaryData(data=new_data)
            uri = None
            name = source.name

        webp_image = __make_webp_image(buffer_view, None, None, new_mime_type, name, uri, export_settings)

        ext_webp["source"] = webp_image

# If user doesn't want to export in KTX2 format, but want KTX2 too. Texture is not KTX2
    if export_settings['gltf_image_format'] != "KTX2" \
            and export_settings['gltf_add_compressed_images'] is True \
            and export_settings['gltf_compressed_images_type'] == "KTX2" \
            and source is not None \
            and source.mime_type != "image/ktx2":
        # We need here to create some KTX2 textures

        new_mime_type = "image/ktx2"
        new_data, _ = image_data.encode(new_mime_type, export_settings)
        if len(new_data) == 0:
            export_settings['log'].warning("Image data is empty, not exporting image")
            return None, False

        if export_settings['gltf_format'] == 'GLTF_SEPARATE':

            uri = ImageData(
                data=new_data,
                mime_type=new_mime_type,
                name=source.uri.name
            )
            buffer_view = None
            name = source.uri.name
            image.set_real_uri(uri, export_settings)  # Note: image, here, is the imported image python file

        else:
            buffer_view = BinaryData(data=new_data)
            uri = None
            name = source.name

        ktx2_image = __make_ktx2_image(buffer_view, None, None, new_mime_type, name, uri, export_settings)

        ext_ktx2["source"] = ktx2_image

# If user doesn't want to export in WebP format, but want WebP too.
# Texture is WebP => Need to make webp required, an remove original
    if export_settings['gltf_image_format'] != "WEBP" \
            and source is not None \
            and source.mime_type == "image/webp":

        # User does not want fallback
        if export_settings['gltf_compressed_images_fallback'] is False:
            ext_webp["source"] = source
            remove_source = True
            required_webp = True

# Is user doesn't want to export in KTX2 format, but want KTX2 too.
# Texture is KTX2 => Need to make KTX2 required, an remove original
    if export_settings['gltf_image_format'] != "KTX2" \
            and source is not None \
            and source.mime_type == "image/ktx2":

        # User does not want fallback
        if export_settings['gltf_compressed_images_fallback'] is False:
            ext_ktx2["source"] = source
            remove_source = True
            required_ktx2 = True

# If user doesn't want to export in webp format, but want WebP too as fallback. Texture is WebP
    if export_settings['gltf_image_format'] != "WEBP" \
            and webp_image is not None \
            and export_settings['gltf_compressed_images_fallback'] is True:
        # Already managed in __gather_source, we only have to assign
        ext_webp["source"] = webp_image

        # Not needed in code, for for documentation:
        # remove_source = False
        # required_webp = False


# If user doesn't want to export in KTX2 format, but want KTX2 too as fallback. Texture is KTX2
    if export_settings['gltf_image_format'] != "KTX2" \
            and ktx2_image is not None \
            and export_settings['gltf_compressed_images_fallback'] is True:
        # Already managed in __gather_source, we only have to assign
        ext_ktx2["source"] = ktx2_image

        # Not needed in code, for for documentation:
        # remove_source = False
        # required_ktx2 = False

    some_extension_added = len(ext_webp) > 0 or len(ext_ktx2) > 0
    if len(ext_webp) > 0:
        extensions["EXT_texture_webp"] = Extension('EXT_texture_webp', ext_webp, required_webp)

    if len(ext_ktx2) > 0:
        extensions["KHR_texture_basisu"] = Extension('KHR_texture_basisu', ext_ktx2, required_ktx2)

    if some_extension_added:
        return extensions, remove_source
    else:
        return None, False


@cached
def __make_webp_image(buffer_view, extensions, extras, mime_type, name, uri, export_settings):
    return gltf2_io.Image(
        buffer_view=buffer_view,
        extensions=extensions,
        extras=extras,
        mime_type=mime_type,
        name=name,
        uri=uri
    )


@cached
def __make_ktx2_image(buffer_view, extensions, extras, mime_type, name, uri, export_settings):
    return gltf2_io.Image(
        buffer_view=buffer_view,
        extensions=extensions,
        extras=extras,
        mime_type=mime_type,
        name=name,
        uri=uri
    )


def __gather_extras(blender_shader_sockets, export_settings):
    return None


def __gather_name(blender_shader_sockets, export_settings):
    return None


def __gather_sampler(blender_shader_sockets, export_settings):
    shader_nodes = [get_texture_node_from_socket(socket, export_settings) for socket in blender_shader_sockets]
    if len(shader_nodes) > 1:
        export_settings['log'].warning(
            "More than one shader node tex image used for a texture. "
            "The resulting glTF sampler will behave like the first shader node tex image."
        )
    first_valid_shader_node = next(filter(lambda x: x is not None, shader_nodes))

    # group_path can't be a list, so transform it to str

    sep_item = "##~~gltf-sep~~##"
    sep_inside_item = "##~~gltf-inside-sep~~##"
    group_path_str = ""
    if len(first_valid_shader_node.group_path) > 0:
        # Retrieving the blender material using this shader tree
        for mat in bpy.data.materials:
            if id(mat.node_tree) == id(first_valid_shader_node.group_path[0].original):
                group_path_str += mat.name  # TODO if linked, we can have multiple materials with same name...
                break

    if group_path_str == "":
        # Inline node tree ?
        return gather_sampler(
            first_valid_shader_node.shader_node,
            group_path_str,
            export_settings)

    if len(first_valid_shader_node.group_path) > 1:
        for idx, i in enumerate(first_valid_shader_node.group_path[1:]):
            group_path_str += sep_item
            if idx == 0:
                group_path_str += first_valid_shader_node.group_path[0].name
            else:
                group_path_str += i.id_data.name
            group_path_str += sep_inside_item
            group_path_str += i.name

    return gather_sampler(
        first_valid_shader_node.shader_node,
        group_path_str,
        export_settings)


def __gather_source(blender_shader_sockets, use_tile, is_data, export_settings):
    source, image_data, factor, udim_image = image.gather_image(
        blender_shader_sockets, use_tile, is_data, export_settings)

    if export_settings['gltf_keep_original_textures'] is False \
            and export_settings['gltf_image_format'] != "KTX2" \
            and source is not None \
            and source.mime_type == "image/ktx2":
        if export_settings['gltf_compressed_images_fallback'] is False:
            # Already managed in __gather_extensions
            return source, None, None, image_data, factor, udim_image
        else:
            # Need to create a PNG texture

            new_mime_type = "image/png"
            new_data, _ = image_data.encode(new_mime_type, export_settings)
            # We should not have empty data here, as we are calculating fallback, so the real image should be ok already

            if export_settings['gltf_format'] == 'GLTF_SEPARATE':
                buffer_view = None
                uri = ImageData(
                    data=new_data,
                    mime_type=new_mime_type,
                    name=source.uri.name
                )
                name = source.uri.name

                image.set_real_uri(uri, export_settings)  # Note: image, here, is the imported image python file

            else:
                uri = None
                buffer_view = BinaryData(data=new_data)
                name = source.name

            png_image = __make_ktx2_image(buffer_view, None, None, new_mime_type, name, uri, export_settings)

        # We inverted the png & KTX2 image, to have the png as main source
        return png_image, None, source, image_data, factor, udim_image

    if export_settings['gltf_keep_original_textures'] is False \
            and export_settings['gltf_image_format'] != "WEBP" \
            and source is not None \
            and source.mime_type == "image/webp":

        if export_settings['gltf_compressed_images_fallback'] is False:
            # Already managed in __gather_extensions
            return source, None, None, image_data, factor, udim_image
        else:
            # Need to create a PNG texture

            new_mime_type = "image/png"
            new_data, _ = image_data.encode(new_mime_type, export_settings)
            # We should not have empty data here, as we are calculating fallback, so the real image should be ok already

            if export_settings['gltf_format'] == 'GLTF_SEPARATE':
                buffer_view = None
                uri = ImageData(
                    data=new_data,
                    mime_type=new_mime_type,
                    name=source.uri.name
                )
                name = source.uri.name

                image.set_real_uri(uri, export_settings)  # Note: image, here, is the imported image python file

            else:
                uri = None
                buffer_view = BinaryData(data=new_data)
                name = source.name

            png_image = __make_webp_image(buffer_view, None, None, new_mime_type, name, uri, export_settings)

        # We inverted the png & WebP image, to have the png as main source
        return png_image, source, None, image_data, factor, udim_image
    return source, None, None, image_data, factor, udim_image
