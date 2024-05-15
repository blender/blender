# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0


from .gltf2_blender_gather_action import gather_actions_animations
from .gltf2_blender_gather_scene_animation import gather_scene_animations
from .gltf2_blender_gather_tracks import gather_tracks_animations


def gather_animations(export_settings):

    # Reinit stored data
    export_settings['ranges'] = {}
    export_settings['slide'] = {}

    if export_settings['gltf_animation_mode'] in ["ACTIVE_ACTIONS", "ACTIONS", "BROADCAST"]:
        return gather_actions_animations(export_settings)
    elif export_settings['gltf_animation_mode'] == "SCENE":
        return gather_scene_animations(export_settings)
    elif export_settings['gltf_animation_mode'] == "NLA_TRACKS":
        return gather_tracks_animations(export_settings)
