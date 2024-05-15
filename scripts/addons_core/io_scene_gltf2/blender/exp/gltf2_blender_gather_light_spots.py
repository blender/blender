# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from typing import Optional
from ...io.com import gltf2_io_lights_punctual


def gather_light_spot(blender_lamp, export_settings) -> Optional[gltf2_io_lights_punctual.LightSpot]:

    if not __filter_light_spot(blender_lamp, export_settings):
        return None

    spot = gltf2_io_lights_punctual.LightSpot(
        inner_cone_angle=__gather_inner_cone_angle(blender_lamp, export_settings),
        outer_cone_angle=__gather_outer_cone_angle(blender_lamp, export_settings)
    )
    return spot


def __filter_light_spot(blender_lamp, _) -> bool:
    if blender_lamp.type != "SPOT":
        return False

    return True


def __gather_inner_cone_angle(blender_lamp, export_settings) -> Optional[float]:
    angle = blender_lamp.spot_size * 0.5

    path_ = {}
    path_['length'] = 1
    path_['path'] = "/extensions/KHR_lights_punctual/lights/XXX/spot.innerConeAngle"
    path_['additional_path'] = "spot_size"
    export_settings['current_paths']["spot_blend"] = path_

    return angle - angle * blender_lamp.spot_blend


def __gather_outer_cone_angle(blender_lamp, export_settings) -> Optional[float]:

    path_ = {}
    path_['length'] = 1
    path_['path'] = "/extensions/KHR_lights_punctual/lights/XXX/spot.outerConeAngle"
    export_settings['current_paths']["spot_size"] = path_

    return blender_lamp.spot_size * 0.5
