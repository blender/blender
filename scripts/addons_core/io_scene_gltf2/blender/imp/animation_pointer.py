# SPDX-FileCopyrightText: 2018-2023 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from ...io.imp.user_extensions import import_user_extensions
from ...io.imp.gltf2_io_binary import BinaryData
from ..exp.material.search_node_tree import NodeSocket, previous_node, from_socket, get_socket, FilterByType, get_socket_from_gltf_material_node, get_texture_node_from_socket  # TODO move to COM
from ..exp.sampler import detect_manual_uv_wrapping  # TODO move to COM
from ..exp.material.unlit import detect_shadeless_material  # TODO move to COM
from ..com.conversion import texture_transform_gltf_to_blender
from .animation_utils import make_fcurve
from .light import BlenderLight
from .camera import BlenderCamera


class BlenderPointerAnim():
    """Blender Pointer Animation."""
    def __new__(cls, *args, **kwargs):
        raise RuntimeError("%s should not be instantiated" % cls)

    @staticmethod
    def anim(gltf, anim_idx, asset, asset_idx, asset_type, name=None, is_unlit=False):
        animation = gltf.data.animations[anim_idx]

        if asset_type in ["LIGHT", "TEX_TRANSFORM", "EXT"]:
            if anim_idx not in asset['animations'].keys():
                return
            tab = asset['animations']
        else:
            if anim_idx not in asset.animations.keys():
                return
            tab = asset.animations

        for channel_idx in tab[anim_idx]:
            channel = animation.channels[channel_idx]
            BlenderPointerAnim.do_channel(gltf, anim_idx, channel, asset, asset_idx,
                                          asset_type, name=name, is_unlit=is_unlit)

    @staticmethod
    def do_channel(gltf, anim_idx, channel, asset, asset_idx, asset_type, name=None, is_unlit=False):
        animation = gltf.data.animations[anim_idx]
        pointer_tab = channel.target.extensions["KHR_animation_pointer"]["pointer"].split("/")

        import_user_extensions('gather_import_animation_pointer_channel_before_hook', gltf, animation, channel)

        # For some asset_type, we need to check what is the real ID type.
        if asset_type == "MATERIAL":
            if len(pointer_tab) == 4 and pointer_tab[1] == "materials" and \
                    pointer_tab[3] == "alphaCutoff":
                target_id_type = "MATERIAL"
            else:
                target_id_type = "NODETREE"
        elif asset_type == "MATERIAL_PBR":
            target_id_type = "NODETREE"
        else:
            target_id_type = asset_type

        action, slot = BlenderPointerAnim.get_or_create_action_and_slot(
            gltf, anim_idx, asset, asset_idx, target_id_type, name_=name)

        keys = BinaryData.get_data_from_accessor(gltf, animation.samplers[channel.sampler].input)
        values = BinaryData.get_data_from_accessor(gltf, animation.samplers[channel.sampler].output)

        if animation.samplers[channel.sampler].interpolation == "CUBICSPLINE":
            # TODO manage tangent?
            values = values[1::3]

        # Convert the curve from glTF to Blender.
        blender_path = None
        num_components = None
        group_name = ''
        # Camera
        if len(pointer_tab) == 5 and pointer_tab[1] == "cameras" and \
                pointer_tab[3] in ["perspective"] and \
                pointer_tab[4] in ["znear", "zfar"]:  # Aspect Ratio is not something we can animate in Blender
            blender_path = {
                "znear": "clip_start",
                "zfar": "clip_end"
            }.get(pointer_tab[4])
            group_name = 'Camera'
            num_components = 1

        if len(pointer_tab) == 5 and pointer_tab[1] == "cameras" and \
                pointer_tab[3] in ["perspective"] and \
                pointer_tab[4] == "yfov":

            blender_path = "lens"
            group_name = 'Camera'
            num_components = 1

            old_values = values.copy()
            sensor = asset.blender_object_data.sensor_height
            for idx, i in enumerate(old_values):
                values[idx] = [BlenderCamera.calc_lens_from_fov(gltf, i[0], sensor)]

        if len(pointer_tab) == 5 and pointer_tab[1] == "cameras" and \
                pointer_tab[3] in ["orthographic"] and \
                pointer_tab[4] in ["ymag", "xmag"]:

            if len(asset.multiple_channels_mag) != 0:

                # We need to calculate the value, based on ymag and xmag
                if "xmag" in asset.multiple_channels_mag.keys():
                    xmag_animation = gltf.data.animations[asset.multiple_channels_mag['xmag'][0]]
                    xmag_channel = xmag_animation.channels[asset.multiple_channels_mag['xmag'][1]]
                    xmag_keys = BinaryData.get_data_from_accessor(
                        gltf, xmag_animation.samplers[xmag_channel.sampler].input)
                    xmag_values = BinaryData.get_data_from_accessor(
                        gltf, xmag_animation.samplers[xmag_channel.sampler].output)
                else:
                    xmag_keys == keys.copy()
                    xmag_values = [asset.orthographic.xmag] * len(keys)

                if "ymag" in asset.multiple_channels_mag.keys():
                    ymag_animation = gltf.data.animations[asset.multiple_channels_mag['ymag'][0]]
                    ymag_channel = ymag_animation.channels[asset.multiple_channels_mag['ymag'][1]]
                    ymag_keys = BinaryData.get_data_from_accessor(
                        gltf, ymag_animation.samplers[ymag_channel.sampler].input)
                    ymag_values = BinaryData.get_data_from_accessor(
                        gltf, ymag_animation.samplers[ymag_channel.sampler].output)
                else:
                    ymag_keys == keys.copy()
                    ymag_values = [asset.orthographic.ymag] * len(keys)

                # We will manage it only if keys are the same... TODO ?
                if xmag_keys == ymag_keys:

                    blender_path = "ortho_scale"
                    group_name = 'Camera'
                    num_components = 1

                    old_values = values.copy()
                    for idx, i in enumerate(old_values):
                        values[idx] = max(xmag_values[idx], ymag_values[idx]) * 2

                # Delete values, as we don't need to add keyframes again for ortho_scale
                # (xmag + ymag channels => only 1 ortho_scale channel in blender)
                asset.multiple_channels_mag = {}

        # Light
        if len(pointer_tab) == 6 and pointer_tab[1] == "extensions" and \
                pointer_tab[2] == "KHR_lights_punctual" and \
                pointer_tab[3] == "lights" and \
                pointer_tab[5] in ["intensity", "color", "range"]:

            blender_path = {
                "color": "color",
                "intensity": "energy"
            }.get(pointer_tab[5])
            group_name = 'Light'
            num_components = 3 if blender_path == "color" else 1

            # TODO perf, using numpy
            if blender_path == "energy":
                old_values = values.copy()
                for idx, i in enumerate(old_values):
                    if asset['type'] in ["SPOT", "POINT"]:
                        values[idx] = [BlenderLight.calc_energy_pointlike(gltf, i[0])]
                    else:
                        values[idx] = [BlenderLight.calc_energy_directional(gltf, i[0])]

            # TODO range, not implemented (even not in static import)

        if len(pointer_tab) == 6 and pointer_tab[1] == "extensions" and \
                pointer_tab[2] == "KHR_lights_punctual" and \
                pointer_tab[3] == "lights" and \
                pointer_tab[5] in ["spot.outerConeAngle", "spot.innerConeAngle"]:

            if pointer_tab[5] == "spot.outerConeAngle":
                blender_path = "spot_size"
                group_name = 'Light'
                num_components = 1

                old_values = values.copy()
                for idx, i in enumerate(old_values):
                    values[idx] = [values[idx][0] * 2]

            if pointer_tab[5] == "spot.innerConeAngle":
                if "spot.outerConeAngle" in asset["multiple_channels"].keys():
                    outer_animation = gltf.data.animations[asset['multiple_channels']['spot.outerConeAngle'][0]]
                    outer_channel = outer_animation.channels[asset['multiple_channels']['spot.outerConeAngle'][1]]
                    outer_keys = BinaryData.get_data_from_accessor(
                        gltf, outer_animation.samplers[outer_channel.sampler].input)
                    outer_values = BinaryData.get_data_from_accessor(
                        gltf, outer_animation.samplers[outer_channel.sampler].output)
                else:
                    outer_keys = keys.copy()
                    outer_values = [[asset['spot']['outerConeAngle']]] * len(keys)

                # We will manage it only if keys are the same... TODO ?
                if keys == outer_keys:
                    old_values = values.copy()
                    for idx, i in enumerate(old_values):
                        values[idx] = [BlenderLight.calc_spot_cone_inner(gltf, outer_values[idx][0], values[idx][0])]
                blender_path = "spot_blend"
                group_name = 'Light'
                num_components = 1

        # Materials
        if len(pointer_tab) == 4 and pointer_tab[1] == "materials" and \
                pointer_tab[3] in ["emissiveFactor", "alphaCutoff"]:

            if pointer_tab[3] == "emissiveFactor":
                emissive_socket = get_socket(asset.blender_nodetree, "Emissive")
                if emissive_socket.socket.is_linked:
                    # We need to find the correct node value to animate (An Emissive Factor node)
                    mix_node = emissive_socket.socket.links[0].from_node
                    if mix_node.type == "MIX":
                        blender_path = mix_node.inputs[7].path_from_id() + ".default_value"
                        group_name = 'Material'
                        num_components = 3
                    else:
                        print("Error, something is wrong, we didn't detect adding a Mix Node because of Pointers")
                else:
                    blender_path = emissive_socket.socket.path_from_id() + ".default_value"
                    num_components = 3
            elif pointer_tab[3] == "alphaCutoff":
                blender_path = "alpha_threshold"
                group_name = 'Material'
                num_components = 1

        if len(pointer_tab) == 5 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "normalTexture" and \
                pointer_tab[4] == "scale":

            normal_socket = get_socket(asset.blender_nodetree, "Normal")
            if normal_socket.socket.is_linked:
                normal_node = normal_socket.socket.links[0].from_node
                if normal_node.type == "NORMAL_MAP":
                    blender_path = normal_node.inputs[0].path_from_id() + ".default_value"
                    group_name = 'Material'
                    num_components = 1

        if len(pointer_tab) == 5 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "occlusionTexture" and \
                pointer_tab[4] == "strength":

            occlusion_socket = get_socket(asset.blender_nodetree, "Occlusion")
            if occlusion_socket.socket is None:
                occlusion_socket = get_socket_from_gltf_material_node(asset.blender_mat.node_tree, "Occlusion")
            if occlusion_socket.socket.is_linked:
                mix_node = occlusion_socket.socket.links[0].from_node
                if mix_node.type == "MIX":
                    blender_path = mix_node.inputs[0].path_from_id() + ".default_value"
                    group_name = 'Material'
                    num_components = 1
                else:
                    print("Error, something is wrong, we didn't detect adding a Mix Node because of Pointers")
            else:
                blender_path = occlusion_socket.socket.path_from_id() + ".default_value"
                group_name = 'Material'
                num_components = 1

        if len(pointer_tab) == 5 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "pbrMetallicRoughness" and \
                pointer_tab[4] in ["baseColorFactor", "roughnessFactor", "metallicFactor"]:

            if pointer_tab[4] == "baseColorFactor":

                # This can be regular PBR, or unlit
                if is_unlit is False:

                    base_color_socket = get_socket(asset.blender_nodetree, "Base Color")
                    if base_color_socket.socket.is_linked:
                        # We need to find the correct node value to animate (An Mix Factor node)
                        mix_node = base_color_socket.links[0].from_node
                        if mix_node.type == "MIX":
                            blender_path = mix_node.inputs[7].path_from_id() + ".default_value"
                            group_name = 'Material'
                            num_components = 3  # Do not use alpha here, will be managed later
                        else:
                            print("Error, something is wrong, we didn't detect adding a Mix Node because of Pointers")
                    else:
                        blender_path = base_color_socket.socket.path_from_id() + ".default_value"
                        num_components = 3  # Do not use alpha here, will be managed later

                else:
                    unlit_info = detect_shadeless_material(asset.blender_nodetree, {})
                    if 'rgb_socket' in unlit_info:
                        socket = unlit_info['rgb_socket']
                        blender_path = socket.socket.path_from_id() + ".default_value"
                        group_name = 'Material'
                        num_components = 3
                    else:
                        socket = NodeSocket(None, None)

            if pointer_tab[4] == "roughnessFactor":
                roughness_socket = get_socket(asset.blender_nodetree, "Roughness")
                if roughness_socket.socket.is_linked:
                    # We need to find the correct node value to animate (An Mix Factor node)
                    mix_node = roughness_socket.links[0].from_node
                    if mix_node.type == "MATH":
                        blender_path = mix_node.inputs[1].path_from_id() + ".default_value"
                        group_name = 'Material'
                        num_components = 1
                    else:
                        print("Error, something is wrong, we didn't detect adding a Mix Node because of Pointers")
                else:
                    blender_path = roughness_socket.socket.path_from_id() + ".default_value"
                    group_name = 'Material'
                    num_components = 1

            if pointer_tab[4] == "metallicFactor":
                metallic_socket = get_socket(asset.blender_nodetree, "Metallic")
                if metallic_socket.socket.is_linked:
                    # We need to find the correct node value to animate (An Mix Factor node)
                    mix_node = metallic_socket.links[0].from_node
                    if mix_node.type == "MATH":
                        blender_path = mix_node.inputs[1].path_from_id() + ".default_value"
                        group_name = 'Material'
                        num_components = 1
                    else:
                        print("Error, something is wrong, we didn't detect adding a Mix Node because of Pointers")
                else:
                    blender_path = metallic_socket.socket.path_from_id() + ".default_value"
                    group_name = 'Material'
                    num_components = 1

        if len(pointer_tab) >= 7 and pointer_tab[1] == "materials" and \
                pointer_tab[-3] == "extensions" and \
                pointer_tab[-2] == "KHR_texture_transform" and \
                pointer_tab[-1] in ["scale", "offset", "rotation"]:

            socket = None
            if pointer_tab[-4] == "baseColorTexture":
                # This can be regular PBR, or unlit
                if is_unlit is False:
                    socket = get_socket(asset['blender_nodetree'], "Base Color")
                else:
                    unlit_info = detect_shadeless_material(asset['blender_nodetree'], {})
                    if 'rgb_socket' in unlit_info:
                        socket = unlit_info['rgb_socket']
                    else:
                        socket = NodeSocket(None, None)
            elif pointer_tab[-4] == "emissiveTexture":
                socket = get_socket(asset.blender_nodetree, "Emission Color")
            elif pointer_tab[-4] == "normalTexture":
                socket = get_socket(asset.blender_nodetree, "Normal")
            elif pointer_tab[-4] == "occlusionTexture":
                socket = get_socket(asset.blender_nodetree, "Occlusion")
                if socket is None:
                    socket = get_socket_from_gltf_material_node(asset.blender_nodetree, "Occlusion")
            elif pointer_tab[-4] == "metallicRoughnessTexture":
                socket = get_socket(asset.blender_nodetree, "Roughness")
            elif pointer_tab[-4] == "specularTexture":
                socket = get_socket(asset['blender_nodetree'], "Specular IOR Level")
            elif pointer_tab[-4] == "specularColorTexture":
                socket = get_socket(asset['blender_nodetree'], "Specular Tint")
            elif pointer_tab[-4] == "sheenColorTexture":
                socket = get_socket(asset['blender_nodetree'], "Sheen Tint")
            elif pointer_tab[-4] == "sheenRoughnessTexture":
                socket = get_socket(asset['blender_nodetree'], "Sheen Roughness")
            elif pointer_tab[-4] == "clearcoatTexture":
                socket = get_socket(asset['blender_nodetree'], "Coat Weight")
            elif pointer_tab[-4] == "clearcoatRoughnessTexture":
                socket = get_socket(asset['blender_nodetree'], "Coat Roughness")
            elif pointer_tab[-4] == "clearcoatNormalTexture":
                socket = get_socket(asset['blender_nodetree'], "Coat Normal")
            elif pointer_tab[-4] == "thicknessTexture":
                socket = get_socket_from_gltf_material_node(asset['blender_nodetree'], "Thickness")
            elif pointer_tab[-4] == "transmissionTexture":
                socket = get_socket(asset['blender_nodetree'], "Transmission Weight")
            else:
                print("Some Texture are not managed for KHR_animation_pointer / KHR_texture_transform")

            tex = get_texture_node_from_socket(socket, {}) if socket.socket is not None else None
            tex_node = tex.shader_node if tex is not None else None
            if tex_node is not None:
                result = detect_manual_uv_wrapping(tex_node, tex.group_path)
                if result:
                    mapping_node = previous_node(result['next_socket'])
                else:
                    mapping_node = previous_node(NodeSocket(tex_node.inputs['Vector'], tex.group_path))
            else:
                mapping_node = None

            if mapping_node is not None:
                if pointer_tab[-1] == "offset":
                    blender_path = mapping_node.node.inputs[1].path_from_id() + ".default_value"
                    group_name = 'Material'
                    num_components = 2
                elif pointer_tab[-1] == "rotation":
                    blender_path = mapping_node.node.inputs[2].path_from_id() + ".default_value"
                    group_name = 'Material'
                    num_components = 2
                elif pointer_tab[-1] == "scale":
                    blender_path = mapping_node.node.inputs[3].path_from_id() + ".default_value"
                    group_name = 'Material'
                    num_components = 2

            if pointer_tab[-1] == "rotation":
                pass  # No conversion needed
            elif pointer_tab[-1] == "scale":
                pass  # No conversion needed
            elif pointer_tab[-1] == "offset":
                # This need scale and rotation
                if 'rotation' in asset['multiple_channels'].keys():
                    animation_rotation = gltf.data.animations[asset['multiple_channels']['rotation'][0]]
                    channel_rotation = animation_rotation.channels[asset['multiple_channels']['rotation'][1]]
                    keys_rotation = BinaryData.get_data_from_accessor(
                        gltf, animation_rotation.samplers[channel_rotation.sampler].input)
                    values_rotation = BinaryData.get_data_from_accessor(
                        gltf, animation_rotation.samplers[channel_rotation.sampler].output)
                else:
                    keys_rotation = keys.copy()
                    values_rotation = [asset.get('rotation', 0.0)] * len(keys)

                if 'scale' in asset['multiple_channels'].keys():
                    animation_scale = gltf.data.animations[asset['multiple_channels']['scale'][0]]
                    channel_scale = animation_scale.channels[asset['multiple_channels']['scale'][1]]
                    keys_scale = BinaryData.get_data_from_accessor(
                        gltf, animation_scale.samplers[channel_scale.sampler].input)
                    values_scale = BinaryData.get_data_from_accessor(
                        gltf, animation_scale.samplers[channel_scale.sampler].output)
                else:
                    keys_scale = keys.copy()
                    values_scale = [asset.get('scale', [1.0, 1.0])] * len(keys)

                # We will manage it only if keys are the same... TODO ?
                if keys == keys_rotation == keys_scale:
                    old_values = values.copy()
                    for idx, i in enumerate(old_values):
                        values[idx] = texture_transform_gltf_to_blender(
                            {'rotation': values_rotation[idx], 'scale': values_scale[idx], 'offset': i}).get('offset')

        if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_emissive_strength" and \
                pointer_tab[5] == "emissiveStrength":

            socket = get_socket(asset['blender_nodetree'], "Emission Strength")
            blender_path = socket.socket.path_from_id() + ".default_value"
            group_name = 'Material'
            num_components = 1

        if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_volume" and \
                pointer_tab[5] in ["thicknessFactor", "attenuationDistance", "attenuationColor"]:

            if pointer_tab[5] == "thicknessFactor":
                thicknesss_socket = get_socket_from_gltf_material_node(asset['blender_nodetree'], 'Thickness')
                if thicknesss_socket.socket.is_linked:
                    mix_node = thicknesss_socket.socket.links[0].from_node
                    if mix_node.type == "MATH":
                        blender_path = mix_node.inputs[1].path_from_id() + ".default_value"
                        group_name = 'Material'
                        num_components = 1
                    else:
                        print("Error, something is wrong, we didn't detect adding a Mix Node because of Pointers")
                else:
                    blender_path = thicknesss_socket.socket.path_from_id() + ".default_value"
                    group_name = 'Material'
                    num_components = 1

            if pointer_tab[5] == "attenuationDistance":
                density_socket = get_socket(asset['blender_nodetree'], 'Density', volume=True)
                blender_path = density_socket.socket.path_from_id() + ".default_value"
                group_name = 'Material'
                num_components = 1

                old_values = values.copy()
                for idx, i in enumerate(old_values):
                    values[idx] = [1.0 / old_values[idx][0]]

            if pointer_tab[5] == "attenuationColor":
                attenuation_color_socket = get_socket(asset['blender_nodetree'], 'Color', volume=True)
                blender_path = attenuation_color_socket.socket.path_from_id() + ".default_value"
                group_name = 'Material'
                num_components = 3

        if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_ior" and \
                pointer_tab[5] == "ior":

            ior_socket = get_socket(asset['blender_nodetree'], 'IOR')
            blender_path = ior_socket.socket.path_from_id() + ".default_value"
            group_name = 'Material'
            num_components = 1

        if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_transmission" and \
                pointer_tab[5] == "transmissionFactor":

            transmission_socket = get_socket(asset['blender_nodetree'], 'Transmission Weight')
            if transmission_socket.socket.is_linked:
                mix_node = transmission_socket.links[0].from_node
                if mix_node.type == "MATH":
                    blender_path = mix_node.inputs[1].path_from_id() + ".default_value"
                    group_name = 'Material'
                    num_components = 1
                else:
                    print("Error, something is wrong, we didn't detect adding a Mix Node because of Pointers")
            else:
                blender_path = transmission_socket.socket.path_from_id() + ".default_value"
                group_name = 'Material'
                num_components = 1

        if len(pointer_tab) == 7 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_clearcoat" and \
                pointer_tab[5] == "clearcoatNormalTexture" and \
                pointer_tab[6] == "scale":
            result = from_socket(
                get_socket(asset['blender_nodetree'], 'Coat Normal'),
                FilterByType(bpy.types.ShaderNodeNormalMap))
            if result:
                blender_path = result[0].shader_node.inputs['Strength'].path_from_id() + ".default_value"
                group_name = 'Material'
                num_components = 1

        if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_clearcoat" and \
                pointer_tab[5] == "clearcoatFactor":
            clearcoat_socket = get_socket(asset['blender_nodetree'], 'Coat Weight')
            if clearcoat_socket.socket.is_linked:
                mix_node = clearcoat_socket.socket.links[0].from_node
                if mix_node.type == "MATH":
                    blender_path = mix_node.inputs[1].path_from_id() + ".default_value"
                    group_name = 'Material'
                    num_components = 1
                else:
                    print("Error, something is wrong, we didn't detect adding a Mix Node because of Pointers")
            else:
                blender_path = clearcoat_socket.path_from_id() + ".default_value"
                group_name = 'Material'
                num_components = 1

        if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_clearcoat" and \
                pointer_tab[5] == "clearcoatRoughnessFactor":
            clearcoat_roughness_socket = get_socket(asset['blender_nodetree'], 'Coat Roughness')
            if clearcoat_roughness_socket.socket.is_linked:
                mix_node = clearcoat_roughness_socket.socket.links[0].from_node
                if mix_node.type == "MATH":
                    blender_path = mix_node.inputs[1].path_from_id() + ".default_value"
                    group_name = 'Material'
                    num_components = 1
                else:
                    print("Error, something is wrong, we didn't detect adding a Mix Node because of Pointers")
            else:
                blender_path = clearcoat_roughness_socket.path_from_id() + ".default_value"
                group_name = 'Material'
                num_components = 1

        if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_sheen" and \
                pointer_tab[5] == "sheenColorFactor":
            sheen_color_socket = get_socket(asset['blender_nodetree'], 'Sheen Tint')
            if sheen_color_socket.socket.is_linked:
                mix_node = sheen_color_socket.socket.links[0].from_node
                if mix_node.type == "MIX":
                    blender_path = mix_node.inputs[7].path_from_id() + ".default_value"
                    group_name = 'Material'
                    num_components = 3
                else:
                    print("Error, something is wrong, we didn't detect adding a Mix Node because of Pointers")
            else:
                blender_path = sheen_color_socket.path_from_id() + ".default_value"
                group_name = 'Material'
                num_components = 3

        if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_sheen" and \
                pointer_tab[5] == "sheenRoughnessFactor":
            sheen_roughness_socket = get_socket(asset['blender_nodetree'], 'Sheen Roughness')
            if sheen_roughness_socket.socket.is_linked:
                mix_node = sheen_roughness_socket.socket.links[0].from_node
                if mix_node.type == "MATH":
                    blender_path = mix_node.inputs[1].path_from_id() + ".default_value"
                    group_name = 'Material'
                    num_components = 1
                else:
                    print("Error, something is wrong, we didn't detect adding a Mix Node because of Pointers")
            else:
                blender_path = sheen_roughness_socket.socket.path_from_id() + ".default_value"
                group_name = 'Material'
                num_components = 1

        if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_specular" and \
                pointer_tab[5] == "specularFactor":
            specular_socket = get_socket(asset['blender_nodetree'], 'Specular IOR Level')
            if specular_socket.socket.is_linked:
                mix_node = specular_socket.socket.links[0].from_node
                if mix_node.type == "MATH":
                    blender_path = mix_node.inputs[1].path_from_id() + ".default_value"
                    group_name = 'Material'
                    num_components = 1
                else:
                    print("Error, something is wrong, we didn't detect adding a Mix Node because of Pointers")
            else:
                blender_path = specular_socket.socket.path_from_id() + ".default_value"
                group_name = 'Material'
                num_components = 1

            old_values = values.copy()
            for idx, i in enumerate(old_values):
                values[idx] = [i[0] / 2.0]

        if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_specular" and \
                pointer_tab[5] == "specularColorFactor":
            specular_color_socket = get_socket(asset['blender_nodetree'], 'Specular Tint')
            if specular_color_socket.socket.is_linked:
                mix_node = specular_color_socket.socket.links[0].from_node
                if mix_node.type == "MIX":
                    blender_path = mix_node.inputs[7].path_from_id() + ".default_value"
                    group_name = 'Material'
                    num_components = 3
                else:
                    print("Error, something is wrong, we didn't detect adding a Mix Node because of Pointers")
            else:
                blender_path = specular_color_socket.socket.path_from_id() + ".default_value"
                group_name = 'Material'
                num_components = 3

        if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_anisotropy" and \
                pointer_tab[5] == "anisotropyStrength":
            anisotropy_socket = get_socket(asset['blender_nodetree'], 'Anisotropic')
            if anisotropy_socket.socket.is_linked:
                mix_node = anisotropy_socket.socket.links[0].from_node
                if mix_node.type == "MATH":
                    blender_path = mix_node.inputs[1].path_from_id() + ".default_value"
                    group_name = 'Material'
                    num_components = 1
                else:
                    print("Error, something is wrong, we didn't detect adding a Mix Node because of Pointers")
            else:
                blender_path = anisotropy_socket.socket.path_from_id() + ".default_value"
                group_name = 'Material'
                num_components = 1

        if len(pointer_tab) == 6 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "extensions" and \
                pointer_tab[4] == "KHR_materials_anisotropy" and \
                pointer_tab[5] == "anisotropyRotation":
            anisotropy_rotation_socket = get_socket(asset['blender_nodetree'], 'Anisotropic Rotation')
            if anisotropy_rotation_socket.socket.is_linked:
                mix_node = anisotropy_rotation_socket.socket.links[0].from_node
                if mix_node.type == "MATH":
                    blender_path = mix_node.inputs[1].path_from_id() + ".default_value"
                    group_name = 'Material'
                    num_components = 1
                else:
                    print("Error, something is wrong, we didn't detect adding a Mix Node because of Pointers")
            else:
                blender_path = anisotropy_rotation_socket.socket.path_from_id() + ".default_value"
                group_name = 'Material'
                num_components = 1

        if blender_path is None:
            gltf.log.warning("Unsupported animation target: %s" % pointer_tab)
            return  # Should not happen if all specification is managed

        fps = bpy.context.scene.render.fps

        coords = [0] * (2 * len(keys))
        coords[::2] = (key[0] * fps for key in keys)

        for i in range(0, num_components):
            coords[1::2] = (vals[i] for vals in values)
            make_fcurve(
                action,
                slot,
                coords,
                data_path=blender_path,
                index=i,
                group_name=group_name,
                interpolation=animation.samplers[channel.sampler].interpolation,
            )

        # For baseColorFactor, we also need to add keyframes to alpha socket
        if len(pointer_tab) == 5 and pointer_tab[1] == "materials" and \
                pointer_tab[3] == "pbrMetallicRoughness" and \
                pointer_tab[4] == "baseColorFactor":

            if is_unlit is False:
                alpha_socket = get_socket(asset.blender_nodetree, "Alpha")
            else:
                unlit_info = detect_shadeless_material(asset.blender_nodetree, {})
                if 'alpha_socket' in unlit_info:
                    alpha_socket = unlit_info['alpha_socket']
            if alpha_socket.socket.is_linked:
                # We need to find the correct node value to animate (An Mix Factor node)
                mix_node = alpha_socket.socket.links[0].from_node
                if mix_node.type == "MATH":
                    blender_path = mix_node.inputs[1].path_from_id() + ".default_value"
                else:
                    print("Error, something is wrong, we didn't detect adding a Mix Node because of Pointers")
            else:
                blender_path = alpha_socket.socket.path_from_id() + ".default_value"

            coords[1::2] = (vals[3] for vals in values)
            make_fcurve(
                action,
                slot,
                coords,
                data_path=blender_path,
                index=0,
                group_name=group_name,
                interpolation=animation.samplers[channel.sampler].interpolation,
            )

    @staticmethod
    def get_or_create_action_and_slot(gltf, anim_idx, asset, asset_idx, asset_type, name_=None):
        animation = gltf.data.animations[anim_idx]

        if asset_type == "CAMERA":
            name = asset.name
            stash = asset.blender_object_data
            target_id_type = "CAMERA"
        elif asset_type == "LIGHT":
            name = asset['name']
            stash = asset['blender_object_data']
            target_id_type = "LIGHT"
        elif asset_type == "MATERIAL":
            name = asset.name
            stash = asset.blender_mat
            target_id_type = "MATERIAL"
        elif asset_type == "NODETREE":
            name = name_ if name_ is not None else asset.name
            stash = asset.blender_nodetree
            target_id_type = "NODETREE"
        elif asset_type == "TEX_TRANSFORM":
            name = name_ if name_ is not None else asset.name
            stash = asset['blender_nodetree']
            target_id_type = "NODETREE"
        elif asset_type == "EXT":
            name = name_ if name_ is not None else asset.name
            stash = asset['blender_nodetree']
            target_id_type = "NODETREE"

        objects = gltf.action_cache.get(anim_idx)
        if not objects:
            # Nothing exists yet for this glTF animation
            gltf.action_cache[anim_idx] = {}

            # So create a new action
            action = bpy.data.actions.new(animation.track_name)
            action.layers.new('layer0')
            action.layers[0].strips.new(type='KEYFRAME')

            gltf.action_cache[anim_idx]['action'] = action
            gltf.action_cache[anim_idx]['object_slots'] = {}

        # We now have an action for the animation, check if we have slots for this object
        slots = gltf.action_cache[anim_idx]['object_slots'].get(name)
        if not slots:

            # We have no slots, create one

            action = gltf.action_cache[anim_idx]['action']
            slot = action.slots.new(stash.id_type, "Slot")
            gltf.needs_stash.append((stash, action, slot))

            action.layers[0].strips[0].channelbags.new(slot)

            gltf.action_cache[anim_idx]['object_slots'][name] = {}
            gltf.action_cache[anim_idx]['object_slots'][name][slot.target_id_type] = (action, slot)
        else:
            # We have slots, check if we have the right slot (based on target_id_type)
            ac_sl = slots.get(target_id_type)
            if not ac_sl:
                action = gltf.action_cache[anim_idx]['action']
                slot = action.slots.new(stash.id_type, "Slot")
                gltf.needs_stash.append((stash, action, slot))

                action.layers[0].strips[0].channelbags.new(slot)

                gltf.action_cache[anim_idx]['object_slots'][name][slot.target_id_type] = (action, slot)
            else:
                action, slot = ac_sl

        # We now have action and slot, we can return the right slot
        return action, slot
