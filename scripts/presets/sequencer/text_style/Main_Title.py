import bpy
strip = bpy.context.active_strip

strip.wrap_width = 1.0
strip.use_bold = False
strip.use_italic = False
strip.font_size = 100.0
strip.color = (1.0, 1.0, 1.0, 1.0)
strip.use_outline = True
strip.outline_color = (0.0, 0.0, 0.0, 0.7)
strip.outline_width = 0.1
strip.use_shadow = True
strip.shadow_color = (0.0, 0.0, 0.0, 0.7)
strip.shadow_angle = 1.134
strip.shadow_offset = 0.04
strip.shadow_blur = 0.1
strip.use_box = False
strip.location = (0.5, 0.5)
strip.alignment_x = 'CENTER'
strip.anchor_x = 'CENTER'
strip.anchor_y = 'CENTER'
