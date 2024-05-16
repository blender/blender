# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from ...io.com.gltf2_io import TextureInfo
from .gltf2_blender_pbrMetallicRoughness import \
    base_color, emission, normal, occlusion, make_settings_node
from .gltf2_blender_material_utils import color_factor_and_texture
from .gltf2_blender_texture import texture, get_source
from .gltf2_blender_image import BlenderImage
import numpy as np


def pbr_specular_glossiness(mh):
    """Creates node tree for pbrSpecularGlossiness materials."""
    ext = mh.get_ext('KHR_materials_pbrSpecularGlossiness', {})

    pbr_node = mh.nodes.new('ShaderNodeBsdfPrincipled')
    out_node = mh.nodes.new('ShaderNodeOutputMaterial')
    pbr_node.location = 10, 300
    out_node.location = 300, 300
    mh.links.new(pbr_node.outputs[0], out_node.inputs[0])

    locs = calc_locations(mh, ext)

    base_color(
        mh,
        is_diffuse=True,
        location=locs['diffuse'],
        color_socket=pbr_node.inputs['Base Color'],
        alpha_socket=pbr_node.inputs['Alpha'] if not mh.is_opaque() else None,
    )

    emission(
        mh,
        location=locs['emission'],
        color_socket=pbr_node.inputs['Emission Color'],
        strength_socket=pbr_node.inputs['Emission Strength'],
    )

    normal(
        mh,
        location=locs['normal'],
        normal_socket=pbr_node.inputs['Normal'],
    )

    if mh.pymat.occlusion_texture is not None:
        if mh.settings_node is None:
            mh.settings_node = make_settings_node(mh)
            mh.settings_node.location = 10, 425
            mh.settings_node.width = 240
        occlusion(
            mh,
            location=locs['occlusion'],
            occlusion_socket=mh.settings_node.inputs['Occlusion'],
        )

    # The F0 color is the specular tint modulated by
    # ((1-IOR)/(1+IOR))^2. Setting IOR=1000 makes this factor
    # approximately 1.
    pbr_node.inputs['IOR'].default_value = 1000

    # Specular
    color_factor_and_texture(
        mh,
        location=locs['specular'],
        label='Specular Color',
        socket=pbr_node.inputs['Specular Tint'],
        factor=ext.get('specularFactor', [1, 1, 1]),
        tex_info=ext.get('specularGlossinessTexture'),
    )

    # Glossiness
    glossiness(
        mh,
        ext,
        location=locs['glossiness'],
        roughness_socket=pbr_node.inputs['Roughness'],
    )


def glossiness(mh, ext, location, roughness_socket):
    # Glossiness = glossinessFactor * specularGlossinessTexture.alpha
    # Roughness = 1 - Glossiness

    factor = ext.get('glossinessFactor', 1)
    tex_info = ext.get('specularGlossinessTexture')
    if tex_info is not None:
        tex_info = TextureInfo.from_dict(tex_info)

    # Simple case: no texture
    if tex_info is None or factor == 0:
        roughness_socket.default_value = 1 - factor
        return

    # Bake an image with the roughness. The reason we don't do
    # 1-X with a node is that won't export.
    roughness_img = make_roughness_image(mh, factor, tex_info)
    if roughness_img is None:
        return

    texture(
        mh,
        tex_info,
        location=location,
        label='ROUGHNESS',
        color_socket=None,
        alpha_socket=roughness_socket,
        is_data=False,
        forced_image=roughness_img,
    )


def make_roughness_image(mh, glossiness_factor, tex_info):
    """
    Bakes the roughness (1-glossiness) into an image. The
    roughness is in the alpha channel.
    """
    pytexture = mh.gltf.data.textures[tex_info.index]
    source = get_source(mh, pytexture)

    if source is None:
        return None

    pyimg = mh.gltf.data.images[source]
    BlenderImage.create(mh.gltf, source)

    # See if cached roughness texture already exists
    if hasattr(pyimg, 'blender_roughness_image_name'):
        return bpy.data.images[pyimg.blender_roughness_image_name]

    orig_image = bpy.data.images[pyimg.blender_image_name]
    # TODO: check for placeholder image and bail

    # Make a copy of the specularGlossiness texture
    # Avoids interfering if it's used elsewhere
    image = orig_image.copy()

    w, h = image.size
    pixels = np.empty(w * h * 4, dtype=np.float32)
    image.pixels.foreach_get(pixels)
    pixels = pixels.reshape((w, h, 4))

    # Glossiness = GlossinessFactor * Texture.alpha
    # Roughness = 1 - Glossiness
    if glossiness_factor != 1:
        pixels[:, :, 3] *= glossiness_factor
    pixels[:, :, 3] *= -1
    pixels[:, :, 3] += 1

    pixels = pixels.reshape(w * h * 4)
    image.pixels.foreach_set(pixels)

    image.pack()

    # Cache for reuse
    pyimg.blender_roughness_image_name = image.name

    return image


def calc_locations(mh, ext):
    """Calculate locations to place each bit of the node graph at."""
    # Lay the blocks out top-to-bottom, aligned on the right
    x = -200
    y = 0
    height = 460  # height of each block
    locs = {}

    locs['occlusion'] = (x, y)
    if mh.pymat.occlusion_texture is not None:
        y -= height

    locs['diffuse'] = (x, y)
    if 'diffuseTexture' in ext or mh.vertex_color:
        y -= height

    locs['glossiness'] = (x, y)
    gloss_factor = ext.get('glossinessFactor', 1)
    if 'specularGlossinessTexture' in ext and gloss_factor != 0:
        y -= height

    locs['normal'] = (x, y)
    if mh.pymat.normal_texture is not None:
        y -= height

    locs['specular'] = (x, y)
    if 'specularGlossinessTexture' in ext:
        y -= height

    locs['emission'] = (x, y)
    if mh.pymat.emissive_texture is not None:
        y -= height

    # Center things
    total_height = -y
    y_offset = total_height / 2 - 20
    for key in locs:
        x, y = locs[key]
        locs[key] = (x, y + y_offset)

    return locs
