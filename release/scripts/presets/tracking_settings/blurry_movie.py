import bpy
settings = bpy.context.edit_movieclip.tracking.settings

settings.default_tracker = 'KLT'
settings.default_pyramid_levels = 4
settings.default_correlation_min = 0.75
settings.default_pattern_size = 11
settings.default_search_size = 202
settings.default_frames_limit = 25
settings.default_pattern_match = 'KEYFRAME'
settings.default_margin = 0
