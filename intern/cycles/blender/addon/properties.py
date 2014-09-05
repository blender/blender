#
# Copyright 2011-2013 Blender Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License
#

# <pep8 compliant>

import bpy
from bpy.props import (BoolProperty,
                       EnumProperty,
                       FloatProperty,
                       IntProperty,
                       PointerProperty)

# enums

import _cycles

enum_devices = (
    ('CPU', "CPU", "Use CPU for rendering"),
    ('GPU', "GPU Compute", "Use GPU compute device for rendering, configured in user preferences"),
    )

if _cycles.with_network:
    enum_devices += (('NETWORK', "Networked Device", "Use networked device for rendering"),)

enum_feature_set = (
    ('SUPPORTED', "Supported", "Only use finished and supported features"),
    ('EXPERIMENTAL', "Experimental", "Use experimental and incomplete features that might be broken or change in the future"),
    )

enum_displacement_methods = (
    ('BUMP', "Bump", "Bump mapping to simulate the appearance of displacement"),
    ('TRUE', "True", "Use true displacement only, requires fine subdivision"),
    ('BOTH', "Both", "Combination of displacement and bump mapping"),
    )

enum_bvh_types = (
    ('DYNAMIC_BVH', "Dynamic BVH", "Objects can be individually updated, at the cost of slower render time"),
    ('STATIC_BVH', "Static BVH", "Any object modification requires a complete BVH rebuild, but renders faster"),
    )

enum_filter_types = (
    ('BOX', "Box", "Box filter"),
    ('GAUSSIAN', "Gaussian", "Gaussian filter"),
    )

enum_aperture_types = (
    ('RADIUS', "Radius", "Directly change the size of the aperture"),
    ('FSTOP', "F/stop", "Change the size of the aperture by f/stops"),
    )

enum_panorama_types = (
    ('EQUIRECTANGULAR', "Equirectangular", "Render the scene with a spherical camera, also known as Lat Long panorama"),
    ('FISHEYE_EQUIDISTANT', "Fisheye Equidistant", "Ideal for fulldomes, ignore the sensor dimensions"),
    ('FISHEYE_EQUISOLID', "Fisheye Equisolid",
                          "Similar to most fisheye modern lens, takes sensor dimensions into consideration"),
    )

enum_curve_primitives = (
    ('TRIANGLES', "Triangles", "Create triangle geometry around strands"),
    ('LINE_SEGMENTS', "Line Segments", "Use line segment primitives"),
    ('CURVE_SEGMENTS', "Curve Segments", "Use segmented cardinal curve primitives"),
    )

enum_triangle_curves = (
    ('CAMERA_TRIANGLES', "Planes", "Create individual triangles forming planes that face camera"),
    ('TESSELLATED_TRIANGLES', "Tessellated", "Create mesh surrounding each strand"),
    )

enum_curve_shape = (
    ('RIBBONS', "Ribbons", "Ignore thickness of each strand"),
    ('THICK', "Thick", "Use thickness of strand when rendering"),
    )

enum_tile_order = (
    ('CENTER', "Center", "Render from center to the edges"),
    ('RIGHT_TO_LEFT', "Right to Left", "Render from right to left"),
    ('LEFT_TO_RIGHT', "Left to Right", "Render from left to right"),
    ('TOP_TO_BOTTOM', "Top to Bottom", "Render from top to bottom"),
    ('BOTTOM_TO_TOP', "Bottom to Top", "Render from bottom to top"),
    )

enum_use_layer_samples = (
    ('USE', "Use", "Per render layer number of samples override scene samples"),
    ('BOUNDED', "Bounded", "Bound per render layer number of samples by global samples"),
    ('IGNORE', "Ignore", "Ignore per render layer number of samples"),
    )

enum_sampling_pattern = (
    ('SOBOL', "Sobol", "Use Sobol random sampling pattern"),
    ('CORRELATED_MUTI_JITTER', "Correlated Multi-Jitter", "Use Correlated Multi-Jitter random sampling pattern"),
    )

enum_integrator = (
    ('BRANCHED_PATH', "Branched Path Tracing", "Path tracing integrator that branches on the first bounce, giving more control over the number of light and material samples"),
    ('PATH', "Path Tracing", "Pure path tracing integrator"),
    )

enum_volume_sampling = (
    ('DISTANCE', "Distance", "Use distance sampling, best for dense volumes with lights far away"),
    ('EQUIANGULAR', "Equiangular", "Use equiangular sampling, best for volumes with low density with light inside or near the volume"),
    ('MULTIPLE_IMPORTANCE', "Multiple Importance", "Combine distance and equi-angular sampling for volumes where neither method is ideal"),
    )


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
                items=enum_devices,
                default='CPU',
                )
        cls.feature_set = EnumProperty(
                name="Feature Set",
                description="Feature set to use for rendering",
                items=enum_feature_set,
                default='SUPPORTED',
                )
        cls.shading_system = BoolProperty(
                name="Open Shading Language",
                description="Use Open Shading Language (CPU rendering only)",
                )

        cls.progressive = EnumProperty(
                name="Integrator",
                description="Method to sample lights and materials",
                items=enum_integrator,
                default='PATH',
                )

        cls.use_square_samples = BoolProperty(
                name="Square Samples",
                description="Square sampling values for easier artist control",
                default=False,
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
        cls.preview_active_layer = BoolProperty(
                name="Preview Active Layer",
                description="Preview active render layer in viewport",
                default=False,
                )

        cls.aa_samples = IntProperty(
                name="AA Samples",
                description="Number of antialiasing samples to render for each pixel",
                min=1, max=10000,
                default=4,
                )
        cls.preview_aa_samples = IntProperty(
                name="AA Samples",
                description="Number of antialiasing samples to render in the viewport, unlimited if 0",
                min=0, max=10000,
                default=4,
                )
        cls.diffuse_samples = IntProperty(
                name="Diffuse Samples",
                description="Number of diffuse bounce samples to render for each AA sample",
                min=1, max=10000,
                default=1,
                )
        cls.glossy_samples = IntProperty(
                name="Glossy Samples",
                description="Number of glossy bounce samples to render for each AA sample",
                min=1, max=10000,
                default=1,
                )
        cls.transmission_samples = IntProperty(
                name="Transmission Samples",
                description="Number of transmission bounce samples to render for each AA sample",
                min=1, max=10000,
                default=1,
                )
        cls.ao_samples = IntProperty(
                name="Ambient Occlusion Samples",
                description="Number of ambient occlusion samples to render for each AA sample",
                min=1, max=10000,
                default=1,
                )
        cls.mesh_light_samples = IntProperty(
                name="Mesh Light Samples",
                description="Number of mesh emission light samples to render for each AA sample",
                min=1, max=10000,
                default=1,
                )

        cls.subsurface_samples = IntProperty(
                name="Subsurface Samples",
                description="Number of subsurface scattering samples to render for each AA sample",
                min=1, max=10000,
                default=1,
                )

        cls.volume_samples = IntProperty(
                name="Volume Samples",
                description="Number of volume scattering samples to render for each AA sample",
                min=1, max=10000,
                default=0,
                )

        cls.sampling_pattern = EnumProperty(
                name="Sampling Pattern",
                description="Random sampling pattern used by the integrator",
                items=enum_sampling_pattern,
                default='SOBOL',
                )

        cls.use_layer_samples = EnumProperty(
                name="Layer Samples",
                description="How to use per render layer sample settings",
                items=enum_use_layer_samples,
                default='USE',
                )

        cls.sample_all_lights_direct = BoolProperty(
                name="Sample All Direct Lights",
                description="Sample all lights (for direct samples), rather than randomly picking one",
                default=True,
                )

        cls.sample_all_lights_indirect = BoolProperty(
                name="Sample All Indirect Lights",
                description="Sample all lights (for indirect samples), rather than randomly picking one",
                default=True,
                )

        cls.no_caustics = BoolProperty(
                name="No Caustics",
                description="Leave out caustics, resulting in a darker image with less noise",
                default=False,
                )
        cls.blur_glossy = FloatProperty(
                name="Filter Glossy",
                description="Adaptively blur glossy shaders after blurry bounces, "
                            "to reduce noise at the cost of accuracy",
                min=0.0, max=10.0,
                default=0.0,
                )

        cls.min_bounces = IntProperty(
                name="Min Bounces",
                description="Minimum number of bounces, setting this lower "
                            "than the maximum enables probabilistic path "
                            "termination (faster but noisier)",
                min=0, max=1024,
                default=3,
                )
        cls.max_bounces = IntProperty(
                name="Max Bounces",
                description="Total maximum number of bounces",
                min=0, max=1024,
                default=12,
                )

        cls.diffuse_bounces = IntProperty(
                name="Diffuse Bounces",
                description="Maximum number of diffuse reflection bounces, bounded by total maximum",
                min=0, max=1024,
                default=4,
                )
        cls.glossy_bounces = IntProperty(
                name="Glossy Bounces",
                description="Maximum number of glossy reflection bounces, bounded by total maximum",
                min=0, max=1024,
                default=4,
                )
        cls.transmission_bounces = IntProperty(
                name="Transmission Bounces",
                description="Maximum number of transmission bounces, bounded by total maximum",
                min=0, max=1024,
                default=12,
                )
        cls.volume_bounces = IntProperty(
                name="Volume Bounces",
                description="Maximum number of volumetric scattering events",
                min=0, max=1024,
                default=0,
                )

        cls.transparent_min_bounces = IntProperty(
                name="Transparent Min Bounces",
                description="Minimum number of transparent bounces, setting "
                            "this lower than the maximum enables "
                            "probabilistic path termination (faster but "
                            "noisier)",
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

        cls.volume_step_size = FloatProperty(
                name="Step Size",
                description="Distance between volume shader samples when rendering the volume "
                            "(lower values give more accurate and detailed results, but also increased render time)",
                default=0.1,
                min=0.0000001, max=100000.0
                )

        cls.volume_max_steps = IntProperty(
                name="Max Steps",
                description="Maximum number of steps through the volume before giving up, "
                            "to avoid extremely long render times with big objects or small step sizes",
                default=1024,
                min=2, max=65536
                )

        cls.film_exposure = FloatProperty(
                name="Exposure",
                description="Image brightness scale",
                min=0.0, max=10.0,
                default=1.0,
                )
        cls.film_transparent = BoolProperty(
                name="Transparent",
                description="World background is transparent with premultiplied alpha",
                default=False,
                )

        cls.filter_type = EnumProperty(
                name="Filter Type",
                description="Pixel filter type",
                items=enum_filter_types,
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

        cls.sample_clamp_direct = FloatProperty(
                name="Clamp Direct",
                description="If non-zero, the maximum value for a direct sample, "
                            "higher values will be scaled down to avoid too "
                            "much noise and slow convergence at the cost of accuracy",
                min=0.0, max=1e8,
                default=0.0,
                )

        cls.sample_clamp_indirect = FloatProperty(
                name="Clamp Indirect",
                description="If non-zero, the maximum value for an indirect sample, "
                            "higher values will be scaled down to avoid too "
                            "much noise and slow convergence at the cost of accuracy",
                min=0.0, max=1e8,
                default=0.0,
                )

        cls.debug_tile_size = IntProperty(
                name="Tile Size",
                description="",
                min=1, max=4096,
                default=1024,
                )

        cls.preview_start_resolution = IntProperty(
                name="Start Resolution",
                description="Resolution to start rendering preview at, "
                            "progressively increasing it to the full viewport size",
                min=8, max=16384,
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
                items=enum_bvh_types,
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
        cls.tile_order = EnumProperty(
                name="Tile Order",
                description="Tile order for rendering",
                items=enum_tile_order,
                default='CENTER',
                options=set(),  # Not animatable!
                )
        cls.use_progressive_refine = BoolProperty(
                name="Progressive Refine",
                description="Instead of rendering each tile until it is finished, "
                            "refine the whole image progressively "
                            "(this renders somewhat slower, "
                            "but time can be saved by manually stopping the render when the noise is low enough)",
                default=False,
                )

        cls.bake_type = EnumProperty(
            name="Bake Type",
            default='COMBINED',
            description="Type of pass to bake",
            items = (
                ('COMBINED', "Combined", ""),
                ('AO', "Ambient Occlusion", ""),
                ('SHADOW', "Shadow", ""),
                ('NORMAL', "Normal", ""),
                ('UV', "UV", ""),
                ('EMIT', "Emit", ""),
                ('ENVIRONMENT', "Environment", ""),
                ('DIFFUSE_DIRECT', "Diffuse Direct", ""),
                ('DIFFUSE_INDIRECT', "Diffuse Indirect", ""),
                ('DIFFUSE_COLOR', "Diffuse Color", ""),
                ('GLOSSY_DIRECT', "Glossy Direct", ""),
                ('GLOSSY_INDIRECT', "Glossy Indirect", ""),
                ('GLOSSY_COLOR', "Glossy Color", ""),
                ('TRANSMISSION_DIRECT', "Transmission Direct", ""),
                ('TRANSMISSION_INDIRECT', "Transmission Indirect", ""),
                ('TRANSMISSION_COLOR', "Transmission Color", ""),
                ('SUBSURFACE_DIRECT', "Subsurface Direct", ""),
                ('SUBSURFACE_INDIRECT', "Subsurface Indirect", ""),
                ('SUBSURFACE_COLOR', "Subsurface Color", ""),
                ),
            )

    @classmethod
    def unregister(cls):
        del bpy.types.Scene.cycles


class CyclesCameraSettings(bpy.types.PropertyGroup):
    @classmethod
    def register(cls):
        import math

        bpy.types.Camera.cycles = PointerProperty(
                name="Cycles Camera Settings",
                description="Cycles camera settings",
                type=cls,
                )

        cls.aperture_type = EnumProperty(
                name="Aperture Type",
                description="Use F/stop number or aperture radius",
                items=enum_aperture_types,
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
                subtype='DISTANCE',
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
        cls.aperture_ratio = FloatProperty(
                name="Aperture Ratio",
                description="Distortion to simulate anamorphic lens bokeh",
                min=0.01, soft_min=1.0, soft_max=2.0,
                default=1.0,
                precision=4,
                )
        cls.panorama_type = EnumProperty(
                name="Panorama Type",
                description="Distortion to use for the calculation",
                items=enum_panorama_types,
                default='FISHEYE_EQUISOLID',
                )
        cls.fisheye_fov = FloatProperty(
                name="Field of View",
                description="Field of view for the fisheye lens",
                min=0.1745, soft_max=2.0 * math.pi, max=10.0 * math.pi,
                subtype='ANGLE',
                default=math.pi,
                )
        cls.fisheye_lens = FloatProperty(
                name="Fisheye Lens",
                description="Lens focal length (mm)",
                min=0.01, soft_max=15.0, max=100.0,
                default=10.5,
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
                name="Multiple Importance Sample",
                description="Use multiple importance sampling for this material, "
                            "disabling may reduce overall noise for large "
                            "objects that emit little light compared to other light sources",
                default=True,
                )
        cls.use_transparent_shadow = BoolProperty(
                name="Transparent Shadows",
                description="Use transparent shadows for this material if it contains a Transparent BSDF, "
                            "disabling will render faster but not give accurate shadows",
                default=True,
                )
        cls.homogeneous_volume = BoolProperty(
                name="Homogeneous Volume",
                description="When using volume rendering, assume volume has the same density everywhere "
                            "(not using any textures), for faster rendering",
                default=False,
                )
        cls.volume_sampling = EnumProperty(
                name="Volume Sampling",
                description="Sampling method to use for volumes",
                items=enum_volume_sampling,
                default='DISTANCE',
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
        cls.samples = IntProperty(
                name="Samples",
                description="Number of light samples to render for each AA sample",
                min=1, max=10000,
                default=1,
                )
        cls.use_multiple_importance_sampling = BoolProperty(
                name="Multiple Importance Sample",
                description="Use multiple importance sampling for the lamp, "
                            "reduces noise for area lamps and sharp glossy materials",
                default=False,
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
                name="Multiple Importance Sample",
                description="Use multiple importance sampling for the environment, "
                            "enabling for non-solid colors is recommended",
                default=False,
                )
        cls.sample_map_resolution = IntProperty(
                name="Map Resolution",
                description="Importance map size is resolution x resolution; "
                            "higher values potentially produce less noise, at the cost of memory and speed",
                min=4, max=8096,
                default=256,
                )
        cls.samples = IntProperty(
                name="Samples",
                description="Number of light samples to render for each AA sample",
                min=1, max=10000,
                default=4,
                )
        cls.homogeneous_volume = BoolProperty(
                name="Homogeneous Volume",
                description="When using volume rendering, assume volume has the same density everywhere"
                            "(not using any textures), for faster rendering",
                default=False,
                )
        cls.volume_sampling = EnumProperty(
                name="Volume Sampling",
                description="Sampling method to use for volumes",
                items=enum_volume_sampling,
                default='EQUIANGULAR',
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

        bpy.types.World.cycles_visibility = PointerProperty(
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
        cls.scatter = BoolProperty(
                name="Volume Scatter",
                description="Object visibility for volume scatter rays",
                default=True,
                )

    @classmethod
    def unregister(cls):
        del bpy.types.Object.cycles_visibility
        del bpy.types.World.cycles_visibility


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
                items=enum_displacement_methods,
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


class CyclesObjectBlurSettings(bpy.types.PropertyGroup):

    @classmethod
    def register(cls):

        bpy.types.Object.cycles = PointerProperty(
                name="Cycles Object Settings",
                description="Cycles object settings",
                type=cls,
                )

        cls.use_motion_blur = BoolProperty(
                name="Use Motion Blur",
                description="Use motion blur for this object",
                default=True,
                )

        cls.use_deform_motion = BoolProperty(
                name="Use Deformation Motion",
                description="Use deformation motion blur for this object",
                default=True,
                )

        cls.motion_steps = IntProperty(
                name="Motion Steps",
                description="Control accuracy of deformation motion blur, more steps gives more memory usage (actual number of steps is 2^(steps - 1))",
                min=1, soft_max=8,
                default=1,
                )

    @classmethod
    def unregister(cls):
        del bpy.types.Object.cycles


class CyclesCurveRenderSettings(bpy.types.PropertyGroup):
    @classmethod
    def register(cls):
        bpy.types.Scene.cycles_curves = PointerProperty(
                name="Cycles Hair Rendering Settings",
                description="Cycles hair rendering settings",
                type=cls,
                )
        cls.primitive = EnumProperty(
                name="Primitive",
                description="Type of primitive used for hair rendering",
                items=enum_curve_primitives,
                default='LINE_SEGMENTS',
                )
        cls.shape = EnumProperty(
                name="Shape",
                description="Form of hair",
                items=enum_curve_shape,
                default='THICK',
                )
        cls.cull_backfacing = BoolProperty(
                name="Cull back-faces",
                description="Do not test the back-face of each strand",
                default=True,
                )
        cls.use_curves = BoolProperty(
                name="Use Cycles Hair Rendering",
                description="Activate Cycles hair rendering for particle system",
                default=True,
                )
        cls.resolution = IntProperty(
                name="Resolution",
                description="Resolution of generated mesh",
                min=3, max=64,
                default=3,
                )
        cls.minimum_width = FloatProperty(
                name="Minimal width",
                description="Minimal pixel width for strands (0 - deactivated)",
                min=0.0, max=100.0,
                default=0.0,
                )
        cls.maximum_width = FloatProperty(
                name="Maximal width",
                description="Maximum extension that strand radius can be increased by",
                min=0.0, max=100.0,
                default=0.1,
                )
        cls.subdivisions = IntProperty(
                name="Subdivisions",
                description="Number of subdivisions used in Cardinal curve intersection (power of 2)",
                min=0, max=24,
                default=4,
                )

    @classmethod
    def unregister(cls):
        del bpy.types.Scene.cycles_curves


class CyclesCurveSettings(bpy.types.PropertyGroup):
    @classmethod
    def register(cls):
        bpy.types.ParticleSettings.cycles = PointerProperty(
                name="Cycles Hair Settings",
                description="Cycles hair settings",
                type=cls,
                )
        cls.radius_scale = FloatProperty(
                name="Radius Scaling",
                description="Multiplier of width properties",
                min=0.0, max=1000.0,
                default=0.01,
                )
        cls.root_width = FloatProperty(
                name="Root Size",
                description="Strand's width at root",
                min=0.0, max=1000.0,
                default=1.0,
                )
        cls.tip_width = FloatProperty(
                name="Tip Multiplier",
                description="Strand's width at tip",
                min=0.0, max=1000.0,
                default=0.0,
                )
        cls.shape = FloatProperty(
                name="Strand Shape",
                description="Strand shape parameter",
                min=-1.0, max=1.0,
                default=0.0,
                )
        cls.use_closetip = BoolProperty(
                name="Close tip",
                description="Set tip radius to zero",
                default=True,
                )

    @classmethod
    def unregister(cls):
        del bpy.types.ParticleSettings.cycles


def register():
    bpy.utils.register_class(CyclesRenderSettings)
    bpy.utils.register_class(CyclesCameraSettings)
    bpy.utils.register_class(CyclesMaterialSettings)
    bpy.utils.register_class(CyclesLampSettings)
    bpy.utils.register_class(CyclesWorldSettings)
    bpy.utils.register_class(CyclesVisibilitySettings)
    bpy.utils.register_class(CyclesMeshSettings)
    bpy.utils.register_class(CyclesCurveRenderSettings)
    bpy.utils.register_class(CyclesCurveSettings)


def unregister():
    bpy.utils.unregister_class(CyclesRenderSettings)
    bpy.utils.unregister_class(CyclesCameraSettings)
    bpy.utils.unregister_class(CyclesMaterialSettings)
    bpy.utils.unregister_class(CyclesLampSettings)
    bpy.utils.unregister_class(CyclesWorldSettings)
    bpy.utils.unregister_class(CyclesMeshSettings)
    bpy.utils.unregister_class(CyclesVisibilitySettings)
    bpy.utils.unregister_class(CyclesCurveRenderSettings)
    bpy.utils.unregister_class(CyclesCurveSettings)
