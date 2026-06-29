# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from ....io.com import gltf2_io
from ..tree import VExportNode
from .drivers import get_sk_drivers
from .sampled.armature.channels import gather_armature_sampled_channels
from .sampled.object.channels import gather_object_sampled_channels
from .sampled.shapekeys.channels import gather_sk_sampled_channels
from .sampled.data.channels import gather_data_sampled_channels
from .anim_utils import link_samplers, add_slide_data
from .anim_extra_utils import gather_blender_element


def gather_scene_animations(export_settings):

    # Even if we don't have any animation,
    # We are going to bake.
    # Here are some cases where there are no action in bpy.data.actions, but we still have to bake:
    # - GN instances, that can be animated
    # - Everything that can be animated by drivers (animation pointer, but not only)

    total_channels = []
    animations = []

    start_frame = bpy.context.scene.frame_start
    end_frame = bpy.context.scene.frame_end

    # The following options has no impact:
    # - We force sampling & baking
    # - Export_frame_range --> Because this is the case for SCENE mode, because we bake all scene frame range
    # - CROP or SLIDE --> Scene don't have negative frames

    # This mode will bake all objects like there are in the scene
    vtree = export_settings['vtree']
    for obj_uuid in vtree.get_all_objects():

        # Do not manage not exported objects
        if vtree.nodes[obj_uuid].node is None:
            if export_settings['gltf_armature_object_remove'] is True:
                # Manage armature object, as this is the object that has the animation
                if not vtree.nodes[obj_uuid].blender_object:
                    continue
            else:
                continue

        if export_settings['vtree'].nodes[obj_uuid].blender_type == VExportNode.COLLECTION:
            continue

        # blender_object can be None for GN instances
        blender_object = export_settings['vtree'].nodes[obj_uuid].blender_object

        export_settings['ranges'][obj_uuid] = {}
        export_settings['ranges'][obj_uuid][obj_uuid] = {'start': start_frame, 'end': end_frame}
        if blender_object and blender_object.type == "ARMATURE":
            # Manage sk drivers
            obj_drivers = get_sk_drivers(obj_uuid, export_settings)
            for obj_dr in obj_drivers:
                if obj_dr not in export_settings['ranges']:
                    export_settings['ranges'][obj_dr] = {}
                export_settings['ranges'][obj_dr][obj_uuid + "_" + obj_uuid] = {}
                export_settings['ranges'][obj_dr][obj_uuid + "_" + obj_uuid]['start'] = start_frame
                export_settings['ranges'][obj_dr][obj_uuid + "_" + obj_uuid]['end'] = end_frame

        if export_settings['gltf_anim_slide_to_zero'] is True and start_frame > 0:
            add_slide_data(start_frame, obj_uuid, obj_uuid, export_settings)

        # Perform baking animation export

        if blender_object and blender_object.type != "ARMATURE":
            # We have to check if this is a skinned mesh, because we don't have to force animation baking on this case
            if export_settings['vtree'].nodes[obj_uuid].skin is None:
                # Setting slot_identifier to None, always
                channels, _ = gather_object_sampled_channels(obj_uuid, obj_uuid, None, export_settings)
                if channels is not None:
                    total_channels.extend(channels)

            if export_settings['gltf_morph_anim'] and blender_object.type == "MESH" \
                    and blender_object.data is not None \
                    and blender_object.data.shape_keys is not None:

                # We must ignore sk for meshes that are driven by armature parent
                ignore_sk = False
                if export_settings['vtree'].nodes[obj_uuid].parent_uuid is not None \
                        and export_settings['vtree'].nodes[export_settings['vtree'].nodes[obj_uuid].parent_uuid].blender_type == VExportNode.ARMATURE:
                    obj_drivers = get_sk_drivers(export_settings['vtree'].nodes[obj_uuid].parent_uuid, export_settings)
                    if obj_uuid in obj_drivers:
                        ignore_sk = True

                if ignore_sk is False:
                    # Setting slot_identifier to None, always
                    channels = gather_sk_sampled_channels('SK', obj_uuid, obj_uuid, None, export_settings)
                    if channels is not None:
                        total_channels.extend(channels)
        elif blender_object is None:
            # This is GN instances
            # Currently, not checking if this instance is skinned.... #TODO
            # No action / slot for GN instances
            # Setting slot_identifier to None, always
            channels, _ = gather_object_sampled_channels(obj_uuid, obj_uuid, None, export_settings)
            if channels is not None:
                total_channels.extend(channels)
        else:
            # Setting slot_identifier to None, always
            channels, _ = gather_armature_sampled_channels(obj_uuid, obj_uuid, None, export_settings)
            if channels is not None:
                total_channels.extend(channels)

        if export_settings['gltf_anim_scene_split_object'] is True:
            if len(total_channels) > 0:
                animation = gltf2_io.Animation(
                    channels=total_channels,
                    extensions=None,
                    extras=None,
                    name=blender_object.name if blender_object else "GN Instance",
                    samplers=[]
                )
                link_samplers(animation, export_settings)
                animations.append(animation)

            total_channels = []

    if export_settings['gltf_export_anim_pointer'] is True:
        # Export now KHR_animation_pointer for materials
        for mat in export_settings['KHR_animation_pointer'][None]['materials'].keys():
            if len(export_settings['KHR_animation_pointer'][None]['materials'][mat]['paths']) == 0:
                continue

            blender_material = export_settings['material_identifiers'][mat]['blender']

            export_settings['ranges'][mat] = {}
            export_settings['ranges'][mat][mat] = {
                'start': start_frame, 'end': end_frame}

            if export_settings['gltf_anim_slide_to_zero'] is True and start_frame > 0:
                add_slide_data(start_frame, mat, mat, export_settings, add_drivers=False)

            # Setting slot_identifier to None, always
            channels = gather_data_sampled_channels(None, 'materials', mat, mat, None, None, export_settings)
            if channels is not None:
                total_channels.extend(channels)

        if export_settings['gltf_anim_scene_split_object'] is True:
            if len(total_channels) > 0:
                animation = gltf2_io.Animation(
                    channels=total_channels,
                    extensions=None,
                    extras=None,
                    name=blender_material.name,
                    samplers=[]
                )
                link_samplers(animation, export_settings)
                animations.append(animation)

            total_channels = []

        # Export now KHR_animation_pointer for lights
        for light in export_settings['KHR_animation_pointer'][None]['lights'].keys():
            if len(export_settings['KHR_animation_pointer'][None]['lights'][light]['paths']) == 0:
                continue

            blender_light = [alight for alight in bpy.data.lights if id(alight) == light][0]

            export_settings['ranges'][light] = {}
            export_settings['ranges'][light][light] = {'start': start_frame, 'end': end_frame}

            if export_settings['gltf_anim_slide_to_zero'] is True and start_frame > 0:
                add_slide_data(start_frame, light, light, export_settings, add_drivers=False)

            # Setting slot_identifier to None, always
            channels = gather_data_sampled_channels(None, 'lights', light, light, None, None, export_settings)
            if channels is not None:
                total_channels.extend(channels)

            if export_settings['gltf_anim_scene_split_object'] is True:
                if len(total_channels) > 0:
                    animation = gltf2_io.Animation(
                        channels=total_channels,
                        extensions=None,
                        extras=None,
                        name=blender_light.name,
                        samplers=[]
                    )
                    link_samplers(animation, export_settings)
                    animations.append(animation)

                total_channels = []

        # Export now KHR_animation_pointer for cameras
        for cam in export_settings['KHR_animation_pointer'][None]['cameras'].keys():
            if len(export_settings['KHR_animation_pointer'][None]['cameras'][cam]['paths']) == 0:
                continue

            blender_camera = [camera for camera in bpy.data.cameras if id(camera) == cam][0]

            export_settings['ranges'][cam] = {}
            export_settings['ranges'][cam][cam] = {'start': start_frame, 'end': end_frame}

            if export_settings['gltf_anim_slide_to_zero'] is True and start_frame > 0:
                add_slide_data(start_frame, cam, cam, export_settings, add_drivers=False)

            # Setting slot_identifier to None, always
            channels = gather_data_sampled_channels(None, 'cameras', cam, cam, None, None, export_settings)
            if channels is not None:
                total_channels.extend(channels)

            if export_settings['gltf_anim_scene_split_object'] is True:
                if len(total_channels) > 0:
                    animation = gltf2_io.Animation(
                        channels=total_channels,
                        extensions=None,
                        extras=None,
                        name=blender_camera.name,
                        samplers=[]
                    )
                    link_samplers(animation, export_settings)
                    animations.append(animation)

                total_channels = []

        # Export now KHR_animation_pointer for extras
        for extra_type in export_settings['KHR_animation_pointer']['extras'].keys():
            for extra in export_settings['KHR_animation_pointer']['extras'][extra_type].keys():
                if len(export_settings['KHR_animation_pointer']['extras'][extra_type][extra]['paths']) == 0:
                    continue

                if extra_type == "bones":
                    continue  # Already managed in armature

                blender_element, _, _ = gather_blender_element('extras', extra_type, extra, export_settings)

                if blender_element is None:
                    continue

                export_settings['ranges'][extra] = {}
                export_settings['ranges'][extra][extra] = {'start': start_frame, 'end': end_frame}
                if export_settings['gltf_anim_slide_to_zero'] is True and start_frame > 0:
                    add_slide_data(start_frame, extra, extra, export_settings, add_drivers=False)

                # Setting slot_identifier to None, always
                channels = gather_data_sampled_channels("extras", extra_type, extra, extra, None, None, export_settings)
                if channels is not None:
                    total_channels.extend(channels)

                if export_settings['gltf_anim_scene_split_object'] is True:
                    if len(total_channels) > 0:
                        animation = gltf2_io.Animation(
                            channels=total_channels,
                            extensions=None,
                            extras=None,
                            name=blender_element.name if blender_element else "Extra " + str(extra),
                            samplers=[]
                        )
                        link_samplers(animation, export_settings)
                        animations.append(animation)

                    total_channels = []

    if export_settings['gltf_anim_scene_split_object'] is False:
        if len(total_channels) > 0:
            animation = gltf2_io.Animation(
                channels=total_channels,
                extensions=None,
                extras=None,
                name=bpy.context.scene.name,
                samplers=[]
            )
            link_samplers(animation, export_settings)
            animations.append(animation)

    return animations
