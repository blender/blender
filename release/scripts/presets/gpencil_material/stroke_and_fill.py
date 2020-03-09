import bpy
material = bpy.context.object.active_material
gpcolor = material.grease_pencil

gpcolor.mode = 'LINE'
gpcolor.stroke_style = 'SOLID'
gpcolor.color = (0.0, 0.0, 0.0, 1.0)
gpcolor.stroke_image = None
gpcolor.pixel_size = 100.0
gpcolor.mix_stroke_factor = 0.0
gpcolor.alignment_mode = 'PATH'
gpcolor.fill_style = 'SOLID'
gpcolor.fill_color = (0.5, 0.5, 0.5, 1.0)
gpcolor.fill_image = None
gpcolor.gradient_type = 'LINEAR'
gpcolor.mix_color = (1.0, 1.0, 1.0, 0.2)
gpcolor.mix_factor = 0.0
gpcolor.flip = False
gpcolor.texture_offset = (0.0, 0.0)
gpcolor.texture_scale = (1.0, 1.0)
gpcolor.texture_angle = 0.0
gpcolor.texture_opacity = 1.0
gpcolor.texture_clamp = False
gpcolor.mix_factor = 0.0
gpcolor.show_stroke = True
gpcolor.show_fill = True
