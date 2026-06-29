# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from copy import deepcopy
import bpy

from ....io.com import gltf2_io
from ....io.com.gltf2_io_extensions import Extension
from ....io.exp.user_extensions import export_user_extensions
from ..cache import cached, cached_by_key
from ...com.material_helpers import get_gltf_old_group_node_name, get_gltf_node_name, get_gltf_node_old_name
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
from .extensions.iridescence import export_iridescence
from .extensions.ior import export_ior
from .extensions.dispersion import export_dispersion
from .search_node_tree import \
    has_image_node_from_socket, \
    NodeSocket, \
    gather_alpha_info, \
    previous_socket, next_node


class BlenderMaterialIndentifier:
    def __init__(self, blender_material, export_settings):
        self.id = id(blender_material)
        self.used = None

        # Cache system
        self.all_nodes = None
        self.all_nodes_tmp = []
        self.nodes = {}  # Cache by type
        self.gltf_material_node = -1
        self.gltf_material_node_group_path = None
        self.active_output_node = None

        self.material = blender_material
        self.export_settings = export_settings

        self.__set_used_material()
        self.name = self.material.name

    def __set_used_material(self):
        # Currently, there are a few cases where we can not use inline material,
        # so we need to keep the original one for those cases

        if self.__can_use_inline() is False:
            self.use_material = self.material
            self.used = "ORIGINAL"
        else:
            self.inline_material = bpy.types.InlineShaderNodes.from_material(self.material)
            self.use_material = self.inline_material
            self.used = "INLINE"

    def get_used_material(self):
        return self.use_material

    def __can_use_inline(self):
        # Currently, inline material does not support animation
        # So, if we want to export animation with KHR_animation_pointer,
        # we need to use the original material, and not the inline one
        if self.export_settings['gltf_export_anim_pointer'] is True and self.material.node_tree.animation_data is not None:
            return False

        # We can not use inline if using the glTF node (for Occlusion, for example)
        # We can not use the method here, because you still don't know if we are going to use the inline version or not
        # Se we need to rely on the original material here, waiting to know later
        # if we are going to use the inline version or not
        _ = self.__get_all_nodes(self.material.node_tree, [self.material.node_tree])
        if self.gltf_material_node is not None and self.gltf_material_node != -1:
            return False

        # We can not use inline if we are collecting additional textures
        if self.export_settings['gltf_unused_textures'] is True:
            return False

        # We will use inline material, let's clear the node cache
        self.all_nodes = None
        self.nodes = {}
        self.gltf_material_node = None
        self.gltf_material_node_group_path = None
        self.active_output_node = None

        return True

    def get_socket(self, name, volume=False):
        """
        For a given material input name, retrieve the corresponding node tree socket.

        :param blender_material: a blender material for which to get the socket
        :param name: the name of the socket
        :return: a blender NodeSocket
        """
        if self.get_used_material().node_tree is not None:
            # i = [input for input in blender_material.node_tree.inputs]
            # o = [output for output in blender_material.node_tree.outputs]
            if name == "Emissive":
                # Check for a dedicated Emission node first, it must supersede the newer built-in one
                # because the newer one is always present in all Principled BSDF materials.
                emissive_socket = self.get_node_socket(bpy.types.ShaderNodeEmission, "Color")
                if emissive_socket.socket is not None:
                    return emissive_socket
                # If a dedicated Emission node was not found, fall back to the Principled BSDF Emission Color socket.
                name = "Emission Color"
                type = bpy.types.ShaderNodeBsdfPrincipled
            elif name == "Background":
                type = bpy.types.ShaderNodeBackground
                name = "Color"
            else:
                if volume is False:
                    type = bpy.types.ShaderNodeBsdfPrincipled
                else:
                    type = bpy.types.ShaderNodeVolumeAbsorption

            return self.get_node_socket(type, name)

        return NodeSocket(None, None)

    def get_node_socket(self, node_type, socket_name):
        if node_type not in self.nodes.keys():
            self.set_material_nodes(node_type)

        inputs = sum([[(input, node[1]) for input in node[0].inputs if input.name == socket_name]
                     for node in self.nodes[node_type]], [])
        if inputs:
            return NodeSocket(inputs[0][0], inputs[0][1])
        return NodeSocket(None, None)

    def set_material_nodes(self, node_type):
        """
        Store result of get_all_nodes_of_type in the Class instance, avoiding to recalculate it several times
        """
        nodes = self.get_all_nodes_of_type(node_type)
        self.nodes[node_type] = [n for n in nodes if self.__check_if_is_linked_to_active_output_node(n) is True]

    def __check_if_is_linked_to_active_output_node(self, node):
        if self.used == "INLINE":
            # No need to check : Inline keeps only the nodes that are linked to the active output node
            return True

        if len(node[0].outputs) == 0:
            return False

        return self.__recursive_check_if_is_linked_to_active_output_node(NodeSocket(node[0].outputs[0], node[1]))

    def __recursive_check_if_is_linked_to_active_output_node(self, socket):
        if socket.socket is None:
            return False

        next_node_forward = next_node(socket)
        if next_node_forward.node is None:
            return False

        if next_node_forward.node.type == 'OUTPUT_MATERIAL' and next_node_forward.node.is_active_output:
            return True

        if len(next_node_forward.node.outputs) == 0:
            return False
        return self.__recursive_check_if_is_linked_to_active_output_node(
            NodeSocket(next_node_forward.node.outputs[0], next_node_forward.group_path))

    def __get_all_nodes(self, node_tree: bpy.types.NodeTree, group_path):
        if self.all_nodes is None:
            self.all_nodes_tmp = []
            self.__get_all_nodes_recursive(node_tree, group_path)
            self.all_nodes = self.all_nodes_tmp
        return self.all_nodes

    def __get_all_nodes_recursive(self, node_tree: bpy.types.NodeTree, group_path):
        gltf_node_group_names = [get_gltf_node_name().lower(), get_gltf_node_old_name().lower()]

        for node in [n for n in node_tree.nodes if not n.mute]:

            # Check if we have the active output node
            if self.active_output_node is None and node.type == 'OUTPUT_MATERIAL' and node.is_active_output:
                self.active_output_node = node

            self.all_nodes_tmp.append((node, group_path.copy()))

        # Some weird node groups with missing datablock can have no node_tree, so checking n.node_tree (See #1797)
        for node in [n for n in node_tree.nodes if n.type == "GROUP" and n.node_tree is not None and not n.mute]:

            # Do not enter the old glTF node group
            if node.node_tree.name != get_gltf_old_group_node_name():
                new_group_path = group_path.copy()
                new_group_path.append(node)
                self.__get_all_nodes_recursive(node.node_tree, new_group_path)

            # Check if we have the glTF material node
            if self.gltf_material_node == -1 and node.node_tree.name.lower() in gltf_node_group_names:
                self.gltf_material_node = node
                self.gltf_material_node_group_path = group_path.copy()

    def __get_active_output_node(self):
        return self.active_output_node

    def get_all_nodes_of_type(self, type):
        """
        Recursively return all nodes including node groups for the materials
        """

        nodes = self.__get_all_nodes(self.get_used_material().node_tree, [self.get_used_material().node_tree])
        nodes = [n for n in nodes if isinstance(n[0], type)]
        return nodes

    def get_gltf_material_node(self):
        if self.gltf_material_node != -1:
            return self.gltf_material_node, self.gltf_material_node_group_path

        self.gltf_material_node = None
        # Because of call of __get_all_nodes, we now have the glTF material node
        _ = self.get_all_nodes_of_type(bpy.types.ShaderNodeGroup)

        return self.gltf_material_node, self.gltf_material_node_group_path

    def get_socket_from_gltf_material_node(self, socket_name: str):
        gltf_material_node, gltf_material_node_group_path = self.get_gltf_material_node()
        if gltf_material_node is not None:
            inputs = [(input, gltf_material_node_group_path)
                      for input in gltf_material_node.inputs if input.name == socket_name]
            if inputs:
                return NodeSocket(inputs[0][0], inputs[0][1])

        return NodeSocket(None, None)

    def detect_shadeless_material(self):
        # Old Background node detection (unlikely to happen)
        bg_socket = self.get_socket("Background")
        if bg_socket.socket is not None:
            return {'rgb_socket': bg_socket}

        # Look for
        # * any color socket, connected to...
        # * optionally, the lightpath trick, connected to...
        # * optionally, a mix-with-transparent (for alpha), connected to...
        # * the output node

        info = {}

        active_output_node = self.__get_active_output_node()
        if active_output_node is None:
            return None

        socket = NodeSocket(active_output_node.inputs[0], [self.get_used_material().node_tree])

        # Be careful not to misidentify a lightpath trick as mix-alpha.
        result = gltf2_unlit.detect_lightpath_trick(socket)
        if result is not None:
            socket = result['next_socket']
        else:
            result = gltf2_unlit.detect_mix_alpha(socket)
            if result is not None:
                socket = result['next_socket']
                info['alpha_socket'] = result['alpha_socket']

            result = gltf2_unlit.detect_lightpath_trick(socket)
            if result is not None:
                socket = result['next_socket']

        # Check if a color socket, or connected to a color socket
        if socket.socket.type != 'RGBA':
            from_socket = previous_socket(socket)
            if from_socket.socket is None:
                return None
            if from_socket.socket.type != 'RGBA':
                return None

        info['rgb_socket'] = socket
        return info


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
def gather_material(bmat, export_settings):
    """
    Gather the material used by the blender primitive.

    :param blender_material: the blender material used in the glTF primitive
    :param export_settings:
    :return: a glTF material
    """

    bmat = BlenderMaterialIndentifier(bmat, export_settings)

    if not __filter_material(bmat, export_settings):
        return None, {"uv_info": {}, "vc_info": {'color': None, 'alpha': None,
                                                 'color_type': None, 'alpha_type': None, 'alpha_mode': "OPAQUE"}, "udim_info": {}}

    if export_settings['gltf_materials'] == "VIEWPORT":
        return export_viewport_material(bmat.material, export_settings), {"uv_info": {}, "vc_info": {
            'color': None, 'alpha': None, 'color_type': None, 'alpha_type': None, 'alpha_mode': "OPAQUE"}, "udim_info": {}}

    nodes_used = export_settings['nodes_used'] = {}

    # Reset exported images / textures nodes
    export_settings['exported_texture_nodes'] = []

    mat_unlit, uvmap_info, vc_info, udim_info = __export_unlit(bmat, export_settings)
    if mat_unlit is not None:
        export_user_extensions('gather_material_hook', export_settings, mat_unlit, bmat)
        return mat_unlit, {"uv_info": uvmap_info, "vc_info": vc_info, "udim_info": udim_info}

    orm_texture = __gather_orm_texture(bmat, export_settings)

    emissive_factor = __gather_emissive_factor(bmat, export_settings)
    emissive_texture, uvmap_info_emissive, udim_info_emissive = __gather_emissive_texture(
        bmat, export_settings)
    extensions, uvmap_info_extensions, udim_info_extensions = __gather_extensions(
        bmat, emissive_factor, export_settings)
    normal_texture, uvmap_info_normal, udim_info_normal = __gather_normal_texture(bmat, export_settings)
    occlusion_texture, uvmap_info_occlusion, udim_occlusion = __gather_occlusion_texture(
        bmat, orm_texture, export_settings)
    pbr_metallic_roughness, uvmap_info_pbr_metallic_roughness, vc_info, udim_info_prb_mr, alpha_info = __gather_pbr_metallic_roughness(
        bmat, orm_texture, export_settings)

    if any([i > 1.0 for i in emissive_factor or []]) is True:
        # Strength is set on extension
        emission_strength = max(emissive_factor)
        emissive_factor = [f / emission_strength for f in emissive_factor]

    material = gltf2_io.Material(
        alpha_cutoff=__gather_alpha_cutoff(alpha_info, export_settings),
        alpha_mode=__gather_alpha_mode(alpha_info, export_settings),
        double_sided=__gather_double_sided(bmat, extensions, export_settings),
        emissive_factor=emissive_factor,
        emissive_texture=emissive_texture,
        extensions=extensions,
        extras=gather_extras(bmat.material, export_settings),
        name=gather_name(bmat, export_settings),
        normal_texture=normal_texture,
        occlusion_texture=occlusion_texture,
        pbr_metallic_roughness=pbr_metallic_roughness
    )

    if export_settings['gltf_extras'] and export_settings['gltf_export_anim_pointer']:
        export_settings['KHR_animation_pointer']['extras']['materials'][bmat.id]['glTF_extras'] = material
        export_settings['material_identifiers'][bmat.id]['gltf'] = material

    uvmap_infos = {}
    udim_infos = {}

    # Get all textures nodes that are not used in the material
    if export_settings['gltf_unused_textures'] is True:
        if bmat.get_used_material().node_tree:

            nodes = bmat.get_all_nodes_of_type(bpy.types.ShaderNodeTexImage)

        else:
            nodes = []
        # Store index of additional texture for this material
        export_settings['additional_texture_export_current_idx'][bmat.id] = len(
            export_settings['additional_texture_export'])
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
    if material.emissive_factor is not None and bmat.get_socket("Base Color").socket is None:
        material.pbr_metallic_roughness = gltf2_pbr_metallic_roughness.get_default_pbr_for_emissive_node()

    export_user_extensions('gather_material_hook', export_settings, material, bmat.get_used_material())

    # Now we have exported the material itself, we need to store some additional data
    # This will be used when trying to export some KHR_animation_pointer

    if len(export_settings['current_paths']) > 0 and bmat.used == "ORIGINAL":
        export_settings['KHR_animation_pointer'][None]['materials'][bmat.id] = {}
        export_settings['KHR_animation_pointer'][None]['materials'][bmat.id]['paths'] = export_settings['current_paths'].copy()

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


def __filter_material(bmat, export_settings):
    return export_settings['gltf_materials']


def __gather_alpha_cutoff(alpha_info, export_settings):
    if alpha_info['alphaMode'] == 'MASK':
        cutoff = alpha_info['alphaCutoff']

        if alpha_info['alphaCutoffPath'] is not None:
            # This can be None, because cutoff can be set using Round, that can not be animated
            path_ = {}
            path_['length'] = 1
            path_['path'] = "/materials/XXX/alphaCutoff"
            export_settings['current_paths'][alpha_info['alphaCutoffPath']] = path_

        return None if cutoff == 0.5 else cutoff
    return None


def __gather_alpha_mode(alpha_info, export_settings):
    mode = alpha_info['alphaMode']
    return None if mode == 'OPAQUE' else mode


def __gather_double_sided(bmat, extensions, export_settings):

    # If user create a volume extension, we force double sided to False
    if 'KHR_materials_volume' in extensions:
        return False

    # use_backface_culling can not be retrieve from inline node tree
    # So we need to check it on original material
    if not bmat.material.use_backface_culling:
        return True
    return None


def __gather_emissive_factor(bmat, export_settings):
    return export_emission_factor(bmat, export_settings)


def __gather_emissive_texture(bmat, export_settings):
    return export_emission_texture(bmat, export_settings)


def __gather_extensions(bmat, emissive_factor, export_settings):
    extensions = {}

    uvmap_infos = {}
    udim_infos = {}

    # KHR_materials_clearcoat
    clearcoat_extension, uvmap_info, udim_info_clearcoat = export_clearcoat(bmat, export_settings)
    if clearcoat_extension:
        extensions["KHR_materials_clearcoat"] = clearcoat_extension
        uvmap_infos.update(uvmap_info)
        udim_infos.update(udim_info_clearcoat)

    # KHR_materials_transmission
    transmission_extension, uvmap_info, udim_info_transmission = export_transmission(bmat, export_settings)
    if transmission_extension:
        extensions["KHR_materials_transmission"] = transmission_extension
        uvmap_infos.update(uvmap_info)
        udim_infos.update(udim_info_transmission)

    # KHR_materials_emissive_strength
    emissive_strength_extension = export_emission_strength_extension(emissive_factor, export_settings)
    if emissive_strength_extension:
        extensions["KHR_materials_emissive_strength"] = emissive_strength_extension

    # KHR_materials_volume
    volume_extension, uvmap_info, udim_info = export_volume(bmat, export_settings)
    if volume_extension:
        extensions["KHR_materials_volume"] = volume_extension
        uvmap_infos.update(uvmap_info)
        udim_infos.update(udim_info)

    # KHR_materials_specular
    specular_extension, uvmap_info, udim_info = export_specular(bmat, export_settings)
    if specular_extension:
        extensions["KHR_materials_specular"] = specular_extension
        uvmap_infos.update(uvmap_info)
        udim_infos.update(udim_info)

    # KHR_materials_sheen
    sheen_extension, uvmap_info, udim_info = export_sheen(bmat, export_settings)
    if sheen_extension:
        extensions["KHR_materials_sheen"] = sheen_extension
        uvmap_infos.update(uvmap_info)
        udim_infos.update(udim_info)

    # KHR_materials_anisotropy
    anisotropy_extension, uvmap_info, udim_info = export_anisotropy(bmat, export_settings)
    if anisotropy_extension:
        extensions["KHR_materials_anisotropy"] = anisotropy_extension
        uvmap_infos.update(uvmap_info)
        udim_infos.update(udim_info)

    # KHR_materials_iridescence
    iridescence_extension, uvmap_info, udim_info = export_iridescence(bmat, export_settings)
    if iridescence_extension:
        extensions["KHR_materials_iridescence"] = iridescence_extension
        uvmap_infos.update(uvmap_info)
        udim_infos.update(udim_info)

    # KHR_materials_dispersion
    dispersion_extension = export_dispersion(bmat, extensions, export_settings)
    if dispersion_extension:
        extensions["KHR_materials_dispersion"] = dispersion_extension

    # KHR_materials_ior
    # Keep this extension at the end, because we export it only if some others are exported
    ior_extension = export_ior(bmat, extensions, export_settings)
    if ior_extension:
        extensions["KHR_materials_ior"] = ior_extension

    return extensions, uvmap_infos, udim_infos


def __gather_normal_texture(bmat, export_settings):
    normal = bmat.get_socket("Normal")
    normal_texture, uvmap_info, udim_info, _ = gltf2_blender_gather_texture_info.gather_material_normal_texture_info_class(
        normal, (normal,), export_settings)

    if len(export_settings['current_texture_transform']) != 0:
        for k in export_settings['current_texture_transform'].keys():
            path_ = {}
            path_['length'] = export_settings['current_texture_transform'][k]['length']
            path_['path'] = export_settings['current_texture_transform'][k]['path'].replace(
                "YYY", "normalTexture/extensions")
            path_['vector_type'] = export_settings['current_texture_transform'][k]['vector_type']
            if k in export_settings['current_paths']:
                if 'additional' not in export_settings['current_paths'][k]:
                    export_settings['current_paths'][k]['additional'] = []
                if path_['path'] != export_settings['current_paths'][k]['path']:
                    export_settings['current_paths'][k]['additional'].append(path_['path'])
            else:
                export_settings['current_paths'][k] = path_

    export_settings['current_texture_transform'] = {}

    if len(export_settings['current_normal_scale']) != 0:
        for k in export_settings['current_normal_scale'].keys():
            path_ = {}
            path_['length'] = export_settings['current_normal_scale'][k]['length']
            path_['path'] = export_settings['current_normal_scale'][k]['path'].replace("YYY", "normalTexture")
            if k in export_settings['current_paths']:
                if 'additional' not in export_settings['current_paths'][k]:
                    export_settings['current_paths'][k]['additional'] = []
                if path_['path'] != export_settings['current_paths'][k]['path']:
                    export_settings['current_paths'][k]['additional'].append(path_['path'])
            else:
                export_settings['current_paths'][k] = path_

    export_settings['current_normal_scale'] = {}

    return normal_texture, {
        "normalTexture": uvmap_info}, {
        'normalTexture': udim_info} if len(
            udim_info.keys()) > 0 else {}


def __gather_orm_texture(bmat, export_settings):
    # Check for the presence of Occlusion, Roughness, Metallic sharing a single image.
    # If not fully shared, return None, so the images will be cached and processed separately.

    occlusion = bmat.get_socket("Occlusion")
    if occlusion.socket is None or not has_image_node_from_socket(occlusion, export_settings):
        occlusion = bmat.get_socket_from_gltf_material_node("Occlusion")
        if occlusion.socket is None or not has_image_node_from_socket(occlusion, export_settings):
            return None

    metallic_socket = bmat.get_socket("Metallic")
    roughness_socket = bmat.get_socket("Roughness")

    hasMetal = metallic_socket.socket is not None and has_image_node_from_socket(metallic_socket, export_settings)
    hasRough = roughness_socket.socket is not None and has_image_node_from_socket(roughness_socket, export_settings)

    # Warning: for default socket, do not use NodeSocket object, because it will break cache
    # Using directlty the Blender socket object
    if not hasMetal and not hasRough:
        metallic_roughness = bmat.get_socket_from_gltf_material_node("MetallicRoughness")
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
            if k in export_settings['current_paths']:
                if 'additional' not in export_settings['current_paths'][k]:
                    export_settings['current_paths'][k]['additional'] = []
                if path_['path'] != export_settings['current_paths'][k]['path']:
                    export_settings['current_paths'][k]['additional'].append(path_['path'])
            else:
                export_settings['current_paths'][k] = path_

        # This case can't happen because we are going to keep only 1 UVMap
        export_settings['log'].warning("This case should not happen, please report a bug")
        for k in export_settings['current_texture_transform'].keys():
            path_ = {}
            path_['length'] = export_settings['current_texture_transform'][k]['length']
            path_['path'] = export_settings['current_texture_transform'][k]['path'].replace(
                "YYY", "pbrMetallicRoughness/metallicRoughnessTexture/extensions")
            path_['vector_type'] = export_settings['current_texture_transform'][k]['vector_type']
            if k in export_settings['current_paths']:
                if 'additional' not in export_settings['current_paths'][k]:
                    export_settings['current_paths'][k]['additional'] = []
                if path_['path'] != export_settings['current_paths'][k]['path']:
                    export_settings['current_paths'][k]['additional'].append(path_['path'])
            else:
                export_settings['current_paths'][k] = path_

    export_settings['current_texture_transform'] = {}

    return result


def __gather_occlusion_texture(bmat, orm_texture, export_settings):
    occlusion = bmat.get_socket("Occlusion")
    if occlusion.socket is None:
        occlusion = bmat.get_socket_from_gltf_material_node("Occlusion")
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
            if k in export_settings['current_paths']:
                if 'additional' not in export_settings['current_paths'][k]:
                    export_settings['current_paths'][k]['additional'] = []
                if path_['path'] != export_settings['current_paths'][k]['path']:
                    export_settings['current_paths'][k]['additional'].append(path_['path'])
            else:
                export_settings['current_paths'][k] = path_

    export_settings['current_occlusion_strength'] = {}

    if len(export_settings['current_texture_transform']) != 0:
        for k in export_settings['current_texture_transform'].keys():
            path_ = {}
            path_['length'] = export_settings['current_texture_transform'][k]['length']
            path_['path'] = export_settings['current_texture_transform'][k]['path'].replace(
                "YYY", "occlusionTexture/extensions")
            path_['vector_type'] = export_settings['current_texture_transform'][k]['vector_type']
            if k in export_settings['current_paths']:
                if 'additional' not in export_settings['current_paths'][k]:
                    export_settings['current_paths'][k]['additional'] = []
                if path_['path'] != export_settings['current_paths'][k]['path']:
                    export_settings['current_paths'][k]['additional'].append(path_['path'])
            else:
                export_settings['current_paths'][k] = path_

    export_settings['current_texture_transform'] = {}

    return occlusion_texture, {
        "occlusionTexture": uvmap_info}, {
        'occlusionTexture': udim_info} if len(
            udim_info.keys()) > 0 else {}


def __gather_pbr_metallic_roughness(bmat, orm_texture, export_settings):
    return gltf2_pbr_metallic_roughness.gather_material_pbr_metallic_roughness(
        bmat,
        orm_texture,
        export_settings)


def __export_unlit(bmat, export_settings):

    info = bmat.detect_shadeless_material()
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
        double_sided=__gather_double_sided(bmat, {}, export_settings),
        extensions={"KHR_materials_unlit": Extension("KHR_materials_unlit", {}, required=False)},
        extras=gather_extras(bmat.material, export_settings),
        name=gather_name(bmat, export_settings),
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

    if export_settings['gltf_extras'] and export_settings['gltf_export_anim_pointer']:
        export_settings['KHR_animation_pointer']['extras']['materials'][bmat.id]['glTF_extras'] = material

    export_user_extensions('gather_material_unlit_hook', export_settings, material, bmat.get_used_material())

    # Now we have exported the material itself, we need to store some additional data
    # This will be used when trying to export some KHR_animation_pointer

    if len(export_settings['current_paths']) > 0 and bmat.used == "ORIGINAL":
        export_settings['KHR_animation_pointer'][None]['materials'][bmat.id] = {}
        export_settings['KHR_animation_pointer'][None]['materials'][id(
            bmat.get_used_material())]['paths'] = export_settings['current_paths'].copy()

    export_settings['current_paths'] = {}

    return material, uvmap_info, vc_info, udim_info


def get_final_material(mesh, blender_material, attr_indices, base_material, uvmap_info, export_settings):

    # First, we need to calculate all index of UVMap

    indices = {}
    additional_indices = 0

    for m, v in uvmap_info.items():

        if m.startswith("additional") and additional_indices <= int(m[10:]):
            additional_indices += 1

        if 'type' not in v.keys():
            continue

        if v['type'] == 'Fixed':
            i = mesh.uv_layers.find(v['value'])
            if i >= 0:
                indices[m] = i
            else:
                # Using render index
                indices[m] = mesh.uv_layers.active_render_index
        elif v['type'] == 'Render':
            indices[m] = mesh.uv_layers.active_render_index
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
    if id(blender_material) in export_settings['KHR_animation_pointer'][None]['materials']:
        export_settings['KHR_animation_pointer'][None]['materials'][id(blender_material)]['glTF_material'] = material

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
        elif tex == "iridescenceTexture":
            if material.extensions["KHR_materials_iridescence"].extension['iridescenceTexture']:
                material.extensions["KHR_materials_iridescence"].extension['iridescenceTexture'].tex_coord = ind
        elif tex == "iridescenceThicknessTexture":
            if material.extensions["KHR_materials_iridescence"].extension['iridescenceThicknessTexture']:
                material.extensions["KHR_materials_iridescence"].extension['iridescenceThicknessTexture'].tex_coord = ind
        elif tex.startswith("additional"):
            export_settings['additional_texture_export'][export_settings['additional_texture_export_current_idx']
                                                         [id(blender_material)] + int(tex[10:])].tex_coord = ind
            # We can use id(blender_material) here, as we are not using inline tree
            # for additional textures, so the material is always the original one
        else:
            export_settings['log'].error("some Textures tex coord are not managed")

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
    tab.append("iridescenceTexture")
    tab.append("iridescenceThicknessTexture")

    for i in range(idx):
        tab.append("additional" + str(i))

    return tab
