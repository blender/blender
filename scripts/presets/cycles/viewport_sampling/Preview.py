import bpy
cycles = bpy.context.scene.cycles

cycles.use_preview_adaptive_sampling = True
cycles.preview_adaptive_threshold = 0.1
cycles.preview_samples = 1024
cycles.preview_adaptive_min_samples = 0
cycles.use_preview_denoising = False
cycles.preview_denoiser = 'AUTO'
cycles.preview_denoising_input_passes = 'RGB_ALBEDO'
cycles.preview_denoising_prefilter = 'FAST'
