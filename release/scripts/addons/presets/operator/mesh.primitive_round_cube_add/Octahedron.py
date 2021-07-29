import bpy
op = bpy.context.active_operator

op.radius = 1.0
op.arc_div = 1
op.lin_div = 0
op.size = (0.0, 0.0, 0.0)
op.div_type = 'CORNERS'
op.odd_axis_align = True
