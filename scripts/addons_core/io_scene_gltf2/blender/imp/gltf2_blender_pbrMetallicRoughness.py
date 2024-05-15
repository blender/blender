# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from re import M
import bpy
from ...io.com.gltf2_io_constants import GLTF_IOR, BLENDER_COAT_ROUGHNESS
from ...io.com.gltf2_io import TextureInfo, MaterialPBRMetallicRoughness
from ..com.gltf2_blender_material_helpers import get_gltf_node_name, create_settings_group
from .gltf2_blender_texture import texture
from .gltf2_blender_KHR_materials_anisotropy import anisotropy
from .gltf2_blender_material_utils import \
    MaterialHelper, scalar_factor_and_texture, color_factor_and_texture, normal_map


def pbr_metallic_roughness(mh: MaterialHelper):
    """Creates node tree for pbrMetallicRoughness materials."""
    pbr_node = mh.nodes.new('ShaderNodeBsdfPrincipled')
    out_node = mh.nodes.new('ShaderNodeOutputMaterial')
    pbr_node.location = 10, 300
    out_node.location = 300, 300
    mh.links.new(pbr_node.outputs[0], out_node.inputs[0])

    need_volume_node = False  # need a place to attach volume?
    need_settings_node = False  # need a place to attach occlusion/thickness?

    if mh.pymat.occlusion_texture is not None:
        need_settings_node = True

    if volume_ext := mh.get_ext('KHR_materials_volume'):
        if volume_ext.get('thicknessFactor', 0) != 0:
            need_volume_node = True
            need_settings_node = True

    # We also need volume node for KHR_animation_pointer
    if mh.gltf.data.extensions_used is not None and "KHR_animation_pointer" in mh.gltf.data.extensions_used:
        if mh.pymat.extensions and "KHR_materials_volume" in mh.pymat.extensions and len(
                mh.pymat.extensions["KHR_materials_volume"]["animations"]) > 0:
            for anim_idx in mh.pymat.extensions["KHR_materials_volume"]["animations"].keys():
                for channel_idx in mh.pymat.extensions["KHR_materials_volume"]["animations"][anim_idx]:
                    channel = mh.gltf.data.animations[anim_idx].channels[channel_idx]
                    pointer_tab = channel.target.extensions["KHR_animation_pointer"]["pointer"].split("/")
                    if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                            pointer_tab[3] == "extensions" and \
                            pointer_tab[4] == "KHR_materials_volume" and \
                            pointer_tab[5] in ["thicknessFactor", "attenuationDistance", "attenuationColor"]:
                        need_volume_node = True
                        need_settings_node = True

    if need_settings_node:
        mh.settings_node = make_settings_node(mh)
        mh.settings_node.location = 40, -370
        mh.settings_node.width = 180

    if need_volume_node:
        volume_node = mh.nodes.new('ShaderNodeVolumeAbsorption')
        volume_node.location = 40, -520 if need_settings_node else -370
        mh.links.new(out_node.inputs[1], volume_node.outputs[0])

    locs = calc_locations(mh)

    emission(
        mh,
        location=locs['emission'],
        color_socket=pbr_node.inputs['Emission Color'],
        strength_socket=pbr_node.inputs['Emission Strength'],
    )

    base_color(
        mh,
        location=locs['base_color'],
        color_socket=pbr_node.inputs['Base Color'],
        alpha_socket=pbr_node.inputs['Alpha'] if not mh.is_opaque() else None,
    )

    metallic_roughness(
        mh,
        location=locs['metallic_roughness'],
        metallic_socket=pbr_node.inputs['Metallic'],
        roughness_socket=pbr_node.inputs['Roughness'],
    )

    normal(
        mh,
        location=locs['normal'],
        normal_socket=pbr_node.inputs['Normal'],
    )

    if mh.pymat.occlusion_texture is not None:
        occlusion(
            mh,
            location=locs['occlusion'],
            occlusion_socket=mh.settings_node.inputs['Occlusion'],
        )

    clearcoat(mh, locs, pbr_node)

    transmission(mh, locs, pbr_node)

    if need_volume_node:
        volume(
            mh,
            location=locs['volume_thickness'],
            volume_node=volume_node,
            thickness_socket=mh.settings_node.inputs[1] if mh.settings_node else None
        )

    specular(mh, locs, pbr_node)

    anisotropy(
        mh,
        location=locs['anisotropy'],
        anisotropy_socket=pbr_node.inputs['Anisotropic'],
        anisotropy_rotation_socket=pbr_node.inputs['Anisotropic Rotation'],
        anisotropy_tangent_socket=pbr_node.inputs['Tangent']
    )

    sheen(mh, locs, pbr_node)

    # IOR
    ior_ext = mh.get_ext('KHR_materials_ior', {})
    ior = ior_ext.get('ior', GLTF_IOR)
    pbr_node.inputs['IOR'].default_value = ior

    if len(ior_ext) > 0:
        mh.pymat.extensions['KHR_materials_ior']['blender_nodetree'] = mh.node_tree  # Needed for KHR_animation_pointer
        mh.pymat.extensions['KHR_materials_ior']['blender_mat'] = mh.mat  # Needed for KHR_animation_pointer


def clearcoat(mh, locs, pbr_node):
    ext = mh.get_ext('KHR_materials_clearcoat', {})
    if len(ext) > 0:
        # Needed for KHR_animation_pointer
        mh.pymat.extensions['KHR_materials_clearcoat']['blender_nodetree'] = mh.node_tree
        mh.pymat.extensions['KHR_materials_clearcoat']['blender_mat'] = mh.mat  # Needed for KHR_animation_pointer

    # We will need clearcoat factor (Mix node) if animated by KHR_animation_pointer (and standard case if clearcoatFactor != 1)
    # Check if animated by KHR_animation_pointer
    force_clearcoat_factor = False
    if mh.gltf.data.extensions_used is not None and "KHR_animation_pointer" in mh.gltf.data.extensions_used:
        if mh.pymat.extensions and "KHR_materials_clearcoat" in mh.pymat.extensions and len(
                mh.pymat.extensions["KHR_materials_clearcoat"]["animations"]) > 0:
            for anim_idx in mh.pymat.extensions["KHR_materials_clearcoat"]["animations"].keys():
                for channel_idx in mh.pymat.extensions["KHR_materials_clearcoat"]["animations"][anim_idx]:
                    channel = mh.gltf.data.animations[anim_idx].channels[channel_idx]
                    pointer_tab = channel.target.extensions["KHR_animation_pointer"]["pointer"].split("/")
                    if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                            pointer_tab[3] == "extensions" and \
                            pointer_tab[4] == "KHR_materials_clearcoat" and \
                            pointer_tab[5] == "clearcoatFactor":
                        force_clearcoat_factor = True

    scalar_factor_and_texture(
        mh,
        location=locs['clearcoat'],
        label='Clearcoat',
        socket=pbr_node.inputs['Coat Weight'],
        factor=ext.get('clearcoatFactor', 0),
        tex_info=ext.get('clearcoatTexture'),
        channel=0,  # Red
        force_mix_node=force_clearcoat_factor
    )

    if len(ext) > 0:
        tex_info = TextureInfo.from_dict(ext.get('clearcoatTexture')) if ext.get(
            'clearcoatTexture') is not None else None
        # Because extensions are dict, they are not passed by reference
        # So we need to update the dict of the KHR_texture_transform extension if needed
        if tex_info is not None and tex_info.extensions is not None and "KHR_texture_transform" in tex_info.extensions:
            mh.pymat.extensions['KHR_materials_clearcoat']['clearcoatTexture']['extensions']['KHR_texture_transform'] = tex_info.extensions["KHR_texture_transform"]

    # We will need clearcoatRoughness factor (Mix node) if animated by
    # KHR_animation_pointer (and standard case if clearcoatRoughnessFactor !=
    # 1)
    force_clearcoat_roughness_factor = False
    # Check if animated by KHR_animation_pointer
    if mh.gltf.data.extensions_used is not None and "KHR_animation_pointer" in mh.gltf.data.extensions_used:
        if mh.pymat.extensions and "KHR_materials_clearcoat" in mh.pymat.extensions and len(
                mh.pymat.extensions["KHR_materials_clearcoat"]["animations"]) > 0:
            for anim_idx in mh.pymat.extensions["KHR_materials_clearcoat"]["animations"].keys():
                for channel_idx in mh.pymat.extensions["KHR_materials_clearcoat"]["animations"][anim_idx]:
                    channel = mh.gltf.data.animations[anim_idx].channels[channel_idx]
                    pointer_tab = channel.target.extensions["KHR_animation_pointer"]["pointer"].split("/")
                    if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                            pointer_tab[3] == "extensions" and \
                            pointer_tab[4] == "KHR_materials_clearcoat" and \
                            pointer_tab[5] == "clearcoatRoughnessFactor":
                        force_clearcoat_roughness_factor = True

    scalar_factor_and_texture(
        mh,
        location=locs['clearcoat_roughness'],
        label='Clearcoat Roughness',
        socket=pbr_node.inputs['Coat Roughness'],
        factor=ext.get('clearcoatRoughnessFactor', BLENDER_COAT_ROUGHNESS if ext.get(
            'clearcoatRoughnessTexture') is None else 0),
        tex_info=ext.get('clearcoatRoughnessTexture'),
        channel=1,  # Green
        force_mix_node=force_clearcoat_roughness_factor
    )

    if len(ext) > 0:
        tex_info = TextureInfo.from_dict(ext.get('clearcoatRoughnessTexture')) if ext.get(
            'clearcoatRoughnessTexture') is not None else None
        # Because extensions are dict, they are not passed by reference
        # So we need to update the dict of the KHR_texture_transform extension if needed
        if tex_info is not None and tex_info.extensions is not None and "KHR_texture_transform" in tex_info.extensions:
            mh.pymat.extensions['KHR_materials_clearcoat']['clearcoatRoughnessTexture']['extensions']['KHR_texture_transform'] = tex_info.extensions["KHR_texture_transform"]

    normal_map(
        mh,
        location=locs['clearcoat_normal'],
        label='Clearcoat Normal',
        socket=pbr_node.inputs['Coat Normal'],
        tex_info=ext.get('clearcoatNormalTexture'),
    )


def transmission(mh, locs, pbr_node):
    ext = mh.get_ext('KHR_materials_transmission', {})
    factor = ext.get('transmissionFactor', 0)

    if len(ext) > 0:
        # Needed for KHR_animation_pointer
        mh.pymat.extensions['KHR_materials_transmission']['blender_nodetree'] = mh.node_tree
        mh.pymat.extensions['KHR_materials_transmission']['blender_mat'] = mh.mat  # Needed for KHR_animation_pointer

    # We need transmission if animated by KHR_animation_pointer
    force_transmission = False
    if mh.gltf.data.extensions_used is not None and "KHR_animation_pointer" in mh.gltf.data.extensions_used:
        if mh.pymat.extensions and "KHR_materials_transmission" in mh.pymat.extensions and len(
                mh.pymat.extensions["KHR_materials_transmission"]["animations"]) > 0:
            for anim_idx in mh.pymat.extensions["KHR_materials_transmission"]["animations"].keys():
                for channel_idx in mh.pymat.extensions["KHR_materials_transmission"]["animations"][anim_idx]:
                    channel = mh.gltf.data.animations[anim_idx].channels[channel_idx]
                    pointer_tab = channel.target.extensions["KHR_animation_pointer"]["pointer"].split("/")
                    if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                            pointer_tab[3] == "extensions" and \
                            pointer_tab[4] == "KHR_materials_transmission" and \
                            pointer_tab[5] == "transmissionFactor":
                        force_transmission = True

    if factor > 0 or force_transmission is True:
        # Activate screen refraction (for Eevee)
        mh.mat.use_screen_refraction = True

    scalar_factor_and_texture(
        mh,
        location=locs['transmission'],
        label='Transmission',
        socket=pbr_node.inputs['Transmission Weight'],
        factor=factor,
        tex_info=ext.get('transmissionTexture'),
        channel=0,  # Red
        force_mix_node=force_transmission,
    )

    if len(ext) > 0:
        tex_info = TextureInfo.from_dict(ext.get('transmissionTexture')) if ext.get(
            'transmissionTexture') is not None else None
        # Because extensions are dict, they are not passed by reference
        # So we need to update the dict of the KHR_texture_transform extension if needed
        if tex_info is not None and tex_info.extensions is not None and "KHR_texture_transform" in tex_info.extensions:
            mh.pymat.extensions['KHR_materials_transmission']['transmissionTexture']['extensions']['KHR_texture_transform'] = tex_info.extensions["KHR_texture_transform"]


def volume(mh, location, volume_node, thickness_socket):
    # Based on https://github.com/KhronosGroup/glTF-Blender-IO/issues/1454#issuecomment-928319444
    ext = mh.get_ext('KHR_materials_volume', {})

    if len(ext) > 0:
        # Needed for KHR_animation_pointer
        mh.pymat.extensions['KHR_materials_volume']['blender_nodetree'] = mh.node_tree
        mh.pymat.extensions['KHR_materials_volume']['blender_mat'] = mh.mat  # Needed for KHR_animation_pointer

    color = ext.get('attenuationColor', [1, 1, 1])
    volume_node.inputs[0].default_value = [*color, 1]

    distance = ext.get('attenuationDistance', float('inf'))
    density = 1 / distance
    volume_node.inputs[1].default_value = density

    # We also need math node if thickness factor is animated in KHR_animation_pointer
    force_math_node = False
    if mh.gltf.data.extensions_used is not None and "KHR_animation_pointer" in mh.gltf.data.extensions_used:
        if len(mh.pymat.extensions["KHR_materials_volume"]["animations"]) > 0:
            for anim_idx in mh.pymat.extensions["KHR_materials_volume"]["animations"].keys():
                for channel_idx in mh.pymat.extensions["KHR_materials_volume"]["animations"][anim_idx]:
                    channel = mh.gltf.data.animations[anim_idx].channels[channel_idx]
                    pointer_tab = channel.target.extensions["KHR_animation_pointer"]["pointer"].split("/")
                    if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                            pointer_tab[3] == "extensions" and \
                            pointer_tab[4] == "KHR_materials_volume" and \
                            pointer_tab[5] == "thicknessFactor":
                        force_math_node = True

    scalar_factor_and_texture(
        mh,
        location=location,
        label='Thickness',
        socket=thickness_socket,
        factor=ext.get('thicknessFactor', 0),
        tex_info=ext.get('thicknessTexture'),
        channel=1,  # Green
        force_mix_node=force_math_node,
    )

    if len(ext) > 0:
        tex_info = TextureInfo.from_dict(ext.get('thicknessTexture')) if ext.get(
            'thicknessTexture') is not None else None
        # Because extensions are dict, they are not passed by reference
        # So we need to update the dict of the KHR_texture_transform extension if needed
        if tex_info is not None and tex_info.extensions is not None and "KHR_texture_transform" in tex_info.extensions:
            mh.pymat.extensions['KHR_materials_volume']['thicknessTexture']['extensions']['KHR_texture_transform'] = tex_info.extensions["KHR_texture_transform"]


def specular(mh, locs, pbr_node):
    ext = mh.get_ext('KHR_materials_specular', {})

    if len(ext) > 0:
        # Needed for KHR_animation_pointer
        mh.pymat.extensions['KHR_materials_specular']['blender_nodetree'] = mh.node_tree
        mh.pymat.extensions['KHR_materials_specular']['blender_mat'] = mh.mat  # Needed for KHR_animation_pointer

    # blender.IORLevel = 0.5 * gltf.specular
    scalar_factor_and_texture(
        mh,
        location=locs['specularTexture'],
        label='Specular',
        socket=pbr_node.inputs['Specular IOR Level'],
        factor=0.5 * ext.get('specularFactor', 1),
        tex_info=ext.get('specularTexture'),
        channel=4,  # Alpha
    )

    if len(ext) > 0:
        tex_info = TextureInfo.from_dict(ext.get('specularTexture')) if ext.get('specularTexture') is not None else None
        # Because extensions are dict, they are not passed by reference
        # So we need to update the dict of the KHR_texture_transform extension if needed
        if tex_info is not None and tex_info.extensions is not None and "KHR_texture_transform" in tex_info.extensions:
            mh.pymat.extensions['KHR_materials_specular']['specularTexture']['extensions']['KHR_texture_transform'] = tex_info.extensions["KHR_texture_transform"]

    color_factor_and_texture(
        mh,
        location=locs['specularColorTexture'],
        label='Specular Color',
        socket=pbr_node.inputs['Specular Tint'],
        factor=ext.get('specularColorFactor', [1, 1, 1]),
        tex_info=ext.get('specularColorTexture'),
    )

    if len(ext) > 0:
        tex_info = TextureInfo.from_dict(ext.get('specularColorTexture')) if ext.get(
            'specularColorTexture') is not None else None
        # Because extensions are dict, they are not passed by reference
        # So we need to update the dict of the KHR_texture_transform extension if needed
        if tex_info is not None and tex_info.extensions is not None and "KHR_texture_transform" in tex_info.extensions:
            mh.pymat.extensions['KHR_materials_specular']['specularColorTexture']['extensions']['KHR_texture_transform'] = tex_info.extensions["KHR_texture_transform"]


def sheen(mh, locs, pbr_node):
    ext = mh.get_ext('KHR_materials_sheen')
    if ext is None:
        return

    mh.pymat.extensions['KHR_materials_sheen']['blender_nodetree'] = mh.node_tree  # Needed for KHR_animation_pointer
    mh.pymat.extensions['KHR_materials_sheen']['blender_mat'] = mh.mat  # Needed for KHR_animation_pointer

    pbr_node.inputs['Sheen Weight'].default_value = 1

    color_factor_and_texture(
        mh,
        location=locs['sheenColorTexture'],
        label='Sheen Color',
        socket=pbr_node.inputs['Sheen Tint'],
        factor=ext.get('sheenColorFactor', [0, 0, 0]),
        tex_info=ext.get('sheenColorTexture'),
    )

    if len(ext) > 0:
        tex_info = TextureInfo.from_dict(ext.get('sheenColorTexture')) if ext.get(
            'sheenColorTexture') is not None else None
        # Because extensions are dict, they are not passed by reference
        # So we need to update the dict of the KHR_texture_transform extension if needed
        if tex_info is not None and tex_info.extensions is not None and "KHR_texture_transform" in tex_info.extensions:
            mh.pymat.extensions['KHR_materials_sheen']['sheenColorTexture']['extensions']['KHR_texture_transform'] = tex_info.extensions["KHR_texture_transform"]

    scalar_factor_and_texture(
        mh,
        location=locs['sheenRoughnessTexture'],
        label='Sheen Roughness',
        socket=pbr_node.inputs['Sheen Roughness'],
        factor=ext.get('sheenRoughnessFactor', 0),
        tex_info=ext.get('sheenRoughnessTexture'),
        channel=4,  # Alpha
    )

    if len(ext) > 0:
        tex_info = TextureInfo.from_dict(ext.get('sheenRoughnessTexture')) if ext.get(
            'sheenRoughnessTexture') is not None else None
        # Because extensions are dict, they are not passed by reference
        # So we need to update the dict of the KHR_texture_transform extension if needed
        if tex_info is not None and tex_info.extensions is not None and "KHR_texture_transform" in tex_info.extensions:
            mh.pymat.extensions['KHR_materials_sheen']['sheenRoughnessTexture']['extensions']['KHR_texture_transform'] = tex_info.extensions["KHR_texture_transform"]


def calc_locations(mh):
    """Calculate locations to place each bit of the node graph at."""
    # Lay the blocks out top-to-bottom, aligned on the right
    x = -200
    y = 0
    height = 460  # height of each block
    locs = {}

    clearcoat_ext = mh.get_ext('KHR_materials_clearcoat', {})
    transmission_ext = mh.get_ext('KHR_materials_transmission', {})
    volume_ext = mh.get_ext('KHR_materials_volume', {})
    specular_ext = mh.get_ext('KHR_materials_specular', {})
    anisotropy_ext = mh.get_ext('KHR_materials_anisotropy', {})
    sheen_ext = mh.get_ext('KHR_materials_sheen', {})

    locs['base_color'] = (x, y)
    if mh.pymat.pbr_metallic_roughness.base_color_texture is not None or mh.vertex_color:
        y -= height
    locs['metallic_roughness'] = (x, y)
    if mh.pymat.pbr_metallic_roughness.metallic_roughness_texture is not None:
        y -= height
    locs['transmission'] = (x, y)
    if 'transmissionTexture' in transmission_ext:
        y -= height
    locs['normal'] = (x, y)
    if mh.pymat.normal_texture is not None:
        y -= height
    locs['specularTexture'] = (x, y)
    if 'specularTexture' in specular_ext:
        y -= height
    locs['specularColorTexture'] = (x, y)
    if 'specularColorTexture' in specular_ext:
        y -= height
    locs['anisotropy'] = (x, y)
    if 'anisotropyTexture' in anisotropy_ext:
        y -= height
    locs['sheenRoughnessTexture'] = (x, y)
    if 'sheenRoughnessTexture' in sheen_ext:
        y -= height
    locs['sheenColorTexture'] = (x, y)
    if 'sheenColorTexture' in sheen_ext:
        y -= height
    locs['clearcoat'] = (x, y)
    if 'clearcoatTexture' in clearcoat_ext:
        y -= height
    locs['clearcoat_roughness'] = (x, y)
    if 'clearcoatRoughnessTexture' in clearcoat_ext:
        y -= height
    locs['clearcoat_normal'] = (x, y)
    if 'clearcoatNormalTexture' in clearcoat_ext:
        y -= height
    locs['emission'] = (x, y)
    if mh.pymat.emissive_texture is not None:
        y -= height
    locs['occlusion'] = (x, y)
    if mh.pymat.occlusion_texture is not None:
        y -= height
    locs['volume_thickness'] = (x, y)
    if 'thicknessTexture' in volume_ext:
        y -= height

    # Center things
    total_height = -y
    y_offset = total_height / 2 - 20
    for key in locs:
        x, y = locs[key]
        locs[key] = (x, y + y_offset)

    return locs


# These functions each create one piece of the node graph, slotting
# their outputs into the given socket, or setting its default value.
# location is roughly the upper-right corner of where to put nodes.


# [Texture] => [Emissive Factor] =>
def emission(mh: MaterialHelper, location, color_socket, strength_socket):
    factor = mh.pymat.emissive_factor or [0, 0, 0]
    ext = mh.get_ext('KHR_materials_emissive_strength', {})
    strength = ext.get('emissiveStrength', 1)
    if len(ext) > 0:
        # Needed for KHR_animation_pointer
        mh.pymat.extensions['KHR_materials_emissive_strength']['blender_nodetree'] = mh.node_tree
        # Needed for KHR_animation_pointer
        mh.pymat.extensions['KHR_materials_emissive_strength']['blender_mat'] = mh.mat

    if factor[0] == factor[1] == factor[2]:
        # Fold greyscale factor into strength
        strength *= factor[0]
        factor = [1, 1, 1]

    # We need to check if emissive factor is animated via KHR_animation_pointer
    # Because if not, we can use direct socket or mix node, depending if there
    # is a texture or not, or if factor is grayscale
    force_mix_node = False
    if mh.gltf.data.extensions_used is not None and "KHR_animation_pointer" in mh.gltf.data.extensions_used:
        if len(mh.pymat.animations) > 0:
            for anim_idx in mh.pymat.animations.keys():
                for channel_idx in mh.pymat.animations[anim_idx]:
                    channel = mh.gltf.data.animations[anim_idx].channels[channel_idx]
                    pointer_tab = channel.target.extensions["KHR_animation_pointer"]["pointer"].split("/")
                    if len(pointer_tab) == 4 and pointer_tab[1] == "materials" and \
                            pointer_tab[3] == "emissiveFactor":
                        force_mix_node = True

    color_factor_and_texture(
        mh,
        location,
        label='Emissive',
        socket=color_socket,
        factor=factor,
        tex_info=mh.pymat.emissive_texture,
        force_mix_node=force_mix_node,
    )
    strength_socket.default_value = strength


#      [Texture] => [Mix Colors] => [Color Factor] =>
# [Vertex Color] => [Mix Alphas] => [Alpha Factor] =>
def base_color(
    mh: MaterialHelper,
    location,
    color_socket,
    alpha_socket=None,
    is_diffuse=False,
):
    """Handle base color (= baseColorTexture * vertexColor * baseColorFactor)."""
    x, y = location
    pbr = mh.pymat.pbr_metallic_roughness
    if not is_diffuse:
        base_color_factor = pbr.base_color_factor
        base_color_texture = pbr.base_color_texture
    else:
        # Handle pbrSpecularGlossiness's diffuse with this function too,
        # since it's almost exactly the same as base color.
        base_color_factor = \
            mh.pymat.extensions['KHR_materials_pbrSpecularGlossiness'] \
            .get('diffuseFactor', [1, 1, 1, 1])
        base_color_texture = \
            mh.pymat.extensions['KHR_materials_pbrSpecularGlossiness'] \
            .get('diffuseTexture', None)
        if base_color_texture is not None:
            base_color_texture = TextureInfo.from_dict(base_color_texture)

    if base_color_factor is None:
        base_color_factor = [1, 1, 1, 1]

    if base_color_texture is None and not mh.vertex_color:
        color_socket.default_value = base_color_factor[:3] + [1]
        if alpha_socket is not None:
            alpha_socket.default_value = base_color_factor[3]
        return

    # Mix in base color factor
    needs_color_factor = base_color_factor[:3] != [1, 1, 1]
    needs_alpha_factor = base_color_factor[3] != 1.0 and alpha_socket is not None

    # We need to check if base color factor is animated via KHR_animation_pointer
    # Because if not, we can use direct socket or mix node, depending if there is a texture or not
    # If there is an animation, we need to force creation of a mix node and math node, for color and alpha
    if mh.gltf.data.extensions_used is not None and "KHR_animation_pointer" in mh.gltf.data.extensions_used:
        if len(mh.pymat.pbr_metallic_roughness.animations) > 0:
            for anim_idx in mh.pymat.pbr_metallic_roughness.animations.keys():
                for channel_idx in mh.pymat.pbr_metallic_roughness.animations[anim_idx]:
                    channel = mh.gltf.data.animations[anim_idx].channels[channel_idx]
                    pointer_tab = channel.target.extensions["KHR_animation_pointer"]["pointer"].split("/")
                    if len(pointer_tab) == 5 and pointer_tab[1] == "materials" and \
                            pointer_tab[3] == "pbrMetallicRoughness" and \
                            pointer_tab[4] == "baseColorFactor":
                        needs_color_factor = True
                        needs_alpha_factor = True if alpha_socket is not None else False

    if needs_color_factor or needs_alpha_factor:
        if needs_color_factor:
            node = mh.node_tree.nodes.new('ShaderNodeMix')
            node.label = 'Color Factor'
            node.data_type = "RGBA"
            node.location = x - 140, y
            node.blend_type = 'MULTIPLY'
            # Outputs
            mh.node_tree.links.new(color_socket, node.outputs[2])
            # Inputs
            node.inputs['Factor'].default_value = 1.0
            color_socket = node.inputs[6]
            node.inputs[7].default_value = base_color_factor[:3] + [1]

        if needs_alpha_factor:
            node = mh.node_tree.nodes.new('ShaderNodeMath')
            node.label = 'Alpha Factor'
            node.location = x - 140, y - 230
            # Outputs
            mh.node_tree.links.new(alpha_socket, node.outputs[0])
            # Inputs
            node.operation = 'MULTIPLY'
            alpha_socket = node.inputs[0]
            node.inputs[1].default_value = base_color_factor[3]

        x -= 200

    # These are where the texture/vertex color node will put its output.
    texture_color_socket = color_socket
    texture_alpha_socket = alpha_socket
    vcolor_color_socket = color_socket
    vcolor_alpha_socket = alpha_socket

    # Mix texture and vertex color together
    if base_color_texture is not None and mh.vertex_color:
        node = mh.node_tree.nodes.new('ShaderNodeMix')
        node.label = 'Mix Vertex Color'
        node.data_type = 'RGBA'
        node.location = x - 140, y
        node.blend_type = 'MULTIPLY'
        # Outputs
        mh.node_tree.links.new(color_socket, node.outputs[2])
        # Inputs
        node.inputs['Factor'].default_value = 1.0
        texture_color_socket = node.inputs[6]
        vcolor_color_socket = node.inputs[7]

        if alpha_socket is not None:
            node = mh.node_tree.nodes.new('ShaderNodeMath')
            node.label = 'Mix Vertex Alpha'
            node.location = x - 140, y - 230
            node.operation = 'MULTIPLY'
            # Outputs
            mh.node_tree.links.new(alpha_socket, node.outputs[0])
            # Inputs
            texture_alpha_socket = node.inputs[0]
            vcolor_alpha_socket = node.inputs[1]

        x -= 200

    # Vertex Color
    if mh.vertex_color:
        node = mh.node_tree.nodes.new('ShaderNodeVertexColor')
        # Do not set the layer name, so rendered one will be used (At import => The first one)
        node.location = x - 250, y - 240
        # Outputs
        mh.node_tree.links.new(vcolor_color_socket, node.outputs['Color'])
        if vcolor_alpha_socket is not None:
            mh.node_tree.links.new(vcolor_alpha_socket, node.outputs['Alpha'])

        x -= 280

    # Texture
    if base_color_texture is not None:
        texture(
            mh,
            tex_info=base_color_texture,
            label='BASE COLOR' if not is_diffuse else 'DIFFUSE',
            location=(x, y),
            color_socket=texture_color_socket,
            alpha_socket=texture_alpha_socket,
        )


# [Texture] => [Separate GB] => [Metal/Rough Factor] =>
def metallic_roughness(mh: MaterialHelper, location, metallic_socket, roughness_socket):
    x, y = location
    pbr = mh.pymat.pbr_metallic_roughness
    metal_factor = pbr.metallic_factor
    rough_factor = pbr.roughness_factor
    if metal_factor is None:
        metal_factor = 1.0
    if rough_factor is None:
        rough_factor = 1.0

    if pbr.metallic_roughness_texture is None:
        metallic_socket.default_value = metal_factor
        roughness_socket.default_value = rough_factor
        return

    need_metal_factor = metal_factor != 1.0
    need_rough_factor = rough_factor != 1.0

    # We need to check if factor is animated via KHR_animation_pointer
    # Because if not, we can use direct socket or mix node, depending if there is a texture or not
    # If there is an animation, we need to force creation of a mix node and math node, for metal or rough
    if mh.gltf.data.extensions_used is not None and "KHR_animation_pointer" in mh.gltf.data.extensions_used:
        if len(mh.pymat.pbr_metallic_roughness.animations) > 0:
            for anim_idx in mh.pymat.pbr_metallic_roughness.animations.keys():
                for channel_idx in mh.pymat.pbr_metallic_roughness.animations[anim_idx]:
                    channel = mh.gltf.data.pbr_metallic_roughness.animations[anim_idx].channels[channel_idx]
                    pointer_tab = channel.target.extensions["KHR_animation_pointer"]["pointer"].split("/")
                    if len(pointer_tab) == 5 and pointer_tab[1] == "materials" and \
                            pointer_tab[3] == "pbrMetallicRoughness" and \
                            pointer_tab[4] == "roughnessFactor":
                        need_rough_factor = True
                    if len(pointer_tab) == 5 and pointer_tab[1] == "materials" and \
                            pointer_tab[3] == "pbrMetallicRoughness" and \
                            pointer_tab[4] == "metallicFactor":
                        need_metal_factor = True

    if need_metal_factor or need_rough_factor:
        # Mix metal factor
        if need_metal_factor:
            node = mh.node_tree.nodes.new('ShaderNodeMath')
            node.label = 'Metallic Factor'
            node.location = x - 140, y
            node.operation = 'MULTIPLY'
            # Outputs
            mh.node_tree.links.new(metallic_socket, node.outputs[0])
            # Inputs
            metallic_socket = node.inputs[0]
            node.inputs[1].default_value = metal_factor

        # Mix rough factor
        if need_rough_factor:
            node = mh.node_tree.nodes.new('ShaderNodeMath')
            node.label = 'Roughness Factor'
            node.location = x - 140, y - 200
            node.operation = 'MULTIPLY'
            # Outputs
            mh.node_tree.links.new(roughness_socket, node.outputs[0])
            # Inputs
            roughness_socket = node.inputs[0]
            node.inputs[1].default_value = rough_factor

        x -= 200

    # Separate RGB
    node = mh.node_tree.nodes.new('ShaderNodeSeparateColor')
    node.location = x - 150, y - 75
    # Outputs
    mh.node_tree.links.new(metallic_socket, node.outputs['Blue'])
    mh.node_tree.links.new(roughness_socket, node.outputs['Green'])
    # Inputs
    color_socket = node.inputs[0]

    x -= 200

    texture(
        mh,
        tex_info=pbr.metallic_roughness_texture,
        label='METALLIC ROUGHNESS',
        location=(x, y),
        is_data=True,
        color_socket=color_socket,
    )


# [Texture] => [Normal Map] =>
def normal(mh: MaterialHelper, location, normal_socket):
    tex_info = mh.pymat.normal_texture
    if tex_info is not None:
        tex_info.blender_nodetree = mh.mat.node_tree  # Used in case of for KHR_animation_pointer
        tex_info.blender_mat = mh.mat  # Used in case of for KHR_animation_pointer #TODOPointer Vertex Color...

    normal_map(
        mh,
        location=location,
        label='Normal Map',
        socket=normal_socket,
        tex_info=tex_info,
    )


# [Texture] => [Separate R] => [Mix Strength] =>
def occlusion(mh: MaterialHelper, location, occlusion_socket):
    x, y = location

    if mh.pymat.occlusion_texture is None:
        return

    strength = mh.pymat.occlusion_texture.strength
    if strength is None:
        strength = 1.0

    strength_needed = strength != 1.0

    # We need to check if occlusion strength is animated via KHR_animation_pointer
    # Because if not, we can use direct socket or mix node, depending if there is a texture or not
    # If there is an animation, we need to force creation of a mix node and math node, for strength
    if mh.gltf.data.extensions_used is not None and "KHR_animation_pointer" in mh.gltf.data.extensions_used:
        if len(mh.pymat.occlusion_texture.animations) > 0:
            for anim_idx in mh.pymat.occlusion_texture.animations.keys():
                for channel_idx in mh.pymat.occlusion_texture.animations[anim_idx]:
                    channel = mh.gltf.data.animations[anim_idx].channels[channel_idx]
                    pointer_tab = channel.target.extensions["KHR_animation_pointer"]["pointer"].split("/")
                    if len(pointer_tab) == 5 and pointer_tab[1] == "materials" and \
                            pointer_tab[3] == "occlusionTexture" and \
                            pointer_tab[4] == "strength":
                        strength_needed = True

    if strength_needed:
        # Mix with white
        node = mh.node_tree.nodes.new('ShaderNodeMix')
        node.label = 'Occlusion Strength'
        node.data_type = 'RGBA'
        node.location = x - 140, y
        node.blend_type = 'MIX'
        # Outputs
        mh.node_tree.links.new(occlusion_socket, node.outputs[2])
        # Inputs
        node.inputs['Factor'].default_value = strength
        node.inputs[6].default_value = [1, 1, 1, 1]
        occlusion_socket = node.inputs[7]

        x -= 200

    # Separate RGB
    node = mh.node_tree.nodes.new('ShaderNodeSeparateColor')
    node.location = x - 150, y - 75
    # Outputs
    mh.node_tree.links.new(occlusion_socket, node.outputs['Red'])
    # Inputs
    color_socket = node.inputs[0]

    x -= 200

    mh.pymat.occlusion_texture.blender_nodetree = mh.mat.node_tree  # Used in case of for KHR_animation_pointer
    # Used in case of for KHR_animation_pointer #TODOPointer Vertex Color...
    mh.pymat.occlusion_texture.blender_mat = mh.mat

    texture(
        mh,
        tex_info=mh.pymat.occlusion_texture,
        label='OCCLUSION',
        location=(x, y),
        is_data=True,
        color_socket=color_socket,
    )


def make_settings_node(mh):
    """
    Make a Group node with a hookup for Occlusion. No effect in Blender, but
    used to tell the exporter what the occlusion map should be.
    """
    node = mh.node_tree.nodes.new('ShaderNodeGroup')
    node.node_tree = get_settings_group()
    return node


def get_settings_group():
    gltf_node_group_name = get_gltf_node_name()
    if gltf_node_group_name in bpy.data.node_groups:
        gltf_node_group = bpy.data.node_groups[gltf_node_group_name]
    else:
        # Create a new node group
        gltf_node_group = create_settings_group(gltf_node_group_name)
    return gltf_node_group
