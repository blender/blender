# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
import os

from ....io.com import gltf2_io
from ....io.com.path import path_to_uri
from ....io.exp import binary_data as gltf2_io_binary_data, image_data as gltf2_io_image_data
from ....io.com import debug as gltf2_io_debug
from ....io.exp.user_extensions import export_user_extensions
from ..cache import cached
from .encode_image import Channel, ExportImage, FillImage, FillImageTile, FillImageRGB2BW
from .search_node_tree import get_texture_node_from_socket, detect_anisotropy_nodes


@cached
def gather_image(
        blender_shader_sockets: typing.Tuple[bpy.types.NodeSocket],
        use_tile: bool,
        export_settings):
    if not __filter_image(blender_shader_sockets, export_settings):
        return None, None, None, None

    image_data, udim_image = __get_image_data(blender_shader_sockets, use_tile, export_settings)

    if udim_image is not None:
        # We are in a UDIM case, so we return no image data
        # This will be used later to create multiple primitives/material/texture with UDIM information
        return None, None, None, udim_image

    if image_data.empty():
        # The export image has no data
        return None, None, None, None

    mime_type = __gather_mime_type(blender_shader_sockets, image_data, export_settings)
    name = __gather_name(image_data, use_tile, export_settings)

    if use_tile is not None:
        name = name.replace("<UDIM>", str(export_settings['current_udim_info']['tile']))

    factor = None

    if image_data.original is None:
        uri, factor_uri = __gather_uri(image_data, mime_type, name, export_settings)
    else:
        # Retrieve URI relative to exported glTF files
        uri = __gather_original_uri(
            image_data.original.filepath,
            blender_shader_sockets[0].socket.id_data.library,
            export_settings
        )
        # In case we can't retrieve image (for example packed images, with original moved)
        # We don't create invalid image without uri
        factor_uri = None
        if uri is None:
            return None, None, None, None

    buffer_view, factor_buffer_view = __gather_buffer_view(image_data, mime_type, name, export_settings)

    factor = factor_uri if uri is not None else factor_buffer_view

    image = __make_image(
        buffer_view,
        __gather_extensions(blender_shader_sockets, export_settings),
        __gather_extras(blender_shader_sockets, export_settings),
        mime_type,
        name,
        uri,
        export_settings
    )

    export_user_extensions('gather_image_hook', export_settings, image, blender_shader_sockets)

    # We also return image_data, as it can be used to generate same file with another extension for WebP management
    return image, image_data, factor, None


def __gather_original_uri(original_uri, library, export_settings):
    path_to_image = bpy.path.abspath(original_uri, library=library)

    if not os.path.exists(path_to_image):
        return None
    try:
        rel_path = os.path.relpath(
            path_to_image,
            start=export_settings['gltf_filedirectory'],
        )
    except ValueError:
        # eg. because no relative path between C:\ and D:\ on Windows
        return None
    return path_to_uri(rel_path)


@cached
def __make_image(buffer_view, extensions, extras, mime_type, name, uri, export_settings):
    return gltf2_io.Image(
        buffer_view=buffer_view,
        extensions=extensions,
        extras=extras,
        mime_type=mime_type,
        name=name,
        uri=uri
    )


def __filter_image(sockets, export_settings):
    if not sockets:
        return False
    return True


@cached
def __gather_buffer_view(image_data, mime_type, name, export_settings):
    if export_settings['gltf_format'] != 'GLTF_SEPARATE':
        data, factor = image_data.encode(mime_type, export_settings)
        return gltf2_io_binary_data.BinaryData(data=data), factor
    return None, None


def __gather_extensions(sockets, export_settings):
    return None


def __gather_extras(sockets, export_settings):
    return None


def __gather_mime_type(sockets, export_image, export_settings):
    # force png or webp if Alpha contained so we can export alpha
    for socket in sockets:
        if socket.socket.name == "Alpha":
            if export_settings["gltf_image_format"] == "WEBP":
                return "image/webp"
            else:
                # If we keep image as is (no channel composition), we need to keep original format (for WebP)
                image = export_image.blender_image(export_settings)
                if image is not None and __is_blender_image_a_webp(image):
                    return "image/webp"
                return "image/png"

    if export_settings["gltf_image_format"] == "AUTO":
        if export_image.original is None:  # We are going to create a new image
            image = export_image.blender_image(export_settings)
        else:
            # Using original image
            image = export_image.original

        if image is not None and __is_blender_image_a_jpeg(image):
            return "image/jpeg"
        elif image is not None and __is_blender_image_a_webp(image):
            return "image/webp"
        return "image/png"

    elif export_settings["gltf_image_format"] == "WEBP":
        return "image/webp"
    elif export_settings["gltf_image_format"] == "JPEG":
        return "image/jpeg"


def __gather_name(export_image, use_tile, export_settings):
    if export_image.original is None:
        # Find all Blender images used in the ExportImage

        if use_tile is not None:
            FillCheck = FillImageTile
        else:
            FillCheck = FillImage

        imgs = []
        for fill in export_image.fills.values():
            if isinstance(fill, FillCheck) or isinstance(fill, FillImageRGB2BW):
                img = fill.image
                if img not in imgs:
                    imgs.append(img)

        # If all the images have the same path, use the common filename
        filepaths = set(img.filepath for img in imgs)
        if len(filepaths) == 1:
            filename = os.path.basename(list(filepaths)[0])
            name, extension = os.path.splitext(filename)
            if extension.lower() in ['.png', '.jpg', '.jpeg']:
                if name:
                    return name

        # Combine the image names: img1-img2-img3
        names = []
        for img in imgs:
            name, extension = os.path.splitext(img.name)
            names.append(name)
        name = '-'.join(names)
        return name or 'Image'
    else:
        return export_image.original.name


@cached
def __gather_uri(image_data, mime_type, name, export_settings):
    if export_settings['gltf_format'] == 'GLTF_SEPARATE':
        # as usual we just store the data in place instead of already resolving the references
        data, factor = image_data.encode(mime_type, export_settings)
        return gltf2_io_image_data.ImageData(
            data=data,
            mime_type=mime_type,
            name=name
        ), factor

    return None, None


def __get_image_data(sockets, use_tile, export_settings) -> ExportImage:
    # For shared resources, such as images, we just store the portion of data that is needed in the glTF property
    # in a helper class. During generation of the glTF in the exporter these will then be combined to actual binary
    # resources.
    results = [get_texture_node_from_socket(socket, export_settings) for socket in sockets]

    if use_tile is None:
        # First checking if texture used is UDIM
        # In that case, we return no texture data for now, and only get that this texture is UDIM
        # This will be used later
        if any([r.shader_node.image.source == "TILED" for r in results if r is not None and r.shader_node.image is not None]):
            return ExportImage(), [
                r.shader_node.image for r in results if r is not None and r.shader_node.image is not None and r.shader_node.image.source == "TILED"][0]

    # If we are here, we are in UDIM split process
    # Check if we need a simple mapping or more complex calculation

    # Case of Anisotropy : It can be a complex node setup, or simple grayscale textures
    # In case of complex node setup, this will be a direct mapping of channels
    # But in case of grayscale textures, we need to combine them, we numpy calculations
    # So we need to check if we have a complex node setup or not

    need_to_check_anisotropy = is_anisotropy = False
    try:
        anisotropy_socket = [s for s in sockets if s.socket.name == 'Anisotropic'][0]
        anisotropy_rotation_socket = [s for s in sockets if s.socket.name == 'Anisotropic Rotation'][0]
        anisotropy_tangent_socket = [s for s in sockets if s.socket.name == 'Tangent'][0]
        need_to_check_anisotropy = True
    except:
        need_to_check_anisotropy = False

    if need_to_check_anisotropy is True:
        is_anisotropy, anisotropy_data = detect_anisotropy_nodes(
            anisotropy_socket,
            anisotropy_rotation_socket,
            anisotropy_tangent_socket,
            export_settings
        )

    if need_to_check_anisotropy is True and is_anisotropy is False:
        # We are not in complex node setup, so we can try to get the image data from grayscale textures
        return __get_image_data_grayscale_anisotropy(sockets, results, export_settings), None

    return __get_image_data_mapping(sockets, results, use_tile, export_settings), None


def __get_image_data_mapping(sockets, results, use_tile, export_settings) -> ExportImage:
    """
    Simple mapping
    Will fit for most of exported textures : RoughnessMetallic, Basecolor, normal, ...
    """
    composed_image = ExportImage()

    for result, socket in zip(results, sockets):
        # Assume that user know what he does, and that channels/images are already combined correctly for pbr
        # If not, we are going to keep only the first texture found
        # Example : If user set up 2 or 3 different textures for Metallic / Roughness / Occlusion
        # Only 1 will be used at export
        # This Warning is displayed in UI of this option
        if export_settings['gltf_keep_original_textures']:
            composed_image = ExportImage.from_original(result.shader_node.image)

        else:
            # rudimentarily try follow the node tree to find the correct image data.
            src_chan = None
            for elem in result.path:
                if isinstance(elem.from_node, bpy.types.ShaderNodeSeparateColor):
                    src_chan = {
                        'Red': Channel.R,
                        'Green': Channel.G,
                        'Blue': Channel.B,
                    }[elem.from_socket.name]
                if elem.from_socket.name == 'Alpha':
                    src_chan = Channel.A

            if src_chan is None:
                # No SeparateColor node found, so take the specification channel that is needed
                # So export is correct if user plug the texture directly to the socket
                if socket.socket.name == 'Metallic':
                    src_chan = Channel.B
                elif socket.socket.name == 'Roughness':
                    src_chan = Channel.G
                elif socket.socket.name == 'Occlusion':
                    src_chan = Channel.R
                elif socket.socket.name == 'Alpha':
                    # For alpha, we need to check if we have a texture plugged in a Color socket
                    # In that case, we will convert RGB to BW
                    if elem.from_socket.type == "RGBA":
                        src_chan = Channel.RGB2BW
                    else:
                        src_chan = Channel.A
                elif socket.socket.name == 'Coat Weight':
                    src_chan = Channel.R
                elif socket.socket.name == 'Coat Roughness':
                    src_chan = Channel.G
                elif socket.socket.name == 'Thickness':  # For KHR_materials_volume
                    src_chan = Channel.G

            if src_chan is None:
                # Seems we can't find the channel
                # We are in a case where user plugged a texture in a Color socket, but we may have used the alpha one
                if socket.socket.name in ["Alpha", "Specular IOR Level", "Sheen Roughness"]:
                    src_chan = Channel.A

            if src_chan is None:
                # We definitely can't find the channel, so keep the first channel even if this is wrong
                src_chan = Channel.R

            dst_chan = None

            # some sockets need channel rewriting (gltf pbr defines fixed channels for some attributes)
            if socket.socket.name == 'Metallic':
                dst_chan = Channel.B
            elif socket.socket.name == 'Roughness':
                dst_chan = Channel.G
            elif socket.socket.name == 'Occlusion':
                dst_chan = Channel.R
            elif socket.socket.name == 'Alpha':
                dst_chan = Channel.A
            elif socket.socket.name == 'Coat Weight':
                dst_chan = Channel.R
            elif socket.socket.name == 'Coat Roughness':
                dst_chan = Channel.G
            elif socket.socket.name == 'Thickness':  # For KHR_materials_volume
                dst_chan = Channel.G
            elif socket.socket.name == "Specular IOR Level":  # For KHR_materials_specular
                dst_chan = Channel.A
            elif socket.socket.name == "Sheen Roughness":  # For KHR_materials_sheen
                dst_chan = Channel.A

            if dst_chan is not None:
                if use_tile is None:
                    if src_chan == Channel.RGB2BW:
                        composed_image.fill_image_bw(result.shader_node.image, dst_chan)
                    else:
                        composed_image.fill_image(result.shader_node.image, dst_chan, src_chan)
                else:
                    if src_chan == Channel.RGB2BW:
                        composed_image.fill_image_bw_tile(
                            result.shader_node.image, export_settings['current_udim_info']['tile'], dst_chan)
                    else:
                        composed_image.fill_image_tile(
                            result.shader_node.image,
                            export_settings['current_udim_info']['tile'],
                            dst_chan,
                            src_chan)

                # Since metal/roughness are always used together, make sure
                # the other channel is filled.
                if socket.socket.name == 'Metallic' and not composed_image.is_filled(Channel.G):
                    composed_image.fill_white(Channel.G)
                elif socket.socket.name == 'Roughness' and not composed_image.is_filled(Channel.B):
                    composed_image.fill_white(Channel.B)

                # Since Alpha is always used together with BaseColor in glTF, make sure
                # the other channels are filled.
                # All these channels may be overwritten later by BaseColor socket
                if socket.socket.name == 'Alpha' and not composed_image.is_filled(Channel.R):
                    composed_image.fill_white(Channel.R)
                if socket.socket.name == 'Alpha' and not composed_image.is_filled(Channel.G):
                    composed_image.fill_white(Channel.G)
                if socket.socket.name == 'Alpha' and not composed_image.is_filled(Channel.B):
                    composed_image.fill_white(Channel.B)

            else:
                # copy full image...eventually following sockets might overwrite things
                if use_tile is None:
                    composed_image = ExportImage.from_blender_image(result.shader_node.image)
                else:
                    composed_image = ExportImage.from_blender_image_tile(export_settings)

    # Check that we don't have some empty channels (based on weird images without any size for example)
    keys = list(composed_image.fills.keys())  # do not loop on dict, we may have to delete an element
    for k in [
        k for k in keys if isinstance(
            composed_image.fills[k],
            FillImage) or isinstance(
            composed_image.fills[k],
            FillImageRGB2BW)]:
        if composed_image.fills[k].image.size[0] == 0 or composed_image.fills[k].image.size[1] == 0:
            export_settings['log'].warning("Image '{}' has no size and cannot be exported.".format(
                composed_image.fills[k].image))
            del composed_image.fills[k]

    return composed_image


def __get_image_data_grayscale_anisotropy(sockets, results, export_settings) -> ExportImage:
    """
    calculating Anisotropy Texture from grayscale textures, settings needed data
    """
    from .extensions.anisotropy import grayscale_anisotropy_calculation
    composed_image = ExportImage()
    composed_image.set_calc(grayscale_anisotropy_calculation)

    results = [get_texture_node_from_socket(socket, export_settings)
               for socket in sockets[:-1]]  # No texture from tangent

    mapping = {
        0: "anisotropy",
        1: "anisotropic_rotation",
    }

    for idx, result in enumerate(results):
        if get_texture_node_from_socket(sockets[idx], export_settings):
            composed_image.store_data(mapping[idx], result.shader_node.image, type="Image")
        else:
            composed_image.store_data(mapping[idx], sockets[idx].socket.default_value, type="Data")

    return composed_image


def __is_blender_image_a_jpeg(image: bpy.types.Image) -> bool:
    if image.source not in ['FILE', 'TILED']:
        return False
    if image.filepath_raw == '' and image.packed_file:
        return image.packed_file.data[:3] == b'\xff\xd8\xff'
    else:
        path = image.filepath_raw.lower()
        return path.endswith('.jpg') or path.endswith('.jpeg') or path.endswith('.jpe')


def __is_blender_image_a_webp(image: bpy.types.Image) -> bool:
    if image.source not in ['FILE', 'TILED']:
        return False
    if image.filepath_raw == '' and image.packed_file:
        return image.packed_file.data[8:12] == b'WEBP'
    else:
        path = image.filepath_raw.lower()
        return path.endswith('.webp')


def get_gltf_image_from_blender_image(blender_image_name, export_settings):
    image_data = ExportImage.from_blender_image(bpy.data.images[blender_image_name])

    name = __gather_name(image_data, None, export_settings)
    mime_type = __get_mime_type_of_image(blender_image_name, export_settings)

    uri, _ = __gather_uri(image_data, mime_type, name, export_settings)
    buffer_view, _ = __gather_buffer_view(image_data, mime_type, name, export_settings)

    return gltf2_io.Image(
        buffer_view=buffer_view,
        extensions=None,
        extras=None,
        mime_type=mime_type,
        name=name,
        uri=uri
    )


def __get_mime_type_of_image(blender_image_name, export_settings):

    image = bpy.data.images[blender_image_name]
    if image.channels == 4:
        if __is_blender_image_a_webp(image):
            return "image/webp"
        return "image/png"

    if export_settings["gltf_image_format"] == "AUTO":
        if __is_blender_image_a_jpeg(image):
            return "image/jpeg"
        elif __is_blender_image_a_webp(image):
            return "image/webp"
        return "image/png"

    elif export_settings["gltf_image_format"] == "JPEG":
        return "image/jpeg"
