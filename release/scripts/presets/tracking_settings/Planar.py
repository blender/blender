import bpy
settings = bpy.context.edit_movieclip.tracking.settings


settings.default_pattern_size = 41
settings.default_search_size = 91
settings.default_motion_model = 'Perspective'
settings.use_default_brute = False
settings.use_default_normalization = False
settings.use_default_mask = False
settings.default_frames_limit = 0
settings.default_pattern_match = 'KEYFRAME'
settings.default_margin = 0
settings.use_default_red_channel = True
settings.use_default_green_channel = True
settings.use_default_blue_channel = True
settings.default_correlation_min = 0.75
settings.default_weight = 1.0
