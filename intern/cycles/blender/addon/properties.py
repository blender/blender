# SPDX-License-Identifier: Apache-2.0
# Copyright 2011-2022 Blender Foundation
from __future__ import annotations

import bpy
from bpy.props import (
    BoolProperty,
    CollectionProperty,
    EnumProperty,
    FloatProperty,
    IntProperty,
    PointerProperty,
    StringProperty,
)

from math import pi

# enums

from . import engine
from . import camera

enum_devices = (
    ('CPU', "CPU", "Use CPU for rendering"),
    ('GPU', "GPU Compute",
        "Use GPU compute device for rendering, configured in the system tab in the user preferences"),
)

enum_feature_set = (
    ('SUPPORTED',
     "Supported",
     "Only use finished and supported features"),
    ('EXPERIMENTAL',
     "Experimental",
     "Use experimental and incomplete features that might be broken or change in the future",
     'ERROR',
     1),
)

enum_displacement_methods = (
    ('BUMP', "Bump Only", "Bump mapping to simulate the appearance of displacement"),
    ('DISPLACEMENT', "Displacement Only", "Use true displacement of surface only, requires fine subdivision"),
    ('BOTH', "Displacement and Bump", "Combination of true displacement and bump mapping for finer detail"),
)

enum_bvh_layouts = (
    ('BVH2', "BVH2", "", 1),
    ('EMBREE', "Embree", "", 4),
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

enum_panorama_types = (
    ('EQUIRECTANGULAR', "Equirectangular", "Spherical camera for environment maps, also known as Lat Long panorama", 0),
    ('EQUIANGULAR_CUBEMAP_FACE', "Equiangular Cubemap Face", "Single face of an equiangular cubemap", 5),
    ('MIRRORBALL', "Mirror Ball", "Mirror ball mapping for environment maps", 3),
    ('FISHEYE_EQUIDISTANT', "Fisheye Equidistant", "Ideal for fulldomes, ignore the sensor dimensions", 1),
    ('FISHEYE_EQUISOLID', "Fisheye Equisolid",
                          "Similar to most fisheye modern lens, takes sensor dimensions into consideration", 2),
    ('FISHEYE_LENS_POLYNOMIAL', "Fisheye Lens Polynomial",
     "Defines the lens projection as polynomial to allow real world camera lenses to be mimicked", 4),
)

enum_curve_shape = (
    ('RIBBONS', "Rounded Ribbons", "Render curves as flat ribbons with rounded normals, for fast rendering"),
    ('THICK', "3D Curves", "Render curves as circular 3D geometry, for accurate results when viewing closely"),
)

enum_use_layer_samples = (
    ('USE', "Use", "Per render layer number of samples override scene samples"),
    ('BOUNDED', "Bounded", "Bound per render layer number of samples by global samples"),
    ('IGNORE', "Ignore", "Ignore per render layer number of samples"),
)

enum_sampling_pattern = (
    ('SOBOL_BURLEY', "Sobol-Burley", "Use on-the-fly computed Owen-scrambled Sobol for random sampling", 0),
    ('TABULATED_SOBOL', "Tabulated Sobol", "Use precomputed tables of Owen-scrambled Sobol for random sampling", 1),
)

enum_emission_sampling = (
    ('NONE',
     'None',
     "Do not use this surface as a light for sampling",
     0),
    ('AUTO',
     'Auto',
     "Automatically determine if the surface should be treated as a light for sampling, based on estimated emission intensity",
     1),
    ('FRONT',
     'Front',
     "Treat only front side of the surface as a light, usually for closed meshes whose interior is not visible",
     2),
    ('BACK',
     'Back',
     "Treat only back side of the surface as a light for sampling",
     3),
    ('FRONT_BACK',
     'Front and Back',
     "Treat surface as a light for sampling, emitting from both the front and back side",
     4),
)

enum_volume_sampling = (
    ('DISTANCE',
     "Distance",
     "Use distance sampling, best for dense volumes with lights far away"),
    ('EQUIANGULAR',
     "Equiangular",
     "Use equiangular sampling, best for volumes with low density with light inside or near the volume"),
    ('MULTIPLE_IMPORTANCE',
     "Multiple Importance",
     "Combine distance and equi-angular sampling for volumes where neither method is ideal"),
)

enum_volume_interpolation = (
    ('LINEAR', "Linear", "Good smoothness and speed"),
    ('CUBIC', "Cubic", "Smoothed high quality interpolation, but slower")
)

enum_world_mis = (
    ('NONE',
     "None",
     "Don't sample the background, faster but might cause noise for non-solid backgrounds"),
    ('AUTOMATIC',
     "Auto",
     "Automatically try to determine the best setting"),
    ('MANUAL',
     "Manual",
     "Manually set the resolution of the sampling map, higher values are slower and require more memory but reduce noise"))

enum_device_type = (
    ('CPU', "CPU", "CPU", 0),
    ('CUDA', "CUDA", "CUDA", 1),
    ('OPTIX', "OptiX", "OptiX", 3),
    ('HIP', "HIP", "HIP", 4),
    ('METAL', "Metal", "Metal", 5),
    ('ONEAPI', "oneAPI", "oneAPI", 6)
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

enum_fast_gi_method = (
    ('REPLACE', "Replace", "Replace global illumination with ambient occlusion after a specified number of bounces"),
    ('ADD', "Add", "Add ambient occlusion to diffuse surfaces"),
)

# NOTE: Identifiers are expected to be an upper case version of identifiers from  `Pass::get_type_enum()`
enum_view3d_shading_render_pass = (
    ('', "General", ""),

    ('COMBINED', "Combined", "Show the Combined Render pass"),
    ('EMISSION', "Emission", "Show the Emission render pass"),
    ('BACKGROUND', "Background", "Show the Background render pass"),
    ('AO', "Ambient Occlusion", "Show the Ambient Occlusion render pass"),
    ('SHADOW_CATCHER', "Shadow Catcher", "Show the Shadow Catcher render pass"),

    ('', "Light", ""),

    ('DIFFUSE_DIRECT', "Diffuse Direct", "Show the Diffuse Direct render pass"),
    ('DIFFUSE_INDIRECT', "Diffuse Indirect", "Show the Diffuse Indirect render pass"),
    ('DIFFUSE_COLOR', "Diffuse Color", "Show the Diffuse Color render pass"),

    ('GLOSSY_DIRECT', "Glossy Direct", "Show the Glossy Direct render pass"),
    ('GLOSSY_INDIRECT', "Glossy Indirect", "Show the Glossy Indirect render pass"),
    ('GLOSSY_COLOR', "Glossy Color", "Show the Glossy Color render pass"),

    ('', "", ""),

    ('TRANSMISSION_DIRECT', "Transmission Direct", "Show the Transmission Direct render pass"),
    ('TRANSMISSION_INDIRECT', "Transmission Indirect", "Show the Transmission Indirect render pass"),
    ('TRANSMISSION_COLOR', "Transmission Color", "Show the Transmission Color render pass"),

    ('VOLUME_DIRECT', "Volume Direct", "Show the Volume Direct render pass"),
    ('VOLUME_INDIRECT', "Volume Indirect", "Show the Volume Indirect render pass"),

    ('', "Data", ""),

    ('POSITION', "Position", "Show the Position render pass"),
    ('NORMAL', "Normal", "Show the Normal render pass"),
    ('UV', "UV", "Show the UV render pass"),
    ('MIST', "Mist", "Show the Mist render pass"),
    ('DENOISING_ALBEDO', "Denoising Albedo", "Albedo pass used by denoiser"),
    ('DENOISING_NORMAL', "Denoising Normal", "Normal pass used by denoiser"),
    ('SAMPLE_COUNT', "Sample Count", "Per-pixel number of samples"),
)

enum_guiding_distribution = (
    ('PARALLAX_AWARE_VMM', "Parallax-Aware VMM", "Use Parallax-aware von Mises-Fisher models as directional distribution", 0),
    ('DIRECTIONAL_QUAD_TREE', "Directional Quad Tree", "Use Directional Quad Trees as directional distribution", 1),
    ('VMM', "VMM", "Use von Mises-Fisher models as directional distribution", 2),
)


def enum_openimagedenoise_denoiser(self, context):
    import _cycles
    if _cycles.with_openimagedenoise:
        return [('OPENIMAGEDENOISE', "OpenImageDenoise", "Use Intel OpenImageDenoise AI denoiser running on the CPU", 4)]
    return []


def enum_optix_denoiser(self, context):
    if not context or bool(context.preferences.addons[__package__].preferences.get_devices_for_type('OPTIX')):
        return [('OPTIX', "OptiX", "Use the OptiX AI denoiser with GPU acceleration, only available on NVIDIA GPUs", 2)]
    return []


def enum_preview_denoiser(self, context):
    optix_items = enum_optix_denoiser(self, context)
    oidn_items = enum_openimagedenoise_denoiser(self, context)

    if len(optix_items) or len(oidn_items):
        items = [
            ('AUTO',
             "Automatic",
             ("Use the fastest available denoiser for viewport rendering "
              "(OptiX if available, OpenImageDenoise otherwise)"),
             0)]
    else:
        items = [('AUTO', "None", "Blender was compiled without a viewport denoiser", 0)]

    items += optix_items
    items += oidn_items
    return items


def enum_denoiser(self, context):
    items = []
    items += enum_optix_denoiser(self, context)
    items += enum_openimagedenoise_denoiser(self, context)
    return items


enum_denoising_input_passes = (
    ('RGB', "None", "Don't use utility passes for denoising", 1),
    ('RGB_ALBEDO', "Albedo", "Use albedo pass for denoising", 2),
    ('RGB_ALBEDO_NORMAL', "Albedo and Normal", "Use albedo and normal passes for denoising", 3),
)

enum_denoising_prefilter = (
    ('NONE',
     "None",
     "No prefiltering, use when guiding passes are noise-free",
     1),
    ('FAST',
     "Fast",
     "Denoise color and guiding passes together. Improves quality when guiding passes are noisy using least amount of extra processing time",
     2),
    ('ACCURATE',
     "Accurate",
     "Prefilter noisy guiding passes before denoising color. Improves quality when guiding passes are noisy using extra processing time",
     3),
)

enum_direct_light_sampling_type = (
    ('MULTIPLE_IMPORTANCE_SAMPLING',
     "Multiple Importance Sampling",
     "Multiple importance sampling is used to combine direct light contributions from next-event estimation and forward path tracing",
     0),
    ('FORWARD_PATH_TRACING',
     "Forward Path Tracing",
     "Direct light contributions are only sampled using forward path tracing",
     1),
    ('NEXT_EVENT_ESTIMATION',
     "Next-Event Estimation",
     "Direct light contributions are only sampled using next-event estimation",
     2),
)


def update_render_passes(self, context):
    view_layer = context.view_layer
    view_layer.update_render_passes()


def update_render_engine(self, context):
    scene = context.scene
    scene.update_render_engine()


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
        update=update_render_engine,
    )
    shading_system: BoolProperty(
        name="Open Shading Language",
        description="Use Open Shading Language",
    )

    preview_pause: BoolProperty(
        name="Pause Preview",
        description="Pause all viewport preview renders",
        default=False,
    )

    use_denoising: BoolProperty(
        name="Use Denoising",
        description="Denoise the rendered image",
        default=True,
        update=update_render_passes,
    )
    denoiser: EnumProperty(
        name="Denoiser",
        description="Denoise the image with the selected denoiser. "
        "For denoising the image after rendering",
        items=enum_denoiser,
        default=4,  # Use integer to avoid error in builds without OpenImageDenoise.
        update=update_render_passes,
    )
    denoising_prefilter: EnumProperty(
        name="Denoising Prefilter",
        description="Prefilter noisy guiding (albedo and normal) passes to improve denoising quality when using OpenImageDenoiser",
        items=enum_denoising_prefilter,
        default='ACCURATE',
    )
    denoising_input_passes: EnumProperty(
        name="Denoising Input Passes",
        description="Passes used by the denoiser to distinguish noise from shader and geometry detail",
        items=enum_denoising_input_passes,
        default='RGB_ALBEDO_NORMAL',
    )

    use_preview_denoising: BoolProperty(
        name="Use Viewport Denoising",
        description="Denoise the image in the 3D viewport",
        default=False,
    )
    preview_denoiser: EnumProperty(
        name="Viewport Denoiser",
        description="Denoise the image after each preview update with the selected denoiser",
        items=enum_preview_denoiser,
        default=0,
    )
    preview_denoising_prefilter: EnumProperty(
        name="Viewport Denoising Prefilter",
        description="Prefilter noisy guiding (albedo and normal) passes to improve denoising quality when using OpenImageDenoiser",
        items=enum_denoising_prefilter,
        default='FAST',
    )
    preview_denoising_input_passes: EnumProperty(
        name="Viewport Denoising Input Passes",
        description="Passes used by the denoiser to distinguish noise from shader and geometry detail",
        items=enum_denoising_input_passes,
        default='RGB_ALBEDO',
    )
    preview_denoising_start_sample: IntProperty(
        name="Start Denoising",
        description="Sample to start denoising the preview at",
        min=0, max=(1 << 24),
        default=1,
    )

    samples: IntProperty(
        name="Samples",
        description="Number of samples to render for each pixel",
        min=1, max=(1 << 24),
        default=4096,
    )
    preview_samples: IntProperty(
        name="Viewport Samples",
        description="Number of samples to render in the viewport, unlimited if 0",
        min=0,
        soft_min=1,
        max=(1 << 24),
        default=1024,
    )

    sample_offset: IntProperty(
        name="Sample Offset",
        description="Number of samples to skip when starting render",
        min=0, max=(1 << 24),
        default=0,
    )

    time_limit: FloatProperty(
        name="Time Limit",
        description="Limit the render time (excluding synchronization time)."
        "Zero disables the limit",
        min=0.0,
        default=0.0,
        step=100.0,
        unit='TIME_ABSOLUTE',
    )

    sampling_pattern: EnumProperty(
        name="Sampling Pattern",
        description="Random sampling pattern used by the integrator",
        items=enum_sampling_pattern,
        default='TABULATED_SOBOL',
    )

    scrambling_distance: FloatProperty(
        name="Scrambling Distance",
        default=1.0,
        min=0.0,
        soft_max=1.0,
        description="Reduce randomization between pixels to improve GPU rendering performance, at the cost of possible rendering artifacts if set too low",
    )
    preview_scrambling_distance: BoolProperty(
        name="Scrambling Distance viewport",
        default=False,
        description="Uses the Scrambling Distance value for the viewport. Faster but may flicker",
    )

    auto_scrambling_distance: BoolProperty(
        name="Automatic Scrambling Distance",
        default=False,
        description="Automatically reduce the randomization between pixels to improve GPU rendering performance, at the cost of possible rendering artifacts",
    )

    use_layer_samples: EnumProperty(
        name="Layer Samples",
        description="How to use per view layer sample settings",
        items=enum_use_layer_samples,
        default='USE',
    )

    light_sampling_threshold: FloatProperty(
        name="Light Sampling Threshold",
        description="Probabilistically terminate light samples when the light contribution is below this threshold (more noise but faster rendering). "
        "Zero disables the test and never ignores lights",
        min=0.0,
        max=1.0,
        default=0.01,
    )

    use_adaptive_sampling: BoolProperty(
        name="Use Adaptive Sampling",
        description="Automatically reduce the number of samples per pixel based on estimated noise level",
        default=True,
    )
    adaptive_threshold: FloatProperty(
        name="Adaptive Sampling Threshold",
        description="Noise level step to stop sampling at, lower values reduce noise at the cost of render time. Zero for automatic setting based on number of AA samples",
        min=0.0,
        max=1.0,
        soft_min=0.001,
        default=0.01,
        precision=4,
    )
    adaptive_min_samples: IntProperty(
        name="Adaptive Min Samples",
        description="Minimum AA samples for adaptive sampling, to discover noisy features before stopping sampling. Zero for automatic setting based on noise threshold",
        min=0,
        max=4096,
        default=0,
    )

    use_preview_adaptive_sampling: BoolProperty(
        name="Use Adaptive Sampling",
        description="Automatically reduce the number of samples per pixel based on estimated noise level, for viewport renders",
        default=True,
    )
    preview_adaptive_threshold: FloatProperty(
        name="Adaptive Sampling Threshold",
        description="Noise level step to stop sampling at, lower values reduce noise at the cost of render time. Zero for automatic setting based on number of AA samples, for viewport renders",
        min=0.0,
        max=1.0,
        soft_min=0.001,
        default=0.1,
        precision=4,
    )
    preview_adaptive_min_samples: IntProperty(
        name="Adaptive Min Samples",
        description="Minimum AA samples for adaptive sampling, to discover noisy features before stopping sampling. Zero for automatic setting based on noise threshold, for viewport renders",
        min=0,
        max=4096,
        default=0,
    )

    direct_light_sampling_type: EnumProperty(
        name="Direct Light Sampling",
        description="The type of strategy used for sampling direct light contributions",
        items=enum_direct_light_sampling_type,
        default='MULTIPLE_IMPORTANCE_SAMPLING',
    )

    use_light_tree: BoolProperty(
        name="Light Tree",
        description="Sample multiple lights more efficiently based on estimated contribution at every shading point",
        default=True,
    )

    min_light_bounces: IntProperty(
        name="Min Light Bounces",
        description="Minimum number of light bounces. Setting this higher reduces noise in the first bounces, "
        "but can also be less efficient for more complex geometry like curves and volumes",
        min=0, max=1024,
        default=0,
    )
    min_transparent_bounces: IntProperty(
        name="Min Transparent Bounces",
        description="Minimum number of transparent bounces. Setting this higher reduces noise in the first bounces, "
        "but can also be less efficient for more complex geometry like curves and volumes",
        min=0, max=1024,
        default=0,
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

    use_guiding: BoolProperty(
        name="Guiding",
        description="Use path guiding for sampling paths. Path guiding incrementally "
        "learns the light distribution of the scene and guides path into directions "
        "with high direct and indirect light contributions",
        default=False,
    )

    use_deterministic_guiding: BoolProperty(
        name="Deterministic",
        description="Makes path guiding deterministic which means renderings will be "
        "reproducible with the same pixel values every time. This feature slows down "
        "training",
        default=True,
    )

    guiding_distribution_type: EnumProperty(
        name="Guiding Distribution Type",
        description="Type of representation for the guiding distribution",
        items=enum_guiding_distribution,
        default='PARALLAX_AWARE_VMM',
    )

    use_surface_guiding: BoolProperty(
        name="Surface Guiding",
        description="Use guiding when sampling directions on a surface",
        default=True,
    )

    surface_guiding_probability: FloatProperty(
        name="Surface Guiding Probability",
        description="The probability of guiding a direction on a surface",
        min=0.0, max=1.0,
        default=0.5,
    )

    use_volume_guiding: BoolProperty(
        name="Volume Guiding",
        description="Use guiding when sampling directions inside a volume",
        default=True,
    )

    guiding_training_samples: IntProperty(
        name="Training Samples",
        description="The maximum number of samples used for training path guiding. "
        "Higher samples lead to more accurate guiding, however may also unnecessarily slow "
        "down rendering once guiding is accurate enough. "
        "A value of 0 will continue training until the last sample",
        min=0,
        soft_min=1,
        default=128,
    )

    volume_guiding_probability: FloatProperty(
        name="Volume Guiding Probability",
        description="The probability of guiding a direction inside a volume",
        min=0.0, max=1.0,
        default=0.5,
    )

    use_guiding_direct_light: BoolProperty(
        name="Guide Direct Light",
        description="Consider the contribution of directly visible light sources during guiding",
        default=True,
    )

    use_guiding_mis_weights: BoolProperty(
        name="Use MIS Weights",
        description="Use the MIS weight to weight the contribution of directly visible light sources during guiding",
        default=True,
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
        description="Maximum number of transparent bounces. This is independent of maximum number of other bounces",
        min=0, max=1024,
        default=8,
    )

    volume_step_rate: FloatProperty(
        name="Step Rate",
        description="Globally adjust detail for volume rendering, on top of automatically estimated step size. "
                    "Higher values reduce render time, lower values render with more detail",
        default=1.0,
        min=0.01, max=100.0, soft_min=0.1, soft_max=10.0, precision=2
    )

    volume_preview_step_rate: FloatProperty(
        name="Step Rate",
        description="Globally adjust detail for volume rendering, on top of automatically estimated step size. "
                    "Higher values reduce render time, lower values render with more detail",
        default=1.0,
        min=0.01, max=100.0, soft_min=0.1, soft_max=10.0, precision=2
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
        name="Viewport Dicing Rate",
        description="Size of a micropolygon in pixels during preview render",
        min=0.1, max=1000.0, soft_min=0.5,
        default=8.0,
        subtype='PIXEL'
    )

    max_subdivisions: IntProperty(
        name="Max Subdivisions",
        description="Stop subdividing when this level is reached even if the dice rate would produce finer tessellation",
        min=0,
        max=16,
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

    debug_bvh_type: EnumProperty(
        name="Viewport BVH Type",
        description="Choose between faster updates, or faster render",
        items=enum_bvh_types,
        default='DYNAMIC_BVH',
    )
    debug_use_spatial_splits: BoolProperty(
        name="Use Spatial Splits",
        description="Use BVH spatial splits: longer builder time, faster render",
        default=False,
    )
    debug_use_hair_bvh: BoolProperty(
        name="Use Curves BVH",
        description="Use special type BVH optimized for curves (uses more ram but renders faster)",
        default=True,
    )
    debug_use_compact_bvh: BoolProperty(
        name="Use Compact BVH",
        description="Use compact BVH structure (uses less ram but renders slower)",
        default=False,
    )
    debug_bvh_time_steps: IntProperty(
        name="BVH Time Steps",
        description="Split BVH primitives by this number of time steps to speed up render time in cost of memory",
        default=0,
        min=0, max=16,
    )

    bake_type: EnumProperty(
        name="Bake Type",
        default='COMBINED',
        description="Type of pass to bake",
        items=(
            ('COMBINED', "Combined", "", 0),
            ('AO', "Ambient Occlusion", "", 1),
            ('SHADOW', "Shadow", "", 2),
            ('POSITION', "Position", "", 11),
            ('NORMAL', "Normal", "", 3),
            ('UV', "UV", "", 4),
            ('ROUGHNESS', "Roughness", "", 5),
            ('EMIT', "Emit", "", 6),
            ('ENVIRONMENT', "Environment", "", 7),
            ('DIFFUSE', "Diffuse", "", 8),
            ('GLOSSY', "Glossy", "", 9),
            ('TRANSMISSION', "Transmission", "", 10),
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

    use_fast_gi: BoolProperty(
        name="Fast GI Approximation",
        description="Approximate diffuse indirect light with background tinted ambient occlusion. This provides fast alternative to full global illumination, for interactive viewport rendering or final renders with reduced quality",
        default=False,
    )

    fast_gi_method: EnumProperty(
        name="Fast GI Method",
        default='REPLACE',
        description="Fast GI approximation method",
        items=enum_fast_gi_method
    )

    ao_bounces: IntProperty(
        name="AO Bounces",
        default=1,
        description="After this number of light bounces, use approximate global illumination. 0 disables this feature",
        min=0, max=1024,
    )

    ao_bounces_render: IntProperty(
        name="AO Bounces Render",
        default=1,
        description="After this number of light bounces, use approximate global illumination. 0 disables this feature",
        min=0, max=1024,
    )

    use_auto_tile: BoolProperty(
        name="Use Tiling",
        description="Render high resolution images in tiles to reduce memory usage, using the specified tile size. Tiles are cached to disk while rendering to save memory",
        default=True,
    )
    tile_size: IntProperty(
        name="Tile Size",
        default=2048,
        description="",
        min=8, max=8192,
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
        default='EMBREE',
    )

    debug_use_cuda_adaptive_compile: BoolProperty(name="Adaptive Compile", default=False)

    debug_use_optix_debug: BoolProperty(
        name="OptiX Module Debug",
        description="Load OptiX module in debug mode: lower logging verbosity level, enable validations, and lower optimization level",
        default=False)

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

    fisheye_polynomial_k0: FloatProperty(
        name="Fisheye Polynomial K0",
        description="Coefficient K0 of the lens polynomial",
        default=camera.default_fisheye_polynomial[0], precision=6, step=0.1, subtype='ANGLE',
    )
    fisheye_polynomial_k1: FloatProperty(
        name="Fisheye Polynomial K1",
        description="Coefficient K1 of the lens polynomial",
        default=camera.default_fisheye_polynomial[1], precision=6, step=0.1, subtype='ANGLE',
    )
    fisheye_polynomial_k2: FloatProperty(
        name="Fisheye Polynomial K2",
        description="Coefficient K2 of the lens polynomial",
        default=camera.default_fisheye_polynomial[2], precision=6, step=0.1, subtype='ANGLE',
    )
    fisheye_polynomial_k3: FloatProperty(
        name="Fisheye Polynomial K3",
        description="Coefficient K3 of the lens polynomial",
        default=camera.default_fisheye_polynomial[3], precision=6, step=0.1, subtype='ANGLE',
    )
    fisheye_polynomial_k4: FloatProperty(
        name="Fisheye Polynomial K4",
        description="Coefficient K4 of the lens polynomial",
        default=camera.default_fisheye_polynomial[4], precision=6, step=0.1, subtype='ANGLE',
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

    emission_sampling: EnumProperty(
        name="Emission Sampling",
        description="Sampling strategy for emissive surfaces",
        items=enum_emission_sampling,
        default="AUTO",
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

    volume_step_rate: FloatProperty(
        name="Step Rate",
        description="Scale the distance between volume shader samples when rendering the volume "
                    "(lower values give more accurate and detailed results, but also increased render time)",
        default=1.0,
        min=0.001, max=1000.0, soft_min=0.1, soft_max=10.0, precision=4
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
    is_caustics_light: BoolProperty(
        name="Shadow Caustics",
        description="Generate approximate caustics in shadows of refractive surfaces. "
        "Lights, caster and receiver objects must have shadow caustics options set to enable this",
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

    is_caustics_light: BoolProperty(
        name="Shadow Caustics",
        description="Generate approximate caustics in shadows of refractive surfaces. "
        "Lights, caster and receiver objects must have shadow caustics options set to enable this",
        default=False,
    )
    sampling_method: EnumProperty(
        name="Sampling Method",
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
    max_bounces: IntProperty(
        name="Max Bounces",
        description="Maximum number of bounces the background light will contribute to the render",
        min=0, max=1024,
        default=1024,
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
        default='EQUIANGULAR',
    )
    volume_interpolation: EnumProperty(
        name="Volume Interpolation",
        description="Interpolation method to use for volumes",
        items=enum_volume_interpolation,
        default='LINEAR',
    )
    volume_step_size: FloatProperty(
        name="Step Size",
        description="Distance between volume shader samples when rendering the volume "
                    "(lower values give more accurate and detailed results, but also increased render time)",
        default=1.0,
        min=0.0000001, max=100000.0, soft_min=0.1, soft_max=100.0, precision=4
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
        bpy.types.World.cycles_visibility = PointerProperty(
            name="Cycles Visibility Settings",
            description="Cycles visibility settings",
            type=cls,
        )

    @classmethod
    def unregister(cls):
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
        min=1,
        max=7,
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

    shadow_terminator_offset: FloatProperty(
        name="Shadow Terminator Shading Offset",
        description="Push the shadow terminator towards the light to hide artifacts on low poly geometry",
        min=0.0, max=1.0,
        default=0.0,
    )

    shadow_terminator_geometry_offset: FloatProperty(
        name="Shadow Terminator Geometry Offset",
        description="Offset rays from the surface to reduce shadow terminator artifact on low poly geometry. Only affects triangles at grazing angles to light",
        min=0.0,
        max=1.0,
        default=0.1,
    )

    ao_distance: FloatProperty(
        name="AO Distance",
        description="AO distance used for approximate global illumination (0 means use world setting)",
        min=0.0,
        default=0.0,
        subtype='DISTANCE',
    )

    is_caustics_caster: BoolProperty(
        name="Cast Shadow Caustics",
        description="With refractive materials, generate approximate caustics in shadows of this object. "
        "Up to 10 bounces inside this object are taken into account. Lights, caster and receiver objects "
        "must have shadow caustics options set to enable this",
        default=False,
    )

    is_caustics_receiver: BoolProperty(
        name="Receive Shadow Caustics",
        description="Receive approximate caustics from refractive materials in shadows on this object. "
        "Lights, caster and receiver objects must have shadow caustics options set to enable this",
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

    shape: EnumProperty(
        name="Shape",
        description="Form of curves",
        items=enum_curve_shape,
        default='RIBBONS',
    )
    subdivisions: IntProperty(
        name="Subdivisions",
        description="Number of subdivisions used in Cardinal curve intersection (power of 2)",
        min=0, max=24,
        default=2,
    )

    @classmethod
    def register(cls):
        bpy.types.Scene.cycles_curves = PointerProperty(
            name="Cycles Curves Rendering Settings",
            description="Cycles curves rendering settings",
            type=cls,
        )

    @classmethod
    def unregister(cls):
        del bpy.types.Scene.cycles_curves


class CyclesRenderLayerSettings(bpy.types.PropertyGroup):

    pass_debug_sample_count: BoolProperty(
        name="Debug Sample Count",
        description="Number of samples/camera rays per pixel",
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

    use_pass_shadow_catcher: BoolProperty(
        name="Shadow Catcher",
        description="Pass containing shadows and light which is to be multiplied into backdrop",
        default=False,
        update=update_render_passes,
    )

    use_denoising: BoolProperty(
        name="Use Denoising",
        description="Denoise the rendered image",
        default=True,
        update=update_render_passes,
    )
    denoising_store_passes: BoolProperty(
        name="Store Denoising Passes",
        description="Store the denoising feature passes and the noisy image. The passes adapt to the denoiser selected for rendering",
        default=False,
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
        has_cuda, has_optix, has_hip, has_metal, has_oneapi = _cycles.get_device_types()

        list = [('NONE', "None", "Don't use compute device", 0)]
        if has_cuda:
            list.append(('CUDA', "CUDA", "Use CUDA for GPU acceleration", 1))
        if has_optix:
            list.append(('OPTIX', "OptiX", "Use OptiX for GPU acceleration", 3))
        if has_hip:
            list.append(('HIP', "HIP", "Use HIP for GPU acceleration", 4))
        if has_metal:
            list.append(('METAL', "Metal", "Use Metal for GPU acceleration", 5))
        if has_oneapi:
            list.append(('ONEAPI', "oneAPI", "Use oneAPI for GPU acceleration", 6))

        return list

    compute_device_type: EnumProperty(
        name="Compute Device Type",
        description="Device to use for computation (rendering with Cycles)",
        items=CyclesPreferences.get_device_types,
    )

    devices: CollectionProperty(type=CyclesDeviceSettings)

    peer_memory: BoolProperty(
        name="Distribute memory across devices",
        description="Make more room for large scenes to fit by distributing memory across interconnected devices (e.g. via NVLink) rather than duplicating it",
        default=False,
    )

    use_metalrt: BoolProperty(
        name="MetalRT (Experimental)",
        description="MetalRT for ray tracing uses less memory for scenes which use curves extensively, and can give better performance in specific cases. However this support is experimental and some scenes may render incorrectly",
        default=False,
    )

    def find_existing_device_entry(self, device):
        for device_entry in self.devices:
            if device_entry.id == device[2] and device_entry.type == device[1]:
                return device_entry
        return None

    def update_device_entries(self, device_list):
        for device in device_list:
            if not device[1] in {'CUDA', 'OPTIX', 'CPU', 'HIP', 'METAL', 'ONEAPI'}:
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

    # Gets all devices types for a compute device type.
    def get_devices_for_type(self, compute_device_type):
        import _cycles
        # Layout of the device tuples: (Name, Type, Persistent ID)
        device_list = _cycles.available_devices(compute_device_type)
        # Make sure device entries are up to date and not referenced before
        # we know we don't add new devices. This way we guarantee to not
        # hold pointers to a resized array.
        self.update_device_entries(device_list)
        # Sort entries into lists
        devices = []
        cpu_devices = []
        for device in device_list:
            entry = self.find_existing_device_entry(device)
            if entry.type == compute_device_type:
                devices.append(entry)
            elif entry.type == 'CPU':
                cpu_devices.append(entry)
        # Extend all GPU devices with CPU.
        if len(devices) and compute_device_type != 'CPU':
            devices.extend(cpu_devices)
        return devices

    # Refresh device list. This does not happen automatically on Blender
    # startup due to unstable OpenCL implementations that can cause crashes.
    def refresh_devices(self):
        import _cycles
        # Ensure `self.devices` is not re-allocated when the second call to
        # get_devices_for_type is made, freeing items from the first list.
        for device_type in ('CUDA', 'OPTIX', 'HIP', 'METAL', 'ONEAPI'):
            self.update_device_entries(_cycles.available_devices(device_type))

    # Deprecated: use refresh_devices instead.
    def get_devices(self, compute_device_type=''):
        self.refresh_devices()
        return None

    def get_compute_device_type(self):
        if self.compute_device_type == '':
            return 'NONE'
        return self.compute_device_type

    def get_num_gpu_devices(self):
        import _cycles
        compute_device_type = self.get_compute_device_type()
        device_list = _cycles.available_devices(compute_device_type)
        num = 0
        for device in device_list:
            if device[1] != compute_device_type:
                continue
            for dev in self.devices:
                if dev.use and dev.id == device[2]:
                    num += 1
        return num

    def has_multi_device(self):
        import _cycles
        compute_device_type = self.get_compute_device_type()
        device_list = _cycles.available_devices(compute_device_type)
        for device in device_list:
            if device[1] == compute_device_type:
                continue
            for dev in self.devices:
                if dev.use and dev.id == device[2]:
                    return True

        return False

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
            col.label(text="No compatible GPUs found for Cycles", icon='INFO')

            if device_type == 'CUDA':
                col.label(text="Requires NVIDIA GPU with compute capability 3.0", icon='BLANK1')
            elif device_type == 'OPTIX':
                col.label(text="Requires NVIDIA GPU with compute capability 5.0", icon='BLANK1')
                col.label(text="and NVIDIA driver version 470 or newer", icon='BLANK1')
            elif device_type == 'HIP':
                import sys
                if sys.platform[:3] == "win":
                    col.label(text="Requires AMD GPU with Vega or RDNA architecture", icon='BLANK1')
                    col.label(text="and AMD Radeon Pro 21.Q4 driver or newer", icon='BLANK1')
                elif sys.platform.startswith("linux"):
                    col.label(text="Requires AMD GPU with Vega or RDNA architecture", icon='BLANK1')
                    col.label(text="and AMD driver version 22.10 or newer", icon='BLANK1')
            elif device_type == 'ONEAPI':
                import sys
                if sys.platform.startswith("win"):
                    col.label(text="Requires Intel GPU with Xe-HPG architecture", icon='BLANK1')
                    col.label(text="and Windows driver version 101.3430 or newer", icon='BLANK1')
                elif sys.platform.startswith("linux"):
                    col.label(text="Requires Intel GPU with Xe-HPG architecture and", icon='BLANK1')
                    col.label(text="  - intel-level-zero-gpu version 1.3.23904 or newer", icon='BLANK1')
                    col.label(text="  - oneAPI Level-Zero Loader", icon='BLANK1')
            elif device_type == 'METAL':
                col.label(text="Requires Apple Silicon with macOS 12.2 or newer", icon='BLANK1')
                col.label(text="or AMD with macOS 12.3 or newer", icon='BLANK1')
            return

        for device in devices:
            import unicodedata
            box.prop(
                device, "use", text=device.name
                .replace('(TM)', unicodedata.lookup('TRADE MARK SIGN'))
                .replace('(tm)', unicodedata.lookup('TRADE MARK SIGN'))
                .replace('(R)', unicodedata.lookup('REGISTERED SIGN'))
                .replace('(C)', unicodedata.lookup('COPYRIGHT SIGN'))
            )

    def draw_impl(self, layout, context):
        row = layout.row()
        row.prop(self, "compute_device_type", expand=True)

        compute_device_type = self.get_compute_device_type()
        if compute_device_type == 'NONE':
            return
        row = layout.row()
        devices = self.get_devices_for_type(compute_device_type)
        self._draw_devices(row, compute_device_type, devices)

        import _cycles
        has_peer_memory = 0
        for device in _cycles.available_devices(compute_device_type):
            if device[3] and self.find_existing_device_entry(device).use:
                has_peer_memory += 1
        if has_peer_memory > 1:
            row = layout.row()
            row.use_property_split = True
            row.prop(self, "peer_memory")

        if compute_device_type == 'METAL':
            import platform
            # MetalRT only works on Apple Silicon at present, pending argument encoding fixes on AMD
            if platform.machine() == 'arm64':
                row = layout.row()
                row.use_property_split = True
                row.prop(self, "use_metalrt")

    def draw(self, context):
        self.draw_impl(self.layout, context)


class CyclesView3DShadingSettings(bpy.types.PropertyGroup):
    render_pass: EnumProperty(
        name="Render Pass",
        description="Render pass to show in the 3D Viewport",
        items=enum_view3d_shading_render_pass,
        default='COMBINED',
    )
    show_active_pixels: BoolProperty(
        name="Show Active Pixels",
        description="When using adaptive sampling highlight pixels which are being sampled",
    )


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
    bpy.utils.register_class(CyclesView3DShadingSettings)

    bpy.types.View3DShading.cycles = bpy.props.PointerProperty(
        name="Cycles Settings",
        type=CyclesView3DShadingSettings,
    )


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
    bpy.utils.unregister_class(CyclesView3DShadingSettings)
