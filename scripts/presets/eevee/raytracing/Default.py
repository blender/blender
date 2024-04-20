import bpy
eevee = bpy.context.scene.eevee
options = eevee.ray_tracing_options

eevee.ray_tracing_method = 'SCREEN'
options.resolution_scale = '2'
options.sample_clamp = 10.0
options.trace_max_roughness = 0.5
options.screen_trace_quality = 0.25
options.screen_trace_thickness = 0.20000000298023224
options.use_denoise = True
options.denoise_spatial = True
options.denoise_temporal = True
options.denoise_bilateral = True
