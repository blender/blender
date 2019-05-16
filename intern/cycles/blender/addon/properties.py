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
# limitations under the License.
#

# <pep8 compliant>

import bpy
from bpy.props import (
    BoolProperty,
    EnumProperty,
    FloatProperty,
    IntProperty,
    PointerProperty,
    StringProperty,
)

from math import pi

# enums

import _cycles

enum_devices = (
    ('CPU', "CPU", "Use CPU for rendering"),
    ('GPU', "GPU Compute", "Use GPU compute device for rendering, configured in the system tab in the user preferences"),
)

if _cycles.with_network:
    enum_devices += (('NETWORK', "Networked Device", "Use networked device for rendering"),)

enum_feature_set = (
    ('SUPPORTED', "Supported", "Only use finished and supported features"),
    ('EXPERIMENTAL', "Experimental", "Use experimental and incomplete features that might be broken or change in the future", 'ERROR', 1),
)

enum_displacement_methods = (
    ('BUMP', "Bump Only", "Bump mapping to simulate the appearance of displacement"),
    ('DISPLACEMENT', "Displacement Only", "Use true displacement of surface only, requires fine subdivision"),
    ('BOTH', "Displacement and Bump", "Combination of true displacement and bump mapping for finer detail"),
)

enum_bvh_layouts = (
    ('BVH2', "BVH2", "", 1),
    ('BVH4', "BVH4", "", 2),
    ('BVH8', "BVH8", "", 4),
)

enum_bvh_types = (
    ('DYNAMIC_BVH', "Dynamic BVH", "Objects can be individually updated, at the cost of slower render time"),
    ('STATIC_BVH', "Static BVH", "Any object modification requires a complete BVH rebuild, but renders faster"),
)

enum_filter_types = (
    ('BOX', "Box", "Box filter"),
    ('GAUSSIAN', "Gaussian", "Gaussian filter"),
    ('BLACKMAN_HARRIS', "Blackman-Harris", "Blackman-Harris filter"),
)

enum_aperture_types = (
    ('RADIUS', "Radius", "Directly change the size of the aperture"),
    ('FSTOP', "F-stop", "Change the size of the aperture by f-stop"),
)

enum_panorama_types = (
    ('EQUIRECTANGULAR', "Equirectangular", "Render the scene with a spherical camera, also known as Lat Long panorama"),
    ('FISHEYE_EQUIDISTANT', "Fisheye Equidistant", "Ideal for fulldomes, ignore the sensor dimensions"),
    ('FISHEYE_EQUISOLID', "Fisheye Equisolid",
                          "Similar to most fisheye modern lens, takes sensor dimensions into consideration"),
    ('MIRRORBALL', "Mirror Ball", "Uses the mirror ball mapping"),
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
    ('HILBERT_SPIRAL', "Hilbert Spiral", "Render in a Hilbert Spiral"),
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

enum_volume_interpolation = (
    ('LINEAR', "Linear", "Good smoothness and speed"),
    ('CUBIC', "Cubic", "Smoothed high quality interpolation, but slower")
)

enum_world_mis = (
    ('NONE', "None", "Don't sample the background, faster but might cause noise for non-solid backgrounds"),
    ('AUTOMATIC', "Auto", "Automatically try to determine the best setting"),
    ('MANUAL', "Manual", "Manually set the resolution of the sampling map, higher values are slower and require more memory but reduce noise")
)

enum_device_type = (
    ('CPU', "CPU", "CPU", 0),
    ('CUDA', "CUDA", "CUDA", 1),
    ('OPENCL', "OpenCL", "OpenCL", 2)
)

enum_texture_limit = (
    ('OFF', "No Limit", "No texture size limit", 0),
    ('128', "128", "Limit texture size to 128 pixels", 1),
    ('256', "256", "Limit texture size to 256 pixels", 2),
    ('512', "512", "Limit texture size to 512 pixels", 3),
    ('1024', "1024", "Limit texture size to 1024 pixels", 4),
    ('2048', "2048", "Limit texture size to 2048 pixels", 5),
    ('4096', "4096", "Limit texture size to 4096 pixels", 6),
    ('8192', "8192", "Limit texture size to 8192 pixels", 7),
)


class CyclesRenderSettings(bpy.types.PropertyGroup):

    device: EnumProperty(
        name="Device",
        description="Device to use for rendering",
        items=enum_devices,
        default='CPU',
    )
    feature_set: EnumProperty(
        name="Feature Set",
        description="Feature set to use for rendering",
        items=enum_feature_set,
        default='SUPPORTED',
    )
    shading_system: BoolProperty(
        name="Open Shading Language",
        description="Use Open Shading Language (CPU rendering only)",
    )

    progressive: EnumProperty(
        name="Integrator",
        description="Method to sample lights and materials",
        items=enum_integrator,
        default='PATH',
    )

    use_square_samples: BoolProperty(
        name="Square Samples",
        description="Square sampling values for easier artist control",
        default=False,
    )

    samples: IntProperty(
        name="Samples",
        description="Number of samples to render for each pixel",
        min=1, max=(1 << 24),
        default=128,
    )
    preview_samples: IntProperty(
        name="Preview Samples",
        description="Number of samples to render in the viewport, unlimited if 0",
        min=0, max=(1 << 24),
        default=32,
    )
    preview_pause: BoolProperty(
        name="Pause Preview",
        description="Pause all viewport preview renders",
        default=False,
    )
    aa_samples: IntProperty(
        name="AA Samples",
        description="Number of antialiasing samples to render for each pixel",
        min=1, max=2097151,
        default=128,
    )
    preview_aa_samples: IntProperty(
        name="AA Samples",
        description="Number of antialiasing samples to render in the viewport, unlimited if 0",
        min=0, max=2097151,
        default=32,
    )
    diffuse_samples: IntProperty(
        name="Diffuse Samples",
        description="Number of diffuse bounce samples to render for each AA sample",
        min=1, max=1024,
        default=1,
    )
    glossy_samples: IntProperty(
        name="Glossy Samples",
        description="Number of glossy bounce samples to render for each AA sample",
        min=1, max=1024,
        default=1,
    )
    transmission_samples: IntProperty(
        name="Transmission Samples",
        description="Number of transmission bounce samples to render for each AA sample",
        min=1, max=1024,
        default=1,
    )
    ao_samples: IntProperty(
        name="Ambient Occlusion Samples",
        description="Number of ambient occlusion samples to render for each AA sample",
        min=1, max=1024,
        default=1,
    )
    mesh_light_samples: IntProperty(
        name="Mesh Light Samples",
        description="Number of mesh emission light samples to render for each AA sample",
        min=1, max=1024,
        default=1,
    )

    subsurface_samples: IntProperty(
        name="Subsurface Samples",
        description="Number of subsurface scattering samples to render for each AA sample",
        min=1, max=1024,
        default=1,
    )

    volume_samples: IntProperty(
        name="Volume Samples",
        description="Number of volume scattering samples to render for each AA sample",
        min=1, max=1024,
        default=1,
    )

    sampling_pattern: EnumProperty(
        name="Sampling Pattern",
        description="Random sampling pattern used by the integrator",
        items=enum_sampling_pattern,
        default='SOBOL',
    )

    use_layer_samples: EnumProperty(
        name="Layer Samples",
        description="How to use per view layer sample settings",
        items=enum_use_layer_samples,
        default='USE',
    )

    sample_all_lights_direct: BoolProperty(
        name="Sample All Direct Lights",
        description="Sample all lights (for direct samples), rather than randomly picking one",
        default=True,
    )

    sample_all_lights_indirect: BoolProperty(
        name="Sample All Indirect Lights",
        description="Sample all lights (for indirect samples), rather than randomly picking one",
        default=True,
    )
    light_sampling_threshold: FloatProperty(
        name="Light Sampling Threshold",
        description="Probabilistically terminate light samples when the light contribution is below this threshold (more noise but faster rendering). "
        "Zero disables the test and never ignores lights",
        min=0.0, max=1.0,
        default=0.01,
    )

    caustics_reflective: BoolProperty(
        name="Reflective Caustics",
        description="Use reflective caustics, resulting in a brighter image (more noise but added realism)",
        default=True,
    )

    caustics_refractive: BoolProperty(
        name="Refractive Caustics",
        description="Use refractive caustics, resulting in a brighter image (more noise but added realism)",
        default=True,
    )

    blur_glossy: FloatProperty(
        name="Filter Glossy",
        description="Adaptively blur glossy shaders after blurry bounces, "
        "to reduce noise at the cost of accuracy",
        min=0.0, max=10.0,
        default=1.0,
    )

    max_bounces: IntProperty(
        name="Max Bounces",
        description="Total maximum number of bounces",
        min=0, max=1024,
        default=12,
    )

    diffuse_bounces: IntProperty(
        name="Diffuse Bounces",
        description="Maximum number of diffuse reflection bounces, bounded by total maximum",
        min=0, max=1024,
        default=4,
    )
    glossy_bounces: IntProperty(
        name="Glossy Bounces",
        description="Maximum number of glossy reflection bounces, bounded by total maximum",
        min=0, max=1024,
        default=4,
    )
    transmission_bounces: IntProperty(
        name="Transmission Bounces",
        description="Maximum number of transmission bounces, bounded by total maximum",
        min=0, max=1024,
        default=12,
    )
    volume_bounces: IntProperty(
        name="Volume Bounces",
        description="Maximum number of volumetric scattering events",
        min=0, max=1024,
        default=0,
    )

    transparent_max_bounces: IntProperty(
        name="Transparent Max Bounces",
        description="Maximum number of transparent bounces",
        min=0, max=1024,
        default=8,
    )

    volume_step_size: FloatProperty(
        name="Step Size",
        description="Distance between volume shader samples when rendering the volume "
        "(lower values give more accurate and detailed results, but also increased render time)",
        default=0.1,
        min=0.0000001, max=100000.0, soft_min=0.01, soft_max=1.0, precision=4,
        unit='LENGTH'
    )

    volume_max_steps: IntProperty(
        name="Max Steps",
        description="Maximum number of steps through the volume before giving up, "
        "to avoid extremely long render times with big objects or small step sizes",
        default=1024,
        min=2, max=65536
    )

    dicing_rate: FloatProperty(
        name="Dicing Rate",
        description="Size of a micropolygon in pixels",
        min=0.1, max=1000.0, soft_min=0.5,
        default=1.0,
        subtype='PIXEL'
    )
    preview_dicing_rate: FloatProperty(
        name="Preview Dicing Rate",
        description="Size of a micropolygon in pixels during preview render",
        min=0.1, max=1000.0, soft_min=0.5,
        default=8.0,
        subtype='PIXEL'
    )

    max_subdivisions: IntProperty(
        name="Max Subdivisions",
        description="Stop subdividing when this level is reached even if the dice rate would produce finer tessellation",
        min=0, max=16,
        default=12,
    )

    dicing_camera: PointerProperty(
        name="Dicing Camera",
        description="Camera to use as reference point when subdividing geometry, useful to avoid crawling "
        "artifacts in animations when the scene camera is moving",
        type=bpy.types.Object,
        poll=lambda self, obj: obj.type == 'CAMERA',
    )
    offscreen_dicing_scale: FloatProperty(
        name="Offscreen Dicing Scale",
        description="Multiplier for dicing rate of geometry outside of the camera view. The dicing rate "
        "of objects is gradually increased the further they are outside the camera view. "
        "Lower values provide higher quality reflections and shadows for off screen objects, "
        "while higher values use less memory",
        min=1.0, soft_max=25.0,
        default=4.0,
    )

    film_exposure: FloatProperty(
        name="Exposure",
        description="Image brightness scale",
        min=0.0, max=10.0,
        default=1.0,
    )
    film_transparent_glass: BoolProperty(
        name="Transparent Glass",
        description="Render transmissive surfaces as transparent, for compositing glass over another background",
        default=False,
    )
    film_transparent_roughness: FloatProperty(
        name="Transparent Roughness Threshold",
        description="For transparent transmission, keep surfaces with roughness above the threshold opaque",
        min=0.0, max=1.0,
        default=0.1,
    )

    # Really annoyingly, we have to keep it around for a few releases,
    # otherwise forward compatibility breaks in really bad manner: CRASH!
    #
    # TODO(sergey): Remove this during 2.8x series of Blender.
    filter_type: EnumProperty(
        name="Filter Type",
        description="Pixel filter type",
        items=enum_filter_types,
        default='BLACKMAN_HARRIS',
    )

    pixel_filter_type: EnumProperty(
        name="Filter Type",
        description="Pixel filter type",
        items=enum_filter_types,
        default='BLACKMAN_HARRIS',
    )

    filter_width: FloatProperty(
        name="Filter Width",
        description="Pixel filter width",
        min=0.01, max=10.0,
        default=1.5,
        subtype='PIXEL'
    )

    seed: IntProperty(
        name="Seed",
        description="Seed value for integrator to get different noise patterns",
        min=0, max=2147483647,
        default=0,
    )

    use_animated_seed: BoolProperty(
        name="Use Animated Seed",
        description="Use different seed values (and hence noise patterns) at different frames",
        default=False,
    )

    sample_clamp_direct: FloatProperty(
        name="Clamp Direct",
        description="If non-zero, the maximum value for a direct sample, "
        "higher values will be scaled down to avoid too "
        "much noise and slow convergence at the cost of accuracy",
        min=0.0, max=1e8,
        default=0.0,
    )

    sample_clamp_indirect: FloatProperty(
        name="Clamp Indirect",
        description="If non-zero, the maximum value for an indirect sample, "
        "higher values will be scaled down to avoid too "
        "much noise and slow convergence at the cost of accuracy",
        min=0.0, max=1e8,
        default=10.0,
    )

    debug_tile_size: IntProperty(
        name="Tile Size",
        description="",
        min=1, max=4096,
        default=1024,
    )

    preview_start_resolution: IntProperty(
        name="Start Resolution",
        description="Resolution to start rendering preview at, "
        "progressively increasing it to the full viewport size",
        min=8, max=16384,
        default=64,
        subtype='PIXEL'
    )

    debug_reset_timeout: FloatProperty(
        name="Reset timeout",
        description="",
        min=0.01, max=10.0,
        default=0.1,
    )
    debug_cancel_timeout: FloatProperty(
        name="Cancel timeout",
        description="",
        min=0.01, max=10.0,
        default=0.1,
    )
    debug_text_timeout: FloatProperty(
        name="Text timeout",
        description="",
        min=0.01, max=10.0,
        default=1.0,
    )

    debug_bvh_type: EnumProperty(
        name="Viewport BVH Type",
        description="Choose between faster updates, or faster render",
        items=enum_bvh_types,
        default='DYNAMIC_BVH',
    )
    use_bvh_embree: BoolProperty(
        name="Use Embree",
        description="Use Embree as ray accelerator",
        default=False,
    )
    debug_use_spatial_splits: BoolProperty(
        name="Use Spatial Splits",
        description="Use BVH spatial splits: longer builder time, faster render",
        default=False,
    )
    debug_use_hair_bvh: BoolProperty(
        name="Use Hair BVH",
        description="Use special type BVH optimized for hair (uses more ram but renders faster)",
        default=True,
    )
    debug_bvh_time_steps: IntProperty(
        name="BVH Time Steps",
        description="Split BVH primitives by this number of time steps to speed up render time in cost of memory",
        default=0,
        min=0, max=16,
    )
    tile_order: EnumProperty(
        name="Tile Order",
        description="Tile order for rendering",
        items=enum_tile_order,
        default='HILBERT_SPIRAL',
        options=set(),  # Not animatable!
    )
    use_progressive_refine: BoolProperty(
        name="Progressive Refine",
        description="Instead of rendering each tile until it is finished, "
        "refine the whole image progressively "
        "(this renders somewhat slower, "
        "but time can be saved by manually stopping the render when the noise is low enough)",
        default=False,
    )

    bake_type: EnumProperty(
        name="Bake Type",
        default='COMBINED',
        description="Type of pass to bake",
        items=(
            ('COMBINED', "Combined", ""),
            ('AO', "Ambient Occlusion", ""),
            ('SHADOW', "Shadow", ""),
            ('NORMAL', "Normal", ""),
            ('UV', "UV", ""),
            ('ROUGHNESS', "Roughness", ""),
            ('EMIT', "Emit", ""),
            ('ENVIRONMENT', "Environment", ""),
            ('DIFFUSE', "Diffuse", ""),
            ('GLOSSY', "Glossy", ""),
            ('TRANSMISSION', "Transmission", ""),
            ('SUBSURFACE', "Subsurface", ""),
        ),
    )

    use_camera_cull: BoolProperty(
        name="Use Camera Cull",
        description="Allow objects to be culled based on the camera frustum",
        default=False,
    )

    camera_cull_margin: FloatProperty(
        name="Camera Cull Margin",
        description="Margin for the camera space culling",
        default=0.1,
        min=0.0, max=5.0,
        subtype='FACTOR'
    )

    use_distance_cull: BoolProperty(
        name="Use Distance Cull",
        description="Allow objects to be culled based on the distance from camera",
        default=False,
    )

    distance_cull_margin: FloatProperty(
        name="Cull Distance",
        description="Cull objects which are further away from camera than this distance",
        default=50,
        min=0.0,
        unit='LENGTH'
    )

    motion_blur_position: EnumProperty(
        name="Motion Blur Position",
        default='CENTER',
        description="Offset for the shutter's time interval, allows to change the motion blur trails",
        items=(
            ('START', "Start on Frame", "The shutter opens at the current frame"),
            ('CENTER', "Center on Frame", "The shutter is open during the current frame"),
            ('END', "End on Frame", "The shutter closes at the current frame"),
        ),
    )

    rolling_shutter_type: EnumProperty(
        name="Shutter Type",
        default='NONE',
        description="Type of rolling shutter effect matching CMOS-based cameras",
        items=(
            ('NONE', "None", "No rolling shutter effect used"),
            ('TOP', "Top-Bottom", "Sensor is being scanned from top to bottom")
            # TODO(seergey): Are there real cameras with different scanning direction?
        ),
    )

    rolling_shutter_duration: FloatProperty(
        name="Rolling Shutter Duration",
        description="Scanline \"exposure\" time for the rolling shutter effect",
        default=0.1,
        min=0.0, max=1.0,
        subtype='FACTOR',
    )

    texture_limit: EnumProperty(
        name="Viewport Texture Limit",
        default='OFF',
        description="Limit texture size used by viewport rendering",
        items=enum_texture_limit
    )

    texture_limit_render: EnumProperty(
        name="Render Texture Limit",
        default='OFF',
        description="Limit texture size used by final rendering",
        items=enum_texture_limit
    )

    ao_bounces: IntProperty(
        name="AO Bounces",
        default=0,
        description="Approximate indirect light with background tinted ambient occlusion at the specified bounce, 0 disables this feature",
        min=0, max=1024,
    )

    ao_bounces_render: IntProperty(
        name="AO Bounces Render",
        default=0,
        description="Approximate indirect light with background tinted ambient occlusion at the specified bounce, 0 disables this feature",
        min=0, max=1024,
    )

    # Various fine-tuning debug flags

    def _devices_update_callback(self, context):
        import _cycles
        scene = context.scene.as_pointer()
        return _cycles.debug_flags_update(scene)

    debug_use_cpu_avx2: BoolProperty(name="AVX2", default=True)
    debug_use_cpu_avx: BoolProperty(name="AVX", default=True)
    debug_use_cpu_sse41: BoolProperty(name="SSE41", default=True)
    debug_use_cpu_sse3: BoolProperty(name="SSE3", default=True)
    debug_use_cpu_sse2: BoolProperty(name="SSE2", default=True)
    debug_bvh_layout: EnumProperty(
        name="BVH Layout",
        items=enum_bvh_layouts,
        default='BVH8',
    )
    debug_use_cpu_split_kernel: BoolProperty(name="Split Kernel", default=False)

    debug_use_cuda_adaptive_compile: BoolProperty(name="Adaptive Compile", default=False)
    debug_use_cuda_split_kernel: BoolProperty(name="Split Kernel", default=False)

    debug_opencl_kernel_type: EnumProperty(
        name="OpenCL Kernel Type",
        default='DEFAULT',
        items=(
            ('DEFAULT', "Default", ""),
            ('MEGA', "Mega", ""),
            ('SPLIT', "Split", ""),
        ),
        update=_devices_update_callback
    )

    debug_opencl_device_type: EnumProperty(
        name="OpenCL Device Type",
        default='ALL',
        items=(
            ('NONE', "None", ""),
            ('ALL', "All", ""),
            ('DEFAULT', "Default", ""),
            ('CPU', "CPU", ""),
            ('GPU', "GPU", ""),
            ('ACCELERATOR', "Accelerator", ""),
        ),
        update=_devices_update_callback
    )

    del _devices_update_callback

    debug_use_opencl_debug: BoolProperty(name="Debug OpenCL", default=False)

    debug_opencl_mem_limit: IntProperty(
        name="Memory limit",
        default=0,
        description="Artificial limit on OpenCL memory usage in MB (0 to disable limit)"
    )

    @classmethod
    def register(cls):
        bpy.types.Scene.cycles = PointerProperty(
            name="Cycles Render Settings",
            description="Cycles render settings",
            type=cls,
        )

    @classmethod
    def unregister(cls):
        del bpy.types.Scene.cycles


class CyclesCameraSettings(bpy.types.PropertyGroup):

    aperture_type: EnumProperty(
        name="Aperture Type",
        description="Use f-stop number or aperture radius",
        items=enum_aperture_types,
        default='RADIUS',
    )
    aperture_fstop: FloatProperty(
        name="Aperture f-stop",
        description="F-stop ratio (lower numbers give more defocus, higher numbers give a sharper image)",
        min=0.0, soft_min=0.1, soft_max=64.0,
        default=5.6,
        step=10,
        precision=1,
    )
    aperture_size: FloatProperty(
        name="Aperture Size",
        description="Radius of the aperture for depth of field (higher values give more defocus)",
        min=0.0, soft_max=10.0,
        default=0.0,
        step=1,
        precision=4,
        subtype='DISTANCE',
    )
    aperture_blades: IntProperty(
        name="Aperture Blades",
        description="Number of blades in aperture for polygonal bokeh (at least 3)",
        min=0, max=100,
        default=0,
    )
    aperture_rotation: FloatProperty(
        name="Aperture Rotation",
        description="Rotation of blades in aperture",
        soft_min=-pi, soft_max=pi,
        subtype='ANGLE',
        default=0,
    )
    aperture_ratio: FloatProperty(
        name="Aperture Ratio",
        description="Distortion to simulate anamorphic lens bokeh",
        min=0.01, soft_min=1.0, soft_max=2.0,
        default=1.0,
        precision=4,
    )
    panorama_type: EnumProperty(
        name="Panorama Type",
        description="Distortion to use for the calculation",
        items=enum_panorama_types,
        default='FISHEYE_EQUISOLID',
    )
    fisheye_fov: FloatProperty(
        name="Field of View",
        description="Field of view for the fisheye lens",
        min=0.1745, soft_max=2.0 * pi, max=10.0 * pi,
        subtype='ANGLE',
        default=pi,
    )
    fisheye_lens: FloatProperty(
        name="Fisheye Lens",
        description="Lens focal length (mm)",
        min=0.01, soft_max=15.0, max=100.0,
        default=10.5,
    )
    latitude_min: FloatProperty(
        name="Min Latitude",
        description="Minimum latitude (vertical angle) for the equirectangular lens",
        min=-0.5 * pi, max=0.5 * pi,
        subtype='ANGLE',
        default=-0.5 * pi,
    )
    latitude_max: FloatProperty(
        name="Max Latitude",
        description="Maximum latitude (vertical angle) for the equirectangular lens",
        min=-0.5 * pi, max=0.5 * pi,
        subtype='ANGLE',
        default=0.5 * pi,
    )
    longitude_min: FloatProperty(
        name="Min Longitude",
        description="Minimum longitude (horizontal angle) for the equirectangular lens",
        min=-pi, max=pi,
        subtype='ANGLE',
        default=-pi,
    )
    longitude_max: FloatProperty(
        name="Max Longitude",
        description="Maximum longitude (horizontal angle) for the equirectangular lens",
        min=-pi, max=pi,
        subtype='ANGLE',
        default=pi,
    )

    @classmethod
    def register(cls):
        bpy.types.Camera.cycles = PointerProperty(
            name="Cycles Camera Settings",
            description="Cycles camera settings",
            type=cls,
        )

    @classmethod
    def unregister(cls):
        del bpy.types.Camera.cycles


class CyclesMaterialSettings(bpy.types.PropertyGroup):

    sample_as_light: BoolProperty(
        name="Multiple Importance Sample",
        description="Use multiple importance sampling for this material, "
        "disabling may reduce overall noise for large "
        "objects that emit little light compared to other light sources",
        default=True,
    )
    use_transparent_shadow: BoolProperty(
        name="Transparent Shadows",
        description="Use transparent shadows for this material if it contains a Transparent BSDF, "
        "disabling will render faster but not give accurate shadows",
        default=True,
    )
    homogeneous_volume: BoolProperty(
        name="Homogeneous Volume",
        description="When using volume rendering, assume volume has the same density everywhere "
        "(not using any textures), for faster rendering",
        default=False,
    )
    volume_sampling: EnumProperty(
        name="Volume Sampling",
        description="Sampling method to use for volumes",
        items=enum_volume_sampling,
        default='MULTIPLE_IMPORTANCE',
    )

    volume_interpolation: EnumProperty(
        name="Volume Interpolation",
        description="Interpolation method to use for smoke/fire volumes",
        items=enum_volume_interpolation,
        default='LINEAR',
    )

    displacement_method: EnumProperty(
        name="Displacement Method",
        description="Method to use for the displacement",
        items=enum_displacement_methods,
        default='BUMP',
    )

    @classmethod
    def register(cls):
        bpy.types.Material.cycles = PointerProperty(
            name="Cycles Material Settings",
            description="Cycles material settings",
            type=cls,
        )

    @classmethod
    def unregister(cls):
        del bpy.types.Material.cycles


class CyclesLightSettings(bpy.types.PropertyGroup):

    cast_shadow: BoolProperty(
        name="Cast Shadow",
        description="Light casts shadows",
        default=True,
    )
    samples: IntProperty(
        name="Samples",
        description="Number of light samples to render for each AA sample",
        min=1, max=10000,
        default=1,
    )
    max_bounces: IntProperty(
        name="Max Bounces",
        description="Maximum number of bounces the light will contribute to the render",
        min=0, max=1024,
        default=1024,
    )
    use_multiple_importance_sampling: BoolProperty(
        name="Multiple Importance Sample",
        description="Use multiple importance sampling for the light, "
        "reduces noise for area lights and sharp glossy materials",
        default=True,
    )
    is_portal: BoolProperty(
        name="Is Portal",
        description="Use this area light to guide sampling of the background, "
        "note that this will make the light invisible",
        default=False,
    )

    @classmethod
    def register(cls):
        bpy.types.Light.cycles = PointerProperty(
            name="Cycles Light Settings",
            description="Cycles light settings",
            type=cls,
        )

    @classmethod
    def unregister(cls):
        del bpy.types.Light.cycles


class CyclesWorldSettings(bpy.types.PropertyGroup):

    sampling_method: EnumProperty(
        name="Sampling method",
        description="How to sample the background light",
        items=enum_world_mis,
        default='AUTOMATIC',
    )
    sample_map_resolution: IntProperty(
        name="Map Resolution",
        description="Importance map size is resolution x resolution/2; "
        "higher values potentially produce less noise, at the cost of memory and speed",
        min=4, max=8192,
        default=1024,
    )
    samples: IntProperty(
        name="Samples",
        description="Number of light samples to render for each AA sample",
        min=1, max=10000,
        default=1,
    )
    max_bounces: IntProperty(
        name="Max Bounces",
        description="Maximum number of bounces the background light will contribute to the render",
        min=0, max=1024,
        default=1024,
    )
    homogeneous_volume: BoolProperty(
        name="Homogeneous Volume",
        description="When using volume rendering, assume volume has the same density everywhere"
        "(not using any textures), for faster rendering",
        default=False,
    )
    volume_sampling: EnumProperty(
        name="Volume Sampling",
        description="Sampling method to use for volumes",
        items=enum_volume_sampling,
        default='EQUIANGULAR',
    )
    volume_interpolation: EnumProperty(
        name="Volume Interpolation",
        description="Interpolation method to use for volumes",
        items=enum_volume_interpolation,
        default='LINEAR',
    )

    @classmethod
    def register(cls):
        bpy.types.World.cycles = PointerProperty(
            name="Cycles World Settings",
            description="Cycles world settings",
            type=cls,
        )

    @classmethod
    def unregister(cls):
        del bpy.types.World.cycles


class CyclesVisibilitySettings(bpy.types.PropertyGroup):

    camera: BoolProperty(
        name="Camera",
        description="Object visibility for camera rays",
        default=True,
    )
    diffuse: BoolProperty(
        name="Diffuse",
        description="Object visibility for diffuse reflection rays",
        default=True,
    )
    glossy: BoolProperty(
        name="Glossy",
        description="Object visibility for glossy reflection rays",
        default=True,
    )
    transmission: BoolProperty(
        name="Transmission",
        description="Object visibility for transmission rays",
        default=True,
    )
    shadow: BoolProperty(
        name="Shadow",
        description="Object visibility for shadow rays",
        default=True,
    )
    scatter: BoolProperty(
        name="Volume Scatter",
        description="Object visibility for volume scatter rays",
        default=True,
    )

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

    @classmethod
    def unregister(cls):
        del bpy.types.Mesh.cycles
        del bpy.types.Curve.cycles
        del bpy.types.MetaBall.cycles


class CyclesObjectSettings(bpy.types.PropertyGroup):

    use_motion_blur: BoolProperty(
        name="Use Motion Blur",
        description="Use motion blur for this object",
        default=True,
    )

    use_deform_motion: BoolProperty(
        name="Use Deformation Motion",
        description="Use deformation motion blur for this object",
        default=True,
    )

    motion_steps: IntProperty(
        name="Motion Steps",
        description="Control accuracy of motion blur, more steps gives more memory usage (actual number of steps is 2^(steps - 1))",
        min=1, soft_max=8,
        default=1,
    )

    use_camera_cull: BoolProperty(
        name="Use Camera Cull",
        description="Allow this object and its duplicators to be culled by camera space culling",
        default=False,
    )

    use_distance_cull: BoolProperty(
        name="Use Distance Cull",
        description="Allow this object and its duplicators to be culled by distance from camera",
        default=False,
    )

    use_adaptive_subdivision: BoolProperty(
        name="Use Adaptive Subdivision",
        description="Use adaptive render time subdivision",
        default=False,
    )

    dicing_rate: FloatProperty(
        name="Dicing Scale",
        description="Multiplier for scene dicing rate (located in the Subdivision panel)",
        min=0.1, max=1000.0, soft_min=0.5,
        default=1.0,
    )

    is_shadow_catcher: BoolProperty(
        name="Shadow Catcher",
        description="Only render shadows on this object, for compositing renders into real footage",
        default=False,
    )

    is_holdout: BoolProperty(
        name="Holdout",
        description="Render objects as a holdout or matte, creating a "
        "hole in the image with zero alpha, to fill out in "
        "compositing with real footage or another render",
        default=False,
    )

    @classmethod
    def register(cls):
        bpy.types.Object.cycles = PointerProperty(
            name="Cycles Object Settings",
            description="Cycles object settings",
            type=cls,
        )

    @classmethod
    def unregister(cls):
        del bpy.types.Object.cycles


class CyclesCurveRenderSettings(bpy.types.PropertyGroup):

    primitive: EnumProperty(
        name="Primitive",
        description="Type of primitive used for hair rendering",
        items=enum_curve_primitives,
        default='LINE_SEGMENTS',
    )
    shape: EnumProperty(
        name="Shape",
        description="Form of hair",
        items=enum_curve_shape,
        default='THICK',
    )
    cull_backfacing: BoolProperty(
        name="Cull Back-faces",
        description="Do not test the back-face of each strand",
        default=True,
    )
    use_curves: BoolProperty(
        name="Use Cycles Hair Rendering",
        description="Activate Cycles hair rendering for particle system",
        default=True,
    )
    resolution: IntProperty(
        name="Resolution",
        description="Resolution of generated mesh",
        min=3, max=64,
        default=3,
    )
    subdivisions: IntProperty(
        name="Subdivisions",
        description="Number of subdivisions used in Cardinal curve intersection (power of 2)",
        min=0, max=24,
        default=4,
    )

    @classmethod
    def register(cls):
        bpy.types.Scene.cycles_curves = PointerProperty(
            name="Cycles Hair Rendering Settings",
            description="Cycles hair rendering settings",
            type=cls,
        )

    @classmethod
    def unregister(cls):
        del bpy.types.Scene.cycles_curves


def update_render_passes(self, context):
    view_layer = context.view_layer
    view_layer.update_render_passes()


class CyclesRenderLayerSettings(bpy.types.PropertyGroup):

    pass_debug_bvh_traversed_nodes: BoolProperty(
        name="Debug BVH Traversed Nodes",
        description="Store Debug BVH Traversed Nodes pass",
        default=False,
        update=update_render_passes,
    )
    pass_debug_bvh_traversed_instances: BoolProperty(
        name="Debug BVH Traversed Instances",
        description="Store Debug BVH Traversed Instances pass",
        default=False,
        update=update_render_passes,
    )
    pass_debug_bvh_intersections: BoolProperty(
        name="Debug BVH Intersections",
        description="Store Debug BVH Intersections",
        default=False,
        update=update_render_passes,
    )
    pass_debug_ray_bounces: BoolProperty(
        name="Debug Ray Bounces",
        description="Store Debug Ray Bounces pass",
        default=False,
        update=update_render_passes,
    )
    pass_debug_render_time: BoolProperty(
        name="Debug Render Time",
        description="Render time in milliseconds per sample and pixel",
        default=False,
        update=update_render_passes,
    )
    use_pass_volume_direct: BoolProperty(
        name="Volume Direct",
        description="Deliver direct volumetric scattering pass",
        default=False,
        update=update_render_passes,
    )
    use_pass_volume_indirect: BoolProperty(
        name="Volume Indirect",
        description="Deliver indirect volumetric scattering pass",
        default=False,
        update=update_render_passes,
    )

    use_denoising: BoolProperty(
        name="Use Denoising",
        description="Denoise the rendered image",
        default=False,
        update=update_render_passes,
    )
    denoising_diffuse_direct: BoolProperty(
        name="Diffuse Direct",
        description="Denoise the direct diffuse lighting",
        default=True,
    )
    denoising_diffuse_indirect: BoolProperty(
        name="Diffuse Indirect",
        description="Denoise the indirect diffuse lighting",
        default=True,
    )
    denoising_glossy_direct: BoolProperty(
        name="Glossy Direct",
        description="Denoise the direct glossy lighting",
        default=True,
    )
    denoising_glossy_indirect: BoolProperty(
        name="Glossy Indirect",
        description="Denoise the indirect glossy lighting",
        default=True,
    )
    denoising_transmission_direct: BoolProperty(
        name="Transmission Direct",
        description="Denoise the direct transmission lighting",
        default=True,
    )
    denoising_transmission_indirect: BoolProperty(
        name="Transmission Indirect",
        description="Denoise the indirect transmission lighting",
        default=True,
    )
    denoising_subsurface_direct: BoolProperty(
        name="Subsurface Direct",
        description="Denoise the direct subsurface lighting",
        default=True,
    )
    denoising_subsurface_indirect: BoolProperty(
        name="Subsurface Indirect",
        description="Denoise the indirect subsurface lighting",
        default=True,
    )
    denoising_strength: FloatProperty(
        name="Denoising Strength",
        description="Controls neighbor pixel weighting for the denoising filter (lower values preserve more detail, but aren't as smooth)",
        min=0.0, max=1.0,
        default=0.5,
    )
    denoising_feature_strength: FloatProperty(
        name="Denoising Feature Strength",
        description="Controls removal of noisy image feature passes (lower values preserve more detail, but aren't as smooth)",
        min=0.0, max=1.0,
        default=0.5,
    )
    denoising_radius: IntProperty(
        name="Denoising Radius",
        description="Size of the image area that's used to denoise a pixel (higher values are smoother, but might lose detail and are slower)",
        min=1, max=25,
        default=8,
        subtype="PIXEL",
    )
    denoising_relative_pca: BoolProperty(
        name="Relative filter",
        description="When removing pixels that don't carry information, use a relative threshold instead of an absolute one (can help to reduce artifacts, but might cause detail loss around edges)",
        default=False,
    )
    denoising_store_passes: BoolProperty(
        name="Store denoising passes",
        description="Store the denoising feature passes and the noisy image",
        default=False,
        update=update_render_passes,
    )
    denoising_neighbor_frames: IntProperty(
        name="Neighbor Frames",
        description="Number of neighboring frames to use for denoising animations (more frames produce smoother results at the cost of performance)",
        min=0, max=7,
        default=0,
    )
    use_pass_crypto_object: BoolProperty(
        name="Cryptomatte Object",
        description="Render cryptomatte object pass, for isolating objects in compositing",
        default=False,
        update=update_render_passes,
        )
    use_pass_crypto_material: BoolProperty(
        name="Cryptomatte Material",
        description="Render cryptomatte material pass, for isolating materials in compositing",
        default=False,
        update=update_render_passes,
        )
    use_pass_crypto_asset: BoolProperty(
        name="Cryptomatte Asset",
        description="Render cryptomatte asset pass, for isolating groups of objects with the same parent",
        default=False,
        update=update_render_passes,
        )
    pass_crypto_depth: IntProperty(
        name="Cryptomatte Levels",
        description="Sets how many unique objects can be distinguished per pixel",
        default=6, min=2, max=16, step=2,
        update=update_render_passes,
        )
    pass_crypto_accurate: BoolProperty(
        name="Cryptomatte Accurate",
        description="Generate a more accurate Cryptomatte pass. CPU only, may render slower and use more memory",
        default=True,
        update=update_render_passes,
        )

    @classmethod
    def register(cls):
        bpy.types.ViewLayer.cycles = PointerProperty(
            name="Cycles ViewLayer Settings",
            description="Cycles ViewLayer Settings",
            type=cls,
        )

    @classmethod
    def unregister(cls):
        del bpy.types.ViewLayer.cycles


class CyclesDeviceSettings(bpy.types.PropertyGroup):
    id: StringProperty(name="ID")
    name: StringProperty(name="Name")
    use: BoolProperty(name="Use", default=True)
    type: EnumProperty(name="Type", items=enum_device_type, default='CUDA')


class CyclesPreferences(bpy.types.AddonPreferences):
    bl_idname = __package__

    def get_device_types(self, context):
        import _cycles
        has_cuda, has_opencl = _cycles.get_device_types()
        list = [('NONE', "None", "Don't use compute device", 0)]
        if has_cuda:
            list.append(('CUDA', "CUDA", "Use CUDA for GPU acceleration", 1))
        if has_opencl:
            list.append(('OPENCL', "OpenCL", "Use OpenCL for GPU acceleration", 2))
        return list

    compute_device_type: EnumProperty(
        name="Compute Device Type",
        description="Device to use for computation (rendering with Cycles)",
        items=get_device_types,
    )

    devices: bpy.props.CollectionProperty(type=CyclesDeviceSettings)

    def find_existing_device_entry(self, device):
        for device_entry in self.devices:
            if device_entry.id == device[2] and device_entry.type == device[1]:
                return device_entry
        return None

    def update_device_entries(self, device_list):
        for device in device_list:
            if not device[1] in {'CUDA', 'OPENCL', 'CPU'}:
                continue
            # Try to find existing Device entry
            entry = self.find_existing_device_entry(device)
            if not entry:
                # Create new entry if no existing one was found
                entry = self.devices.add()
                entry.id = device[2]
                entry.name = device[0]
                entry.type = device[1]
                entry.use = entry.type != 'CPU'
            elif entry.name != device[0]:
                # Update name in case it changed
                entry.name = device[0]

    # Gets all devices types by default.
    def get_devices(self, compute_device_type=''):
        import _cycles
        # Layout of the device tuples: (Name, Type, Persistent ID)
        device_list = _cycles.available_devices(compute_device_type)
        # Make sure device entries are up to date and not referenced before
        # we know we don't add new devices. This way we guarantee to not
        # hold pointers to a resized array.
        self.update_device_entries(device_list)
        # Sort entries into lists
        cuda_devices = []
        opencl_devices = []
        cpu_devices = []
        for device in device_list:
            entry = self.find_existing_device_entry(device)
            if entry.type == 'CUDA':
                cuda_devices.append(entry)
            elif entry.type == 'OPENCL':
                opencl_devices.append(entry)
            elif entry.type == 'CPU':
                cpu_devices.append(entry)
        # Extend all GPU devices with CPU.
        cuda_devices.extend(cpu_devices)
        opencl_devices.extend(cpu_devices)
        return cuda_devices, opencl_devices

    def get_num_gpu_devices(self):
        import _cycles
        device_list = _cycles.available_devices(self.compute_device_type)
        num = 0
        for device in device_list:
            if device[1] != self.compute_device_type:
                continue
            for dev in self.devices:
                if dev.use and dev.id == device[2]:
                    num += 1
        return num

    def has_active_device(self):
        return self.get_num_gpu_devices() > 0

    def _draw_devices(self, layout, device_type, devices):
        box = layout.box()

        found_device = False
        for device in devices:
            if device.type == device_type:
                found_device = True
                break

        if not found_device:
            col = box.column(align=True)
            col.label(text="No compatible GPUs found for path tracing", icon='INFO')
            col.label(text="Cycles will render on the CPU", icon='BLANK1')
            return

        for device in devices:
            box.prop(device, "use", text=device.name)

    def draw_impl(self, layout, context):
        row = layout.row()
        row.prop(self, "compute_device_type", expand=True)

        cuda_devices, opencl_devices = self.get_devices(self.compute_device_type)
        row = layout.row()
        if self.compute_device_type == 'CUDA':
            self._draw_devices(row, 'CUDA', cuda_devices)
        elif self.compute_device_type == 'OPENCL':
            self._draw_devices(row, 'OPENCL', opencl_devices)

    def draw(self, context):
        self.draw_impl(self.layout, context)


def register():
    bpy.utils.register_class(CyclesRenderSettings)
    bpy.utils.register_class(CyclesCameraSettings)
    bpy.utils.register_class(CyclesMaterialSettings)
    bpy.utils.register_class(CyclesLightSettings)
    bpy.utils.register_class(CyclesWorldSettings)
    bpy.utils.register_class(CyclesVisibilitySettings)
    bpy.utils.register_class(CyclesMeshSettings)
    bpy.utils.register_class(CyclesObjectSettings)
    bpy.utils.register_class(CyclesCurveRenderSettings)
    bpy.utils.register_class(CyclesDeviceSettings)
    bpy.utils.register_class(CyclesPreferences)
    bpy.utils.register_class(CyclesRenderLayerSettings)


def unregister():
    bpy.utils.unregister_class(CyclesRenderSettings)
    bpy.utils.unregister_class(CyclesCameraSettings)
    bpy.utils.unregister_class(CyclesMaterialSettings)
    bpy.utils.unregister_class(CyclesLightSettings)
    bpy.utils.unregister_class(CyclesWorldSettings)
    bpy.utils.unregister_class(CyclesMeshSettings)
    bpy.utils.unregister_class(CyclesObjectSettings)
    bpy.utils.unregister_class(CyclesVisibilitySettings)
    bpy.utils.unregister_class(CyclesCurveRenderSettings)
    bpy.utils.unregister_class(CyclesDeviceSettings)
    bpy.utils.unregister_class(CyclesPreferences)
    bpy.utils.unregister_class(CyclesRenderLayerSettings)
