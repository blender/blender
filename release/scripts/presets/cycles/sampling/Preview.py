import bpy
cycles = bpy.context.scene.cycles

cycles.use_adaptive_sampling = True
cycles.adaptive_threshold = 0.1
cycles.samples = 1024
cycles.adaptive_min_samples = 0
cycles.time_limit = 0.0
cycles.use_denoising = True
cycles.denoiser = 'OPENIMAGEDENOISE'
cycles.denoising_input_passes = 'RGB_ALBEDO_NORMAL'
cycles.denoising_prefilter = 'ACCURATE'
