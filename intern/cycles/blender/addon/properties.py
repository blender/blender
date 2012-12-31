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

# enums

enum_devices = (
    ('CPU', "CPU", "Use CPU for rendering"),
    ('GPU', "GPU Compute", "Use GPU compute device for rendering, configured in user preferences"))

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

enum_curve_presets = (
    ('CUSTOM', "Custom", "Set general parameters"),
    ('TANGENT_SHADING', "Tangent Normal", "Use planar geometry and tangent normals"),
    ('TRUE_NORMAL', "True Normal", "Use true normals (good for thin strands)"),
    ('ACCURATE_PRESET', "Accurate", "Use best settings (suitable for glass materials)"),
    )

enum_curve_primitives = (
    ('TRIANGLES', "Triangles", "Create triangle geometry around strands"),
    ('LINE_SEGMENTS', "Line Segments", "Use line segment primitives"),
    ('CURVE_SEGMENTS', "?Curve Segments?", "Use curve segment primitives (not implemented)"),
    )

enum_triangle_curves = (
    ('CAMERA', "Planes", "Create individual triangles forming planes that face camera"),
    ('RIBBONS', "Ribbons", "Create individual triangles forming ribbon"),
    ('TESSELLATED', "Tessellated", "Create mesh surrounding each strand"),
    )

enum_line_curves = (
    ('ACCURATE', "Accurate", "Always take into consideration strand width for intersections"),
    ('QT_CORRECTED', "Corrected", "Ignore width for initial intersection and correct later"),
    ('ENDCORRECTED', "Correct found", "Ignore width for all intersections and only correct closest"),
    ('QT_UNCORRECTED', "Uncorrected", "Calculate intersections without considering width"),
    )

enum_curves_interpolation = (
    ('LINEAR', "Linear interpolation", "Use Linear interpolation between segments"),
    ('CARDINAL', "Cardinal interpolation", "Use cardinal interpolation between segments"),
    ('BSPLINE', "B-spline interpolation", "Use b-spline interpolation between segments"),
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

        cls.progressive = BoolProperty(
                name="Progressive",
                description="Use progressive sampling of lighting",
                default=True,
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

        cls.sample_clamp = FloatProperty(
                name="Clamp",
                description="If non-zero, the maximum value for a sample, "
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
        cls.use_progressive_refine = BoolProperty(
                name="Progressive Refine",
                description="Instead of rendering each tile until it is finished, "
                            "refine the whole image progressively "
                            "(this renders somewhat slower, "
                            "but time can be saved by manually stopping the render when the noise is low enough)",
                default=False,
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
        cls.panorama_type = EnumProperty(
                name="Panorama Type",
                description="Distortion to use for the calculation",
                items=enum_panorama_types,
                default='FISHEYE_EQUISOLID',
                )
        cls.fisheye_fov = FloatProperty(
                name="Field of View",
                description="Field of view for the fisheye lens",
                min=0.1745, soft_max=2 * math.pi, max=10.0 * math.pi,
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
                name="Sample as Lamp",
                description="Use direct light sampling for this material, "
                            "disabling may reduce overall noise for large "
                            "objects that emit little light compared to other light sources",
                default=True,
                )
        cls.homogeneous_volume = BoolProperty(
                name="Homogeneous Volume",
                description="When using volume rendering, assume volume has the same density everywhere, "
                            "for faster rendering",
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
        cls.samples = IntProperty(
                name="Samples",
                description="Number of light samples to render for each AA sample",
                min=1, max=10000,
                default=1,
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
                description="Use direct light sampling for the environment, "
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

class CyclesCurveRenderSettings(bpy.types.PropertyGroup):
    @classmethod
    def register(cls):
        bpy.types.Scene.cycles_curves = PointerProperty(
                name="Cycles Hair Rendering Settings",
                description="Cycles hair rendering settings",
                type=cls,
                )
        cls.preset = EnumProperty(
                name="Mode",
                description="Hair rendering mode",
                items=enum_curve_presets,
                default='TRUE_NORMAL',
                )
        cls.primitive = EnumProperty(
                name="Primitive",
                description="Type of primitive used for hair rendering",
                items=enum_curve_primitives,
                default='LINE_SEGMENTS',
                )
        cls.triangle_method = EnumProperty(
                name="Mesh Geometry",
                description="Method for creating triangle geometry",
                items=enum_triangle_curves,
                default='CAMERA',
                )
        cls.line_method = EnumProperty(
                name="Intersection Method",
                description="Method for line segment intersection",
                items=enum_line_curves,
                default='ACCURATE',
                )
        cls.interpolation = EnumProperty(
                name="Interpolation",
                description="Interpolation method",
                items=enum_curves_interpolation,
                default='BSPLINE',
                )
        cls.use_backfacing = BoolProperty(
                name="Check back-faces",
                description="Test back-faces of strands",
                default=False,
                )
        cls.use_encasing = BoolProperty(
                name="Exclude encasing",
                description="Ignore strands encasing a ray's initial location",
                default=True,
                )
        cls.use_tangent_normal_geometry = BoolProperty(
                name="Tangent normal geometry",
                description="Use the tangent normal for actual normal",
                default=False,
                )
        cls.use_tangent_normal = BoolProperty(
                name="Tangent normal default",
                description="Use the tangent normal for all normals",
                default=False,
                )
        cls.use_tangent_normal_correction = BoolProperty(
                name="Strand slope correction",
                description="Correct the tangent normal for the strand's slope",
                default=False,
                )
        cls.use_cache = BoolProperty(
                name="Export Cached data",
                default=True,
                )
        cls.use_parents = BoolProperty(
                name="Use parent strands",
                description="Use parents with children",
                default=False,
                )
        cls.use_smooth = BoolProperty(
                name="Smooth Strands",
                description="Use vertex normals",
                default=True,
                )
        cls.use_joined = BoolProperty(
                name="Join",
                description="Fill gaps between segments (requires more memory)",
                default=False,
                )
        cls.use_curves = BoolProperty(
                name="Use Cycles Hair Rendering",
                description="Activate Cycles hair rendering for particle system",
                default=True,
                )        
        cls.segments = IntProperty(
                name="Segments",
                description="Number of segments between path keys (note that this combines with the 'draw step' value)",
                min=1, max=64,
                default=1,
                )
        cls.resolution = IntProperty(
                name="Resolution",
                description="Resolution of generated mesh",
                min=3, max=64,
                default=3,
                )
        cls.normalmix = FloatProperty(
                name="Normal mix",
                description="Scale factor for tangent normal removal (zero gives ray normal)",
                min=0, max=2.0,
                default=1,
                )
        cls.encasing_ratio = FloatProperty(
                name="Encasing ratio",
                description="Scale factor for encasing strand width",
                min=0, max=100.0,
                default=1.01,
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
        cls.root_width = FloatProperty(
                name="Root Size Multiplier",
                description="Multiplier of particle size for the strand's width at root",
                min=0.0, max=1000.0,
                default=1.0,
                )
        cls.tip_width = FloatProperty(
                name="Tip Size Multiplier",
                description="Multiplier of particle size for the strand's width at tip",
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
