# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import math
from typing import Optional, List, Dict, Any
from ...io.com import lights_punctual as gltf2_io_lights_punctual
from ...io.com import debug as gltf2_io_debug
from ..com.extras import generate_extras
from ..com.conversion import PBR_WATTS_TO_LUMENS
from ..com.blender_default import LIGHTS
from .cache import cached
from . import light_spots as gltf2_blender_gather_light_spots
from .material import search_node_tree


@cached
def gather_lights_punctual(blender_lamp, export_settings) -> Optional[Dict[str, Any]]:

    export_settings['current_paths'] = {}  # For KHR_animation_pointer

    if not __filter_lights_punctual(blender_lamp, export_settings):
        return None

    light = gltf2_io_lights_punctual.Light(
        color=__gather_color(blender_lamp, export_settings),
        intensity=__gather_intensity(blender_lamp, export_settings),
        spot=__gather_spot(blender_lamp, export_settings),
        type=__gather_type(blender_lamp, export_settings),
        range=__gather_range(blender_lamp, export_settings),
        name=__gather_name(blender_lamp, export_settings),
        extensions=__gather_extensions(blender_lamp, export_settings),
        extras=__gather_extras(blender_lamp, export_settings)
    )

    return light.to_dict()


def __filter_lights_punctual(blender_lamp, export_settings) -> bool:
    if blender_lamp.type in ["HEMI", "AREA"]:
        export_settings['log'].warning("Unsupported light source {}".format(blender_lamp.type))
        return False

    return True


def __gather_color(blender_lamp, export_settings) -> Optional[List[float]]:
    emission_node = __get_cycles_emission_node(blender_lamp)
    if emission_node is not None:

        # Store data for KHR_animation_pointer
        path_ = {}
        path_['length'] = 3
        path_['path'] = "/extensions/KHR_lights_punctual/lights/XXX/color"
        export_settings['current_paths']["node_tree." + emission_node.inputs["Color"].path_from_id() +
                                         ".default_value"] = path_

        return list(emission_node.inputs["Color"].default_value)[:3]

    # Store data for KHR_animation_pointer
    path_ = {}
    path_['length'] = 3
    path_['path'] = "/extensions/KHR_lights_punctual/lights/XXX/color"
    export_settings['current_paths']['color'] = path_

    return list(blender_lamp.color)


def __gather_intensity(blender_lamp, export_settings) -> Optional[float]:
    emission_node = __get_cycles_emission_node(blender_lamp)
    if emission_node is not None:
        if blender_lamp.type != 'SUN':
            # When using cycles, the strength should be influenced by a LightFalloff node
            result = search_node_tree.from_socket(
                search_node_tree.NodeSocket(emission_node.inputs.get("Strength"), blender_lamp.node_tree),
                search_node_tree.FilterByType(bpy.types.ShaderNodeLightFalloff)
            )
            if result:
                quadratic_falloff_node = result[0].shader_node
                emission_strength = quadratic_falloff_node.inputs["Strength"].default_value / (math.pi * 4.0)

                # Store data for KHR_animation_pointer
                path_ = {}
                path_['length'] = 1
                path_['path'] = "/extensions/KHR_lights_punctual/lights/XXX/intensity"
                path_['lamp_type'] = blender_lamp.type
                export_settings['current_paths']["node_tree." +
                                                 quadratic_falloff_node.inputs["Strength"].path_from_id() +
                                                 ".default_value"] = path_

            else:
                export_settings['log'].warning('No quadratic light falloff node attached to emission strength property')

                path_ = {}
                path_['length'] = 1
                path_['path'] = "/extensions/KHR_lights_punctual/lights/XXX/intensity"
                path_['lamp_type'] = blender_lamp.type
                export_settings['current_paths']["energy"] = path_

                emission_strength = blender_lamp.energy
        else:
            emission_strength = emission_node.inputs["Strength"].default_value

            path_ = {}
            path_['length'] = 1
            path_['path'] = "/extensions/KHR_lights_punctual/lights/XXX/intensity"
            path_['lamp_type'] = blender_lamp.type
            export_settings['current_paths']["node_tree." +
                                             emission_node.inputs["Strength"].path_from_id() +
                                             ".default_value"] = path_

    else:
        emission_strength = blender_lamp.energy

        path_ = {}
        path_['length'] = 1
        path_['path'] = "/extensions/KHR_lights_punctual/lights/XXX/intensity"
        path_['lamp_type'] = blender_lamp.type
        export_settings['current_paths']["energy"] = path_

    if export_settings['gltf_lighting_mode'] == 'RAW':
        return emission_strength
    else:
        # Assume at this point the computed strength is still in the appropriate
        # watt-related SI unit, which if everything up to here was done with
        # physical basis it hopefully should be.
        if blender_lamp.type == 'SUN':  # W/m^2 in Blender to lm/m^2 for GLTF/KHR_lights_punctual.
            emission_luminous = emission_strength
        else:
            # Other than directional, only point and spot lamps are supported by GLTF.
            # In Blender, points are omnidirectional W, and spots are specified as if they're points.
            # Point and spot should both be lm/r^2 in GLTF.
            emission_luminous = emission_strength / (4 * math.pi)
        if export_settings['gltf_lighting_mode'] == 'SPEC':
            emission_luminous *= PBR_WATTS_TO_LUMENS
        elif export_settings['gltf_lighting_mode'] == 'COMPAT':
            pass  # Just so we have an exhaustive tree to catch bugged values.
        else:
            raise ValueError(export_settings['gltf_lighting_mode'])
        return emission_luminous


def __gather_spot(blender_lamp, export_settings) -> Optional[gltf2_io_lights_punctual.LightSpot]:
    if blender_lamp.type == "SPOT":
        return gltf2_blender_gather_light_spots.gather_light_spot(blender_lamp, export_settings)
    return None


def __gather_type(blender_lamp, _) -> str:
    return LIGHTS[blender_lamp.type]


def __gather_range(blender_lamp, export_settings) -> Optional[float]:
    if blender_lamp.use_custom_distance:

        path_ = {}
        path_['length'] = 1
        path_['path'] = "/extensions/KHR_lights_punctual/lights/XXX/range"
        export_settings['current_paths']["cutoff_distance"] = path_

        return blender_lamp.cutoff_distance
    return None


def __gather_name(blender_lamp, export_settings) -> Optional[str]:
    return blender_lamp.name


def __gather_extensions(blender_lamp, export_settings) -> Optional[dict]:
    return None


def __gather_extras(blender_lamp, export_settings) -> Optional[Any]:
    if export_settings['gltf_extras']:
        return generate_extras(blender_lamp)
    return None


def __get_cycles_emission_node(blender_lamp) -> Optional[bpy.types.ShaderNodeEmission]:
    if blender_lamp.use_nodes and blender_lamp.node_tree:
        for currentNode in blender_lamp.node_tree.nodes:
            is_shadernode_output = isinstance(currentNode, bpy.types.ShaderNodeOutputLight)
            if is_shadernode_output:
                if not currentNode.is_active_output:
                    continue
                result = search_node_tree.from_socket(
                    search_node_tree.NodeSocket(currentNode.inputs.get("Surface"), blender_lamp.node_tree),
                    search_node_tree.FilterByType(bpy.types.ShaderNodeEmission)
                )
                if not result:
                    continue
                return result[0].shader_node
    return None
