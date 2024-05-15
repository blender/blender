# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from ...io.com.gltf2_io import Sampler
from ...io.com.gltf2_io_constants import TextureFilter, TextureWrap
from ...io.imp.gltf2_io_user_extensions import import_user_extensions
from ..com.gltf2_blender_conversion import texture_transform_gltf_to_blender
from .gltf2_blender_image import BlenderImage


def texture(
    mh,
    tex_info,
    location,  # Upper-right corner of the TexImage node
    label,  # Label for the TexImg node
    color_socket,
    alpha_socket=None,
    is_data=False,
    forced_image=None
):
    """Creates nodes for a TextureInfo and hooks up the color/alpha outputs."""
    x, y = location
    pytexture = mh.gltf.data.textures[tex_info.index]

    import_user_extensions('gather_import_texture_before_hook', mh.gltf, pytexture, mh,
                           tex_info, location, label, color_socket, alpha_socket, is_data)

    if pytexture.sampler is not None:
        pysampler = mh.gltf.data.samplers[pytexture.sampler]
    else:
        pysampler = Sampler.from_dict({})

    needs_uv_map = False  # whether to create UVMap node

    # Image Texture
    tex_img = mh.node_tree.nodes.new('ShaderNodeTexImage')
    tex_img.location = x - 240, y
    tex_img.label = label

    # Get image
    if forced_image is None:
        source = get_source(mh, pytexture)
        if source is not None:
            BlenderImage.create(mh.gltf, source)
            pyimg = mh.gltf.data.images[source]
            blender_image_name = pyimg.blender_image_name
            if blender_image_name:
                tex_img.image = bpy.data.images[blender_image_name]
    else:
        tex_img.image = forced_image
    # Set colorspace for data images
    if is_data:
        if tex_img.image:
            tex_img.image.colorspace_settings.is_data = True
    # Set filtering
    set_filtering(tex_img, pysampler)
    # Outputs
    if color_socket is not None:
        mh.node_tree.links.new(color_socket, tex_img.outputs['Color'])
    if alpha_socket is not None:
        mh.node_tree.links.new(alpha_socket, tex_img.outputs['Alpha'])
    # Inputs
    uv_socket = tex_img.inputs[0]

    x -= 340

    # Do wrapping
    wrap_s = pysampler.wrap_s
    wrap_t = pysampler.wrap_t
    if wrap_s is None:
        wrap_s = TextureWrap.Repeat
    if wrap_t is None:
        wrap_t = TextureWrap.Repeat
    # If wrapping is the same in both directions, just set tex_img.extension
    if wrap_s == wrap_t == TextureWrap.Repeat:
        tex_img.extension = 'REPEAT'
    elif wrap_s == wrap_t == TextureWrap.ClampToEdge:
        tex_img.extension = 'EXTEND'
    elif wrap_s == wrap_t == TextureWrap.MirroredRepeat:
        tex_img.extension = 'MIRROR'
    else:
        # Otherwise separate the UV components and use math nodes to compute
        # the wrapped UV coordinates
        # => [Separate XYZ] => [Wrap for S] => [Combine XYZ] =>
        #                   => [Wrap for T] =>

        tex_img.extension = 'EXTEND'  # slightly better errors near the edge than REPEAT

        # Combine XYZ
        com_uv = mh.node_tree.nodes.new('ShaderNodeCombineXYZ')
        com_uv.location = x - 140, y - 100
        mh.node_tree.links.new(uv_socket, com_uv.outputs[0])
        u_socket = com_uv.inputs[0]
        v_socket = com_uv.inputs[1]
        x -= 200

        for i in [0, 1]:
            wrap = [wrap_s, wrap_t][i]
            socket = [u_socket, v_socket][i]
            if wrap == TextureWrap.Repeat:
                # WRAP node for REPEAT
                math = mh.node_tree.nodes.new('ShaderNodeMath')
                math.location = x - 140, y + 30 - i * 200
                math.operation = 'WRAP'
                math.inputs[1].default_value = 0
                math.inputs[2].default_value = 1
                mh.node_tree.links.new(socket, math.outputs[0])
                socket = math.inputs[0]
            elif wrap == TextureWrap.MirroredRepeat:
                # PINGPONG node for MIRRORED_REPEAT
                math = mh.node_tree.nodes.new('ShaderNodeMath')
                math.location = x - 140, y + 30 - i * 200
                math.operation = 'PINGPONG'
                math.inputs[1].default_value = 1
                mh.node_tree.links.new(socket, math.outputs[0])
                socket = math.inputs[0]
            else:
                # Pass-through CLAMP since the tex_img node is set to EXTEND
                pass
            if i == 0:
                u_socket = socket
            else:
                v_socket = socket
        x -= 200

        # Separate XYZ
        sep_uv = mh.node_tree.nodes.new('ShaderNodeSeparateXYZ')
        sep_uv.location = x - 140, y - 100
        mh.node_tree.links.new(u_socket, sep_uv.outputs[0])
        mh.node_tree.links.new(v_socket, sep_uv.outputs[1])
        uv_socket = sep_uv.inputs[0]
        x -= 200

        needs_uv_map = True

    # UV Transform (for KHR_texture_transform)
    needs_tex_transform = 'KHR_texture_transform' in (tex_info.extensions or {})

    # We also need to create tex transform if this property is animated in KHR_animation_pointer
    if mh.gltf.data.extensions_used is not None and "KHR_animation_pointer" in mh.gltf.data.extensions_used:
        if tex_info.extensions is not None and "KHR_texture_transform" in tex_info.extensions:
            if len(tex_info.extensions["KHR_texture_transform"]["animations"]) > 0:
                for anim_idx in tex_info.extensions["KHR_texture_transform"]["animations"].keys():
                    for channel_idx in tex_info.extensions["KHR_texture_transform"]["animations"][anim_idx]:
                        channel = mh.gltf.data.animations[anim_idx].channels[channel_idx]
                        pointer_tab = channel.target.extensions["KHR_animation_pointer"]["pointer"].split("/")
                        if len(pointer_tab) >= 7 and pointer_tab[1] == "materials" and \
                                pointer_tab[-3] == "extensions" and \
                                pointer_tab[-2] == "KHR_texture_transform" and \
                                pointer_tab[-1] in ["scale", "offset", "rotation"]:
                            needs_tex_transform = True
                            # Store multiple channel data, as we will need all channels to convert to
                            # blender data when animated
                            if "multiple_channels" not in tex_info.extensions['KHR_texture_transform'].keys():
                                tex_info.extensions['KHR_texture_transform']["multiple_channels"] = {}
                            tex_info.extensions['KHR_texture_transform']["multiple_channels"][pointer_tab[-1]
                                                                                              ] = (anim_idx, channel_idx)

    if needs_tex_transform:
        mapping = mh.node_tree.nodes.new('ShaderNodeMapping')
        mapping.location = x - 160, y + 30
        mapping.vector_type = 'POINT'
        # Outputs
        mh.node_tree.links.new(uv_socket, mapping.outputs[0])
        # Inputs
        uv_socket = mapping.inputs[0]

        transform = tex_info.extensions['KHR_texture_transform']
        transform = texture_transform_gltf_to_blender(transform)
        mapping.inputs['Location'].default_value[0] = transform['offset'][0]
        mapping.inputs['Location'].default_value[1] = transform['offset'][1]
        mapping.inputs['Rotation'].default_value[2] = transform['rotation']
        mapping.inputs['Scale'].default_value[0] = transform['scale'][0]
        mapping.inputs['Scale'].default_value[1] = transform['scale'][1]

        x -= 260
        needs_uv_map = True

        # Needed for KHR_animation_pointer
        tex_info.extensions['KHR_texture_transform']['blender_nodetree'] = mh.node_tree

    # UV Map
    uv_idx = tex_info.tex_coord or 0
    try:
        uv_idx = tex_info.extensions['KHR_texture_transform']['texCoord']
    except Exception:
        pass
    if uv_idx != 0 or needs_uv_map:
        uv_map = mh.node_tree.nodes.new('ShaderNodeUVMap')
        uv_map.location = x - 160, y - 70
        uv_map.uv_map = 'UVMap' if uv_idx == 0 else 'UVMap.%03d' % uv_idx
        # Outputs
        mh.node_tree.links.new(uv_socket, uv_map.outputs[0])

    import_user_extensions('gather_import_texture_after_hook', mh.gltf, pytexture, mh.node_tree,
                           mh, tex_info, location, label, color_socket, alpha_socket, is_data)


def get_source(mh, pytexture):
    src = pytexture.source
    try:
        webp_src = pytexture.extensions['EXT_texture_webp']['source']
    except Exception:
        webp_src = None

    if mh.gltf.import_settings['import_webp_texture']:
        return webp_src if webp_src is not None else src
    else:
        return src if src is not None else webp_src


def set_filtering(tex_img, pysampler):
    """Set the filtering/interpolation on an Image Texture from the glTf sampler."""
    minf = pysampler.min_filter
    magf = pysampler.mag_filter

    # Ignore mipmapping
    if minf in [TextureFilter.NearestMipmapNearest, TextureFilter.NearestMipmapLinear]:
        minf = TextureFilter.Nearest
    elif minf in [TextureFilter.LinearMipmapNearest, TextureFilter.LinearMipmapLinear]:
        minf = TextureFilter.Linear

    # If both are nearest or the only specified one was nearest, use nearest.
    if (minf, magf) in [
        (TextureFilter.Nearest, TextureFilter.Nearest),
        (TextureFilter.Nearest, None),
        (None, TextureFilter.Nearest),
    ]:
        tex_img.interpolation = 'Closest'
    else:
        tex_img.interpolation = 'Linear'
