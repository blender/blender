#
# Copyright 2011, Blender Foundation.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#

# <pep8 compliant>

import bpy
from bpy.props import (BoolProperty,
                       EnumProperty,
                       FloatProperty,
                       IntProperty,
                       PointerProperty)

import math

from . import enums


class CyclesRenderSettings(bpy.types.PropertyGroup):
    @classmethod
    def register(cls):
        bpy.types.Scene.cycles = PointerProperty(
                name="Cycles Render Settings",
                description="Cycles render settings",
                type=cls,
                )
        cls.device = EnumProperty(
                name="Device",
                description="Device to use for rendering",
                items=enums.devices,
                default='CPU',
                )
        cls.feature_set = EnumProperty(
                name="Feature Set",
                description="Feature set to use for rendering",
                items=enums.feature_set,
                default='SUPPORTED',
                )
        cls.shading_system = EnumProperty(
                name="Shading System",
                description="Shading system to use for rendering",
                items=enums.shading_systems,
                default='GPU_COMPATIBLE',
                )

        cls.samples = IntProperty(
                name="Samples",
                description="Number of samples to render for each pixel",
                min=1, max=2147483647,
                default=10,
                )
        cls.preview_samples = IntProperty(
                name="Preview Samples",
                description="Number of samples to render in the viewport, unlimited if 0",
                min=0, max=2147483647,
                default=10,
                )
        cls.preview_pause = BoolProperty(
                name="Pause Preview",
                description="Pause all viewport preview renders",
                default=False,
                )

        cls.no_caustics = BoolProperty(
                name="No Caustics",
                description="Leave out caustics, resulting in a darker image with less noise",
                default=False,
                )
        cls.blur_caustics = FloatProperty(
                name="Blur Caustics",
                description="Blur caustics to reduce noise",
                min=0.0, max=1.0,
                default=0.0,
                )

        cls.min_bounces = IntProperty(
                name="Min Bounces",
                description="Minimum number of bounces, setting this lower than the maximum enables probalistic path termination (faster but noisier)",
                min=0, max=1024,
                default=3,
                )
        cls.max_bounces = IntProperty(
                name="Max Bounces",
                description="Total maximum number of bounces",
                min=0, max=1024,
                default=8,
                )

        cls.diffuse_bounces = IntProperty(
                name="Diffuse Bounces",
                description="Maximum number of diffuse reflection bounces, bounded by total maximum",
                min=0, max=1024,
                default=128,
                )
        cls.glossy_bounces = IntProperty(
                name="Glossy Bounces",
                description="Maximum number of glossy reflection bounces, bounded by total maximum",
                min=0, max=1024,
                default=128,
                )
        cls.transmission_bounces = IntProperty(
                name="Transmission Bounces",
                description="Maximum number of transmission bounces, bounded by total maximum",
                min=0, max=1024,
                default=128,
                )

        cls.transparent_min_bounces = IntProperty(
                name="Transparent Min Bounces",
                description="Minimum number of transparent bounces, setting this lower than the maximum enables probalistic path termination (faster but noisier)",
                min=0, max=1024,
                default=8,
                )
        cls.transparent_max_bounces = IntProperty(
                name="Transparent Max Bounces",
                description="Maximum number of transparent bounces",
                min=0, max=1024,
                default=8,
                )
        cls.use_transparent_shadows = BoolProperty(
                name="Transparent Shadows",
                description="Use transparency of surfaces for rendering shadows",
                default=True,
                )

        cls.film_exposure = FloatProperty(
                name="Exposure",
                description="Image brightness scale",
                min=0.0, max=10.0,
                default=1.0,
                )
        cls.film_transparent = BoolProperty(
                name="Transparent",
                description="World background is transparent",
                default=False,
                )

        cls.filter_type = EnumProperty(
                name="Filter Type",
                description="Pixel filter type",
                items=enums.filter_types,
                default='GAUSSIAN',
                )
        cls.filter_width = FloatProperty(
                name="Filter Width",
                description="Pixel filter width",
                min=0.01, max=10.0,
                default=1.5,
                )

        cls.seed = IntProperty(
                name="Seed",
                description="Seed value for integrator to get different noise patterns",
                min=0, max=2147483647,
                default=0,
                )

        cls.debug_tile_size = IntProperty(
                name="Tile Size",
                description="",
                min=1, max=4096,
                default=1024,
                )
        cls.debug_min_size = IntProperty(
                name="Min Size",
                description="",
                min=1, max=4096,
                default=64,
                )
        cls.debug_reset_timeout = FloatProperty(
                name="Reset timeout",
                description="",
                min=0.01, max=10.0,
                default=0.1,
                )
        cls.debug_cancel_timeout = FloatProperty(
                name="Cancel timeout",
                description="",
                min=0.01, max=10.0,
                default=0.1,
                )
        cls.debug_text_timeout = FloatProperty(
                name="Text timeout",
                description="",
                min=0.01, max=10.0,
                default=1.0,
                )

        cls.debug_bvh_type = EnumProperty(
                name="Viewport BVH Type",
                description="Choose between faster updates, or faster render",
                items=enums.bvh_types,
                default='DYNAMIC_BVH',
                )
        cls.debug_use_spatial_splits = BoolProperty(
                name="Use Spatial Splits",
                description="Use BVH spatial splits: longer builder time, faster render",
                default=False,
                )
        cls.use_cache = BoolProperty(
                name="Cache BVH",
                description="Cache last built BVH to disk for faster re-render if no geometry changed",
                default=False,
                )

    @classmethod
    def unregister(cls):
        del bpy.types.Scene.cycles


class CyclesCameraSettings(bpy.types.PropertyGroup):
    @classmethod
    def register(cls):
        bpy.types.Camera.cycles = PointerProperty(
                name="Cycles Camera Settings",
                description="Cycles camera settings",
                type=cls,
                )

        cls.aperture_type = EnumProperty(
                name="Aperture Type",
                description="Use F/stop number or aperture radius",
                items=enums.aperture_types,
                default='RADIUS',
                )
        cls.aperture_fstop = FloatProperty(
                name="Aperture F/stop",
                description="F/stop ratio (lower numbers give more defocus, higher numbers give a sharper image)",
                min=0.0, soft_min=0.1, soft_max=64.0,
                default=5.6,
                step=10,
                precision=1,
                )
        cls.aperture_size = FloatProperty(
                name="Aperture Size",
                description="Radius of the aperture for depth of field (higher values give more defocus)",
                min=0.0, soft_max=10.0,
                default=0.0,
                step=1,
                precision=4,
                )
        cls.aperture_blades = IntProperty(
                name="Aperture Blades",
                description="Number of blades in aperture for polygonal bokeh (at least 3)",
                min=0, max=100,
                default=0,
                )
        cls.aperture_rotation = FloatProperty(
                name="Aperture Rotation",
                description="Rotation of blades in aperture",
                soft_min=-math.pi, soft_max=math.pi,
                subtype='ANGLE',
                default=0,
                )

    @classmethod
    def unregister(cls):
        del bpy.types.Camera.cycles


class CyclesMaterialSettings(bpy.types.PropertyGroup):
    @classmethod
    def register(cls):
        bpy.types.Material.cycles = PointerProperty(
                name="Cycles Material Settings",
                description="Cycles material settings",
                type=cls,
                )
        cls.sample_as_light = BoolProperty(
                name="Sample as Lamp",
                description="Use direct light sampling for this material, disabling may reduce overall noise for large objects that emit little light compared to other light sources",
                default=True,
                )
        cls.homogeneous_volume = BoolProperty(
                name="Homogeneous Volume",
                description="When using volume rendering, assume volume has the same density everywhere, for faster rendering",
                default=False,
                )

    @classmethod
    def unregister(cls):
        del bpy.types.Material.cycles


class CyclesLampSettings(bpy.types.PropertyGroup):
    @classmethod
    def register(cls):
        bpy.types.Lamp.cycles = PointerProperty(
                name="Cycles Lamp Settings",
                description="Cycles lamp settings",
                type=cls,
                )
        cls.cast_shadow = BoolProperty(
                name="Cast Shadow",
                description="Lamp casts shadows",
                default=True,
                )

    @classmethod
    def unregister(cls):
        del bpy.types.Lamp.cycles


class CyclesWorldSettings(bpy.types.PropertyGroup):
    @classmethod
    def register(cls):
        bpy.types.World.cycles = PointerProperty(
                name="Cycles World Settings",
                description="Cycles world settings",
                type=cls,
                )
        cls.sample_as_light = BoolProperty(
                name="Sample as Lamp",
                description="Use direct light sampling for the environment, enabling for non-solid colors is recommended",
                default=False,
                )
        cls.sample_map_resolution = IntProperty(
                name="Map Resolution",
                description="Importance map size is resolution x resolution; higher values potentially produce less noise, at the cost of memory and speed",
                min=4, max=8096,
                default=256,
                )

    @classmethod
    def unregister(cls):
        del bpy.types.World.cycles


class CyclesVisibilitySettings(bpy.types.PropertyGroup):
    @classmethod
    def register(cls):
        bpy.types.Object.cycles_visibility = PointerProperty(
                name="Cycles Visibility Settings",
                description="Cycles visibility settings",
                type=cls,
                )

        cls.camera = BoolProperty(
                name="Camera",
                description="Object visibility for camera rays",
                default=True,
                )
        cls.diffuse = BoolProperty(
                name="Diffuse",
                description="Object visibility for diffuse reflection rays",
                default=True,
                )
        cls.glossy = BoolProperty(
                name="Glossy",
                description="Object visibility for glossy reflection rays",
                default=True,
                )
        cls.transmission = BoolProperty(
                name="Transmission",
                description="Object visibility for transmission rays",
                default=True,
                )
        cls.shadow = BoolProperty(
                name="Shadow",
                description="Object visibility for shadow rays",
                default=True,
                )

    @classmethod
    def unregister(cls):
        del bpy.types.Object.cycles_visibility


class CyclesMeshSettings(bpy.types.PropertyGroup):
    @classmethod
    def register(cls):
        bpy.types.Mesh.cycles = PointerProperty(
                name="Cycles Mesh Settings",
                description="Cycles mesh settings",
                type=cls,
                )
        bpy.types.Curve.cycles = PointerProperty(
                name="Cycles Mesh Settings",
                description="Cycles mesh settings",
                type=cls,
                )
        bpy.types.MetaBall.cycles = PointerProperty(
                name="Cycles Mesh Settings",
                description="Cycles mesh settings",
                type=cls,
                )

        cls.displacement_method = EnumProperty(
                name="Displacement Method",
                description="Method to use for the displacement",
                items=enums.displacement_methods,
                default='BUMP',
                )
        cls.use_subdivision = BoolProperty(
                name="Use Subdivision",
                description="Subdivide mesh for rendering",
                default=False,
                )
        cls.dicing_rate = FloatProperty(
                name="Dicing Rate",
                description="",
                min=0.001, max=1000.0,
                default=1.0,
                )

    @classmethod
    def unregister(cls):
        del bpy.types.Mesh.cycles
        del bpy.types.Curve.cycles
        del bpy.types.MetaBall.cycles


def register():
    bpy.utils.register_class(CyclesRenderSettings)
    bpy.utils.register_class(CyclesCameraSettings)
    bpy.utils.register_class(CyclesMaterialSettings)
    bpy.utils.register_class(CyclesLampSettings)
    bpy.utils.register_class(CyclesWorldSettings)
    bpy.utils.register_class(CyclesVisibilitySettings)
    bpy.utils.register_class(CyclesMeshSettings)


def unregister():
    bpy.utils.unregister_class(CyclesRenderSettings)
    bpy.utils.unregister_class(CyclesCameraSettings)
    bpy.utils.unregister_class(CyclesMaterialSettings)
    bpy.utils.unregister_class(CyclesLampSettings)
    bpy.utils.unregister_class(CyclesWorldSettings)
    bpy.utils.unregister_class(CyclesMeshSettings)
    bpy.utils.unregister_class(CyclesVisibilitySettings)
