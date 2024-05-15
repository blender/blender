# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import math
from ...io.com import gltf2_io
from ...blender.com.gltf2_blender_conversion import yvof_blender_to_gltf
from ...io.exp.gltf2_io_user_extensions import export_user_extensions
from ..com.gltf2_blender_extras import generate_extras
from .gltf2_blender_gather_cache import cached


@cached
def gather_camera(blender_camera, export_settings):
    if not __filter_camera(blender_camera, export_settings):
        return None

    export_settings['current_paths'] = {}  # For KHR_animation_pointer

    camera = gltf2_io.Camera(
        extensions=__gather_extensions(blender_camera, export_settings),
        extras=__gather_extras(blender_camera, export_settings),
        name=__gather_name(blender_camera, export_settings),
        orthographic=__gather_orthographic(blender_camera, export_settings),
        perspective=__gather_perspective(blender_camera, export_settings),
        type=__gather_type(blender_camera, export_settings)
    )

    export_user_extensions('gather_camera_hook', export_settings, camera, blender_camera)

    return camera


def __filter_camera(blender_camera, export_settings):
    return bool(__gather_type(blender_camera, export_settings))


def __gather_extensions(blender_camera, export_settings):
    return None


def __gather_extras(blender_camera, export_settings):
    if export_settings['gltf_extras']:
        return generate_extras(blender_camera)
    return None


def __gather_name(blender_camera, export_settings):
    return blender_camera.name


def __gather_orthographic(blender_camera, export_settings):
    if __gather_type(blender_camera, export_settings) == "orthographic":
        orthographic = gltf2_io.CameraOrthographic(
            extensions=None,
            extras=None,
            xmag=None,
            ymag=None,
            zfar=None,
            znear=None
        )

        _render = bpy.context.scene.render
        scene_x = _render.resolution_x * _render.pixel_aspect_x
        scene_y = _render.resolution_y * _render.pixel_aspect_y
        scene_square = max(scene_x, scene_y)
        del _render

        # `Camera().ortho_scale` (and also FOV FTR) maps to the maximum of either image width or image height— This is the box that gets shown from camera view with the checkbox `.show_sensor = True`.

        orthographic.xmag = blender_camera.ortho_scale * (scene_x / scene_square) / 2
        orthographic.ymag = blender_camera.ortho_scale * (scene_y / scene_square) / 2

        orthographic.znear = blender_camera.clip_start
        orthographic.zfar = blender_camera.clip_end

        # Store data for KHR_animation_pointer
        path_ = {}
        path_['length'] = 1
        path_['path'] = "/cameras/XXX/orthographic/xmag"
        export_settings['current_paths']['ortho_scale_x'] = path_

        path_ = {}
        path_['length'] = 1
        path_['path'] = "/cameras/XXX/orthographic/ymag"
        export_settings['current_paths']['ortho_scale_y'] = path_

        path_ = {}
        path_['length'] = 1
        path_['path'] = "/cameras/XXX/orthographic/zfar"
        export_settings['current_paths']['clip_end'] = path_

        path_ = {}
        path_['length'] = 1
        path_['path'] = "/cameras/XXX/orthographic/znear"
        export_settings['current_paths']['clip_start'] = path_

        return orthographic
    return None


def __gather_perspective(blender_camera, export_settings):
    if __gather_type(blender_camera, export_settings) == "perspective":
        perspective = gltf2_io.CameraPerspective(
            aspect_ratio=None,
            extensions=None,
            extras=None,
            yfov=None,
            zfar=None,
            znear=None
        )

        _render = bpy.context.scene.render
        width = _render.pixel_aspect_x * _render.resolution_x
        height = _render.pixel_aspect_y * _render.resolution_y
        perspective.aspect_ratio = width / height
        del _render

        perspective.yfov = yvof_blender_to_gltf(blender_camera.angle, width, height, blender_camera.sensor_fit)

        perspective.znear = blender_camera.clip_start
        perspective.zfar = blender_camera.clip_end

        path_ = {}
        path_['length'] = 1
        path_['path'] = "/cameras/XXX/perspective/zfar"
        export_settings['current_paths']['clip_end'] = path_

        path_ = {}
        path_['length'] = 1
        path_['path'] = "/cameras/XXX/perspective/znear"
        export_settings['current_paths']['clip_start'] = path_

        path_ = {}
        path_['length'] = 1
        path_['path'] = "/cameras/XXX/perspective/yfov"
        path_['sensor_fit'] = 'sensor_fit'
        export_settings['current_paths']['angle'] = path_

        # aspect ratio is not animatable in blender

        return perspective
    return None


def __gather_type(blender_camera, export_settings):
    if blender_camera.type == 'PERSP':
        return "perspective"
    elif blender_camera.type == 'ORTHO':
        return "orthographic"
    return None
