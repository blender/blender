import bpy
strip = bpy.context.active_strip

strip.wrap_width = 0.8
strip.use_bold = False
strip.use_italic = False
strip.font_size = 40.0
strip.color = (1.0, 1.0, 0.2, 1.0)
strip.use_outline = True
strip.outline_color = (0.0, 0.0, 0.0, 1.0)
strip.outline_width = 0.08
strip.use_shadow = True
strip.shadow_color = (0.0, 0.0, 0.0, 1.0)
strip.shadow_angle = 0.523
strip.shadow_offset = 0.05
strip.shadow_blur = 0.0
strip.use_box = False
strip.location = (0.5, 0.15)
strip.alignment_x = 'CENTER'
strip.anchor_x = 'CENTER'
strip.anchor_y = 'CENTER'
