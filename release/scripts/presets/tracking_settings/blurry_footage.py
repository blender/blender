import bpy
settings = bpy.context.edit_movieclip.tracking.settings

settings.default_tracker = 'Hybrid'
settings.default_pyramid_levels = 2
settings.default_correlation_min = 0.75
settings.default_pattern_size = 21
settings.default_search_size = 100
settings.default_frames_limit = 0
settings.default_pattern_match = 'PREV_FRAME'
settings.default_margin = 0
settings.use_default_red_channel = True
settings.use_default_green_channel = True
settings.use_default_blue_channel = True
