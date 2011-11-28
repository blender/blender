import bpy
settings = bpy.context.edit_movieclip.tracking.settings

settings.default_tracker = 'KLT'
settings.default_pyramid_levels = 2
settings.default_correlation_min = 0.75
settings.default_pattern_size = 11
settings.default_search_size = 121
settings.default_frames_limit = 0
settings.default_pattern_match = 'KEYFRAME'
settings.default_margin = 0
