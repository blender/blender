import bpy
eevee = bpy.context.scene.eevee
options = eevee.ray_tracing_options

eevee.ray_tracing_method = 'SCREEN'
options.resolution_scale = '2'
options.trace_max_roughness = 0.5
options.screen_trace_quality = 0.25
options.screen_trace_thickness = 0.20000000298023224
options.use_denoise = True
options.denoise_spatial = True
options.denoise_temporal = True
options.denoise_bilateral = True
eevee.fast_gi_method = 'GLOBAL_ILLUMINATION'
eevee.fast_gi_resolution = '2'
eevee.fast_gi_ray_count = 2
eevee.fast_gi_step_count = 8
eevee.fast_gi_quality = 0.25
eevee.fast_gi_distance = 0.0
eevee.fast_gi_thickness_near = 0.25
eevee.fast_gi_thickness_far = 0.7853981852531433
eevee.fast_gi_bias = 0.05000000074505806
