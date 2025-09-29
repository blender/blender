# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from copy import deepcopy
import bpy

from ....io.com import gltf2_io
from ....io.com.gltf2_io_extensions import Extension
from ....io.exp.user_extensions import export_user_extensions
from ..cache import cached, cached_by_key
from . import unlit as gltf2_unlit
from . import texture_info as gltf2_blender_gather_texture_info
from . import pbr_metallic_roughness as gltf2_pbr_metallic_roughness
from .material_utils import gather_extras, gather_name
from .material_viewport import export_viewport_material
from .extensions.volume import export_volume
from .extensions.emission import export_emission_factor, \
    export_emission_texture, export_emission_strength_extension
from .extensions.sheen import export_sheen
from .extensions.specular import export_specular
from .extensions.transmission import export_transmission
from .extensions.clearcoat import export_clearcoat
from .extensions.anisotropy import export_anisotropy
from .extensions.ior import export_ior
from .search_node_tree import \
    has_image_node_from_socket, \
    get_socket_from_gltf_material_node, \
    get_socket, \
    get_node_socket, \
    get_material_nodes, \
    NodeSocket, \
    gather_alpha_info


@cached
def get_material_cache_key(blender_material, export_settings):
    # Use id of material
    # Do not use bpy.types that can be unhashable
    # Do not use material name, that can be not unique (when linked)
    # We use here the id of original material as for apply modifier, the material has a new id
    # So, when no modifier applied => original is the same id
    # And when modifier applied => new one is different id, but original is still the same
    return (
        (id(blender_material.original),),
    )


@cached_by_key(key=get_material_cache_key)
def gather_material(blender_material, export_settings):
    """
    Gather the material used by the blender primitive.

    :param blender_material: the blender material used in the glTF primitive
    :param export_settings:
    :return: a glTF material
    """
    if not __filter_material(blender_material, export_settings):
        return None, {"uv_info": {}, "vc_info": {'color': None, 'alpha': None,
                                                 'color_type': None, 'alpha_type': None, 'alpha_mode': "OPAQUE"}, "udim_info": {}}


    if export_settings['gltf_materials'] == "VIEWPORT":
        return export_viewport_material(blender_material, export_settings), {"uv_info": {}, "vc_info": {'color': None, 'alpha': None,
                                                 'color_type': None, 'alpha_type': None, 'alpha_mode': "OPAQUE"}, "udim_info": {}}

    nodes_used = export_settings['nodes_used'] = {}

    # Reset exported images / textures nodes
    export_settings['exported_texture_nodes'] = []

    mat_unlit, uvmap_info, vc_info, udim_info = __export_unlit(blender_material, export_settings)
    if mat_unlit is not None:
        export_user_extensions('gather_material_hook', export_settings, mat_unlit, blender_material)
        return mat_unlit, {"uv_info": uvmap_info, "vc_info": vc_info, "udim_info": udim_info}

    orm_texture = __gather_orm_texture(blender_material, export_settings)

    emissive_factor = __gather_emissive_factor(blender_material, export_settings)
    emissive_texture, uvmap_info_emissive, udim_info_emissive = __gather_emissive_texture(
        blender_material, export_settings)
    extensions, uvmap_info_extensions, udim_info_extensions = __gather_extensions(
        blender_material, emissive_factor, export_settings)
    normal_texture, uvmap_info_normal, udim_info_normal = __gather_normal_texture(blender_material, export_settings)
    occlusion_texture, uvmap_info_occlusion, udim_occlusion = __gather_occlusion_texture(
        blender_material, orm_texture, export_settings)
    pbr_metallic_roughness, uvmap_info_pbr_metallic_roughness, vc_info, udim_info_prb_mr = __gather_pbr_metallic_roughness(
        blender_material, orm_texture, export_settings)

    if any([i > 1.0 for i in emissive_factor or []]) is True:
        # Strength is set on extension
        emission_strength = max(emissive_factor)
        emissive_factor = [f / emission_strength for f in emissive_factor]

    alpha_socket = get_socket(blender_material.node_tree, "Alpha")
    if isinstance(alpha_socket.socket, bpy.types.NodeSocket):
        alpha_info = gather_alpha_info(alpha_socket.to_node_nav())
    else:
        alpha_info = gather_alpha_info(None)

    material = gltf2_io.Material(
        alpha_cutoff=__gather_alpha_cutoff(alpha_info, export_settings),
        alpha_mode=__gather_alpha_mode(alpha_info, export_settings),
        double_sided=__gather_double_sided(blender_material, extensions, export_settings),
        emissive_factor=emissive_factor,
        emissive_texture=emissive_texture,
        extensions=extensions,
        extras=gather_extras(blender_material, export_settings),
        name=gather_name(blender_material, export_settings),
        normal_texture=normal_texture,
        occlusion_texture=occlusion_texture,
        pbr_metallic_roughness=pbr_metallic_roughness
    )

    uvmap_infos = {}
    udim_infos = {}

    # Get all textures nodes that are not used in the material
    if export_settings['gltf_unused_textures'] is True:
        if blender_material.node_tree:
            nodes = get_material_nodes(
                blender_material.node_tree, [
                    blender_material.node_tree], bpy.types.ShaderNodeTexImage)
        else:
            nodes = []
        cpt_additional = 0
        for node in nodes:
            if nodes_used.get(node[0].name):
                continue

            s = NodeSocket(node[0].outputs[0], node[1])
            tex, uv_info_additional, udim_info, _ = gltf2_blender_gather_texture_info.gather_texture_info(
                s, (s,), export_settings)
            if tex is not None:
                export_settings['exported_images'][node[0].image.name] = 1  # Fully used
                uvmap_infos.update({'additional' + str(cpt_additional): uv_info_additional})
                udim_infos.update({'additional' + str(cpt_additional): udim_info})
                cpt_additional += 1
                export_settings['additional_texture_export'].append(tex)

    export_settings.pop('nodes_used')

    uvmap_infos.update(uvmap_info_emissive)
    uvmap_infos.update(uvmap_info_extensions)
    uvmap_infos.update(uvmap_info_normal)
    uvmap_infos.update(uvmap_info_occlusion)
    uvmap_infos.update(uvmap_info_pbr_metallic_roughness)

    udim_infos = {}
    udim_infos.update(udim_info_prb_mr)
    udim_infos.update(udim_info_normal)
    udim_infos.update(udim_info_emissive)
    udim_infos.update(udim_occlusion)
    udim_infos.update(udim_info_extensions)

    # If emissive is set, from an emissive node (not PBR)
    # We need to set manually default values for
    # pbr_metallic_roughness.baseColor
    if material.emissive_factor is not None and get_node_socket(
            blender_material.node_tree,
            bpy.types.ShaderNodeBsdfPrincipled,
            "Base Color").socket is None:
        material.pbr_metallic_roughness = gltf2_pbr_metallic_roughness.get_default_pbr_for_emissive_node()

    export_user_extensions('gather_material_hook', export_settings, material, blender_material)

    # Now we have exported the material itself, we need to store some additional data
    # This will be used when trying to export some KHR_animation_pointer

    if len(export_settings['current_paths']) > 0:
        export_settings['KHR_animation_pointer']['materials'][id(blender_material)] = {}
        export_settings['KHR_animation_pointer']['materials'][id(
            blender_material)]['paths'] = export_settings['current_paths'].copy()

    export_settings['current_paths'] = {}

    return material, {"uv_info": uvmap_infos, "vc_info": vc_info, "udim_info": udim_infos}


def get_new_material_texture_shared(base, node):
    if node is None:
        return
    if callable(node) is True:
        return
    if node.__str__().startswith('__'):
        return
    if type(node) in [gltf2_io.TextureInfo,
                      gltf2_io.MaterialOcclusionTextureInfoClass,
                      gltf2_io.MaterialNormalTextureInfoClass]:
        node.index = base.index
    else:
        if hasattr(node, '__dict__'):
            for attr, value in node.__dict__.items():
                get_new_material_texture_shared(getattr(base, attr), value)
        else:
            # For extensions (on a dict)
            if type(node).__name__ == 'dict':
                for i in node.keys():
                    get_new_material_texture_shared(base[i], node[i])


def __filter_material(blender_material, export_settings):
    return export_settings['gltf_materials']


def __gather_alpha_cutoff(alpha_info, export_settings):
    if alpha_info['alphaMode'] == 'MASK':
        cutoff = alpha_info['alphaCutoff']

        path_ = {}
        path_['length'] = 1
        path_['path'] = "/materials/XXX/alphaCutoff"
        export_settings['current_paths']['alpha_threshold'] = path_

        return None if cutoff == 0.5 else cutoff
    return None


def __gather_alpha_mode(alpha_info, export_settings):
    mode = alpha_info['alphaMode']
    return None if mode == 'OPAQUE' else mode


def __gather_double_sided(blender_material, extensions, export_settings):

    # If user create a volume extension, we force double sided to False
    if 'KHR_materials_volume' in extensions:
        return False

    if not blender_material.use_backface_culling:
        return True
    return None


def __gather_emissive_factor(blender_material, export_settings):
    return export_emission_factor(blender_material, export_settings)


def __gather_emissive_texture(blender_material, export_settings):
    return export_emission_texture(blender_material, export_settings)


def __gather_extensions(blender_material, emissive_factor, export_settings):
    extensions = {}

    uvmap_infos = {}
    udim_infos = {}

    # KHR_materials_clearcoat
    clearcoat_extension, uvmap_info, udim_info_clearcoat = export_clearcoat(blender_material, export_settings)
    if clearcoat_extension:
        extensions["KHR_materials_clearcoat"] = clearcoat_extension
        uvmap_infos.update(uvmap_info)
        udim_infos.update(udim_info_clearcoat)

    # KHR_materials_transmission

    transmission_extension, uvmap_info, udim_info_transmission = export_transmission(blender_material, export_settings)
    if transmission_extension:
        extensions["KHR_materials_transmission"] = transmission_extension
        uvmap_infos.update(uvmap_info)
        udim_infos.update(udim_info_transmission)

    # KHR_materials_emissive_strength
    emissive_strength_extension = export_emission_strength_extension(emissive_factor, export_settings)
    if emissive_strength_extension:
        extensions["KHR_materials_emissive_strength"] = emissive_strength_extension

    # KHR_materials_volume

    volume_extension, uvmap_info, udim_info = export_volume(blender_material, export_settings)
    if volume_extension:
        extensions["KHR_materials_volume"] = volume_extension
        uvmap_infos.update(uvmap_info)
        udim_infos.update(udim_info)

    # KHR_materials_specular
    specular_extension, uvmap_info, udim_info = export_specular(blender_material, export_settings)
    if specular_extension:
        extensions["KHR_materials_specular"] = specular_extension
        uvmap_infos.update(uvmap_info)
        udim_infos.update(udim_info)

    # KHR_materials_sheen
    sheen_extension, uvmap_info, udim_info = export_sheen(blender_material, export_settings)
    if sheen_extension:
        extensions["KHR_materials_sheen"] = sheen_extension
        uvmap_infos.update(uvmap_info)
        udim_infos.update(udim_info)

    # KHR_materials_anisotropy
    anisotropy_extension, uvmap_info, udim_info = export_anisotropy(blender_material, export_settings)
    if anisotropy_extension:
        extensions["KHR_materials_anisotropy"] = anisotropy_extension
        uvmap_infos.update(uvmap_info)
        udim_infos.update(udim_info)

    # KHR_materials_ior
    # Keep this extension at the end, because we export it only if some others are exported
    ior_extension = export_ior(blender_material, extensions, export_settings)
    if ior_extension:
        extensions["KHR_materials_ior"] = ior_extension

    return extensions, uvmap_infos, udim_infos


def __gather_normal_texture(blender_material, export_settings):
    normal = get_socket(blender_material.node_tree, "Normal")
    normal_texture, uvmap_info, udim_info, _ = gltf2_blender_gather_texture_info.gather_material_normal_texture_info_class(
        normal, (normal,), export_settings)

    if len(export_settings['current_texture_transform']) != 0:
        for k in export_settings['current_texture_transform'].keys():
            path_ = {}
            path_['length'] = export_settings['current_texture_transform'][k]['length']
            path_['path'] = export_settings['current_texture_transform'][k]['path'].replace(
                "YYY", "normalTexture/extensions")
            path_['vector_type'] = export_settings['current_texture_transform'][k]['vector_type']
            export_settings['current_paths'][k] = path_

    export_settings['current_texture_transform'] = {}

    if len(export_settings['current_normal_scale']) != 0:
        for k in export_settings['current_normal_scale'].keys():
            path_ = {}
            path_['length'] = export_settings['current_normal_scale'][k]['length']
            path_['path'] = export_settings['current_normal_scale'][k]['path'].replace("YYY", "normalTexture")
            export_settings['current_paths'][k] = path_

    export_settings['current_normal_scale'] = {}

    return normal_texture, {
        "normalTexture": uvmap_info}, {
        'normalTexture': udim_info} if len(
            udim_info.keys()) > 0 else {}


def __gather_orm_texture(blender_material, export_settings):
    # Check for the presence of Occlusion, Roughness, Metallic sharing a single image.
    # If not fully shared, return None, so the images will be cached and processed separately.

    occlusion = get_socket(blender_material.node_tree, "Occlusion")
    if occlusion.socket is None or not has_image_node_from_socket(occlusion, export_settings):
        occlusion = get_socket_from_gltf_material_node(
            blender_material.node_tree, "Occlusion")
        if occlusion.socket is None or not has_image_node_from_socket(occlusion, export_settings):
            return None

    metallic_socket = get_socket(blender_material.node_tree, "Metallic")
    roughness_socket = get_socket(blender_material.node_tree, "Roughness")

    hasMetal = metallic_socket.socket is not None and has_image_node_from_socket(metallic_socket, export_settings)
    hasRough = roughness_socket.socket is not None and has_image_node_from_socket(roughness_socket, export_settings)

    # Warning: for default socket, do not use NodeSocket object, because it will break cache
    # Using directlty the Blender socket object
    if not hasMetal and not hasRough:
        metallic_roughness = get_socket_from_gltf_material_node(
            blender_material.node_tree, "MetallicRoughness")
        if metallic_roughness.socket is None or not has_image_node_from_socket(metallic_roughness, export_settings):
            return None
        result = (occlusion, metallic_roughness)
    elif not hasMetal:
        result = (occlusion, roughness_socket)
    elif not hasRough:
        result = (occlusion, metallic_socket)
    else:
        result = (occlusion, roughness_socket, metallic_socket)

    if not gltf2_blender_gather_texture_info.check_same_size_images(result, export_settings):
        export_settings['log'].info(
            "Occlusion and metal-roughness texture will be exported separately "
            "(use same-sized images if you want them combined)"
        )
        return None

    # Double-check this will past the filter in texture_info
    info, _, _, _ = gltf2_blender_gather_texture_info.gather_texture_info(result[0], result, export_settings)
    if info is None:
        return None

    if len(export_settings['current_texture_transform']) != 0:
        for k in export_settings['current_texture_transform'].keys():
            path_ = {}
            path_['length'] = export_settings['current_texture_transform'][k]['length']
            path_['path'] = export_settings['current_texture_transform'][k]['path'].replace(
                "YYY", "occlusionTexture/extensions")
            path_['vector_type'] = export_settings['current_texture_transform'][k]['vector_type']
            export_settings['current_paths'][k] = path_

        # This case can't happen because we are going to keep only 1 UVMap
        export_settings['log'].warning("This case should not happen, please report a bug")
        for k in export_settings['current_texture_transform'].keys():
            path_ = {}
            path_['length'] = export_settings['current_texture_transform'][k]['length']
            path_['path'] = export_settings['current_texture_transform'][k]['path'].replace(
                "YYY", "pbrMetallicRoughness/metallicRoughnessTexture/extensions")
            path_['vector_type'] = export_settings['current_texture_transform'][k]['vector_type']
            export_settings['current_paths'][k] = path_

    export_settings['current_texture_transform'] = {}

    return result


def __gather_occlusion_texture(blender_material, orm_texture, export_settings):
    occlusion = get_socket(blender_material.node_tree, "Occlusion")
    if occlusion.socket is None:
        occlusion = get_socket_from_gltf_material_node(
            blender_material.node_tree, "Occlusion")
    if occlusion.socket is None:
        return None, {}, {}
    occlusion_texture, uvmap_info, udim_info, _ = gltf2_blender_gather_texture_info.gather_material_occlusion_texture_info_class(
        occlusion, orm_texture or (occlusion,), export_settings)

    if len(export_settings['current_occlusion_strength']) != 0:
        for k in export_settings['current_occlusion_strength'].keys():
            path_ = {}
            path_['length'] = export_settings['current_occlusion_strength'][k]['length']
            path_['path'] = export_settings['current_occlusion_strength'][k]['path']
            path_['reverse'] = export_settings['current_occlusion_strength'][k]['reverse']
            export_settings['current_paths'][k] = path_

    export_settings['current_occlusion_strength'] = {}

    if len(export_settings['current_texture_transform']) != 0:
        for k in export_settings['current_texture_transform'].keys():
            path_ = {}
            path_['length'] = export_settings['current_texture_transform'][k]['length']
            path_['path'] = export_settings['current_texture_transform'][k]['path'].replace(
                "YYY", "occlusionTexture/extensions")
            path_['vector_type'] = export_settings['current_texture_transform'][k]['vector_type']
            export_settings['current_paths'][k] = path_

    export_settings['current_texture_transform'] = {}

    return occlusion_texture, \
        {"occlusionTexture": uvmap_info}, {'occlusionTexture': udim_info} if len(udim_info.keys()) > 0 else {}


def __gather_pbr_metallic_roughness(blender_material, orm_texture, export_settings):
    return gltf2_pbr_metallic_roughness.gather_material_pbr_metallic_roughness(
        blender_material,
        orm_texture,
        export_settings)


def __export_unlit(blender_material, export_settings):

    info = gltf2_unlit.detect_shadeless_material(
        blender_material.node_tree,
        export_settings)
    if info is None:
        return None, {}, {"color": None, "alpha": None, "color_type": None, "alpha_type": None, "alpha_mode": "OPAQUE"}, {}

    base_color_texture, uvmap_info, udim_info = gltf2_unlit.gather_base_color_texture(info, export_settings)

    if info.get('alpha_socket'):
        alpha_info = gather_alpha_info(info['alpha_socket'].to_node_nav())
    else:
        alpha_info = gather_alpha_info(None)

    base_color_factor, vc_info = gltf2_unlit.gather_base_color_factor(info, export_settings)

    material = gltf2_io.Material(
        alpha_cutoff=__gather_alpha_cutoff(alpha_info, export_settings),
        alpha_mode=__gather_alpha_mode(alpha_info, export_settings),
        double_sided=__gather_double_sided(blender_material, {}, export_settings),
        extensions={"KHR_materials_unlit": Extension("KHR_materials_unlit", {}, required=False)},
        extras=gather_extras(blender_material, export_settings),
        name=gather_name(blender_material, export_settings),
        emissive_factor=None,
        emissive_texture=None,
        normal_texture=None,
        occlusion_texture=None,

        pbr_metallic_roughness=gltf2_io.MaterialPBRMetallicRoughness(
            base_color_factor=base_color_factor,
            base_color_texture=base_color_texture,
            metallic_factor=0.0,
            roughness_factor=0.9,
            metallic_roughness_texture=None,
            extensions=None,
            extras=None,
        )
    )

    export_user_extensions('gather_material_unlit_hook', export_settings, material, blender_material)

    # Now we have exported the material itself, we need to store some additional data
    # This will be used when trying to export some KHR_animation_pointer

    if len(export_settings['current_paths']) > 0:
        export_settings['KHR_animation_pointer']['materials'][id(blender_material)] = {}
        export_settings['KHR_animation_pointer']['materials'][id(
            blender_material)]['paths'] = export_settings['current_paths'].copy()

    export_settings['current_paths'] = {}

    return material, uvmap_info, vc_info, udim_info


def get_active_uvmap_index(blender_mesh):
    # retrieve active render UVMap
    active_uvmap_idx = 0
    for i in range(len(blender_mesh.uv_layers)):
        if blender_mesh.uv_layers[i].active_render is True:
            active_uvmap_idx = i
            break
    return active_uvmap_idx


def get_final_material(mesh, blender_material, attr_indices, base_material, uvmap_info, export_settings):

    # First, we need to calculate all index of UVMap

    indices = {}
    additional_indices = 0

    for m, v in uvmap_info.items():

        if m.startswith("additional") and additional_indices <= int(m[10:]):
            additional_indices = +1

        if 'type' not in v.keys():
            continue

        if v['type'] == 'Fixed':
            i = mesh.uv_layers.find(v['value'])
            if i >= 0:
                indices[m] = i
            else:
                # Using active index
                indices[m] = get_active_uvmap_index(mesh)
        elif v['type'] == 'Active':
            indices[m] = get_active_uvmap_index(mesh)
        elif v['type'] == "Attribute":
            # This can be a regular UVMap or a custom attribute
            i = mesh.uv_layers.find(v['value'])
            if i >= 0:
                indices[m] = i
            else:
                indices[m] = attr_indices[v['value']]

    # Now we have all needed indices, let's create a set that can be used for
    # caching, so containing all possible textures
    all_textures = get_all_textures(additional_indices)

    caching_indices = []
    for tex in all_textures:
        caching_indices.append(indices.get(tex, None))

    caching_indices = [i if i != 0 else None for i in caching_indices]
    caching_indices = tuple(caching_indices)

    material = __get_final_material_with_indices(blender_material, base_material, caching_indices, export_settings)

    # We need to set the material paths info with the real final material (material with all texCoord, etc.. set)
    if id(blender_material) in export_settings['KHR_animation_pointer']['materials']:
        export_settings['KHR_animation_pointer']['materials'][id(blender_material)]['glTF_material'] = material

    return material


@cached
def caching_material_tex_indices(blender_material, material, caching_indices, export_settings):
    return (
        (id(blender_material),),
        (caching_indices,)
    )


@cached_by_key(key=caching_material_tex_indices)
def __get_final_material_with_indices(blender_material, base_material, caching_indices, export_settings):

    if base_material is None:
        return None

    if all([i is None for i in caching_indices]):
        return base_material

    material = deepcopy(base_material)
    get_new_material_texture_shared(base_material, material)

    for tex, ind in zip(get_all_textures(len(caching_indices) - len(get_all_textures())), caching_indices):

        if ind is None:
            continue

        # Need to check if texture is not None, because it can be the case for UDIM on non managed UDIM textures
        if tex == "emissiveTexture":
            if material.emissive_texture:
                material.emissive_texture.tex_coord = ind
        elif tex == "normalTexture":
            if material.normal_texture:
                material.normal_texture.tex_coord = ind
        elif tex == "occlusionTexture":
            if material.occlusion_texture:
                material.occlusion_texture.tex_coord = ind
        elif tex == "baseColorTexture":
            if material.pbr_metallic_roughness.base_color_texture:
                material.pbr_metallic_roughness.base_color_texture.tex_coord = ind
        elif tex == "metallicRoughnessTexture":
            if material.pbr_metallic_roughness.metallic_roughness_texture:
                material.pbr_metallic_roughness.metallic_roughness_texture.tex_coord = ind
        elif tex == "clearcoatTexture":
            if material.extensions["KHR_materials_clearcoat"].extension['clearcoatTexture']:
                material.extensions["KHR_materials_clearcoat"].extension['clearcoatTexture'].tex_coord = ind
        elif tex == "clearcoatRoughnessTexture":
            if material.extensions["KHR_materials_clearcoat"].extension['clearcoatRoughnessTexture']:
                material.extensions["KHR_materials_clearcoat"].extension['clearcoatRoughnessTexture'].tex_coord = ind
        elif tex == "clearcoatNormalTexture":
            if material.extensions["KHR_materials_clearcoat"].extension['clearcoatNormalTexture']:
                material.extensions["KHR_materials_clearcoat"].extension['clearcoatNormalTexture'].tex_coord = ind
        elif tex == "transmissionTexture":
            if material.extensions["KHR_materials_transmission"].extension['transmissionTexture']:
                material.extensions["KHR_materials_transmission"].extension['transmissionTexture'].tex_coord = ind
        elif tex == "specularTexture":
            if material.extensions["KHR_materials_specular"].extension['specularTexture']:
                material.extensions["KHR_materials_specular"].extension['specularTexture'].tex_coord = ind
        elif tex == "specularColorTexture":
            if material.extensions["KHR_materials_specular"].extension['specularColorTexture']:
                material.extensions["KHR_materials_specular"].extension['specularColorTexture'].tex_coord = ind
        elif tex == "sheenColorTexture":
            if material.extensions["KHR_materials_sheen"].extension['sheenColorTexture']:
                material.extensions["KHR_materials_sheen"].extension['sheenColorTexture'].tex_coord = ind
        elif tex == "sheenRoughnessTexture":
            if material.extensions["KHR_materials_sheen"].extension['sheenRoughnessTexture']:
                material.extensions["KHR_materials_sheen"].extension['sheenRoughnessTexture'].tex_coord = ind
        elif tex == "thicknessTexture":
            if material.extensions["KHR_materials_volume"].extension['thicknessTexture']:
                material.extensions["KHR_materials_volume"].extension['thicknessTexture'].tex_ccord = ind
        elif tex == "anisotropyTexture":
            if material.extensions["KHR_materials_anisotropy"].extension['anisotropyTexture']:
                material.extensions["KHR_materials_anisotropy"].extension['anisotropyTexture'].tex_coord = ind
        elif tex.startswith("additional"):
            export_settings['additional_texture_export'][export_settings['additional_texture_export_current_idx'] +
                                                         int(tex[10:])].tex_coord = ind
        else:
            export_settings['log'].error("some Textures tex coord are not managed")

    export_settings['additional_texture_export_current_idx'] = len(export_settings['additional_texture_export'])

    return material


def get_material_from_idx(material_idx, materials, export_settings):
    mat = None
    if export_settings['gltf_materials'] in ["EXPORT", "VIEWPORT"] and material_idx is not None:
        if materials:
            i = material_idx if material_idx < len(materials) else -1
            mat = materials[i]
    return mat


def get_base_material(material_idx, materials, export_settings):

    export_settings['current_paths'] = {}

    material = None
    material_info = {
        "uv_info": {},
        "vc_info": {
            "color": None,
            "alpha": None,
            "color_type": None,
            "alpha_type": None,
            "alpha_mode": "OPAQUE"
        },
        "udim_info": {}
    }

    mat = get_material_from_idx(material_idx, materials, export_settings)
    if mat is not None:
        material, material_info = gather_material(
            mat,
            export_settings
        )

    if material is None:
        # If no material, the mesh can still have vertex color
        # So, retrieving it if user request it
        if export_settings['gltf_active_vertex_color_when_no_material'] is True:
            material_info["vc_info"] = {"color_type": "active", "alpha_type": "active"}
            # VC will have alpha, as there is no material to know if alpha is used or not
            material_info["vc_info"]["alpha_mode"] = "BLEND"

    return material, material_info


def get_all_textures(idx=0):
    # Make sure to have all texture here, always in same order
    tab = []

    tab.append("emissiveTexture")
    tab.append("normalTexture")
    tab.append("occlusionTexture")
    tab.append("baseColorTexture")
    tab.append("metallicRoughnessTexture")
    tab.append("clearcoatTexture")
    tab.append("clearcoatRoughnessTexture")
    tab.append("clearcoatNormalTexture")
    tab.append("transmissionTexture")
    tab.append("specularTexture")
    tab.append("specularColorTexture")
    tab.append("sheenColorTexture")
    tab.append("sheenRoughnessTexture")
    tab.append("thicknessTexture")
    tab.append("anisotropyTexture")

    for i in range(idx):
        tab.append("additional" + str(i))

    return tab
