# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from math import tan
from ..com.extras import set_extras
from ...io.imp.user_extensions import import_user_extensions


class BlenderCamera():
    """Blender Camera."""
    def __new__(cls, *args, **kwargs):
        raise RuntimeError("%s should not be instantiated" % cls)

    @staticmethod
    def create(gltf, vnode, camera_id):
        """Camera creation."""
        pycamera = gltf.data.cameras[camera_id]

        import_user_extensions('gather_import_camera_before_hook', gltf, vnode, pycamera)

        if not pycamera.name:
            pycamera.name = "Camera"

        cam = bpy.data.cameras.new(pycamera.name)
        set_extras(cam, pycamera.extras)

        # Blender create a perspective camera by default
        if pycamera.type == "orthographic":
            cam.type = "ORTHO"

            cam.ortho_scale = max(pycamera.orthographic.xmag, pycamera.orthographic.ymag) * 2

            cam.clip_start = pycamera.orthographic.znear
            cam.clip_end = pycamera.orthographic.zfar

            # Store multiple channel data, as we will need all channels to convert to
            # blender data when animated by KHR_animation_pointer
            if gltf.data.extensions_used is not None and "KHR_animation_pointer" in gltf.data.extensions_used:
                if len(pycamera.animations) > 0:
                    for anim_idx in pycamera.animations.keys():
                        for channel_idx in pycamera.animations[anim_idx]:
                            channel = gltf.data.animations[anim_idx].channels[channel_idx]
                            pointer_tab = channel.target.extensions["KHR_animation_pointer"]["pointer"].split("/")
                            if len(pointer_tab) == 5 and pointer_tab[1] == "cameras" and \
                                    pointer_tab[3] == "orthographic" and \
                                    pointer_tab[4] in ["xmag", "ymag"]:
                                # Store multiple channel data, as we will need all channels to convert to
                                # blender data when animated
                                if not hasattr(pycamera, "multiple_channels_mag"):
                                    pycamera.multiple_channels_mag = {}
                                pycamera.multiple_channels_mag[pointer_tab[4]] = (anim_idx, channel_idx)

        else:
            cam.angle_y = pycamera.perspective.yfov
            cam.lens_unit = "FOV"
            cam.sensor_fit = "VERTICAL"

            # TODO: fov/aspect ratio

            cam.clip_start = pycamera.perspective.znear
            if pycamera.perspective.zfar is not None:
                cam.clip_end = pycamera.perspective.zfar
            else:
                # Infinite projection
                cam.clip_end = 1e12  # some big number

        pycamera.blender_object_data = cam  # Needed in case of KHR_animation_pointer

        return cam

    @staticmethod
    def calc_lens_from_fov(gltf, input_value, sensor):
        return (sensor / 2.0) / tan(input_value * 0.5)
